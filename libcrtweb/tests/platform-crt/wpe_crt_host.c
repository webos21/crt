/* Native host for the "crt" WPEPlatform display (Web Tranche 3A): the WPE UI process side.
 *
 * Not a CRT program (it links the native WPE WebKit build, GLib and the module in
 * libcrtweb/platform/wpe-crt). It creates a WebKitWebView on the "crt" display, loads a page, and
 * stays alive while the consumer is connected; frames and input go over the wire protocol.
 */
#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <wpe/webkit.h>

static GMainLoop *loop;
static int status = 1;

static void load_changed(WebKitWebView *view, WebKitLoadEvent event, gpointer user_data)
{
    (void)view;
    (void)user_data;
    if (event == WEBKIT_LOAD_FINISHED)
        g_print("WPE_CRT_HOST_LOADED\n");
}

/* WebKit does not push the document title into the WPE toplevel; the embedder does. */
static void title_changed(GObject *object, GParamSpec *spec, gpointer user_data)
{
    (void)spec;
    (void)user_data;
    WebKitWebView *view = WEBKIT_WEB_VIEW(object);
    WPEView *wpe_view = webkit_web_view_get_wpe_view(view);
    WPEToplevel *toplevel = wpe_view ? wpe_view_get_toplevel(wpe_view) : NULL;
    const char *title = webkit_web_view_get_title(view);
    if (toplevel && title)
        wpe_toplevel_set_title(toplevel, title);
}

static void view_closed(WebKitWebView *view, gpointer user_data)
{
    (void)view;
    (void)user_data;
    status = 0;
    g_main_loop_quit(loop);
}

static void web_process_terminated(WebKitWebView *view, WebKitWebProcessTerminationReason reason, gpointer user_data)
{
    (void)view;
    (void)user_data;
    g_printerr("wpe-crt-host: WebProcess terminated (%d)\n", (int)reason);
    g_main_loop_quit(loop);
}

static gboolean timed_out(gpointer user_data)
{
    (void)user_data;
    g_printerr("wpe-crt-host: timed out\n");
    g_main_loop_quit(loop);
    return G_SOURCE_REMOVE;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s PAGE.html   (WPE_DISPLAY=crt, CRTWEB_SURFACE_SOCKET=<path>)\n", argv[0]);
        return 2;
    }
    GError *error = NULL;
    WPEDisplay *display = wpe_display_get_default();
    if (!display) {
        fprintf(stderr, "wpe-crt-host: no WPE display (is WPE_PLATFORMS_PATH set and WPE_DISPLAY=crt?)\n");
        return 1;
    }
    if (g_strcmp0(g_type_name(G_OBJECT_TYPE(display)), "WPEDisplayCRT")) {
        fprintf(stderr, "wpe-crt-host: the display is %s, not WPEDisplayCRT\n", g_type_name(G_OBJECT_TYPE(display)));
        return 1;
    }
    (void)error;

    WebKitWebView *view = WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW, "display", display, NULL));
    loop = g_main_loop_new(NULL, FALSE);
    g_signal_connect(view, "load-changed", G_CALLBACK(load_changed), NULL);
    g_signal_connect(view, "close", G_CALLBACK(view_closed), NULL);
    g_signal_connect(view, "notify::title", G_CALLBACK(title_changed), NULL);
    g_signal_connect(view, "web-process-terminated", G_CALLBACK(web_process_terminated), NULL);
    g_timeout_add_seconds(120, timed_out, NULL);

    GFile *file = g_file_new_for_commandline_arg(argv[1]);
    char *uri = g_file_get_uri(file);
    g_object_unref(file);
    webkit_web_view_load_uri(view, uri);
    g_free(uri);
    g_main_loop_run(loop);

    g_object_unref(view);
    g_main_loop_unref(loop);
    return status;
}
