// libcrtweb: the CRT-owned web runtime and view API (Web Tranche 4) over the PlatformCRT port.
//
// The only translation unit that sees WebKit. Public contract: include/crtweb/crtweb.h and
// docs/design/libcrtweb_api_policy.md. Everything runs on the thread that created the runtime;
// WebKit's run loop is driven by crtweb_runtime_pump() and every application callback is delivered
// from there.

#define BUILDING_CRT__ 1
#ifndef CRTWEB_BUILD_LIBRARY
#define CRTWEB_BUILD_LIBRARY 1
#endif

#include "crtweb/crtweb.h"

#include <WebKit/WKBase.h>
#include <WebKit/WKContext.h>
#include <WebKit/WKArray.h>
#include <WebKit/WKContextConfigurationRef.h>
#include <WebKit/WKDictionary.h>
#include <WebKit/WKNumber.h>
#include <WebKit/WKErrorRef.h>
#include <WebKit/WKFrameInfoRef.h>
#include <WebKit/WKFramePolicyListener.h>
#include <WebKit/WKGeometry.h>
#include <WebKit/WKNavigationActionRef.h>
#include <WebKit/WKPage.h>
#include <WebKit/WKPageConfigurationRef.h>
#include <WebKit/WKPageNavigationClient.h>
#include <WebKit/WKRunLoop.h>
#include <WebKit/WKString.h>
#include <WebKit/WKType.h>
#include <WebKit/WKURL.h>
#include <WebKit/WKURLRequest.h>
#include <WebKit/WKView.h>
#include <WebKit/WKViewClient.h>
#include <WebKit/WKViewCRT.h>

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <stdlib.h>
#endif

struct crtweb_view;

struct crtweb_runtime {
    std::thread::id owner;
    WKContextRef context { nullptr };
    std::string applicationName;
    std::vector<crtweb_view*> views;
    std::vector<crtweb_view*> doomed; // destroyed from inside a pump: freed when the pump ends
    bool pumping { false };
    bool activity { false };
};

struct ScriptRequest {
    crtweb_view* view;
    crtweb_script_callback callback;
    void* userData;
};

struct crtweb_view {
    crtweb_runtime* runtime { nullptr };
    WKViewRef view { nullptr };
    WKPageRef page { nullptr };
    WKPageConfigurationRef pageConfiguration { nullptr };
    crtweb_view_callbacks callbacks { };
    uint32_t width { 0 };
    uint32_t height { 0 };
    uint64_t serial { 0 };
    bool damaged { false };
    bool painted { false };
    bool destroyed { false };
    bool terminatedReported { false };
    std::string title;
    std::string url;
    std::vector<ScriptRequest*> scripts;
};

namespace {

crtweb_runtime* gRuntime;
bool gRunLoopInitialized;

std::string toUTF8(WKStringRef string)
{
    if (!string)
        return { };
    std::vector<char> buffer(WKStringGetMaximumUTF8CStringSize(string));
    size_t length = WKStringGetUTF8CString(string, buffer.data(), buffer.size());
    return std::string(buffer.data(), length ? length - 1 : 0);
}

std::string takeString(WKStringRef string)
{
    std::string result = toUTF8(string);
    if (string)
        WKRelease(string);
    return result;
}

std::string takeURL(WKURLRef url)
{
    if (!url)
        return { };
    std::string result = takeString(WKURLCopyString(url));
    WKRelease(url);
    return result;
}

bool onOwnerThread(const crtweb_runtime* runtime)
{
    return runtime && runtime->owner == std::this_thread::get_id();
}

crtweb_result copyOut(const std::string& value, char* buffer, size_t capacity, size_t* needed)
{
    if (needed)
        *needed = value.size() + 1;
    if (!buffer && capacity)
        return CRTWEB_ERROR_INVALID_ARGUMENT;
    if (buffer && capacity) {
        size_t count = value.size() < capacity - 1 ? value.size() : capacity - 1;
        memcpy(buffer, value.data(), count);
        buffer[count] = '\0';
    }
    return CRTWEB_OK;
}

bool hasAllowedScheme(const char* url)
{
    static const char* const schemes[] = { "http:", "https:", "file:", "about:" };
    for (const char* scheme : schemes) {
        size_t length = strlen(scheme);
        size_t i = 0;
        while (i < length && url[i] && (url[i] | 0x20) == scheme[i])
            ++i;
        if (i == length)
            return true;
    }
    return false;
}

// ---- WebKit client callbacks -------------------------------------------------------------------

crtweb_view* viewFrom(const void* clientInfo)
{
    auto* view = const_cast<crtweb_view*>(static_cast<const crtweb_view*>(clientInfo));
    return view && !view->destroyed ? view : nullptr;
}

void deliverLoad(crtweb_view* view, crtweb_load_event event, const std::string& url, const char* message)
{
    view->runtime->activity = true;
    if (view->callbacks.on_load)
        view->callbacks.on_load(view, event, url.c_str(), message, view->callbacks.user_data);
}

void didStartProvisionalNavigation(WKPageRef page, WKNavigationRef, WKTypeRef, const void* clientInfo)
{
    if (crtweb_view* view = viewFrom(clientInfo))
        deliverLoad(view, CRTWEB_LOAD_STARTED, takeURL(WKPageCopyActiveURL(page)), nullptr);
}

void didCommitNavigation(WKPageRef page, WKNavigationRef, WKTypeRef, const void* clientInfo)
{
    if (crtweb_view* view = viewFrom(clientInfo))
        deliverLoad(view, CRTWEB_LOAD_COMMITTED, takeURL(WKPageCopyActiveURL(page)), nullptr);
}

void didFinishNavigation(WKPageRef page, WKNavigationRef, WKTypeRef, const void* clientInfo)
{
    if (crtweb_view* view = viewFrom(clientInfo))
        deliverLoad(view, CRTWEB_LOAD_FINISHED, takeURL(WKPageCopyActiveURL(page)), nullptr);
}

void failed(WKPageRef page, WKErrorRef error, const void* clientInfo)
{
    crtweb_view* view = viewFrom(clientInfo);
    if (!view)
        return;
    std::string url = takeURL(error ? WKErrorCopyFailingURL(error) : nullptr);
    if (url.empty())
        url = takeURL(WKPageCopyActiveURL(page));
    std::string message = takeString(error ? WKErrorCopyLocalizedDescription(error) : nullptr);
    deliverLoad(view, CRTWEB_LOAD_FAILED, url, message.c_str());
}

void didFailProvisionalNavigation(WKPageRef page, WKNavigationRef, WKErrorRef error, WKTypeRef, const void* clientInfo)
{
    failed(page, error, clientInfo);
}

void didFailNavigation(WKPageRef page, WKNavigationRef, WKErrorRef error, WKTypeRef, const void* clientInfo)
{
    failed(page, error, clientInfo);
}

void decidePolicyForNavigationAction(WKPageRef, WKNavigationActionRef action, WKFramePolicyListenerRef listener, WKTypeRef, const void* clientInfo)
{
    crtweb_view* view = viewFrom(clientInfo);
    bool allow = true;
    if (view && view->callbacks.on_navigation_request) {
        WKFrameInfoRef frame = WKNavigationActionCopyTargetFrameInfo(action);
        bool mainFrame = frame && WKFrameInfoGetIsMainFrame(frame);
        if (frame)
            WKRelease(frame);
        if (mainFrame) {
            WKURLRequestRef request = WKNavigationActionCopyRequest(action);
            std::string url = takeURL(request ? WKURLRequestCopyURL(request) : nullptr);
            if (request)
                WKRelease(request);
            view->runtime->activity = true;
            allow = view->callbacks.on_navigation_request(view, url.c_str(), view->callbacks.user_data) != 0;
        }
    }
    if (allow)
        WKFramePolicyListenerUse(listener);
    else
        WKFramePolicyListenerIgnore(listener);
}

void webProcessDidCrash(WKPageRef, const void* clientInfo)
{
    crtweb_view* view = viewFrom(clientInfo);
    if (!view || view->terminatedReported)
        return;
    view->terminatedReported = true;
    view->runtime->activity = true;
    if (view->callbacks.on_process_terminated)
        view->callbacks.on_process_terminated(view, view->callbacks.user_data);
}

void setViewNeedsDisplay(WKViewRef, WKRect, const void* clientInfo)
{
    if (crtweb_view* view = viewFrom(clientInfo))
        view->damaged = true;
}

// Result of WKPageEvaluateJavaScript: the page's value as WebKit's API objects (string, number, boolean,
// array, dictionary), rendered the way JavaScript's String(value) would for the scalar cases.
std::string numberToString(double value)
{
    if (value != value)
        return "NaN";
    if (value == static_cast<double>(static_cast<long long>(value)) && value < 1e15 && value > -1e15)
        return std::to_string(static_cast<long long>(value));
    char buffer[40];
    for (int digits = 15; digits <= 17; ++digits) {
        snprintf(buffer, sizeof buffer, "%.*g", digits, value);
        if (strtod(buffer, nullptr) == value)
            break;
    }
    return buffer;
}

std::string scriptResultToString(WKTypeRef result, bool& ok)
{
    ok = true;
    if (!result)
        return "undefined";
    WKTypeID type = WKGetTypeID(result);
    if (type == WKStringGetTypeID())
        return toUTF8(static_cast<WKStringRef>(result));
    if (type == WKDoubleGetTypeID())
        return numberToString(WKDoubleGetValue(static_cast<WKDoubleRef>(result)));
    if (type == WKUInt64GetTypeID())
        return std::to_string(WKUInt64GetValue(static_cast<WKUInt64Ref>(result)));
    if (type == WKBooleanGetTypeID())
        return WKBooleanGetValue(static_cast<WKBooleanRef>(result)) ? "true" : "false";
    if (type == WKArrayGetTypeID()) {
        auto array = static_cast<WKArrayRef>(result);
        std::string text;
        for (size_t i = 0; i < WKArrayGetSize(array); ++i) {
            if (i)
                text += ',';
            bool itemOk;
            text += scriptResultToString(WKArrayGetItemAtIndex(array, i), itemOk);
        }
        return text;
    }
    if (type == WKDictionaryGetTypeID())
        return "[object Object]";
    return "null";
}

void scriptFinished(WKTypeRef result, WKErrorRef error, void* context)
{
    std::unique_ptr<ScriptRequest> request(static_cast<ScriptRequest*>(context));
    crtweb_view* view = request->view;
    if (!view)
        return; // the view was destroyed first
    for (size_t i = 0; i < view->scripts.size(); ++i) {
        if (view->scripts[i] == request.get()) {
            view->scripts.erase(view->scripts.begin() + i);
            break;
        }
    }
    view->runtime->activity = true;
    bool ok = false;
    std::string text = error || !result ? std::string() : scriptResultToString(result, ok);
    if (request->callback) {
        if (ok && !error)
            request->callback(view, text.c_str(), 0, request->userData);
        else
            request->callback(view, nullptr, 1, request->userData);
    }
}

void detach(crtweb_view* view)
{
    // Stop WebKit calling back into a view the application no longer has.
    WKPageSetPageNavigationClient(view->page, nullptr);
    WKViewSetViewClient(view->view, nullptr);
    for (ScriptRequest* request : view->scripts)
        request->view = nullptr;
    view->scripts.clear();
    view->callbacks = { };
}

void release(crtweb_view* view)
{
    WKRelease(view->view);
    WKRelease(view->pageConfiguration);
    delete view;
}

// One WebKit run-loop pass: queued main-thread work, IPC dispatch, timers.
void runLoopPass()
{
    WKRunLoopCallOnMainThread([](void*) { WKRunLoopStopMain(); }, nullptr);
    WKRunLoopRunMain();
}

void pollState(crtweb_view* view)
{
    std::string title = takeString(WKPageCopyTitle(view->page));
    if (title != view->title) {
        view->title = title;
        view->runtime->activity = true;
        if (view->callbacks.on_title_changed)
            view->callbacks.on_title_changed(view, view->title.c_str(), view->callbacks.user_data);
        if (view->destroyed)
            return;
    }
    std::string url = takeURL(WKPageCopyActiveURL(view->page));
    if (url != view->url) {
        view->url = url;
        view->runtime->activity = true;
        if (view->callbacks.on_url_changed)
            view->callbacks.on_url_changed(view, view->url.c_str(), view->callbacks.user_data);
    }
}

} // namespace

// ---- public API ---------------------------------------------------------------------------------

extern "C" {

uint32_t crtweb_api_version(void)
{
    return CRTWEB_API_VERSION;
}

const char* crtweb_result_string(crtweb_result result)
{
    switch (result) {
    case CRTWEB_OK: return "ok";
    case CRTWEB_WOULD_BLOCK: return "would block";
    case CRTWEB_ERROR_INVALID_ARGUMENT: return "invalid argument";
    case CRTWEB_ERROR_STATE: return "invalid state or wrong thread";
    case CRTWEB_ERROR_OUT_OF_MEMORY: return "out of memory";
    case CRTWEB_ERROR_UNSUPPORTED: return "unsupported";
    case CRTWEB_ERROR_PLATFORM: return "platform error";
    }
    return "unknown";
}

crtweb_result crtweb_runtime_create(const crtweb_runtime_config* config, crtweb_runtime** out_runtime)
{
    if (!out_runtime)
        return CRTWEB_ERROR_INVALID_ARGUMENT;
    *out_runtime = nullptr;
    if (config && config->struct_size < sizeof(uint32_t) * 2)
        return CRTWEB_ERROR_INVALID_ARGUMENT;
    if (gRuntime)
        return CRTWEB_ERROR_STATE;

    if (config && config->struct_size >= offsetof(crtweb_runtime_config, process_directory) + sizeof(char*)
        && config->process_directory && *config->process_directory) {
#if defined(_WIN32)
        _putenv_s("CRTWEB_PROCESS_DIR", config->process_directory);
#else
        setenv("CRTWEB_PROCESS_DIR", config->process_directory, 1);
#endif
    }

    auto* runtime = new (std::nothrow) crtweb_runtime;
    if (!runtime)
        return CRTWEB_ERROR_OUT_OF_MEMORY;
    runtime->owner = std::this_thread::get_id();
    if (config && config->struct_size >= offsetof(crtweb_runtime_config, application_name) + sizeof(char*)
        && config->application_name)
        runtime->applicationName = config->application_name;

    if (!gRunLoopInitialized) {
        WKRunLoopInitializeMain();
        gRunLoopInitialized = true;
    }
    WKContextConfigurationRef contextConfiguration = WKContextConfigurationCreate();
    runtime->context = WKContextCreateWithConfiguration(contextConfiguration);
    WKRelease(contextConfiguration);
    if (!runtime->context) {
        delete runtime;
        return CRTWEB_ERROR_PLATFORM;
    }
    gRuntime = runtime;
    *out_runtime = runtime;
    return CRTWEB_OK;
}

crtweb_result crtweb_runtime_pump(crtweb_runtime* runtime, uint32_t timeout_ms)
{
    if (!runtime)
        return CRTWEB_ERROR_INVALID_ARGUMENT;
    if (!onOwnerThread(runtime) || runtime->pumping)
        return CRTWEB_ERROR_STATE;
    runtime->pumping = true;
    runtime->activity = false;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    for (;;) {
        runLoopPass();
        // Views may be created, destroyed or reloaded from callbacks: iterate a copy and skip the
        // ones that went away.
        std::vector<crtweb_view*> snapshot = runtime->views;
        for (crtweb_view* view : snapshot) {
            if (view->destroyed)
                continue;
            pollState(view);
            if (!view->destroyed && view->damaged) {
                view->damaged = false;
                ++view->serial;
                view->painted = true;
                runtime->activity = true;
                if (view->callbacks.on_frame)
                    view->callbacks.on_frame(view, view->serial, view->callbacks.user_data);
            }
        }
        if (runtime->activity || std::chrono::steady_clock::now() >= deadline)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    runtime->pumping = false;
    for (crtweb_view* view : runtime->doomed)
        release(view);
    runtime->doomed.clear();
    return CRTWEB_OK;
}

void crtweb_runtime_destroy(crtweb_runtime* runtime)
{
    if (!runtime || !onOwnerThread(runtime) || runtime != gRuntime || runtime->pumping)
        return;
    std::vector<crtweb_view*> views = runtime->views;
    for (crtweb_view* view : views)
        crtweb_view_destroy(view);
    WKRelease(runtime->context);
    runtime->context = nullptr;
    // The Web process exits on its own once nothing references it; let the run loop deliver that.
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(300);
    while (std::chrono::steady_clock::now() < deadline) {
        runLoopPass();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    gRuntime = nullptr;
    delete runtime;
}

crtweb_result crtweb_view_create(crtweb_runtime* runtime, const crtweb_view_config* config, crtweb_view** out_view)
{
    if (!runtime || !config || !out_view || config->struct_size < offsetof(crtweb_view_config, scale_percent))
        return CRTWEB_ERROR_INVALID_ARGUMENT;
    *out_view = nullptr;
    if (!onOwnerThread(runtime))
        return CRTWEB_ERROR_STATE;
    if (!config->width || !config->height)
        return CRTWEB_ERROR_INVALID_ARGUMENT;
    if (config->struct_size >= sizeof(crtweb_view_config) && config->scale_percent && config->scale_percent != 100)
        return CRTWEB_ERROR_UNSUPPORTED;

    auto* view = new (std::nothrow) crtweb_view;
    if (!view)
        return CRTWEB_ERROR_OUT_OF_MEMORY;
    view->runtime = runtime;
    view->width = config->width;
    view->height = config->height;
    view->pageConfiguration = WKPageConfigurationCreate();
    WKPageConfigurationSetContext(view->pageConfiguration, runtime->context);
    view->view = WKViewCreate(view->pageConfiguration);
    if (!view->view) {
        WKRelease(view->pageConfiguration);
        delete view;
        return CRTWEB_ERROR_PLATFORM;
    }
    view->page = WKViewGetPage(view->view);

    WKViewClientV0 viewClient = { };
    viewClient.base.version = 0;
    viewClient.base.clientInfo = view;
    viewClient.setViewNeedsDisplay = setViewNeedsDisplay;
    WKViewSetViewClient(view->view, &viewClient.base);

    WKPageNavigationClientV0 navigationClient = { };
    navigationClient.base.version = 0;
    navigationClient.base.clientInfo = view;
    navigationClient.decidePolicyForNavigationAction = decidePolicyForNavigationAction;
    navigationClient.didStartProvisionalNavigation = didStartProvisionalNavigation;
    navigationClient.didFailProvisionalNavigation = didFailProvisionalNavigation;
    navigationClient.didCommitNavigation = didCommitNavigation;
    navigationClient.didFinishNavigation = didFinishNavigation;
    navigationClient.didFailNavigation = didFailNavigation;
    navigationClient.webProcessDidCrash = webProcessDidCrash;
    WKPageSetPageNavigationClient(view->page, &navigationClient.base);

    if (!runtime->applicationName.empty()) {
        WKStringRef name = WKStringCreateWithUTF8CString(runtime->applicationName.c_str());
        WKPageSetApplicationNameForUserAgent(view->page, name);
        WKRelease(name);

    }

    WKViewCRTSetSize(view->view, view->width, view->height);
    WKViewSetActive(view->view, true);
    WKViewSetVisible(view->view, true);

    runtime->views.push_back(view);
    *out_view = view;
    return CRTWEB_OK;
}

void crtweb_view_destroy(crtweb_view* view)
{
    if (!view || view->destroyed || !onOwnerThread(view->runtime))
        return;
    crtweb_runtime* runtime = view->runtime;
    view->destroyed = true;
    for (size_t i = 0; i < runtime->views.size(); ++i) {
        if (runtime->views[i] == view) {
            runtime->views.erase(runtime->views.begin() + i);
            break;
        }
    }
    detach(view);
    if (runtime->pumping)
        runtime->doomed.push_back(view);
    else
        release(view);
}

#define CRTWEB_CHECK_VIEW(view)                                                              \
    if (!(view))                                                                             \
        return CRTWEB_ERROR_INVALID_ARGUMENT;                                                \
    if ((view)->destroyed || !onOwnerThread((view)->runtime))                                \
        return CRTWEB_ERROR_STATE

crtweb_result crtweb_view_load_url(crtweb_view* view, const char* url)
{
    CRTWEB_CHECK_VIEW(view);
    if (!url || !*url || !hasAllowedScheme(url))
        return CRTWEB_ERROR_INVALID_ARGUMENT;
    WKURLRef wkURL = WKURLCreateWithUTF8CString(url);
    if (!wkURL)
        return CRTWEB_ERROR_INVALID_ARGUMENT;
    view->terminatedReported = false;
    WKPageLoadURL(view->page, wkURL);
    WKRelease(wkURL);
    return CRTWEB_OK;
}

crtweb_result crtweb_view_load_html(crtweb_view* view, const char* html, const char* base_url)
{
    CRTWEB_CHECK_VIEW(view);
    if (!html)
        return CRTWEB_ERROR_INVALID_ARGUMENT;
    if (base_url && *base_url && !hasAllowedScheme(base_url))
        return CRTWEB_ERROR_INVALID_ARGUMENT;
    WKStringRef string = WKStringCreateWithUTF8CString(html);
    WKURLRef base = WKURLCreateWithUTF8CString(base_url && *base_url ? base_url : "about:blank");
    view->terminatedReported = false;
    WKPageLoadHTMLString(view->page, string, base);
    WKRelease(base);
    WKRelease(string);
    return CRTWEB_OK;
}

crtweb_result crtweb_view_stop(crtweb_view* view)
{
    CRTWEB_CHECK_VIEW(view);
    WKPageStopLoading(view->page);
    return CRTWEB_OK;
}

crtweb_result crtweb_view_reload(crtweb_view* view)
{
    CRTWEB_CHECK_VIEW(view);
    view->terminatedReported = false;
    WKPageReload(view->page);
    return CRTWEB_OK;
}

crtweb_result crtweb_view_go_back(crtweb_view* view)
{
    CRTWEB_CHECK_VIEW(view);
    if (!WKPageCanGoBack(view->page))
        return CRTWEB_WOULD_BLOCK;
    WKPageGoBack(view->page);
    return CRTWEB_OK;
}

crtweb_result crtweb_view_go_forward(crtweb_view* view)
{
    CRTWEB_CHECK_VIEW(view);
    if (!WKPageCanGoForward(view->page))
        return CRTWEB_WOULD_BLOCK;
    WKPageGoForward(view->page);
    return CRTWEB_OK;
}

int crtweb_view_can_go_back(const crtweb_view* view)
{
    if (!view || view->destroyed || !onOwnerThread(view->runtime))
        return 0;
    return WKPageCanGoBack(view->page) ? 1 : 0;
}

int crtweb_view_can_go_forward(const crtweb_view* view)
{
    if (!view || view->destroyed || !onOwnerThread(view->runtime))
        return 0;
    return WKPageCanGoForward(view->page) ? 1 : 0;
}

crtweb_result crtweb_view_get_title(const crtweb_view* view, char* buffer, size_t capacity, size_t* out_needed)
{
    CRTWEB_CHECK_VIEW(view);
    return copyOut(takeString(WKPageCopyTitle(view->page)), buffer, capacity, out_needed);
}

crtweb_result crtweb_view_get_url(const crtweb_view* view, char* buffer, size_t capacity, size_t* out_needed)
{
    CRTWEB_CHECK_VIEW(view);
    return copyOut(takeURL(WKPageCopyActiveURL(view->page)), buffer, capacity, out_needed);
}

crtweb_result crtweb_view_evaluate_script(crtweb_view* view, const char* script_utf8, crtweb_script_callback callback, void* user_data)
{
    CRTWEB_CHECK_VIEW(view);
    if (!script_utf8)
        return CRTWEB_ERROR_INVALID_ARGUMENT;
    auto* request = new (std::nothrow) ScriptRequest { view, callback, user_data };
    if (!request)
        return CRTWEB_ERROR_OUT_OF_MEMORY;
    view->scripts.push_back(request);
    WKStringRef script = WKStringCreateWithUTF8CString(script_utf8);
    WKPageEvaluateJavaScriptInMainFrame(view->page, script, request, scriptFinished);
    WKRelease(script);
    return CRTWEB_OK;
}

crtweb_result crtweb_view_set_size(crtweb_view* view, uint32_t width, uint32_t height, uint32_t scale_percent)
{
    CRTWEB_CHECK_VIEW(view);
    if (!width || !height)
        return CRTWEB_ERROR_INVALID_ARGUMENT;
    if (scale_percent && scale_percent != 100)
        return CRTWEB_ERROR_UNSUPPORTED;
    view->width = width;
    view->height = height;
    WKViewCRTSetSize(view->view, width, height);
    view->damaged = true;
    return CRTWEB_OK;
}

crtweb_result crtweb_view_set_focus(crtweb_view* view, int focused)
{
    CRTWEB_CHECK_VIEW(view);
    WKViewSetFocus(view->view, focused != 0);
    return CRTWEB_OK;
}

crtweb_result crtweb_view_set_visible(crtweb_view* view, int visible)
{
    CRTWEB_CHECK_VIEW(view);
    WKViewSetVisible(view->view, visible != 0);
    return CRTWEB_OK;
}

crtweb_result crtweb_view_get_frame_info(const crtweb_view* view, crtweb_frame_info* out_info)
{
    CRTWEB_CHECK_VIEW(view);
    if (!out_info || out_info->struct_size < sizeof(crtweb_frame_info))
        return CRTWEB_ERROR_INVALID_ARGUMENT;
    out_info->serial = view->serial;
    out_info->width = view->width;
    out_info->height = view->height;
    return CRTWEB_OK;
}

crtweb_result crtweb_view_copy_frame(crtweb_view* view, void* pixels, uint32_t width, uint32_t height, size_t stride_bytes, uint64_t* out_serial)
{
    CRTWEB_CHECK_VIEW(view);
    if (!pixels || !width || !height || stride_bytes < static_cast<size_t>(width) * 4)
        return CRTWEB_ERROR_INVALID_ARGUMENT;
    if (!view->painted)
        return CRTWEB_WOULD_BLOCK;
    if (!WKViewCRTCopyPixels(view->view, pixels, width, height, stride_bytes))
        return CRTWEB_WOULD_BLOCK;
    if (out_serial)
        *out_serial = view->serial;
    return CRTWEB_OK;
}

crtweb_result crtweb_view_pointer(crtweb_view* view, crtweb_view_pointer_action action, uint32_t button, double x, double y, uint32_t modifiers)
{
    CRTWEB_CHECK_VIEW(view);
    uint32_t wkAction;
    switch (action) {
    case CRTWEB_VIEW_POINTER_MOVE: wkAction = kWKViewCRTPointerMove; break;
    case CRTWEB_VIEW_POINTER_DOWN: wkAction = kWKViewCRTPointerDown; break;
    case CRTWEB_VIEW_POINTER_UP: wkAction = kWKViewCRTPointerUp; break;
    default: return CRTWEB_ERROR_INVALID_ARGUMENT;
    }
    WKViewCRTHandlePointer(view->view, wkAction, button, x, y, modifiers, 0);
    return CRTWEB_OK;
}

crtweb_result crtweb_view_wheel(crtweb_view* view, double x, double y, double delta_x, double delta_y)
{
    CRTWEB_CHECK_VIEW(view);
    WKViewCRTHandleWheel(view->view, x, y, delta_x, delta_y, 0);
    return CRTWEB_OK;
}

crtweb_result crtweb_view_key(crtweb_view* view, int down, int repeat, uint32_t keysym, uint32_t keycode, uint32_t modifiers)
{
    CRTWEB_CHECK_VIEW(view);
    if (!keycode)
        return CRTWEB_ERROR_INVALID_ARGUMENT;
    WKViewCRTHandleKey(view->view, down != 0, repeat != 0, keysym, keycode, modifiers, 0);
    return CRTWEB_OK;
}

crtweb_result crtweb_view_text(crtweb_view* view, const char* utf8)
{
    CRTWEB_CHECK_VIEW(view);
    if (!utf8)
        return CRTWEB_ERROR_INVALID_ARGUMENT;
    WKViewCRTHandleText(view->view, utf8);
    return CRTWEB_OK;
}

crtweb_result crtweb_view_set_callbacks(crtweb_view* view, const crtweb_view_callbacks* callbacks)
{
    CRTWEB_CHECK_VIEW(view);
    view->callbacks = { };
    if (callbacks) {
        if (callbacks->struct_size < offsetof(crtweb_view_callbacks, on_load))
            return CRTWEB_ERROR_INVALID_ARGUMENT;
        size_t size = callbacks->struct_size < sizeof(crtweb_view_callbacks) ? callbacks->struct_size : sizeof(crtweb_view_callbacks);
        memcpy(&view->callbacks, callbacks, size);
        view->callbacks.struct_size = sizeof(crtweb_view_callbacks);
    }
    return CRTWEB_OK;
}

} // extern "C"
