/*
 * settings-tabs.c — Tab Builder Functions
 * Implementation of all settings panel tabs.
 */

#include "settings-ui.h"
#include "settings-tabs.h"
#include "../../core/utils.h"
#include "../../core/ocws-theme-utils.h"
#include <stdlib.h>
#include <string.h>

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

// ============================================================
// Tab: Shell Modes
// ============================================================

GtkWidget* build_shell_tab(void) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 30);

    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), "<span size='x-large' weight='bold'>Select Shell Experience</span>");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), title, FALSE, FALSE, 0);

    GtkWidget *flowbox = gtk_flow_box_new();
    gtk_widget_set_valign(flowbox, GTK_ALIGN_START);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flowbox), 4);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flowbox), GTK_SELECTION_NONE);

    gtk_flow_box_insert(GTK_FLOW_BOX(flowbox), create_shell_card("OCWS Double Panel", "Dual-panel sfwbar — the default OCWS experience", "doublepanel", "preferences-desktop-symbolic"), -1);
    gtk_flow_box_insert(GTK_FLOW_BOX(flowbox), create_shell_card("Crystal Dock", "SFWBar + bottom macOS-style dock", "crystaldock", "computer-apple-symbolic"), -1);
    gtk_flow_box_insert(GTK_FLOW_BOX(flowbox), create_shell_card("DankMaterialShell", "Material Design 3 integrated shell", "dms", "view-app-grid-symbolic"), -1);
    gtk_flow_box_insert(GTK_FLOW_BOX(flowbox), create_shell_card("Noctalia", "Minimalist plugin-driven shell", "noctalia", "weather-clear-night-symbolic"), -1);

    gtk_box_pack_start(GTK_BOX(vbox), flowbox, TRUE, TRUE, 0);
    return vbox;
}

// ============================================================
// Tab: Appearance
// ============================================================

static void on_theme_color_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    const char *theme = (const char *)data;
    if (!theme || !is_shell_safe(theme)) return;
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "theme-engine.sh apply %s &", theme);
    system(cmd);
}

static void on_icon_apply_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    GtkWidget *combo = GTK_WIDGET(data);
    char *active = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo));
    if (active) {
        if (is_shell_safe(active)) {
            char cmd[256];
            snprintf(cmd, sizeof(cmd), "gsettings set org.gnome.desktop.interface icon-theme '%s' &", active);
            system(cmd);
        }
        g_free(active);
    }
}

static void on_cursor_apply_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    GtkWidget *combo = GTK_WIDGET(data);
    char *active = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo));
    if (active) {
        if (is_shell_safe(active)) {
            char cmd[256];
            snprintf(cmd, sizeof(cmd), "gsettings set org.gnome.desktop.interface cursor-theme '%s' &", active);
            system(cmd);
        }
        g_free(active);
    }
}

GtkWidget* build_appearance_tab(void) {
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 20);
    gtk_container_add(GTK_CONTAINER(scroll), vbox);

    // Theme Colors (always visible)
    GtkWidget *card = create_card("Theme Colors", "🎨");
    gtk_box_pack_start(GTK_BOX(vbox), card, FALSE, FALSE, 0);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_box_pack_start(GTK_BOX(card), grid, FALSE, FALSE, 0);

    for (int i = 0; i < OCWS_THEME_COUNT; i++) {
        GtkWidget *color_btn = gtk_button_new();
        GtkStyleContext *ctx = gtk_widget_get_style_context(color_btn);
        gtk_widget_set_size_request(color_btn, 32, 32);
        gtk_widget_set_tooltip_text(color_btn, OCWS_THEMES[i].slug);

        char css[256];
        snprintf(css, sizeof(css), "* { background-color: %s; border-radius: 16px; }", OCWS_THEMES[i].accent);
        GtkCssProvider *provider = gtk_css_provider_new();
        gtk_css_provider_load_from_data(provider, css, -1, NULL);
        gtk_style_context_add_provider(ctx, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref(provider);

        g_signal_connect(color_btn, "clicked", G_CALLBACK(on_theme_color_clicked), (gpointer)OCWS_THEMES[i].slug);
        gtk_grid_attach(GTK_GRID(grid), color_btn, i % 5, i / 5, 1, 1);
    }

    // Icon Theme (collapsible, starts collapsed)
    card = create_collapsible_card("Icon Theme", "📁", FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), card, FALSE, FALSE, 0);
    GtkWidget *content = get_collapsible_content(card);
    GtkWidget *icon_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *icon_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(icon_combo), "Papirus-Dark");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(icon_combo), "Papirus-Light");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(icon_combo), "Tela-dark");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(icon_combo), "Colloid-dark");
    gtk_combo_box_set_active(GTK_COMBO_BOX(icon_combo), 0);
    gtk_widget_set_hexpand(icon_combo, TRUE);
    GtkWidget *icon_apply = gtk_button_new_with_label("Apply");
    g_signal_connect(icon_apply, "clicked", G_CALLBACK(on_icon_apply_clicked), icon_combo);
    gtk_box_pack_start(GTK_BOX(icon_row), icon_combo, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(icon_row), icon_apply, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), icon_row, FALSE, FALSE, 0);

    // Cursor Theme (collapsible, starts collapsed)
    card = create_collapsible_card("Cursor Theme", "🖱️", FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), card, FALSE, FALSE, 0);
    content = get_collapsible_content(card);
    GtkWidget *cursor_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *cursor_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(cursor_combo), "Catppuccin-Mocha-Dark");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(cursor_combo), "Catppuccin-Mocha-Light");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(cursor_combo), "Bibata-Modern-Classic");
    gtk_combo_box_set_active(GTK_COMBO_BOX(cursor_combo), 0);
    gtk_widget_set_hexpand(cursor_combo, TRUE);
    GtkWidget *cursor_apply = gtk_button_new_with_label("Apply");
    g_signal_connect(cursor_apply, "clicked", G_CALLBACK(on_cursor_apply_clicked), cursor_combo);
    gtk_box_pack_start(GTK_BOX(cursor_row), cursor_combo, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(cursor_row), cursor_apply, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), cursor_row, FALSE, FALSE, 0);

    // Font Scaling (always visible)
    card = create_card("Font Scaling", "🔤");
    gtk_box_pack_start(GTK_BOX(vbox), card, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), create_live_slider_row("Font Size", 10, 6, 24, "pt", "font-scale.sh set %.1f &"), FALSE, FALSE, 0);

    // Compositor Effects (collapsible, starts collapsed)
    card = create_collapsible_card("Compositor Effects", "✨", FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), card, FALSE, FALSE, 0);
    content = get_collapsible_content(card);
    gtk_box_pack_start(GTK_BOX(content), create_live_slider_row("Window Gaps", 10, 0, 40, "px", "ocws-kv set theme window_gaps %d; labwc -r &"), FALSE, FALSE, 0);

    // Read current corner radius and margins from rc.xml
    int corner_radius = 8;
    int margin_top = 0, margin_bottom = 4, margin_left = 4, margin_right = 4;
    {
        const char *home = getenv("HOME");
        if (home && *home) {
            char rc_path[512];
            snprintf(rc_path, sizeof(rc_path), "%s/.config/labwc/rc.xml", home);
            FILE *f = fopen(rc_path, "r");
            if (f) {
                char line[256];
                while (fgets(line, sizeof(line), f)) {
                    char *p;
                    if ((p = strstr(line, "<cornerRadius>"))) {
                        corner_radius = atoi(p + strlen("<cornerRadius>"));
                    } else if ((p = strstr(line, "<margin "))) {
                        char *a;
                        if ((a = strstr(p, "top=\""))) margin_top = atoi(a + 5);
                        if ((a = strstr(p, "bottom=\""))) margin_bottom = atoi(a + 8);
                        if ((a = strstr(p, "left=\""))) margin_left = atoi(a + 6);
                        if ((a = strstr(p, "right=\""))) margin_right = atoi(a + 7);
                    }
                }
                fclose(f);
            }
        }
    }

    gtk_box_pack_start(GTK_BOX(content), create_live_slider_row("Corner Radius", corner_radius, 0, 30, "px",
        "sed -i 's|<cornerRadius>[^<]*</cornerRadius>|<cornerRadius>%d</cornerRadius>|' ~/.config/labwc/rc.xml; labwc -r &"), FALSE, FALSE, 0);

    // Window Margins
    GtkWidget *margin_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(margin_label), "<b>Window Margins</b>");
    gtk_label_set_xalign(GTK_LABEL(margin_label), 0.0);
    gtk_widget_set_margin_top(margin_label, 8);
    gtk_box_pack_start(GTK_BOX(content), margin_label, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(content), create_live_slider_row("Margin Top", margin_top, 0, 80, "px",
        "sed -i 's|<margin top=\"[^\"]*\"|<margin top=\"%d\"|' ~/.config/labwc/rc.xml; labwc -r &"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), create_live_slider_row("Margin Bottom", margin_bottom, 0, 80, "px",
        "sed -i 's|bottom=\"[^\"]*\"|bottom=\"%d\"|' ~/.config/labwc/rc.xml; labwc -r &"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), create_live_slider_row("Margin Left", margin_left, 0, 80, "px",
        "sed -i 's|left=\"[^\"]*\"|left=\"%d\"|' ~/.config/labwc/rc.xml; labwc -r &"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), create_live_slider_row("Margin Right", margin_right, 0, 80, "px",
        "sed -i 's|right=\"[^\"]*\"|right=\"%d\"|' ~/.config/labwc/rc.xml; labwc -r &"), FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(content), create_live_toggle_row("Window Blur", "Enable background blur for transparent windows", TRUE, "ocws-kv set theme window_blur 1; labwc -r &", "ocws-kv set theme window_blur 0; labwc -r &"), FALSE, FALSE, 0);

    // Theme Center button (always visible)
    GtkWidget *tc_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_top(tc_row, 8);
    GtkWidget *tc_lbl = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(tc_lbl),
        "<b>Advanced Theme Management</b>\n"
        "<span size='small'>Browse, preview, and apply themes across all config surfaces</span>");
    gtk_label_set_xalign(GTK_LABEL(tc_lbl), 0.0);
    gtk_box_pack_start(GTK_BOX(tc_row), tc_lbl, TRUE, TRUE, 0);
    GtkWidget *tc_btn = gtk_button_new_with_label("Open Theme Center");
    g_signal_connect(tc_btn, "clicked", G_CALLBACK(execute_command),
                     (gpointer)"ocws-theme-center &");
    gtk_box_pack_start(GTK_BOX(tc_row), tc_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), tc_row, FALSE, FALSE, 0);

    return scroll;
}

// ============================================================
// Tab: Bar Configuration
// ============================================================

GtkWidget* build_bar_config_tab(void) {
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 20);
    gtk_container_add(GTK_CONTAINER(scroll), vbox);

    // Position (always visible)
    GtkWidget *card = create_card("Bar Position", "📐");
    gtk_box_pack_start(GTK_BOX(vbox), card, FALSE, FALSE, 0);
    const char *positions[] = {"Top", "Bottom", "Left", "Right"};
    gtk_box_pack_start(GTK_BOX(card), create_button_group("Position", positions, 4, 0), FALSE, FALSE, 0);

    // Size & Spacing (collapsible, starts expanded)
    card = create_collapsible_card("Size & Spacing", "📏", TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), card, FALSE, FALSE, 0);
    GtkWidget *content = get_collapsible_content(card);
    gtk_box_pack_start(GTK_BOX(content), create_live_slider_row("Bar Thickness", 32, 24, 64, "px", "ocws-kv set bar.thickness %d &"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), create_live_slider_row("Edge Spacing", 4, 0, 32, "px", "ocws-kv set bar.margin %d &"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), create_live_slider_row("Widget Padding", 8, 0, 32, "px", "ocws-kv set bar.padding %d &"), FALSE, FALSE, 0);

    // Transparency (collapsible, starts collapsed)
    card = create_collapsible_card("Transparency", "👁️", FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), card, FALSE, FALSE, 0);
    content = get_collapsible_content(card);
    gtk_box_pack_start(GTK_BOX(content), create_live_slider_row("Bar Opacity", 100, 0, 100, "%", "ocws-kv set bar.opacity %d &"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), create_live_slider_row("Widget Opacity", 100, 0, 100, "%", "ocws-kv set bar.widget_opacity %d &"), FALSE, FALSE, 0);

    // Corners (collapsible, starts collapsed)
    card = create_collapsible_card("Corners & Background", "🔲", FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), card, FALSE, FALSE, 0);
    content = get_collapsible_content(card);
    gtk_box_pack_start(GTK_BOX(content), create_live_slider_row("Corner Radius", 12, 0, 24, "px", "ocws-kv set bar.radius %d &"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), create_live_toggle_row("Square Corners", "Remove rounded corners", FALSE, "ocws-kv set bar.square 1 &", "ocws-kv set bar.square 0 &"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), create_live_toggle_row("No Background", "Transparent bar background", FALSE, "ocws-kv set bar.nobg 1 &", "ocws-kv set bar.nobg 0 &"), FALSE, FALSE, 0);

    // Visibility (collapsible, starts collapsed)
    card = create_collapsible_card("Visibility", "👁️", FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), card, FALSE, FALSE, 0);
    content = get_collapsible_content(card);
    gtk_box_pack_start(GTK_BOX(content), create_live_toggle_row("Auto-hide", "Hide when not hovering", FALSE, "ocws-kv set bar.autohide 1 &", "ocws-kv set bar.autohide 0 &"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), create_live_slider_row("Hide Delay", 250, 0, 2000, "ms", "ocws-kv set bar.hide_delay %d &"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), create_live_toggle_row("Scroll Switching", "Switch workspace with scroll", TRUE, "ocws-kv set bar.scroll_switch 1 &", "ocws-kv set bar.scroll_switch 0 &"), FALSE, FALSE, 0);

    return scroll;
}

// ============================================================
// Tab: Widgets
// ============================================================

GtkWidget* build_widgets_tab(void) {
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 20);
    gtk_container_add(GTK_CONTAINER(scroll), vbox);

    // Presets
    GtkWidget *card = create_card("Widget Presets", "⚡");
    gtk_box_pack_start(GTK_BOX(vbox), card, FALSE, FALSE, 0);
    const char *presets[] = {"Standard", "Full", "Minimal", "Custom"};
    gtk_box_pack_start(GTK_BOX(card), create_button_group("Preset", presets, 4, 1), FALSE, FALSE, 0);

    // Widget List
    card = create_collapsible_card("Widgets", "📦", TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), card, FALSE, FALSE, 0);
    GtkWidget *content = get_collapsible_content(card);
    const char *widgets[] = {
        "Launcher", "Workspaces", "Clock", "Volume", "Battery",
        "Network", "Bluetooth", "Tray", "Dock", "Media",
        "System Monitor", "Weather", "Night Light", "Power Profile", "Quick Settings",
        "Clipboard", "Keybinds", "Keyboard Layout", "Show Desktop", "Notification Center"
    };
    for (int i = 0; i < 20; i++) {
        char key_name[64];
        snprintf(key_name, sizeof(key_name), "widget.%s", widgets[i]);
        char cmd_on[128], cmd_off[128];
        snprintf(cmd_on, sizeof(cmd_on), "ocws-kv set '%s' 1 &", key_name);
        snprintf(cmd_off, sizeof(cmd_off), "ocws-kv set '%s' 0 &", key_name);
        gtk_box_pack_start(GTK_BOX(content), create_live_toggle_row(widgets[i], NULL, TRUE, cmd_on, cmd_off), FALSE, FALSE, 0);
    }

    return scroll;
}

// ============================================================
// Tab: Workspaces
// ============================================================

GtkWidget* build_workspaces_tab(void) {
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 20);
    gtk_container_add(GTK_CONTAINER(scroll), vbox);

    GtkWidget *card = create_card("Workspace Settings", "🖥️");
    gtk_box_pack_start(GTK_BOX(vbox), card, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), create_live_slider_row("Count", 5, 1, 12, "", "ocws-kv set workspace.count %d &"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), create_live_toggle_row("Show Names", "Display workspace names instead of numbers", FALSE, "ocws-kv set workspace.show_names 1 &", "ocws-kv set workspace.show_names 0 &"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), create_live_toggle_row("App Icons", "Show running app icons in workspace", TRUE, "ocws-kv set workspace.app_icons 1 &", "ocws-kv set workspace.app_icons 0 &"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), create_live_toggle_row("Scroll Switch", "Switch workspace with scroll wheel", TRUE, "ocws-kv set workspace.scroll 1 &", "ocws-kv set workspace.scroll 0 &"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), create_live_toggle_row("Drag Reorder", "Drag to reorder workspaces", TRUE, "ocws-kv set workspace.drag 1 &", "ocws-kv set workspace.drag 0 &"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), create_live_toggle_row("Follow Focus", "Bar shows focused workspace", FALSE, "ocws-kv set workspace.follow 1 &", "ocws-kv set workspace.follow 0 &"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), create_live_toggle_row("Occupied Only", "Hide empty workspaces", FALSE, "ocws-kv set workspace.occupied_only 1 &", "ocws-kv set workspace.occupied_only 0 &"), FALSE, FALSE, 0);

    return scroll;
}

// ============================================================
// Tab: Notifications
// ============================================================

GtkWidget* build_notifications_tab(void) {
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 20);
    gtk_container_add(GTK_CONTAINER(scroll), vbox);

    // Daemon (always visible)
    GtkWidget *card = create_card("Notification Daemon", "🔔");
    gtk_box_pack_start(GTK_BOX(vbox), card, FALSE, FALSE, 0);
    const char *daemons[] = {"Mako", "Dunst", "OCWS Native", "Disable"};
    gtk_box_pack_start(GTK_BOX(card), create_button_group("Daemon", daemons, 4, 0), FALSE, FALSE, 0);

    // Behavior (collapsible, starts expanded)
    card = create_collapsible_card("Behavior", "⚙️", TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), card, FALSE, FALSE, 0);
    GtkWidget *content = get_collapsible_content(card);
    gtk_box_pack_start(GTK_BOX(content), create_live_slider_row("Timeout", 5, 1, 30, "s", "ocws-kv set notif.timeout %d &"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), create_live_slider_row("Max Visible", 5, 1, 10, "", "ocws-kv set notif.max %d &"), FALSE, FALSE, 0);

    // Appearance (collapsible, starts collapsed)
    card = create_collapsible_card("Appearance", "🎨", FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), card, FALSE, FALSE, 0);
    content = get_collapsible_content(card);
    gtk_box_pack_start(GTK_BOX(content), create_live_slider_row("Border Radius", 8, 0, 24, "px", "ocws-kv set notif.radius %d &"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), create_live_toggle_row("Actions", "Show action buttons in notifications", TRUE, "ocws-kv set notif.actions 1 &", "ocws-kv set notif.actions 0 &"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), create_live_toggle_row("Persistence", "Save notification history", TRUE, "ocws-kv set notif.persist 1 &", "ocws-kv set notif.persist 0 &"), FALSE, FALSE, 0);

    return scroll;
}

// ============================================================
// Tab: Diagnostics
// ============================================================

static void on_refresh_health(GtkWidget *btn, gpointer textview) {
    (void)btn;
    load_healthcheck(GTK_WIDGET(textview));
}

GtkWidget* build_diagnostics_tab(void) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 30);

    // Header
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *lbl = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl), "<span size='large' weight='bold'>System Diagnostics</span>");
    GtkWidget *refresh_btn = gtk_button_new_from_icon_name("view-refresh-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_box_pack_start(GTK_BOX(header), lbl, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(header), refresh_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), header, FALSE, FALSE, 0);

    // Notebook for different checks
    GtkWidget *notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);

    // Validation tab
    GtkWidget *scroll1 = gtk_scrolled_window_new(NULL, NULL);
    GtkWidget *textview1 = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(textview1), FALSE);
    gtk_style_context_add_class(gtk_widget_get_style_context(textview1), "terminal");
    gtk_container_add(GTK_CONTAINER(scroll1), textview1);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), scroll1, gtk_label_new("Validation"));
    g_signal_connect(refresh_btn, "clicked", G_CALLBACK(on_refresh_health), textview1);

    // System Info tab
    GtkWidget *scroll2 = gtk_scrolled_window_new(NULL, NULL);
    GtkWidget *textview2 = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(textview2), FALSE);
    gtk_style_context_add_class(gtk_widget_get_style_context(textview2), "terminal");
    gtk_container_add(GTK_CONTAINER(scroll2), textview2);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), scroll2, gtk_label_new("System Info"));
    load_system_info(textview2);

    // Auto-load validation
    load_healthcheck(textview1);

    return vbox;
}

// ============================================================
// Tab: Quick Settings
// ============================================================

GtkWidget* build_quick_settings_tab(void) {
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 20);
    gtk_container_add(GTK_CONTAINER(scroll), vbox);

    // Audio Controls
    GtkWidget *card = create_card("Audio Controls", "🔊");
    gtk_box_pack_start(GTK_BOX(vbox), card, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), create_live_slider_row("Master Volume", 50, 0, 150, "%", "ocws-volume set %d &"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), create_live_slider_row("Microphone", 80, 0, 100, "%", "wpctl set-volume @DEFAULT_AUDIO_SOURCE@ %d%% &"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), create_live_toggle_row("Mute Output", "Mute all audio output", FALSE, "ocws-volume mute &", "ocws-volume unmute &"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), create_live_toggle_row("Mute Microphone", "Mute audio input", FALSE, "wpctl set-mute @DEFAULT_AUDIO_SOURCE@ 1 &", "wpctl set-mute @DEFAULT_AUDIO_SOURCE@ 0 &"), FALSE, FALSE, 0);

    // Display & Brightness
    card = create_card("Display", "☀️");
    gtk_box_pack_start(GTK_BOX(vbox), card, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), create_live_slider_row("Brightness", 75, 0, 100, "%", "ocws-brightness set %d &"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), create_live_toggle_row("Night Light", "Reduce blue light", FALSE, "wlsunset -t 4000 &", "killall wlsunset &"), FALSE, FALSE, 0);

    // Connectivity
    card = create_card("Connectivity", "📶");
    gtk_box_pack_start(GTK_BOX(vbox), card, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), create_live_toggle_row("Wi-Fi", "Enable wireless networking", TRUE, "nmcli radio wifi on &", "nmcli radio wifi off &"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), create_live_toggle_row("Bluetooth", "Enable Bluetooth adapters", TRUE, "bluetoothctl power on &", "bluetoothctl power off &"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), create_live_toggle_row("Airplane Mode", "Disable all radios", FALSE, "nmcli radio all off &", "nmcli radio all on &"), FALSE, FALSE, 0);

    // Input & Hardware
    card = create_card("Input", "🖱️");
    gtk_box_pack_start(GTK_BOX(vbox), card, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), create_live_toggle_row("Natural Scrolling", "Invert scroll direction", TRUE, "toggle-natural-scroll.sh 1 &", "toggle-natural-scroll.sh 0 &"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), create_live_toggle_row("Tap to Click", "Touchpad tap to click", TRUE, "toggle-tap-to-click.sh 1 &", "toggle-tap-to-click.sh 0 &"), FALSE, FALSE, 0);

    return scroll;
}

// ============================================================
// Tab: Keybindings
// ============================================================

GtkWidget* build_keybinds_tab(void) {
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 20);
    gtk_container_add(GTK_CONTAINER(scroll), vbox);

    // Header with search and actions
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *lbl = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl), "<span size='large' weight='bold'>Keybinding Manager</span>");
    gtk_box_pack_start(GTK_BOX(header), lbl, TRUE, TRUE, 0);

    GtkWidget *search_entry = gtk_search_entry_new();
    gtk_widget_set_size_request(search_entry, 200, -1);
    gtk_box_pack_start(GTK_BOX(header), search_entry, FALSE, FALSE, 0);

    GtkWidget *add_btn = gtk_button_new_with_label("+ Add");
    gtk_box_pack_start(GTK_BOX(header), add_btn, FALSE, FALSE, 0);

    GtkWidget *check_btn = gtk_button_new_with_label("Check Conflicts");
    gtk_box_pack_start(GTK_BOX(header), check_btn, FALSE, FALSE, 0);

    GtkWidget *reload_btn = gtk_button_new_from_icon_name("view-refresh-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_box_pack_start(GTK_BOX(header), reload_btn, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), header, FALSE, FALSE, 0);

    // Preset selector
    GtkWidget *preset_card = create_card("Presets", "📋");
    gtk_box_pack_start(GTK_BOX(vbox), preset_card, FALSE, FALSE, 0);
    const char *presets[] = {"Default", "Vim", "Emacs", "Custom"};
    gtk_box_pack_start(GTK_BOX(preset_card), create_button_group("Preset", presets, 4, 0), FALSE, FALSE, 0);

    // Keybinding list
    GtkWidget *list_card = create_card("Keybindings", "⌨️");
    gtk_box_pack_start(GTK_BOX(vbox), list_card, TRUE, TRUE, 0);

    GtkWidget *listbox = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(listbox), GTK_SELECTION_SINGLE);
    gtk_widget_set_vexpand(listbox, TRUE);
    gtk_box_pack_start(GTK_BOX(list_card), listbox, TRUE, TRUE, 0);

    // Populate with keybindings from rc.xml
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    char rc_path[512];
    snprintf(rc_path, sizeof(rc_path), "%s/.config/labwc/rc.xml", home);

    FILE *fp = fopen(rc_path, "r");
    if (fp) {
        char line[1024];
        while (fgets(line, sizeof(line), fp)) {
            char *key_start = strstr(line, "key=\"");
            if (!key_start) continue;
            key_start += 5;
            char *key_end = strchr(key_start, '"');
            if (!key_end) continue;

            char key[64] = {0};
            int key_len = key_end - key_start;
            if (key_len > 0 && key_len < (int)sizeof(key)) {
                strncpy(key, key_start, key_len);
                key[key_len] = '\0';
            }

            char *action_start = strstr(line, "name=\"");
            char action[128] = {0};
            if (action_start) {
                action_start += 6;
                char *action_end = strchr(action_start, '"');
                if (action_end) {
                    int action_len = action_end - action_start;
                    if (action_len > 0 && action_len < (int)sizeof(action)) {
                        strncpy(action, action_start, action_len);
                        action[action_len] = '\0';
                    }
                }
            }

            char command[256] = {0};
            if (strcmp(action, "Execute") == 0) {
                char *cmd_start = strstr(line, "<command>");
                if (cmd_start) {
                    cmd_start += 9;
                    char *cmd_end = strstr(line, "</command>");
                    if (cmd_end) {
                        int cmd_len = cmd_end - cmd_start;
                        if (cmd_len > 0 && cmd_len < (int)sizeof(command)) {
                            strncpy(command, cmd_start, cmd_len);
                            command[cmd_len] = '\0';
                        }
                    }
                }
            }

            if (key[0]) {
                char display_key[128];
                snprintf(display_key, sizeof(display_key), "%s", key);
                char *p;
                while ((p = strstr(display_key, "W-")) != NULL) { memmove(p+6, p+2, strlen(p+2)+1); memcpy(p, "Super+", 6); }
                while ((p = strstr(display_key, "A-")) != NULL) { memmove(p+4, p+2, strlen(p+2)+1); memcpy(p, "Alt+", 4); }
                while ((p = strstr(display_key, "C-")) != NULL) { memmove(p+5, p+2, strlen(p+2)+1); memcpy(p, "Ctrl+", 5); }
                while ((p = strstr(display_key, "S-")) != NULL) { memmove(p+6, p+2, strlen(p+2)+1); memcpy(p, "Shift+", 6); }

                GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
                gtk_widget_set_margin_top(row, 4);
                gtk_widget_set_margin_bottom(row, 4);
                gtk_widget_set_margin_start(row, 8);
                gtk_widget_set_margin_end(row, 8);

                char key_markup[256];
                snprintf(key_markup, sizeof(key_markup), "<span font_family='monospace' weight='bold' foreground='%s'>%s</span>", OCWS_ACCENT(), display_key);
                GtkWidget *key_lbl = gtk_label_new(NULL);
                gtk_label_set_markup(GTK_LABEL(key_lbl), key_markup);
                gtk_widget_set_size_request(key_lbl, 180, -1);
                gtk_label_set_xalign(GTK_LABEL(key_lbl), 0.0);
                gtk_box_pack_start(GTK_BOX(row), key_lbl, FALSE, FALSE, 0);

                GtkWidget *arrow_lbl = gtk_label_new("→");
                gtk_style_context_add_class(gtk_widget_get_style_context(arrow_lbl), "dim-label");
                gtk_box_pack_start(GTK_BOX(row), arrow_lbl, FALSE, FALSE, 0);

                char action_markup[256];
                if (command[0]) {
                    snprintf(action_markup, sizeof(action_markup), "<b>%s</b>  <span foreground='%s'>%s</span>", action, OCWS_MUTED(), command);
                } else {
                    snprintf(action_markup, sizeof(action_markup), "<b>%s</b>", action);
                }
                GtkWidget *action_lbl = gtk_label_new(NULL);
                gtk_label_set_markup(GTK_LABEL(action_lbl), action_markup);
                gtk_label_set_ellipsize(GTK_LABEL(action_lbl), PANGO_ELLIPSIZE_END);
                gtk_box_pack_start(GTK_BOX(row), action_lbl, TRUE, TRUE, 0);

                GtkWidget *edit_btn = gtk_button_new_from_icon_name("document-edit-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
                gtk_widget_set_valign(edit_btn, GTK_ALIGN_CENTER);
                gtk_box_pack_start(GTK_BOX(row), edit_btn, FALSE, FALSE, 0);

                GtkWidget *del_btn = gtk_button_new_from_icon_name("edit-delete-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
                gtk_widget_set_valign(del_btn, GTK_ALIGN_CENTER);
                gtk_box_pack_start(GTK_BOX(row), del_btn, FALSE, FALSE, 0);

                gtk_list_box_insert(GTK_LIST_BOX(listbox), row, -1);
            }
        }
        fclose(fp);
    }

    // Bottom actions
    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(btn_box, GTK_ALIGN_END);

    GtkWidget *import_btn = gtk_button_new_with_label("Import Preset");
    GtkWidget *export_btn = gtk_button_new_with_label("Export Preset");
    GtkWidget *validate_btn = gtk_button_new_with_label("Validate");
    GtkWidget *apply_btn = gtk_button_new_with_label("Apply & Reload");
    gtk_style_context_add_class(gtk_widget_get_style_context(apply_btn), "suggested-action");

    gtk_box_pack_start(GTK_BOX(btn_box), import_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(btn_box), export_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(btn_box), validate_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(btn_box), apply_btn, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), btn_box, FALSE, FALSE, 0);

    return scroll;
}

// ============================================================
// Tab: About
// ============================================================

GtkWidget* build_about_tab(void) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_halign(vbox, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(vbox, GTK_ALIGN_CENTER);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 40);

    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title),
        "<span size='xx-large' weight='bold'>OCWS</span>\n"
        "<span size='medium'>Our C-Written Shell</span>");
    gtk_label_set_use_markup(GTK_LABEL(title), TRUE);
    gtk_label_set_justify(GTK_LABEL(title), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(vbox), title, FALSE, FALSE, 0);

    char version_buf[128];
    snprintf(version_buf, sizeof(version_buf), "Version %s", VERSION);
    GtkWidget *ver_lbl = gtk_label_new(version_buf);
    gtk_style_context_add_class(gtk_widget_get_style_context(ver_lbl), "dim-label");
    gtk_box_pack_start(GTK_BOX(vbox), ver_lbl, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);

    GtkWidget *desc = gtk_label_new(
        "A batteries-included desktop shell built on sfwbar,\n"
        "inspired by DankMaterialShell and Noctalia.\n"
        "Written in C for maximum performance and minimal memory usage.");
    gtk_label_set_line_wrap(GTK_LABEL(desc), TRUE);
    gtk_label_set_justify(GTK_LABEL(desc), GTK_JUSTIFY_CENTER);
    gtk_style_context_add_class(gtk_widget_get_style_context(desc), "dim-label");
    gtk_box_pack_start(GTK_BOX(vbox), desc, FALSE, FALSE, 0);

    GtkWidget *link_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_set_halign(link_box, GTK_ALIGN_CENTER);
    GtkWidget *github_btn = gtk_button_new_with_label("GitHub");
    GtkWidget *docs_btn = gtk_button_new_with_label("Documentation");
    GtkWidget *report_btn = gtk_button_new_with_label("Report Issue");
    g_signal_connect(github_btn, "clicked", G_CALLBACK(execute_command),
                     (gpointer)"xdg-open https://github.com/naranyala/labwc-fuzzel-sfwbar &");
    g_signal_connect(docs_btn, "clicked", G_CALLBACK(execute_command),
                     (gpointer)"xdg-open https://github.com/naranyala/labwc-fuzzel-sfwbar/tree/main/docs &");
    g_signal_connect(report_btn, "clicked", G_CALLBACK(execute_command),
                     (gpointer)"xdg-open https://github.com/naranyala/labwc-fuzzel-sfwbar/issues &");
    gtk_box_pack_start(GTK_BOX(link_box), github_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(link_box), docs_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(link_box), report_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), link_box, FALSE, FALSE, 0);

    GtkWidget *license = gtk_label_new("MIT License");
    gtk_style_context_add_class(gtk_widget_get_style_context(license), "dim-label");
    gtk_box_pack_start(GTK_BOX(vbox), license, FALSE, FALSE, 0);

    return vbox;
}

// ============================================================
// Tab: Credits / Thanks
// ============================================================

static void on_credit_url_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    const char *url = (const char *)data;
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "xdg-open '%s' &", url);
    system(cmd);
}

GtkWidget* build_credits_tab(void) {
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 20);
    gtk_container_add(GTK_CONTAINER(scroll), vbox);

    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title),
        "<span size='x-large' weight='bold'>Credits &amp; Dependencies</span>");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), title, FALSE, FALSE, 0);

    GtkWidget *desc = gtk_label_new(
        "OCWS is built on the shoulders of amazing open-source projects.\n"
        "Please visit and support these upstream repositories.");
    gtk_label_set_line_wrap(GTK_LABEL(desc), TRUE);
    gtk_label_set_xalign(GTK_LABEL(desc), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), desc, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 4);

    struct { const char *name; const char *desc; const char *url; } deps[] = {
        {"labwc",             "Wayland compositor",           "https://github.com/labwc/labwc"},
        {"sfwbar",            "Status bar for Wayland",       "https://github.com/LBCrion/sfwbar"},
        {"fuzzel",            "Application launcher",         "https://codeberg.org/dnkl/fuzzel"},
        {"foot",              "Terminal emulator",            "https://codeberg.org/dnkl/foot"},
        {"mako",              "Notification daemon",          "https://github.com/emersion/mako"},
        {"rofi",              "Window switcher & launcher",   "https://github.com/DaveDavenport/rofi"},
        {"DankMaterialShell", "Material Design 3 shell",     "https://github.com/DankShrine/dms"},
        {"wl-clipboard",      "Clipboard utilities",          "https://github.com/bugaevc/wl-clipboard"},
        {"cliphist",          "Clipboard history",            "https://github.com/sentriz/cliphist"},
        {"swaybg",            "Wallpaper setter",             "https://github.com/swaywm/swaybg"},
        {"swaylock",          "Screen locker",                "https://github.com/swaywm/swaylock"},
        {"swayidle",          "Idle management",              "https://github.com/swaywm/swayidle"},
        {"grim & slurp",      "Screenshot tools",             "https://github.com/emersion/grim"},
        {"playerctl",         "Media player controller",      "https://github.com/altdesktop/playerctl"},
        {"brightnessctl",     "Brightness control",           "https://github.com/Hummer12007/brightnessctl"},
        {"wlr-randr",         "Output configuration",         "https://gitlab.freedesktop.org/emersion/wlr-randr"},
        {"gammastep",         "Color temperature",            "https://gitlab.com/chinstrap/gammastep"},
    };
    int n_deps = sizeof(deps) / sizeof(deps[0]);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_box_pack_start(GTK_BOX(vbox), grid, FALSE, FALSE, 0);

    for (int i = 0; i < n_deps; i++) {
        int row = i;

        GtkWidget *name_lbl = gtk_label_new(NULL);
        char *markup = g_strdup_printf("<b>%s</b>", deps[i].name);
        gtk_label_set_markup(GTK_LABEL(name_lbl), markup);
        g_free(markup);
        gtk_widget_set_size_request(name_lbl, 160, -1);
        gtk_label_set_xalign(GTK_LABEL(name_lbl), 0.0);
        gtk_grid_attach(GTK_GRID(grid), name_lbl, 0, row, 1, 1);

        GtkWidget *desc_lbl = gtk_label_new(deps[i].desc);
        gtk_style_context_add_class(gtk_widget_get_style_context(desc_lbl), "dim-label");
        gtk_label_set_xalign(GTK_LABEL(desc_lbl), 0.0);
        gtk_grid_attach(GTK_GRID(grid), desc_lbl, 1, row, 1, 1);

        GtkWidget *link_btn = gtk_button_new_with_label("Visit");
        g_signal_connect(link_btn, "clicked", G_CALLBACK(on_credit_url_clicked),
                         (gpointer)deps[i].url);
        gtk_grid_attach(GTK_GRID(grid), link_btn, 2, row, 1, 1);
    }

    return scroll;
}
