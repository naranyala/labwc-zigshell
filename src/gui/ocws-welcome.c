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
#include <sys/wait.h>
#include <stdarg.h>
#include <time.h>

static void free_ptr(gpointer data, GClosure *closure);

static void log_msg(const char *fmt, ...) {
    char path[512];
    snprintf(path, sizeof(path), "%s/.cache/ocws-welcome.log", getenv("HOME") ? getenv("HOME") : "/tmp");
    FILE *f = fopen(path, "a");
    if (!f) return;
    
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char tbuf[64];
    strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", t);
    
    fprintf(f, "[%s] ", tbuf);
    
    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);
    
    fprintf(f, "\n");
    fclose(f);
}

static int run_cmd_logged(const char *cmd) {
    log_msg("EXEC: %s", cmd);
    int rc = system(cmd);
    if (rc != 0) {
        log_msg("ERROR: command failed with code %d: %s", WEXITSTATUS(rc), cmd);
    } else {
        log_msg("SUCCESS: %s", cmd);
    }
    return rc;
}

static void send_notification(const char *title, const char *body, const char *icon) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "notify-send -i '%s' '%s' '%s' &", icon ? icon : "info", title, body);
    run_cmd_logged(cmd);
}

/* Shell-safe string: rejects shell metacharacters */
static int is_shell_safe(const char *s) {
    if (!s || !*s) return 0;
    for (const char *p = s; *p; p++) {
        char c = *p;
        if (c == ';' || c == '|' || c == '&' || c == '$' ||
            c == '(' || c == ')' || c == '{' || c == '}' ||
            c == '`' || c == '"' || c == '\'' || c == '\\' ||
            c == '\n' || c == '\r' || c == '<' || c == '>')
            return 0;
    }
    return 1;
}

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
static GtkWidget *g_shell_status = NULL;
static int        g_page       = 0;
static const int  TOTAL_PAGES  = 10;

static const char *PAGE_NAMES[] = {
    "intro", "health", "monitors", "mount", "shell", "theme", "options", "tools", "thanks", "finish"
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
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "toggle-shell %s 2>/dev/null || ~/.local/bin/toggle-shell %s", mode, mode);

    /* Run toggle-shell to completion (it kills the old shell and starts
     * the new one) BEFORE updating the UI, so the selection state always
     * reflects what is actually running. */
    int rc = run_cmd_logged(cmd);

    if (rc == 0) {
        send_notification("Shell Mode", "Successfully switched shell mode", "preferences-desktop");
        highlight_selected(btn);
        if (g_shell_status) {
            char *msg = g_strdup_printf("Active shell: %s", mode);
            gtk_label_set_text(GTK_LABEL(g_shell_status), msg);
            g_free(msg);
        }
    } else if (g_shell_status) {
        char *msg = g_strdup_printf("Could not switch to %s — engine not installed?",
                                    mode);
        gtk_label_set_text(GTK_LABEL(g_shell_status), msg);
        g_free(msg);
    }
}

static void on_theme_select(GtkWidget *btn, gpointer data) {
    const char *theme_name = (const char *)data;

    /* Validate theme name — reject shell metacharacters */
    if (!theme_name || !is_shell_safe(theme_name)) {
        g_warning("rejected unsafe theme name");
        return;
    }

    /* Use theme.sh which handles INI lookup, template expansion, and labwc reload */
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "theme.sh %s", theme_name);
    run_cmd_async(cmd);
    send_notification("Theme Applied", theme_name, "color-select-color");
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

static void on_test_notification(GtkWidget *w, gpointer data) {
    (void)w; (void)data;
    send_notification("OCWS Ready", "Notifications are working perfectly!", "emblem-ok-symbolic");
}

static void on_toggle_changed(GtkSwitch *sw, GParamSpec *pspec, gpointer data) {
    (void)pspec;
    const char *cmd_prefix = (const char *)data;
    gboolean active = gtk_switch_get_active(sw);
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "%s %s &", cmd_prefix, active ? "true" : "false");
    run_cmd_logged(cmd);
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

/* ---- Page: System Health ---- */
static void on_install_missing(GtkWidget *btn, gpointer data) {
    (void)data;
    send_notification("Package Manager", "Opening package manager...", "system-software-install");
    run_cmd_logged("ocws-pkgmgr &");
    gtk_button_set_label(GTK_BUTTON(btn), "Opened Package Manager");
}

static GtkWidget *build_health_page(void) {
    GtkWidget *page = make_page_box();
    GtkWidget *vbox = get_page_content(page);

    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title),
        "<span size='x-large' weight='bold'>System Health</span>");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), title, FALSE, FALSE, 0);

    GtkWidget *desc = gtk_label_new(
        "Check if recommended dependencies are installed.");
    gtk_label_set_line_wrap(GTK_LABEL(desc), TRUE);
    gtk_label_set_xalign(GTK_LABEL(desc), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), desc, FALSE, FALSE, 4);

    gtk_box_pack_start(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 8);

    const char *deps[] = {
        "grim", "slurp", "wl-clipboard", "fuzzel", "swayosd-server", "playerctl"
    };
    int missing = 0;

    for (size_t i = 0; i < sizeof(deps)/sizeof(deps[0]); i++) {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_set_margin_bottom(row, 4);
        
        char *path = g_find_program_in_path(deps[i]);
        gboolean found = (path != NULL);
        if (path) g_free(path);
        if (!found) missing++;

        GtkWidget *icon = gtk_image_new_from_icon_name(
            found ? "emblem-default" : "dialog-error", GTK_ICON_SIZE_BUTTON);
        gtk_box_pack_start(GTK_BOX(row), icon, FALSE, FALSE, 0);

        GtkWidget *lbl = gtk_label_new(deps[i]);
        gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);
        gtk_box_pack_start(GTK_BOX(row), lbl, TRUE, TRUE, 0);
        
        gtk_box_pack_start(GTK_BOX(vbox), row, FALSE, FALSE, 0);
    }

    if (missing > 0) {
        GtkWidget *btn = gtk_button_new_with_label("Install Missing");
        g_signal_connect(btn, "clicked", G_CALLBACK(on_install_missing), NULL);
        gtk_box_pack_start(GTK_BOX(vbox), btn, FALSE, FALSE, 10);
    }

    return page;
}

/* ---- Page: Monitors ---- */
static void on_open_wlr_randr(GtkWidget *btn, gpointer data) {
    (void)data;
    run_cmd_logged("foot -e sh -c 'wlr-randr; echo; echo Press Enter to close...; read' &");
}

static void on_apply_hidpi(GtkWidget *btn, gpointer data) {
    (void)data;
    run_cmd_logged("wlr-randr --output eDP-1 --scale 1.5 &");
    send_notification("Display Settings", "Applied 1.5x HiDPI Scaling", "video-display");
    gtk_button_set_label(GTK_BUTTON(btn), "Applied");
}

static GtkWidget *build_monitors_page(void) {
    GtkWidget *page = make_page_box();
    GtkWidget *vbox = get_page_content(page);

    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title),
        "<span size='x-large' weight='bold'>Monitors & Scaling</span>");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), title, FALSE, FALSE, 0);

    GtkWidget *desc = gtk_label_new(
        "Configure display scaling and monitor layout.");
    gtk_label_set_line_wrap(GTK_LABEL(desc), TRUE);
    gtk_label_set_xalign(GTK_LABEL(desc), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), desc, FALSE, FALSE, 4);

    gtk_box_pack_start(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 8);

    /* Toggle for HiDPI */
    GtkWidget *row1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *lbl1 = gtk_label_new("HiDPI Scaling (1.5x)");
    gtk_label_set_xalign(GTK_LABEL(lbl1), 0.0);
    gtk_box_pack_start(GTK_BOX(row1), lbl1, TRUE, TRUE, 0);
    
    GtkWidget *btn1 = gtk_button_new_with_label("Apply 1.5x Scale");
    g_signal_connect(btn1, "clicked", G_CALLBACK(on_apply_hidpi), NULL);
    gtk_box_pack_start(GTK_BOX(row1), btn1, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), row1, FALSE, FALSE, 8);

    /* Advanced Config */
    GtkWidget *row2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *lbl2 = gtk_label_new("List Displays (wlr-randr)");
    gtk_label_set_xalign(GTK_LABEL(lbl2), 0.0);
    gtk_box_pack_start(GTK_BOX(row2), lbl2, TRUE, TRUE, 0);
    
    GtkWidget *btn2 = gtk_button_new_with_label("Check Displays");
    g_signal_connect(btn2, "clicked", G_CALLBACK(on_open_wlr_randr), NULL);
    gtk_box_pack_start(GTK_BOX(row2), btn2, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), row2, FALSE, FALSE, 8);

    return page;
}

/* ---- Page X: Mount Partitions ---- */
static void on_mount_partition(GtkWidget *btn, gpointer data) {
    const char *part = (const char *)data;
    
    char label[128] = {0};
    char uuid[128] = {0};
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "lsblk -P -o LABEL,UUID %s", part);
    FILE *f = popen(cmd, "r");
    if (f) {
        char line[512];
        if (fgets(line, sizeof(line), f)) {
            char *l = strstr(line, "LABEL=\"");
            if (l) {
                l += 7;
                char *end = strchr(l, '"');
                if (end && end - l < sizeof(label) - 1) strncpy(label, l, end - l);
            }
            char *u = strstr(line, "UUID=\"");
            if (u) {
                u += 6;
                char *end = strchr(u, '"');
                if (end && end - u < sizeof(uuid) - 1) strncpy(uuid, u, end - u);
            }
        }
        pclose(f);
    }
    
    const char *disk_name = (label[0] != '\0') ? label : uuid;
    if (disk_name[0] == '\0') disk_name = "unknown";
    
    char mount_base[256];
    const char *user = getenv("USER");
    if (!user) user = "naranyala";
    snprintf(mount_base, sizeof(mount_base), "/media/%s", user);
    
    char mount_point[512];
    snprintf(mount_point, sizeof(mount_point), "%s/%s", mount_base, disk_name);
    
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s' && mount '%s' '%s'", mount_point, part, mount_point);
    int rc = run_cmd_logged(cmd);
    if (rc != 0) {
        /* Fallback with pkexec if unprivileged mount fails */
        snprintf(cmd, sizeof(cmd), "mkdir -p '%s' && pkexec mount '%s' '%s'", mount_point, part, mount_point);
        rc = run_cmd_logged(cmd);
    }

    if (rc == 0) {
        send_notification("Partition Mounted", disk_name, "drive-harddisk");
        gtk_button_set_label(GTK_BUTTON(btn), "Mounted");
        gtk_widget_set_sensitive(btn, FALSE);
    } else {
        send_notification("Mount Failed", "Could not mount partition", "dialog-error");
        gtk_button_set_label(GTK_BUTTON(btn), "Failed");
    }
}

static GtkWidget *build_mount_page(void) {
    GtkWidget *page = make_page_box();
    GtkWidget *vbox = get_page_content(page);

    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title),
        "<span size='x-large' weight='bold'>Mount Partitions</span>");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), title, FALSE, FALSE, 0);

    GtkWidget *desc = gtk_label_new(
        "Scan and mount available partitions to your media directory.");
    gtk_label_set_line_wrap(GTK_LABEL(desc), TRUE);
    gtk_label_set_xalign(GTK_LABEL(desc), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), desc, FALSE, FALSE, 4);

    gtk_box_pack_start(GTK_BOX(vbox),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 8);

    FILE *f = popen("lsblk -P -p -o NAME,TYPE,FSTYPE,SIZE,LABEL,UUID | grep 'TYPE=\"part\"'", "r");
    if (f) {
        char line[1024];
        while (fgets(line, sizeof(line), f)) {
            char name[128] = {0}, fstype[64] = {0}, size[64] = {0}, label[128] = {0};
            
            char *n = strstr(line, "NAME=\"");
            if (n) { n += 6; char *e = strchr(n, '"'); if (e && e - n < sizeof(name) - 1) strncpy(name, n, e - n); }
            
            char *t = strstr(line, "FSTYPE=\"");
            if (t) { t += 8; char *e = strchr(t, '"'); if (e && e - t < sizeof(fstype) - 1) strncpy(fstype, t, e - t); }
            
            char *s = strstr(line, "SIZE=\"");
            if (s) { s += 6; char *e = strchr(s, '"'); if (e && e - s < sizeof(size) - 1) strncpy(size, s, e - s); }
            
            char *l = strstr(line, "LABEL=\"");
            if (l) { l += 7; char *e = strchr(l, '"'); if (e && e - l < sizeof(label) - 1) strncpy(label, l, e - l); }

            GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
            gtk_widget_set_margin_bottom(row, 8);

            GtkWidget *icon = gtk_image_new_from_icon_name("drive-harddisk", GTK_ICON_SIZE_LARGE_TOOLBAR);
            gtk_widget_set_size_request(icon, 32, -1);
            gtk_box_pack_start(GTK_BOX(row), icon, FALSE, FALSE, 0);

            GtkWidget *info = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
            GtkWidget *_lbl = gtk_label_new(NULL);
            char *markup = g_strdup_printf("<b>%s</b>", name);
            gtk_label_set_markup(GTK_LABEL(_lbl), markup);
            g_free(markup);
            gtk_label_set_xalign(GTK_LABEL(_lbl), 0.0);
            gtk_box_pack_start(GTK_BOX(info), _lbl, FALSE, FALSE, 0);

            char desc_text[256];
            snprintf(desc_text, sizeof(desc_text), "Label: %s — %s — %s", 
                     label[0] ? label : "NoLabel", fstype[0] ? fstype : "Unknown", size);
            GtkWidget *_d = gtk_label_new(desc_text);
            gtk_label_set_xalign(GTK_LABEL(_d), 0.0);
            gtk_style_context_add_class(gtk_widget_get_style_context(_d), "dim-label");
            gtk_box_pack_start(GTK_BOX(info), _d, FALSE, FALSE, 0);

            GtkWidget *_btn = gtk_button_new_with_label("Mount");
            gtk_widget_set_valign(_btn, GTK_ALIGN_CENTER);
            g_signal_connect_data(_btn, "clicked", G_CALLBACK(on_mount_partition), g_strdup(name), (GClosureNotify)free_ptr, 0);

            gtk_box_pack_start(GTK_BOX(row), info, TRUE, TRUE, 0);
            gtk_box_pack_start(GTK_BOX(row), _btn, FALSE, FALSE, 0);
            gtk_box_pack_start(GTK_BOX(vbox), row, FALSE, FALSE, 0);
        }
        pclose(f);
    }

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

    /* Determine the active shell so we can pre-highlight the user's
     * current choice (written by install.sh / toggle-shell). */
    char cfg_dir[512], mode_path[512], active_mode[64] = {0};
    get_config_dir(cfg_dir, sizeof(cfg_dir));
    snprintf(mode_path, sizeof(mode_path), "%s/mode", cfg_dir);
    FILE *mf = fopen(mode_path, "r");
    if (mf) {
        if (fgets(active_mode, sizeof(active_mode), mf)) {
            active_mode[strcspn(active_mode, "\n")] = '\0';
        }
        fclose(mf);
    }

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

        if (active_mode[0] && strcmp(active_mode, OCWS_SHELLS[i].mode) == 0) {
            gtk_style_context_add_class(ctx, "suggested-action");
        }
    }

    g_shell_status = gtk_label_new(NULL);
    gtk_label_set_xalign(GTK_LABEL(g_shell_status), 0.0);
    gtk_style_context_add_class(gtk_widget_get_style_context(g_shell_status),
                                "dim-label");
    gtk_box_pack_start(GTK_BOX(vbox), g_shell_status, FALSE, FALSE, 8);

    return page;
}

/* ---- Page 3: Theme ---- */
static void on_open_theme_center(GtkWidget *btn, gpointer data) {
    (void)data;
    send_notification("Theme Center", "Opening Advanced Theme Center...", "preferences-desktop-theme");
    run_cmd_logged("ocws-theme-center &");
}

static void on_open_style(GtkWidget *btn, gpointer data) {
    (void)data;
    send_notification("Style Editor", "Opening Style Editor...", "preferences-desktop-appearance");
    run_cmd_logged("ocws-style &");
}

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

    GtkWidget *sep2 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox), sep2, FALSE, FALSE, 12);

    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(btn_box, GTK_ALIGN_CENTER);
    
    GtkWidget *btn_theme_center = gtk_button_new_with_label("Open Theme Center");
    g_signal_connect(btn_theme_center, "clicked", G_CALLBACK(on_open_theme_center), NULL);
    gtk_box_pack_start(GTK_BOX(btn_box), btn_theme_center, FALSE, FALSE, 0);

    GtkWidget *btn_style = gtk_button_new_with_label("Edit Stylesheets (ocws-style)");
    g_signal_connect(btn_style, "clicked", G_CALLBACK(on_open_style), NULL);
    gtk_box_pack_start(GTK_BOX(btn_box), btn_style, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), btn_box, FALSE, FALSE, 8);

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

    OPTION_ROW(vbox,
        "Test Notifications",
        "Send a test notification to verify your daemon is running",
        "Test",
        on_test_notification);

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
    run_cmd_logged(full_cmd);
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
    run_cmd_logged(cmd);
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
    gtk_stack_add_named(GTK_STACK(g_stack), build_health_page(),  "health");
    gtk_stack_add_named(GTK_STACK(g_stack), build_monitors_page(),"monitors");
    gtk_stack_add_named(GTK_STACK(g_stack), build_mount_page(),   "mount");
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
