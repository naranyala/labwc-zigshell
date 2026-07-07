/*
 * test_store.c — Comprehensive test suite for libocws-store.
 *
 * Run with:  zig build -Drelease=false test-store
 * Or:        ./zig-out/bin/test_store
 *
 * Core reactivity tests need no display. GTK widget-binding tests run only
 * when a display is available (gtk_init_check), and are skipped otherwise.
 */

#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>
#include "libocws/store.h"
#include "libocws/store-widgets.h"

/* ============================================================
 * Test fixture: a plain GObject with an int "count" property.
 * Used to exercise the generic two-way binding without a display.
 * ============================================================ */

typedef struct _TestObj      TestObj;
typedef struct _TestObjClass TestObjClass;

struct _TestObj { GObject parent; gint count; };
struct _TestObjClass { GObjectClass parent; };
#define TEST_OBJ(t) ((TestObj *)(t))

static GType test_obj_get_type(void);
G_DEFINE_TYPE(TestObj, test_obj, G_TYPE_OBJECT)

static void test_obj_set_property(GObject *o, guint id, const GValue *v, GParamSpec *p) {
    (void)p;
    if (id == 1) TEST_OBJ(o)->count = g_value_get_int(v);
}
static void test_obj_get_property(GObject *o, guint id, GValue *v, GParamSpec *p) {
    (void)p;
    if (id == 1) g_value_set_int(v, TEST_OBJ(o)->count);
}
static void test_obj_class_init(TestObjClass *c) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(c);
    gobject_class->set_property = test_obj_set_property;
    gobject_class->get_property = test_obj_get_property;
    GParamSpec *ps = g_param_spec_int("count", "count", "count",
                                      G_MININT, G_MAXINT, 0, G_PARAM_READWRITE);
    g_object_class_install_property(gobject_class, 1, ps);
}
static void test_obj_init(TestObj *o) { (void)o; }

/* ============================================================
 * Helpers for signal counting
 * ============================================================ */

typedef struct {
    int           total;
    int           for_key;
    gboolean      last_bool;
    gint          last_int;
    char         *last_string;
} Counter;

static void on_any_changed(OcwsStore *s, const char *key, const GValue *v, Counter *c) {
    (void)s;
    c->total++;
    if (key && strcmp(key, "watchme") == 0) c->for_key++;
    if (v && G_VALUE_HOLDS_BOOLEAN(v)) c->last_bool = g_value_get_boolean(v);
    if (v && G_VALUE_HOLDS_INT(v))     c->last_int  = g_value_get_int(v);
    if (v && G_VALUE_HOLDS_STRING(v))  { g_free(c->last_string); c->last_string = g_strdup(g_value_get_string(v)); }
}

/* ============================================================
 * 1. Typed set/get + defaults
 * ============================================================ */
static void test_typed(void) {
    OcwsStore *s = ocws_store_new();

    g_assert_false(ocws_store_has(s, "missing"));
    /* defaults when absent */
    g_assert_cmpint(ocws_store_get_int(s, "x", 42), ==, 42);
    g_assert_cmpint(ocws_store_get_bool(s, "x", TRUE), ==, TRUE);
    g_assert_cmpfloat(ocws_store_get_double(s, "x", 1.5), ==, 1.5);
    char *d = ocws_store_get_string(s, "x", "def");
    g_assert_cmpstr(d, ==, "def"); g_free(d);

    ocws_store_set_bool(s, "b", TRUE);
    ocws_store_set_int(s, "i", 7);
    ocws_store_set_double(s, "d", 3.25);
    ocws_store_set_string(s, "str", "hello");

    g_assert_true(ocws_store_has(s, "b"));
    g_assert_cmpint(ocws_store_get_bool(s, "b", FALSE), ==, TRUE);
    g_assert_cmpint(ocws_store_get_int(s, "i", 0), ==, 7);
    g_assert_cmpfloat(ocws_store_get_double(s, "d", 0), ==, 3.25);
    g_assert_cmpstr(ocws_store_get_string(s, "str", ""), ==, "hello");

    g_object_unref(s);
}

/* ============================================================
 * 2. has / remove / clear
 * ============================================================ */
static void test_remove_clear(void) {
    OcwsStore *s = ocws_store_new();
    ocws_store_set_int(s, "a", 1);
    ocws_store_set_int(s, "b", 2);
    g_assert_true(ocws_store_has(s, "a"));

    ocws_store_remove(s, "a");
    g_assert_false(ocws_store_has(s, "a"));
    g_assert_true(ocws_store_has(s, "b"));

    ocws_store_clear(s);
    g_assert_false(ocws_store_has(s, "b"));
    g_object_unref(s);
}

/* ============================================================
 * 3. Generic "changed" signal carries key + value
 * ============================================================ */
static void test_changed_signal(void) {
    OcwsStore *s = ocws_store_new();
    Counter c = {0};
    ocws_store_watch(s, NULL, (OcwsWatchFn)on_any_changed, &c, NULL);

    ocws_store_set_int(s, "k", 99);
    g_assert_cmpint(c.total, ==, 1);
    g_assert_cmpint(c.last_int, ==, 99);

    ocws_store_set_string(s, "k", "abc");
    g_assert_cmpint(c.total, ==, 2);
    g_assert_cmpstr(c.last_string, ==, "abc");

    g_object_unref(s);
    g_free(c.last_string);
}

/* ============================================================
 * 4. Detailed "changed::key" fires only for that key (selector)
 * ============================================================ */
static void test_selector(void) {
    OcwsStore *s = ocws_store_new();
    Counter a = {0}, b = {0};
    ocws_store_watch(s, "alpha", (OcwsWatchFn)on_any_changed, &a, NULL);
    ocws_store_watch(s, "beta",  (OcwsWatchFn)on_any_changed, &b, NULL);

    ocws_store_set_int(s, "alpha", 1);
    g_assert_cmpint(a.total, ==, 1);
    g_assert_cmpint(b.total, ==, 0);

    ocws_store_set_int(s, "beta", 2);
    g_assert_cmpint(a.total, ==, 1);
    g_assert_cmpint(b.total, ==, 1);

    /* an unrelated key touches neither */
    ocws_store_set_int(s, "gamma", 3);
    g_assert_cmpint(a.total, ==, 1);
    g_assert_cmpint(b.total, ==, 1);

    g_object_unref(s);
}

/* ============================================================
 * 5. watch / unwatch
 * ============================================================ */
static void test_watch_unwatch(void) {
    OcwsStore *s = ocws_store_new();
    Counter c = {0};
    guint id = ocws_store_watch(s, "watchme", (OcwsWatchFn)on_any_changed, &c, NULL);

    ocws_store_set_int(s, "watchme", 1);
    g_assert_cmpint(c.for_key, ==, 1);

    ocws_store_unwatch(s, id);
    ocws_store_set_int(s, "watchme", 2);
    g_assert_cmpint(c.for_key, ==, 1);  /* no longer called */

    g_object_unref(s);
}

/* ============================================================
 * 6. Computed / derived values (+ chaining)
 * ============================================================ */
static void compute_vol_label(OcwsStore *s, const char *name, gpointer data) {
    (void)data;
    gboolean muted = ocws_store_get_bool(s, "muted", FALSE);
    gint vol = ocws_store_get_int(s, "volume", 0);
    char *label = muted ? g_strdup("Muted")
                        : g_strdup_printf("Vol %d", vol);
    ocws_store_set_string(s, name, label);
    g_free(label);
}
static void compute_vol_upper(OcwsStore *s, const char *name, gpointer data) {
    (void)data;
    char *l = ocws_store_get_string(s, "vol_label", "");
    char *up = g_ascii_strup(l ? l : "", -1);
    ocws_store_set_string(s, name, up);
    g_free(up); g_free(l);
}

static void test_computed(void) {
    OcwsStore *s = ocws_store_new();
    const char *deps_vol[] = {"muted", "volume"};
    ocws_store_computed(s, "vol_label", deps_vol, 2, compute_vol_label, NULL, NULL);
    const char *deps_up[] = {"vol_label"};
    ocws_store_computed(s, "vol_upper", deps_up, 1, compute_vol_upper, NULL, NULL);

    ocws_store_set_int(s, "volume", 5);
    ocws_store_set_bool(s, "muted", FALSE);
    g_assert_cmpstr(ocws_store_get_string(s, "vol_label", ""), ==, "Vol 5");
    g_assert_cmpstr(ocws_store_get_string(s, "vol_upper", ""), ==, "VOL 5");

    ocws_store_set_bool(s, "muted", TRUE);
    g_assert_cmpstr(ocws_store_get_string(s, "vol_label", ""), ==, "Muted");
    /* chained compute: vol_upper recomputes from vol_label */
    g_assert_cmpstr(ocws_store_get_string(s, "vol_upper", ""), ==, "MUTED");

    ocws_store_set_int(s, "volume", 8);
    g_assert_cmpstr(ocws_store_get_string(s, "vol_label", ""), ==, "Muted");
    ocws_store_set_bool(s, "muted", FALSE);
    g_assert_cmpstr(ocws_store_get_string(s, "vol_label", ""), ==, "Vol 8");
    g_assert_cmpstr(ocws_store_get_string(s, "vol_upper", ""), ==, "VOL 8");

    g_object_unref(s);
}

/* ============================================================
 * 7. Batching coalesces notifications
 * ============================================================ */
static gboolean emitted_during_batch = FALSE;
static void flag_on_change(OcwsStore *s, const char *k, const GValue *v, gpointer d) {
    (void)s; (void)k; (void)v; (void)d; emitted_during_batch = TRUE;
}

static void test_batch(void) {
    OcwsStore *s = ocws_store_new();
    Counter c = {0};
    ocws_store_watch(s, NULL, (OcwsWatchFn)on_any_changed, &c, NULL);
    /* also a raw signal hook to detect any emission during batch */
    gulong hook = g_signal_connect(s, "changed", G_CALLBACK(flag_on_change), NULL);

    ocws_store_batch_begin(s);
    emitted_during_batch = FALSE;
    ocws_store_set_int(s, "a", 1);
    ocws_store_set_int(s, "b", 2);
    ocws_store_set_int(s, "a", 3);   /* same key twice */
    g_assert_false(emitted_during_batch);
    g_assert_cmpint(c.total, ==, 0);  /* nothing notified yet */

    ocws_store_batch_commit(s);
    /* commit notifies each distinct touched key exactly once */
    g_assert_cmpint(c.total, ==, 2);  /* a and b */
    g_assert_cmpint(ocws_store_get_int(s, "a", 0), ==, 3);
    g_assert_cmpint(ocws_store_get_int(s, "b", 0), ==, 2);

    g_signal_handler_disconnect(s, hook);
    g_object_unref(s);
}

/* ============================================================
 * 8. Generic two-way binding to a GObject property (+ transforms)
 * ============================================================ */
static gboolean fwd_x10(const GValue *src, GValue *dst, gpointer d) {
    (void)d; g_value_set_int(dst, g_value_get_int(src) * 10); return TRUE;
}
static gboolean bwd_d10(const GValue *src, GValue *dst, gpointer d) {
    (void)d; g_value_set_int(dst, g_value_get_int(src) / 10); return TRUE;
}

static void test_generic_bind(void) {
    OcwsStore *s = ocws_store_new();
    TestObj *o = g_object_new(test_obj_get_type(), NULL);

    gpointer h = ocws_store_bind(s, "vol", G_OBJECT(o), "count",
                                 G_BINDING_BIDIRECTIONAL,
                                 fwd_x10, bwd_d10, NULL);
    g_assert_nonnull(h);

    /* store -> target (with forward transform x10) */
    ocws_store_set_int(s, "vol", 5);
    g_assert_cmpint(o->count, ==, 50);

    /* target -> store (with backward transform /10) */
    g_object_set(o, "count", 30, NULL);
    g_assert_cmpint(ocws_store_get_int(s, "vol", 0), ==, 3);

    ocws_store_unbind(h);
    /* after unbind, store changes no longer reach the target */
    ocws_store_set_int(s, "vol", 9);
    g_assert_cmpint(o->count, ==, 30);

    g_object_unref(o);
    g_object_unref(s);
}

/* ============================================================
 * 9. Persistence round-trip (all types)
 * ============================================================ */
static void test_persist(void) {
    OcwsStore *s = ocws_store_new();
    ocws_store_set_bool(s, "b", TRUE);
    ocws_store_set_int(s, "i", -12345);
    ocws_store_set_double(s, "d", 2.718281828);
    ocws_store_set_string(s, "s", "persist me");

    char tmpl[] = "/tmp/ocws-store-test-XXXXXX";
    int fd = g_mkstemp(tmpl);
    g_assert_cmpint(fd, >=, 0);
    close(fd);

    g_assert_true(ocws_store_save(s, tmpl));
    g_object_unref(s);

    OcwsStore *s2 = ocws_store_new();
    g_assert_true(ocws_store_load(s2, tmpl));
    g_assert_cmpint(ocws_store_get_bool(s2, "b", FALSE), ==, TRUE);
    g_assert_cmpint(ocws_store_get_int(s2, "i", 0), ==, -12345);
    g_assert_cmpfloat(ocws_store_get_double(s2, "d", 0), ==, 2.718281828);
    g_assert_cmpstr(ocws_store_get_string(s2, "s", ""), ==, "persist me");

    g_object_unref(s2);
    unlink(tmpl);
}

/* ============================================================
 * 10. GTK widget bindings (display-gated)
 * ============================================================ */
static void test_gtk_bind_label(void) {
    OcwsStore *s = ocws_store_default();
    ocws_store_clear(s);
    GtkWidget *label = gtk_label_new("init");
    ocws_store_bind_label(GTK_LABEL(label), "title");
    ocws_store_set_string(s, "title", "Reactive!");
    g_assert_cmpstr(gtk_label_get_text(GTK_LABEL(label)), ==, "Reactive!");
    gtk_widget_destroy(label);
}

static void test_gtk_bind_switch(void) {
    OcwsStore *s = ocws_store_default();
    ocws_store_clear(s);
    GtkWidget *sw = gtk_switch_new();
    ocws_store_bind_switch(GTK_SWITCH(sw), "bt");
    /* store -> widget */
    ocws_store_set_bool(s, "bt", TRUE);
    g_assert_true(gtk_switch_get_active(GTK_SWITCH(sw)));
    /* widget -> store */
    gtk_switch_set_active(GTK_SWITCH(sw), FALSE);
    g_assert_false(ocws_store_get_bool(s, "bt", TRUE));
    gtk_widget_destroy(sw);
}

static void test_gtk_bind_scale(void) {
    OcwsStore *s = ocws_store_default();
    ocws_store_clear(s);
    GtkWidget *scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
    ocws_store_bind_scale(GTK_SCALE(scale), "bright");
    ocws_store_set_int(s, "bright", 73);
    g_assert_cmpint((gint)gtk_range_get_value(GTK_RANGE(scale)), ==, 73);
    gtk_range_set_value(GTK_RANGE(scale), 21);
    g_assert_cmpint(ocws_store_get_int(s, "bright", 0), ==, 21);
    gtk_widget_destroy(scale);
}

/* ============================================================
 * main
 * ============================================================ */
int main(int argc, char **argv) {
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/store/typed_set_get", test_typed);
    g_test_add_func("/store/remove_clear", test_remove_clear);
    g_test_add_func("/store/changed_signal", test_changed_signal);
    g_test_add_func("/store/selector", test_selector);
    g_test_add_func("/store/watch_unwatch", test_watch_unwatch);
    g_test_add_func("/store/computed_chained", test_computed);
    g_test_add_func("/store/batch", test_batch);
    g_test_add_func("/store/generic_bind", test_generic_bind);
    g_test_add_func("/store/persist", test_persist);

    /* GTK widget-binding tests need a usable display + GTK init. They are
     * opt-in (OCWS_RUN_GTK_TESTS=1) because initializing GTK in a headless or
     * misconfigured environment can abort the whole process. The core
     * reactivity suite above never touches GTK. */
    if (getenv("OCWS_RUN_GTK_TESTS")) {
        if (gtk_init_check(NULL, NULL)) {
            g_test_add_func("/store/gtk_bind_label", test_gtk_bind_label);
            g_test_add_func("/store/gtk_bind_switch", test_gtk_bind_switch);
            g_test_add_func("/store/gtk_bind_scale", test_gtk_bind_scale);
        } else {
            g_test_message("GTK init failed — skipping GTK widget binding tests");
        }
    } else {
        g_test_message("GTK widget tests skipped (set OCWS_RUN_GTK_TESTS=1 to enable)");
    }

    return g_test_run();
}
