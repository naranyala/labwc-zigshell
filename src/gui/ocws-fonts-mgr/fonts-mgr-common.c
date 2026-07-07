/*
 * fonts-mgr-common.c — Helpers, paths, logging, async UI, CSS
 */

#include "fonts-mgr-common.h"
#include <stdarg.h>

/* ============================================================
 * Global Variables
 * ============================================================ */

FontPackage *g_packages = NULL;
SystemFont *g_system_fonts = NULL;
int g_system_font_count = 0;
int g_system_font_capacity = 0;

GtkListStore *g_font_list_store = NULL;
GtkTreeModelFilter *g_font_filter = NULL;
GtkSearchEntry *g_search_entry = NULL;
GtkTreeView *g_treeview = NULL;
GtkTextBuffer *g_log_buffer = NULL;
GtkWidget *g_log_view = NULL;
GtkLabel *g_stats_label = NULL;
GtkListStore *g_managed_store = NULL;

GtkWidget *g_preview_box = NULL;
GtkLabel *g_preview_family = NULL;
GtkLabel *g_preview_style = NULL;
GtkLabel *g_preview_file = NULL;
GtkLabel *g_preview_sample_sm = NULL;
GtkLabel *g_preview_sample_md = NULL;
GtkLabel *g_preview_sample_lg = NULL;
GtkLabel *g_preview_sample_xl = NULL;
GtkLabel *g_preview_bold = NULL;
GtkLabel *g_preview_italic = NULL;
GtkLabel *g_preview_bold_italic = NULL;
GtkLabel *g_preview_mono = NULL;
GtkLabel *g_preview_empty = NULL;
GtkEntry *g_preview_text_entry = NULL;

const char *FONTS_DIR = NULL;
const char *MANAGED_DIR = NULL;
const char *CURSORS_DIR = NULL;

/* ============================================================
 * Paths
 * ============================================================ */

void fonts_mgr_init_paths(void) {
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

/* ============================================================
 * Logging
 * ============================================================ */

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

void fonts_mgr_log_msg(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    char *msg = g_strdup(buf);
    g_idle_add(append_log_idle, msg);
}

void fonts_mgr_run_cmd_logged(const char *cmd) {
    fonts_mgr_log_msg("$ %s", cmd);
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        fonts_mgr_log_msg("ERROR: Failed to execute command");
        return;
    }
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0;
        fonts_mgr_log_msg("  %s", line);
    }
    int ret = pclose(fp);
    if (WIFEXITED(ret) && WEXITSTATUS(ret) != 0) {
        fonts_mgr_log_msg("Exit code: %d", WEXITSTATUS(ret));
    }
}

/* ============================================================
 * File Helpers
 * ============================================================ */

int fonts_mgr_dir_exists(const char *path) { return ocws_fonts_dir_exists(path); }
int fonts_mgr_file_exists(const char *path) { return ocws_fonts_file_exists(path); }
void fonts_mgr_make_dir_p(const char *path) { ocws_fonts_make_dir_p(path); }

/* ============================================================
 * Async UI Helpers
 * ============================================================ */

static gboolean set_label_text_idle(gpointer data) {
    LabelUpdate *u = (LabelUpdate *)data;
    gtk_label_set_text(GTK_LABEL(u->label), u->text);
    g_free(u);
    return G_SOURCE_REMOVE;
}

void fonts_mgr_set_label_async(GtkWidget *label, const char *text) {
    LabelUpdate *u = g_new0(LabelUpdate, 1);
    u->label = label;
    u->text = g_strdup(text);
    g_idle_add(set_label_text_idle, u);
}

static gboolean set_sensitivity_idle(gpointer data) {
    SensitivityUpdate *u = (SensitivityUpdate *)data;
    gtk_widget_set_sensitive(u->widget, u->sensitive);
    g_free(u);
    return G_SOURCE_REMOVE;
}

void fonts_mgr_set_sensitive_async(GtkWidget *widget, gboolean sensitive) {
    SensitivityUpdate *u = g_new0(SensitivityUpdate, 1);
    u->widget = widget;
    u->sensitive = sensitive;
    g_idle_add(set_sensitivity_idle, u);
}


