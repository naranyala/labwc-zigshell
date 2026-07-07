#ifndef OCWS_STRING_H
#define OCWS_STRING_H

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Pretty-print a slug (e.g. "my-theme" -> "My Theme") */
static inline char *ocws_str_prettify(const char *slug) {
    if (!slug) return NULL;
    char *buf = strdup(slug);
    if (!buf) return NULL;
    
    int cap = 1;
    for (int i = 0; buf[i]; i++) {
        if (buf[i] == '-' || buf[i] == '_') {
            buf[i] = ' ';
            cap = 1;
        } else if (cap) {
            buf[i] = toupper((unsigned char)buf[i]);
            cap = 0;
        }
    }
    return buf;
}

#endif
