#ifndef OCWS_GTK_BUILDER_H
#define OCWS_GTK_BUILDER_H

#include <gtk/gtk.h>
#include <stdarg.h>

/**
 * libocws/gtk_builder.h - Declarative UI Utilities for GTK3
 * 
 * Dramatically reduces boilerplate by allowing you to build GTK 
 * layouts using nested, declarative functions (similar to Flutter/SwiftUI).
 */

// =====================================================================
// Layout Builders (Variadic Functions)
// =====================================================================

/**
 * Creates a Horizontal Box and automatically packs a NULL-terminated list of children.
 * Example: ocws_hbox(10, btn1, label1, NULL);
 */
static inline GtkWidget *ocws_hbox(int spacing, ...) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, spacing);
    va_list args;
    va_start(args, spacing);
    
    GtkWidget *child;
    while ((child = va_arg(args, GtkWidget *)) != NULL) {
        gtk_box_pack_start(GTK_BOX(box), child, FALSE, FALSE, 0);
    }
    
    va_end(args);
    return box;
}

/**
 * Creates a Vertical Box and automatically packs a NULL-terminated list of children.
 * Example: ocws_vbox(10, header, ocws_hbox(...), NULL);
 */
static inline GtkWidget *ocws_vbox(int spacing, ...) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, spacing);
    va_list args;
    va_start(args, spacing);
    
    GtkWidget *child;
    while ((child = va_arg(args, GtkWidget *)) != NULL) {
        gtk_box_pack_start(GTK_BOX(box), child, FALSE, FALSE, 0);
    }
    
    va_end(args);
    return box;
}

// =====================================================================
// Widget Helpers (One-Liners)
// =====================================================================

/**
 * Creates a label, optionally applying markup or a CSS class.
 * Pass NULL for css_class if not needed.
 */
static inline GtkWidget *ocws_label(const char *text, const char *css_class) {
    GtkWidget *lbl = gtk_label_new(NULL);
    // If text contains HTML tags like <b>, use markup, else normal text
    if (text && (strstr(text, "<") != NULL && strstr(text, ">") != NULL)) {
        gtk_label_set_markup(GTK_LABEL(lbl), text);
    } else {
        gtk_label_set_text(GTK_LABEL(lbl), text);
    }
    
    if (css_class) {
        gtk_style_context_add_class(gtk_widget_get_style_context(lbl), css_class);
    }
    return lbl;
}

/**
 * Creates a button with a label or icon, and binds the click callback instantly.
 */
static inline GtkWidget *ocws_button(const char *label_or_icon, GCallback on_click, gpointer data) {
    GtkWidget *btn = NULL;
    
    // Simple heuristic: if it ends in "-symbolic" or contains no spaces and has dashes, treat as icon
    if (strstr(label_or_icon, "-symbolic") || (strchr(label_or_icon, '-') && !strchr(label_or_icon, ' '))) {
        btn = gtk_button_new_from_icon_name(label_or_icon, GTK_ICON_SIZE_BUTTON);
    } else {
        btn = gtk_button_new_with_label(label_or_icon);
    }
    
    if (on_click) {
        g_signal_connect(btn, "clicked", on_click, data);
    }
    
    return btn;
}

// =====================================================================
// Modifier Helpers
// =====================================================================

/**
 * Applies margins to any widget instantly (like CSS padding/margin).
 * Returns the same widget so you can chain it inline!
 */
static inline GtkWidget *ocws_with_margin(GtkWidget *widget, int top, int right, int bottom, int left) {
    gtk_widget_set_margin_top(widget, top);
    gtk_widget_set_margin_end(widget, right);
    gtk_widget_set_margin_bottom(widget, bottom);
    gtk_widget_set_margin_start(widget, left);
    return widget;
}

/**
 * Forces a widget to expand and fill available space.
 */
static inline GtkWidget *ocws_expanded(GtkWidget *widget) {
    gtk_widget_set_hexpand(widget, TRUE);
    gtk_widget_set_vexpand(widget, TRUE);
    // If it's in a box, this hints it should fill
    gtk_widget_set_halign(widget, GTK_ALIGN_FILL);
    gtk_widget_set_valign(widget, GTK_ALIGN_FILL);
    return widget;
}

#endif // OCWS_GTK_BUILDER_H
