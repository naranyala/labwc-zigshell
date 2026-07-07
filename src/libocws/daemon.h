#ifndef OCWS_DAEMON_H
#define OCWS_DAEMON_H

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static volatile sig_atomic_t ocws_daemon_running = 1;

static inline void ocws_daemon_signal_handler(int sig) {
    (void)sig;
    ocws_daemon_running = 0;
}

static inline void ocws_daemon_setup_signals(void) {
    struct sigaction sa;
    sa.sa_handler = ocws_daemon_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

static inline int ocws_daemon_write_pid(const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "%d\n", getpid());
    fclose(f);
    return 0;
}

static inline int ocws_daemon_read_pid(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    int pid = -1;
    fscanf(f, "%d", &pid);
    fclose(f);
    return pid;
}

static inline int ocws_daemon_is_running(const char *pid_file) {
    int pid = ocws_daemon_read_pid(pid_file);
    if (pid <= 0) return 0;
    return kill(pid, 0) == 0;
}

static inline void ocws_daemon_remove_pid(const char *path) {
    unlink(path);
}

#endif
