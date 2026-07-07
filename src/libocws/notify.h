#ifndef OCWS_NOTIFY_H
#define OCWS_NOTIFY_H

#include <stdio.h>
#include <string.h>

static inline void ocws_notify(const char *title, const char *body, const char *icon) {
    char cmd[1024];
    if (icon && icon[0]) {
        snprintf(cmd, sizeof(cmd), "notify-send -a '%s' -t 3000 -i '%s' '%s' 2>/dev/null",
                 title, icon, body);
    } else {
        snprintf(cmd, sizeof(cmd), "notify-send -a '%s' -t 3000 '%s' 2>/dev/null",
                 title, body);
    }
    system(cmd);
}

static inline void ocws_notify_urgent(const char *title, const char *body) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "notify-send -u critical -a '%s' -t 5000 '%s' 2>/dev/null",
             title, body);
    system(cmd);
}

#endif
