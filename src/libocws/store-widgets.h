/*
 * ocws-store-widgets.h — Pleasant GTK widgets bound to OcwsStore.
 *
 * Everything here is static-inline (header-only), so just #include it.
 * It builds on libocws/store.h: each builder creates a ready-made,
 * state-bound settings row so you never hand-wire a switch/scale/entry
 * to your store again.
 *
 * Requires: #include "libocws/store.h" (pulled in below) and a GTK init.
 */

#ifndef OCWS_STORE_WIDGETS_H
#define OCWS_STORE_WIDGETS_H

#include "libocws/store.h"
#include <gtk/gtk.h>

/* ----------------------------------------------------------------
 * Row scaffold: title (+ optional subtitle) on the left, control on the right.
 * Returns a GtkBox you pack into a list / card.
 * ---------------------------------------------------------------- */
static inline GtkWidget *ocws_row(const char *title, const char *subtitle,
                                  GtkWidget *control) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_start(row, 12);
    gtk_widget_set_margin_end(row, 12);
    gtk_widget_set_margin_top(row, 8);
    gtk_widget_set_margin_bottom(row, 8);
    gtk_widget_set_valign(row, GTK_ALIGN_CENTER);

    GtkWidget *labels = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *t = gtk_label_new(title);
    gtk_widget_set_halign(t, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(labels), t, FALSE, FALSE, 0);
    if (subtitle) {
        GtkWidget *s = gtk_label_new(subtitle);
        gtk_label_set_line_wrap(GTK_LABEL(s), TRUE);
        gtk_style_context_add_class(gtk_widget_get_style_context(s), "dim-label");
        gtk_widget_set_halign(s, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(labels), s, FALSE, FALSE, 0);
    }
    gtk_box_pack_start(GTK_BOX(row), labels, TRUE, TRUE, 0);

    if (control) {
        gtk_widget_set_valign(control, GTK_ALIGN_CENTER);
        gtk_box_pack_end(GTK_BOX(row), control, FALSE, FALSE, 0);
    }
    return row;
}

/* Two-way bound switch row. */
static inline GtkWidget *ocws_row_switch(OcwsStore *store, const char *key,
                                         const char *title, const char *subtitle) {
    GtkWidget *sw = gtk_switch_new();
    ocws_store_bind_switch_store(store, GTK_SWITCH(sw), key);
    return ocws_row(title, subtitle, sw);
}

/* Two-way bound slider row (store holds an int; scale is double). */
static inline GtkWidget *ocws_row_slider(OcwsStore *store, const char *key,
                                         const char *title,
                                         double min, double max, double step) {
    GtkWidget *scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,
                                                min, max, step);
    gtk_widget_set_size_request(scale, 160, -1);
    ocws_store_bind_scale_store(store, GTK_SCALE(scale), key);
    return ocws_row(title, NULL, scale);
}

/* Two-way bound text entry row (store holds a string). */
static inline GtkWidget *ocws_row_entry(OcwsStore *store, const char *key,
                                        const char *title) {
    GtkWidget *entry = gtk_entry_new();
    ocws_store_bind(store, key, G_OBJECT(entry), "text",
                    G_BINDING_BIDIRECTIONAL, NULL, NULL, NULL);
    return ocws_row(title, NULL, entry);
}

/* One-way bound label row (store -> label, read-only display). */
static inline GtkWidget *ocws_row_label(OcwsStore *store, const char *key,
                                        const char *title) {
    GtkWidget *lbl = gtk_label_new("");
    ocws_store_bind_label_store(store, GTK_LABEL(lbl), key);
    return ocws_row(title, NULL, lbl);
}

/* ----------------------------------------------------------------
 * CSS: load a stylesheet file (e.g. the generated tokens.css) as an
 * application-priority provider. Call once at startup.
 * ---------------------------------------------------------------- */
static inline void ocws_gtk_load_css_file(const char *path) {
    if (!path) return;
    GtkCssProvider *prov = gtk_css_provider_new();
    if (gtk_css_provider_load_from_path(prov, path, NULL)) {
        gtk_style_context_add_provider_for_screen(
            gdk_screen_get_default(),
            GTK_STYLE_PROVIDER(prov),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
    g_object_unref(prov);
}

/* ----------------------------------------------------------------
 * Threading: run `work` on a worker thread, then invoke
 * `on_done(result)` back on the GTK main loop. result is whatever
 * `work` returns. No GLib main-loop knowledge required by callers.
 * ---------------------------------------------------------------- */
typedef struct {
    GThreadFunc work;
    gpointer    work_data;
    GSourceFunc on_done;
} OcwsAsync;

static gboolean ocws_async_dispatch(gpointer data) {
    OcwsAsync *a = data;
    if (a->on_done) a->on_done(a->work_data);
    g_free(a);
    return G_SOURCE_REMOVE;
}

static gpointer ocws_async_thread(gpointer data) {
    OcwsAsync *a = data;
    gpointer result = a->work(a->work_data);
    /* marshal the completion onto the main loop */
    g_idle_add(ocws_async_dispatch, a);
    (void)result;
    return NULL;
}

static inline void ocws_gtk_run_async(GThreadFunc work, gpointer work_data,
                                     GSourceFunc on_done) {
    OcwsAsync *a = g_new0(OcwsAsync, 1);
    a->work = work;
    a->work_data = work_data;
    a->on_done = on_done;
    g_thread_new("ocws-async", ocws_async_thread, a);
}

#endif /* OCWS_STORE_WIDGETS_H */
