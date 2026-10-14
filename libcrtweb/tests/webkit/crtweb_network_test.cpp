// Tranche 8A: an actual browser request through the public CRT API. A build-host-only Python
// fixture supplies fresh certificates and HTTP responses; this target links only CRT/libcrtweb.
#include "crtweb/crtweb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <chrono>
#include <string>

static bool finished, failed;
static std::string title;
static int failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "crtweb_network_test: FAILED line %d: %s\n", __LINE__, #c); ++failures; } } while (0)

int main(int argc, char** argv)
{
    // mode, CA file (or '-'), URL, success/failure. One process per immutable trust configuration.
    if (argc != 5) return 2;
    std::string ca;
    if (strcmp(argv[2], "-")) {
        FILE* file = fopen(argv[2], "rb");
        if (!file) return 2;
        char buffer[4096];
        size_t n;
        while ((n = fread(buffer, 1, sizeof buffer, file))) ca.append(buffer, n);
        fclose(file);
    }
    crtweb_network_config config = { };
    config.struct_size = sizeof config;
    config.ca_pem = ca.empty() ? nullptr : ca.c_str();
    config.insecure_skip_verify_for_local_development = 2;
    CHECK(crtweb_network_configure(&config) == CRTWEB_ERROR_INVALID_ARGUMENT);
    config.insecure_skip_verify_for_local_development = !strcmp(argv[1], "insecure");
    auto valid = config;
    config.struct_size = 1;
    CHECK(crtweb_network_configure(&config) == CRTWEB_ERROR_INVALID_ARGUMENT);
    config = valid;
    std::string oversized(CRTWEB_NETWORK_CA_PEM_MAX_SIZE + 1, 'x');
    config.ca_pem = oversized.c_str();
    CHECK(crtweb_network_configure(&config) == CRTWEB_ERROR_INVALID_ARGUMENT);
    config = valid;
    if (strcmp(argv[1], "default")) {
        CHECK(crtweb_network_configure(&config) == CRTWEB_OK);
        if (!strcmp(argv[1], "reset"))
            CHECK(crtweb_network_configure(nullptr) == CRTWEB_OK);
        // Verify configure copied the input, not the caller's memory.
        ca.assign(ca.size(), 'x');
    } else {
        // Ambient variables must not turn default HTTPS into an insecure request.
        setenv("CRTWEB_INTERNAL_INSECURE_TLS", "1", 1);
        setenv("CRTWEB_INTERNAL_CA_PEM", "ambient", 1);
    }
    crtweb_runtime* runtime = nullptr;
    CHECK(crtweb_runtime_create(nullptr, &runtime) == CRTWEB_OK);
    if (!runtime) return 1;
    CHECK(crtweb_network_configure(nullptr) == CRTWEB_ERROR_STATE);
    crtweb_view_config viewConfig = { };
    viewConfig.struct_size = sizeof viewConfig;
    viewConfig.width = 320;
    viewConfig.height = 200;
    crtweb_view* view = nullptr;
    CHECK(crtweb_view_create(runtime, &viewConfig, &view) == CRTWEB_OK);
    if (!view) { crtweb_runtime_destroy(runtime); return 1; }
    crtweb_view_callbacks callbacks = { };
    callbacks.struct_size = sizeof callbacks;
    callbacks.on_load = [](crtweb_view*, crtweb_load_event event, const char*, const char*, void*) {
        finished |= event == CRTWEB_LOAD_FINISHED;
        failed |= event == CRTWEB_LOAD_FAILED;
    };
    callbacks.on_title_changed = [](crtweb_view*, const char* text, void*) { title = text; };
    CHECK(crtweb_view_set_callbacks(view, &callbacks) == CRTWEB_OK);
    CHECK(crtweb_view_load_url(view, argv[3]) == CRTWEB_OK);
    bool success = !strcmp(argv[4], "success");
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while (std::chrono::steady_clock::now() < deadline && !failed && !(finished && title == "network:ok"))
        crtweb_runtime_pump(runtime, 20);
    CHECK(success ? (!failed && finished && title == "network:ok") : (failed && !finished));
    crtweb_runtime_destroy(runtime);
    CHECK(crtweb_network_configure(nullptr) == CRTWEB_ERROR_STATE);
    if (failures) return 1;
    printf("crtweb_network_test: ok\n");
    return 0;
}
