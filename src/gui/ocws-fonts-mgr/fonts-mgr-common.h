/*
 * fonts-mgr-common.h — Shared types, globals, and declarations
 * for the OCWS Fonts Manager GUI.
 */

#ifndef FONTS_MGR_COMMON_H
#define FONTS_MGR_COMMON_H

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

#define APP_ID "org.ocws.fontsmgr"
#define FONTS_MGR_VERSION "2.0.0"

/* ============================================================
 * Font Packages — runtime wrapper around shared library packages
 * ============================================================ */

typedef struct {
    const ocws_font_pkg_t *pkg;
    int installed;
    int managed;
} FontPackage;

/* ============================================================
 * System Font Entry
 * ============================================================ */

typedef struct {
    char *family;
    char *style;
    char *file;
    int is_managed;
} SystemFont;

/* ============================================================
 * Async UI Helper Types
 * ============================================================ */

typedef struct {
    int pkg_index;
    GtkWidget *btn;
    GtkWidget *status_lbl;
} InstallRowData;

typedef struct {
    GtkWidget *label;
    const char *text;
} LabelUpdate;

typedef struct {
    GtkWidget *widget;
    gboolean sensitive;
} SensitivityUpdate;

typedef struct {
    char pkg_name[256];
    char pkg_url[512];
} RemoveRowData;

/* ============================================================
 * Global Variables (defined in fonts-mgr-common.c)
 * ============================================================ */

extern FontPackage *g_packages;
extern SystemFont *g_system_fonts;
extern int g_system_font_count;
extern int g_system_font_capacity;

extern GtkListStore *g_font_list_store;
extern GtkTreeModelFilter *g_font_filter;
extern GtkSearchEntry *g_search_entry;
extern GtkTreeView *g_treeview;
extern GtkTextBuffer *g_log_buffer;
extern GtkWidget *g_log_view;
extern GtkLabel *g_stats_label;
extern GtkListStore *g_managed_store;

/* Preview panel */
extern GtkWidget *g_preview_box;
extern GtkLabel *g_preview_family;
extern GtkLabel *g_preview_style;
extern GtkLabel *g_preview_file;
extern GtkLabel *g_preview_sample_sm;
extern GtkLabel *g_preview_sample_md;
extern GtkLabel *g_preview_sample_lg;
extern GtkLabel *g_preview_sample_xl;
extern GtkLabel *g_preview_bold;
extern GtkLabel *g_preview_italic;
extern GtkLabel *g_preview_bold_italic;
extern GtkLabel *g_preview_mono;
extern GtkLabel *g_preview_empty;
extern GtkEntry *g_preview_text_entry;

/* Paths */
extern const char *FONTS_DIR;
extern const char *MANAGED_DIR;
extern const char *CURSORS_DIR;

/* ============================================================
 * Helper Functions (fonts-mgr-common.c)
 * ============================================================ */

void fonts_mgr_init_paths(void);
void fonts_mgr_log_msg(const char *fmt, ...);
void fonts_mgr_run_cmd_logged(const char *cmd);
int fonts_mgr_dir_exists(const char *path);
int fonts_mgr_file_exists(const char *path);
void fonts_mgr_make_dir_p(const char *path);
void fonts_mgr_set_label_async(GtkWidget *label, const char *text);
void fonts_mgr_set_sensitive_async(GtkWidget *widget, gboolean sensitive);

/* ============================================================
 * Font Operations (fonts-mgr-fonts.c)
 * ============================================================ */

void fonts_mgr_free_system_fonts(void);
void fonts_mgr_scan_system_fonts(void);
void fonts_mgr_populate_font_list(void);
void fonts_mgr_mark_font_managed(const char *pkg_name, const char *url);
int fonts_mgr_is_font_managed(const char *pkg_name);
void fonts_mgr_load_managed_list(GtkListStore *store);
gboolean fonts_mgr_font_filter_func(GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data);
void fonts_mgr_on_search_changed(GtkSearchEntry *entry, gpointer user_data);

/* ============================================================
 * Font Preview (fonts-mgr-preview.c)
 * ============================================================ */

extern const char *PREVIEW_TEXT;
extern const char *PREVIEW_TEXT_MONO;

void fonts_mgr_set_label_font(GtkLabel *label, const char *family, int size, int bold, int italic);
void fonts_mgr_update_font_preview(const char *family, const char *style, const char *file);
void fonts_mgr_on_font_row_activated(GtkTreeView *tree_view, GtkTreePath *path,
                                     GtkTreeViewColumn *column, gpointer user_data);
void fonts_mgr_on_font_selection_changed(GtkTreeSelection *selection, gpointer user_data);
void fonts_mgr_on_preview_text_changed(GtkEntry *entry, gpointer user_data);
void fonts_mgr_on_preview_size_changed(GtkSpinButton *spin, gpointer user_data);

/* ============================================================
 * Installer Workers (fonts-mgr-installer.c)
 * ============================================================ */

gpointer fonts_mgr_install_worker(gpointer user_data);
void fonts_mgr_on_install_clicked(GtkWidget *widget, gpointer data);
gpointer fonts_mgr_remove_worker(gpointer user_data);
void fonts_mgr_on_remove_managed(GtkWidget *widget, gpointer tree_view);
void fonts_mgr_on_font_scale_up(GtkWidget *widget, gpointer data);
void fonts_mgr_on_font_scale_down(GtkWidget *widget, gpointer data);
void fonts_mgr_on_font_scale_status(GtkWidget *widget, gpointer data);
void fonts_mgr_on_font_scale_reset(GtkWidget *widget, gpointer data);
gpointer fonts_mgr_rebuild_cache_worker(gpointer user_data);
void fonts_mgr_on_rebuild_cache(GtkWidget *widget, gpointer data);

/* ============================================================
 * Tab Builders (fonts-mgr-tabs.h)
 * ============================================================ */

GtkWidget* fonts_mgr_build_system_fonts_tab(void);
GtkWidget* fonts_mgr_build_online_installer_tab(void);
GtkWidget* fonts_mgr_build_managed_fonts_tab(void);
GtkWidget* fonts_mgr_build_font_config_tab(void);
GtkWidget* fonts_mgr_build_output_log_tab(void);

#endif /* FONTS_MGR_COMMON_H */
