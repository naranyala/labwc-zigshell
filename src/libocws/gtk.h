#ifndef OCWS_GTK_H
#define OCWS_GTK_H

#include <gtk/gtk.h>
#include <string.h>

/**
 * Enforces a modern, premium GTK theme (like adw-gtk3-dark) across the app,
 * overriding the host distribution's default theme.
 */
static inline void ocws_gtk_enforce_premium_theme(void) {
    GtkSettings *settings = gtk_settings_get_default();
    if (settings) {
        // Attempt to use Adw-Gtk3 if installed, otherwise defaults to standard dark
        g_object_set(settings, "gtk-theme-name", "adw-gtk3-dark", NULL);
        g_object_set(settings, "gtk-application-prefer-dark-theme", TRUE, NULL);
        
        // Also enable smooth scrolling for all scroll windows
        g_object_set(settings, "gtk-enable-animations", TRUE, NULL);
    }
}

/**
 * Dynamically injects "Material You" CSS styling, reading an accent hex color.
 * Also adds the base classes for floating Cards and Shadows.
 */
static inline void ocws_gtk_apply_dynamic_css(GtkApplication *app, const char *accent_hex) {
    (void)app; // Not strictly needed since we use screen default
    
    if (!accent_hex) accent_hex = "#89b4fa"; // Default fallback (Mocha Blue)
    
    // Generate the CSS string dynamically
    char css[2048];
    snprintf(css, sizeof(css),
        "/* OCWS Premium Injected CSS */\n"
        "@define-color accent_color %s;\n"
        "@define-color accent_bg_color %s;\n"
        "\n"
        "/* The Floating Card Abstraction */\n"
        ".ocws-card {\n"
        "    background-color: alpha(@theme_bg_color, 0.6);\n"
        "    border-radius: 12px;\n"
        "    box-shadow: 0px 4px 16px rgba(0, 0, 0, 0.25);\n"
        "    border: 1px solid alpha(currentColor, 0.08);\n"
        "    margin: 8px;\n"
        "    padding: 16px;\n"
        "}\n"
        "\n"
        "/* Premium Button Styling */\n"
        "button, .button {\n"
        "    border-radius: 8px;\n"
        "    transition: all 0.2s ease;\n"
        "}\n"
        "button:hover {\n"
        "    box-shadow: 0px 2px 8px rgba(0, 0, 0, 0.15);\n"
        "}\n"
        "button.suggested-action {\n"
        "    background-color: @accent_bg_color;\n"
        "    color: white;\n"
        "}\n",
        accent_hex, accent_hex);

    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, css, -1, NULL);
    
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 20
    );
    
    g_object_unref(provider);
}

/**
 * Helper to wrap a widget inside a beautiful floating OCWS Card.
 * Returns the card container (GtkFrame), so you can pack it into your layout.
 */
static inline GtkWidget *ocws_gtk_create_card(GtkWidget *child) {
    // We use a GtkFrame without a label as the card body
    GtkWidget *frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
    
    // Apply our dynamic CSS class
    GtkStyleContext *ctx = gtk_widget_get_style_context(frame);
    gtk_style_context_add_class(ctx, "ocws-card");
    
    if (child) {
        gtk_container_add(GTK_CONTAINER(frame), child);
    }
    
    return frame;
}

#endif // OCWS_GTK_H
