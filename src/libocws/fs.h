#ifndef OCWS_FS_H
#define OCWS_FS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <pwd.h>
#include <unistd.h>

#define OCWS_CONFIG_DIR ".config/ocws"

static inline void get_config_dir(char *buf, size_t len) {
    const char *home = getenv("HOME");
    if (!home || !*home) {
        struct passwd *pw = getpwuid(getuid());
        home = pw ? pw->pw_dir : NULL;
    }
    if (!home || !*home) {
        buf[0] = '\0';
        return;
    }
    snprintf(buf, len, "%s/%s", home, OCWS_CONFIG_DIR);
}

#endif
