/* Native Linux reference harness for Web Tranche 2.
 *
 * This is intentionally not a CRT program. It links the unmodified upstream
 * WPE WebKit build and asks WPEPlatform's built-in headless display to render a
 * local page. The resulting PPM is a convenient visual/diff artifact for the
 * later PlatformCRT implementation.
 */
#include <glib.h>
#include <jsc/jsc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wpe/headless/wpe-headless.h>
#include <wpe/webkit.h>

enum { VIEW_WIDTH = 640, VIEW_HEIGHT = 480 };

typedef struct {
    GMainLoop *loop;
    WebKitWebView *web_view;
    const char *output_path;
    gboolean finished;
    int status;
} Acceptance;

static void fail(Acceptance *acceptance, const char *message)
{
    if (acceptance->finished)
        return;
    g_printerr("wpe-reference: %s\n", message);
    acceptance->status = 1;
    acceptance->finished = TRUE;
    g_main_loop_quit(acceptance->loop);
}

static uint64_t fnv1a64(const unsigned char *data, gsize size)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    for (gsize i = 0; i < size; ++i) {
        hash ^= data[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static gboolean write_ppm(const char *path, WebKitImage *image)
{
    const int width = webkit_image_get_width(image);
    const int height = webkit_image_get_height(image);
    const guint stride = webkit_image_get_stride(image);
    gsize size = 0;
    const unsigned char *bytes = g_bytes_get_data(webkit_image_as_bytes(image), &size);
    if (width != VIEW_WIDTH || height != VIEW_HEIGHT || stride < (guint)width * 4
        || size < (gsize)stride * height)
        return FALSE;

    uint32_t colors[16] = { 0 };
    unsigned color_count = 0;
    for (int y = 0; y < height && color_count < G_N_ELEMENTS(colors); ++y) {
        const unsigned char *row = bytes + (gsize)y * stride;
        for (int x = 0; x < width && color_count < G_N_ELEMENTS(colors); ++x) {
            uint32_t color = ((uint32_t)row[x * 4 + 2] << 16)
                | ((uint32_t)row[x * 4 + 1] << 8) | row[x * 4];
            unsigned index = 0;
            while (index < color_count && colors[index] != color)
                ++index;
            if (index == color_count)
                colors[color_count++] = color;
        }
    }
    /* A successful compositor snapshot must contain the fixture's several CSS/canvas colors. */
    if (color_count < 4)
        return FALSE;

    FILE *output = fopen(path, "wb");
    if (!output)
        return FALSE;
    fprintf(output, "P6\n%d %d\n255\n", width, height);
    for (int y = 0; y < height; ++y) {
        const unsigned char *row = bytes + (gsize)y * stride;
        for (int x = 0; x < width; ++x) {
            /* WebKitImage's documented format is BGRA8888 on this host. */
            const unsigned char rgb[] = { row[x * 4 + 2], row[x * 4 + 1], row[x * 4] };
            if (fwrite(rgb, sizeof(rgb), 1, output) != 1) {
                fclose(output);
                return FALSE;
            }
        }
    }
    if (fclose(output) != 0)
        return FALSE;

    g_print("WPE_REFERENCE_SNAPSHOT width=%d height=%d stride=%u fnv1a64=%016" G_GINT64_MODIFIER "x\n",
        width, height, stride, (guint64)fnv1a64(bytes, size));
    return TRUE;
}

static void snapshot_finished(GObject *object, GAsyncResult *result, gpointer user_data)
{
    Acceptance *acceptance = user_data;
    GError *error = NULL;
    WebKitImage *image = webkit_web_view_get_snapshot_finish(WEBKIT_WEB_VIEW(object), result, &error);
    if (!image) {
        fail(acceptance, error ? error->message : "snapshot failed");
        g_clear_error(&error);
        return;
    }
    if (!write_ppm(acceptance->output_path, image)) {
        g_object_unref(image);
        fail(acceptance, "snapshot dimensions or PPM output are invalid");
        return;
    }
    g_object_unref(image);
    acceptance->finished = TRUE;
    acceptance->status = 0;
    g_main_loop_quit(acceptance->loop);
}

static void javascript_finished(GObject *object, GAsyncResult *result, gpointer user_data)
{
    Acceptance *acceptance = user_data;
    GError *error = NULL;
    JSCValue *value = webkit_web_view_evaluate_javascript_finish(WEBKIT_WEB_VIEW(object), result, &error);
    if (!value) {
        fail(acceptance, error ? error->message : "JavaScript evaluation failed");
        g_clear_error(&error);
        return;
    }
    char *text = jsc_value_to_string(value);
    g_object_unref(value);
    if (!text || strcmp(text, "CRT WPE reference|local-html-ok|42|192x96")) {
        g_free(text);
        fail(acceptance, "the rendered document did not produce the expected DOM/canvas proof");
        return;
    }
    g_print("WPE_REFERENCE_DOM %s\n", text);
    g_free(text);
    webkit_web_view_get_snapshot(acceptance->web_view, WEBKIT_SNAPSHOT_REGION_VISIBLE,
        WEBKIT_SNAPSHOT_OPTIONS_NONE, NULL, snapshot_finished, acceptance);
}

static void load_changed(WebKitWebView *web_view, WebKitLoadEvent event, gpointer user_data)
{
    Acceptance *acceptance = user_data;
    if (event != WEBKIT_LOAD_FINISHED || acceptance->finished)
        return;
    static const char proof[] =
        "[document.title,document.querySelector('#marker').textContent,"
        "document.documentElement.dataset.referenceReady,"
        "document.querySelector('#proof').width+'x'+document.querySelector('#proof').height].join('|')";
    webkit_web_view_evaluate_javascript(web_view, proof, -1, NULL, NULL, NULL,
        javascript_finished, acceptance);
}

static gboolean load_failed(WebKitWebView *web_view, WebKitLoadEvent event, const char *uri,
    GError *error, gpointer user_data)
{
    (void)web_view;
    (void)event;
    (void)uri;
    fail(user_data, error ? error->message : "page load failed");
    return TRUE;
}

static void web_process_terminated(WebKitWebView *web_view,
    WebKitWebProcessTerminationReason reason, gpointer user_data)
{
    (void)web_view;
    (void)reason;
    fail(user_data, "WebProcess terminated before the snapshot completed");
}

static gboolean timed_out(gpointer user_data)
{
    fail(user_data, "timed out after 30 seconds");
    return G_SOURCE_REMOVE;
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: %s REFERENCE.html SNAPSHOT.ppm\n", argv[0]);
        return 2;
    }

    GError *error = NULL;
    WPEDisplay *display = wpe_display_headless_new();
    if (!wpe_display_connect(display, &error)) {
        fprintf(stderr, "wpe-reference: headless display: %s\n", error->message);
        g_error_free(error);
        g_object_unref(display);
        return 1;
    }

    WebKitWebView *web_view = WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
        "display", display, NULL));
    g_object_unref(display);
    WPEView *wpe_view = webkit_web_view_get_wpe_view(web_view);
    WPEToplevel *toplevel = wpe_view_get_toplevel(wpe_view);
    if (!toplevel || !wpe_toplevel_resize(toplevel, VIEW_WIDTH, VIEW_HEIGHT)) {
        fprintf(stderr, "wpe-reference: could not resize the headless toplevel\n");
        g_object_unref(web_view);
        return 1;
    }

    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    Acceptance acceptance = { loop, web_view, argv[2], FALSE, 1 };
    g_signal_connect(web_view, "load-changed", G_CALLBACK(load_changed), &acceptance);
    g_signal_connect(web_view, "load-failed", G_CALLBACK(load_failed), &acceptance);
    g_signal_connect(web_view, "web-process-terminated", G_CALLBACK(web_process_terminated), &acceptance);
    guint timeout = g_timeout_add_seconds(30, timed_out, &acceptance);

    GFile *file = g_file_new_for_commandline_arg(argv[1]);
    char *uri = g_file_get_uri(file);
    g_object_unref(file);
    webkit_web_view_load_uri(web_view, uri);
    g_free(uri);
    g_main_loop_run(loop);

    if (!acceptance.finished)
        acceptance.status = 1;
    if (g_main_context_find_source_by_id(NULL, timeout))
        g_source_remove(timeout);
    g_object_unref(web_view);
    g_main_loop_unref(loop);
    return acceptance.status;
}
