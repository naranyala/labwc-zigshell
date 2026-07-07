/*
 * ocws-store.c — Reactive state/store management for GTK (GObject).
 * See ocws-store.h for the full API and design notes.
 */

#include "store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * Types
 * ============================================================ */

typedef struct {
    char    **deps;
    gsize      n_deps;
    OcwsComputeFn fn;
    gpointer     data;
    GDestroyNotify notify;
} ComputedSpec;

typedef struct {
    OcwsStore *store;
    char      *key;
    GObject   *target;
    char      *prop;
    GBindingFlags flags;
    OcwsTransform fwd;
    OcwsTransform bwd;
    gpointer   data;
    gulong     fwd_id;
    gulong     bwd_id;
    gboolean   updating;
} BindCtx;

struct _OcwsStore {
    GObject parent;
    GHashTable *values;    /* key (owned) -> GValue* (owned) */
    GHashTable *computed;  /* name (owned) -> ComputedSpec* */
    GHashTable *dirty;     /* key (owned) set during a batch */
    gboolean    in_batch;
};

/* ============================================================
 * Signals
 * ============================================================ */

enum {
    SIGNAL_CHANGED,
    N_SIGNALS
};

static guint signals[N_SIGNALS];

/* ============================================================
 * GObject boilerplate
 * ============================================================ */

static void computed_spec_free(gpointer ptr) {
    ComputedSpec *spec = ptr;
    if (!spec) return;
    for (gsize i = 0; i < spec->n_deps; i++) g_free(spec->deps[i]);
    g_free(spec->deps);
    if (spec->notify) spec->notify(spec->data);
    g_free(spec);
}

static void value_free(gpointer p) {
    GValue *v = p;
    if (v) { g_value_unset(v); g_free(v); }
}

static void closure_free_str(gpointer data, GClosure *closure) {
    (void)closure;
    g_free(data);
}

static void ocws_store_finalize(GObject *object) {
    OcwsStore *self = OCWS_STORE(object);
    g_hash_table_destroy(self->values);
    g_hash_table_destroy(self->computed);
    g_hash_table_destroy(self->dirty);
    G_OBJECT_CLASS(g_type_class_peek_parent(
        g_type_class_peek(OCWS_TYPE_STORE)))->finalize(object);
}

static void ocws_store_class_init(OcwsStoreClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = ocws_store_finalize;

    signals[SIGNAL_CHANGED] = g_signal_new(
        "changed",
        OCWS_TYPE_STORE,
        G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
        0, NULL, NULL,
        NULL,                 /* generic marshaller */
        G_TYPE_NONE, 2,
        G_TYPE_STRING,        /* key */
        G_TYPE_POINTER        /* GValue* */
    );
}

static void ocws_store_init(OcwsStore *self) {
    self->values = g_hash_table_new_full(g_str_hash, g_str_equal,
                                         g_free, value_free);
    self->computed = g_hash_table_new_full(g_str_hash, g_str_equal,
                                           g_free, computed_spec_free);
    self->dirty = g_hash_table_new_full(g_str_hash, g_str_equal,
                                        g_free, NULL);
    self->in_batch = FALSE;
}

G_DEFINE_TYPE(OcwsStore, ocws_store, G_TYPE_OBJECT)

/* ============================================================
 * Internal helpers
 * ============================================================ */

static void emit_changed(OcwsStore *self, const char *key, const GValue *v) {
    g_signal_emit(self, signals[SIGNAL_CHANGED],
                  g_quark_from_string(key), key, v);
}

/* Recompute every computed whose deps include `changed_key`. */
static void trigger_computeds(OcwsStore *self, const char *changed_key) {
    GHashTableIter it;
    char *name;
    ComputedSpec *spec;
    g_hash_table_iter_init(&it, self->computed);
    while (g_hash_table_iter_next(&it, (void **)&name, (void **)&spec)) {
        for (gsize i = 0; i < spec->n_deps; i++) {
            if (strcmp(spec->deps[i], changed_key) == 0) {
                spec->fn(self, name, spec->data);
                break;
            }
        }
    }
}

static void store_set_value_internal(OcwsStore *self, const char *key,
                                     const GValue *v) {
    GValue *copy = g_new0(GValue, 1);
    g_value_init(copy, G_VALUE_TYPE(v));
    g_value_copy(v, copy);

    g_hash_table_insert(self->values, g_strdup(key), copy);

    if (self->in_batch) {
        g_hash_table_add(self->dirty, g_strdup(key));
    } else {
        emit_changed(self, key, copy);
        trigger_computeds(self, key);
    }
}

/* ============================================================
 * Construction
 * ============================================================ */

OcwsStore *ocws_store_new(void) {
    return g_object_new(OCWS_TYPE_STORE, NULL);
}

OcwsStore *ocws_store_default(void) {
    static OcwsStore *singleton = NULL;
    if (!singleton) singleton = ocws_store_new();
    return singleton;
}

/* ============================================================
 * Typed setters / getters
 * ============================================================ */

void ocws_store_set_bool(OcwsStore *self, const char *key, gboolean v) {
    g_return_if_fail(OCWS_IS_STORE(self));
    GValue val = G_VALUE_INIT;
    g_value_init(&val, G_TYPE_BOOLEAN);
    g_value_set_boolean(&val, v);
    store_set_value_internal(self, key, &val);
    g_value_unset(&val);
}

void ocws_store_set_int(OcwsStore *self, const char *key, gint v) {
    g_return_if_fail(OCWS_IS_STORE(self));
    GValue val = G_VALUE_INIT;
    g_value_init(&val, G_TYPE_INT);
    g_value_set_int(&val, v);
    store_set_value_internal(self, key, &val);
    g_value_unset(&val);
}

void ocws_store_set_double(OcwsStore *self, const char *key, gdouble v) {
    g_return_if_fail(OCWS_IS_STORE(self));
    GValue val = G_VALUE_INIT;
    g_value_init(&val, G_TYPE_DOUBLE);
    g_value_set_double(&val, v);
    store_set_value_internal(self, key, &val);
    g_value_unset(&val);
}

void ocws_store_set_string(OcwsStore *self, const char *key, const char *v) {
    g_return_if_fail(OCWS_IS_STORE(self));
    GValue val = G_VALUE_INIT;
    g_value_init(&val, G_TYPE_STRING);
    g_value_set_string(&val, v ? v : "");
    store_set_value_internal(self, key, &val);
    g_value_unset(&val);
}

void ocws_store_set_value(OcwsStore *self, const char *key, const GValue *v) {
    g_return_if_fail(OCWS_IS_STORE(self) && v);
    store_set_value_internal(self, key, v);
}

#define GET_TYPED(ret, TYPE, holds, getter, dflt)                       \
    g_return_val_if_fail(OCWS_IS_STORE(self), dflt);                    \
    GValue *v = g_hash_table_lookup(self->values, key);                 \
    if (!v || !holds(v)) return dflt;                                   \
    return getter(v);

gboolean ocws_store_get_bool(OcwsStore *self, const char *key, gboolean def) {
    GET_TYPED(gboolean, BOOLEAN, G_VALUE_HOLDS_BOOLEAN, g_value_get_boolean, def);
}

gint ocws_store_get_int(OcwsStore *self, const char *key, gint def) {
    GET_TYPED(gint, INT, G_VALUE_HOLDS_INT, g_value_get_int, def);
}

gdouble ocws_store_get_double(OcwsStore *self, const char *key, gdouble def) {
    GET_TYPED(gdouble, DOUBLE, G_VALUE_HOLDS_DOUBLE, g_value_get_double, def);
}

char *ocws_store_get_string(OcwsStore *self, const char *key, const char *def) {
    g_return_val_if_fail(OCWS_IS_STORE(self), NULL);
    GValue *v = g_hash_table_lookup(self->values, key);
    if (!v || !G_VALUE_HOLDS_STRING(v)) return def ? g_strdup(def) : NULL;
    return g_strdup(g_value_get_string(v));
}

gboolean ocws_store_get_value(OcwsStore *self, const char *key, GValue *out) {
    g_return_val_if_fail(OCWS_IS_STORE(self) && out, FALSE);
    GValue *v = g_hash_table_lookup(self->values, key);
    if (!v) return FALSE;
    g_value_init(out, G_VALUE_TYPE(v));
    g_value_copy(v, out);
    return TRUE;
}

gboolean ocws_store_has(OcwsStore *self, const char *key) {
    g_return_val_if_fail(OCWS_IS_STORE(self), FALSE);
    return g_hash_table_contains(self->values, key);
}

void ocws_store_remove(OcwsStore *self, const char *key) {
    g_return_if_fail(OCWS_IS_STORE(self));
    if (!g_hash_table_contains(self->values, key)) return;
    g_hash_table_remove(self->values, key);
    if (self->in_batch) g_hash_table_add(self->dirty, g_strdup(key));
    else emit_changed(self, key, NULL);
}

void ocws_store_clear(OcwsStore *self) {
    g_return_if_fail(OCWS_IS_STORE(self));
    GList *keys = g_hash_table_get_keys(self->values);
    for (GList *k = keys; k; k = k->next) {
        if (self->in_batch) g_hash_table_add(self->dirty, g_strdup(k->data));
        else emit_changed(self, (const char *)k->data, NULL);
    }
    g_list_free(keys);
    g_hash_table_remove_all(self->values);
}

/* ============================================================
 * Reactivity: watch / effects
 * ============================================================ */

guint ocws_store_watch(OcwsStore *self, const char *key,
                       OcwsWatchFn cb, gpointer data, GDestroyNotify notify) {
    g_return_val_if_fail(OCWS_IS_STORE(self) && cb, 0);
    char *sig = key ? g_strdup_printf("changed::%s", key) : g_strdup("changed");
    guint id = g_signal_connect_data(self, sig, G_CALLBACK(cb), data,
                                     (GClosureNotify)notify, 0);
    g_free(sig);
    return id;
}

void ocws_store_unwatch(OcwsStore *self, guint id) {
    g_return_if_fail(OCWS_IS_STORE(self));
    if (id) g_signal_handler_disconnect(self, id);
}

/* ============================================================
 * Derived / computed values
 * ============================================================ */

void ocws_store_computed(OcwsStore *self, const char *name,
                         const char **deps, gsize n_deps,
                         OcwsComputeFn fn, gpointer data, GDestroyNotify notify) {
    g_return_if_fail(OCWS_IS_STORE(self) && name && fn);
    ComputedSpec *spec = g_new0(ComputedSpec, 1);
    spec->n_deps = n_deps;
    spec->deps = g_new0(char *, n_deps ? n_deps : 1);
    for (gsize i = 0; i < n_deps; i++) spec->deps[i] = g_strdup(deps[i]);
    spec->fn = fn;
    spec->data = data;
    spec->notify = notify;
    g_hash_table_insert(self->computed, g_strdup(name), spec);
    /* initial compute */
    fn(self, name, data);
}

/* ============================================================
 * Batching
 * ============================================================ */

void ocws_store_batch_begin(OcwsStore *self) {
    g_return_if_fail(OCWS_IS_STORE(self));
    self->in_batch = TRUE;
}

void ocws_store_batch_commit(OcwsStore *self) {
    g_return_if_fail(OCWS_IS_STORE(self));
    if (!self->in_batch) return;
    self->in_batch = FALSE;

    /* Drain dirty set: emit + recompute, looping in case computeds
     * add new dirty keys (chained computeds). */
    while (g_hash_table_size(self->dirty) > 0) {
        GList *keys = g_hash_table_get_keys(self->dirty);
        g_hash_table_remove_all(self->dirty);
        for (GList *k = keys; k; k = k->next) {
            const char *key = (const char *)k->data;
            GValue *v = g_hash_table_lookup(self->values, key);
            emit_changed(self, key, v);   /* v may be NULL (remove/clear) */
            trigger_computeds(self, key);
        }
        g_list_free(keys);
    }
}

/* ============================================================
 * Persistence (type-tagged flat file)
 *   b:key=0/1
 *   i:key=<int>
 *   d:key=<double>
 *   s:key=<string>
 * ============================================================ */

gboolean ocws_store_save(OcwsStore *self, const char *path) {
    g_return_val_if_fail(OCWS_IS_STORE(self) && path, FALSE);
    FILE *f = fopen(path, "w");
    if (!f) return FALSE;
    GHashTableIter it;
    char *key;
    GValue *v;
    g_hash_table_iter_init(&it, self->values);
    while (g_hash_table_iter_next(&it, (void **)&key, (void **)&v)) {
        if (G_VALUE_HOLDS_BOOLEAN(v))
            fprintf(f, "b:%s=%d\n", key, g_value_get_boolean(v));
        else if (G_VALUE_HOLDS_INT(v))
            fprintf(f, "i:%s=%d\n", key, g_value_get_int(v));
        else if (G_VALUE_HOLDS_DOUBLE(v))
            fprintf(f, "d:%s=%.17g\n", key, g_value_get_double(v));
        else if (G_VALUE_HOLDS_STRING(v))
            fprintf(f, "s:%s=%s\n", key, g_value_get_string(v));
    }
    fclose(f);
    return TRUE;
}

gboolean ocws_store_load(OcwsStore *self, const char *path) {
    g_return_val_if_fail(OCWS_IS_STORE(self) && path, FALSE);
    FILE *f = fopen(path, "r");
    if (!f) return FALSE;
    char line[65536];
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        if (len < 3 || line[1] != ':') continue;
        char type = line[0];
        char *eq = strchr(line + 2, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = line + 2;
        char *val = eq + 1;
        switch (type) {
        case 'b': ocws_store_set_bool(self, key, strcmp(val, "1") == 0); break;
        case 'i': ocws_store_set_int(self, key, atoi(val)); break;
        case 'd': ocws_store_set_double(self, key, g_ascii_strtod(val, NULL)); break;
        case 's': ocws_store_set_string(self, key, val); break;
        default: break;
        }
    }
    fclose(f);
    return TRUE;
}

/* ============================================================
 * GTK convenience bindings
 * ============================================================ */

static void on_label_changed(OcwsStore *self, const char *key,
                             const GValue *v, GtkLabel *label) {
    (void)self; (void)key;
    if (!v) return;
    if (G_VALUE_HOLDS_STRING(v)) {
        gtk_label_set_text(label, g_value_get_string(v));
    } else {
        char *s = g_strdup_value_contents(v);
        gtk_label_set_text(label, s);
        g_free(s);
    }
}

void ocws_store_bind_label_store(OcwsStore *self, GtkLabel *label, const char *key) {
    char *cur = ocws_store_get_string(self, key, NULL);
    if (cur) { gtk_label_set_text(label, cur); g_free(cur); }
    char *sig = g_strdup_printf("changed::%s", key);
    g_signal_connect_object(self, sig, G_CALLBACK(on_label_changed),
                            G_OBJECT(label), 0);
    g_free(sig);
}

void ocws_store_bind_label(GtkLabel *label, const char *key) {
    ocws_store_bind_label_store(ocws_store_default(), label, key);
}

static void on_switch_store(OcwsStore *self, const char *key,
                            const GValue *v, GtkSwitch *sw) {
    (void)self; (void)key;
    if (v && G_VALUE_HOLDS_BOOLEAN(v))
        gtk_switch_set_active(sw, g_value_get_boolean(v));
}

static void on_switch_active(GtkSwitch *sw, GParamSpec *pspec, gpointer key) {
    (void)pspec;
    ocws_store_set_bool(ocws_store_default(), (const char *)key,
                        gtk_switch_get_active(sw));
}

void ocws_store_bind_switch_store(OcwsStore *self, GtkSwitch *sw, const char *key) {
    gtk_switch_set_active(sw, ocws_store_get_bool(self, key, FALSE));

    char *sig = g_strdup_printf("changed::%s", key);
    g_signal_connect_object(self, sig, G_CALLBACK(on_switch_store),
                            G_OBJECT(sw), 0);
    g_free(sig);

    /* widget -> store: closure carries the key */
    g_signal_connect_data(sw, "notify::active", G_CALLBACK(on_switch_active),
                          g_strdup(key), closure_free_str, 0);
}

void ocws_store_bind_switch(GtkSwitch *sw, const char *key) {
    ocws_store_bind_switch_store(ocws_store_default(), sw, key);
}

static void on_scale_store(OcwsStore *self, const char *key,
                           const GValue *v, GtkScale *scale) {
    (void)self; (void)key;
    if (v && G_VALUE_HOLDS_INT(v))
        gtk_range_set_value(GTK_RANGE(scale), g_value_get_int(v));
}

static void on_scale_value(GtkRange *r, GParamSpec *pspec, gpointer key) {
    (void)pspec;
    ocws_store_set_int(ocws_store_default(), (const char *)key,
                       (gint)gtk_range_get_value(r));
}

void ocws_store_bind_scale_store(OcwsStore *self, GtkScale *scale, const char *key) {
    gtk_range_set_value(GTK_RANGE(scale), ocws_store_get_int(self, key, 0));

    char *sig = g_strdup_printf("changed::%s", key);
    g_signal_connect_object(self, sig, G_CALLBACK(on_scale_store),
                            G_OBJECT(scale), 0);
    g_free(sig);

    g_signal_connect_data(scale, "notify::value", G_CALLBACK(on_scale_value),
                          g_strdup(key), closure_free_str, 0);
}

void ocws_store_bind_scale(GtkScale *scale, const char *key) {
    ocws_store_bind_scale_store(ocws_store_default(), scale, key);
}

/* ============================================================
 * Generic two-way binding to any GObject property
 * ============================================================ */

static void bind_apply_forward(BindCtx *ctx, const GValue *src) {
    if (!src) return;
    GValue dst = G_VALUE_INIT;
    g_value_init(&dst, G_VALUE_TYPE(src));
    gboolean ok;
    if (ctx->fwd) ok = ctx->fwd(src, &dst, ctx->data);
    else ok = g_value_transform(src, &dst);
    if (!ok) { g_value_unset(&dst); return; }
    ctx->updating = TRUE;
    g_object_set_property(ctx->target, ctx->prop, &dst);
    ctx->updating = FALSE;
    g_value_unset(&dst);
}

static void on_bind_forward(OcwsStore *self, const char *key,
                            const GValue *v, BindCtx *ctx) {
    (void)self; (void)key;
    if (ctx->updating) return;
    bind_apply_forward(ctx, v);
}

static void on_bind_backward(GObject *target, GParamSpec *pspec, BindCtx *ctx) {
    (void)target; (void)pspec;
    if (ctx->updating) return;
    if (!(ctx->flags & G_BINDING_BIDIRECTIONAL)) return;
    GValue src = G_VALUE_INIT;
    g_object_get_property(ctx->target, ctx->prop, &src);
    GValue dst = G_VALUE_INIT;
    g_value_init(&dst, G_VALUE_TYPE(&src));
    gboolean ok;
    if (ctx->bwd) ok = ctx->bwd(&src, &dst, ctx->data);
    else ok = g_value_transform(&src, &dst);
    g_value_unset(&src);
    if (!ok) { g_value_unset(&dst); return; }
    ctx->updating = TRUE;
    ocws_store_set_value(ctx->store, ctx->key, &dst);
    ctx->updating = FALSE;
    g_value_unset(&dst);
}

gpointer ocws_store_bind(OcwsStore *self, const char *key,
                         GObject *target, const char *prop,
                         GBindingFlags flags,
                         OcwsTransform fwd, OcwsTransform bwd, gpointer data) {
    g_return_val_if_fail(OCWS_IS_STORE(self) && key && target && prop, NULL);
    BindCtx *ctx = g_new0(BindCtx, 1);
    ctx->store = self;
    ctx->key = g_strdup(key);
    ctx->target = g_object_ref(target);
    ctx->prop = g_strdup(prop);
    ctx->flags = flags;
    ctx->fwd = fwd;
    ctx->bwd = bwd;
    ctx->data = data;

    char *sig = g_strdup_printf("changed::%s", key);
    ctx->fwd_id = g_signal_connect_data(self, sig, G_CALLBACK(on_bind_forward),
                                        ctx, NULL, 0);
    g_free(sig);

    if (flags & G_BINDING_BIDIRECTIONAL) {
        char *psig = g_strdup_printf("notify::%s", prop);
        ctx->bwd_id = g_signal_connect_data(target, psig,
                                            G_CALLBACK(on_bind_backward),
                                            ctx, NULL, 0);
        g_free(psig);
    }

    /* initial push store -> target */
    GValue v = G_VALUE_INIT;
    if (ocws_store_get_value(self, key, &v)) {
        bind_apply_forward(ctx, &v);
        g_value_unset(&v);
    }
    return ctx;
}

void ocws_store_unbind(gpointer handle) {
    BindCtx *ctx = handle;
    if (!ctx) return;
    if (ctx->fwd_id) g_signal_handler_disconnect(ctx->store, ctx->fwd_id);
    if (ctx->bwd_id) g_signal_handler_disconnect(ctx->target, ctx->bwd_id);
    g_object_unref(ctx->target);
    g_free(ctx->key);
    g_free(ctx->prop);
    g_free(ctx);
}
