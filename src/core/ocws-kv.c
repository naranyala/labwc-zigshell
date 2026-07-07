#include "ocws-kv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#define OCWS_KV_MAX_LINE 65536
#define OCWS_KV_INITIAL_CAP 64

typedef struct {
    char *key;
    char *value;
} ocws_kv_entry;

struct ocws_kv {
    char *path;
    ocws_kv_entry *entries;
    size_t count;
    size_t capacity;
    int dirty;
};

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    return d;
}

static void ensure_parent_dir(const char *path) {
    char *copy = xstrdup(path);
    char *slash = strrchr(copy, '/');
    if (slash) {
        *slash = '\0';
        /* recursive mkdir -p */
        for (char *p = copy + 1; *p; p++) {
            if (*p == '/') {
                *p = '\0';
                mkdir(copy, 0755);
                *p = '/';
            }
        }
        mkdir(copy, 0755);
    }
    free(copy);
}

/* Trim leading/trailing whitespace in place */
static char *trim(char *s) {
    if (!s) return NULL;
    while (*s == ' ' || *s == '\t') s++;
    char *end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' || end[-1] == '\r'))
        end--;
    *end = '\0';
    return s;
}

static void entry_free(ocws_kv_entry *e) {
    free(e->key);
    free(e->value);
    e->key = NULL;
    e->value = NULL;
}

ocws_kv *ocws_kv_open(const char *path) {
    if (!path) return NULL;

    ocws_kv *kv = calloc(1, sizeof(ocws_kv));
    if (!kv) return NULL;

    kv->path = xstrdup(path);
    kv->capacity = OCWS_KV_INITIAL_CAP;
    kv->entries = calloc(kv->capacity, sizeof(ocws_kv_entry));
    if (!kv->entries) {
        free(kv->path);
        free(kv);
        return NULL;
    }

    ensure_parent_dir(path);

    FILE *f = fopen(path, "r");
    if (f) {
        char line[OCWS_KV_MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            char *trimmed = trim(line);
            if (!*trimmed || *trimmed == '#') continue;

            char *eq = strchr(trimmed, '=');
            if (!eq) continue;

            *eq = '\0';
            char *key = trim(trimmed);
            char *value = trim(eq + 1);

            if (!*key) continue;

            /* skip duplicates — last wins */
            int found = 0;
            for (size_t i = 0; i < kv->count; i++) {
                if (strcmp(kv->entries[i].key, key) == 0) {
                    free(kv->entries[i].value);
                    kv->entries[i].value = xstrdup(value);
                    found = 1;
                    break;
                }
            }
            if (!found) {
                if (kv->count >= kv->capacity) {
                    size_t new_cap = kv->capacity * 2;
                    ocws_kv_entry *tmp = realloc(kv->entries, new_cap * sizeof(ocws_kv_entry));
                    if (!tmp) return NULL;
                    kv->entries = tmp;
                    kv->capacity = new_cap;
                }
                kv->entries[kv->count].key = xstrdup(key);
                kv->entries[kv->count].value = xstrdup(value);
                kv->count++;
            }
        }
        fclose(f);
    }

    return kv;
}

void ocws_kv_close(ocws_kv *kv) {
    if (!kv) return;
    if (kv->dirty) ocws_kv_flush(kv);
    for (size_t i = 0; i < kv->count; i++) entry_free(&kv->entries[i]);
    free(kv->entries);
    free(kv->path);
    free(kv);
}

char *ocws_kv_get(ocws_kv *kv, const char *key) {
    if (!kv || !key) return NULL;
    for (size_t i = 0; i < kv->count; i++) {
        if (strcmp(kv->entries[i].key, key) == 0)
            return xstrdup(kv->entries[i].value);
    }
    return NULL;
}

char *ocws_kv_get_or(ocws_kv *kv, const char *key, const char *def) {
    char *val = ocws_kv_get(kv, key);
    if (val) return val;
    return xstrdup(def);
}

int ocws_kv_set(ocws_kv *kv, const char *key, const char *value) {
    if (!kv || !key || !value) return -1;

    for (size_t i = 0; i < kv->count; i++) {
        if (strcmp(kv->entries[i].key, key) == 0) {
            free(kv->entries[i].value);
            kv->entries[i].value = xstrdup(value);
            kv->dirty = 1;
            return 0;
        }
    }

    if (kv->count >= kv->capacity) {
        size_t new_cap = kv->capacity * 2;
        ocws_kv_entry *tmp = realloc(kv->entries, new_cap * sizeof(ocws_kv_entry));
        if (!tmp) return -1;
        kv->entries = tmp;
        kv->capacity = new_cap;
    }
    kv->entries[kv->count].key = xstrdup(key);
    kv->entries[kv->count].value = xstrdup(value);
    kv->count++;
    kv->dirty = 1;
    return 0;
}

int ocws_kv_del(ocws_kv *kv, const char *key) {
    if (!kv || !key) return -1;

    for (size_t i = 0; i < kv->count; i++) {
        if (strcmp(kv->entries[i].key, key) == 0) {
            entry_free(&kv->entries[i]);
            /* shift remaining entries */
            for (size_t j = i; j < kv->count - 1; j++)
                kv->entries[j] = kv->entries[j + 1];
            kv->count--;
            kv->dirty = 1;
            return 0;
        }
    }
    return -1;
}

int ocws_kv_has(ocws_kv *kv, const char *key) {
    if (!kv || !key) return 0;
    for (size_t i = 0; i < kv->count; i++) {
        if (strcmp(kv->entries[i].key, key) == 0) return 1;
    }
    return 0;
}

int ocws_kv_list(ocws_kv *kv, const char *prefix, ocws_kv_list_fn callback, void *ctx) {
    if (!kv || !callback) return 0;

    size_t prefix_len = prefix ? strlen(prefix) : 0;
    int listed = 0;

    for (size_t i = 0; i < kv->count; i++) {
        if (prefix && strncmp(kv->entries[i].key, prefix, prefix_len) != 0)
            continue;
        callback(kv->entries[i].key, kv->entries[i].value, ctx);
        listed++;
    }
    return listed;
}

int ocws_kv_flush(ocws_kv *kv) {
    if (!kv) return -1;

    /* atomic write: write to temp, then rename */
    size_t path_len = strlen(kv->path);
    char *tmppath = malloc(path_len + 5);
    if (!tmppath) return -1;
    snprintf(tmppath, path_len + 5, "%s.tmp", kv->path);

    FILE *f = fopen(tmppath, "w");
    if (!f) {
        free(tmppath);
        return -1;
    }

    fprintf(f, "# OCWS State Store\n");
    for (size_t i = 0; i < kv->count; i++) {
        fprintf(f, "%s=%s\n", kv->entries[i].key, kv->entries[i].value);
    }
    fclose(f);

    if (rename(tmppath, kv->path) != 0) {
        /* fallback: remove and retry */
        remove(kv->path);
        rename(tmppath, kv->path);
    }
    free(tmppath);

    kv->dirty = 0;
    return 0;
}
