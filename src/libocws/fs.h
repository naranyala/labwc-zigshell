#ifndef OCWS_FS_H
#define OCWS_FS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#define OCWS_CONFIG_DIR ".config/ocws"

static inline void get_config_dir(char *buf, size_t len) {
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(buf, len, "%s/%s", home, OCWS_CONFIG_DIR);
}

#endif
