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
// Usage: crtweb_host consumer-socket [--data-dir=DIR] [--cookie-policy=N] [initial-url]
//   DIR: persistent website data (cookies, cache, storage); without it the runtime is ephemeral.
//   N: 0 accept all cookies, 1 none, 2 first party only.

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

#include <map>
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

// A download decision the host is waiting for (the engine's request callback is synchronous).
uint32_t gDecisionFor;
bool gDecisionArrived;
bool gDecisionAccept;
std::string gDecisionPath;
std::map<uint32_t, crtweb_dialog*> gDialogs;

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
    gFrameReady = false;
    sendEvent(CRTWEB_EVENT_PROCESS_TERMINATED, 0, 0, std::string());
}

int onDownloadRequest(crtweb_view*, const crtweb_download_info* info, char* path, size_t capacity, void*)
{
    std::string argument = std::string(info->url) + std::string(1, '\0') + info->suggested_filename + std::string(1, '\0')
        + info->mime_type + std::string(1, '\0') + std::to_string(info->expected_length);
    gDecisionFor = info->id;
    gDecisionArrived = false;
    sendEvent(CRTWEB_EVENT_DOWNLOAD_REQUEST, info->id, 0, argument);
    // Wait for the application's answer, reading the socket meanwhile; no answer cancels.
    for (int waited = 0; !gDecisionArrived && !gProducer.bye && waited < 60000; waited += 20) {
        if (crtweb_producer_pump(&gProducer, 20) != 0)
            break;
    }
    if (!gDecisionArrived || !gDecisionAccept || gDecisionPath.empty() || gDecisionPath.size() >= capacity)
        return 0;
    memcpy(path, gDecisionPath.c_str(), gDecisionPath.size() + 1);
    return 1;
}

void onDownloadProgress(crtweb_view*, uint32_t id, uint64_t received, uint64_t expected, void*)
{
    sendEvent(CRTWEB_EVENT_DOWNLOAD_PROGRESS, id, 0,
        std::to_string(received) + std::string(1, '\0') + std::to_string(expected));
}

void onDownloadFinished(crtweb_view*, uint32_t id, crtweb_download_status status, const char* path, void*)
{
    sendEvent(CRTWEB_EVENT_DOWNLOAD_FINISHED, id, static_cast<uint32_t>(status), path ? path : "");
}

int onPopupRequest(crtweb_view*, const char* url, void*)
{
    sendEvent(CRTWEB_EVENT_POPUP_REQUESTED, 0, 0, url ? url : "");
    return 0; // the host shows one view; the application decides what to do with the request
}

void onCloseRequested(crtweb_view*, void*)
{
    sendEvent(CRTWEB_EVENT_CLOSE_REQUESTED, 0, 0, std::string());
}

void onDialog(crtweb_view*, crtweb_dialog* dialog, crtweb_dialog_kind kind, const char* message, const char* defaultText, void*)
{
    static uint32_t nextId;
    uint32_t id = ++nextId;
    gDialogs[id] = dialog;
    sendEvent(CRTWEB_EVENT_DIALOG, id, static_cast<uint32_t>(kind),
        std::string(message ? message : "") + std::string(1, '\0') + (defaultText ? defaultText : ""));
}

void onDataCleared(crtweb_runtime*, crtweb_result, void* data)
{
    sendEvent(CRTWEB_EVENT_DATA_CLEARED, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(data)), 0, std::string());
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
    case CRTWEB_COMMAND_IME_SET_COMPOSITION:
        crtweb_view_ime_set_composition(gView, argument.c_str(), command.id, command.flags);
        break;
    case CRTWEB_COMMAND_IME_COMMIT:
        crtweb_view_ime_commit(gView, argument.c_str());
        break;
    case CRTWEB_COMMAND_IME_CANCEL:
        crtweb_view_ime_cancel(gView);
        break;
    case CRTWEB_COMMAND_DIALOG_RESULT: {
        auto found = gDialogs.find(command.id);
        if (found != gDialogs.end()) {
            crtweb_dialog_complete(found->second, (command.flags & 1u) != 0, argument.c_str());
            gDialogs.erase(found);
        }
        break;
    }
    case CRTWEB_COMMAND_DOWNLOAD_DECISION:
        if (command.id == gDecisionFor) {
            gDecisionAccept = (command.flags & 1u) != 0;
            gDecisionPath = argument;
            gDecisionArrived = true;
        }
        break;
    case CRTWEB_COMMAND_CLEAR_DATA:
        if (crtweb_runtime_clear_data(gRuntime, command.flags, onDataCleared,
                reinterpret_cast<void*>(static_cast<uintptr_t>(command.id))) != CRTWEB_OK)
            sendEvent(CRTWEB_EVENT_DATA_CLEARED, command.id, 0, std::string());
        break;
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
    std::string dataDirectory;
    uint32_t cookiePolicy = 0;
    std::string caFile, caPEM;
    bool insecureTLS = false;
    const char* initialURL = nullptr;
    if (argc < 2) {
        fprintf(stderr, "usage: %s consumer-socket [--data-dir=DIR] [--cookie-policy=N] [--ca-pem-file=FILE] "
            "[--insecure-skip-verify-for-local-development] [initial-url]\n", argv[0]);
        return 2;
    }
    for (int i = 2; i < argc; ++i) {
        if (!strncmp(argv[i], "--data-dir=", 11))
            dataDirectory = argv[i] + 11;
        else if (!strncmp(argv[i], "--cookie-policy=", 16))
            cookiePolicy = static_cast<uint32_t>(atoi(argv[i] + 16));
        else if (!strncmp(argv[i], "--ca-pem-file=", 14))
            caFile = argv[i] + 14;
        else if (!strcmp(argv[i], "--insecure-skip-verify-for-local-development"))
            insecureTLS = true;
        else if (!strncmp(argv[i], "--", 2)) {
            fprintf(stderr, "crtweb_host: unknown option %s\n", argv[i]);
            return 2;
        }
        else
            initialURL = argv[i];
    }
    if (!caFile.empty()) {
        FILE* file = fopen(caFile.c_str(), "rb");
        if (!file) {
            fprintf(stderr, "crtweb_host: cannot open CA bundle\n");
            return 2;
        }
        char buffer[4096];
        size_t count;
        while ((count = fread(buffer, 1, sizeof buffer, file)) && caPEM.size() <= CRTWEB_NETWORK_CA_PEM_MAX_SIZE)
            caPEM.append(buffer, count);
        bool invalid = ferror(file) || caPEM.empty() || caPEM.size() > CRTWEB_NETWORK_CA_PEM_MAX_SIZE
            || caPEM.find('\0') != std::string::npos;
        fclose(file);
        if (invalid) {
            fprintf(stderr, "crtweb_host: CA bundle is unreadable, empty or too large\n");
            return 2;
        }
    }
    crtweb_network_config networkConfig = { };
    networkConfig.struct_size = sizeof networkConfig;
    networkConfig.ca_pem = caPEM.empty() ? nullptr : caPEM.c_str();
    networkConfig.insecure_skip_verify_for_local_development = insecureTLS;
    if (crtweb_network_configure(&networkConfig) != CRTWEB_OK) {
        fprintf(stderr, "crtweb_host: cannot configure network trust\n");
        return 1;
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
    runtimeConfig.application_name = "CRTWebHost";
    if (!dataDirectory.empty())
        runtimeConfig.data_directory = dataDirectory.c_str();
    runtimeConfig.cookie_policy = cookiePolicy;
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
    callbacks.on_download_request = onDownloadRequest;
    callbacks.on_download_progress = onDownloadProgress;
    callbacks.on_download_finished = onDownloadFinished;
    callbacks.on_popup_request = onPopupRequest;
    callbacks.on_close_requested = onCloseRequested;
    callbacks.on_dialog = onDialog;
    crtweb_view_set_callbacks(gView, &callbacks);
    crtweb_view_set_focus(gView, 1);
    if (initialURL)
        crtweb_view_load_url(gView, initialURL);

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
