/*
 * ocws-welcome.c — OCWS Welcome / First-Run Setup Wizard
 *
 * A GTK3 multi-page welcome popup shown on every startup unless the
 * user checks "Do not show this again".  Guides the user through:
 *   1. What is OCWS?
 *   2. Shell mode selection
 *   3. Theme picker
 *   4. Quick toggles (wallpaper, notifications, …)
 *   5. Ready / finish page
 *
 * Persistence flag: ~/.config/ocws/welcome-disabled
 *
 * Build:
 *   gcc -O2 -o ocws-welcome src/ocws-welcome.c \
 *       $(pkg-config --cflags --libs gtk+-3.0 glib-2.0)
 */

#include <gtk/gtk.h>
#include <glib.h>
#include <stdlib.h>
#include "../core/utils.h"
#include "../libocws/gtk.h"
#include "../libocws/string.h"
#include <unistd.h>
#include <sys/stat.h>
#include "utils.h"

static void free_ptr(gpointer data, GClosure *closure);

/* ================================================================
 * Constants
 * ================================================================ */

#define DISABLE_FILE     "welcome-disabled"
#define THEMES_SYSTEM    "/usr/share/ocws/themes"
#define APP_ID           "org.ocws.welcome"

/* ================================================================
 * Globals
 * ================================================================ */

static GtkWidget *g_stack      = NULL;
static GtkWidget *g_btn_prev   = NULL;
static GtkWidget *g_btn_next   = NULL;
static GtkWidget *g_checkbox   = NULL;
static int        g_page       = 0;
static const int  TOTAL_PAGES  = 7;

static const char *PAGE_NAMES[] = {
    "intro", "shell", "theme", "options", "tools", "thanks", "finish"
};

/* ================================================================
 * Path helpers
 * ================================================================ */

static void get_disable_path(char *buf, size_t len) {
    char dir[512];
    get_config_dir(dir, sizeof(dir));
    snprintf(buf, len, "%s/%s", dir, DISABLE_FILE);
}

static gboolean is_welcome_disabled(void) {
    char path[512];
    get_disable_path(path, sizeof(path));
    return access(path, F_OK) == 0;
}

/* ================================================================
 * Callbacks
 * ================================================================ */

static void on_dont_show_toggled(GtkToggleButton *btn, gpointer data) {
    (void)data;
    char dir[512], path[512];
    get_config_dir(dir, sizeof(dir));
    mkdir(dir, 0755);
    get_disable_path(path, sizeof(path));

    if (gtk_toggle_button_get_active(btn)) {
        FILE *f = fopen(path, "w");
        if (f) { fprintf(f, "1\n"); fclose(f); }
    } else {
        remove(path);
    }
}

static void update_nav_buttons(void) {
    gtk_widget_set_sensitive(g_btn_prev, g_page > 0);
    if (g_page >= TOTAL_PAGES - 1) {
        gtk_button_set_label(GTK_BUTTON(g_btn_next), "Finish");
    } else {
        gtk_button_set_label(GTK_BUTTON(g_btn_next), "Next →");
    }
}

static void on_next(GtkWidget *w, gpointer data) {
    (void)w;
    if (g_page >= TOTAL_PAGES - 1) {
        /* Finish — close the window */
        gtk_widget_destroy(gtk_widget_get_toplevel(GTK_WIDGET(data)));
        return;
    }
    g_page++;
    gtk_stack_set_visible_child_name(GTK_STACK(g_stack), PAGE_NAMES[g_page]);
    update_nav_buttons();
}

static void on_prev(GtkWidget *w, gpointer data) {
    (void)w; (void)data;
    if (g_page <= 0) return;
    g_page--;
    gtk_stack_set_visible_child_name(GTK_STACK(g_stack), PAGE_NAMES[g_page]);
    update_nav_buttons();
}

static void on_shell_select(GtkWidget *btn, gpointer data) {
    const char *mode = (const char *)data;
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "toggle-shell %s", mode);
    run_cmd_async(cmd);
    highlight_selected(btn);
}

static void on_theme_select(GtkWidget *btn, gpointer data) {
    const char *theme_name = (const char *)data;

    /* Use theme.sh which handles INI lookup, template expansion, and labwc reload */
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "theme.sh %s", theme_name);
    run_cmd_async(cmd);
    highlight_selected(btn);
}

static void on_open_settings(GtkWidget *w, gpointer data) {
    (void)w; (void)data;
    run_cmd_async("ocws-settings");
}

static void on_randomize_wallpaper(GtkWidget *w, gpointer data) {
    (void)w; (void)data;
    run_cmd_async("wallpaper random");
}

static void on_toggle_changed(GtkSwitch *sw, GParamSpec *pspec, gpointer data) {
    (void)pspec;
    const char *cmd_prefix = (const char *)data;
    gboolean active = gtk_switch_get_active(sw);
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "%s %s &", cmd_prefix, active ? "true" : "false");
    system(cmd);
}

/* ================================================================
 * Page builders
 * ================================================================ */

static GtkWidget *make_page_box(void) {
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(vbox, 30);
    gtk_widget_set_margin_bottom(vbox, 20);
    gtk_widget_set_margin_start(vbox, 40);
    gtk_widget_set_margin_end(vbox, 40);
    gtk_container_add(GTK_CONTAINER(scroll), vbox);
    return scroll;
}

static GtkWidget *get_page_content(GtkWidget *scroll) {
    return gtk_bin_get_child(GTK_BIN(gtk_bin_get_child(GTK_BIN(scroll))));
}

/* ---- Page 1: Intro ---- */
static GtkWidget *build_intro_page(void) {
    GtkWidget *page = make_page_box();
    GtkWidget *vbox = get_page_content(page);

    /* Logo / icon */
    GtkWidget *icon = gtk_image_new_from_icon_name("preferences-desktop", GTK_ICON_SIZE_DIALOG);
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 72);
    gtk_widget_set_halign(icon, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(vbox), icon, FALSE, FALSE, 0);

    /* Title */
    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title),
        "<span size='xx-large' weight='bold'>Welcome to OCWS</span>");
    gtk_widget_set_halign(title, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(vbox), title, FALSE, FALSE, 4);

    /* Subtitle */
    GtkWidget *sub = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(sub),
        "<span size='large' alpha='60%'>Our C-Written Shell — A lightweight Wayland desktop</span>");
    gtk_widget_set_halign(sub, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(vbox), sub, FALSE, FALSE, 0);

    /* Separator */
    gtk_box_pack_start(GTK_BOX(vbox),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 8);

    /* Description */
    GtkWidget *desc = gtk_label_new(
        "OCWS is a modular desktop environment built entirely in C for the "
        "labwc Wayland compositor. It provides:\n\n"
        "  •  Multiple shell modes — double-panel, crystal-dock, DankMaterialShell, noctalia\n"
        "  •  11 curated color themes — from Catppuccin Mocha to Tokyo Night\n"
        "  •  Wallpaper management — randomizer, time-of-day transitions\n"
        "  •  Lightweight C utilities — screenshot, clipboard, volume, brightness & more\n"
        "  •  Unified settings panel — one control center to rule them all\n\n"
        "This wizard will walk you through the essentials to get your desktop looking great."
    );
    gtk_label_set_line_wrap(GTK_LABEL(desc), TRUE);
    gtk_label_set_xalign(GTK_LABEL(desc), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), desc, FALSE, FALSE, 4);

    return page;
}

/* ---- Page 2: Shell Mode ---- */
static GtkWidget *build_shell_page(void) {
    GtkWidget *page = make_page_box();
    GtkWidget *vbox = get_page_content(page);

    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title),
        "<span size='x-large' weight='bold'>Choose Your Shell</span>");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), title, FALSE, FALSE, 0);

    GtkWidget *desc = gtk_label_new(
        "OCWS supports multiple shell modes. Each provides a different desktop experience. "
        "You can switch between them at any time via the right-click menu or toggle-shell command.");
    gtk_label_set_line_wrap(GTK_LABEL(desc), TRUE);
    gtk_label_set_xalign(GTK_LABEL(desc), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), desc, FALSE, FALSE, 4);

    /* Shell option cards in a flow grid */
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 12);
    gtk_widget_set_halign(grid, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(vbox), grid, FALSE, FALSE, 8);

    for (int i = 0; i < OCWS_SHELL_COUNT; i++) {
        GtkWidget *btn = gtk_button_new();
        gtk_widget_set_size_request(btn, 200, 120);
        GtkStyleContext *ctx = gtk_widget_get_style_context(btn);
        gtk_style_context_add_class(ctx, "welcome-card");

        GtkWidget *inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        gtk_widget_set_halign(inner, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(inner, GTK_ALIGN_CENTER);

        GtkWidget *ic = gtk_image_new_from_icon_name(OCWS_SHELLS[i].icon, GTK_ICON_SIZE_LARGE_TOOLBAR);
        gtk_box_pack_start(GTK_BOX(inner), ic, FALSE, FALSE, 0);

        GtkWidget *lbl = gtk_label_new(NULL);
        char *m = g_strdup_printf("<b>%s</b>", OCWS_SHELLS[i].name);
        gtk_label_set_markup(GTK_LABEL(lbl), m);
        g_free(m);
        gtk_box_pack_start(GTK_BOX(inner), lbl, FALSE, FALSE, 0);

        GtkWidget *d = gtk_label_new(OCWS_SHELLS[i].desc);
        gtk_label_set_justify(GTK_LABEL(d), GTK_JUSTIFY_CENTER);
        gtk_label_set_line_wrap(GTK_LABEL(d), TRUE);
        GtkStyleContext *dctx = gtk_widget_get_style_context(d);
        gtk_style_context_add_class(dctx, "dim-label");
        gtk_box_pack_start(GTK_BOX(inner), d, FALSE, FALSE, 0);

        gtk_container_add(GTK_CONTAINER(btn), inner);
        g_signal_connect(btn, "clicked", G_CALLBACK(on_shell_select),
                         (gpointer)OCWS_SHELLS[i].mode);
        gtk_grid_attach(GTK_GRID(grid), btn, i % 2, i / 2, 1, 1);
    }

    return page;
}

/* ---- Page 3: Theme ---- */
static GtkWidget *build_theme_page(void) {
    GtkWidget *page = make_page_box();
    GtkWidget *vbox = get_page_content(page);

    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title),
        "<span size='x-large' weight='bold'>Pick a Theme</span>");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), title, FALSE, FALSE, 0);

    GtkWidget *desc = gtk_label_new(
        "OCWS ships with curated color themes that style labwc, GTK, terminals, "
        "and bar widgets consistently. Choose one below to apply instantly.");
    gtk_label_set_line_wrap(GTK_LABEL(desc), TRUE);
    gtk_label_set_xalign(GTK_LABEL(desc), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), desc, FALSE, FALSE, 4);

    GtkWidget *flow = gtk_flow_box_new();
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flow), 4);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flow), GTK_SELECTION_NONE);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(flow), 10);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(flow), 10);
    gtk_widget_set_halign(flow, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(vbox), flow, FALSE, FALSE, 8);

    for (int i = 0; i < OCWS_THEME_COUNT; i++) {
        GtkWidget *btn = gtk_button_new();
        gtk_widget_set_size_request(btn, 130, 70);
        GtkStyleContext *ctx = gtk_widget_get_style_context(btn);
        gtk_style_context_add_class(ctx, "welcome-card");

        /* Apply individual accent via inline CSS */
        GtkCssProvider *prov = gtk_css_provider_new();
        char css[256];
        snprintf(css, sizeof(css),
                 "button { border-left: 4px solid %s; }", OCWS_THEMES[i].accent);
        gtk_css_provider_load_from_data(prov, css, -1, NULL);
        gtk_style_context_add_provider(ctx, GTK_STYLE_PROVIDER(prov),
                                       GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);
        g_object_unref(prov);

        const char *display_name = OCWS_THEMES[i].name
            ? OCWS_THEMES[i].name : ocws_str_prettify(OCWS_THEMES[i].slug);
        GtkWidget *lbl = gtk_label_new(display_name);
        if (!OCWS_THEMES[i].name) free((char *)display_name);
        gtk_container_add(GTK_CONTAINER(btn), lbl);

        g_signal_connect(btn, "clicked", G_CALLBACK(on_theme_select),
                         (gpointer)OCWS_THEMES[i].slug);
        gtk_container_add(GTK_CONTAINER(flow), btn);
    }

    /* Also scan user themes dir */
    char user_dir[512];
    snprintf(user_dir, sizeof(user_dir), "%s/.local/share/ocws/themes",
             getenv("HOME") ? getenv("HOME") : "/tmp");
    char **extra = NULL;
    int n_extra = scan_themes(user_dir, &extra, 20);
    if (n_extra > 0) {
        GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
        gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 4);

        GtkWidget *lbl2 = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(lbl2), "<b>User Themes</b>");
        gtk_label_set_xalign(GTK_LABEL(lbl2), 0.0);
        gtk_box_pack_start(GTK_BOX(vbox), lbl2, FALSE, FALSE, 0);

        GtkWidget *flow2 = gtk_flow_box_new();
        gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flow2), 4);
        gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flow2), GTK_SELECTION_NONE);
        gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(flow2), 10);
        gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(flow2), 10);
        gtk_box_pack_start(GTK_BOX(vbox), flow2, FALSE, FALSE, 4);

        for (int i = 0; i < n_extra; i++) {
            GtkWidget *btn = gtk_button_new();
            gtk_widget_set_size_request(btn, 130, 50);
            GtkStyleContext *ctx = gtk_widget_get_style_context(btn);
            gtk_style_context_add_class(ctx, "welcome-card");

            char *pretty = ocws_str_prettify(extra[i]);
            GtkWidget *lbl = gtk_label_new(pretty);
            free(pretty);
            gtk_container_add(GTK_CONTAINER(btn), lbl);

            g_signal_connect_data(btn, "clicked", G_CALLBACK(on_theme_select),
                             (gpointer)extra[i], (GClosureNotify)free_ptr, 0);
            gtk_container_add(GTK_CONTAINER(flow2), btn);
        }
    }
    free(extra);

    return page;
}

/* ---- Page 4: Quick Options ---- */
static GtkWidget *build_options_page(void) {
    GtkWidget *page = make_page_box();
    GtkWidget *vbox = get_page_content(page);

    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title),
        "<span size='x-large' weight='bold'>Quick Options</span>");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), title, FALSE, FALSE, 0);

    GtkWidget *desc = gtk_label_new(
        "Quickly configure common dotfiles options. "
        "You can always fine-tune these later in the OCWS Settings panel.");
    gtk_label_set_line_wrap(GTK_LABEL(desc), TRUE);
    gtk_label_set_xalign(GTK_LABEL(desc), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), desc, FALSE, FALSE, 4);

    gtk_box_pack_start(GTK_BOX(vbox),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 4);

    /* Option row helper macro */
    #define OPTION_ROW(parent, label_text, desc_text, btn_text, callback) \
        do { \
            GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10); \
            gtk_widget_set_margin_bottom(row, 8); \
            GtkWidget *info = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2); \
            GtkWidget *_lbl = gtk_label_new(NULL); \
            gtk_label_set_markup(GTK_LABEL(_lbl), "<b>" label_text "</b>"); \
            gtk_label_set_xalign(GTK_LABEL(_lbl), 0.0); \
            gtk_box_pack_start(GTK_BOX(info), _lbl, FALSE, FALSE, 0); \
            GtkWidget *_d = gtk_label_new(desc_text); \
            gtk_label_set_xalign(GTK_LABEL(_d), 0.0); \
            gtk_label_set_line_wrap(GTK_LABEL(_d), TRUE); \
            gtk_style_context_add_class(gtk_widget_get_style_context(_d), "dim-label"); \
            gtk_box_pack_start(GTK_BOX(info), _d, FALSE, FALSE, 0); \
            GtkWidget *_btn = gtk_button_new_with_label(btn_text); \
            gtk_widget_set_valign(_btn, GTK_ALIGN_CENTER); \
            g_signal_connect(_btn, "clicked", G_CALLBACK(callback), NULL); \
            gtk_box_pack_start(GTK_BOX(row), info, TRUE, TRUE, 0); \
            gtk_box_pack_start(GTK_BOX(row), _btn, FALSE, FALSE, 0); \
            gtk_box_pack_start(GTK_BOX(parent), row, FALSE, FALSE, 0); \
        } while(0)

    OPTION_ROW(vbox,
        "Randomize Wallpaper",
        "Pick a random wallpaper from ~/Pictures/wallpapers",
        "Randomize",
        on_randomize_wallpaper);

    OPTION_ROW(vbox,
        "Open Settings Panel",
        "Full control center — appearance, bar, widgets, keybinds & more",
        "Open",
        on_open_settings);

    #undef OPTION_ROW

    /* Toggle rows for common options */
    gtk_box_pack_start(GTK_BOX(vbox),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 4);

    GtkWidget *tog_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(tog_title),
        "<b>Dotfiles Toggles</b>");
    gtk_label_set_xalign(GTK_LABEL(tog_title), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), tog_title, FALSE, FALSE, 4);

    /* Natural scrolling toggle */
    {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_set_margin_bottom(row, 6);
        GtkWidget *info = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        GtkWidget *lbl = gtk_label_new("Natural Scrolling");
        gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);
        gtk_box_pack_start(GTK_BOX(info), lbl, FALSE, FALSE, 0);
        GtkWidget *d = gtk_label_new("Reverse scroll direction (touchpad-style)");
        gtk_label_set_xalign(GTK_LABEL(d), 0.0);
        gtk_style_context_add_class(gtk_widget_get_style_context(d), "dim-label");
        gtk_box_pack_start(GTK_BOX(info), d, FALSE, FALSE, 0);

        GtkWidget *sw = gtk_switch_new();
        gtk_switch_set_active(GTK_SWITCH(sw), TRUE);
        gtk_widget_set_valign(sw, GTK_ALIGN_CENTER);
        g_signal_connect(sw, "notify::active", G_CALLBACK(on_toggle_changed),
                         (gpointer)"gsettings set org.gnome.desktop.peripherals.touchpad natural-scroll");
        gtk_box_pack_start(GTK_BOX(row), info, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(row), sw, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), row, FALSE, FALSE, 0);
    }

    /* Screen protection / night light toggle */
    {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_set_margin_bottom(row, 6);
        GtkWidget *info = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        GtkWidget *lbl = gtk_label_new("Night Light (Gammastep)");
        gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);
        gtk_box_pack_start(GTK_BOX(info), lbl, FALSE, FALSE, 0);
        GtkWidget *d = gtk_label_new("Reduce blue light in the evening");
        gtk_label_set_xalign(GTK_LABEL(d), 0.0);
        gtk_style_context_add_class(gtk_widget_get_style_context(d), "dim-label");
        gtk_box_pack_start(GTK_BOX(info), d, FALSE, FALSE, 0);

        GtkWidget *sw = gtk_switch_new();
        gtk_switch_set_active(GTK_SWITCH(sw), TRUE);
        gtk_widget_set_valign(sw, GTK_ALIGN_CENTER);
        g_signal_connect(sw, "notify::active", G_CALLBACK(on_toggle_changed),
                         (gpointer)"toggle-gammastep");
        gtk_box_pack_start(GTK_BOX(row), info, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(row), sw, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), row, FALSE, FALSE, 0);
    }

    return page;
}

/* ---- Page 5: GUI Tools ---- */
static void on_launch_tool(GtkWidget *w, gpointer data) {
    (void)w;
    const char *cmd = (const char *)data;
    char full_cmd[256];
    snprintf(full_cmd, sizeof(full_cmd), "%s &", cmd);
    system(full_cmd);
}

static GtkWidget *build_tools_page(void) {
    GtkWidget *page = make_page_box();
    GtkWidget *vbox = get_page_content(page);

    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title),
        "<span size='x-large' weight='bold'>GUI Tools</span>");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), title, FALSE, FALSE, 0);

    GtkWidget *desc = gtk_label_new(
        "OCWS includes dedicated GUI tools for managing your desktop. "
        "Launch them anytime from the app launcher or command bar.");
    gtk_label_set_line_wrap(GTK_LABEL(desc), TRUE);
    gtk_label_set_xalign(GTK_LABEL(desc), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), desc, FALSE, FALSE, 4);

    gtk_box_pack_start(GTK_BOX(vbox),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 4);

    /* Tool card helper macro */
    #define TOOL_ROW(parent, icon_name, tool_name, desc_text, cmd) \
        do { \
            GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10); \
            gtk_widget_set_margin_bottom(row, 8); \
            GtkWidget *icon = gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_LARGE_TOOLBAR); \
            gtk_widget_set_size_request(icon, 32, -1); \
            gtk_box_pack_start(GTK_BOX(row), icon, FALSE, FALSE, 0); \
            GtkWidget *info = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2); \
            GtkWidget *_lbl = gtk_label_new(NULL); \
            gtk_label_set_markup(GTK_LABEL(_lbl), "<b>" tool_name "</b>"); \
            gtk_label_set_xalign(GTK_LABEL(_lbl), 0.0); \
            gtk_box_pack_start(GTK_BOX(info), _lbl, FALSE, FALSE, 0); \
            GtkWidget *_d = gtk_label_new(desc_text); \
            gtk_label_set_xalign(GTK_LABEL(_d), 0.0); \
            gtk_label_set_line_wrap(GTK_LABEL(_d), TRUE); \
            gtk_style_context_add_class(gtk_widget_get_style_context(_d), "dim-label"); \
            gtk_box_pack_start(GTK_BOX(info), _d, FALSE, FALSE, 0); \
            GtkWidget *_btn = gtk_button_new_with_label("Launch"); \
            gtk_widget_set_valign(_btn, GTK_ALIGN_CENTER); \
            g_signal_connect(_btn, "clicked", G_CALLBACK(on_launch_tool), (gpointer)cmd); \
            gtk_box_pack_start(GTK_BOX(row), info, TRUE, TRUE, 0); \
            gtk_box_pack_start(GTK_BOX(row), _btn, FALSE, FALSE, 0); \
            gtk_box_pack_start(GTK_BOX(parent), row, FALSE, FALSE, 0); \
        } while(0)

    /* Core Tools */
    GtkWidget *core_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(core_title),
        "<b>Core Applications</b>");
    gtk_label_set_xalign(GTK_LABEL(core_title), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), core_title, FALSE, FALSE, 4);

    TOOL_ROW(vbox,
        "preferences-desktop",
        "OCWS Settings",
        "Full control center — appearance, bar, widgets, keybinds & more",
        "ocws-settings");

    TOOL_ROW(vbox,
        "dialog-information",
        "Welcome Screen",
        "Setup wizard for first-time configuration",
        "ocws-welcome");

    TOOL_ROW(vbox,
        "system-software-install",
        "Package Manager",
        "Resolve dependencies & build engines from source",
        "ocws-pkgmgr");

    TOOL_ROW(vbox,
        "drive-harddisk",
        "System Monitor",
        "CPU, memory, disk, and network statistics",
        "ocws-sysmon");

    gtk_box_pack_start(GTK_BOX(vbox),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 4);

    /* Utility Tools */
    GtkWidget *util_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(util_title),
        "<b>Utilities</b>");
    gtk_label_set_xalign(GTK_LABEL(util_title), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), util_title, FALSE, FALSE, 4);

    TOOL_ROW(vbox,
        "accessories-text-editor",
        "Dock Manager",
        "Manage dock pinned apps across shells",
        "ocws-dock-mgr");

    TOOL_ROW(vbox,
        "preferences-desktop-wallpaper",
        "Workspace Manager",
        "Kanban-style workspace organization",
        "ocws-workspace-mgr");

    TOOL_ROW(vbox,
        "edit-paste",
        "Clipboard Manager",
        "History and search clipboard entries",
        "ocws-clip");

    TOOL_ROW(vbox,
        "color-select-color",
        "Color Picker",
        "Extract colors from screen",
        "ocws-color");

    TOOL_ROW(vbox,
        "camera-photo",
        "OCR Tool",
        "Extract text from screen regions",
        "ocws-ocr");

    TOOL_ROW(vbox,
        "edit-find",
        "Search",
        "Search across files and applications",
        "ocws-search");

    TOOL_ROW(vbox,
        "multimedia-audio-player",
        "Media Player",
        "Control media playback",
        "ocws-player");

    TOOL_ROW(vbox,
        "applets-screenshooter",
        "Screenshot",
        "Capture and annotate screenshots",
        "ocws-shot");

    TOOL_ROW(vbox,
        "system-lock-screen",
        "Lock Screen",
        "Lock your desktop session",
        "ocws-lock");

    #undef TOOL_ROW

    return page;
}

/* ---- Page 6: Thanks / Credits ---- */
static void on_open_url(GtkWidget *w, gpointer data) {
    (void)w;
    const char *url = (const char *)data;
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "xdg-open '%s' &", url);
    system(cmd);
}

static GtkWidget *build_thanks_page(void) {
    GtkWidget *page = make_page_box();
    GtkWidget *vbox = get_page_content(page);

    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title),
        "<span size='x-large' weight='bold'>Thank You</span>");
    gtk_widget_set_halign(title, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(vbox), title, FALSE, FALSE, 4);

    GtkWidget *sub = gtk_label_new(
        "OCWS is built on the shoulders of amazing open-source projects.\n"
        "Please visit and support these upstream repositories.");
    gtk_label_set_justify(GTK_LABEL(sub), GTK_JUSTIFY_CENTER);
    gtk_label_set_line_wrap(GTK_LABEL(sub), TRUE);
    gtk_widget_set_halign(sub, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(vbox), sub, FALSE, FALSE, 4);

    gtk_box_pack_start(GTK_BOX(vbox),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 8);

    struct { const char *name; const char *desc; const char *url; } deps[] = {
        {"labwc",           "Wayland compositor",             "https://github.com/labwc/labwc"},
        {"sfwbar",          "Status bar for Wayland",         "https://github.com/LBCrion/sfwbar"},
        {"fuzzel",          "Application launcher",           "https://codeberg.org/dnkl/fuzzel"},
        {"foot",            "Terminal emulator",              "https://codeberg.org/dnkl/foot"},
        {"mako",            "Notification daemon",            "https://github.com/emersion/mako"},
        {"rofi",            "Window switcher & launcher",     "https://github.com/DaveDavenport/rofi"},
        {"DankMaterialShell", "Material Design 3 shell",     "https://github.com/DankShrine/dms"},
        {"wl-clipboard",    "Clipboard utilities",            "https://github.com/bugaevc/wl-clipboard"},
        {"cliphist",        "Clipboard history",              "https://github.com/sentriz/cliphist"},
        {"swaybg",          "Wallpaper setter",               "https://github.com/swaywm/swaybg"},
        {"swaylock",        "Screen locker",                  "https://github.com/swaywm/swaylock"},
        {"swayidle",        "Idle management",                "https://github.com/swaywm/swayidle"},
        {"grim & slurp",    "Screenshot tools",               "https://github.com/emersion/grim"},
        {"playerctl",       "Media player controller",        "https://github.com/altdesktop/playerctl"},
        {"brightnessctl",   "Brightness control",             "https://github.com/Hummer12007/brightnessctl"},
        {"wlr-randr",       "Output configuration",           "https://gitlab.freedesktop.org/emersion/wlr-randr"},
        {"gammastep",       "Color temperature",              "https://gitlab.com/chinstrap/gammastep"},
    };
    int n_deps = sizeof(deps) / sizeof(deps[0]);

    for (int i = 0; i < n_deps; i++) {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_set_margin_bottom(row, 2);

        GtkWidget *name_lbl = gtk_label_new(NULL);
        char *markup = g_strdup_printf("<b>%s</b>", deps[i].name);
        gtk_label_set_markup(GTK_LABEL(name_lbl), markup);
        g_free(markup);
        gtk_widget_set_size_request(name_lbl, 160, -1);
        gtk_label_set_xalign(GTK_LABEL(name_lbl), 0.0);
        gtk_box_pack_start(GTK_BOX(row), name_lbl, FALSE, FALSE, 0);

        GtkWidget *desc_lbl = gtk_label_new(deps[i].desc);
        gtk_style_context_add_class(gtk_widget_get_style_context(desc_lbl), "dim-label");
        gtk_widget_set_size_request(desc_lbl, 200, -1);
        gtk_label_set_xalign(GTK_LABEL(desc_lbl), 0.0);
        gtk_box_pack_start(GTK_BOX(row), desc_lbl, TRUE, TRUE, 0);

        GtkWidget *link_btn = gtk_button_new_with_label("Visit");
        gtk_widget_set_size_request(link_btn, 60, -1);
        g_signal_connect(link_btn, "clicked", G_CALLBACK(on_open_url),
                         (gpointer)deps[i].url);
        gtk_box_pack_start(GTK_BOX(row), link_btn, FALSE, FALSE, 0);

        gtk_box_pack_start(GTK_BOX(vbox), row, FALSE, FALSE, 0);
    }

    return page;
}

/* ---- Page 6: Finish ---- */
static GtkWidget *build_finish_page(void) {
    GtkWidget *page = make_page_box();
    GtkWidget *vbox = get_page_content(page);

    /* Centered content */
    GtkWidget *icon = gtk_image_new_from_icon_name("emblem-ok-symbolic", GTK_ICON_SIZE_DIALOG);
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 64);
    gtk_widget_set_halign(icon, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(vbox), icon, FALSE, FALSE, 10);

    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title),
        "<span size='x-large' weight='bold'>You're All Set!</span>");
    gtk_widget_set_halign(title, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(vbox), title, FALSE, FALSE, 4);

    GtkWidget *desc = gtk_label_new(
        "Your OCWS desktop is ready to use.\n\n"
        "You can always access the full Settings panel from the\n"
        "right-click menu → Settings, or by running ocws-settings.\n\n"
        "To re-show this welcome wizard, delete the file:\n"
        "~/.config/ocws/welcome-disabled");
    gtk_label_set_justify(GTK_LABEL(desc), GTK_JUSTIFY_CENTER);
    gtk_label_set_line_wrap(GTK_LABEL(desc), TRUE);
    gtk_widget_set_halign(desc, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(vbox), desc, FALSE, FALSE, 10);

    /* Useful quick links */
    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 8);

    GtkWidget *links_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(links_title), "<b>Quick Links</b>");
    gtk_widget_set_halign(links_title, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(vbox), links_title, FALSE, FALSE, 0);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(hbox, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 8);

    GtkWidget *btn_settings = gtk_button_new_with_label("Open Settings");
    g_signal_connect(btn_settings, "clicked", G_CALLBACK(on_open_settings), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), btn_settings, FALSE, FALSE, 0);

    GtkWidget *btn_wall = gtk_button_new_with_label("Randomize Wallpaper");
    g_signal_connect(btn_wall, "clicked", G_CALLBACK(on_randomize_wallpaper), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), btn_wall, FALSE, FALSE, 0);

    return page;
}

/* ================================================================
 * CSS
 * ================================================================ */

static void free_ptr(gpointer data, GClosure *closure) {
    (void)closure;
    g_free(data);
}

/* ================================================================
 * App activation
 * ================================================================ */

static void activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;

    // Premium GTK abstractions handle CSS and themes dynamically
    // No more static CSS loading needed
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Welcome to OCWS");
    gtk_window_set_default_size(GTK_WINDOW(window), 580, 520);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

    /* Header bar */
    GtkWidget *header = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), TRUE);
    gtk_header_bar_set_title(GTK_HEADER_BAR(header), "Welcome to OCWS");
    gtk_header_bar_set_subtitle(GTK_HEADER_BAR(header), "Setup Wizard");
    gtk_window_set_titlebar(GTK_WINDOW(window), header);

    /* Outer box */
    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), outer);

    /* Page stack */
    g_stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(g_stack),
                                  GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    gtk_stack_set_transition_duration(GTK_STACK(g_stack), 250);
    gtk_box_pack_start(GTK_BOX(outer), g_stack, TRUE, TRUE, 0);

    gtk_stack_add_named(GTK_STACK(g_stack), build_intro_page(),   "intro");
    gtk_stack_add_named(GTK_STACK(g_stack), build_shell_page(),   "shell");
    gtk_stack_add_named(GTK_STACK(g_stack), build_theme_page(),   "theme");
    gtk_stack_add_named(GTK_STACK(g_stack), build_options_page(), "options");
    gtk_stack_add_named(GTK_STACK(g_stack), build_tools_page(),   "tools");
    gtk_stack_add_named(GTK_STACK(g_stack), build_thanks_page(),  "thanks");
    gtk_stack_add_named(GTK_STACK(g_stack), build_finish_page(),  "finish");

    /* Bottom bar: checkbox + nav buttons */
    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(outer), sep, FALSE, FALSE, 0);

    GtkWidget *bottom = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_top(bottom, 10);
    gtk_widget_set_margin_bottom(bottom, 10);
    gtk_widget_set_margin_start(bottom, 16);
    gtk_widget_set_margin_end(bottom, 16);
    gtk_box_pack_start(GTK_BOX(outer), bottom, FALSE, FALSE, 0);

    g_checkbox = gtk_check_button_new_with_label("Do not show again");
    if (is_welcome_disabled()) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_checkbox), TRUE);
    }
    g_signal_connect(g_checkbox, "toggled", G_CALLBACK(on_dont_show_toggled), NULL);
    gtk_box_pack_start(GTK_BOX(bottom), g_checkbox, TRUE, TRUE, 0);

    g_btn_prev = gtk_button_new_with_label("← Back");
    g_signal_connect(g_btn_prev, "clicked", G_CALLBACK(on_prev), NULL);
    gtk_box_pack_start(GTK_BOX(bottom), g_btn_prev, FALSE, FALSE, 0);

    g_btn_next = gtk_button_new_with_label("Next →");
    GtkStyleContext *ctx = gtk_widget_get_style_context(g_btn_next);
    gtk_style_context_add_class(ctx, "suggested-action");
    g_signal_connect(g_btn_next, "clicked", G_CALLBACK(on_next), window);
    gtk_box_pack_start(GTK_BOX(bottom), g_btn_next, FALSE, FALSE, 0);

    g_page = 0;
    update_nav_buttons();

    gtk_widget_show_all(window);
}

/* ================================================================
 * Entry point
 * ================================================================ */

int main(int argc, char **argv) {
    /* Honour --force to show even if disabled */
    gboolean force = FALSE;
    int new_argc = 0;
    char **new_argv = g_new0(char *, argc + 1);

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--force") == 0 || strcmp(argv[i], "-f") == 0) {
            force = TRUE;
        } else {
            new_argv[new_argc++] = argv[i];
        }
    }

    if (!force && is_welcome_disabled()) {
        g_free(new_argv);
        return 0;   /* silently exit — user said don't show */
    }

    GtkApplication *app = gtk_application_new(APP_ID, G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), new_argc, new_argv);
    g_object_unref(app);
    g_free(new_argv);
    return status;
}
