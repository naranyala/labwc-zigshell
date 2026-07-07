/*
 * fonts-mgr-ui.c — Tab builders and application activate()
 */

#include "fonts-mgr-common.h"
#include "../../libocws/gtk.h"

/* ============================================================
 * Tab 1: System Fonts (split: tree left, preview right)
 * ============================================================ */

GtkWidget* fonts_mgr_build_system_fonts_tab(void) {
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
    g_signal_connect(refresh_btn, "clicked", G_CALLBACK(fonts_mgr_on_rebuild_cache), g_stats_label);
    gtk_box_pack_start(GTK_BOX(stats_box), refresh_btn, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(sys_left), stats_box, FALSE, FALSE, 0);

    /* Search */
    g_search_entry = GTK_SEARCH_ENTRY(gtk_search_entry_new());
    gtk_entry_set_placeholder_text(GTK_ENTRY(g_search_entry), "Search fonts by family, style, or path...");
    g_signal_connect(g_search_entry, "search-changed", G_CALLBACK(fonts_mgr_on_search_changed), NULL);
    gtk_box_pack_start(GTK_BOX(sys_left), GTK_WIDGET(g_search_entry), FALSE, FALSE, 5);

    /* Font list store: family, style, file, source */
    g_font_list_store = gtk_list_store_new(4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    g_font_filter = GTK_TREE_MODEL_FILTER(gtk_tree_model_filter_new(GTK_TREE_MODEL(g_font_list_store), NULL));
    gtk_tree_model_filter_set_visible_func(g_font_filter, fonts_mgr_font_filter_func, NULL, NULL);

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

    g_signal_connect(g_treeview, "row-activated", G_CALLBACK(fonts_mgr_on_font_row_activated), NULL);

    GtkTreeSelection *font_sel = gtk_tree_view_get_selection(g_treeview);
    g_signal_connect(font_sel, "changed", G_CALLBACK(fonts_mgr_on_font_selection_changed), NULL);

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
    g_signal_connect(g_preview_text_entry, "changed", G_CALLBACK(fonts_mgr_on_preview_text_changed), NULL);
    gtk_box_pack_start(GTK_BOX(text_row), GTK_WIDGET(g_preview_text_entry), TRUE, TRUE, 0);

    /* Base size spinner */
    GtkWidget *size_lbl = gtk_label_new("Base size:");
    gtk_style_context_add_class(gtk_widget_get_style_context(size_lbl), "dim-label");
    gtk_box_pack_start(GTK_BOX(text_row), size_lbl, FALSE, FALSE, 0);

    GtkAdjustment *size_adj = gtk_adjustment_new(14, 8, 48, 1, 2, 0);
    GtkWidget *size_spin = gtk_spin_button_new(size_adj, 1, 0);
    gtk_widget_set_size_request(size_spin, 50, -1);
    g_signal_connect(size_spin, "value-changed", G_CALLBACK(fonts_mgr_on_preview_size_changed), NULL);
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

    GtkWidget *bold_lbl = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(bold_lbl), "<span alpha='50%%'>Bold</span>");
    gtk_label_set_xalign(bold_lbl, 0.0);
    gtk_box_pack_start(GTK_BOX(preview_content), bold_lbl, FALSE, FALSE, 0);
    g_preview_bold = GTK_LABEL(gtk_label_new(PREVIEW_TEXT));
    gtk_label_set_xalign(g_preview_bold, 0.0);
    gtk_label_set_line_wrap(g_preview_bold, TRUE);
    gtk_box_pack_start(GTK_BOX(preview_content), GTK_WIDGET(g_preview_bold), FALSE, FALSE, 2);

    GtkWidget *italic_lbl = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(italic_lbl), "<span alpha='50%%'>Italic</span>");
    gtk_label_set_xalign(italic_lbl, 0.0);
    gtk_box_pack_start(GTK_BOX(preview_content), italic_lbl, FALSE, FALSE, 0);
    g_preview_italic = GTK_LABEL(gtk_label_new(PREVIEW_TEXT));
    gtk_label_set_xalign(g_preview_italic, 0.0);
    gtk_label_set_line_wrap(g_preview_italic, TRUE);
    gtk_box_pack_start(GTK_BOX(preview_content), GTK_WIDGET(g_preview_italic), FALSE, FALSE, 2);

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

    return sys_paned;
}

/* ============================================================
 * Tab 2: Online Installer
 * ============================================================ */

GtkWidget* fonts_mgr_build_online_installer_tab(void) {
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
        fpkg->managed = fonts_mgr_is_font_managed(pkg->name);
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
        const char *icon = fpkg->managed ? "*" : (fpkg->installed ? "+" : "o");
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
        g_signal_connect(install_btn, "clicked", G_CALLBACK(fonts_mgr_on_install_clicked), row_data);
        gtk_box_pack_start(GTK_BOX(row), install_btn, FALSE, FALSE, 0);

        gtk_box_pack_start(GTK_BOX(pkg_list), row, FALSE, FALSE, 0);
    }

    return online_box;
}

/* ============================================================
 * Tab 3: Managed Fonts
 * ============================================================ */

GtkWidget* fonts_mgr_build_managed_fonts_tab(void) {
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

    g_signal_connect(remove_btn, "clicked", G_CALLBACK(fonts_mgr_on_remove_managed), managed_tree);

    return managed_box;
}

/* ============================================================
 * Tab 4: Font Config
 * ============================================================ */

GtkWidget* fonts_mgr_build_font_config_tab(void) {
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
    g_signal_connect(scale_down_btn, "clicked", G_CALLBACK(fonts_mgr_on_font_scale_down), NULL);
    gtk_box_pack_start(GTK_BOX(scale_btns), scale_down_btn, FALSE, FALSE, 0);

    GtkWidget *scale_up_btn = gtk_button_new_with_label("A+ Increase");
    g_signal_connect(scale_up_btn, "clicked", G_CALLBACK(fonts_mgr_on_font_scale_up), NULL);
    gtk_box_pack_start(GTK_BOX(scale_btns), scale_up_btn, FALSE, FALSE, 0);

    GtkWidget *scale_status_btn = gtk_button_new_with_label("Show Status");
    g_signal_connect(scale_status_btn, "clicked", G_CALLBACK(fonts_mgr_on_font_scale_status), NULL);
    gtk_box_pack_start(GTK_BOX(scale_btns), scale_status_btn, FALSE, FALSE, 0);

    GtkWidget *scale_reset_btn = gtk_button_new_with_label("Reset");
    g_signal_connect(scale_reset_btn, "clicked", G_CALLBACK(fonts_mgr_on_font_scale_reset), NULL);
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
    if (fonts_mgr_file_exists(fc_path)) {
        fc_markup = g_strdup_printf("<span foreground='%s'>fontconfig present at: %s</span>", OCWS_OK(), fc_path);
    } else {
        fc_markup = g_strdup_printf("<span foreground='%s'>fontconfig not found at: %s</span>", OCWS_URGENT(), fc_path);
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
        "  User fonts: ~/.local/share/fonts/\n"
        "  Managed metadata: ~/.local/share/fonts/ocws-managed/\n"
        "  Fontconfig: ~/.config/fontconfig/fonts.conf\n"
        "  Cursor themes: ~/.local/share/icons/\n\n"
        "Run 'install-fonts.sh' from the dotfiles root to install all base fonts.\n"
        "Run 'install-fonts-cursors.sh' to install Nerd Fonts and cursor themes.");
    gtk_label_set_line_wrap(GTK_LABEL(df_desc), TRUE);
    gtk_label_set_xalign(GTK_LABEL(df_desc), 0.0);
    gtk_box_pack_start(GTK_BOX(df_box), df_desc, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(config_box), df_frame, FALSE, FALSE, 0);

    /* Spacer */
    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(config_box), spacer, TRUE, TRUE, 0);

    return config_box;
}

/* ============================================================
 * Tab 5: Output Log
 * ============================================================ */

GtkWidget* fonts_mgr_build_output_log_tab(void) {
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
    g_signal_connect(clear_btn, "clicked", G_CALLBACK(fonts_mgr_on_rebuild_cache), NULL);
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

    return log_box;
}

/* ============================================================
 * Application Activate
 * ============================================================ */

void fonts_mgr_activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;
    fonts_mgr_init_paths();
    ocws_gtk_enforce_premium_theme();
    ocws_gtk_apply_dynamic_css(app, NULL);

    /* Ensure managed dir exists */
    fonts_mgr_make_dir_p(MANAGED_DIR);

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

    /* Add tabs */
    GtkWidget *sys_fonts = fonts_mgr_build_system_fonts_tab();
    gtk_stack_add_titled(GTK_STACK(stack), sys_fonts, "system_fonts", "System Fonts");

    GtkWidget *online = fonts_mgr_build_online_installer_tab();
    gtk_stack_add_titled(GTK_STACK(stack), online, "online_installer", "Online Installer");

    GtkWidget *managed = fonts_mgr_build_managed_fonts_tab();
    gtk_stack_add_titled(GTK_STACK(stack), managed, "managed_fonts", "Managed Fonts");

    GtkWidget *config = fonts_mgr_build_font_config_tab();
    gtk_stack_add_titled(GTK_STACK(stack), config, "font_config", "Font Config");

    GtkWidget *log = fonts_mgr_build_output_log_tab();
    gtk_stack_add_titled(GTK_STACK(stack), log, "output_log", "Output Log");

    /* Initialize data */
    fonts_mgr_log_msg("OCWS Fonts Manager v%s", FONTS_MGR_VERSION);
    fonts_mgr_log_msg("Type: %s", APP_ID);
    fonts_mgr_log_msg("");

    fonts_mgr_scan_system_fonts();
    fonts_mgr_populate_font_list();
    fonts_mgr_load_managed_list(g_managed_store);

    fonts_mgr_log_msg("Ready. Browse 'Online Installer' to add fonts, or 'System Fonts' to view what's installed.");

    gtk_widget_show_all(window);
}
