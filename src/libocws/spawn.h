#ifndef OCWS_SPAWN_H
#define OCWS_SPAWN_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static inline void run_cmd_async(const char *cmd) {
    if (cmd && cmd[0]) {
        char full[1024];
        size_t len = strlen(cmd);
        int has_amp = 0;
        
        while (len > 0 && (cmd[len-1] == ' ' || cmd[len-1] == '\t')) len--;
        if (len > 0 && cmd[len-1] == '&') has_amp = 1;

        if (has_amp) {
            snprintf(full, sizeof(full), "%s", cmd);
        } else {
            snprintf(full, sizeof(full), "%s &", cmd);
        }
        
        // POSIX standard system call, zero GUI dependencies
        int ret = system(full);
        (void)ret; // Suppress unused result warning
    }
}

#endif
