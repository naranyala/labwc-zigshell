#ifndef OCWS_CLI_COMMON_H
#define OCWS_CLI_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>

/* ANSI Colors */
#define OCWS_C_RED     "\033[0;31m"
#define OCWS_C_GREEN   "\033[0;32m"
#define OCWS_C_YELLOW  "\033[1;33m"
#define OCWS_C_CYAN    "\033[0;36m"
#define OCWS_C_BOLD    "\033[1m"
#define OCWS_C_DIM     "\033[2m"
#define OCWS_C_NC      "\033[0m"

static inline void cli_pass(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("  " OCWS_C_GREEN "✓" OCWS_C_NC " ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

static inline void cli_fail(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "  " OCWS_C_RED "✗" OCWS_C_NC " ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(1);
}

static inline void cli_warn(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "  " OCWS_C_YELLOW "⚠" OCWS_C_NC " ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

static inline void cli_info(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("\n" OCWS_C_CYAN "==>" OCWS_C_NC " ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

static inline void cli_check_cmd(const char *cmd) {
    char path[256];
    snprintf(path, sizeof(path), "which %s >/dev/null 2>&1", cmd);
    if (system(path) != 0) {
        cli_fail("Required command not found: %s", cmd);
    }
}

static inline const char* cli_get_home(void) {
    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        if (pw) home = pw->pw_dir;
    }
    return home ? home : "/tmp";
}

static inline void cli_get_timestamp(char *buf, size_t len) {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    strftime(buf, len, "%Y%m%d-%H%M%S", tm);
}

static inline void cli_mkdir_p(const char *path) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", path);
    system(cmd);
}

static inline int cli_file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

#endif
