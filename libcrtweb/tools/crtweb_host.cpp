// crtweb_host: one web view as a process that speaks the CRT web surface protocol.
//
// The out-of-process half of the crtui WebView (Web Tranche 4). It is built only from the public
// crtweb API (include/crtweb/crtweb.h) and the producer helper of the frozen 3A contract: it connects
// to the consumer (an application's crtweb::WebView) listening on a socket, renders one page, sends
// frames and titles back, turns the consumer's input messages into view input, and carries the page
// control extension (platform/crtweb_surface_control.h): navigation commands in, load/URL/history/
// script-result events out. Keeping WebKit in its own process is the architecture: a crtui/crtgfx
// application links its own static C runtime, Skia and host GPU/window libraries, which cannot share
// an address space with libWebKit.so (docs/design/libcrtweb_api_policy.md, "Why a separate process").
//
// Usage: crtweb_host consumer-socket [initial-url]

#include "crtweb/crtweb.h"

#include "../platform/crtweb_surface_control.h"
#include "../platform/crtweb_surface_producer.h"

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <vector>

namespace {

crtweb_runtime* gRuntime;
crtweb_view* gView;
crtweb_producer gProducer;
crtweb_blob_assembler gCommand;
uint32_t gWidth = 640;
uint32_t gHeight = 480;
bool gFrameReady;
int gHistory = -1;

void sendEvent(crtweb_event event, uint32_t id, uint32_t flags, const std::string& argument)
{
    crtweb_blob_send(gProducer.socket_fd, CRTWEB_P2C_EVENT, event, id, flags, argument.data(), argument.size());
}

void historyIfChanged()
{
    int state = (crtweb_view_can_go_back(gView) ? 1 : 0) | (crtweb_view_can_go_forward(gView) ? 2 : 0);
    if (state != gHistory) {
        gHistory = state;
        sendEvent(CRTWEB_EVENT_HISTORY, 0, static_cast<uint32_t>(state), std::string());
    }
}

void onLoad(crtweb_view*, crtweb_load_event event, const char* url, const char* message, void*)
{
    crtweb_event wire;
    std::string argument = url ? url : "";
    switch (event) {
    case CRTWEB_LOAD_STARTED: wire = CRTWEB_EVENT_LOAD_STARTED; break;
    case CRTWEB_LOAD_COMMITTED: wire = CRTWEB_EVENT_LOAD_COMMITTED; break;
    case CRTWEB_LOAD_FINISHED: wire = CRTWEB_EVENT_LOAD_FINISHED; break;
    case CRTWEB_LOAD_FAILED:
        wire = CRTWEB_EVENT_LOAD_FAILED;
        argument.push_back('\0');
        argument += message ? message : "";
        break;
    default: return;
    }
    sendEvent(wire, 0, 0, argument);
    if (event != CRTWEB_LOAD_STARTED)
        historyIfChanged();
}

void onTitle(crtweb_view*, const char* title, void*)
{
    crtweb_producer_send_title(&gProducer, title);
}

void onURL(crtweb_view*, const char* url, void*)
{
    sendEvent(CRTWEB_EVENT_URL_CHANGED, 0, 0, url);
    historyIfChanged();
}

void onFrame(crtweb_view*, uint64_t, void*)
{
    gFrameReady = true;
}

void onTerminated(crtweb_view*, void*)
{
    sendEvent(CRTWEB_EVENT_PROCESS_TERMINATED, 0, 0, std::string());
}

struct ScriptRequest {
    uint32_t id;
};

void onScript(crtweb_view*, const char* result, int error, void* data)
{
    std::unique_ptr<ScriptRequest> request(static_cast<ScriptRequest*>(data));
    sendEvent(CRTWEB_EVENT_SCRIPT_RESULT, request->id, error ? 1 : 0, result ? result : "");
}

void runCommand(const crtweb_blob_assembler& command)
{
    std::string argument(command.data, command.length);
    switch (command.code) {
    case CRTWEB_COMMAND_LOAD_URL:
        if (crtweb_view_load_url(gView, argument.c_str()) != CRTWEB_OK)
            sendEvent(CRTWEB_EVENT_LOAD_FAILED, 0, 0, argument + std::string(1, '\0') + "unsupported or malformed URL");
        break;
    case CRTWEB_COMMAND_LOAD_HTML: {
        size_t split = argument.find('\0');
        std::string html = argument.substr(0, split);
        std::string base = split == std::string::npos ? std::string() : argument.substr(split + 1);
        crtweb_view_load_html(gView, html.c_str(), base.empty() ? nullptr : base.c_str());
        break;
    }
    case CRTWEB_COMMAND_STOP: crtweb_view_stop(gView); break;
    case CRTWEB_COMMAND_RELOAD: crtweb_view_reload(gView); break;
    case CRTWEB_COMMAND_GO_BACK: crtweb_view_go_back(gView); break;
    case CRTWEB_COMMAND_GO_FORWARD: crtweb_view_go_forward(gView); break;
    case CRTWEB_COMMAND_EVALUATE: {
        auto* request = new ScriptRequest { command.id };
        if (crtweb_view_evaluate_script(gView, argument.c_str(), onScript, request) != CRTWEB_OK) {
            delete request;
            sendEvent(CRTWEB_EVENT_SCRIPT_RESULT, command.id, 1, std::string());
        }
        break;
    }
    default:
        break;
    }
}

void onMessage(void*, const void* data)
{
    auto* header = static_cast<const crtweb_wire_header*>(data);
    switch (header->type) {
    case CRTWEB_C2P_RESIZE: {
        auto* resize = static_cast<const crtweb_wire_resize*>(data);
        if (resize->width && resize->height) {
            gWidth = resize->width;
            gHeight = resize->height;
            crtweb_view_set_size(gView, gWidth, gHeight, 100);
            gFrameReady = true;
        }
        break;
    }
    case CRTWEB_C2P_FOCUS:
        crtweb_view_set_focus(gView, static_cast<const crtweb_wire_focus*>(data)->focused != 0);
        break;
    case CRTWEB_C2P_POINTER: {
        auto* pointer = static_cast<const crtweb_wire_pointer*>(data);
        crtweb_view_pointer_action action;
        switch (pointer->action) {
        case CRTWEB_POINTER_MOVE:
        case CRTWEB_POINTER_ENTER: action = CRTWEB_VIEW_POINTER_MOVE; break;
        case CRTWEB_POINTER_DOWN: action = CRTWEB_VIEW_POINTER_DOWN; break;
        case CRTWEB_POINTER_UP: action = CRTWEB_VIEW_POINTER_UP; break;
        default: return;
        }
        crtweb_view_pointer(gView, action, pointer->button, pointer->x, pointer->y, pointer->modifiers);
        break;
    }
    case CRTWEB_C2P_WHEEL: {
        auto* wheel = static_cast<const crtweb_wire_wheel*>(data);
        crtweb_view_wheel(gView, wheel->x, wheel->y, wheel->delta_x, wheel->delta_y);
        break;
    }
    case CRTWEB_C2P_KEY: {
        auto* key = static_cast<const crtweb_wire_key*>(data);
        crtweb_view_key(gView, key->down != 0, key->repeat != 0, key->keyval, key->keycode, key->modifiers);
        break;
    }
    case CRTWEB_C2P_TEXT: {
        char text[sizeof(static_cast<const crtweb_wire_text*>(data)->utf8) + 1] = { };
        memcpy(text, static_cast<const crtweb_wire_text*>(data)->utf8, sizeof(static_cast<const crtweb_wire_text*>(data)->utf8));
        crtweb_view_text(gView, text);
        break;
    }
    case CRTWEB_C2P_COMMAND:
        if (crtweb_blob_assembler_feed(&gCommand, static_cast<const crtweb_wire_blob*>(data)) == 1)
            runCommand(gCommand);
        break;
    default:
        break;
    }
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "usage: %s consumer-socket [initial-url]\n", argv[0]);
        return 2;
    }
    gProducer.on_message = onMessage;
    if (crtweb_producer_connect(&gProducer, argv[1], gWidth, gHeight, 100) != 0) {
        fprintf(stderr, "crtweb_host: cannot connect to %s\n", argv[1]);
        return 1;
    }
    if (gProducer.requested_width && gProducer.requested_height) {
        gWidth = gProducer.requested_width;
        gHeight = gProducer.requested_height;
    }

    crtweb_runtime_config runtimeConfig = { };
    runtimeConfig.struct_size = sizeof runtimeConfig;
    runtimeConfig.application_name = "CRTWebHost/1";
    if (crtweb_runtime_create(&runtimeConfig, &gRuntime) != CRTWEB_OK) {
        fprintf(stderr, "crtweb_host: cannot create the runtime\n");
        return 1;
    }
    crtweb_view_config viewConfig = { };
    viewConfig.struct_size = sizeof viewConfig;
    viewConfig.width = gWidth;
    viewConfig.height = gHeight;
    if (crtweb_view_create(gRuntime, &viewConfig, &gView) != CRTWEB_OK) {
        fprintf(stderr, "crtweb_host: cannot create the view\n");
        return 1;
    }
    crtweb_view_callbacks callbacks = { };
    callbacks.struct_size = sizeof callbacks;
    callbacks.on_load = onLoad;
    callbacks.on_title_changed = onTitle;
    callbacks.on_url_changed = onURL;
    callbacks.on_frame = onFrame;
    callbacks.on_process_terminated = onTerminated;
    crtweb_view_set_callbacks(gView, &callbacks);
    crtweb_view_set_focus(gView, 1);
    if (argc == 3)
        crtweb_view_load_url(gView, argv[2]);

    alarm(600);
    signal(SIGALRM, [](int) { _exit(3); });

    std::vector<uint32_t> pixels;
    while (!gProducer.bye) {
        if (crtweb_producer_pump(&gProducer, 2) != 0)
            break;
        crtweb_runtime_pump(gRuntime, 2);
        if (gFrameReady) {
            pixels.assign(static_cast<size_t>(gWidth) * gHeight, 0);
            if (crtweb_view_copy_frame(gView, pixels.data(), gWidth, gHeight, gWidth * 4, nullptr) == CRTWEB_OK) {
                int result = crtweb_producer_try_present(&gProducer, pixels.data(), gWidth, gHeight, gWidth * 4, nullptr);
                if (result == 0)
                    gFrameReady = false;
                else if (result < 0)
                    break;
                // 1: the consumer has not acknowledged yet; the newest picture is sent next time.
            }
        }
    }
    crtweb_producer_goodbye(&gProducer, 500);
    crtweb_view_destroy(gView);
    crtweb_runtime_destroy(gRuntime);
    return 0;
}
