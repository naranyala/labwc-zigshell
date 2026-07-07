/*
 * ocws-store.h — Reactive state/store management for GTK (GObject).
 *
 * OcwsStore is a single-owner, dynamically-keyed reactive store. It is a plain
 * GObject, so it composes with GTK for free: every key change emits a "changed"
 * signal (with a per-key detail "changed::key"), which drives bindings,
 * computed/derived values, effects (watch), and GTK widget bindings.
 *
 * Design goals:
 *   - One source of truth shared by the bar, settings panel, AI runner, etc.
 *   - Rich reactivity primitives: detailed signals (selectors), watch/effects,
 *     computed/derived values, batching, two-way bindings, persistence.
 *   - GObject-native: integrates with gtk_widget / g_object_bind_property style.
 */

#ifndef OCWS_STORE_H
#define OCWS_STORE_H

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OCWS_TYPE_STORE (ocws_store_get_type())
G_DECLARE_FINAL_TYPE(OcwsStore, ocws_store, OCWS, STORE, GObject)

/* ---- Construction ---- */
OcwsStore *ocws_store_new(void);
OcwsStore *ocws_store_default(void);   /* process-wide singleton */

/* ---- Typed setters ---- */
void ocws_store_set_bool   (OcwsStore *self, const char *key, gboolean v);
void ocws_store_set_int    (OcwsStore *self, const char *key, gint v);
void ocws_store_set_double (OcwsStore *self, const char *key, gdouble v);
void ocws_store_set_string (OcwsStore *self, const char *key, const char *v);
void ocws_store_set_value  (OcwsStore *self, const char *key, const GValue *v);

/* ---- Typed getters (return `def` / FALSE when key is absent) ---- */
gboolean ocws_store_get_bool   (OcwsStore *self, const char *key, gboolean def);
gint     ocws_store_get_int    (OcwsStore *self, const char *key, gint def);
gdouble  ocws_store_get_double (OcwsStore *self, const char *key, gdouble def);
char    *ocws_store_get_string (OcwsStore *self, const char *key, const char *def); /* caller frees */
gboolean ocws_store_get_value  (OcwsStore *self, const char *key, GValue *out);      /* FALSE if absent */

/* ---- Queries ---- */
gboolean ocws_store_has   (OcwsStore *self, const char *key);
void     ocws_store_remove(OcwsStore *self, const char *key);
void     ocws_store_clear (OcwsStore *self);

/* ---- Reactivity: watch a key (an effect).
 * Connects to "changed::key" (or "changed" when key == NULL for all keys).
 * Returns a handler id usable with ocws_store_unwatch(). */
typedef void (*OcwsWatchFn)(OcwsStore *self, const char *key,
                            const GValue *value, gpointer data);
guint ocws_store_watch   (OcwsStore *self, const char *key,
                          OcwsWatchFn cb, gpointer data, GDestroyNotify notify);
void  ocws_store_unwatch (OcwsStore *self, guint id);

/* ---- Derived/computed values ----
 * `name` is recomputed whenever any key in `deps` changes. The compute
 * callback is expected to write the result back via ocws_store_set_* on `name`.
 * Chained computeds (a computed depending on another computed's name) work. */
typedef void (*OcwsComputeFn)(OcwsStore *self, const char *name, gpointer data);
void ocws_store_computed(OcwsStore *self, const char *name,
                         const char **deps, gsize n_deps,
                         OcwsComputeFn fn, gpointer data, GDestroyNotify notify);

/* ---- Batching: coalesce change notifications until commit ---- */
void ocws_store_batch_begin (OcwsStore *self);
void ocws_store_batch_commit(OcwsStore *self);

/* ---- Persistence: type-tagged flat file (round-trips all types) ---- */
gboolean ocws_store_save(OcwsStore *self, const char *path);
gboolean ocws_store_load(OcwsStore *self, const char *path);

/* ---- GTK convenience bindings (operate on the default store) ----
 * One-way: store -> widget. Two-way: store <-> widget. */
void ocws_store_bind_label (GtkLabel  *label, const char *key);
void ocws_store_bind_switch(GtkSwitch *sw,    const char *key);
void ocws_store_bind_scale (GtkScale  *scale, const char *key);

/* Store-aware variants (bind to an explicit OcwsStore instance). The
 * convenience forms above delegate to these with ocws_store_default(). */
void ocws_store_bind_label_store (OcwsStore *self, GtkLabel  *label, const char *key);
void ocws_store_bind_switch_store(OcwsStore *self, GtkSwitch *sw,    const char *key);
void ocws_store_bind_scale_store (OcwsStore *self, GtkScale  *scale, const char *key);

/* ---- Generic two-way binding to any GObject property ----
 * forward: store value -> target property ; backward: target property -> store.
 * Pass G_BINDING_BIDIRECTIONAL in `flags` for two-way. `fwd`/`bwd` transforms
 * are optional (NULL = direct copy / g_value_transform). Returns an opaque
 * handle freed by ocws_store_unbind(). */
typedef gboolean (*OcwsTransform)(const GValue *src, GValue *dst, gpointer data);
gpointer ocws_store_bind(OcwsStore *self, const char *key,
                         GObject *target, const char *prop,
                         GBindingFlags flags,
                         OcwsTransform fwd, OcwsTransform bwd, gpointer data);
void ocws_store_unbind(gpointer handle);

G_END_DECLS

#endif /* OCWS_STORE_H */
