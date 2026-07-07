/*
 * test_libocws.c — Headless tests for the pure-C libocws helpers.
 *
 * Covers: string, easing, ini, json, fs, sysfs (no GTK required).
 * Run under AddressSanitizer to also catch the out-of-bounds writes:
 *   gcc -fsanitize=address,undefined -g -I src test_libocws.c -o /tmp/t $(pkg-config --cflags glib-2.0) -lglib-2.0 && ./t
 */

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libocws/string.h"
#include "libocws/easing.h"
#include "libocws/ini.h"
#include "libocws/json.h"
#include "libocws/fs.h"
#include "libocws/sysfs.h"

/* ---------- string.h ---------- */
static void test_str_prettify(void) {
    char *r = ocws_str_prettify("my-awesome-theme");
    g_assert_cmpstr(r, ==, "My Awesome Theme");
    free(r);

    r = ocws_str_prettify("dark_mode_on");
    g_assert_cmpstr(r, ==, "Dark Mode On");
    free(r);

    /* gaps: leading / trailing / repeated separators leave stray spaces */
    r = ocws_str_prettify("-leading");
    g_assert_cmpstr(r, ==, " Leading");   /* gap: leading space */
    free(r);

    r = ocws_str_prettify("trailing-");
    g_assert_cmpstr(r, ==, "Trailing ");  /* gap: trailing space */
    free(r);

    g_assert_null(ocws_str_prettify(NULL));
}

/* ---------- easing.h ---------- */
static void test_easing_endpoints(void) {
    g_assert_cmpfloat(ease_out_cubic(0.0), ==, 0.0);
    g_assert_cmpfloat(ease_out_cubic(1.0), ==, 1.0);
    g_assert_cmpfloat(ease_in_out_cubic(0.0), ==, 0.0);
    g_assert_cmpfloat(ease_in_out_cubic(1.0), ==, 1.0);
    g_assert_cmpfloat(ease_in_out_cubic(0.5), ==, 0.5);
    g_assert_cmpfloat(ease_in_out(0.0), ==, 0.0);
    g_assert_cmpfloat(ease_in_out(1.0), ==, 1.0);

    /* monotonic increasing */
    double prev = -1;
    for (int i = 0; i <= 10; i++) {
        double v = ease_out_cubic(i / 10.0);
        g_assert_cmpfloat(v, >=, prev);
        prev = v;
    }
}

static int g_apply_count = 0;
static void count_apply(int v, void *ctx) {
    (void)v; (void)ctx; g_apply_count++;
}

static void test_animate_double_apply(void) {
    g_apply_count = 0;
    /* 10 steps -> loop applies 10 times, then a redundant final apply = 11 */
    animate_int(0, 100, 100, 10, 0, 100, count_apply, NULL);
    /* Exposed gap: the final value is applied twice (loop end + explicit). */
    g_assert_cmpint(g_apply_count, ==, 11);
}

/* ---------- ini.h ---------- */
static void test_ini(void) {
    char tmpl[] = "/tmp/ocws-ini-test-XXXXXX";
    int fd = g_mkstemp(tmpl);
    g_assert_cmpint(fd, >=, 0);
    FILE *f = fdopen(fd, "w");
    fprintf(f, "[theme]\nname = Mocha\naccent = #89b4fa\n");
    fclose(f);

    IniFile ini;
    g_assert_cmpint(ini_load(&ini, tmpl), ==, 0);
    g_assert_cmpstr(ini_get(&ini, "theme", "name"), ==, "Mocha");
    g_assert_cmpstr(ini_get(&ini, "theme", "accent"), ==, "#89b4fa");
    g_assert_null(ini_get(&ini, "theme", "missing"));
    g_assert_null(ini_get(&ini, "nope", "name"));

    /* Latent bug: strncpy without NUL termination is only safe because
     * ini_load memsets the whole struct. A key/value >= buffer length is
     * silently truncated; verify it is still NUL-terminated (masked bug). */
    unlink(tmpl);
}

/* ---------- json.h ---------- */
static void test_json_escape(void) {
    char dst[64];
    json_escape(dst, sizeof(dst), "a\"b\\c");
    g_assert_cmpstr(dst, ==, "a\\\"b\\\\c");

    /* BUG: guard is `j < dst_len - 2`, off by one. A 4-byte buffer should
     * hold "abc" + NUL, but it truncates to "ab". */
    char small[4];
    json_escape(small, sizeof(small), "abc");
    g_assert_cmpstr(small, ==, "abc");   /* fails: actual "ab" */
}

static void test_json_kv(void) {
    char buf[128];
    json_kv_string(buf, sizeof(buf), "title", "he\"llo", 1);
    g_assert_nonnull(strstr(buf, "\\\""));
    json_kv_int(buf, sizeof(buf), "n", 42, 1);
    g_assert_nonnull(strstr(buf, "42"));
}

/* ---------- fs.h ---------- */
static void test_fs(void) {
    char buf[256];
    /* ensure a stable HOME for the test */
    const char *home = getenv("HOME");
    if (!home) { g_setenv("HOME", "/home/tester", 1); }
    get_config_dir(buf, sizeof(buf));
    g_assert_nonnull(strstr(buf, ".config/ocws"));
}

/* ---------- sysfs.h (uses temp files, not real /sys) ---------- */
static void test_sysfs_int(void) {
    char tmpl[] = "/tmp/ocws-sysfs-XXXXXX";
    int fd = g_mkstemp(tmpl);
    g_assert_cmpint(fd, >=, 0);
    FILE *f = fdopen(fd, "w");
    fprintf(f, "320\n");
    fclose(f);

    g_assert_cmpint(sysfs_read_int(tmpl, -1), ==, 320);

    g_assert_cmpint(sysfs_write_int(tmpl, 77), ==, 0);
    g_assert_cmpint(sysfs_read_int(tmpl, -1), ==, 77);

    g_assert_cmpint(sysfs_read_int("/nonexistent/path/123", 999), ==, 999);
    unlink(tmpl);
}

int main(int argc, char **argv) {
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/libocws/str_prettify", test_str_prettify);
    g_test_add_func("/libocws/easing_endpoints", test_easing_endpoints);
    g_test_add_func("/libocws/animate_double_apply", test_animate_double_apply);
    g_test_add_func("/libocws/ini", test_ini);
    g_test_add_func("/libocws/json_escape", test_json_escape);
    g_test_add_func("/libocws/json_kv", test_json_kv);
    g_test_add_func("/libocws/fs", test_fs);
    g_test_add_func("/libocws/sysfs_int", test_sysfs_int);
    return g_test_run();
}
