#ifndef OCWS_IPC_H
#define OCWS_IPC_H

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define OCWS_EMIT_BIN "ocws-emit"

static inline void ocws_emit(const char *namespace, const char *value) {
    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stderr);
        execlp(OCWS_EMIT_BIN, OCWS_EMIT_BIN, namespace, value, NULL);
        _exit(1);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    }
}

static inline void ocws_emit_raw(const char *sfwbar_var, const char *value) {
    char cmd[512];
    if (value && (value[0] == '-' || (value[0] >= '0' && value[0] <= '9'))) {
        snprintf(cmd, sizeof(cmd), "sfwbar -R \"SetVal %s = %s\" 2>/dev/null", sfwbar_var, value);
    } else {
        snprintf(cmd, sizeof(cmd), "sfwbar -R \"SetVal %s = \\\"%s\\\"\" 2>/dev/null", sfwbar_var, value);
    }
    system(cmd);
}

#endif
