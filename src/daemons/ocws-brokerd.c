/*
 * ocws-brokerd.c — C-native Event Bus Daemon
 *
 * Replaces ocws-daemon.sh with a single-process event loop:
 *   - inotify watcher for backlight brightness changes
 *   - pactl subscribe for PulseAudio volume changes
 *   - playerctl metadata watcher for media art
 *
 * Emits state to sfwbar via ocws-emit (fork+exec).
 *
 * Build: zig build (in build.zig)
 * Usage: ocws-brokerd [--verbose] [--no-media]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/inotify.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <fcntl.h>
#include <dlfcn.h>
#include "../libocws/plugin_api.h"
#include "../libocws/bus.h"
#include "../libocws/notify.h"

/* ============================================================
 * Plugin host — loads .so plugins and bridges the event bus
 * ============================================================ */

static void log_msg(const char *fmt, ...);

#define MAX_PLUGINS 64

/* Uses the established OcwsPlugin ABI (src/libocws/plugin_api.h). */
typedef struct {
    void       *handle;
    OcwsPlugin *plugin;
    time_t      next_tick;
} loaded_plugin_t;

static loaded_plugin_t g_plugins[MAX_PLUGINS];
static int             g_nplugins = 0;
static char            g_active_id[64] = {0};
static char            g_cfg_buf[256]  = {0};

/* Forward plugin events to sfwbar via ocws-emit (skip internal topics). */
static void brokerd_sfwbar_bridge(const char *topic, const char *value) {
    if (topic && topic[0] == '_') return;
    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stderr);
        execlp("ocws-emit", "ocws-emit", topic, value, NULL);
        _exit(1);
    } else if (pid > 0) {
        waitpid(pid, NULL, 0);
    }
}

static void host_notify(const char *title, const char *body, const char *icon) {
    ocws_notify(title, body, icon);
}

/* Read a key=value from ~/.config/ocws/plugins/<id>/config. */
static const char *host_config_get(const char *key, const char *def) {
    if (!g_active_id[0]) return def;
    const char *h = getenv("HOME");
    char home[512];
    snprintf(home, sizeof(home), "%s", h ? h : "/tmp");
    char path[768];
    snprintf(path, sizeof(path), "%s/.config/ocws/plugins/%s/config", home, g_active_id);
    FILE *f = fopen(path, "r");
    if (!f) return def;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(line, key) == 0) {
            strncpy(g_cfg_buf, eq + 1, sizeof(g_cfg_buf) - 1);
            fclose(f);
            return g_cfg_buf;
        }
    }
    fclose(f);
    return def;
}

static void plugin_on_event(const char *topic, const char *json, void *user) {
    OcwsPlugin *p = (OcwsPlugin *)user;
    if (p && p->on_event) p->on_event(topic, json);
}

/* Minimal manifest parse: extract "id" and "library" strings. */
static void parse_manifest(const char *buf, char *id, size_t id_sz,
                           char *library, size_t lib_sz) {
    id[0] = library[0] = '\0';
    char *p = strstr((char *)buf, "\"id\"");
    if (p) { p = strchr(p, ':'); if (p) { p++; while (*p==' '||*p=='"') p++;
             char *e = strchr(p, '"'); if (e) { size_t n = (size_t)(e-p);
             if (n >= id_sz) n = id_sz-1; memcpy(id, p, n); id[n]='\0'; } } }
    p = strstr((char *)buf, "\"library\"");
    if (p) { p = strchr(p, ':'); if (p) { p++; while (*p==' '||*p=='"') p++;
             char *e = strchr(p, '"'); if (e) { size_t n = (size_t)(e-p);
             if (n >= lib_sz) n = lib_sz-1; memcpy(library, p, n); library[n]='\0'; } } }
}

static void load_plugins(void) {
    /* Register host services so plugins can emit/notify/config via
     * libocws-pluginrt (shared by host + plugins → one bus instance). */
    ocws_plugin_set_host(ocws_bus_emit, host_notify, host_config_get);
    ocws_bus_set_sfwbar_bridge(brokerd_sfwbar_bridge);

    const char *env = getenv("OCWS_PLUGIN_DIR");
    const char *h = getenv("HOME");
    char home[512];
    snprintf(home, sizeof(home), "%s", h ? h : "/tmp");

    char dirs[2][768];
    int ndirs = 0;
    if (env && env[0]) snprintf(dirs[ndirs++], sizeof(dirs[0]), "%s", env);
    snprintf(dirs[ndirs++], sizeof(dirs[0]), "%s/.local/share/ocws/plugins", home);

    for (int d = 0; d < ndirs; d++) {
        DIR *dp = opendir(dirs[d]);
        if (!dp) continue;
        struct dirent *ent;
        while ((ent = readdir(dp)) != NULL) {
            if (ent->d_name[0] == '.') continue;
            char pdir[1024];
            snprintf(pdir, sizeof(pdir), "%s/%s", dirs[d], ent->d_name);

            char mjson[1024];
            snprintf(mjson, sizeof(mjson), "%s/plugin.json", pdir);
            FILE *mf = fopen(mjson, "r");
            if (!mf) continue;
            char buf[2048]; size_t total = 0;
            while (total < sizeof(buf) - 1 &&
                   fgets(buf + total, (int)(sizeof(buf) - total), mf))
                total += strlen(buf + total);
            fclose(mf);

            char id[64] = {0}, library[128] = {0};
            parse_manifest(buf, id, sizeof(id), library, sizeof(library));
            if (!id[0]) snprintf(id, sizeof(id), "%s", ent->d_name);
            if (!library[0]) snprintf(library, sizeof(library), "lib%s.so", id);

            char libpath[1024];
            snprintf(libpath, sizeof(libpath), "%s/%s", pdir, library);
            void *hnd = dlopen(libpath, RTLD_NOW | RTLD_LOCAL);
            if (!hnd) { log_msg("plugin load failed: %s (%s)", libpath, dlerror()); continue; }

            OcwsPlugin *pl = dlsym(hnd, "OCWS_PLUGIN_ENTRY");
            if (!pl) { log_msg("no OCWS_PLUGIN_ENTRY in %s", libpath); dlclose(hnd); continue; }
            if (pl->api_version != OCWS_PLUGIN_API_VERSION) {
                log_msg("plugin %s API mismatch (got %d)", id, pl->api_version);
                dlclose(hnd); continue;
            }
            if (g_nplugins >= MAX_PLUGINS) { dlclose(hnd); break; }

            loaded_plugin_t *lp = &g_plugins[g_nplugins];
            lp->handle    = hnd;
            lp->plugin    = pl;
            lp->next_tick = 0;

            snprintf(g_active_id, sizeof(g_active_id), "%s", id);
            if (pl->init && pl->init() != 0) {
                log_msg("plugin %s init failed", id);
                dlclose(hnd);
                g_active_id[0] = '\0';
                continue;
            }
            g_active_id[0] = '\0';

            if (pl->on_event)
                ocws_bus_subscribe("*", plugin_on_event, pl);

            g_nplugins++;
            log_msg("loaded plugin: %s", id);
        }
        closedir(dp);
    }
    log_msg("plugins loaded: %d", g_nplugins);
}

static void run_plugin_ticks(void) {
    time_t now = time(NULL);
    for (int i = 0; i < g_nplugins; i++) {
        OcwsPlugin *pl = g_plugins[i].plugin;
        if (pl->on_tick && pl->tick_interval_sec > 0 && now >= g_plugins[i].next_tick) {
            pl->on_tick();
            g_plugins[i].next_tick = now + pl->tick_interval_sec;
        }
    }
}

static void unload_plugins(void) {
    for (int i = 0; i < g_nplugins; i++) {
        if (g_plugins[i].plugin->shutdown) g_plugins[i].plugin->shutdown();
        if (g_plugins[i].handle) dlclose(g_plugins[i].handle);
    }
    g_nplugins = 0;
}

/* ============================================================
 * Configuration
 * ============================================================ */

#define VERSION "1.0.0"
#define EMIT_BIN "ocws-emit"
#define MAX_EVENTS 1024
#define EVENT_BUF_LEN (MAX_EVENTS * (sizeof(struct inotify_event) + 16))
#define POLL_INTERVAL_MS 100

static int opt_verbose = 0;
static volatile sig_atomic_t running = 1;
static char g_cover_path[512] = {0};

static const char *get_cover_path(void) {
    if (g_cover_path[0]) return g_cover_path;
    const char *rt = getenv("XDG_RUNTIME_DIR");
    if (rt && *rt)
        snprintf(g_cover_path, sizeof(g_cover_path), "%s/ocws-cover.jpg", rt);
    else {
        const char *h = getenv("HOME");
        if (h && *h) {
            snprintf(g_cover_path, sizeof(g_cover_path), "%s/.cache/ocws/ocws-cover.jpg", h);
            /* Ensure directory exists */
            char dir[512];
            snprintf(dir, sizeof(dir), "%s/.cache/ocws", h);
            mkdir(dir, 0700);
        } else
            return "/dev/null";
    }
    return g_cover_path;
}

/* ============================================================
 * Signal Handling
 * ============================================================ */

static void signal_handler(int sig) {
    (void)sig;
    running = 0;
}

static void setup_signals(void) {
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

/* ============================================================
 * Logging
 * ============================================================ */

static void log_msg(const char *fmt, ...) {
    if (!opt_verbose) return;
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    fprintf(stderr, "[%02d:%02d:%02d] %s\n", tm->tm_hour, tm->tm_min, tm->tm_sec, buf);
}

/* ============================================================
 * Emit to sfwbar
 * ============================================================ */

static void emit_event(const char *ns, const char *value) {
    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stderr);
        execlp(EMIT_BIN, EMIT_BIN, ns, value, NULL);
        _exit(1);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        log_msg("emit %s = %s", ns, value);
    }
}

/* ============================================================
 * Sysfs Helpers
 * ============================================================ */

static int sysfs_read_int(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    int val = -1;
    fscanf(f, "%d", &val);
    fclose(f);
    return val;
}

static int find_backlight_device(char *name, size_t len) {
    DIR *d = opendir("/sys/class/backlight");
    if (!d) return -1;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        char path[256];
        snprintf(path, sizeof(path), "/sys/class/backlight/%s/brightness", ent->d_name);
        FILE *f = fopen(path, "r");
        if (f) {
            fclose(f);
            strncpy(name, ent->d_name, len - 1);
            name[len - 1] = '\0';
            closedir(d);
            return 0;
        }
    }
    closedir(d);
    return -1;
}

/* ============================================================
 * Backlight Watcher (inotify)
 * ============================================================ */

static int inotify_fd = -1;
static int backlight_wd = -1;
static char backlight_dev[64] = {0};
static int last_brightness = -1;

static int init_backlight_watcher(void) {
    if (find_backlight_device(backlight_dev, sizeof(backlight_dev)) != 0) {
        log_msg("No backlight device found");
        return -1;
    }

    inotify_fd = inotify_init1(IN_NONBLOCK);
    if (inotify_fd < 0) {
        log_msg("inotify_init1 failed: %s", strerror(errno));
        return -1;
    }

    char path[256];
    snprintf(path, sizeof(path), "/sys/class/backlight/%s/brightness", backlight_dev);
    backlight_wd = inotify_add_watch(inotify_fd, path, IN_MODIFY);
    if (backlight_wd < 0) {
        log_msg("inotify_add_watch failed: %s", strerror(errno));
        close(inotify_fd);
        inotify_fd = -1;
        return -1;
    }

    last_brightness = sysfs_read_int(path);
    log_msg("Watching backlight: %s (value=%d)", backlight_dev, last_brightness);
    return 0;
}

static void check_backlight(void) {
    if (inotify_fd < 0) return;

    char buf[EVENT_BUF_LEN];
    struct timeval tv = {0, 0};
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(inotify_fd, &rfds);

    if (select(inotify_fd + 1, &rfds, NULL, NULL, &tv) <= 0) return;

    int len = read(inotify_fd, buf, sizeof(buf));
    if (len < 0) return;

    int i = 0;
    while (i < len) {
        struct inotify_event *event = (struct inotify_event *)&buf[i];
        if (event->mask & IN_MODIFY) {
            char path[256];
            snprintf(path, sizeof(path), "/sys/class/backlight/%s/brightness", backlight_dev);
            int cur = sysfs_read_int(path);
            if (cur >= 0 && cur != last_brightness) {
                int max_b = sysfs_read_int("/sys/class/backlight/" "/max_brightness");
                /* Try with device name */
                char max_path[256];
                snprintf(max_path, sizeof(max_path), "/sys/class/backlight/%s/max_brightness", backlight_dev);
                max_b = sysfs_read_int(max_path);
                int pct = max_b > 0 ? (cur * 100) / max_b : cur;
                emit_event("System.Brightness", "0"); /* placeholder */
                char val[16];
                snprintf(val, sizeof(val), "%d", pct);
                emit_event("System.Brightness", val);
                last_brightness = cur;
            }
        }
        i += sizeof(struct inotify_event) + event->len;
    }
}

/* ============================================================
 * Volume Watcher (pactl subscribe)
 * ============================================================ */

static FILE *pactl_fp = NULL;
static int volume_pipe[2] = {-1, -1};

static int init_volume_watcher(void) {
    if (access("/usr/bin/pactl", F_OK) != 0 &&
        access("/usr/bin/wpctl", F_OK) != 0) {
        log_msg("pactl/wpctl not found, skipping volume watcher");
        return -1;
    }

    if (pipe(volume_pipe) < 0) {
        log_msg("pipe failed: %s", strerror(errno));
        return -1;
    }

    pid_t pid = fork();
    if (pid == 0) {
        close(volume_pipe[0]);
        dup2(volume_pipe[1], STDOUT_FILENO);
        close(volume_pipe[1]);
        freopen("/dev/null", "w", stderr);
        execlp("pactl", "pactl", "subscribe", NULL);
        _exit(1);
    } else if (pid > 0) {
        close(volume_pipe[1]);
        volume_pipe[1] = -1;
        /* Set non-blocking */
        int flags = fcntl(volume_pipe[0], F_GETFL, 0);
        fcntl(volume_pipe[0], F_SETFL, flags | O_NONBLOCK);
        log_msg("Volume watcher started (pactl subscribe)");
        return 0;
    }
    return -1;
}

static void check_volume(void) {
    if (volume_pipe[0] < 0) return;

    char buf[1024];
    ssize_t n = read(volume_pipe[0], buf, sizeof(buf) - 1);
    if (n <= 0) return;
    buf[n] = '\0';

    /* Check if this is a sink change event */
    if (strstr(buf, "Event 'change' on sink")) {
        /* Get current volume */
        FILE *fp = popen("pactl get-sink-volume @DEFAULT_SINK@ 2>/dev/null | grep -oP '\\d+%%' | head -1 | tr -d '%%'", "r");
        if (fp) {
            char vol[16] = {0};
            if (fgets(vol, sizeof(vol), fp)) {
                vol[strcspn(vol, "\n")] = '\0';
                emit_event("System.Volume", vol);
            }
            pclose(fp);
        }

        /* Get mute state */
        fp = popen("pactl get-sink-mute @DEFAULT_SINK@ 2>/dev/null", "r");
        if (fp) {
            char mute[64] = {0};
            if (fgets(mute, sizeof(mute), fp)) {
                int muted = strstr(mute, "yes") != NULL ? 1 : 0;
                char val[8];
                snprintf(val, sizeof(val), "%d", muted);
                emit_event("System.VolumeMuted", val);
            }
            pclose(fp);
        }
    }
}

/* ============================================================
 * Media Art Watcher (playerctl)
 * ============================================================ */

static FILE *playerctl_fp = NULL;

static int init_media_watcher(void) {
    if (access("/usr/bin/playerctl", F_OK) != 0) {
        log_msg("playerctl not found, skipping media watcher");
        return -1;
    }

    playerctl_fp = popen("playerctl metadata -F mpris:artUrl 2>/dev/null", "r");
    if (!playerctl_fp) {
        log_msg("Failed to start playerctl");
        return -1;
    }

    /* Set non-blocking */
    int fd = fileno(playerctl_fp);
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    log_msg("Media watcher started (playerctl)");
    return 0;
}

static void check_media(void) {
    if (!playerctl_fp) return;

    char buf[1024];
    ssize_t n = read(fileno(playerctl_fp), buf, sizeof(buf) - 1);
    if (n <= 0) return;
    buf[n] = '\0';

    /* Process each line */
    char *line = strtok(buf, "\n");
    while (line) {
        line[strcspn(line, "\r")] = '\0';
        if (line[0] == '\0') { line = strtok(NULL, "\n"); continue; }

        if (strncmp(line, "file://", 7) == 0) {
            const char *path = line + 7;
            const char *cover = get_cover_path();
            pid_t cpid = fork();
            if (cpid == 0) { execlp("cp", "cp", path, cover, NULL); _exit(1); }
            else if (cpid > 0) waitpid(cpid, NULL, 0);
            log_msg("Media art: %s", path);
        } else if (strncmp(line, "http", 4) == 0) {
            const char *cover = get_cover_path();
            pid_t cpid = fork();
            if (cpid == 0) {
                execlp("curl", "curl", "-sSL", "--max-time", "10",
                       "--connect-timeout", "5", line,
                       "-o", cover, NULL);
                _exit(1);
            }
            else if (cpid > 0) waitpid(cpid, NULL, 0);
            log_msg("Media art downloaded");
        } else {
            unlink(get_cover_path());
        }

        /* Also emit media metadata */
        FILE *fp = popen("playerctl metadata title 2>/dev/null", "r");
        if (fp) {
            char title[256] = {0};
            if (fgets(title, sizeof(title), fp)) {
                title[strcspn(title, "\n")] = '\0';
                emit_event("Media.Title", title);
            }
            pclose(fp);
        }

        fp = popen("playerctl metadata artist 2>/dev/null", "r");
        if (fp) {
            char artist[256] = {0};
            if (fgets(artist, sizeof(artist), fp)) {
                artist[strcspn(artist, "\n")] = '\0';
                emit_event("Media.Artist", artist);
            }
            pclose(fp);
        }

        fp = popen("playerctl status 2>/dev/null", "r");
        if (fp) {
            char status[64] = {0};
            if (fgets(status, sizeof(status), fp)) {
                status[strcspn(status, "\n")] = '\0';
                int playing = (strcmp(status, "Playing") == 0) ? 1 : 0;
                char val[8];
                snprintf(val, sizeof(val), "%d", playing);
                emit_event("Media.Status", val);
            }
            pclose(fp);
        }

        line = strtok(NULL, "\n");
    }
}

/* ============================================================
 * Cleanup
 * ============================================================ */

static void cleanup(void) {
    unload_plugins();
    if (inotify_fd >= 0) {
        if (backlight_wd >= 0) inotify_rm_watch(inotify_fd, backlight_wd);
        close(inotify_fd);
    }
    if (volume_pipe[0] >= 0) close(volume_pipe[0]);
    if (volume_pipe[1] >= 0) close(volume_pipe[1]);
    if (playerctl_fp) pclose(playerctl_fp);
    log_msg("ocws-brokerd stopped");
}

/* ============================================================
 * Usage
 * ============================================================ */

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [options]\n\n"
        "C-native Event Bus daemon for OCWS.\n"
        "Replaces ocws-daemon.sh with a single-process event loop.\n\n"
        "Options:\n"
        "  -v, --verbose    Enable verbose logging\n"
        "  -m, --no-media  Disable media art watcher\n"
        "  -h, --help       Show this help\n\n"
        "Watchers:\n"
        "  - inotify on /sys/class/backlight/*/brightness\n"
        "  - pactl subscribe for PulseAudio events\n"
        "  - playerctl metadata for media art\n",
        prog);
}

/* ============================================================
 * Main
 * ============================================================ */

int main(int argc, char *argv[]) {
    int no_media = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0)
            opt_verbose = 1;
        else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--no-media") == 0)
            no_media = 1;
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        }
    }

    umask(0077);
    setup_signals();

    ocws_bus_init();
    ocws_bus_set_sfwbar_bridge(brokerd_sfwbar_bridge);

    log_msg("ocws-brokerd v%s starting", VERSION);

    /* Initialize watchers */
    init_backlight_watcher();
    init_volume_watcher();
    if (!no_media) init_media_watcher();

    load_plugins();

    log_msg("Event loop running (Ctrl+C to stop)");

    /* Event loop */
    while (running) {
        check_backlight();
        check_volume();
        if (!no_media) check_media();
        run_plugin_ticks();
        usleep(POLL_INTERVAL_MS * 1000);
    }

    cleanup();
    return 0;
}
