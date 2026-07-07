#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include "../libocws/easing.h"
#include "../libocws/sysfs.h"

static char opt_device[64] = "";
static int opt_step = 5;
static int opt_duration = 200;
static int opt_interval = 500;
static char opt_format[32] = "sh";

static int get_max_brightness(void) {
    return sysfs_read_device_int("backlight", opt_device[0] ? opt_device : NULL,
                                 "max_brightness", -1);
}

static int get_brightness(void) {
    return sysfs_read_device_int("backlight", opt_device[0] ? opt_device : NULL,
                                 "brightness", -1);
}

static int set_brightness_raw(int value) {
    return sysfs_write_device_int("backlight", opt_device[0] ? opt_device : NULL,
                                  "brightness", value);
}

static void apply_brightness(int value, void *ctx) {
    (void)ctx;
    set_brightness_raw(value);
}

static void animate_to(int target, int duration_ms) {
    int max_b = get_max_brightness();
    if (max_b <= 0) { set_brightness_raw(target); return; }

    int cur = get_brightness();
    if (cur < 0) cur = target;

    animate_int(cur, target, duration_ms, 8, 0, max_b, apply_brightness, NULL);
}

static void pct(int percent) {
    int max_b = get_max_brightness();
    if (max_b <= 0) { fprintf(stderr, "error: no backlight found\n"); return; }
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    int target = (max_b * percent) / 100;
    animate_to(target, opt_duration);
}

static void adjust(int delta) {
    int max_b = get_max_brightness();
    if (max_b <= 0) { fprintf(stderr, "error: no backlight found\n"); return; }
    int cur = get_brightness();
    if (cur < 0) cur = 0;

    int step = (max_b * opt_step) / 100;
    if (step < 1) step = 1;
    int target = cur + (delta > 0 ? step : -step);

    if (target < 0) target = 0;
    if (target > max_b) target = max_b;

    animate_to(target, opt_duration);
}

static void print_state(int pct_val, int cur, int max_b) {
    if (strcmp(opt_format, "json") == 0) {
        printf("{\"brightness\": %d, \"raw\": %d, \"max\": %d}\n", pct_val, cur, max_b);
    } else {
        printf("BRIGHTNESS=%d\n", pct_val);
        printf("BRIGHTNESS_RAW=%d\n", cur);
        printf("BRIGHTNESS_MAX=%d\n", max_b);
    }
    fflush(stdout);
}

static void show(void) {
    int max_b = get_max_brightness();
    int cur = get_brightness();
    if (max_b <= 0 || cur < 0) {
        fprintf(stderr, "error: no backlight found\n");
        return;
    }
    int pct_val = (cur * 100) / max_b;
    print_state(pct_val, cur, max_b);
}

static void monitor(void) {
    int last = -1;
    while (1) {
        int cur = get_brightness();
        if (cur >= 0 && cur != last) {
            int max_b = get_max_brightness();
            int pct_val = max_b > 0 ? (cur * 100) / max_b : 0;
            print_state(pct_val, cur, max_b);
            last = cur;
        }
        usleep(opt_interval * 1000);
    }
}

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [options] <command> [args]\n\n"
        "Smooth hardware backlight control with animated transitions.\n\n"
        "Options:\n"
        "  -d, --device <dev>   Backlight device (e.g. intel_backlight, auto-detected if omitted)\n"
        "  -s, --step <pct>     Brightness step for up/down (default: 5)\n"
        "  -t, --duration <ms>  Animation duration in ms (default: 200, 0=disable)\n"
        "  -i, --interval <ms>  Polling interval for monitor (default: 500)\n"
        "  -f, --format <fmt>   Output format: sh, json (default: sh)\n"
        "  -h, --help           Show this help\n\n"
        "Commands:\n"
        "  get              Show current brightness\n"
        "  set <0-100>      Set brightness with smooth animation\n"
        "  up               Increase by step\n"
        "  down             Decrease by step\n"
        "  min              Set to 0%%\n"
        "  max              Set to 100%%\n"
        "  monitor          Stream brightness changes continuously\n"
        "  raw-get          Get raw brightness value\n"
        "  raw-set <val>    Set raw brightness value\n\n"
        "Examples:\n"
        "  %s --device intel_backlight get\n"
        "  %s --format json monitor\n"
        "  %s --duration 0 set 50\n",
        prog, prog, prog, prog);
}

int main(int argc, char *argv[]) {
    static struct option long_options[] = {
        {"device", required_argument, 0, 'd'},
        {"step", required_argument, 0, 's'},
        {"duration", required_argument, 0, 't'},
        {"interval", required_argument, 0, 'i'},
        {"format", required_argument, 0, 'f'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int opt_index = 0;
    while ((opt = getopt_long(argc, argv, "d:s:t:i:f:h", long_options, &opt_index)) != -1) {
        switch (opt) {
            case 'd': strncpy(opt_device, optarg, sizeof(opt_device) - 1); break;
            case 's': opt_step = atoi(optarg); break;
            case 't': opt_duration = atoi(optarg); break;
            case 'i': opt_interval = atoi(optarg); break;
            case 'f': strncpy(opt_format, optarg, sizeof(opt_format) - 1); break;
            case 'h': usage(argv[0]); return 0;
            default: usage(argv[0]); return 1;
        }
    }

    if (optind >= argc) { usage(argv[0]); return 1; }

    const char *cmd = argv[optind];

    if (strcmp(cmd, "get") == 0) show();
    else if (strcmp(cmd, "set") == 0 && optind + 1 < argc) pct(atoi(argv[optind + 1]));
    else if (strcmp(cmd, "up") == 0) adjust(1);
    else if (strcmp(cmd, "down") == 0) adjust(-1);
    else if (strcmp(cmd, "min") == 0) pct(0);
    else if (strcmp(cmd, "max") == 0) pct(100);
    else if (strcmp(cmd, "monitor") == 0) monitor();
    else if (strcmp(cmd, "raw-get") == 0) {
        int v = get_brightness();
        if (v >= 0) printf("%d\n", v);
        else { fprintf(stderr, "error: no backlight\n"); return 1; }
    }
    else if (strcmp(cmd, "raw-set") == 0 && optind + 1 < argc) {
        if (set_brightness_raw(atoi(argv[optind + 1])) != 0) {
            fprintf(stderr, "error: failed to set brightness\n"); return 1;
        }
    }
    else { usage(argv[0]); return 1; }

    return 0;
}
