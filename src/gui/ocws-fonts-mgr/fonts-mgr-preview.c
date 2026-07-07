/*
 * fonts-mgr-preview.c — Font preview panel logic
 */

#include "fonts-mgr-common.h"

/* ============================================================
 * Preview Constants
 * ============================================================ */

const char *PREVIEW_TEXT = "The quick brown fox jumps over the lazy dog";
const char *PREVIEW_TEXT_MONO = "AaBbCcDdEeFf 0123456789 !@#$%^&*()";

/* ============================================================
 * Font Rendering Helpers
 * ============================================================ */

void fonts_mgr_set_label_font(GtkLabel *label, const char *family, int size, int bold, int italic) {
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

/* ============================================================
 * Preview Update
 * ============================================================ */

void fonts_mgr_update_font_preview(const char *family, const char *style, const char *file) {
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
    fonts_mgr_set_label_font(g_preview_sample_sm, family, 10, 0, 0);

    gtk_label_set_text(g_preview_sample_md, text);
    fonts_mgr_set_label_font(g_preview_sample_md, family, 14, 0, 0);

    gtk_label_set_text(g_preview_sample_lg, text);
    fonts_mgr_set_label_font(g_preview_sample_lg, family, 20, 0, 0);

    gtk_label_set_text(g_preview_sample_xl, text);
    fonts_mgr_set_label_font(g_preview_sample_xl, family, 28, 0, 0);

    /* Style variants */
    gtk_label_set_text(g_preview_bold, text);
    fonts_mgr_set_label_font(g_preview_bold, family, 16, 1, 0);

    gtk_label_set_text(g_preview_italic, text);
    fonts_mgr_set_label_font(g_preview_italic, family, 16, 0, 1);

    gtk_label_set_text(g_preview_bold_italic, text);
    fonts_mgr_set_label_font(g_preview_bold_italic, family, 16, 1, 1);

    /* Monospace style preview */
    gtk_label_set_text(g_preview_mono, PREVIEW_TEXT_MONO);
    fonts_mgr_set_label_font(g_preview_mono, family, 13, 0, 0);
}

/* ============================================================
 * Selection Callbacks
 * ============================================================ */

void fonts_mgr_on_font_row_activated(GtkTreeView *tree_view, GtkTreePath *path,
                                     GtkTreeViewColumn *column, gpointer user_data) {
    (void)column; (void)user_data;
    GtkTreeModel *model = gtk_tree_view_get_model(tree_view);
    GtkTreeIter iter;

    if (!gtk_tree_model_get_iter(model, &iter, path)) return;

    gchar *family = NULL;
    gchar *style = NULL;
    gchar *file = NULL;
    gtk_tree_model_get(model, &iter, 0, &family, 1, &style, 2, &file, -1);

    fonts_mgr_update_font_preview(family, style, file);

    g_free(family);
    g_free(style);
    g_free(file);
}

void fonts_mgr_on_font_selection_changed(GtkTreeSelection *selection, gpointer user_data) {
    (void)user_data;
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        fonts_mgr_update_font_preview(NULL, NULL, NULL);
        return;
    }

    gchar *family = NULL;
    gchar *style = NULL;
    gchar *file = NULL;
    gtk_tree_model_get(model, &iter, 0, &family, 1, &style, 2, &file, -1);

    fonts_mgr_update_font_preview(family, style, file);

    g_free(family);
    g_free(style);
    g_free(file);
}

void fonts_mgr_on_preview_text_changed(GtkEntry *entry, gpointer user_data) {
    (void)user_data;
    GtkTreeSelection *sel = gtk_tree_view_get_selection(g_treeview);
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (!gtk_tree_selection_get_selected(sel, &model, &iter)) return;

    gchar *family = NULL;
    gchar *style = NULL;
    gchar *file = NULL;
    gtk_tree_model_get(model, &iter, 0, &family, 1, &style, 2, &file, -1);

    fonts_mgr_update_font_preview(family, style, file);

    g_free(family);
    g_free(style);
    g_free(file);
}

void fonts_mgr_on_preview_size_changed(GtkSpinButton *spin, gpointer user_data) {
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
        fonts_mgr_set_label_font(g_preview_sample_sm, family, size, 0, 0);

        gtk_label_set_text(g_preview_sample_md, text);
        fonts_mgr_set_label_font(g_preview_sample_md, family, (int)(size * 1.4), 0, 0);

        gtk_label_set_text(g_preview_sample_lg, text);
        fonts_mgr_set_label_font(g_preview_sample_lg, family, (int)(size * 2.0), 0, 0);

        gtk_label_set_text(g_preview_sample_xl, text);
        fonts_mgr_set_label_font(g_preview_sample_xl, family, (int)(size * 2.8), 0, 0);

        gtk_label_set_text(g_preview_bold, text);
        fonts_mgr_set_label_font(g_preview_bold, family, size, 1, 0);

        gtk_label_set_text(g_preview_italic, text);
        fonts_mgr_set_label_font(g_preview_italic, family, size, 0, 1);

        gtk_label_set_text(g_preview_bold_italic, text);
        fonts_mgr_set_label_font(g_preview_bold_italic, family, size, 1, 1);

        gtk_label_set_text(g_preview_mono, PREVIEW_TEXT_MONO);
        fonts_mgr_set_label_font(g_preview_mono, family, (int)(size * 0.85), 0, 0);
    }

    g_free(family);
    g_free(style);
    g_free(file);
}
