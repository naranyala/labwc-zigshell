#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <dirent.h>
#include "../libocws/notify.h"

#define RECORD_DIR_DEFAULT "$HOME/Videos/recordings"
#define CODEC_DEFAULT "libx264"
#define CRF_DEFAULT "23"
#define AUDIO_DEFAULT "auto"

static volatile int running = 1;

static const char *pid_path(void) {
    static char buf[256] = {0};
    if (buf[0]) return buf;
    const char *rt = getenv("XDG_RUNTIME_DIR");
    if (rt && *rt)
        snprintf(buf, sizeof(buf), "%s/ocws-recorder.pid", rt);
    else {
        const char *home = getenv("HOME");
        if (home && *home)
            snprintf(buf, sizeof(buf), "%s/.config/ocws/ocws-recorder.pid", home);
        else
            return NULL;
    }
    return buf;
}

static void on_signal(int sig) {
    (void)sig;
    running = 0;
}

static int is_safe_ident(const char *s) {
    for (; *s; s++)
        if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') ||
              (*s >= '0' && *s <= '9') || *s == '_' || *s == '-' || *s == '.'))
            return 0;
    return 1;
}

static int is_safe_codec(const char *c) {
    static const char *allowed[] = {
        "libx264", "libx265", "libvp8", "libvp9",
        "libaom-av1", "h264_vaapi", "hevc_vaapi", "mjpeg", NULL
    };
    for (int i = 0; allowed[i]; i++)
        if (strcmp(c, allowed[i]) == 0) return 1;
    return 0;
}

static int is_safe_crf(const char *c) {
    char *end;
    long v = strtol(c, &end, 10);
    return *c && *end == '\0' && v >= 0 && v <= 51;
}

static int check_cmd(const char *cmd) {
    char buf[256];
    snprintf(buf, sizeof(buf), "command -v %s >/dev/null 2>&1", cmd);
    return system(buf) == 0;
}

static pid_t read_pid(void) {
    FILE *f = fopen(pid_path(), "r");
    if (!f) return -1;
    pid_t pid = -1;
    fscanf(f, "%d", &pid);
    fclose(f);
    return pid;
}

static void write_pid(pid_t pid) {
    FILE *f = fopen(pid_path(), "w");
    if (f) { fprintf(f, "%d\n", pid); fclose(f); }
}

static void remove_pid(void) {
    remove(pid_path());
}

static int is_recording(void) {
    pid_t pid = read_pid();
    if (pid <= 0) return 0;
    return kill(pid, 0) == 0;
}

static void notify(const char *title, const char *body) {
    ocws_notify(title, body, NULL);
}

static const char *get_record_dir(void) {
    const char *dir = getenv("OCWS_RECORD_DIR");
    if (dir && *dir) return dir;
    return RECORD_DIR_DEFAULT;
}

static void start_recording(const char *audio, const char *codec, const char *crf, int fullscreen) {
    if (is_recording()) {
        fprintf(stderr, "Already recording (PID %d)\n", read_pid());
        return;
    }

    if (!check_cmd("wf-recorder")) {
        fprintf(stderr, "error: wf-recorder not found\n");
        fprintf(stderr, "install: sudo apt install wf-recorder\n");
        return;
    }

    /* Create output directory */
    char dir_expanded[512];
    const char *dir = get_record_dir();
    if (strncmp(dir, "$HOME", 5) == 0)
        snprintf(dir_expanded, sizeof(dir_expanded), "%s%s", getenv("HOME") ?: "/tmp", dir + 5);
    else
        snprintf(dir_expanded, sizeof(dir_expanded), "%s", dir);
    pid_t mkpid = fork();
    if (mkpid == 0) { execlp("mkdir", "mkdir", "-p", dir_expanded, NULL); _exit(1); }
    else if (mkpid > 0) waitpid(mkpid, NULL, 0);

    /* Generate filename with timestamp */
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char filename[256];
    snprintf(filename, sizeof(filename), "%s/recording-%04d%02d%02d-%02d%02d%02d.mp4",
        dir_expanded,
        tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
        tm->tm_hour, tm->tm_min, tm->tm_sec);

    /* Build argument array — no shell involved, no injection possible */
    char *args[32];
    int argc = 0;
    args[argc++] = "wf-recorder";
    args[argc++] = "-f";
    args[argc++] = filename;
    args[argc++] = "-c";
    args[argc++] = (char *)codec;
    args[argc++] = "--crf";
    args[argc++] = (char *)crf;

    if (strcmp(audio, "none") != 0) {
        args[argc++] = "--audio";
        if (strcmp(audio, "auto") != 0) {
            args[argc++] = "-A";
            args[argc++] = (char *)audio;
        }
    }

    if (!fullscreen && check_cmd("slurp")) {
        args[argc++] = "-g";
        /* slurp must run in parent to capture geometry before exec */
        char geometry[128] = {0};
        FILE *gp = popen("slurp", "r");
        if (gp) {
            if (fgets(geometry, sizeof(geometry), gp))
                geometry[strcspn(geometry, "\n")] = '\0';
            pclose(gp);
        }
        if (geometry[0] == '\0') return;
        args[argc++] = geometry;
    }

    args[argc] = NULL;

    fprintf(stderr, "ocws-recorder: starting recording...\n");
    fprintf(stderr, "  file: %s\n", filename);

    pid_t pid = fork();
    if (pid == 0) {
        execvp("wf-recorder", args);
        _exit(1);
    } else if (pid > 0) {
        write_pid(pid);
        notify("Recording Started", filename);
        fprintf(stderr, "  PID: %d\n", pid);
    } else {
        perror("fork");
    }
}

static void stop_recording(void) {
    pid_t pid = read_pid();
    if (pid <= 0 || !is_recording()) {
        fprintf(stderr, "No active recording\n");
        return;
    }

    kill(pid, SIGINT);
    fprintf(stderr, "ocws-recorder: stopping recording (PID %d)...\n", pid);

    int status;
    waitpid(pid, &status, 0);
    remove_pid();

    int success = WIFEXITED(status) && WEXITSTATUS(status) == 0;
    if (success) {
        notify("Recording Saved", "Recording completed successfully");
        fprintf(stderr, "Recording saved\n");
    } else {
        ocws_notify_error("Recording Failed", "Recording stopped with error");
        fprintf(stderr, "Recording failed\n");
    }
}

static void pause_recording(void) {
    pid_t pid = read_pid();
    if (pid <= 0 || !is_recording()) {
        fprintf(stderr, "No active recording\n");
        return;
    }

    kill(pid, SIGSTOP);
    fprintf(stderr, "ocws-recorder: paused\n");
    notify("Recording Paused", "Recording is paused");
}

static void resume_recording(void) {
    pid_t pid = read_pid();
    if (pid <= 0) {
        fprintf(stderr, "No paused recording\n");
        return;
    }

    kill(pid, SIGCONT);
    fprintf(stderr, "ocws-recorder: resumed\n");
    notify("Recording Resumed", "Recording is active again");
}

static void show_status(void) {
    if (is_recording()) {
        pid_t pid = read_pid();
        printf("RECORDING=true\n");
        printf("RECORDING_PID=%d\n", pid);

        /* Try to get file size */
        char proc_path[64];
        snprintf(proc_path, sizeof(proc_path), "/proc/%d/fd/1", pid);
        char link[512] = {0};
        ssize_t len = readlink(proc_path, link, sizeof(link) - 1);
        if (len > 0) {
            link[len] = '\0';
            struct stat st;
            if (stat(link, &st) == 0)
                printf("RECORDING_SIZE=%ld\n", (long)st.st_size);
            printf("RECORDING_FILE=%s\n", link);
        }
    } else {
        printf("RECORDING=false\n");
    }
}

static void list_recordings(void) {
    const char *dir = get_record_dir();
    if (strncmp(dir, "$HOME", 5) == 0) {
        char expanded[512];
        snprintf(expanded, sizeof(expanded), "%s%s", getenv("HOME") ?: "/tmp", dir + 5);
        dir = expanded;
    }
    pid_t pid = fork();
    if (pid == 0) {
        execlp("ls", "ls", "-lt", dir, NULL);
        _exit(1);
    } else if (pid > 0)
        waitpid(pid, NULL, 0);
}

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s <command> [args]\n\n"
        "Screen recording tool for OCWS desktop shell.\n\n"
        "Commands:\n"
        "  start [OPTIONS]   Start recording\n"
        "  stop              Stop current recording\n"
        "  pause             Pause recording\n"
        "  resume            Resume paused recording\n"
        "  toggle            Start/stop toggle\n"
        "  status            Show recording status\n"
        "  list              List recent recordings\n"
        "  -h                Show this help\n\n"
        "Start Options:\n"
        "  -r                Full screen (default: region select)\n"
        "  -a AUDIO          Audio: auto (default), none, or device name\n"
        "  -c CODEC          Video codec (default: libx264)\n"
        "  --crf N           Quality 0-51 (default: 23, lower=better)\n\n"
        "Environment:\n"
        "  OCWS_RECORD_DIR   Output directory (default: ~/Videos/recordings)\n\n"
        "Examples:\n"
        "  %s start              # select region, record with audio\n"
        "  %s start -r           # full screen recording\n"
        "  %s start -a none      # no audio\n"
        "  %s toggle             # toggle recording on/off\n",
        prog, prog, prog, prog);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    umask(0077);
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    if (argc < 2) { usage(argv[0]); return 1; }

    const char *cmd = argv[1];

    if (strcmp(cmd, "start") == 0) {
        const char *audio = AUDIO_DEFAULT;
        const char *codec = CODEC_DEFAULT;
        const char *crf = CRF_DEFAULT;
        int fullscreen = 0;

        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-r") == 0) fullscreen = 1;
            else if (strcmp(argv[i], "-a") == 0 && i + 1 < argc) {
                audio = argv[++i];
                if (strcmp(audio, "none") != 0 && strcmp(audio, "auto") != 0 && !is_safe_ident(audio)) {
                    fprintf(stderr, "warning: invalid audio device name, using 'auto'\n");
                    audio = AUDIO_DEFAULT;
                }
            }
            else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
                codec = argv[++i];
                if (!is_safe_codec(codec)) {
                    fprintf(stderr, "warning: invalid codec '%s', using '%s'\n", codec, CODEC_DEFAULT);
                    codec = CODEC_DEFAULT;
                }
            }
            else if (strcmp(argv[i], "--crf") == 0 && i + 1 < argc) {
                crf = argv[++i];
                if (!is_safe_crf(crf)) {
                    fprintf(stderr, "warning: invalid CRF '%s', using '%s'\n", crf, CRF_DEFAULT);
                    crf = CRF_DEFAULT;
                }
            }
        }

        start_recording(audio, codec, crf, fullscreen);
    } else if (strcmp(cmd, "stop") == 0) {
        stop_recording();
    } else if (strcmp(cmd, "pause") == 0) {
        pause_recording();
    } else if (strcmp(cmd, "resume") == 0) {
        resume_recording();
    } else if (strcmp(cmd, "toggle") == 0) {
        if (is_recording()) stop_recording();
        else start_recording(AUDIO_DEFAULT, CODEC_DEFAULT, CRF_DEFAULT, 0);
    } else if (strcmp(cmd, "status") == 0) {
        show_status();
    } else if (strcmp(cmd, "list") == 0) {
        list_recordings();
    } else if (strcmp(cmd, "-h") == 0 || strcmp(cmd, "--help") == 0) {
        usage(argv[0]);
    } else {
        fprintf(stderr, "unknown command: %s\n", cmd);
        usage(argv[0]);
        return 1;
    }

    return 0;
}
