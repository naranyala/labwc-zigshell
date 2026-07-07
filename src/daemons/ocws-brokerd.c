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
#include <dirent.h>
#include <time.h>
#include <fcntl.h>

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
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "cp '%s' /tmp/ocws-cover.jpg 2>/dev/null", path);
            system(cmd);
            log_msg("Media art: %s", path);
        } else if (strncmp(line, "http", 4) == 0) {
            char cmd[1024];
            snprintf(cmd, sizeof(cmd),
                "curl -sSL --max-time 10 --connect-timeout 5 '%s' -o /tmp/ocws-cover.jpg 2>/dev/null",
                line);
            system(cmd);
            log_msg("Media art downloaded");
        } else {
            system("rm -f /tmp/ocws-cover.jpg 2>/dev/null");
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

    setup_signals();

    log_msg("ocws-brokerd v%s starting", VERSION);

    /* Initialize watchers */
    init_backlight_watcher();
    init_volume_watcher();
    if (!no_media) init_media_watcher();

    log_msg("Event loop running (Ctrl+C to stop)");

    /* Event loop */
    while (running) {
        check_backlight();
        check_volume();
        if (!no_media) check_media();
        usleep(POLL_INTERVAL_MS * 1000);
    }

    cleanup();
    return 0;
}
