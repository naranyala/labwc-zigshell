/*
 * ocws-workspace-mgr.c — OCWS Workspace Manager (Kanban Board)
 *
 * Lists all toplevel Wayland windows via wlr-foreign-toplevel-management.
 * Shows them in workspace columns, allows focus/close/minimize via the
 * wlr protocol.  Move-to-workspace uses xdotool (XWayland fallback).
 *
 * Runtime deps: wayland-client (linked), xdotool (optional, for move)
 */

#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>
#include <glib.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "wlr-foreign-toplevel-management-unstable-v1-client.h"
#include "../libocws/gtk.h"

#define APP_ID           "org.ocws.workspace-mgr"
#define PRESETS_FILE     ".config/ocws/workspace-presets.ini"
#define RC_FILE          ".config/labwc/rc.xml"

/* ================================================================
 * Data types
 * ================================================================ */

typedef struct {
    struct zwlr_foreign_toplevel_handle_v1 *handle;
    char        *title;
    char        *app_id;
    gboolean     active;
} WindowInfo;

typedef struct {
    int         number;
    char        name[64];
    GList      *windows;
    GtkWidget  *column;
    GtkWidget  *list_box;
} WorkspaceData;

typedef struct {
    char name[64];
    char command[256];
    char icon[64];
} PresetApp;

/* ================================================================
 * Globals
 * ================================================================ */

static GList       *g_workspaces = NULL;
static int          g_ws_count   = 5;
static PresetApp   *g_presets    = NULL;
static int          g_npresets   = 0;
static GtkWidget   *g_scroll     = NULL;
static GtkWidget   *g_main_box   = NULL;
static GtkWidget   *g_refresh_btn = NULL;

/* Wayland protocol state */
static struct wl_display                          *g_wl_dpy  = NULL;
static struct zwlr_foreign_toplevel_manager_v1    *g_wl_mgr  = NULL;
static struct wl_seat                             *g_wl_seat = NULL;
static GList                                      *g_wl_wins = NULL; /* WindowInfo* */

/* ================================================================
 * Utilities
 * ================================================================ */

static char* run_cmd(const char *fmt, ...) {
    char cmd[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(cmd, sizeof(cmd), fmt, ap);
    va_end(ap);

    FILE *fp = popen(cmd, "r");
    if (!fp) return g_strdup("");

    char *result = NULL;
    char line[4096];
    size_t len = 0;
    while (fgets(line, sizeof(line), fp)) {
        size_t n = strlen(line);
        result = g_realloc(result, len + n + 1);
        memcpy(result + len, line, n + 1);
        len += n;
    }
    pclose(fp);
    if (!result) result = g_strdup("");
    if (len > 0 && result[len-1] == '\n') result[len-1] = '\0';
    return result;
}

/* ================================================================
 * rc.xml parser — get workspace count
 * ================================================================ */

static int parse_workspace_count(void) {
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", home, RC_FILE);

    char *content = run_cmd("grep -o '<number>[0-9]*</number>' '%s' 2>/dev/null | head -1 | grep -o '[0-9]*'", path);
    int count = atoi(content);
    g_free(content);
    return count > 0 ? count : 9;
}

/* ================================================================
 * Wayland protocol — wlr-foreign-toplevel-management
 * ================================================================ */

static WindowInfo* win_by_handle(struct zwlr_foreign_toplevel_handle_v1 *h) {
    for (GList *l = g_wl_wins; l; l = l->next) {
        WindowInfo *w = (WindowInfo *)l->data;
        if (w->handle == h) return w;
    }
    return NULL;
}

static void wl_title(void *data, struct zwlr_foreign_toplevel_handle_v1 *h, const char *t) {
    (void)data;
    WindowInfo *w = win_by_handle(h);
    if (!w) return;
    g_free(w->title);
    w->title = g_strdup(t);
}

static void wl_app_id(void *data, struct zwlr_foreign_toplevel_handle_v1 *h, const char *a) {
    (void)data;
    WindowInfo *w = win_by_handle(h);
    if (!w) return;
    g_free(w->app_id);
    w->app_id = g_strdup(a);
}

static void wl_state(void *data, struct zwlr_foreign_toplevel_handle_v1 *h, struct wl_array *s) {
    (void)data;
    WindowInfo *w = win_by_handle(h);
    if (!w) return;
    w->active = FALSE;
    uint32_t *v;
    wl_array_for_each(v, s) {
        if (*v == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED)
            w->active = TRUE;
    }
}

static void wl_done(void *data, struct zwlr_foreign_toplevel_handle_v1 *h) {
    (void)data; (void)h;
}

static void wl_closed(void *data, struct zwlr_foreign_toplevel_handle_v1 *h) {
    (void)data;
    for (GList *l = g_wl_wins; l; l = l->next) {
        WindowInfo *w = (WindowInfo *)l->data;
        if (w->handle == h) {
            zwlr_foreign_toplevel_handle_v1_destroy(h);
            w->handle = NULL;
            break;
        }
    }
}

static void wl_output_enter(void *data, struct zwlr_foreign_toplevel_handle_v1 *h, struct wl_output *o) {
    (void)data; (void)h; (void)o;
}
static void wl_output_leave(void *data, struct zwlr_foreign_toplevel_handle_v1 *h, struct wl_output *o) {
    (void)data; (void)h; (void)o;
}
static void wl_parent(void *data, struct zwlr_foreign_toplevel_handle_v1 *h, struct zwlr_foreign_toplevel_handle_v1 *p) {
    (void)data; (void)h; (void)p;
}

static const struct zwlr_foreign_toplevel_handle_v1_listener wl_handle_listener = {
    .title        = wl_title,
    .app_id       = wl_app_id,
    .output_enter = wl_output_enter,
    .output_leave = wl_output_leave,
    .state        = wl_state,
    .done         = wl_done,
    .closed       = wl_closed,
    .parent       = wl_parent,
};

static void wl_toplevel(void *data, struct zwlr_foreign_toplevel_manager_v1 *mgr, struct zwlr_foreign_toplevel_handle_v1 *h) {
    (void)data; (void)mgr;
    WindowInfo *w = g_new0(WindowInfo, 1);
    w->handle = h;
    g_wl_wins = g_list_append(g_wl_wins, w);
    zwlr_foreign_toplevel_handle_v1_add_listener(h, &wl_handle_listener, NULL);
}

static void wl_finished(void *data, struct zwlr_foreign_toplevel_manager_v1 *mgr) {
    (void)data; (void)mgr;
}

static const struct zwlr_foreign_toplevel_manager_v1_listener wl_mgr_listener = {
    .toplevel = wl_toplevel,
    .finished = wl_finished,
};

static void wl_registry_global(void *data, struct wl_registry *reg, uint32_t name, const char *iface, uint32_t ver) {
    (void)data;
    if (strcmp(iface, "zwlr_foreign_toplevel_manager_v1") == 0) {
        g_wl_mgr = wl_registry_bind(reg, name, &zwlr_foreign_toplevel_manager_v1_interface, ver < 3 ? ver : 3);
        zwlr_foreign_toplevel_manager_v1_add_listener(g_wl_mgr, &wl_mgr_listener, NULL);
    } else if (strcmp(iface, "wl_seat") == 0 && !g_wl_seat) {
        g_wl_seat = wl_registry_bind(reg, name, &wl_seat_interface, ver < 1 ? ver : 1);
    }
}

static void wl_registry_global_remove(void *data, struct wl_registry *reg, uint32_t name) {
    (void)data; (void)reg; (void)name;
}

static const struct wl_registry_listener wl_reg_listener = {
    .global        = wl_registry_global,
    .global_remove = wl_registry_global_remove,
};

static void init_wayland(GdkDisplay *gdk_dpy) {
    if (!GDK_IS_WAYLAND_DISPLAY(gdk_dpy)) return;
    g_wl_dpy = gdk_wayland_display_get_wl_display(gdk_dpy);
    if (!g_wl_dpy) return;
    struct wl_registry *reg = wl_display_get_registry(g_wl_dpy);
    wl_registry_add_listener(reg, &wl_reg_listener, NULL);
    wl_display_roundtrip(g_wl_dpy);
}

/* ================================================================
 * Refresh workspace windows from Wayland state
 * ================================================================ */

static void refresh_windows(void) {
    for (GList *l = g_workspaces; l; l = l->next) {
        WorkspaceData *ws = (WorkspaceData *)l->data;
        for (GList *wl = ws->windows; wl; wl = wl->next) g_free(wl->data);
        g_list_free(ws->windows);
        ws->windows = NULL;
    }

    /* Clean closed windows from g_wl_wins */
    GList *to_free = NULL;
    for (GList *l = g_wl_wins; l; ) {
        WindowInfo *w = (WindowInfo *)l->data;
        GList *next = l->next;
        if (!w->handle) {
            g_free(w->title);
            g_free(w->app_id);
            g_free(w);
            g_wl_wins = g_list_delete_link(g_wl_wins, l);
        }
        l = next;
    }

    /* Copy active windows into workspace 0 (all on current desktop) */
    for (GList *l = g_wl_wins; l; l = l->next) {
        WindowInfo *w = (WindowInfo *)l->data;
        if (!w->handle) continue;

        /* Skip panels/docks */
        if (w->app_id && (strstr(w->app_id, "sfwbar") || strstr(w->app_id, "labwc")))
            continue;

        WorkspaceData *ws = (WorkspaceData *)g_workspaces->data; /* workspace 0 */
        WindowInfo *copy = g_new0(WindowInfo, 1);
        memcpy(copy, w, sizeof(WindowInfo));
        copy->title = g_strdup(w->title);
        copy->app_id = g_strdup(w->app_id);
        ws->windows = g_list_append(ws->windows, copy);
    }
}

/* ================================================================
 * Presets parser
 * ================================================================ */

static void load_presets(void) {
    if (g_presets) {
        for (int i = 0; i < g_npresets; i++)
            g_free(g_presets[i].name);
        g_free(g_presets);
        g_presets = NULL;
        g_npresets = 0;
    }

    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", home, PRESETS_FILE);

    GKeyFile *kf = g_key_file_new();
    if (!g_key_file_load_from_file(kf, path, G_KEY_FILE_NONE, NULL)) {
        g_key_file_free(kf);
        return;
    }

    gsize n = 0;
    gchar **keys = g_key_file_get_keys(kf, "apps", &n, NULL);
    if (!keys) { g_key_file_free(kf); return; }

    g_presets = g_new0(PresetApp, n);
    for (gsize i = 0; i < n; i++) {
        char *val = g_key_file_get_value(kf, "apps", keys[i], NULL);
        if (val) {
            g_strlcpy(g_presets[g_npresets].name, keys[i], sizeof(g_presets[g_npresets].name));
            g_strlcpy(g_presets[g_npresets].command, val, sizeof(g_presets[g_npresets].command));
            g_free(val);
            g_npresets++;
        }
    }
    g_strfreev(keys);
    g_key_file_free(kf);
}

/* ================================================================
 * Actions
 * ================================================================ */

static void on_focus_window(GtkWidget *btn, gpointer data) {
    (void)btn;
    struct zwlr_foreign_toplevel_handle_v1 *h = data;
    if (h && g_wl_seat) zwlr_foreign_toplevel_handle_v1_activate(h, g_wl_seat);
}

static void on_minimize_window(GtkWidget *btn, gpointer data) {
    (void)btn;
    struct zwlr_foreign_toplevel_handle_v1 *h = data;
    if (h) zwlr_foreign_toplevel_handle_v1_set_minimized(h);
}

static void on_close_window(GtkWidget *btn, gpointer data) {
    (void)btn;
    struct zwlr_foreign_toplevel_handle_v1 *h = data;
    if (h) zwlr_foreign_toplevel_handle_v1_close(h);
}

static void on_launch_app(GtkWidget *btn, gpointer data) {
    (void)btn;
    const char *cmd = (const char *)data;
    run_cmd("%s &", cmd);
}

typedef struct {
    struct zwlr_foreign_toplevel_handle_v1 *handle;
    int desktop;
} MoveArgs;

static void free_ptr(gpointer data, GClosure *closure) {
    (void)closure;
    g_free(data);
}

static void free_move_args(gpointer data, GClosure *closure) {
    (void)closure;
    g_free(data);
}

/* Phase 2: send wtype shortcut to move the (now focused) window */
static gboolean do_send_move(gpointer data) {
    MoveArgs *a = (MoveArgs *)data;
    char cmd[64];
    /* LabWC: Shift+Super+<n> = SendToDesktop <n> */
    snprintf(cmd, sizeof(cmd), "wtype -M super -M shift %d -m shift -m super", a->desktop + 1);
    run_cmd("%s", cmd);
    /* ponytail: wtype sends to compositor, so focused window moves;
       no way to restore focus to our app without our own handle */
    g_free(a);
    return G_SOURCE_REMOVE;
}

/* Move window to another workspace via keyboard shortcut simulation */
static void on_move_window(GtkWidget *btn, gpointer data) {
    (void)btn;
    MoveArgs *a = (MoveArgs *)data;
    if (!a->handle || !g_wl_seat) return;

    /* Phase 1: focus the target window */
    zwlr_foreign_toplevel_handle_v1_activate(a->handle, g_wl_seat);

    /* Phase 2: after compositor processes focus, send key shortcut */
    MoveArgs *copy = g_new(MoveArgs, 1);
    copy->handle  = a->handle;
    copy->desktop = a->desktop;
    g_timeout_add(80, do_send_move, copy);
}

static GtkWidget* make_window_card(WindowInfo *win, WorkspaceData *parent_ws) {
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    GtkStyleContext *ctx = gtk_widget_get_style_context(card);
    gtk_style_context_add_class(ctx, "win-card");

    gtk_widget_set_margin_top(card, 4);
    gtk_widget_set_margin_bottom(card, 4);
    gtk_widget_set_margin_start(card, 6);
    gtk_widget_set_margin_end(card, 6);

    /* App ID header */
    const char *label = win->app_id && win->app_id[0] ? win->app_id : "Window";
    char header_markup[512];
    snprintf(header_markup, sizeof(header_markup),
             "<span weight='bold' size='small'>%s</span>", label);
    GtkWidget *hdr = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(hdr), header_markup);
    gtk_label_set_xalign(GTK_LABEL(hdr), 0.0);
    gtk_widget_set_halign(hdr, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(card), hdr, FALSE, FALSE, 0);

    /* Window title */
    GtkWidget *title_lbl = gtk_label_new(win->title && win->title[0] ? win->title : "(untitled)");
    gtk_label_set_xalign(GTK_LABEL(title_lbl), 0.0);
    gtk_label_set_ellipsize(GTK_LABEL(title_lbl), PANGO_ELLIPSIZE_END);
    gtk_style_context_add_class(gtk_widget_get_style_context(title_lbl), "dim-label");
    gtk_box_pack_start(GTK_BOX(card), title_lbl, FALSE, FALSE, 0);

    /* Action buttons row */
    GtkWidget *actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_widget_set_margin_top(actions, 4);

    /* Focus - minimize - close */
    GtkWidget *btn;
    btn = gtk_button_new_from_icon_name("go-jump-symbolic", GTK_ICON_SIZE_MENU);
    gtk_widget_set_tooltip_text(btn, "Focus window");
    g_signal_connect(btn, "clicked", G_CALLBACK(on_focus_window), win->handle);
    gtk_box_pack_start(GTK_BOX(actions), btn, FALSE, FALSE, 0);

    btn = gtk_button_new_from_icon_name("window-minimize-symbolic", GTK_ICON_SIZE_MENU);
    gtk_widget_set_tooltip_text(btn, "Minimize");
    g_signal_connect(btn, "clicked", G_CALLBACK(on_minimize_window), win->handle);
    gtk_box_pack_start(GTK_BOX(actions), btn, FALSE, FALSE, 0);

    btn = gtk_button_new_from_icon_name("window-close-symbolic", GTK_ICON_SIZE_MENU);
    gtk_widget_set_tooltip_text(btn, "Close window");
    g_signal_connect(btn, "clicked", G_CALLBACK(on_close_window), win->handle);
    gtk_box_pack_start(GTK_BOX(actions), btn, FALSE, FALSE, 0);

    /* Move-to-workspace (xdotool, XWayland only) */
    GtkWidget *move_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    GtkWidget *move_lbl = gtk_label_new("→");
    gtk_style_context_add_class(gtk_widget_get_style_context(move_lbl), "dim-label");
    gtk_box_pack_start(GTK_BOX(move_box), move_lbl, FALSE, FALSE, 0);

    for (int i = 0; i < g_ws_count; i++) {
        if (i == parent_ws->number) continue;
        char ws_name[16];
        snprintf(ws_name, sizeof(ws_name), "WS%d", i + 1);
        GtkWidget *ws_btn = gtk_button_new_with_label(ws_name);
        gtk_widget_set_size_request(ws_btn, 36, -1);
        MoveArgs *a = g_new(MoveArgs, 1);
        a->handle  = win->handle;
        a->desktop = i;
        g_object_set_data_full(G_OBJECT(ws_btn), "move-args", a, g_free);
        g_signal_connect(ws_btn, "clicked", G_CALLBACK(on_move_window), a);
        gtk_box_pack_start(GTK_BOX(move_box), ws_btn, FALSE, FALSE, 1);
    }
    gtk_box_pack_start(GTK_BOX(actions), move_box, FALSE, FALSE, 4);

    gtk_box_pack_start(GTK_BOX(card), actions, FALSE, FALSE, 0);

    return card;
}

/* ================================================================
 * UI: Workspace column
 * ================================================================ */

static GtkWidget* make_workspace_column(WorkspaceData *ws) {
    GtkWidget *frame = gtk_frame_new(NULL);
    GtkStyleContext *fctx = gtk_widget_get_style_context(frame);
    gtk_style_context_add_class(fctx, "ws-column");
    gtk_widget_set_size_request(frame, 240, -1);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_container_add(GTK_CONTAINER(frame), vbox);

    /* Header */
    char header_markup[128];
    snprintf(header_markup, sizeof(header_markup),
             "<span weight='bold' size='large'>Workspace %d</span>", ws->number + 1);
    GtkWidget *header = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(header), header_markup);
    gtk_widget_set_halign(header, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(header, 8);
    gtk_widget_set_margin_bottom(header, 4);
    gtk_box_pack_start(GTK_BOX(vbox), header, FALSE, FALSE, 0);

    /* Launch buttons row — predefined apps for this workspace */
    if (g_npresets > 0) {
        GtkWidget *preset_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
        gtk_widget_set_halign(preset_box, GTK_ALIGN_CENTER);
        gtk_widget_set_margin_bottom(preset_box, 4);
        for (int i = 0; i < g_npresets; i++) {
            GtkWidget *pbtn = gtk_button_new_with_label(g_presets[i].name);
            gtk_widget_set_tooltip_text(pbtn, g_presets[i].command);
            gtk_widget_set_size_request(pbtn, -1, 28);
            g_signal_connect_data(pbtn, "clicked", G_CALLBACK(on_launch_app),
                             g_strdup(g_presets[i].command), (GClosureNotify)free_ptr, 0);
            gtk_box_pack_start(GTK_BOX(preset_box), pbtn, FALSE, FALSE, 2);
        }
        gtk_box_pack_start(GTK_BOX(vbox), preset_box, FALSE, FALSE, 0);
    }

    /* Separator */
    gtk_box_pack_start(GTK_BOX(vbox),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 2);

    /* Window list (scrolled) */
    GtkWidget *list_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(list_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    GtkWidget *list_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(list_scroll), list_box);
    gtk_box_pack_start(GTK_BOX(vbox), list_scroll, TRUE, TRUE, 0);

    /* Populate windows */
    for (GList *wl = ws->windows; wl; wl = wl->next) {
        WindowInfo *win = (WindowInfo *)wl->data;
        GtkWidget *card = make_window_card(win, ws);
        gtk_box_pack_start(GTK_BOX(list_box), card, FALSE, FALSE, 0);
    }

    /* Empty state */
    if (!ws->windows) {
        GtkWidget *empty = gtk_label_new("No windows");
        gtk_style_context_add_class(gtk_widget_get_style_context(empty), "dim-label");
        gtk_widget_set_margin_top(empty, 20);
        gtk_box_pack_start(GTK_BOX(list_box), empty, FALSE, FALSE, 0);
    }

    return frame;
}

/* ================================================================
 * UI: Rebuild all columns
 * ================================================================ */

static void rebuild_columns(void) {
    /* Destroy existing columns if this is a refresh */
    if (g_scroll) {
        gtk_widget_destroy(g_scroll);
        g_scroll = NULL;
    }

    g_ws_count = parse_workspace_count();

    /* Ensure workspace list is sized correctly */
    int len = g_list_length(g_workspaces);
    while (len > g_ws_count) {
        GList *last = g_list_last(g_workspaces);
        WorkspaceData *ws = (WorkspaceData *)last->data;
        for (GList *wl = ws->windows; wl; wl = wl->next) g_free(wl->data);
        g_list_free(ws->windows);
        g_free(ws);
        g_workspaces = g_list_delete_link(g_workspaces, last);
        len--;
    }
    while (len < g_ws_count) {
        WorkspaceData *ws = g_new0(WorkspaceData, 1);
        ws->number = len;
        snprintf(ws->name, sizeof(ws->name), "Workspace %d", len + 1);
        ws->windows = NULL;
        g_workspaces = g_list_append(g_workspaces, ws);
        len++;
    }

    /* Load presets */
    load_presets();

    /* Refresh window list */
    refresh_windows();

    /* Build horizontal scrollable area */
    g_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(g_scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(hbox, 12);
    gtk_widget_set_margin_end(hbox, 12);
    gtk_widget_set_margin_top(hbox, 8);
    gtk_widget_set_margin_bottom(hbox, 8);
    gtk_container_add(GTK_CONTAINER(g_scroll), hbox);

    for (GList *l = g_workspaces; l; l = l->next) {
        WorkspaceData *ws = (WorkspaceData *)l->data;
        GtkWidget *col = make_workspace_column(ws);
        ws->column = col;
        gtk_box_pack_start(GTK_BOX(hbox), col, FALSE, FALSE, 0);
    }

    gtk_box_pack_start(GTK_BOX(g_main_box), g_scroll, TRUE, TRUE, 0);
    gtk_widget_show_all(g_main_box);
}

/* ================================================================
 * App activation
 * ================================================================ */

static gboolean delayed_refresh(gpointer user) {
    (void)user;
    rebuild_columns();
    return G_SOURCE_REMOVE;
}

static void activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;

    GtkWidget *window = gtk_application_window_new(app);
    ocws_gtk_enforce_premium_theme();
    ocws_gtk_apply_dynamic_css(app, NULL);

    gtk_window_set_title(GTK_WINDOW(window), "OCWS Workspace Manager");
    gtk_window_set_default_size(GTK_WINDOW(window), 1100, 600);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

    /* Header bar */
    GtkWidget *header = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), TRUE);
    gtk_header_bar_set_title(GTK_HEADER_BAR(header), "Workspace Manager");
    gtk_header_bar_set_subtitle(GTK_HEADER_BAR(header), "Kanban Board");
    gtk_window_set_titlebar(GTK_WINDOW(window), header);

    /* Refresh button in header */
    g_refresh_btn = gtk_button_new_from_icon_name("view-refresh-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(g_refresh_btn, "Refresh window list");
    g_signal_connect(g_refresh_btn, "clicked", G_CALLBACK(rebuild_columns), NULL);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), g_refresh_btn);

    /* Main layout */
    g_main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), g_main_box);

    /* Init Wayland after the display is guaranteed ready */
    init_wayland(gtk_widget_get_display(window));

    /* First build (synchronous, may get empty results) */
    rebuild_columns();

    /* Refresh again once events settle — the Wayland toplevel list
       might arrive right after the synchronous roundtrip */
    g_idle_add(delayed_refresh, NULL);

    gtk_widget_show_all(window);
}

/* ================================================================
 * Entry point
 * ================================================================ */

static GLogWriterOutput filter_log(GLogLevelFlags level, const GLogField *fields, gsize n, gpointer user) {
    (void)user;
    for (gsize i = 0; i < n; i++) {
        if (strcmp(fields[i].key, "MESSAGE") == 0 &&
            fields[i].value && strstr((const char *)fields[i].value, "Unknown key"))
            return G_LOG_WRITER_HANDLED;
    }
    return g_log_writer_default(level, fields, n, user);
}

int main(int argc, char **argv) {
    g_log_set_writer_func(filter_log, NULL, NULL);
    GtkApplication *app = gtk_application_new(APP_ID, G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
