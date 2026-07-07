/*
 * ocws-pkgmgr.c — OCWS Package Manager GUI
 *
 * GTK3 application for:
 *   1. Dependency resolution — check/install system packages
 *   2. Build from source — fetch latest & compile core engines
 *
 * Build:
 *   zig build   (handled by build.zig)
 */

#include <gtk/gtk.h>
#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "utils.h"
#include "ocws-theme-utils.h"
#include "../libocws/gtk.h"

#define APP_ID "org.ocws.pkgmgr"

/* ================================================================
 * Dependency data
 * ================================================================ */

typedef struct {
    const char *name;
    const char *desc;
    const char *category;
    const char *arch_pkg;    /* Arch package name (NULL = build from source) */
    const char *debian_pkg;  /* Debian/Ubuntu package name */
    const char *fedora_pkg;  /* Fedora package name */
    const char *check_cmd;   /* command to verify installed */
} PkgDep;

static const PkgDep DEPS[] = {
    /* Core compositor & bar */
    {"labwc",           "Wayland compositor",            "Core",   "labwc",    "labwc",    "labwc",    "labwc --version"},
    {"sfwbar",          "Status bar for Wayland",        "Core",   "sfwbar",   "sfwbar",   "sfwbar",   "sfwbar --version"},
    {"fuzzel",          "Application launcher",           "Core",   "fuzzel",   "fuzzel",   "fuzzel",   "fuzzel --version"},
    {"gtk-layer-shell", "GTK Layer Shell for Wayland",   "Core",   "gtk-layer-shell", "libgtk-layer-shell-dev", "gtk-layer-shell-devel", NULL},

    /* Shell modes */
    {"dms",             "DankMaterialShell",              "Shells", NULL,       NULL,       NULL,       "dms --version"},
    {"crystal-dock",    "macOS-style dock",              "Shells", "crystal-dock", NULL,    NULL,       "crystal-dock --version"},

    /* Terminal */
    {"foot",            "Wayland terminal emulator",      "Apps",   "foot",     "foot",     "foot",     "foot --version"},

    /* Launchers */
    {"rofi",            "Window switcher & launcher",     "Apps",   "rofi-wayland", "rofi", "rofi",    "rofi -version"},

    /* Notifications */
    {"mako",            "Notification daemon",            "Apps",   "mako",     "mako",     "mako",     "mako --version"},

    /* Wallpaper & display */
    {"swaybg",          "Wallpaper setter",               "Display","swaybg",   "swaybg",   "swaybg",   "swaybg -h"},
    {"wlr-randr",       "Output configuration tool",      "Display","wlr-randr","wlr-randr","wlr-randr","wlr-randr --version"},
    {"gammastep",       "Color temperature daemon",       "Display","gammastep","gammastep","gammastep","gammastep --version"},

    /* Screen & idle */
    {"swaylock",        "Screen locker",                  "System", "swaylock", "swaylock", "swaylock", "swaylock --version"},
    {"swayidle",        "Idle management daemon",         "System", "swayidle", "swayidle", "swayidle", "swayidle --version"},
    {"flameshot",       "Screenshot tool",                "System", "flameshot","flameshot","flameshot","flameshot --version"},

    /* Media */
    {"playerctl",       "Media player controller",        "Media",  "playerctl","playerctl","playerctl","playerctl --version"},

    /* Clipboard */
    {"wl-clipboard",    "Wayland clipboard utilities",    "System", "wl-clipboard","wl-clipboard","wl-clipboard","wl-paste --version"},
    {"cliphist",        "Clipboard history manager",      "System", "cliphist", "cliphist", "cliphist", "cliphist --version"},

    /* Hardware */
    {"brightnessctl",   "Brightness control",             "System", "brightnessctl","brightnessctl","brightnessctl","brightnessctl --version"},

    /* Utilities */
    {"nautilus",        "File manager",                   "Apps",   "nautilus", "nautilus", "nautilus", "nautilus --version"},
    {"networkmanager",  "Network management daemon",      "System", "networkmanager","network-manager","NetworkManager","nmcli --version"},
    {"bluez",           "Bluetooth support",              "System", "bluez",    "bluetooth","bluez",    "bluetoothctl version"},
};

static const int DEP_COUNT = sizeof(DEPS) / sizeof(DEPS[0]);

/* ================================================================
 * Build targets
 * ================================================================ */

typedef struct {
    const char *name;
    const char *desc;
    const char *repo_url;
    const char *build_type; /* "meson" or "make" */
} BuildTarget;

static const BuildTarget BUILDS[] = {
    {"labwc",      "Latest labwc from git",     "https://github.com/labwc/labwc.git",             "meson"},
    {"sfwbar",     "Latest sfwbar from git",    "https://github.com/LBCrion/sfwbar.git",          "meson"},
    {"fuzzel",     "Latest fuzzel from git",    "https://codeberg.org/dnkl/fuzzel.git",            "meson"},
    {"dms",        "Latest DMS from git",       "https://github.com/DankShrine/dms.git",          "make"},
    {"crystal-dock","Latest crystal-dock from git","https://github.com/igrekster/crystal-dock.git","make"},
};

static const int BUILD_COUNT = sizeof(BUILDS) / sizeof(BUILDS[0]);

/* ================================================================
 * Globals
 * ================================================================ */

static GtkTextBuffer *g_log_buffer = NULL;
static GtkWidget *g_log_view = NULL;

/* ================================================================
 * Helpers
 * ================================================================ */

static int check_installed(const char *check_cmd) {
    if (!check_cmd) return -1; /* unknown */
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "%s >/dev/null 2>&1", check_cmd);
    return system(cmd) == 0 ? 1 : 0;
}

static gboolean append_log_idle(gpointer data) {
    char *msg = (char *)data;
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(g_log_buffer, &end);
    gtk_text_buffer_insert(g_log_buffer, &end, msg, -1);
    gtk_text_buffer_insert(g_log_buffer, &end, "\n", -1);
    
    /* Auto-scroll */
    gtk_text_buffer_get_end_iter(g_log_buffer, &end);
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(g_log_view), &end, 0.0, FALSE, 0.0, 1.0);
    g_free(msg);
    return G_SOURCE_REMOVE;
}

static void log_msg(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    char *msg = g_strdup(buf);
    g_idle_add(append_log_idle, msg);
}

static void run_cmd_logged(const char *cmd) {
    log_msg("$ %s", cmd);
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        log_msg("ERROR: Failed to execute command");
        return;
    }
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0;
        log_msg("  %s", line);
    }
    int ret = pclose(fp);
    if (WIFEXITED(ret)) {
        log_msg("Exit code: %d", WEXITSTATUS(ret));
    }
}

/* ================================================================
 * Dependency Scanner
 * ================================================================ */

static void on_scan_deps(GtkWidget *widget, gpointer data) {
    (void)widget; (void)data;
    log_msg("=== Scanning Dependencies ===");

    int found = 0, missing = 0, unknown = 0;

    for (int i = 0; i < DEP_COUNT; i++) {
        int status = check_installed(DEPS[i].check_cmd);
        const char *icon;
        if (status == 1) {
            icon = "✓";
            found++;
        } else if (status == 0) {
            icon = "✗";
            missing++;
        } else {
            icon = "?";
            unknown++;
        }
        log_msg("[%s] %-20s %s", icon, DEPS[i].name, DEPS[i].desc);
    }

    log_msg("");
    log_msg("Result: %d found, %d missing, %d unknown", found, missing, unknown);

    if (missing > 0) {
        log_msg("Click 'Install Missing' to install missing packages via your package manager.");
    }
}

/* ================================================================
 * Install Missing (runs distro script)
 * ================================================================ */

static void on_install_missing(GtkWidget *widget, gpointer data) {
    (void)widget; (void)data;
    log_msg("=== Installing Missing Dependencies ===");
    log_msg("Detecting distribution and running installer...");

    /* Detect distro */
    const char *distro = "unknown";
    if (access("/etc/os-release", R_OK) == 0) {
        FILE *fp = popen(". /etc/os-release && echo $ID", "r");
        if (fp) {
            char buf[64] = {0};
            if (fgets(buf, sizeof(buf), fp)) {
                buf[strcspn(buf, "\n")] = 0;
                distro = strdup(buf);
            }
            pclose(fp);
        }
    }
    log_msg("Detected distro: %s", distro);

    char pkgs[2048] = {0};
    gboolean is_arch = (strcmp(distro, "arch") == 0 || strcmp(distro, "manjaro") == 0 || strcmp(distro, "endeavouros") == 0 || access("/usr/bin/pacman", F_OK) == 0);
    gboolean is_debian = (strcmp(distro, "debian") == 0 || strcmp(distro, "ubuntu") == 0 || strcmp(distro, "linuxmint") == 0 || access("/usr/bin/apt", F_OK) == 0);
    gboolean is_fedora = (strcmp(distro, "fedora") == 0 || access("/usr/bin/dnf", F_OK) == 0);

    for (int i = 0; i < DEP_COUNT; i++) {
        if (check_installed(DEPS[i].check_cmd) == 0) {
            const char *pkg_name = NULL;
            if (is_arch) pkg_name = DEPS[i].arch_pkg;
            else if (is_debian) pkg_name = DEPS[i].debian_pkg;
            else if (is_fedora) pkg_name = DEPS[i].fedora_pkg;
            
            if (pkg_name) {
                strcat(pkgs, pkg_name);
                strcat(pkgs, " ");
            } else {
                log_msg("WARN: No package mapping for %s on this distro, or must be built from source.", DEPS[i].name);
            }
        }
    }

    if (strlen(pkgs) > 0) {
        char cmd[2048];
        if (is_arch) {
            snprintf(cmd, sizeof(cmd), "pkexec pacman -S --needed --noconfirm %s 2>&1", pkgs);
        } else if (is_debian) {
            snprintf(cmd, sizeof(cmd), "pkexec apt-get install -y %s 2>&1", pkgs);
        } else if (is_fedora) {
            snprintf(cmd, sizeof(cmd), "pkexec dnf install -y %s 2>&1", pkgs);
        } else {
            log_msg("Unsupported package manager.");
            return;
        }
        
        g_thread_new("install-deps", (GThreadFunc)run_cmd_logged, g_strdup(cmd));
    } else {
        log_msg("No installable missing packages found.");
    }
}

/* ================================================================
 * Build from Source
 * ================================================================ */

typedef struct {
    int index;
    GtkWidget *btn;
    GtkWidget *status_lbl;
} BuildRowData;

static gpointer build_worker(gpointer user_data) {
    BuildRowData *d = (BuildRowData *)user_data;
    const BuildTarget *t = &BUILDS[d->index];

    /* Update UI from main thread */
    gdk_threads_add_idle_full(G_PRIORITY_HIGH,
        (GSourceFunc)gtk_label_set_text,
        d->status_lbl, "Building...");

    log_msg("=== Building %s ===", t->name);
    log_msg("Repository: %s", t->repo_url);

    char build_dir[256];
    snprintf(build_dir, sizeof(build_dir), "/tmp/ocws-build-%s", t->name);

    char cmd[1024];

    /* Clean previous build */
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", build_dir);
    system(cmd);

    /* Clone */
    snprintf(cmd, sizeof(cmd), "git clone --depth=1 '%s' '%s' 2>&1", t->repo_url, build_dir);
    run_cmd_logged(cmd);

    /* Build */
    if (strcmp(t->build_type, "meson") == 0) {
        snprintf(cmd, sizeof(cmd),
            "cd '%s' && meson setup build --prefix=/usr/local --buildtype=release 2>&1 && "
            "ninja -C build 2>&1",
            build_dir);
    } else {
        snprintf(cmd, sizeof(cmd),
            "cd '%s' && make -j$(nproc) 2>&1",
            build_dir);
    }
    run_cmd_logged(cmd);

    /* Install */
    log_msg("Installing (requires sudo)...");
    if (strcmp(t->build_type, "meson") == 0) {
        snprintf(cmd, sizeof(cmd),
            "cd '%s' && pkexec ninja -C build install 2>&1",
            build_dir);
    } else {
        snprintf(cmd, sizeof(cmd),
            "cd '%s' && pkexec make install 2>&1",
            build_dir);
    }
    run_cmd_logged(cmd);

    /* Update UI */
    gdk_threads_add_idle_full(G_PRIORITY_HIGH,
        (GSourceFunc)gtk_label_set_text,
        d->status_lbl, "Done");

    log_msg("=== %s build complete ===", t->name);
    return NULL;
}

static void on_build_clicked(GtkWidget *widget, gpointer data) {
    BuildRowData *d = (BuildRowData *)data;
    gtk_widget_set_sensitive(d->btn, FALSE);
    gtk_label_set_text(GTK_LABEL(d->status_lbl), "Building...");

    g_thread_new("build", build_worker, d);
}

/* ================================================================
 * Health Checks
 * ================================================================ */

static gpointer health_worker(gpointer user_data) {
    GtkWidget *status_lbl = GTK_WIDGET(user_data);

    gdk_threads_add_idle_full(G_PRIORITY_HIGH,
        (GSourceFunc)gtk_label_set_text, status_lbl, "Running...");

    log_msg("=== Running Full Health Check ===");

    char script[512];
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";

    /* Check multiple locations for ocws-health.sh */
    if (access("./scripts/ocws-health.sh", X_OK) == 0) {
        snprintf(script, sizeof(script), "./scripts/ocws-health.sh");
    } else if (access("/usr/local/bin/ocws-health.sh", X_OK) == 0) {
        snprintf(script, sizeof(script), "/usr/local/bin/ocws-health.sh");
    } else if (access("/usr/bin/ocws-health.sh", X_OK) == 0) {
        snprintf(script, sizeof(script), "/usr/bin/ocws-health.sh");
    } else {
        snprintf(script, sizeof(script), "%s/.local/bin/ocws-health.sh", home);
    }

    if (access(script, X_OK) == 0) {
        char cmd[600];
        snprintf(cmd, sizeof(cmd), "bash '%s' 2>&1", script);
        run_cmd_logged(cmd);

        gdk_threads_add_idle_full(G_PRIORITY_HIGH,
            (GSourceFunc)gtk_label_set_text, status_lbl, "Complete");
    } else {
        /* Fallback: try to just run it from PATH */
        log_msg("Attempting to run ocws-health.sh from PATH...");
        run_cmd_logged("ocws-health.sh 2>&1 || echo 'ERROR: ocws-health.sh not found'");
        gdk_threads_add_idle_full(G_PRIORITY_HIGH,
            (GSourceFunc)gtk_label_set_text, status_lbl, "Check output");
    }

    return NULL;
}

static void on_health_check(GtkWidget *widget, gpointer data) {
    (void)widget;
    GtkWidget *status_lbl = GTK_WIDGET(data);
    g_thread_new("health", health_worker, status_lbl);
}

static gpointer quick_health_worker(gpointer user_data) {
    const char *section = (const char *)user_data;

    log_msg("=== Quick Health: %s ===", section);

    if (strcmp(section, "system") == 0) {
        /* Memory */
        FILE *fp = popen("awk '/MemTotal/{t=$2} /MemAvailable/{a=$2} END{printf \"%.0f\", (t-a)*100/t}' /proc/meminfo", "r");
        if (fp) {
            char buf[32] = {0};
            if (fgets(buf, sizeof(buf), fp)) {
                buf[strcspn(buf, "\n")] = 0;
                int pct = atoi(buf);
                log_msg("[%s] Memory: %d%% used", pct < 70 ? "PASS" : (pct < 85 ? "WARN" : "FAIL"), pct);
            }
            pclose(fp);
        }
        /* Disk */
        fp = popen("df / | tail -1 | awk '{print $5}'", "r");
        if (fp) {
            char buf[32] = {0};
            if (fgets(buf, sizeof(buf), fp)) {
                buf[strcspn(buf, "\n")] = 0;
                int pct = atoi(buf);
                log_msg("[%s] Disk: %d%% used", pct < 70 ? "PASS" : (pct < 85 ? "WARN" : "FAIL"), pct);
            }
            pclose(fp);
        }
    } else if (strcmp(section, "services") == 0) {
        const char *svcs[] = {"labwc", "sfwbar", "mako", "swayidle", NULL};
        for (int i = 0; svcs[i]; i++) {
            char cmd[128];
            snprintf(cmd, sizeof(cmd), "pgrep -x %s >/dev/null 2>&1", svcs[i]);
            int running = system(cmd) == 0;
            log_msg("[%s] %s", running ? "PASS" : "WARN", svcs[i]);
        }
    } else if (strcmp(section, "config") == 0) {
        const char *home = getenv("HOME");
        if (!home) home = "/tmp";
        const char *files[] = {
            ".config/labwc/rc.xml",
            ".config/labwc/autostart",
            ".config/labwc/menu.xml",
            ".config/ocws/mode",
            NULL
        };
        for (int i = 0; files[i]; i++) {
            char path[512];
            snprintf(path, sizeof(path), "%s/%s", home, files[i]);
            log_msg("[%s] %s", access(path, R_OK) == 0 ? "PASS" : "FAIL", files[i]);
        }
    } else if (strcmp(section, "binaries") == 0) {
        const char *bins[] = {"ocws", "ocws-kv", "ocws-emit", "ocws-welcome", "ocws-settings", "ocws-pkgmgr", NULL};
        for (int i = 0; bins[i]; i++) {
            char cmd[128];
            snprintf(cmd, sizeof(cmd), "which %s >/dev/null 2>&1", bins[i]);
            int found = system(cmd) == 0;
            log_msg("[%s] %s", found ? "PASS" : "FAIL", bins[i]);
        }
    }

    return NULL;
}

static void on_quick_health(GtkWidget *widget, gpointer data) {
    (void)widget;
    const char *section = (const char *)data;
    g_thread_new("quick-health", quick_health_worker, (gpointer)section);
}

/* ================================================================
 * Updater
 * ================================================================ */

static gpointer update_check_worker(gpointer user_data) {
    GtkWidget *status_lbl = GTK_WIDGET(user_data);

    gdk_threads_add_idle_full(G_PRIORITY_HIGH,
        (GSourceFunc)gtk_label_set_text, status_lbl, "Checking...");

    log_msg("=== Checking for Updates ===");

    /* Check GitHub API for latest labwc release */
    log_msg("Checking labwc...");
    FILE *fp = popen("curl -sL 'https://api.github.com/repos/labwc/labwc/releases/latest' 2>/dev/null | grep '\"tag_name\"' | head -1 | sed 's/.*\"tag_name\": \"\\(.*\\)\".*/\\1/'", "r");
    if (fp) {
        char buf[128] = {0};
        if (fgets(buf, sizeof(buf), fp)) {
            buf[strcspn(buf, "\n")] = 0;
            if (buf[0]) {
                log_msg("  Latest labwc: %s", buf);

                /* Compare with installed */
                FILE *fp2 = popen("labwc --version 2>/dev/null | head -1", "r");
                if (fp2) {
                    char installed[128] = {0};
                    if (fgets(installed, sizeof(installed), fp2)) {
                        installed[strcspn(installed, "\n")] = 0;
                        log_msg("  Installed:   %s", installed);
                    }
                    pclose(fp2);
                }
            } else {
                log_msg("  Could not fetch latest labwc version");
            }
        }
        pclose(fp);
    }

    /* Check sfwbar */
    log_msg("Checking sfwbar...");
    fp = popen("curl -sL 'https://api.github.com/repos/LBCrion/sfwbar/releases/latest' 2>/dev/null | grep '\"tag_name\"' | head -1 | sed 's/.*\"tag_name\": \"\\(.*\\)\".*/\\1/'", "r");
    if (fp) {
        char buf[128] = {0};
        if (fgets(buf, sizeof(buf), fp)) {
            buf[strcspn(buf, "\n")] = 0;
            if (buf[0]) {
                log_msg("  Latest sfwbar: %s", buf);
            } else {
                log_msg("  Could not fetch latest sfwbar version");
            }
        }
        pclose(fp);
    }

    /* Check OCWS repo */
    log_msg("Checking OCWS...");
    fp = popen("curl -sL 'https://api.github.com/repos/naranyala/labwc-fuzzel-sfwbar/commits?per_page=1' 2>/dev/null | grep '\"sha\"' | head -1 | sed 's/.*\"sha\": \"\\(.*\\)\".*/\\1/' | head -c 7", "r");
    if (fp) {
        char buf[64] = {0};
        if (fgets(buf, sizeof(buf), fp)) {
            buf[strcspn(buf, "\n")] = 0;
            if (buf[0]) {
                log_msg("  Latest OCWS commit: %s", buf);
            }
        }
        pclose(fp);
    }

    log_msg("Update check complete.");
    gdk_threads_add_idle_full(G_PRIORITY_HIGH,
        (GSourceFunc)gtk_label_set_text, status_lbl, "Check complete");

    return NULL;
}

static void on_update_check(GtkWidget *widget, gpointer data) {
    (void)widget;
    GtkWidget *status_lbl = GTK_WIDGET(data);
    g_thread_new("update-check", update_check_worker, status_lbl);
}

static gpointer update_engine_worker(gpointer user_data) {
    const char *engine = (const char *)user_data;

    log_msg("=== Updating %s ===", engine);

    char cmd[1024];

    if (strcmp(engine, "ocws") == 0) {
        /* Update OCWS dotfiles */
        log_msg("Assuming current directory is the OCWS repository...");
        snprintf(cmd, sizeof(cmd),
            "if [ -d .git ]; then git pull origin main 2>&1 && bash install.sh 2>&1; else echo 'Not in a git repository. Please run ocws-pkgmgr from the dotfiles directory.'; fi");
        run_cmd_logged(cmd);
    } else if (strcmp(engine, "labwc") == 0) {
        snprintf(cmd, sizeof(cmd),
            "cd /tmp && rm -rf _ocws-labwc-update && "
            "git clone --depth=1 https://github.com/labwc/labwc.git _ocws-labwc-update && "
            "cd _ocws-labwc-update && "
            "meson setup build --prefix=/usr/local --buildtype=release 2>&1 && "
            "ninja -C build 2>&1 && "
            "pkexec ninja -C build install 2>&1 && "
            "cd /tmp && rm -rf _ocws-labwc-update");
        run_cmd_logged(cmd);
    } else if (strcmp(engine, "sfwbar") == 0) {
        snprintf(cmd, sizeof(cmd),
            "cd /tmp && rm -rf _ocws-sfwbar-update && "
            "git clone --depth=1 https://github.com/LBCrion/sfwbar.git _ocws-sfwbar-update && "
            "cd _ocws-sfwbar-update && "
            "meson setup build --prefix=/usr/local --buildtype=release 2>&1 && "
            "ninja -C build 2>&1 && "
            "pkexec ninja -C build install 2>&1 && "
            "cd /tmp && rm -rf _ocws-sfwbar-update");
        run_cmd_logged(cmd);
    }

    log_msg("=== %s update complete ===", engine);
    return NULL;
}

static void on_update_engine(GtkWidget *widget, gpointer data) {
    (void)widget;
    const char *engine = (const char *)data;
    g_thread_new("update-engine", update_engine_worker, (gpointer)engine);
}

static gpointer update_all_worker(gpointer user_data) {
    (void)user_data;
    const char *engines[] = {"labwc", "sfwbar", "ocws"};
    for (int i = 0; i < 3; i++) {
        update_engine_worker((gpointer)engines[i]);
    }
    log_msg("=== All updates complete ===");
    return NULL;
}

static void on_update_all(GtkWidget *widget, gpointer data) {
    (void)widget; (void)data;
    g_thread_new("update-all", update_all_worker, NULL);
}

/* ================================================================
 * UI Construction
 * ================================================================ */

static void activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;

    GtkWidget *window = gtk_application_window_new(app);
    ocws_gtk_enforce_premium_theme();
    ocws_gtk_apply_dynamic_css(app, NULL);
    gtk_window_set_title(GTK_WINDOW(window), "OCWS Package Manager");
    gtk_window_set_default_size(GTK_WINDOW(window), 900, 650);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

    /* Header */
    GtkWidget *header = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), TRUE);
    gtk_header_bar_set_title(GTK_HEADER_BAR(header), "OCWS Package Manager");
    gtk_header_bar_set_subtitle(GTK_HEADER_BAR(header), "Resolve Dependencies & Build from Source");
    gtk_window_set_titlebar(GTK_WINDOW(window), header);

    /* Main layout */
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(window), hbox);

    /* Left panel: Dependencies + Build + Health + Updater */
    GtkWidget *left_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_size_request(left_scroll, 420, -1);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(left_scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(hbox), left_scroll, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_separator_new(GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 0);

    GtkWidget *left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(left_scroll), left);

    /* Right panel: Log */
    GtkWidget *right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(hbox), right, TRUE, TRUE, 0);

    /* --- Left: Dependency Scanner --- */
    GtkWidget *dep_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(dep_header, 12);
    gtk_widget_set_margin_top(dep_header, 12);
    gtk_widget_set_margin_bottom(dep_header, 8);
    GtkWidget *dep_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(dep_title),
        "<span size='large' weight='bold'>Dependencies</span>");
    gtk_box_pack_start(GTK_BOX(dep_header), dep_title, TRUE, TRUE, 0);

    GtkWidget *scan_btn = gtk_button_new_with_label("Scan");
    g_signal_connect(scan_btn, "clicked", G_CALLBACK(on_scan_deps), NULL);
    gtk_box_pack_start(GTK_BOX(dep_header), scan_btn, FALSE, FALSE, 0);

    GtkWidget *install_btn = gtk_button_new_with_label("Install Missing");
    GtkStyleContext *ctx = gtk_widget_get_style_context(install_btn);
    gtk_style_context_add_class(ctx, "suggested-action");
    g_signal_connect(install_btn, "clicked", G_CALLBACK(on_install_missing), NULL);
    gtk_box_pack_start(GTK_BOX(dep_header), install_btn, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(left), dep_header, FALSE, FALSE, 0);

    /* Dependency list */
    GtkWidget *dep_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_container_set_border_width(GTK_CONTAINER(dep_box), 8);
    gtk_box_pack_start(GTK_BOX(left), dep_box, FALSE, FALSE, 0);

    const char *last_cat = "";
    for (int i = 0; i < DEP_COUNT; i++) {
        /* Category header */
        if (strcmp(DEPS[i].category, last_cat) != 0) {
            last_cat = DEPS[i].category;
            GtkWidget *cat_lbl = gtk_label_new(NULL);
            char *cat_markup = g_strdup_printf("<span weight='bold' alpha='70%%'>%s</span>", last_cat);
            gtk_label_set_markup(GTK_LABEL(cat_lbl), cat_markup);
            g_free(cat_markup);
            gtk_label_set_xalign(GTK_LABEL(cat_lbl), 0.0);
            gtk_widget_set_margin_top(cat_lbl, 8);
            gtk_box_pack_start(GTK_BOX(dep_box), cat_lbl, FALSE, FALSE, 0);
        }

        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        GtkStyleContext *rctx = gtk_widget_get_style_context(row);
        gtk_style_context_add_class(rctx, "dep-card");

        int installed = check_installed(DEPS[i].check_cmd);
        const char *status_icon = installed == 1 ? "✓" : (installed == 0 ? "✗" : "?");
        const char *status_color = installed == 1 ? OCWS_OK() : (installed == 0 ? OCWS_URGENT() : OCWS_MUTED());

        GtkWidget *icon_lbl = gtk_label_new(NULL);
        char *icon_markup = g_strdup_printf("<span foreground='%s' weight='bold'>%s</span>", status_color, status_icon);
        gtk_label_set_markup(GTK_LABEL(icon_lbl), icon_markup);
        g_free(icon_markup);
        gtk_widget_set_size_request(icon_lbl, 20, -1);
        gtk_box_pack_start(GTK_BOX(row), icon_lbl, FALSE, FALSE, 0);

        GtkWidget *name_lbl = gtk_label_new(NULL);
        char *name_markup = g_strdup_printf("<b>%s</b>", DEPS[i].name);
        gtk_label_set_markup(GTK_LABEL(name_lbl), name_markup);
        g_free(name_markup);
        gtk_widget_set_size_request(name_lbl, 130, -1);
        gtk_label_set_xalign(GTK_LABEL(name_lbl), 0.0);
        gtk_box_pack_start(GTK_BOX(row), name_lbl, FALSE, FALSE, 0);

        GtkWidget *desc_lbl = gtk_label_new(DEPS[i].desc);
        gtk_style_context_add_class(gtk_widget_get_style_context(desc_lbl), "dim-label");
        gtk_label_set_xalign(GTK_LABEL(desc_lbl), 0.0);
        gtk_box_pack_start(GTK_BOX(row), desc_lbl, TRUE, TRUE, 0);

        gtk_box_pack_start(GTK_BOX(dep_box), row, FALSE, FALSE, 0);
    }

    /* Separator */
    gtk_box_pack_start(GTK_BOX(left),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 4);

    /* --- Left: Build from Source --- */
    GtkWidget *build_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(build_header, 12);
    gtk_widget_set_margin_bottom(build_header, 8);
    GtkWidget *build_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(build_title),
        "<span size='large' weight='bold'>Build from Source</span>");
    gtk_box_pack_start(GTK_BOX(build_header), build_title, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(left), build_header, FALSE, FALSE, 0);

    GtkWidget *build_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_container_set_border_width(GTK_CONTAINER(build_box), 8);
    gtk_box_pack_start(GTK_BOX(left), build_box, FALSE, FALSE, 0);

    for (int i = 0; i < BUILD_COUNT; i++) {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_set_margin_bottom(row, 4);

        GtkWidget *name_lbl = gtk_label_new(NULL);
        char *nmarkup = g_strdup_printf("<b>%s</b>", BUILDS[i].name);
        gtk_label_set_markup(GTK_LABEL(name_lbl), nmarkup);
        g_free(nmarkup);
        gtk_widget_set_size_request(name_lbl, 110, -1);
        gtk_label_set_xalign(GTK_LABEL(name_lbl), 0.0);
        gtk_box_pack_start(GTK_BOX(row), name_lbl, FALSE, FALSE, 0);

        GtkWidget *desc_lbl = gtk_label_new(BUILDS[i].desc);
        gtk_style_context_add_class(gtk_widget_get_style_context(desc_lbl), "dim-label");
        gtk_label_set_xalign(GTK_LABEL(desc_lbl), 0.0);
        gtk_box_pack_start(GTK_BOX(row), desc_lbl, TRUE, TRUE, 0);

        GtkWidget *status_lbl = gtk_label_new("Ready");
        gtk_style_context_add_class(gtk_widget_get_style_context(status_lbl), "dim-label");
        gtk_widget_set_size_request(status_lbl, 60, -1);
        gtk_box_pack_start(GTK_BOX(row), status_lbl, FALSE, FALSE, 0);

        GtkWidget *build_btn = gtk_button_new_with_label("Build");
        BuildRowData *d = g_new0(BuildRowData, 1);
        d->index = i;
        d->btn = build_btn;
        d->status_lbl = status_lbl;
        g_signal_connect(build_btn, "clicked", G_CALLBACK(on_build_clicked), d);
        gtk_box_pack_start(GTK_BOX(row), build_btn, FALSE, FALSE, 0);

        gtk_box_pack_start(GTK_BOX(build_box), row, FALSE, FALSE, 0);
    }

    /* Separator */
    gtk_box_pack_start(GTK_BOX(left),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 4);

    /* --- Left: Health Checks --- */
    GtkWidget *health_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(health_header, 12);
    gtk_widget_set_margin_bottom(health_header, 8);
    GtkWidget *health_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(health_title),
        "<span size='large' weight='bold'>Health Checks</span>");
    gtk_box_pack_start(GTK_BOX(health_header), health_title, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(left), health_header, FALSE, FALSE, 0);

    GtkWidget *health_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(health_box, 12);
    gtk_widget_set_margin_bottom(health_box, 8);
    gtk_box_pack_start(GTK_BOX(left), health_box, FALSE, FALSE, 0);

    /* Quick status row */
    GtkWidget *health_status_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *health_status_lbl = gtk_label_new("Not checked yet");
    gtk_style_context_add_class(gtk_widget_get_style_context(health_status_lbl), "dim-label");
    gtk_label_set_xalign(GTK_LABEL(health_status_lbl), 0.0);
    gtk_box_pack_start(GTK_BOX(health_status_row), health_status_lbl, TRUE, TRUE, 0);

    GtkWidget *health_run_btn = gtk_button_new_with_label("Run Full Check");
    GtkStyleContext *hctx = gtk_widget_get_style_context(health_run_btn);
    gtk_style_context_add_class(hctx, "suggested-action");
    g_signal_connect(health_run_btn, "clicked", G_CALLBACK(on_health_check), health_status_lbl);
    gtk_box_pack_start(GTK_BOX(health_status_row), health_run_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(health_box), health_status_row, FALSE, FALSE, 0);

    /* Quick check buttons row */
    GtkWidget *quick_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *btn;

    btn = gtk_button_new_with_label("System");
    g_signal_connect(btn, "clicked", G_CALLBACK(on_quick_health), (gpointer)"system");
    gtk_box_pack_start(GTK_BOX(quick_row), btn, TRUE, TRUE, 0);

    btn = gtk_button_new_with_label("Services");
    g_signal_connect(btn, "clicked", G_CALLBACK(on_quick_health), (gpointer)"services");
    gtk_box_pack_start(GTK_BOX(quick_row), btn, TRUE, TRUE, 0);

    btn = gtk_button_new_with_label("Config");
    g_signal_connect(btn, "clicked", G_CALLBACK(on_quick_health), (gpointer)"config");
    gtk_box_pack_start(GTK_BOX(quick_row), btn, TRUE, TRUE, 0);

    btn = gtk_button_new_with_label("Binaries");
    g_signal_connect(btn, "clicked", G_CALLBACK(on_quick_health), (gpointer)"binaries");
    gtk_box_pack_start(GTK_BOX(quick_row), btn, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(health_box), quick_row, FALSE, FALSE, 0);

    /* Separator */
    gtk_box_pack_start(GTK_BOX(left),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 4);

    /* --- Left: Updater --- */
    GtkWidget *update_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(update_header, 12);
    gtk_widget_set_margin_bottom(update_header, 8);
    GtkWidget *update_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(update_title),
        "<span size='large' weight='bold'>Updater</span>");
    gtk_box_pack_start(GTK_BOX(update_header), update_title, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(left), update_header, FALSE, FALSE, 0);

    GtkWidget *update_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(update_box, 12);
    gtk_widget_set_margin_bottom(update_box, 8);
    gtk_box_pack_start(GTK_BOX(left), update_box, FALSE, FALSE, 0);

    /* Update status */
    GtkWidget *update_status_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *update_status_lbl = gtk_label_new("Click to check for updates");
    gtk_style_context_add_class(gtk_widget_get_style_context(update_status_lbl), "dim-label");
    gtk_label_set_xalign(GTK_LABEL(update_status_lbl), 0.0);
    gtk_box_pack_start(GTK_BOX(update_status_row), update_status_lbl, TRUE, TRUE, 0);

    GtkWidget *update_check_btn = gtk_button_new_with_label("Check Updates");
    g_signal_connect(update_check_btn, "clicked", G_CALLBACK(on_update_check), update_status_lbl);
    gtk_box_pack_start(GTK_BOX(update_status_row), update_check_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(update_box), update_status_row, FALSE, FALSE, 0);

    /* Update action buttons */
    GtkWidget *update_action_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);

    GtkWidget *update_ocws_btn = gtk_button_new_with_label("Update OCWS");
    GtkStyleContext *uctx = gtk_widget_get_style_context(update_ocws_btn);
    gtk_style_context_add_class(uctx, "suggested-action");
    g_signal_connect(update_ocws_btn, "clicked", G_CALLBACK(on_update_engine), (gpointer)"ocws");
    gtk_box_pack_start(GTK_BOX(update_action_row), update_ocws_btn, TRUE, TRUE, 0);

    GtkWidget *update_labwc_btn = gtk_button_new_with_label("Update labwc");
    g_signal_connect(update_labwc_btn, "clicked", G_CALLBACK(on_update_engine), (gpointer)"labwc");
    gtk_box_pack_start(GTK_BOX(update_action_row), update_labwc_btn, TRUE, TRUE, 0);

    GtkWidget *update_sfwbar_btn = gtk_button_new_with_label("Update sfwbar");
    g_signal_connect(update_sfwbar_btn, "clicked", G_CALLBACK(on_update_engine), (gpointer)"sfwbar");
    gtk_box_pack_start(GTK_BOX(update_action_row), update_sfwbar_btn, TRUE, TRUE, 0);

    GtkWidget *update_all_btn = gtk_button_new_with_label("Update All");
    g_signal_connect(update_all_btn, "clicked", G_CALLBACK(on_update_all), NULL);
    gtk_box_pack_start(GTK_BOX(update_action_row), update_all_btn, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(update_box), update_action_row, FALSE, FALSE, 0);

    /* --- Right: Log Output --- */
    GtkWidget *log_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(log_header, 12);
    gtk_widget_set_margin_top(log_header, 12);
    gtk_widget_set_margin_bottom(log_header, 8);
    GtkWidget *log_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(log_title),
        "<span size='large' weight='bold'>Output Log</span>");
    gtk_box_pack_start(GTK_BOX(log_header), log_title, TRUE, TRUE, 0);

    GtkWidget *clear_btn = gtk_button_new_with_label("Clear");
    g_signal_connect(clear_btn, "clicked", G_CALLBACK(on_scan_deps), NULL);
    gtk_box_pack_start(GTK_BOX(log_header), clear_btn, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(right), log_header, FALSE, FALSE, 0);

    GtkWidget *log_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(log_scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(right), log_scroll, TRUE, TRUE, 0);

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

    /* Initial log message */
    log_msg("OCWS Package Manager v0.1.0");
    log_msg("Click 'Scan' to check dependencies, or 'Build' to compile from source.");

    gtk_widget_show_all(window);
}

/* ================================================================
 * Entry point
 * ================================================================ */

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new(APP_ID, G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
