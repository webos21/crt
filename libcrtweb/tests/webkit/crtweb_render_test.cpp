// Web Tranche 3B, milestone B5: first render.
//
// A UI process on WebKit's C API loads libcrtweb/tests/wpe-reference/reference.html (the Tranche 2
// fixture) into a 640x480 view, waits for the Web process to paint it, evaluates the Tranche 2 proof
// in the page, copies the backing store out and checks it:
//   * the proof string equals the one the native WPE reference produced:
//     "CRT WPE reference|local-html-ok|42|192x96" (DOM text, a dataset value set by script, and a
//     canvas size);
//   * the frame is 640x480 and has at least four distinct colours, among them the fixture's CSS
//     background, the aside, the canvas border and the two canvas fills;
//   * with a second argument, the frame goes through the web surface protocol (the 3A frame
//     producer contract) to the consumer listening on that socket.
// Arguments: reference.html [output.ppm] [consumer-socket-path].

#define BUILDING_CRT__ 1

#include <JavaScriptCore/JavaScript.h>
#include <WebKit/WKBase.h>
#include <WebKit/WKContext.h>
#include <WebKit/WKContextConfigurationRef.h>
#include <WebKit/WKGeometry.h>
#include <WebKit/WKPage.h>
#include <WebKit/WKPageConfigurationRef.h>
#include <WebKit/WKRunLoop.h>
#include <WebKit/WKSerializedScriptValue.h>
#include <WebKit/WKString.h>
#include <WebKit/WKType.h>
#include <WebKit/WKURL.h>
#include <WebKit/WKView.h>
#include <WebKit/WKViewCRT.h>

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <set>
#include <string>
#include <vector>

#include "../../platform/crtweb_surface_producer.h"

enum { ViewWidth = 640, ViewHeight = 480 };

static int failures;
static bool finishedNavigation;
static bool proofReceived;
static std::string proofText;
static unsigned needsDisplayCalls;
static bool webProcessCrashed;

static void check(bool condition, const char* what)
{
    if (condition)
        return;
    fprintf(stderr, "crtweb_render_test: FAILED %s\n", what);
    ++failures;
}

static std::string stringFromWK(WKStringRef string)
{
    std::vector<char> buffer(WKStringGetMaximumUTF8CStringSize(string));
    WKStringGetUTF8CString(string, buffer.data(), buffer.size());
    return buffer.data();
}

static void didFinishNavigation(WKPageRef, WKNavigationRef, WKTypeRef, const void*)
{
    finishedNavigation = true;
    WKRunLoopStopMain();
}

static void webProcessDidCrash(WKPageRef, const void*)
{
    webProcessCrashed = true;
    WKRunLoopStopMain();
}

static void setViewNeedsDisplay(WKViewRef, WKRect, const void*)
{
    ++needsDisplayCalls;
}

static void didEvaluate(WKTypeRef result, WKErrorRef, void*)
{
    proofReceived = true;
    if (result && WKGetTypeID(result) == WKStringGetTypeID())
        proofText = stringFromWK(static_cast<WKStringRef>(result));
    else if (result && WKGetTypeID(result) == WKSerializedScriptValueGetTypeID()) {
        JSGlobalContextRef context = JSGlobalContextCreate(nullptr);
        JSValueRef exception = nullptr;
        JSValueRef value = WKSerializedScriptValueDeserialize(static_cast<WKSerializedScriptValueRef>(result), context, &exception);
        if (value && JSValueIsString(context, value)) {
            JSStringRef text = JSValueToStringCopy(context, value, nullptr);
            std::vector<char> buffer(JSStringGetMaximumUTF8CStringSize(text));
            JSStringGetUTF8CString(text, buffer.data(), buffer.size());
            proofText = buffer.data();
            JSStringRelease(text);
        }
        JSGlobalContextRelease(context);
    }
    WKRunLoopStopMain();
}

static void stopTimer(void*)
{
    WKRunLoopStopMain();
}

// Run the main run loop for about `milliseconds` so painting and IPC settle.
static void spin(unsigned milliseconds)
{
    WKRunLoopCallOnMainThread([](void*) { }, nullptr);
    for (unsigned waited = 0; waited < milliseconds; waited += 20) {
        usleep(20 * 1000);
        WKRunLoopCallOnMainThread(stopTimer, nullptr);
        WKRunLoopRunMain();
    }
}

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

int main(int argc, char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s reference.html [output.ppm] [consumer-socket]\n", argv[0]);
        return 2;
    }
    std::string html = readFile(argv[1]);
    check(!html.empty(), "reference.html is readable");

    WKRunLoopInitializeMain();

    WKContextConfigurationRef contextConfiguration = WKContextConfigurationCreate();
    WKContextRef context = WKContextCreateWithConfiguration(contextConfiguration);
    WKPageConfigurationRef pageConfiguration = WKPageConfigurationCreate();
    WKPageConfigurationSetContext(pageConfiguration, context);

    WKViewRef view = WKViewCreate(pageConfiguration);
    WKViewClientV0 viewClient = { };
    viewClient.base.version = 0;
    viewClient.setViewNeedsDisplay = setViewNeedsDisplay;
    WKViewSetViewClient(view, &viewClient.base);
    WKViewSetSize(view, WKSizeMake(ViewWidth, ViewHeight));
    WKViewSetActive(view, true);
    WKViewSetVisible(view, true);
    WKViewSetFocus(view, true);

    WKPageRef page = WKViewGetPage(view);
    WKPageNavigationClientV0 navigationClient = { };
    navigationClient.base.version = 0;
    navigationClient.didFinishNavigation = didFinishNavigation;
    navigationClient.webProcessDidCrash = webProcessDidCrash;
    WKPageSetPageNavigationClient(page, &navigationClient.base);

    alarm(90);
    signal(SIGALRM, [](int) { _exit(2); });

    WKStringRef htmlString = WKStringCreateWithUTF8CString(html.c_str());
    WKURLRef baseURL = WKURLCreateWithUTF8CString("about:blank");
    WKPageLoadHTMLString(page, htmlString, baseURL);
    WKRunLoopRunMain();
    check(finishedNavigation && !webProcessCrashed, "reference.html loaded");

    // Let the Web process paint and the UI process compose.
    spin(500);
    printf("crtweb_render_test: %u setViewNeedsDisplay calls\n", needsDisplayCalls);
    check(needsDisplayCalls > 0, "the UI process was told the view needs display");

    // The Tranche 2 proof, evaluated in the page.
    static const char proofScript[] =
        "[document.title,document.querySelector('#marker').textContent,"
        "document.documentElement.dataset.referenceReady,"
        "document.querySelector('#proof').width+'x'+document.querySelector('#proof').height].join('|')";
    WKStringRef script = WKStringCreateWithUTF8CString(proofScript);
    WKPageEvaluateJavaScriptInMainFrame(page, script, nullptr, didEvaluate);
    WKRunLoopRunMain();
    check(proofReceived, "the proof script returned");
    printf("crtweb_render_test: proof %s\n", proofText.c_str());
    check(proofText == "CRT WPE reference|local-html-ok|42|192x96", "DOM/canvas proof equals the WPE reference's");

    // The pixels.
    std::vector<uint32_t> pixels(static_cast<size_t>(ViewWidth) * ViewHeight, 0);
    bool complete = WKViewCRTCopyPixels(view, pixels.data(), ViewWidth, ViewHeight, ViewWidth * 4);
    check(complete, "the whole 640x480 view was painted");

    std::set<uint32_t> colours(pixels.begin(), pixels.end());
    printf("crtweb_render_test: %zu distinct colours\n", colours.size());
    check(colours.size() >= 4, "at least four distinct colours");
    // B, G, R, A in memory = 0xAARRGGBB as a little-endian word.
    auto has = [&](uint32_t rgb) { return colours.count(0xff000000u | rgb) > 0; };
    check(has(0x10243c), "background #10243c present");
    check(has(0x1982c4), "aside #1982c4 present");
    check(has(0x8ac926), "canvas border #8ac926 present");
    check(has(0xff595e), "canvas fill #ff595e present");
    check(has(0x6a4c93), "canvas fill #6a4c93 present");

    if (argc >= 3) {
        if (FILE* out = fopen(argv[2], "wb")) {
            fprintf(out, "P6\n%d %d\n255\n", ViewWidth, ViewHeight);
            for (uint32_t pixel : pixels) {
                unsigned char rgb[3] = { static_cast<unsigned char>(pixel >> 16), static_cast<unsigned char>(pixel >> 8), static_cast<unsigned char>(pixel) };
                fwrite(rgb, 1, 3, out);
            }
            fclose(out);
        }
    }

    if (argc >= 4) {
        // Deliver the frame through the web surface protocol (the 3A frame producer contract).
        crtweb_producer producer = { };
        if (crtweb_producer_connect(&producer, argv[3], ViewWidth, ViewHeight, 100) != 0)
            check(false, "connected to the consumer and received CONFIG");
        else {
            check(crtweb_producer_present(&producer, pixels.data(), ViewWidth, ViewHeight, ViewWidth * 4, "CRT WPE reference") == 0,
                "the frame was presented");
            check(crtweb_producer_wait_ack(&producer, 5000) == 0, "the consumer acknowledged the frame");
            // The consumer says BYE when it has checked the frame; closing first would break its send.
            for (int i = 0; i < 100 && !producer.bye; ++i)
                crtweb_producer_pump(&producer, 50);
            crtweb_producer_close(&producer);
        }
    }

    alarm(0);
    if (failures)
        return 1;
    printf("crtweb_render_test: ok\n");
    return 0;
}
