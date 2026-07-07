#ifndef OCWS_JSON_H
#define OCWS_JSON_H

#include <stdio.h>
#include <string.h>

static inline void json_escape(char *dst, size_t dst_len, const char *src) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j < dst_len - 2; i++) {
        switch (src[i]) {
            case '"':  dst[j++] = '\\'; dst[j++] = '"'; break;
            case '\\': dst[j++] = '\\'; dst[j++] = '\\'; break;
            case '\n': dst[j++] = '\\'; dst[j++] = 'n'; break;
            case '\t': dst[j++] = '\\'; dst[j++] = 't'; break;
            default:   dst[j++] = src[i]; break;
        }
    }
    dst[j] = '\0';
}

static inline void json_kv_string(char *buf, size_t len, const char *key, const char *val, int first) {
    char escaped[1024];
    json_escape(escaped, sizeof(escaped), val);
    snprintf(buf, len, "%s\"%s\": \"%s\"", first ? "" : ", ", key, escaped);
}

static inline void json_kv_int(char *buf, size_t len, const char *key, int val, int first) {
    snprintf(buf, len, "%s\"%s\": %d", first ? "" : ", ", key, val);
}

static inline void json_kv_bool(char *buf, size_t len, const char *key, int val, int first) {
    snprintf(buf, len, "%s\"%s\": %s", first ? "" : ", ", key, val ? "true" : "false");
}

#endif
