// libcrtweb: the CRT-owned web runtime and view API (Web Tranche 4) over the PlatformCRT port.
//
// The only translation unit that sees WebKit. Public contract: include/crtweb/crtweb.h and
// docs/design/libcrtweb_api_policy.md. Everything runs on the thread that created the first runtime;
// WebKit's run loop is driven by crtweb_runtime_pump() and every application callback is delivered
// from there.

#define BUILDING_CRT__ 1
#ifndef CRTWEB_BUILD_LIBRARY
#define CRTWEB_BUILD_LIBRARY 1
#endif

#include "crtweb/crtweb.h"

#include <WebKit/WKArray.h>
#include <WebKit/WKBase.h>
#include <WebKit/WKContext.h>
#include <WebKit/WKContextConfigurationRef.h>
#include <WebKit/WKContextDownloadClient.h>
#include <WebKit/WKCookieManager.h>
#include <WebKit/WKDictionary.h>
#include <WebKit/WKDownloadRef.h>
#include <WebKit/WKErrorRef.h>
#include <WebKit/WKFrameInfoRef.h>
#include <WebKit/WKFramePolicyListener.h>
#include <WebKit/WKGeometry.h>
#include <WebKit/WKHTTPCookieStoreRef.h>
#include <WebKit/WKNavigationActionRef.h>
#include <WebKit/WKNavigationResponseRef.h>
#include <WebKit/WKNumber.h>
#include <WebKit/WKPage.h>
#include <WebKit/WKPageConfigurationRef.h>
#include <WebKit/WKPageNavigationClient.h>
#include <WebKit/WKPageUIClient.h>
#include <WebKit/WKRunLoop.h>
#include <WebKit/WKString.h>
#include <WebKit/WKType.h>
#include <WebKit/WKURL.h>
#include <WebKit/WKURLRequest.h>
#include <WebKit/WKURLResponse.h>
#include <WebKit/WKView.h>
#include <WebKit/WKViewClient.h>
#include <WebKit/WKViewCRT.h>
#include <WebKit/WKWebsiteDataStoreConfigurationRef.h>
#include <WebKit/WKWebsiteDataStoreRef.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <new>
#include <ctype.h>
#include <string>
#include <strings.h>
#include <thread>
#include <type_traits>
#include <vector>

#if defined(_WIN32)
#include <direct.h>
#include <stdlib.h>
#endif

struct crtweb_view;

struct DataOperation;

struct crtweb_runtime {
    WKContextRef context { nullptr };
    WKWebsiteDataStoreRef dataStore { nullptr };
    std::string applicationName;
    std::vector<crtweb_view*> views;
    std::vector<DataOperation*> operations;
    std::vector<uint64_t> processTokens;
    uint32_t nextDownloadId { 0 };
};

struct DataOperation {
    crtweb_runtime* runtime;
    unsigned remaining;
    crtweb_data_callback callback;
    void* userData;
};

struct ScriptRequest {
    crtweb_view* view;
    crtweb_script_callback callback;
    void* userData;
};

struct DownloadState {
    crtweb_view* view { nullptr };
    uint32_t id { 0 };
    std::string url;
    std::string suggestedFilename;
    std::string mimeType;
    std::string path;
    uint64_t expected { 0 };
    uint64_t received { 0 };
};

enum class DialogType { Alert, Confirm, Prompt, BeforeUnload };

struct crtweb_dialog {
    crtweb_view* view;
    DialogType type;
    WKTypeRef listener; // retained
    bool completed { false };
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
    bool composing { false };
    std::string title;
    std::string url;
    std::vector<ScriptRequest*> scripts;
    std::vector<crtweb_dialog*> dialogs;
};

namespace {

std::vector<crtweb_runtime*> gRuntimes;
std::thread::id gOwner;
bool gRunLoopInitialized;
bool gNetworkConfigured;
bool gPumping;
bool gActivity;
std::vector<crtweb_view*> gDoomed; // destroyed from inside a pump: freed when the pump ends
std::map<WKDownloadRef, DownloadState> gDownloads;

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

bool onOwnerThread()
{
    return !gRuntimes.empty() && gOwner == std::this_thread::get_id();
}

bool knownRuntime(const crtweb_runtime* runtime)
{
    return std::find(gRuntimes.begin(), gRuntimes.end(), runtime) != gRuntimes.end();
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

void makeDirectory(const std::string& path)
{
#if defined(_WIN32)
    _mkdir(path.c_str());
#else
    mkdir(path.c_str(), 0700);
#endif
}

crtweb_view* findViewByPage(WKPageRef page)
{
    for (crtweb_runtime* runtime : gRuntimes) {
        for (crtweb_view* view : runtime->views) {
            if (view->page == page)
                return view;
        }
    }
    return nullptr;
}

// ---- view callbacks from WebKit ---------------------------------------------------------------

crtweb_view* viewFrom(const void* clientInfo)
{
    auto* view = const_cast<crtweb_view*>(static_cast<const crtweb_view*>(clientInfo));
    return view && !view->destroyed ? view : nullptr;
}

void deliverLoad(crtweb_view* view, crtweb_load_event event, const std::string& url, const char* message)
{
    gActivity = true;
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
    if (crtweb_view* view = viewFrom(clientInfo)) {
        // A reload after process death may start while crash notifications from the retired
        // process pool are still queued.  Keep the termination latch set until the replacement
        // process has actually committed; clearing it when load/reload is merely requested lets
        // a late old-generation notification masquerade as a second crash of the new page.
        view->terminatedReported = false;
        deliverLoad(view, CRTWEB_LOAD_COMMITTED, takeURL(WKPageCopyActiveURL(page)), nullptr);
    }
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
            gActivity = true;
            allow = view->callbacks.on_navigation_request(view, url.c_str(), view->callbacks.user_data) != 0;
        }
    }
    if (allow)
        WKFramePolicyListenerUse(listener);
    else
        WKFramePolicyListenerIgnore(listener);
}

// A response the engine will not show is a download: an attachment, or a type it cannot display.
void decidePolicyForNavigationResponse(WKPageRef, WKNavigationResponseRef response, WKFramePolicyListenerRef listener, WKTypeRef, const void*)
{
    bool download = !WKNavigationResponseCanShowMIMEType(response);
    if (!download) {
        if (WKURLResponseRef urlResponse = WKNavigationResponseCopyResponse(response)) {
            download = WKURLResponseIsAttachment(urlResponse);
            WKRelease(urlResponse);
        }
    }
    if (download)
        WKFramePolicyListenerDownload(listener);
    else
        WKFramePolicyListenerUse(listener);
}

void webProcessDidCrash(WKPageRef, const void* clientInfo)
{
    crtweb_view* view = viewFrom(clientInfo);
    if (!view || view->terminatedReported)
        return;
    view->terminatedReported = true;
    view->painted = false;
    view->damaged = false;
    view->composing = false;
    gActivity = true;
    if (view->callbacks.on_process_terminated)
        view->callbacks.on_process_terminated(view, view->callbacks.user_data);
}

void setViewNeedsDisplay(WKViewRef, WKRect, const void* clientInfo)
{
    if (crtweb_view* view = viewFrom(clientInfo))
        if (!view->terminatedReported)
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
    gActivity = true;
    bool ok = false;
    std::string text = error || !result ? std::string() : scriptResultToString(result, ok);
    if (request->callback) {
        if (ok && !error)
            request->callback(view, text.c_str(), 0, request->userData);
        else
            request->callback(view, nullptr, 1, request->userData);
    }
}

// ---- dialogs and popups -------------------------------------------------------------------------

void completeDialog(crtweb_dialog* dialog, bool accepted, const std::string* text)
{
    if (dialog->completed)
        return;
    dialog->completed = true;
    switch (dialog->type) {
    case DialogType::Alert:
        WKPageRunJavaScriptAlertResultListenerCall(static_cast<WKPageRunJavaScriptAlertResultListenerRef>(dialog->listener));
        break;
    case DialogType::Confirm:
        WKPageRunJavaScriptConfirmResultListenerCall(static_cast<WKPageRunJavaScriptConfirmResultListenerRef>(dialog->listener), accepted);
        break;
    case DialogType::Prompt: {
        WKStringRef answer = accepted ? WKStringCreateWithUTF8CString(text ? text->c_str() : "") : nullptr;
        WKPageRunJavaScriptPromptResultListenerCall(static_cast<WKPageRunJavaScriptPromptResultListenerRef>(dialog->listener), answer);
        if (answer)
            WKRelease(answer);
        break;
    }
    case DialogType::BeforeUnload:
        WKPageRunBeforeUnloadConfirmPanelResultListenerCall(static_cast<WKPageRunBeforeUnloadConfirmPanelResultListenerRef>(dialog->listener), accepted);
        break;
    }
    WKRelease(dialog->listener);
    dialog->listener = nullptr;
    if (crtweb_view* view = dialog->view) {
        auto& list = view->dialogs;
        list.erase(std::remove(list.begin(), list.end(), dialog), list.end());
    }
    delete dialog;
}

void showDialog(crtweb_view* view, DialogType type, WKTypeRef listener, const std::string& message, const std::string& defaultText)
{
    auto* dialog = new crtweb_dialog { view, type, WKRetain(listener), false };
    view->dialogs.push_back(dialog);
    gActivity = true;
    if (!view->callbacks.on_dialog) {
        // The documented defaults: an alert is dismissed, a confirm and a prompt are cancelled, leaving
        // a page is allowed.
        completeDialog(dialog, type == DialogType::Alert || type == DialogType::BeforeUnload, nullptr);
        return;
    }
    crtweb_dialog_kind kind = type == DialogType::Alert ? CRTWEB_DIALOG_ALERT
        : type == DialogType::Confirm ? CRTWEB_DIALOG_CONFIRM
        : type == DialogType::Prompt ? CRTWEB_DIALOG_PROMPT : CRTWEB_DIALOG_BEFORE_UNLOAD;
    view->callbacks.on_dialog(view, dialog, kind, message.c_str(), defaultText.c_str(), view->callbacks.user_data);
}

void runJavaScriptAlert(WKPageRef, WKStringRef message, WKFrameRef, WKSecurityOriginRef, WKPageRunJavaScriptAlertResultListenerRef listener, const void* clientInfo)
{
    if (crtweb_view* view = viewFrom(clientInfo))
        showDialog(view, DialogType::Alert, listener, toUTF8(message), std::string());
    else
        WKPageRunJavaScriptAlertResultListenerCall(listener);
}

void runJavaScriptConfirm(WKPageRef, WKStringRef message, WKFrameRef, WKSecurityOriginRef, WKPageRunJavaScriptConfirmResultListenerRef listener, const void* clientInfo)
{
    if (crtweb_view* view = viewFrom(clientInfo))
        showDialog(view, DialogType::Confirm, listener, toUTF8(message), std::string());
    else
        WKPageRunJavaScriptConfirmResultListenerCall(listener, false);
}

void runJavaScriptPrompt(WKPageRef, WKStringRef message, WKStringRef defaultValue, WKFrameRef, WKSecurityOriginRef, WKPageRunJavaScriptPromptResultListenerRef listener, const void* clientInfo)
{
    if (crtweb_view* view = viewFrom(clientInfo))
        showDialog(view, DialogType::Prompt, listener, toUTF8(message), toUTF8(defaultValue));
    else
        WKPageRunJavaScriptPromptResultListenerCall(listener, nullptr);
}

void runBeforeUnloadConfirmPanel(WKPageRef, WKStringRef message, WKFrameRef, WKPageRunBeforeUnloadConfirmPanelResultListenerRef listener, const void* clientInfo)
{
    if (crtweb_view* view = viewFrom(clientInfo))
        showDialog(view, DialogType::BeforeUnload, listener, toUTF8(message), std::string());
    else
        WKPageRunBeforeUnloadConfirmPanelResultListenerCall(listener, true);
}

void closePage(WKPageRef, const void* clientInfo)
{
    crtweb_view* view = viewFrom(clientInfo);
    if (!view)
        return;
    gActivity = true;
    if (view->callbacks.on_close_requested)
        view->callbacks.on_close_requested(view, view->callbacks.user_data);
}

void installClients(crtweb_view* view);

WKPageRef createNewPage(WKPageRef, WKPageConfigurationRef configuration, WKNavigationActionRef action, WKWindowFeaturesRef, const void* clientInfo)
{
    crtweb_view* opener = viewFrom(clientInfo);
    if (!opener || !opener->callbacks.on_popup_request)
        return nullptr;
    WKURLRequestRef request = WKNavigationActionCopyRequest(action);
    std::string url = takeURL(request ? WKURLRequestCopyURL(request) : nullptr);
    if (request)
        WKRelease(request);
    gActivity = true;
    if (!opener->callbacks.on_popup_request(opener, url.c_str(), opener->callbacks.user_data))
        return nullptr;

    auto* popup = new (std::nothrow) crtweb_view;
    if (!popup)
        return nullptr;
    popup->runtime = opener->runtime;
    popup->width = opener->width;
    popup->height = opener->height;
    popup->view = WKViewCreate(configuration);
    if (!popup->view) {
        delete popup;
        return nullptr;
    }
    popup->page = WKViewGetPage(popup->view);
    installClients(popup);
    WKViewCRTSetSize(popup->view, popup->width, popup->height);
    WKViewSetActive(popup->view, true);
    WKViewSetVisible(popup->view, true);
    opener->runtime->views.push_back(popup);
    if (opener->callbacks.on_popup_created)
        opener->callbacks.on_popup_created(opener, popup, opener->callbacks.user_data);
    // The engine adopts one reference of the returned page; the view keeps its own.
    return static_cast<WKPageRef>(const_cast<void*>(WKRetain(popup->page)));
}

void installClients(crtweb_view* view)
{
    WKViewClientV0 viewClient = { };
    viewClient.base.version = 0;
    viewClient.base.clientInfo = view;
    viewClient.setViewNeedsDisplay = setViewNeedsDisplay;
    WKViewSetViewClient(view->view, &viewClient.base);

    WKPageNavigationClientV0 navigationClient = { };
    navigationClient.base.version = 0;
    navigationClient.base.clientInfo = view;
    navigationClient.decidePolicyForNavigationAction = decidePolicyForNavigationAction;
    navigationClient.decidePolicyForNavigationResponse = decidePolicyForNavigationResponse;
    navigationClient.didStartProvisionalNavigation = didStartProvisionalNavigation;
    navigationClient.didFailProvisionalNavigation = didFailProvisionalNavigation;
    navigationClient.didCommitNavigation = didCommitNavigation;
    navigationClient.didFinishNavigation = didFinishNavigation;
    navigationClient.didFailNavigation = didFailNavigation;
    navigationClient.webProcessDidCrash = webProcessDidCrash;
    WKPageSetPageNavigationClient(view->page, &navigationClient.base);

    WKPageUIClientV9 uiClient = { };
    uiClient.base.version = 9;
    uiClient.base.clientInfo = view;
    uiClient.createNewPage = createNewPage;
    uiClient.close = closePage;
    uiClient.runJavaScriptAlert = runJavaScriptAlert;
    uiClient.runJavaScriptConfirm = runJavaScriptConfirm;
    uiClient.runJavaScriptPrompt = runJavaScriptPrompt;
    uiClient.runBeforeUnloadConfirmPanel = runBeforeUnloadConfirmPanel;
    WKPageSetPageUIClient(view->page, &uiClient.base);
}

// ---- downloads ----------------------------------------------------------------------------------

DownloadState* downloadFor(WKDownloadRef download)
{
    auto found = gDownloads.find(download);
    return found == gDownloads.end() ? nullptr : &found->second;
}

void downloadDidStart(WKContextRef, WKDownloadRef download, const void* clientInfo)
{
    auto* runtime = const_cast<crtweb_runtime*>(static_cast<const crtweb_runtime*>(clientInfo));
    DownloadState state;
    state.view = findViewByPage(WKDownloadGetOriginatingPage(download));
    state.id = ++runtime->nextDownloadId;
    if (WKURLRequestRef request = WKDownloadCopyRequest(download)) {
        state.url = takeURL(WKURLRequestCopyURL(request));
        WKRelease(request);
    }
    gDownloads[download] = state;
}

void downloadDidReceiveResponse(WKContextRef, WKDownloadRef download, WKURLResponseRef response, const void*)
{
    if (DownloadState* state = downloadFor(download); state && response) {
        state->mimeType = takeString(WKURLResponseCopyMIMEType(response));
        state->suggestedFilename = takeString(WKURLResponseCopySuggestedFilename(response));
        state->expected = WKURLResponseGetExpectedContentLength(response);
    }
}

WKStringRef downloadDecideDestination(WKContextRef, WKDownloadRef download, WKStringRef filename, bool* allowOverwrite, const void*)
{
    DownloadState* state = downloadFor(download);
    if (!state || !state->view || state->view->destroyed || !state->view->callbacks.on_download_request)
        return nullptr; // cancelled: no one to decide
    if (state->suggestedFilename.empty())
        state->suggestedFilename = toUTF8(filename);
    crtweb_download_info info = { };
    info.struct_size = sizeof info;
    info.id = state->id;
    info.url = state->url.c_str();
    info.suggested_filename = state->suggestedFilename.c_str();
    info.mime_type = state->mimeType.c_str();
    info.expected_length = state->expected;
    std::vector<char> path(4096, '\0');
    gActivity = true;
    crtweb_view* view = state->view;
    if (!view->callbacks.on_download_request(view, &info, path.data(), path.size(), view->callbacks.user_data) || !path[0])
        return nullptr;
    path.back() = '\0';
    state->path = path.data();
    if (allowOverwrite)
        *allowOverwrite = true;
    return WKStringCreateWithUTF8CString(state->path.c_str());
}

void downloadDidReceiveData(WKContextRef, WKDownloadRef download, uint64_t length, const void*)
{
    DownloadState* state = downloadFor(download);
    if (!state)
        return;
    state->received += length;
    crtweb_view* view = state->view;
    if (view && !view->destroyed && view->callbacks.on_download_progress) {
        gActivity = true;
        view->callbacks.on_download_progress(view, state->id, state->received, state->expected, view->callbacks.user_data);
    }
}

void finishDownload(WKDownloadRef download, crtweb_download_status status)
{
    DownloadState* state = downloadFor(download);
    if (!state)
        return;
    DownloadState done = *state;
    gDownloads.erase(download);
    crtweb_view* view = done.view;
    if (view && !view->destroyed && view->callbacks.on_download_finished) {
        gActivity = true;
        view->callbacks.on_download_finished(view, done.id, status, done.path.c_str(), view->callbacks.user_data);
    }
}

void downloadDidFinish(WKContextRef, WKDownloadRef download, const void*)
{
    finishDownload(download, CRTWEB_DOWNLOAD_OK);
}

void downloadDidFail(WKContextRef, WKDownloadRef download, WKErrorRef, const void*)
{
    finishDownload(download, CRTWEB_DOWNLOAD_FAILED);
}

void downloadDidCancel(WKContextRef, WKDownloadRef download, const void*)
{
    finishDownload(download, CRTWEB_DOWNLOAD_CANCELLED);
}

void detach(crtweb_view* view)
{
    // Stop WebKit calling back into a view the application no longer has.
    WKPageSetPageNavigationClient(view->page, nullptr);
    WKPageSetPageUIClient(view->page, nullptr);
    WKViewSetViewClient(view->view, nullptr);
    for (ScriptRequest* request : view->scripts)
        request->view = nullptr;
    view->scripts.clear();
    // Pending dialogs are answered as cancelled so the page is not left blocked.
    std::vector<crtweb_dialog*> dialogs = view->dialogs;
    for (crtweb_dialog* dialog : dialogs)
        completeDialog(dialog, false, nullptr);
    for (auto& entry : gDownloads) {
        if (entry.second.view == view) {
            entry.second.view = nullptr;
            WKDownloadCancel(entry.first, nullptr, nullptr);
        }
    }
    view->callbacks = { };
    WKPageClose(view->page);
}

void release(crtweb_view* view)
{
    WKRelease(view->view);
    if (view->pageConfiguration)
        WKRelease(view->pageConfiguration);
    delete view;
}

// One WebKit run-loop pass: queued main-thread work, IPC dispatch, timers.
void runLoopPass()
{
    WKRunLoopCallOnMainThread([](void*) { WKRunLoopStopMain(); }, nullptr);
    WKRunLoopRunMain();
}

void rememberProcesses(crtweb_runtime* runtime)
{
    auto remember = [&](uint64_t token) {
        if (token && std::find(runtime->processTokens.begin(), runtime->processTokens.end(), token)
            == runtime->processTokens.end()) runtime->processTokens.push_back(token);
    };
    // Forget completed launches so a long-lived browser retains no unbounded PID/token history.
    auto& tokens = runtime->processTokens;
    tokens.erase(std::remove_if(tokens.begin(), tokens.end(), [](uint64_t token) {
        return !WKViewCRTProcessIsRunning(token);
    }), tokens.end());
    for (auto* view : runtime->views)
        remember(WKViewCRTProcessToken(view->view));
    std::vector<uint64_t> pool(WKContextCRTProcessTokens(runtime->context, nullptr, 0));
    WKContextCRTProcessTokens(runtime->context, pool.data(), pool.size());
    for (uint64_t token : pool) remember(token);
    remember(WKWebsiteDataStoreCRTProcessToken(runtime->dataStore));
}

void pollState(crtweb_view* view)
{
    std::string title = takeString(WKPageCopyTitle(view->page));
    if (title != view->title) {
        view->title = title;
        gActivity = true;
        if (view->callbacks.on_title_changed)
            view->callbacks.on_title_changed(view, view->title.c_str(), view->callbacks.user_data);
        if (view->destroyed)
            return;
    }
    std::string url = takeURL(WKPageCopyActiveURL(view->page));
    if (url != view->url) {
        view->url = url;
        gActivity = true;
        if (view->callbacks.on_url_changed)
            view->callbacks.on_url_changed(view, view->url.c_str(), view->callbacks.user_data);
    }
}

void dataOperationStep(DataOperation* operation)
{
    if (--operation->remaining)
        return;
    crtweb_runtime* runtime = operation->runtime;
    if (runtime) {
        auto& list = runtime->operations;
        list.erase(std::remove(list.begin(), list.end(), operation), list.end());
        gActivity = true;
        if (operation->callback)
            operation->callback(runtime, CRTWEB_OK, operation->userData);
    }
    delete operation;
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

#define CRTWEB_HAS_FIELD(config, field) \
    ((config) && (config)->struct_size >= offsetof(std::remove_pointer_t<decltype(config)>, field) + sizeof((config)->field))

// A directory the application names with a Windows host path ("C:\dir", what getcwd() returns outside a
// rootfs) in the CRT's POSIX spelling ("/c/dir"). SQLite's unix VFS, which keeps the cookie database, takes
// a path that does not start with '/' for a relative one and prefixes the working directory to it
// ("/C:\dir/C:\dir/cookies.db": unable to open database file); the CRT's file API maps /c/dir to
// C:\dir itself. Other spellings pass through.
static std::string crtPath(const char* path)
{
#if defined(WTF_CRT_COFF_ASM)
    if (isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':' && (path[2] == '/' || path[2] == '\\' || !path[2])) {
        std::string result = "/";
        result += static_cast<char>(tolower(static_cast<unsigned char>(path[0])));
        for (const char* c = path + 2; *c; ++c)
            result += *c == '\\' ? '/' : *c;
        return result;
    }
#endif
    return path;
}

crtweb_result crtweb_network_configure(const crtweb_network_config* config)
{
    if (gRunLoopInitialized)
        return CRTWEB_ERROR_STATE;
    if (config && config->struct_size < offsetof(crtweb_network_config, insecure_skip_verify_for_local_development)
        + sizeof(config->insecure_skip_verify_for_local_development))
        return CRTWEB_ERROR_INVALID_ARGUMENT;
    const char* ca = config && config->ca_pem ? config->ca_pem : "";
    int insecure = config ? config->insecure_skip_verify_for_local_development : 0;
    if (strnlen(ca, CRTWEB_NETWORK_CA_PEM_MAX_SIZE + 1) > CRTWEB_NETWORK_CA_PEM_MAX_SIZE
        || (insecure != 0 && insecure != 1))
        return CRTWEB_ERROR_INVALID_ARGUMENT;
    // CRT's process launcher forwards the CRT environment on all hosts. These are private launch
    // parameters, not ambient user-facing overrides: runtime_create resets them unless this API was
    // used. Copy before returning; CurlSSLHandle in each child takes its own certificate snapshot.
    if (setenv("CRTWEB_INTERNAL_CA_PEM", ca, 1)
        || setenv("CRTWEB_INTERNAL_INSECURE_TLS", insecure ? "1" : "0", 1)) {
        gNetworkConfigured = false;
        return CRTWEB_ERROR_PLATFORM;
    }
    gNetworkConfigured = true;
    return CRTWEB_OK;
}

crtweb_result crtweb_runtime_create(const crtweb_runtime_config* config, crtweb_runtime** out_runtime)
{
    if (!out_runtime)
        return CRTWEB_ERROR_INVALID_ARGUMENT;
    *out_runtime = nullptr;
    if (config && config->struct_size < sizeof(uint32_t) * 2)
        return CRTWEB_ERROR_INVALID_ARGUMENT;
    if (!gRuntimes.empty() && !onOwnerThread())
        return CRTWEB_ERROR_STATE;
    if (!gRunLoopInitialized && !gNetworkConfigured) {
        crtweb_result status = crtweb_network_configure(nullptr);
        if (status != CRTWEB_OK)
            return status;
    }

    if (CRTWEB_HAS_FIELD(config, process_directory) && config->process_directory && *config->process_directory) {
#if defined(_WIN32)
        _putenv_s("CRTWEB_PROCESS_DIR", config->process_directory);
#else
        setenv("CRTWEB_PROCESS_DIR", config->process_directory, 1);
#endif
    }

    auto* runtime = new (std::nothrow) crtweb_runtime;
    if (!runtime)
        return CRTWEB_ERROR_OUT_OF_MEMORY;
    if (CRTWEB_HAS_FIELD(config, application_name) && config->application_name)
        runtime->applicationName = config->application_name;

    if (!gRunLoopInitialized) {
        WKRunLoopInitializeMain();
        gRunLoopInitialized = true;
    }

    // The website data store: ephemeral, or rooted in the data directory.
    if (CRTWEB_HAS_FIELD(config, data_directory) && config->data_directory && *config->data_directory) {
        std::string root = crtPath(config->data_directory);
        while (root.size() > 1 && (root.back() == '/' || root.back() == '\\'))
            root.pop_back();
        makeDirectory(root);
        WKWebsiteDataStoreConfigurationRef storeConfiguration = WKWebsiteDataStoreConfigurationCreate();
        auto set = [&](const char* name, void (*setter)(WKWebsiteDataStoreConfigurationRef, WKStringRef)) {
            std::string path = root + "/" + name;
            makeDirectory(path);
            WKStringRef string = WKStringCreateWithUTF8CString(path.c_str());
            setter(storeConfiguration, string);
            WKRelease(string);
        };
        set("cache", WKWebsiteDataStoreConfigurationSetNetworkCacheDirectory);
        set("localstorage", WKWebsiteDataStoreConfigurationSetLocalStorageDirectory);
        set("indexeddb", WKWebsiteDataStoreConfigurationSetIndexedDBDatabaseDirectory);
        set("general", WKWebsiteDataStoreConfigurationSetGeneralStorageDirectory);
        std::string cookies = root + "/cookies.db";
        WKStringRef cookieFile = WKStringCreateWithUTF8CString(cookies.c_str());
        WKWebsiteDataStoreConfigurationSetCookieStorageFile(storeConfiguration, cookieFile);
        WKRelease(cookieFile);
        runtime->dataStore = WKWebsiteDataStoreCreateWithConfiguration(storeConfiguration);
        WKRelease(storeConfiguration);
    } else {
        runtime->dataStore = WKWebsiteDataStoreCreateNonPersistentDataStore();
    }
    if (!runtime->dataStore) {
        delete runtime;
        return CRTWEB_ERROR_PLATFORM;
    }

    WKContextConfigurationRef contextConfiguration = WKContextConfigurationCreate();
    runtime->context = WKContextCreateWithConfiguration(contextConfiguration);
    WKRelease(contextConfiguration);
    if (!runtime->context) {
        WKRelease(runtime->dataStore);
        delete runtime;
        return CRTWEB_ERROR_PLATFORM;
    }

    WKContextDownloadClientV0 downloadClient = { };
    downloadClient.base.version = 0;
    downloadClient.base.clientInfo = runtime;
    downloadClient.didStart = downloadDidStart;
    downloadClient.didReceiveResponse = downloadDidReceiveResponse;
    downloadClient.decideDestinationWithSuggestedFilename = downloadDecideDestination;
    downloadClient.didReceiveData = downloadDidReceiveData;
    downloadClient.didFinish = downloadDidFinish;
    downloadClient.didFail = downloadDidFail;
    downloadClient.didCancel = downloadDidCancel;
    WKContextSetDownloadClient(runtime->context, &downloadClient.base);

    if (gRuntimes.empty())
        gOwner = std::this_thread::get_id();
    gRuntimes.push_back(runtime);
    *out_runtime = runtime;

    if (CRTWEB_HAS_FIELD(config, cookie_policy) && config->cookie_policy)
        crtweb_runtime_set_cookie_policy(runtime, static_cast<crtweb_cookie_policy>(config->cookie_policy));
    return CRTWEB_OK;
}

crtweb_result crtweb_runtime_set_cookie_policy(crtweb_runtime* runtime, crtweb_cookie_policy policy)
{
    if (!runtime || !knownRuntime(runtime))
        return CRTWEB_ERROR_INVALID_ARGUMENT;
    if (!onOwnerThread())
        return CRTWEB_ERROR_STATE;
    WKHTTPCookieAcceptPolicy wkPolicy;
    switch (policy) {
    case CRTWEB_COOKIES_ALWAYS: wkPolicy = kWKHTTPCookieAcceptPolicyAlways; break;
    case CRTWEB_COOKIES_NEVER: wkPolicy = kWKHTTPCookieAcceptPolicyNever; break;
    case CRTWEB_COOKIES_NO_THIRD_PARTY: wkPolicy = kWKHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain; break;
    default: return CRTWEB_ERROR_INVALID_ARGUMENT;
    }
    WKHTTPCookieStoreSetHTTPCookieAcceptPolicy(WKWebsiteDataStoreGetHTTPCookieStore(runtime->dataStore), wkPolicy, nullptr, [](void*) { });
    return CRTWEB_OK;
}

crtweb_result crtweb_runtime_clear_data(crtweb_runtime* runtime, uint32_t types, crtweb_data_callback callback, void* user_data)
{
    if (!runtime || !knownRuntime(runtime))
        return CRTWEB_ERROR_INVALID_ARGUMENT;
    if (!onOwnerThread())
        return CRTWEB_ERROR_STATE;
    types &= CRTWEB_DATA_COOKIES | CRTWEB_DATA_CACHE;
    if (!types)
        return CRTWEB_ERROR_INVALID_ARGUMENT;
    auto* operation = new (std::nothrow) DataOperation { runtime, 0, callback, user_data };
    if (!operation)
        return CRTWEB_ERROR_OUT_OF_MEMORY;
    operation->remaining = ((types & CRTWEB_DATA_COOKIES) ? 1u : 0u) + ((types & CRTWEB_DATA_CACHE) ? 1u : 0u);
    runtime->operations.push_back(operation);
    // `remaining` counts the requests still to finish; the step function deletes the operation.
    if (types & CRTWEB_DATA_COOKIES) {
        WKHTTPCookieStoreDeleteAllCookies(WKWebsiteDataStoreGetHTTPCookieStore(runtime->dataStore), operation,
            [](void* context) { dataOperationStep(static_cast<DataOperation*>(context)); });
    }
    if (types & CRTWEB_DATA_CACHE) {
        WKWebsiteDataStoreRemoveNetworkCache(runtime->dataStore, operation,
            [](void* context) { dataOperationStep(static_cast<DataOperation*>(context)); });
    }
    return CRTWEB_OK;
}

crtweb_result crtweb_runtime_pump(crtweb_runtime* runtime, uint32_t timeout_ms)
{
    if (!runtime)
        return CRTWEB_ERROR_INVALID_ARGUMENT;
    if (!knownRuntime(runtime))
        return CRTWEB_ERROR_STATE;
    if (!onOwnerThread() || gPumping)
        return CRTWEB_ERROR_STATE;
    gPumping = true;
    gActivity = false;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    for (;;) {
        runLoopPass();
        for (auto* each : gRuntimes) rememberProcesses(each);
        // Views may be created, destroyed or reloaded from callbacks: iterate a copy and skip the
        // ones that went away.
        std::vector<crtweb_view*> snapshot;
        for (crtweb_runtime* each : gRuntimes)
            snapshot.insert(snapshot.end(), each->views.begin(), each->views.end());
        for (crtweb_view* view : snapshot) {
            if (view->destroyed)
                continue;
            pollState(view);
            if (!view->destroyed && view->damaged) {
                view->damaged = false;
                ++view->serial;
                view->painted = true;
                gActivity = true;
                if (view->callbacks.on_frame)
                    view->callbacks.on_frame(view, view->serial, view->callbacks.user_data);
            }
        }
        if (gActivity || std::chrono::steady_clock::now() >= deadline)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    gPumping = false;
    for (crtweb_view* view : gDoomed)
        release(view);
    gDoomed.clear();
    return CRTWEB_OK;
}

void crtweb_runtime_destroy(crtweb_runtime* runtime)
{
    if (!runtime || !knownRuntime(runtime) || !onOwnerThread() || gPumping)
        return;
    rememberProcesses(runtime);
    std::vector<uint64_t> foreignTokens(WKViewCRTLiveProcessTokens(nullptr, 0));
    foreignTokens.resize(WKViewCRTLiveProcessTokens(foreignTokens.data(), foreignTokens.size()));
    for (uint64_t owned : runtime->processTokens)
        foreignTokens.erase(std::remove(foreignTokens.begin(), foreignTokens.end(), owned), foreignTokens.end());
    std::vector<crtweb_view*> views = runtime->views;
    for (crtweb_view* view : views)
        crtweb_view_destroy(view);
    // Closing an in-flight page can initialize its store's NetworkProcess. Capture that launch
    // after page close as well, before releasing the context/store which owns the process proxy.
    rememberProcesses(runtime);
    for (DataOperation* operation : runtime->operations)
        operation->runtime = nullptr; // its WebKit callback still arrives and frees it
    runtime->operations.clear();
    WKContextSetDownloadClient(runtime->context, nullptr);
    WKRelease(runtime->context);
    runtime->context = nullptr;
    WKRelease(runtime->dataStore);
    runtime->dataStore = nullptr;
    gRuntimes.erase(std::remove(gRuntimes.begin(), gRuntimes.end(), runtime), gRuntimes.end());
    // WebKit may share its default NetworkProcess across website data stores. A surviving runtime
    // owns that launch too; closing one runtime must neither wait for it nor terminate it.
    auto refreshOwnership = [&] {
        for (auto* other : gRuntimes) {
            rememberProcesses(other);
            for (uint64_t shared : other->processTokens) {
                auto& tokens = runtime->processTokens;
                tokens.erase(std::remove(tokens.begin(), tokens.end(), shared), tokens.end());
                if (std::find(foreignTokens.begin(), foreignTokens.end(), shared) == foreignTokens.end())
                    foreignTokens.push_back(shared);
            }
        }
        // Deferred page/session cleanup can launch a NetworkProcess after its data store has been
        // released. The launcher registry still owns that child. Include new launches while excluding
        // every launch which predated teardown or belongs to a surviving runtime's pool/store.
        std::vector<uint64_t> live(WKViewCRTLiveProcessTokens(nullptr, 0));
        live.resize(WKViewCRTLiveProcessTokens(live.data(), live.size()));
        for (uint64_t token : live)
            if (std::find(foreignTokens.begin(), foreignTokens.end(), token) == foreignTokens.end()
                && std::find(runtime->processTokens.begin(), runtime->processTokens.end(), token) == runtime->processTokens.end())
                runtime->processTokens.push_back(token);
    };
    refreshOwnership();
    auto running = [&] {
        return std::any_of(runtime->processTokens.begin(), runtime->processTokens.end(), WKViewCRTProcessIsRunning);
    };
    // Give cooperative close two seconds, then terminate only this runtime's still-owned launches.
    // The reaper's token retires after waitpid, so socket EOF alone cannot masquerade as teardown.
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    runLoopPass();
    refreshOwnership();
    while (running() && std::chrono::steady_clock::now() < deadline) {
        runLoopPass();
        refreshOwnership();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (running()) {
        for (uint64_t token : runtime->processTokens) WKViewCRTTerminateProcess(token);
        deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (running() && std::chrono::steady_clock::now() < deadline) {
            runLoopPass();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        if (running()) fprintf(stderr, "crtweb: child reaping exceeded shutdown deadline\n");
    }
    delete runtime;
}

crtweb_result crtweb_view_create(crtweb_runtime* runtime, const crtweb_view_config* config, crtweb_view** out_view)
{
    if (!runtime || !config || !out_view || config->struct_size < offsetof(crtweb_view_config, scale_percent))
        return CRTWEB_ERROR_INVALID_ARGUMENT;
    *out_view = nullptr;
    if (!knownRuntime(runtime) || !onOwnerThread())
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
    WKPageConfigurationSetWebsiteDataStore(view->pageConfiguration, runtime->dataStore);
    view->view = WKViewCreate(view->pageConfiguration);
    if (!view->view) {
        WKRelease(view->pageConfiguration);
        delete view;
        return CRTWEB_ERROR_PLATFORM;
    }
    view->page = WKViewGetPage(view->view);
    installClients(view);

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
    if (!view || view->destroyed || !onOwnerThread())
        return;
    crtweb_runtime* runtime = view->runtime;
    rememberProcesses(runtime);
    view->destroyed = true;
    for (size_t i = 0; i < runtime->views.size(); ++i) {
        if (runtime->views[i] == view) {
            runtime->views.erase(runtime->views.begin() + i);
            break;
        }
    }
    detach(view);
    if (gPumping)
        gDoomed.push_back(view);
    else
        release(view);
}

#define CRTWEB_CHECK_VIEW(view)                                                              \
    if (!(view))                                                                             \
        return CRTWEB_ERROR_INVALID_ARGUMENT;                                                \
    if ((view)->destroyed || !onOwnerThread())                                               \
        return CRTWEB_ERROR_STATE

// On Windows a CRT program meets host paths spelled "C:\dir" (getcwd() outside a rootfs): a `file:` URL made from
// one reaches the network stack, a POSIX-persona libcurl, as file:///C:/dir, which that libcurl rejects as a bad
// URL. The CRT's own spelling of a drive path is /c/dir, which its file API and libcurl both open. Rewritten
// here, at the one boundary where the application hands over URLs; other schemes and hosts are untouched.
static std::string localFileURL(const char* url)
{
#if defined(WTF_CRT_COFF_ASM)
    if (!strncasecmp(url, "file://", 7)) {
        const char* path = url + 7;
        if (*path == '/')
            ++path;
        if (isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':' && (path[2] == '/' || path[2] == '\\')) {
            std::string result = "file:///";
            result += static_cast<char>(tolower(static_cast<unsigned char>(path[0])));
            for (const char* c = path + 2; *c; ++c)
                result += *c == '\\' ? '/' : *c;
            return result;
        }
    }
#endif
    return url;
}

crtweb_result crtweb_view_load_url(crtweb_view* view, const char* url)
{
    CRTWEB_CHECK_VIEW(view);
    if (!url || !*url || !hasAllowedScheme(url))
        return CRTWEB_ERROR_INVALID_ARGUMENT;
    std::string localURL = localFileURL(url);
    WKURLRef wkURL = WKURLCreateWithUTF8CString(localURL.c_str());
    if (!wkURL)
        return CRTWEB_ERROR_INVALID_ARGUMENT;
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
    std::string localBase = base_url && *base_url ? localFileURL(base_url) : std::string("about:blank");
    WKURLRef base = WKURLCreateWithUTF8CString(localBase.c_str());
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
    if (!view || view->destroyed || !onOwnerThread())
        return 0;
    return WKPageCanGoBack(view->page) ? 1 : 0;
}

int crtweb_view_can_go_forward(const crtweb_view* view)
{
    if (!view || view->destroyed || !onOwnerThread())
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

crtweb_result crtweb_view_ime_set_composition(crtweb_view* view, const char* text_utf8, uint32_t cursor_start, uint32_t cursor_length)
{
    CRTWEB_CHECK_VIEW(view);
    if (!text_utf8)
        return CRTWEB_ERROR_INVALID_ARGUMENT;
    WKViewCRTSetComposition(view->view, text_utf8, cursor_start, cursor_length);
    view->composing = *text_utf8 != '\0';
    return CRTWEB_OK;
}

crtweb_result crtweb_view_ime_commit(crtweb_view* view, const char* text_utf8)
{
    CRTWEB_CHECK_VIEW(view);
    if (!text_utf8)
        return CRTWEB_ERROR_INVALID_ARGUMENT;
    if (view->composing) {
        // The final text replaces the composition and ends it.
        WKViewCRTConfirmCompositionWithText(view->view, text_utf8);
        view->composing = false;
    } else {
        WKViewCRTInsertText(view->view, text_utf8);
    }
    return CRTWEB_OK;
}

crtweb_result crtweb_view_ime_cancel(crtweb_view* view)
{
    CRTWEB_CHECK_VIEW(view);
    WKViewCRTCancelComposition(view->view);
    view->composing = false;
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

crtweb_result crtweb_dialog_complete(crtweb_dialog* dialog, int accepted, const char* text_utf8)
{
    if (!dialog || dialog->completed)
        return CRTWEB_ERROR_INVALID_ARGUMENT;
    if (!onOwnerThread())
        return CRTWEB_ERROR_STATE;
    std::string text = text_utf8 ? text_utf8 : "";
    completeDialog(dialog, accepted != 0, &text);
    return CRTWEB_OK;
}

} // extern "C"
