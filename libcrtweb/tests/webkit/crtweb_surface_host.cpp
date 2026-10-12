// Web Tranche 3B, milestone B6: a web surface *producer* host on the CRT WebKit port.
//
// The CRT counterpart of libcrtweb/platform/wpe-crt (the WPEPlatform module of Tranche 3A): it
// loads one page into a view, sends every painted frame to the consumer listening on a socket
// (libcrtweb/platform/crtweb_surface_producer.h, the frozen frame contract), forwards the
// consumer's input to the page (WKViewCRTHandle*, the frozen input contract), reports the document
// title (TITLE) and follows RESIZE and FOCUS, until the consumer says BYE. It is what the 3A
// acceptance consumer (surface_probe) is run against instead of the native WPE host.
//
// Arguments: page.html socket-path

#define BUILDING_CRT__ 1

#include <JavaScriptCore/JavaScript.h>
#include <WebKit/WKBase.h>
#include <WebKit/WKSerializedScriptValue.h>
#include <WebKit/WKContext.h>
#include <WebKit/WKContextConfigurationRef.h>
#include <WebKit/WKGeometry.h>
#include <WebKit/WKPage.h>
#include <WebKit/WKPageConfigurationRef.h>
#include <WebKit/WKRunLoop.h>
#include <WebKit/WKString.h>
#include <WebKit/WKType.h>
#include <WebKit/WKURL.h>
#include <WebKit/WKView.h>
#include <WebKit/WKViewCRT.h>

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "../../platform/crtweb_surface_producer.h"

static WKViewRef gView;
static crtweb_producer gProducer;
static uint32_t gWidth = 640;
static uint32_t gHeight = 480;
static bool gDirty;
static std::string gSentTitle;

static std::string readFile(const char* path)
{
    std::string content;
    if (FILE* file = fopen(path, "rb")) {
        char buffer[4096];
        size_t length;
        while ((length = fread(buffer, 1, sizeof buffer, file)) > 0)
            content.append(buffer, length);
        fclose(file);
    }
    return content;
}

static bool gLoaded;
static uint64_t gLoadedAtNs;

static void didFinishNavigation(WKPageRef, WKNavigationRef, WKTypeRef, const void*)
{
    gLoaded = true;
    gLoadedAtNs = crtweb_producer_now_ns();
}

static void setViewNeedsDisplay(WKViewRef, WKRect, const void*)
{
    gDirty = true;
}

static void onMessage(void*, const void* data)
{
    auto* header = static_cast<const crtweb_wire_header*>(data);
    switch (header->type) {
    case CRTWEB_C2P_RESIZE: {
        auto* resize = static_cast<const crtweb_wire_resize*>(data);
        if (resize->width && resize->height) {
            gWidth = resize->width;
            gHeight = resize->height;
            WKViewCRTSetSize(gView, gWidth, gHeight);
            gDirty = true;
        }
        break;
    }
    case CRTWEB_C2P_FOCUS:
        WKViewSetFocus(gView, static_cast<const crtweb_wire_focus*>(data)->focused != 0);
        break;
    case CRTWEB_C2P_POINTER: {
        auto* pointer = static_cast<const crtweb_wire_pointer*>(data);
        uint32_t action = 0;
        switch (pointer->action) {
        case CRTWEB_POINTER_MOVE:
        case CRTWEB_POINTER_ENTER:
            action = kWKViewCRTPointerMove;
            break;
        case CRTWEB_POINTER_DOWN:
            action = kWKViewCRTPointerDown;
            break;
        case CRTWEB_POINTER_UP:
            action = kWKViewCRTPointerUp;
            break;
        default:
            break; // LEAVE: WebKit has no leave event on this API
        }
        if (action)
            WKViewCRTHandlePointer(gView, action, pointer->button, pointer->x, pointer->y, pointer->modifiers, pointer->time_ms);
        break;
    }
    case CRTWEB_C2P_WHEEL: {
        auto* wheel = static_cast<const crtweb_wire_wheel*>(data);
        WKViewCRTHandleWheel(gView, wheel->x, wheel->y, wheel->delta_x, wheel->delta_y, wheel->time_ms);
        break;
    }
    case CRTWEB_C2P_KEY: {
        auto* key = static_cast<const crtweb_wire_key*>(data);
        WKViewCRTHandleKey(gView, key->down != 0, key->repeat != 0, key->keyval, key->keycode, key->modifiers, key->time_ms);
        break;
    }
    case CRTWEB_C2P_TEXT:
        WKViewCRTHandleText(gView, static_cast<const crtweb_wire_text*>(data)->utf8);
        if (getenv("CRTWEB_HOST_TRACE")) {
            WKStringRef script = WKStringCreateWithUTF8CString("(function(){function h(tag,css,attrs){var e=document.createElement(tag);e.style.cssText='position:fixed;visibility:hidden;'+css;if(tag=='button')e.textContent='Xg';document.body.appendChild(e);var r=e.getBoundingClientRect().height;e.remove();return r;}return ['input='+h('input','font:20px sans-serif'),'inputDejaVu='+h('input','font:20px DejaVu Sans Mono'),'inputLH='+h('input','font:20px sans-serif;line-height:24px'),'inputH='+h('input','font:20px sans-serif;height:30px'),'inputNoApp='+h('input','font:20px sans-serif;-webkit-appearance:none'),'button='+h('button','font:20px sans-serif'),'textarea='+h('textarea','font:20px sans-serif'),'select='+h('select','font:20px sans-serif')].join(';');})()");
            WKPageEvaluateJavaScriptInMainFrame(WKViewGetPage(gView), script, nullptr, [](WKTypeRef result, WKErrorRef, void*) {
                fprintf(stderr, "host trace: callback type %lu (string %lu, serialized %lu)\n", result ? (unsigned long)WKGetTypeID(result) : 0ul, (unsigned long)WKStringGetTypeID(), (unsigned long)WKSerializedScriptValueGetTypeID());
                if (result && WKGetTypeID(result) == WKStringGetTypeID()) {
                    WKStringRef string = static_cast<WKStringRef>(result);
                    std::vector<char> buffer(WKStringGetMaximumUTF8CStringSize(string));
                    WKStringGetUTF8CString(string, buffer.data(), buffer.size());
                    fprintf(stderr, "host trace: %s\n", buffer.data());
                } else if (result && WKGetTypeID(result) == WKSerializedScriptValueGetTypeID()) {
                    JSGlobalContextRef context = JSGlobalContextCreate(nullptr);
                    JSValueRef value = WKSerializedScriptValueDeserialize(static_cast<WKSerializedScriptValueRef>(result), context, nullptr);
                    if (value) {
                        JSStringRef text = JSValueToStringCopy(context, value, nullptr);
                        std::vector<char> buffer(JSStringGetMaximumUTF8CStringSize(text));
                        JSStringGetUTF8CString(text, buffer.data(), buffer.size());
                        fprintf(stderr, "host trace: %s\n", buffer.data());
                        JSStringRelease(text);
                    }
                    JSGlobalContextRelease(context);
                }
            });
        }
        break;
    default:
        break;
    }
}

int main(int argc, char** argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: %s page.html consumer-socket\n", argv[0]);
        return 2;
    }
    std::string html = readFile(argv[1]);
    if (html.empty()) {
        fprintf(stderr, "crtweb_surface_host: cannot read %s\n", argv[1]);
        return 2;
    }

    gProducer.on_message = onMessage;
    if (crtweb_producer_connect(&gProducer, argv[2], gWidth, gHeight, 100) != 0) {
        fprintf(stderr, "crtweb_surface_host: cannot connect to %s\n", argv[2]);
        return 1;
    }
    if (gProducer.requested_width && gProducer.requested_height) {
        gWidth = gProducer.requested_width;
        gHeight = gProducer.requested_height;
    }

    WKRunLoopInitializeMain();
    WKContextConfigurationRef contextConfiguration = WKContextConfigurationCreate();
    WKContextRef context = WKContextCreateWithConfiguration(contextConfiguration);
    WKPageConfigurationRef pageConfiguration = WKPageConfigurationCreate();
    WKPageConfigurationSetContext(pageConfiguration, context);

    gView = WKViewCreate(pageConfiguration);
    WKViewClientV0 viewClient = { };
    viewClient.base.version = 0;
    viewClient.setViewNeedsDisplay = setViewNeedsDisplay;
    WKViewSetViewClient(gView, &viewClient.base);
    WKViewSetSize(gView, WKSizeMake(gWidth, gHeight));
    WKViewSetActive(gView, true);
    WKViewSetVisible(gView, true);
    WKViewSetFocus(gView, true);

    WKPageRef page = WKViewGetPage(gView);
    WKPageNavigationClientV0 navigationClient = { };
    navigationClient.base.version = 0;
    navigationClient.didFinishNavigation = didFinishNavigation;
    WKPageSetPageNavigationClient(page, &navigationClient.base);

    WKStringRef htmlString = WKStringCreateWithUTF8CString(html.c_str());
    WKURLRef baseURL = WKURLCreateWithUTF8CString("about:blank");
    WKPageLoadHTMLString(page, htmlString, baseURL);

    alarm(300);
    signal(SIGALRM, [](int) { _exit(3); });
    printf("CRTWEB_HOST_RUNNING\n");
    fflush(stdout);

    std::vector<uint32_t> pixels;
    while (!gProducer.bye) {
        // Consumer messages first (this dispatches input), waiting a few ms for new ones.
        if (crtweb_producer_pump(&gProducer, 4) != 0) {
            fprintf(stderr, "crtweb_surface_host: the consumer connection ended (errno %d)\n", errno);
            break;
        }

        // Let the page, the Web process and the IPC run.
        WKRunLoopCallOnMainThread([](void*) { WKRunLoopStopMain(); }, nullptr);
        WKRunLoopRunMain();

        // The title is polled: the legacy loader client that reports changes does not accept a
        // title callback, and the navigation client has none.
        if (WKStringRef title = WKPageCopyTitle(page)) {
            std::vector<char> buffer(WKStringGetMaximumUTF8CStringSize(title));
            WKStringGetUTF8CString(title, buffer.data(), buffer.size());
            WKRelease(title);
            if (gSentTitle != buffer.data()) {
                gSentTitle = buffer.data();
                crtweb_producer_send_title(&gProducer, gSentTitle.c_str());
            }
        }

        // The first picture is the loaded page, not the blank view before it: the Web process paints
        // after didFinishNavigation reaches the UI process, so the first frame waits a moment (the
        // legacy C API has no "first paint" callback) and is then sent whatever the dirty state.
        if (gLoaded && gLoadedAtNs && crtweb_producer_now_ns() - gLoadedAtNs >= 250ull * 1000000ull) {
            gLoadedAtNs = 0;
            gDirty = true;
        }
        if (gDirty && gLoaded && !gLoadedAtNs) {
            pixels.assign(static_cast<size_t>(gWidth) * gHeight, 0);
            if (WKViewCRTCopyPixels(gView, pixels.data(), gWidth, gHeight, gWidth * 4)) {
                int result = crtweb_producer_try_present(&gProducer, pixels.data(), gWidth, gHeight, gWidth * 4, nullptr);
                if (result == 0) {
                    gDirty = false;
                    static unsigned dumped;
                    if (const char* prefix = getenv("CRTWEB_HOST_DUMP"); prefix && dumped < 3) {
                        std::string path = std::string(prefix) + std::to_string(dumped++) + ".ppm";
                        if (FILE* out = fopen(path.c_str(), "wb")) {
                            fprintf(out, "P6\n%u %u\n255\n", gWidth, gHeight);
                            for (uint32_t pixel : pixels) {
                                unsigned char rgb[3] = { static_cast<unsigned char>(pixel >> 16), static_cast<unsigned char>(pixel >> 8), static_cast<unsigned char>(pixel) };
                                fwrite(rgb, 1, 3, out);
                            }
                            fclose(out);
                        }
                    }
                }
                else if (result < 0) {
                    fprintf(stderr, "crtweb_surface_host: sending a frame failed (errno %d)\n", errno);
                    break;
                }
                // result 1: the newest picture waits (latest wins); it is copied again next time.
            }
            // An incomplete or stale picture (a resize not yet answered): the next update repaints.
        }
    }

    crtweb_producer_goodbye(&gProducer, 500);
    printf("crtweb_surface_host: done\n");
    return 0;
}
