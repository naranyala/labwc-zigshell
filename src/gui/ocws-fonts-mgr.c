/*
 * ocws-fonts-mgr.c — OCWS Fonts Manager GUI
 *
 * GTK3 application for:
 *   1. System font scanning — discover all installed fonts
 *   2. Online font installer — download & install fonts as dotfiles
 *   3. Managed fonts — view/remove OCWS-installed fonts
 *   4. Font config — fontconfig & font scale integration
 *
 * Build:
 *   zig build   (handled by build.zig)
 */

#include <gtk/gtk.h>
#include <glib.h>
#include <gio/gio.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include "utils.h"
#include "ocws-theme-utils.h"
#include "ocws-fonts.h"
#include "../libocws/gtk.h"

#define APP_ID "org.ocws.fontsmgr"

/* ================================================================
 * Font Packages — runtime wrapper around shared library packages
 * ================================================================ */

typedef struct {
    const ocws_font_pkg_t *pkg;
    int installed;   /* runtime: 1 if already in system */
    int managed;     /* runtime: 1 if installed by OCWS */
} FontPackage;

static FontPackage *g_packages = NULL;

/* ================================================================
 * System Font Entry
 * ================================================================ */

typedef struct {
    char *family;
    char *style;
    char *file;
    int is_managed;  /* installed by OCWS */
} SystemFont;

static SystemFont *g_system_fonts = NULL;
static int g_system_font_count = 0;
static int g_system_font_capacity = 0;

/* ================================================================
 * Globals
 * ================================================================ */

static GtkListStore *g_font_list_store = NULL;
static GtkTreeModelFilter *g_font_filter = NULL;
static GtkSearchEntry *g_search_entry = NULL;
static GtkTreeView *g_treeview = NULL;
static GtkTextBuffer *g_log_buffer = NULL;
static GtkWidget *g_log_view = NULL;
static GtkWidget *g_install_btn = NULL;
static GtkWidget *g_remove_btn = NULL;
static GtkLabel *g_stats_label = NULL;
static GtkListStore *g_managed_store = NULL;

/* ================================================================
 * Font Preview Panel
 * ================================================================ */

static GtkWidget *g_preview_box = NULL;
static GtkLabel *g_preview_family = NULL;
static GtkLabel *g_preview_style = NULL;
static GtkLabel *g_preview_file = NULL;
static GtkLabel *g_preview_sample_sm = NULL;
static GtkLabel *g_preview_sample_md = NULL;
static GtkLabel *g_preview_sample_lg = NULL;
static GtkLabel *g_preview_sample_xl = NULL;
static GtkLabel *g_preview_bold = NULL;
static GtkLabel *g_preview_italic = NULL;
static GtkLabel *g_preview_bold_italic = NULL;
static GtkLabel *g_preview_mono = NULL;
static GtkLabel *g_preview_empty = NULL;
static GtkEntry *g_preview_text_entry = NULL;

static const char *FONTS_DIR = NULL;
static const char *MANAGED_DIR = NULL;
static const char *CURSORS_DIR = NULL;

/* ================================================================
 * Helpers
 * ================================================================ */

static void init_paths(void) {
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";

    static char fonts_buf[512];
    static char managed_buf[512];
    static char cursors_buf[512];

    snprintf(fonts_buf, sizeof(fonts_buf), "%s/.local/share/fonts", home);
    snprintf(managed_buf, sizeof(managed_buf), "%s/.local/share/fonts/ocws-managed", home);
    snprintf(cursors_buf, sizeof(cursors_buf), "%s/.local/share/icons", home);

    FONTS_DIR = fonts_buf;
    MANAGED_DIR = managed_buf;
    CURSORS_DIR = cursors_buf;
}

static gboolean append_log_idle(gpointer data) {
    char *msg = (char *)data;
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(g_log_buffer, &end);
    gtk_text_buffer_insert(g_log_buffer, &end, msg, -1);
    gtk_text_buffer_insert(g_log_buffer, &end, "\n", -1);

    gtk_text_buffer_get_end_iter(g_log_buffer, &end);
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(g_log_view), &end, 0.0, FALSE, 0.0, 1.0);
    g_free(msg);
    return G_SOURCE_REMOVE;
}

static void log_msg(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    char *msg = g_strdup(buf);
    g_idle_add(append_log_idle, msg);
}

static void run_cmd_logged(const char *cmd) {
    log_msg("$ %s", cmd);
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        log_msg("ERROR: Failed to execute command");
        return;
    }
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0;
        log_msg("  %s", line);
    }
    int ret = pclose(fp);
    if (WIFEXITED(ret) && WEXITSTATUS(ret) != 0) {
        log_msg("Exit code: %d", WEXITSTATUS(ret));
    }
}

static int dir_exists(const char *path) { return ocws_fonts_dir_exists(path); }
static int file_exists(const char *path) { return ocws_fonts_file_exists(path); }
static void make_dir_p(const char *path) { ocws_fonts_make_dir_p(path); }

/* ================================================================
 * Managed Fonts Tracking
 *
 * OCWS-installed fonts are tracked in:
 *   ~/.local/share/fonts/ocws-managed/<package>/
 *   ~/.local/share/fonts/ocws-managed/<package>/.ocws-meta
 *
 * The .ocws-meta file contains:
 *   package=<name>
 *   url=<source_url>
 *   installed=<timestamp>
 * ================================================================ */

static void mark_font_managed(const char *pkg_name, const char *url) {
    char meta_dir[512];
    snprintf(meta_dir, sizeof(meta_dir), "%s/%s", MANAGED_DIR, pkg_name);
    make_dir_p(meta_dir);

    char meta_path[512];
    snprintf(meta_path, sizeof(meta_path), "%s/.ocws-meta", meta_dir);

    FILE *f = fopen(meta_path, "w");
    if (f) {
        time_t now = time(NULL);
        char timebuf[64];
        strftime(timebuf, sizeof(timebuf), "%Y-%m-%dT%H:%M:%S", localtime(&now));
        fprintf(f, "package=%s\n", pkg_name);
        fprintf(f, "url=%s\n", url);
        fprintf(f, "installed=%s\n", timebuf);
        fclose(f);
    }
}

static int is_font_managed(const char *pkg_name) {
    char meta_path[512];
    snprintf(meta_path, sizeof(meta_path), "%s/%s/.ocws-meta", MANAGED_DIR, pkg_name);
    return file_exists(meta_path);
}

static void load_managed_list(GtkListStore *store) {
    gtk_list_store_clear(store);

    if (!dir_exists(MANAGED_DIR)) return;

    DIR *d = opendir(MANAGED_DIR);
    if (!d) return;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char meta_path[512];
        snprintf(meta_path, sizeof(meta_path), "%s/%s/.ocws-meta", MANAGED_DIR, entry->d_name);

        if (!file_exists(meta_path)) continue;

        FILE *f = fopen(meta_path, "r");
        if (!f) continue;

        char pkg_name[256] = {0};
        char url[512] = {0};
        char installed_time[128] = {0};
        char line[512];

        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\n")] = 0;
            if (sscanf(line, "package=%255s", pkg_name) == 1) continue;
            if (sscanf(line, "url=%511s", url) == 1) continue;
            if (sscanf(line, "installed=%127s", installed_time) == 1) continue;
        }
        fclose(f);

        GtkTreeIter iter;
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
            0, pkg_name[0] ? pkg_name : entry->d_name,
            1, url[0] ? url : "-",
            2, installed_time[0] ? installed_time : "-",
            -1);
    }
    closedir(d);
}

/* ================================================================
 * System Font Scanner
 * ================================================================ */

static void free_system_fonts(void) {
    for (int i = 0; i < g_system_font_count; i++) {
        g_free(g_system_fonts[i].family);
        g_free(g_system_fonts[i].style);
        g_free(g_system_fonts[i].file);
    }
    g_free(g_system_fonts);
    g_system_fonts = NULL;
    g_system_font_count = 0;
    g_system_font_capacity = 0;
}

static void scan_system_fonts(void) {
    free_system_fonts();

    log_msg("Scanning system fonts...");

    gchar *stdout_buf = NULL;
    gint exit_status;

    const gchar *argv[] = {"fc-list", ":", "file", "family", "style", NULL};

    if (!g_spawn_sync(NULL, (gchar **)argv, NULL, G_SPAWN_SEARCH_PATH,
                       NULL, NULL, &stdout_buf, NULL, &exit_status, NULL)) {
        log_msg("ERROR: fc-list failed");
        return;
    }

    gchar **lines = g_strsplit(stdout_buf, "\n", -1);
    for (int i = 0; lines[i] != NULL; i++) {
        if (strlen(lines[i]) == 0) continue;

        gchar **parts = g_strsplit(lines[i], ":", 3);
        if (g_strv_length(parts) >= 2) {
            if (g_system_font_count >= g_system_font_capacity) {
                g_system_font_capacity = g_system_font_capacity ? g_system_font_capacity * 2 : 512;
                g_system_fonts = g_realloc(g_system_fonts, g_system_font_capacity * sizeof(SystemFont));
            }

            SystemFont *sf = &g_system_fonts[g_system_font_count++];
            sf->file = g_strstrip(g_strdup(parts[0]));
            sf->family = g_strstrip(g_strdup(parts[1]));
            sf->style = g_strv_length(parts) == 3 ? g_strstrip(g_strdup(parts[2])) : g_strdup("Regular");

            /* Check if managed by OCWS */
            sf->is_managed = 0;
            if (sf->file && strstr(sf->file, "ocws-managed")) {
                sf->is_managed = 1;
            }
        }
        g_strfreev(parts);
    }
    g_strfreev(lines);
    if (stdout_buf) g_free(stdout_buf);

    log_msg("Found %d font entries", g_system_font_count);
}

static void populate_font_list(void) {
    gtk_list_store_clear(g_font_list_store);

    for (int i = 0; i < g_system_font_count; i++) {
        GtkTreeIter iter;
        gtk_list_store_append(g_font_list_store, &iter);
        gtk_list_store_set(g_font_list_store, &iter,
            0, g_system_fonts[i].family,
            1, g_system_fonts[i].style,
            2, g_system_fonts[i].file,
            3, g_system_fonts[i].is_managed ? "OCWS" : "System",
            -1);
    }

    if (g_stats_label) {
        char buf[128];
        int managed_count = 0;
        for (int i = 0; i < g_system_font_count; i++) {
            if (g_system_fonts[i].is_managed) managed_count++;
        }
        snprintf(buf, sizeof(buf), "%d fonts total, %d OCWS-managed", g_system_font_count, managed_count);
        gtk_label_set_text(g_stats_label, buf);
    }
}

/* ================================================================
 * Font Filter (search)
 * ================================================================ */

static gboolean font_filter_func(GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data) {
    (void)user_data;
    const gchar *query = gtk_entry_get_text(GTK_ENTRY(g_search_entry));
    if (!query || query[0] == '\0') return TRUE;

    gchar *family = NULL;
    gchar *style = NULL;
    gchar *file = NULL;
    gtk_tree_model_get(model, iter, 0, &family, 1, &style, 2, &file, -1);

    gchar *query_lower = g_utf8_strdown(query, -1);
    gboolean match = FALSE;

    if (family) {
        gchar *f_lower = g_utf8_strdown(family, -1);
        if (strstr(f_lower, query_lower)) match = TRUE;
        g_free(f_lower);
    }
    if (!match && style) {
        gchar *s_lower = g_utf8_strdown(style, -1);
        if (strstr(s_lower, query_lower)) match = TRUE;
        g_free(s_lower);
    }
    if (!match && file) {
        gchar *fl_lower = g_utf8_strdown(file, -1);
        if (strstr(fl_lower, query_lower)) match = TRUE;
        g_free(fl_lower);
    }

    g_free(query_lower);
    if (family) g_free(family);
    if (style) g_free(style);
    if (file) g_free(file);

    return match;
}

static void on_search_changed(GtkSearchEntry *entry, gpointer user_data) {
    (void)entry;
    (void)user_data;
    gtk_tree_model_filter_refilter(g_font_filter);
}

/* ================================================================
 * Font Preview — render selected font at various sizes/styles
 * ================================================================ */

static const char *PREVIEW_TEXT = "The quick brown fox jumps over the lazy dog";
static const char *PREVIEW_TEXT_MONO = "AaBbCcDdEeFf 0123456789 !@#$%^&*()";

static void set_label_font(GtkLabel *label, const char *family, int size, int bold, int italic) {
    PangoFontDescription *pfd = pango_font_description_new();
    pango_font_description_set_family(pfd, family);
    pango_font_description_set_size(pfd, size * PANGO_SCALE);

    PangoWeight weight = bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL;
    pango_font_description_set_weight(pfd, weight);

    PangoStyle style = italic ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL;
    pango_font_description_set_style(pfd, style);

    gtk_widget_override_font(GTK_WIDGET(label), pfd);
    pango_font_description_free(pfd);
}

static void update_font_preview(const char *family, const char *style, const char *file) {
    if (!g_preview_box) return;

    if (!family || family[0] == '\0') {
        gtk_widget_hide(g_preview_box);
        gtk_widget_show(g_preview_empty);
        return;
    }

    gtk_widget_show(g_preview_box);
    gtk_widget_hide(g_preview_empty);

    /* Header info */
    char buf[512];
    snprintf(buf, sizeof(buf), "%s", family);
    gtk_label_set_text(g_preview_family, buf);

    snprintf(buf, sizeof(buf), "%s", style ? style : "Regular");
    gtk_label_set_text(g_preview_style, buf);

    /* Shorten file path for display */
    const char *short_file = file;
    if (file) {
        const char *last_slash = strrchr(file, '/');
        if (last_slash) short_file = last_slash + 1;
    }
    snprintf(buf, sizeof(buf), "%s", short_file ? short_file : "-");
    gtk_label_set_text(g_preview_file, buf);

    /* Get custom text or default */
    const char *text = PREVIEW_TEXT;
    if (g_preview_text_entry) {
        const char *custom = gtk_entry_get_text(g_preview_text_entry);
        if (custom && custom[0] != '\0') {
            text = custom;
        }
    }

    /* Size variants */
    gtk_label_set_text(g_preview_sample_sm, text);
    set_label_font(g_preview_sample_sm, family, 10, 0, 0);

    gtk_label_set_text(g_preview_sample_md, text);
    set_label_font(g_preview_sample_md, family, 14, 0, 0);

    gtk_label_set_text(g_preview_sample_lg, text);
    set_label_font(g_preview_sample_lg, family, 20, 0, 0);

    gtk_label_set_text(g_preview_sample_xl, text);
    set_label_font(g_preview_sample_xl, family, 28, 0, 0);

    /* Style variants */
    gtk_label_set_text(g_preview_bold, text);
    set_label_font(g_preview_bold, family, 16, 1, 0);

    gtk_label_set_text(g_preview_italic, text);
    set_label_font(g_preview_italic, family, 16, 0, 1);

    gtk_label_set_text(g_preview_bold_italic, text);
    set_label_font(g_preview_bold_italic, family, 16, 1, 1);

    /* Monospace style preview */
    gtk_label_set_text(g_preview_mono, PREVIEW_TEXT_MONO);
    set_label_font(g_preview_mono, family, 13, 0, 0);
}

static void on_font_row_activated(GtkTreeView *tree_view, GtkTreePath *path,
                                   GtkTreeViewColumn *column, gpointer user_data) {
    (void)column; (void)user_data;
    GtkTreeModel *model = gtk_tree_view_get_model(tree_view);
    GtkTreeIter iter;

    if (!gtk_tree_model_get_iter(model, &iter, path)) return;

    gchar *family = NULL;
    gchar *style = NULL;
    gchar *file = NULL;
    gtk_tree_model_get(model, &iter, 0, &family, 1, &style, 2, &file, -1);

    update_font_preview(family, style, file);

    g_free(family);
    g_free(style);
    g_free(file);
}

static void on_font_selection_changed(GtkTreeSelection *selection, gpointer user_data) {
    (void)user_data;
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        update_font_preview(NULL, NULL, NULL);
        return;
    }

    gchar *family = NULL;
    gchar *style = NULL;
    gchar *file = NULL;
    gtk_tree_model_get(model, &iter, 0, &family, 1, &style, 2, &file, -1);

    update_font_preview(family, style, file);

    g_free(family);
    g_free(style);
    g_free(file);
}

static void on_preview_text_changed(GtkEntry *entry, gpointer user_data) {
    (void)user_data;
    GtkTreeSelection *sel = gtk_tree_view_get_selection(g_treeview);
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (!gtk_tree_selection_get_selected(sel, &model, &iter)) return;

    gchar *family = NULL;
    gchar *style = NULL;
    gchar *file = NULL;
    gtk_tree_model_get(model, &iter, 0, &family, 1, &style, 2, &file, -1);

    update_font_preview(family, style, file);

    g_free(family);
    g_free(style);
    g_free(file);
}

static void on_preview_size_changed(GtkSpinButton *spin, gpointer user_data) {
    (void)user_data;
    int size = (int)gtk_spin_button_get_value_as_int(spin);

    GtkTreeSelection *sel = gtk_tree_view_get_selection(g_treeview);
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (!gtk_tree_selection_get_selected(sel, &model, &iter)) return;

    gchar *family = NULL;
    gchar *style = NULL;
    gchar *file = NULL;
    gtk_tree_model_get(model, &iter, 0, &family, 1, &style, 2, &file, -1);

    /* Re-render all size variants with custom size */
    const char *text = PREVIEW_TEXT;
    if (g_preview_text_entry) {
        const char *custom = gtk_entry_get_text(g_preview_text_entry);
        if (custom && custom[0] != '\0') text = custom;
    }

    if (family) {
        gtk_label_set_text(g_preview_sample_sm, text);
        set_label_font(g_preview_sample_sm, family, size, 0, 0);

        gtk_label_set_text(g_preview_sample_md, text);
        set_label_font(g_preview_sample_md, family, (int)(size * 1.4), 0, 0);

        gtk_label_set_text(g_preview_sample_lg, text);
        set_label_font(g_preview_sample_lg, family, (int)(size * 2.0), 0, 0);

        gtk_label_set_text(g_preview_sample_xl, text);
        set_label_font(g_preview_sample_xl, family, (int)(size * 2.8), 0, 0);

        gtk_label_set_text(g_preview_bold, text);
        set_label_font(g_preview_bold, family, size, 1, 0);

        gtk_label_set_text(g_preview_italic, text);
        set_label_font(g_preview_italic, family, size, 0, 1);

        gtk_label_set_text(g_preview_bold_italic, text);
        set_label_font(g_preview_bold_italic, family, size, 1, 1);

        gtk_label_set_text(g_preview_mono, PREVIEW_TEXT_MONO);
        set_label_font(g_preview_mono, family, (int)(size * 0.85), 0, 0);
    }

    g_free(family);
    g_free(style);
    g_free(file);
}

/* ================================================================
 * Online Installer — download & install
 * ================================================================ */

typedef struct {
    int pkg_index;
    GtkWidget *btn;
    GtkWidget *status_lbl;
} InstallRowData;

typedef struct {
    GtkWidget *label;
    const char *text;
} LabelUpdate;

static gboolean set_label_text_idle(gpointer data) {
    LabelUpdate *u = (LabelUpdate *)data;
    gtk_label_set_text(GTK_LABEL(u->label), u->text);
    g_free(u);
    return G_SOURCE_REMOVE;
}

static void set_label_async(GtkWidget *label, const char *text) {
    LabelUpdate *u = g_new0(LabelUpdate, 1);
    u->label = label;
    u->text = g_strdup(text);
    g_idle_add(set_label_text_idle, u);
}

typedef struct {
    GtkWidget *widget;
    gboolean sensitive;
} SensitivityUpdate;

static gboolean set_sensitivity_idle(gpointer data) {
    SensitivityUpdate *u = (SensitivityUpdate *)data;
    gtk_widget_set_sensitive(u->widget, u->sensitive);
    g_free(u);
    return G_SOURCE_REMOVE;
}

static void set_sensitive_async(GtkWidget *widget, gboolean sensitive) {
    SensitivityUpdate *u = g_new0(SensitivityUpdate, 1);
    u->widget = widget;
    u->sensitive = sensitive;
    g_idle_add(set_sensitivity_idle, u);
}

static gpointer install_worker(gpointer user_data) {
    InstallRowData *d = (InstallRowData *)user_data;
    FontPackage *fp = &g_packages[d->pkg_index];
    const ocws_font_pkg_t *pkg = fp->pkg;

    set_label_async(d->status_lbl, "Installing...");

    log_msg("=== Installing %s ===", pkg->name);
    log_msg("URL: %s", pkg->url);

    /* Determine install directory */
    const char *base_dir = pkg->is_cursor ? CURSORS_DIR : FONTS_DIR;
    char install_dir[512];
    snprintf(install_dir, sizeof(install_dir), "%s/%s", base_dir, pkg->install_subdir);
    make_dir_p(install_dir);

    /* Download to /tmp */
    char tmp_path[512];
    snprintf(tmp_path, sizeof(tmp_path), "/tmp/ocws-font-%s", pkg->archive_name);

    char cmd[2048];

    /* Download */
    set_label_async(d->status_lbl, "Downloading...");

    if (strstr(pkg->url, ".ttf") && !strstr(pkg->url, ".tar.") && !strstr(pkg->url, ".zip")) {
        /* Single TTF file — direct download */
        snprintf(cmd, sizeof(cmd), "curl -fLsS -o '%s/%s' '%s' 2>&1",
            install_dir, pkg->archive_name, pkg->url);
        run_cmd_logged(cmd);
    } else {
        /* Archive — download then extract */
        snprintf(cmd, sizeof(cmd),
            "curl -fLsS -o '%s' '%s' 2>&1 || wget -q -O '%s' '%s' 2>&1",
            tmp_path, pkg->url, tmp_path, pkg->url);
        run_cmd_logged(cmd);

        /* Extract */
        set_label_async(d->status_lbl, "Extracting...");

        const char *ext = strrchr(pkg->archive_name, '.');
        if (ext && strcmp(ext, ".zip") == 0) {
            snprintf(cmd, sizeof(cmd), "unzip -qo '%s' -d '%s' 2>&1", tmp_path, install_dir);
        } else if (strstr(pkg->archive_name, ".tar.xz")) {
            snprintf(cmd, sizeof(cmd), "tar -xJf '%s' -C '%s' 2>&1", tmp_path, install_dir);
        } else if (strstr(pkg->archive_name, ".tar.gz") || strstr(pkg->archive_name, ".tgz")) {
            snprintf(cmd, sizeof(cmd), "tar -xzf '%s' -C '%s' 2>&1", tmp_path, install_dir);
        } else {
            snprintf(cmd, sizeof(cmd), "cp '%s' '%s/' 2>&1", tmp_path, install_dir);
        }
        run_cmd_logged(cmd);

        /* Cleanup temp */
        snprintf(cmd, sizeof(cmd), "rm -f '%s'", tmp_path);
        system(cmd);
    }

    /* Mark as managed */
    set_label_async(d->status_lbl, "Tracking...");

    mark_font_managed(pkg->name, pkg->url);
    log_msg("Marked as OCWS-managed: %s", pkg->name);

    /* Rebuild font cache */
    set_label_async(d->status_lbl, "Rebuilding cache...");

    log_msg("Rebuilding font cache...");
    run_cmd_logged("fc-cache -fv 2>&1 | tail -5");

    /* Update UI */
    set_label_async(d->status_lbl, "Installed");

    if (d->btn) {
        set_sensitive_async(d->btn, TRUE);
    }

    log_msg("=== %s installation complete ===", pkg->name);

    /* Refresh managed list */
    gdk_threads_add_idle_full(G_PRIORITY_HIGH,
        (GSourceFunc)load_managed_list, g_managed_store, NULL);

    return NULL;
}

static void on_install_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    InstallRowData *d = (InstallRowData *)data;
    gtk_widget_set_sensitive(d->btn, FALSE);
    g_thread_new("install-font", install_worker, d);
}

/* ================================================================
 * Remove managed font
 * ================================================================ */

typedef struct {
    char pkg_name[256];
    char pkg_url[512];
} RemoveRowData;

static gpointer remove_worker(gpointer user_data) {
    RemoveRowData *d = (RemoveRowData *)user_data;

    log_msg("=== Removing managed font: %s ===", d->pkg_name);

    /* Find which package this is */
    const ocws_font_pkg_t *pkg = NULL;
    for (int i = 0; i < OCWS_FONT_PACKAGE_COUNT; i++) {
        if (strcmp(g_packages[i].pkg->name, d->pkg_name) == 0) {
            pkg = g_packages[i].pkg;
            break;
        }
    }

    if (!pkg) {
        log_msg("ERROR: Package not found: %s", d->pkg_name);
        g_free(d);
        return NULL;
    }

    /* Remove the installed directory */
    const char *base_dir = pkg->is_cursor ? CURSORS_DIR : FONTS_DIR;
    char install_dir[512];
    snprintf(install_dir, sizeof(install_dir), "%s/%s", base_dir, pkg->install_subdir);

    char cmd[1024];
    if (dir_exists(install_dir)) {
        snprintf(cmd, sizeof(cmd), "rm -rf '%s'", install_dir);
        run_cmd_logged(cmd);
    }

    /* Remove managed metadata */
    char meta_dir[512];
    snprintf(meta_dir, sizeof(meta_dir), "%s/%s", MANAGED_DIR, d->pkg_name);
    if (dir_exists(meta_dir)) {
        snprintf(cmd, sizeof(cmd), "rm -rf '%s'", meta_dir);
        run_cmd_logged(cmd);
    }

    /* Rebuild font cache */
    log_msg("Rebuilding font cache...");
    run_cmd_logged("fc-cache -fv 2>&1 | tail -5");

    log_msg("=== %s removed ===", d->pkg_name);

    /* Refresh lists */
    gdk_threads_add_idle_full(G_PRIORITY_HIGH,
        (GSourceFunc)load_managed_list, g_managed_store, NULL);

    g_free(d);
    return NULL;
}

static void on_remove_managed(GtkWidget *widget, gpointer tree_view) {
    (void)widget;
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (!gtk_tree_selection_get_selected(sel, &model, &iter)) {
        log_msg("No font selected for removal");
        return;
    }

    char *name = NULL;
    char *url = NULL;
    gtk_tree_model_get(model, &iter, 0, &name, 1, &url, -1);

    if (!name) return;

    RemoveRowData *d = g_new0(RemoveRowData, 1);
    strncpy(d->pkg_name, name, sizeof(d->pkg_name) - 1);
    if (url) strncpy(d->pkg_url, url, sizeof(d->pkg_url) - 1);

    g_free(name);
    if (url) g_free(url);

    g_thread_new("remove-font", remove_worker, d);
}

/* ================================================================
 * Font Scale integration
 * ================================================================ */

static void on_font_scale_up(GtkWidget *widget, gpointer data) {
    (void)widget; (void)data;
    log_msg("Running: font-scale up");
    run_cmd_logged("font-scale up 2>&1");
}

static void on_font_scale_down(GtkWidget *widget, gpointer data) {
    (void)widget; (void)data;
    log_msg("Running: font-scale down");
    run_cmd_logged("font-scale down 2>&1");
}

static void on_font_scale_status(GtkWidget *widget, gpointer data) {
    (void)widget; (void)data;
    log_msg("Running: font-scale status");
    run_cmd_logged("font-scale status 2>&1");
}

static void on_font_scale_reset(GtkWidget *widget, gpointer data) {
    (void)widget; (void)data;
    log_msg("Running: font-scale reset");
    run_cmd_logged("font-scale reset 2>&1");
}

/* ================================================================
 * Rebuild font cache
 * ================================================================ */

static gpointer rebuild_cache_worker(gpointer user_data) {
    GtkWidget *label = GTK_WIDGET(user_data);
    set_label_async(label, "Rebuilding...");
    log_msg("Rebuilding font cache...");
    run_cmd_logged("fc-cache -fv 2>&1 | tail -5");
    log_msg("Font cache rebuilt");
    set_label_async(label, "Done");
    return NULL;
}

static void on_rebuild_cache(GtkWidget *widget, gpointer data) {
    (void)widget;
    g_thread_new("rebuild-cache", rebuild_cache_worker, data);
}

/* ================================================================
 * CSS theming
 * ================================================================ */



/* ================================================================
 * UI Construction
 * ================================================================ */

static void activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;
    init_paths();
    ocws_gtk_enforce_premium_theme();
    ocws_gtk_apply_dynamic_css(app, NULL);

    /* Ensure managed dir exists */
    make_dir_p(MANAGED_DIR);

    /* Initialize shared packages */
    g_packages = calloc(OCWS_FONT_PACKAGE_COUNT, sizeof(FontPackage));
    for (int i = 0; i < OCWS_FONT_PACKAGE_COUNT; i++) {
        g_packages[i].pkg = &OCWS_FONT_PACKAGES[i];
    }

    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "OCWS Fonts Manager");
    gtk_window_set_default_size(GTK_WINDOW(window), 1000, 700);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

    /* Header */
    GtkWidget *header = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), TRUE);
    gtk_header_bar_set_title(GTK_HEADER_BAR(header), "OCWS Fonts Manager");
    gtk_header_bar_set_subtitle(GTK_HEADER_BAR(header), "System Fonts & Online Installer");
    gtk_window_set_titlebar(GTK_WINDOW(window), header);

    /* Main layout */
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(window), hbox);

    /* Sidebar & Stack */
    GtkWidget *sidebar = gtk_stack_sidebar_new();
    gtk_widget_set_size_request(sidebar, 180, -1);
    GtkWidget *stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_SLIDE_UP_DOWN);
    gtk_stack_sidebar_set_stack(GTK_STACK_SIDEBAR(sidebar), GTK_STACK(stack));

    gtk_box_pack_start(GTK_BOX(hbox), sidebar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_separator_new(GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), stack, TRUE, TRUE, 0);

    /* ============================================================
     * Tab 1: System Fonts (split: tree left, preview right)
     * ============================================================ */
    GtkWidget *sys_paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_container_set_border_width(GTK_CONTAINER(sys_paned), 5);

    /* --- Left: font list --- */
    GtkWidget *sys_left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_size_request(sys_left, 500, -1);

    /* Stats bar */
    GtkWidget *stats_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_bottom(stats_box, 5);

    g_stats_label = GTK_LABEL(gtk_label_new("Scanning..."));
    gtk_label_set_xalign(GTK_LABEL(g_stats_label), 0.0);
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(g_stats_label)), "dim-label");
    gtk_box_pack_start(GTK_BOX(stats_box), GTK_WIDGET(g_stats_label), TRUE, TRUE, 0);

    GtkWidget *refresh_btn = gtk_button_new_with_label("Refresh");
    g_signal_connect(refresh_btn, "clicked", G_CALLBACK(on_rebuild_cache), g_stats_label);
    gtk_box_pack_start(GTK_BOX(stats_box), refresh_btn, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(sys_left), stats_box, FALSE, FALSE, 0);

    /* Search */
    g_search_entry = GTK_SEARCH_ENTRY(gtk_search_entry_new());
    gtk_entry_set_placeholder_text(GTK_ENTRY(g_search_entry), "Search fonts by family, style, or path...");
    g_signal_connect(g_search_entry, "search-changed", G_CALLBACK(on_search_changed), NULL);
    gtk_box_pack_start(GTK_BOX(sys_left), GTK_WIDGET(g_search_entry), FALSE, FALSE, 5);

    /* Font list store: family, style, file, source */
    g_font_list_store = gtk_list_store_new(4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    g_font_filter = GTK_TREE_MODEL_FILTER(gtk_tree_model_filter_new(GTK_TREE_MODEL(g_font_list_store), NULL));
    gtk_tree_model_filter_set_visible_func(g_font_filter, font_filter_func, NULL, NULL);

    g_treeview = GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(g_font_filter)));
    const char *titles[] = {"Family", "Style", "File Path", "Source"};
    int widths[] = {200, 150, 300, 80};
    for (int i = 0; i < 4; i++) {
        GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
        GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(titles[i], renderer, "text", i, NULL);
        gtk_tree_view_column_set_sort_column_id(column, i);
        gtk_tree_view_column_set_resizable(column, TRUE);
        gtk_tree_view_column_set_min_width(column, widths[i]);
        gtk_tree_view_append_column(g_treeview, column);
    }

    /* Connect row-activated (double-click) for preview */
    g_signal_connect(g_treeview, "row-activated", G_CALLBACK(on_font_row_activated), NULL);

    /* Connect selection change for live preview */
    GtkTreeSelection *font_sel = gtk_tree_view_get_selection(g_treeview);
    g_signal_connect(font_sel, "changed", G_CALLBACK(on_font_selection_changed), NULL);

    GtkWidget *scrolled_tree = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_tree), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scrolled_tree), GTK_WIDGET(g_treeview));
    gtk_box_pack_start(GTK_BOX(sys_left), scrolled_tree, TRUE, TRUE, 0);

    gtk_paned_pack1(GTK_PANED(sys_paned), sys_left, TRUE, TRUE);

    /* --- Right: font preview panel --- */
    GtkWidget *sys_right_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sys_right_scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    g_preview_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    /* Empty state */
    g_preview_empty = GTK_LABEL(gtk_label_new("Select a font to preview"));
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(g_preview_empty)), "dim-label");
    gtk_widget_set_vexpand(GTK_WIDGET(g_preview_empty), TRUE);
    gtk_widget_set_valign(GTK_WIDGET(g_preview_empty), GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(g_preview_box), GTK_WIDGET(g_preview_empty), FALSE, FALSE, 0);

    /* Preview content (hidden until selection) */
    GtkWidget *preview_content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(preview_content, 15);
    gtk_widget_set_margin_end(preview_content, 15);
    gtk_widget_set_margin_top(preview_content, 10);

    /* Font name header */
    GtkWidget *header_frame = gtk_frame_new(NULL);
    GtkStyleContext *hf_ctx = gtk_widget_get_style_context(header_frame);
    gtk_style_context_add_class(hf_ctx, "pkg-card");
    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_container_set_border_width(GTK_CONTAINER(header_box), 8);
    gtk_container_add(GTK_CONTAINER(header_frame), header_box);

    g_preview_family = GTK_LABEL(gtk_label_new(NULL));
    PangoFontDescription *pfd_title = pango_font_description_from_string("Noto Sans Bold 16");
    gtk_widget_override_font(GTK_WIDGET(g_preview_family), pfd_title);
    pango_font_description_free(pfd_title);
    gtk_label_set_xalign(g_preview_family, 0.0);
    gtk_box_pack_start(GTK_BOX(header_box), GTK_WIDGET(g_preview_family), FALSE, FALSE, 0);

    GtkWidget *style_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    g_preview_style = GTK_LABEL(gtk_label_new("Regular"));
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(g_preview_style)), "dim-label");
    gtk_label_set_xalign(g_preview_style, 0.0);
    gtk_box_pack_start(GTK_BOX(style_row), GTK_WIDGET(g_preview_style), FALSE, FALSE, 0);

    g_preview_file = GTK_LABEL(gtk_label_new("-"));
    gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(g_preview_file)), "dim-label");
    gtk_label_set_xalign(g_preview_file, 0.0);
    gtk_label_set_ellipsize(g_preview_file, PANGO_ELLIPSIZE_MIDDLE);
    gtk_box_pack_start(GTK_BOX(style_row), GTK_WIDGET(g_preview_file), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(header_box), style_row, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(preview_content), header_frame, FALSE, FALSE, 0);

    /* Custom text entry */
    GtkWidget *text_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_margin_top(text_row, 10);
    gtk_widget_set_margin_bottom(text_row, 5);

    GtkWidget *text_lbl = gtk_label_new("Preview:");
    gtk_style_context_add_class(gtk_widget_get_style_context(text_lbl), "dim-label");
    gtk_box_pack_start(GTK_BOX(text_row), text_lbl, FALSE, FALSE, 0);

    g_preview_text_entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_text(g_preview_text_entry, PREVIEW_TEXT);
    gtk_entry_set_placeholder_text(g_preview_text_entry, "Custom preview text...");
    g_signal_connect(g_preview_text_entry, "changed", G_CALLBACK(on_preview_text_changed), NULL);
    gtk_box_pack_start(GTK_BOX(text_row), GTK_WIDGET(g_preview_text_entry), TRUE, TRUE, 0);

    /* Base size spinner */
    GtkWidget *size_lbl = gtk_label_new("Base size:");
    gtk_style_context_add_class(gtk_widget_get_style_context(size_lbl), "dim-label");
    gtk_box_pack_start(GTK_BOX(text_row), size_lbl, FALSE, FALSE, 0);

    GtkAdjustment *size_adj = gtk_adjustment_new(14, 8, 48, 1, 2, 0);
    GtkWidget *size_spin = gtk_spin_button_new(size_adj, 1, 0);
    gtk_widget_set_size_request(size_spin, 50, -1);
    g_signal_connect(size_spin, "value-changed", G_CALLBACK(on_preview_size_changed), NULL);
    gtk_box_pack_start(GTK_BOX(text_row), size_spin, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(preview_content), text_row, FALSE, FALSE, 0);

    /* Separator */
    gtk_box_pack_start(GTK_BOX(preview_content),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 5);

    /* Section: Size Variants */
    GtkWidget *size_hdr = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(size_hdr), "<span weight='bold' alpha='70%%'>Size Variants</span>");
    gtk_label_set_xalign(size_hdr, 0.0);
    gtk_widget_set_margin_bottom(size_hdr, 4);
    gtk_box_pack_start(GTK_BOX(preview_content), size_hdr, FALSE, FALSE, 0);

    g_preview_sample_sm = GTK_LABEL(gtk_label_new(PREVIEW_TEXT));
    gtk_label_set_xalign(g_preview_sample_sm, 0.0);
    gtk_label_set_line_wrap(g_preview_sample_sm, TRUE);
    gtk_box_pack_start(GTK_BOX(preview_content), GTK_WIDGET(g_preview_sample_sm), FALSE, FALSE, 2);

    g_preview_sample_md = GTK_LABEL(gtk_label_new(PREVIEW_TEXT));
    gtk_label_set_xalign(g_preview_sample_md, 0.0);
    gtk_label_set_line_wrap(g_preview_sample_md, TRUE);
    gtk_box_pack_start(GTK_BOX(preview_content), GTK_WIDGET(g_preview_sample_md), FALSE, FALSE, 2);

    g_preview_sample_lg = GTK_LABEL(gtk_label_new(PREVIEW_TEXT));
    gtk_label_set_xalign(g_preview_sample_lg, 0.0);
    gtk_label_set_line_wrap(g_preview_sample_lg, TRUE);
    gtk_box_pack_start(GTK_BOX(preview_content), GTK_WIDGET(g_preview_sample_lg), FALSE, FALSE, 2);

    g_preview_sample_xl = GTK_LABEL(gtk_label_new(PREVIEW_TEXT));
    gtk_label_set_xalign(g_preview_sample_xl, 0.0);
    gtk_label_set_line_wrap(g_preview_sample_xl, TRUE);
    gtk_box_pack_start(GTK_BOX(preview_content), GTK_WIDGET(g_preview_sample_xl), FALSE, FALSE, 2);

    /* Separator */
    gtk_box_pack_start(GTK_BOX(preview_content),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 5);

    /* Section: Style Variants */
    GtkWidget *style_hdr = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(style_hdr), "<span weight='bold' alpha='70%%'>Style Variants</span>");
    gtk_label_set_xalign(style_hdr, 0.0);
    gtk_widget_set_margin_bottom(style_hdr, 4);
    gtk_box_pack_start(GTK_BOX(preview_content), style_hdr, FALSE, FALSE, 0);

    /* Bold */
    GtkWidget *bold_lbl = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(bold_lbl), "<span alpha='50%%'>Bold</span>");
    gtk_label_set_xalign(bold_lbl, 0.0);
    gtk_box_pack_start(GTK_BOX(preview_content), bold_lbl, FALSE, FALSE, 0);
    g_preview_bold = GTK_LABEL(gtk_label_new(PREVIEW_TEXT));
    gtk_label_set_xalign(g_preview_bold, 0.0);
    gtk_label_set_line_wrap(g_preview_bold, TRUE);
    gtk_box_pack_start(GTK_BOX(preview_content), GTK_WIDGET(g_preview_bold), FALSE, FALSE, 2);

    /* Italic */
    GtkWidget *italic_lbl = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(italic_lbl), "<span alpha='50%%'>Italic</span>");
    gtk_label_set_xalign(italic_lbl, 0.0);
    gtk_box_pack_start(GTK_BOX(preview_content), italic_lbl, FALSE, FALSE, 0);
    g_preview_italic = GTK_LABEL(gtk_label_new(PREVIEW_TEXT));
    gtk_label_set_xalign(g_preview_italic, 0.0);
    gtk_label_set_line_wrap(g_preview_italic, TRUE);
    gtk_box_pack_start(GTK_BOX(preview_content), GTK_WIDGET(g_preview_italic), FALSE, FALSE, 2);

    /* Bold Italic */
    GtkWidget *bi_lbl = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(bi_lbl), "<span alpha='50%%'>Bold Italic</span>");
    gtk_label_set_xalign(bi_lbl, 0.0);
    gtk_box_pack_start(GTK_BOX(preview_content), bi_lbl, FALSE, FALSE, 0);
    g_preview_bold_italic = GTK_LABEL(gtk_label_new(PREVIEW_TEXT));
    gtk_label_set_xalign(g_preview_bold_italic, 0.0);
    gtk_label_set_line_wrap(g_preview_bold_italic, TRUE);
    gtk_box_pack_start(GTK_BOX(preview_content), GTK_WIDGET(g_preview_bold_italic), FALSE, FALSE, 2);

    /* Separator */
    gtk_box_pack_start(GTK_BOX(preview_content),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 5);

    /* Section: Character Set */
    GtkWidget *char_hdr = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(char_hdr), "<span weight='bold' alpha='70%%'>Character Set</span>");
    gtk_label_set_xalign(char_hdr, 0.0);
    gtk_widget_set_margin_bottom(char_hdr, 4);
    gtk_box_pack_start(GTK_BOX(preview_content), char_hdr, FALSE, FALSE, 0);

    g_preview_mono = GTK_LABEL(gtk_label_new(PREVIEW_TEXT_MONO));
    gtk_label_set_xalign(g_preview_mono, 0.0);
    gtk_label_set_line_wrap(g_preview_mono, TRUE);
    gtk_box_pack_start(GTK_BOX(preview_content), GTK_WIDGET(g_preview_mono), FALSE, FALSE, 2);

    gtk_box_pack_start(GTK_BOX(g_preview_box), preview_content, FALSE, FALSE, 0);

    /* Spacer to push content to top */
    GtkWidget *preview_spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(preview_spacer, TRUE);
    gtk_box_pack_start(GTK_BOX(g_preview_box), preview_spacer, TRUE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(sys_right_scroll), g_preview_box);
    gtk_paned_pack2(GTK_PANED(sys_paned), sys_right_scroll, TRUE, TRUE);

    gtk_stack_add_titled(GTK_STACK(stack), sys_paned, "system_fonts", "System Fonts");

    /* ============================================================
     * Tab 2: Online Installer
     * ============================================================ */
    GtkWidget *online_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(online_box), 10);

    GtkWidget *online_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(online_title),
        "<span size='large' weight='bold'>Online Fonts Installer</span>");
    gtk_widget_set_halign(online_title, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(online_box), online_title, FALSE, FALSE, 5);

    GtkWidget *online_desc = gtk_label_new(
        "Browse and install fonts from online sources. "
        "Installed fonts are tracked as OCWS dotfiles and can be managed from the 'Managed' tab.");
    gtk_label_set_line_wrap(GTK_LABEL(online_desc), TRUE);
    gtk_label_set_xalign(GTK_LABEL(online_desc), 0.0);
    gtk_style_context_add_class(gtk_widget_get_style_context(online_desc), "dim-label");
    gtk_box_pack_start(GTK_BOX(online_box), online_desc, FALSE, FALSE, 5);

    /* Scrolled list of packages */
    GtkWidget *pkg_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(pkg_scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(online_box), pkg_scroll, TRUE, TRUE, 0);

    GtkWidget *pkg_list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_container_add(GTK_CONTAINER(pkg_scroll), pkg_list);

    /* Group by category */
    const char *last_category = "";
    for (int i = 0; i < OCWS_FONT_PACKAGE_COUNT; i++) {
        FontPackage *fpkg = &g_packages[i];
        const ocws_font_pkg_t *pkg = fpkg->pkg;

        /* Check if already installed */
        fpkg->installed = 0;
        fpkg->managed = is_font_managed(pkg->name);
        if (fpkg->managed) {
            fpkg->installed = 1;
        } else {
            fpkg->installed = ocws_font_pkg_is_installed(pkg);
        }

        /* Category header */
        if (strcmp(pkg->category, last_category) != 0) {
            last_category = pkg->category;
            GtkWidget *cat_lbl = gtk_label_new(NULL);
            char *cat_markup = g_strdup_printf("<span weight='bold' alpha='70%%'>%s</span>", pkg->category);
            gtk_label_set_markup(GTK_LABEL(cat_lbl), cat_markup);
            g_free(cat_markup);
            gtk_label_set_xalign(GTK_LABEL(cat_lbl), 0.0);
            gtk_widget_set_margin_top(cat_lbl, 10);
            gtk_widget_set_margin_start(cat_lbl, 4);
            gtk_box_pack_start(GTK_BOX(pkg_list), cat_lbl, FALSE, FALSE, 0);
        }

        /* Package row */
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        GtkStyleContext *rctx = gtk_widget_get_style_context(row);
        gtk_style_context_add_class(rctx, "pkg-card");
        if (fpkg->installed) {
            gtk_style_context_add_class(rctx, "pkg-installed");
        } else {
            gtk_style_context_add_class(rctx, "pkg-available");
        }

        /* Status icon */
        GtkWidget *icon_lbl = gtk_label_new(NULL);
        const char *icon = fpkg->managed ? "★" : (fpkg->installed ? "✓" : "○");
        const char *color = fpkg->managed ? OCWS_ACCENT() : (fpkg->installed ? OCWS_OK() : OCWS_MUTED());
        char *icon_markup = g_strdup_printf("<span foreground='%s' weight='bold'>%s</span>", color, icon);
        gtk_label_set_markup(GTK_LABEL(icon_lbl), icon_markup);
        g_free(icon_markup);
        gtk_widget_set_size_request(icon_lbl, 20, -1);
        gtk_box_pack_start(GTK_BOX(row), icon_lbl, FALSE, FALSE, 0);

        /* Name */
        GtkWidget *name_lbl = gtk_label_new(NULL);
        char *name_markup = g_strdup_printf("<b>%s</b>", pkg->name);
        gtk_label_set_markup(GTK_LABEL(name_lbl), name_markup);
        g_free(name_markup);
        gtk_widget_set_size_request(name_lbl, 140, -1);
        gtk_label_set_xalign(GTK_LABEL(name_lbl), 0.0);
        gtk_box_pack_start(GTK_BOX(row), name_lbl, FALSE, FALSE, 0);

        /* Description */
        GtkWidget *desc_lbl = gtk_label_new(pkg->desc);
        gtk_style_context_add_class(gtk_widget_get_style_context(desc_lbl), "dim-label");
        gtk_label_set_xalign(GTK_LABEL(desc_lbl), 0.0);
        gtk_box_pack_start(GTK_BOX(row), desc_lbl, TRUE, TRUE, 0);

        /* Status text */
        GtkWidget *status_lbl = gtk_label_new(fpkg->managed ? "OCWS-managed" : (fpkg->installed ? "Installed" : "Available"));
        gtk_style_context_add_class(gtk_widget_get_style_context(status_lbl), "dim-label");
        gtk_widget_set_size_request(status_lbl, 90, -1);
        gtk_box_pack_start(GTK_BOX(row), status_lbl, FALSE, FALSE, 0);

        /* Install button */
        GtkWidget *install_btn = gtk_button_new_with_label(fpkg->installed ? "Reinstall" : "Install");
        GtkStyleContext *btn_ctx = gtk_widget_get_style_context(install_btn);
        if (!fpkg->installed) {
            gtk_style_context_add_class(btn_ctx, "suggested-action");
        }

        InstallRowData *row_data = g_new0(InstallRowData, 1);
        row_data->pkg_index = i;
        row_data->btn = install_btn;
        row_data->status_lbl = status_lbl;
        g_signal_connect(install_btn, "clicked", G_CALLBACK(on_install_clicked), row_data);
        gtk_box_pack_start(GTK_BOX(row), install_btn, FALSE, FALSE, 0);

        gtk_box_pack_start(GTK_BOX(pkg_list), row, FALSE, FALSE, 0);
    }

    gtk_stack_add_titled(GTK_STACK(stack), online_box, "online_installer", "Online Installer");

    /* ============================================================
     * Tab 3: Managed Fonts
     * ============================================================ */
    GtkWidget *managed_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(managed_box), 10);

    GtkWidget *managed_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_bottom(managed_header, 5);

    GtkWidget *managed_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(managed_title),
        "<span size='large' weight='bold'>OCWS Managed Fonts</span>");
    gtk_widget_set_halign(managed_title, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(managed_header), managed_title, TRUE, TRUE, 0);

    GtkWidget *remove_btn = gtk_button_new_with_label("Remove Selected");
    GtkStyleContext *rm_ctx = gtk_widget_get_style_context(remove_btn);
    gtk_style_context_add_class(rm_ctx, "destructive-action");
    gtk_box_pack_start(GTK_BOX(managed_header), remove_btn, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(managed_box), managed_header, FALSE, FALSE, 0);

    GtkWidget *managed_desc = gtk_label_new(
        "Fonts installed by OCWS are tracked here. "
        "Removing a font will delete it from the system and untrack it.");
    gtk_label_set_line_wrap(GTK_LABEL(managed_desc), TRUE);
    gtk_label_set_xalign(GTK_LABEL(managed_desc), 0.0);
    gtk_style_context_add_class(gtk_widget_get_style_context(managed_desc), "dim-label");
    gtk_box_pack_start(GTK_BOX(managed_box), managed_desc, FALSE, FALSE, 5);

    /* Managed fonts list */
    g_managed_store = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

    GtkWidget *managed_tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(g_managed_store));
    const char *m_titles[] = {"Package", "Source URL", "Installed"};
    int m_widths[] = {160, 400, 140};
    for (int i = 0; i < 3; i++) {
        GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
        GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(m_titles[i], renderer, "text", i, NULL);
        gtk_tree_view_column_set_resizable(column, TRUE);
        gtk_tree_view_column_set_min_width(column, m_widths[i]);
        gtk_tree_view_append_column(GTK_TREE_VIEW(managed_tree), column);
    }

    GtkWidget *managed_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(managed_scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(managed_scroll), managed_tree);
    gtk_box_pack_start(GTK_BOX(managed_box), managed_scroll, TRUE, TRUE, 0);

    g_signal_connect(remove_btn, "clicked", G_CALLBACK(on_remove_managed), managed_tree);

    gtk_stack_add_titled(GTK_STACK(stack), managed_box, "managed_fonts", "Managed Fonts");

    /* ============================================================
     * Tab 4: Font Config
     * ============================================================ */
    GtkWidget *config_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(config_box), 10);

    GtkWidget *config_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(config_title),
        "<span size='large' weight='bold'>Font Configuration</span>");
    gtk_widget_set_halign(config_title, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(config_box), config_title, FALSE, FALSE, 5);

    /* Font Scale section */
    GtkWidget *scale_frame = gtk_frame_new("Font Scaling");
    gtk_widget_set_margin_top(scale_frame, 10);
    GtkWidget *scale_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(scale_box), 10);
    gtk_container_add(GTK_CONTAINER(scale_frame), scale_box);

    GtkWidget *scale_desc = gtk_label_new(
        "Adjust font size globally across all surfaces (GTK3/4, labwc, sfwbar, Qt). "
        "Changes are applied immediately to running applications.");
    gtk_label_set_line_wrap(GTK_LABEL(scale_desc), TRUE);
    gtk_label_set_xalign(GTK_LABEL(scale_desc), 0.0);
    gtk_box_pack_start(GTK_BOX(scale_box), scale_desc, FALSE, FALSE, 0);

    GtkWidget *scale_btns = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(scale_btns, GTK_ALIGN_START);

    GtkWidget *scale_down_btn = gtk_button_new_with_label("A- Decrease");
    g_signal_connect(scale_down_btn, "clicked", G_CALLBACK(on_font_scale_down), NULL);
    gtk_box_pack_start(GTK_BOX(scale_btns), scale_down_btn, FALSE, FALSE, 0);

    GtkWidget *scale_up_btn = gtk_button_new_with_label("A+ Increase");
    g_signal_connect(scale_up_btn, "clicked", G_CALLBACK(on_font_scale_up), NULL);
    gtk_box_pack_start(GTK_BOX(scale_btns), scale_up_btn, FALSE, FALSE, 0);

    GtkWidget *scale_status_btn = gtk_button_new_with_label("Show Status");
    g_signal_connect(scale_status_btn, "clicked", G_CALLBACK(on_font_scale_status), NULL);
    gtk_box_pack_start(GTK_BOX(scale_btns), scale_status_btn, FALSE, FALSE, 0);

    GtkWidget *scale_reset_btn = gtk_button_new_with_label("Reset");
    g_signal_connect(scale_reset_btn, "clicked", G_CALLBACK(on_font_scale_reset), NULL);
    gtk_box_pack_start(GTK_BOX(scale_btns), scale_reset_btn, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(scale_box), scale_btns, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(config_box), scale_frame, FALSE, FALSE, 0);

    /* Fontconfig section */
    GtkWidget *fc_frame = gtk_frame_new("Fontconfig");
    GtkWidget *fc_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(fc_box), 10);
    gtk_container_add(GTK_CONTAINER(fc_frame), fc_box);

    GtkWidget *fc_desc = gtk_label_new(
        "Font configuration file controls font substitution, rendering hints, "
        "and anti-aliasing. The OCWS dotfiles include a tuned fontconfig for "
        "Noto Sans, Inter, and Nerd Fonts.");
    gtk_label_set_line_wrap(GTK_LABEL(fc_desc), TRUE);
    gtk_label_set_xalign(GTK_LABEL(fc_desc), 0.0);
    gtk_box_pack_start(GTK_BOX(fc_box), fc_desc, FALSE, FALSE, 0);

    /* Show fontconfig status */
    char fc_path[512];
    const char *home = getenv("HOME");
    snprintf(fc_path, sizeof(fc_path), "%s/.config/fontconfig/fonts.conf", home ? home : "/tmp");

    GtkWidget *fc_status = gtk_label_new(NULL);
    char *fc_markup;
    if (file_exists(fc_path)) {
        fc_markup = g_strdup_printf("<span foreground='%s'>✓ fontconfig present at: %s</span>", OCWS_OK(), fc_path);
    } else {
        fc_markup = g_strdup_printf("<span foreground='%s'>✗ fontconfig not found at: %s</span>", OCWS_URGENT(), fc_path);
    }
    gtk_label_set_markup(GTK_LABEL(fc_status), fc_markup);
    g_free(fc_markup);
    gtk_label_set_line_wrap(GTK_LABEL(fc_status), TRUE);
    gtk_label_set_xalign(GTK_LABEL(fc_status), 0.0);
    gtk_box_pack_start(GTK_BOX(fc_box), fc_status, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(config_box), fc_frame, FALSE, FALSE, 0);

    /* Dotfiles info */
    GtkWidget *df_frame = gtk_frame_new("Dotfiles Integration");
    GtkWidget *df_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(df_box), 10);
    gtk_container_add(GTK_CONTAINER(df_frame), df_box);

    GtkWidget *df_desc = gtk_label_new(
        "OCWS manages fonts as part of its dotfiles system:\n\n"
        "• User fonts: ~/.local/share/fonts/\n"
        "• Managed metadata: ~/.local/share/fonts/ocws-managed/\n"
        "• Fontconfig: ~/.config/fontconfig/fonts.conf\n"
        "• Cursor themes: ~/.local/share/icons/\n\n"
        "Run 'install-fonts.sh' from the dotfiles root to install all base fonts.\n"
        "Run 'install-fonts-cursors.sh' to install Nerd Fonts and cursor themes.");
    gtk_label_set_line_wrap(GTK_LABEL(df_desc), TRUE);
    gtk_label_set_xalign(GTK_LABEL(df_desc), 0.0);
    gtk_box_pack_start(GTK_BOX(df_box), df_desc, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(config_box), df_frame, FALSE, FALSE, 0);

    /* Spacer */
    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(config_box), spacer, TRUE, TRUE, 0);

    gtk_stack_add_titled(GTK_STACK(stack), config_box, "font_config", "Font Config");

    /* ============================================================
     * Tab 5: Output Log
     * ============================================================ */
    GtkWidget *log_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_set_border_width(GTK_CONTAINER(log_box), 10);

    GtkWidget *log_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_bottom(log_header, 5);

    GtkWidget *log_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(log_title),
        "<span size='large' weight='bold'>Output Log</span>");
    gtk_widget_set_halign(log_title, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(log_header), log_title, TRUE, TRUE, 0);

    GtkWidget *clear_btn = gtk_button_new_with_label("Clear");
    g_signal_connect(clear_btn, "clicked", G_CALLBACK(on_rebuild_cache), NULL);
    gtk_box_pack_start(GTK_BOX(log_header), clear_btn, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(log_box), log_header, FALSE, FALSE, 0);

    GtkWidget *log_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(log_scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(log_box), log_scroll, TRUE, TRUE, 0);

    g_log_buffer = gtk_text_buffer_new(NULL);
    g_log_view = gtk_text_view_new_with_buffer(g_log_buffer);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(g_log_view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(g_log_view), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(g_log_view), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(g_log_view), 8);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(g_log_view), 8);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(g_log_view), 8);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(g_log_view), 8);
    gtk_container_add(GTK_CONTAINER(log_scroll), g_log_view);

    gtk_stack_add_titled(GTK_STACK(stack), log_box, "output_log", "Output Log");

    /* ============================================================
     * Initialize data
     * ============================================================ */
    log_msg("OCWS Fonts Manager v2.0.0");
    log_msg("Type: %s", APP_ID);
    log_msg("");

    /* Scan system fonts in background */
    scan_system_fonts();
    populate_font_list();

    /* Load managed fonts */
    load_managed_list(g_managed_store);

    log_msg("Ready. Browse 'Online Installer' to add fonts, or 'System Fonts' to view what's installed.");

    gtk_widget_show_all(window);
}

/* ================================================================
 * Entry point
 * ================================================================ */

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new(APP_ID, G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
