#ifndef OCWS_INI_H
#define OCWS_INI_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INI_MAX_SECTIONS 32
#define INI_MAX_KEYS 128
#define INI_KEY_LEN 64
#define INI_VAL_LEN 256

typedef struct {
    char key[INI_KEY_LEN];
    char value[INI_VAL_LEN];
} IniKV;

typedef struct {
    char name[INI_KEY_LEN];
    IniKV keys[INI_MAX_KEYS];
    int key_count;
} IniSection;

typedef struct {
    IniSection sections[INI_MAX_SECTIONS];
    int section_count;
} IniFile;

static inline void ini_trim(char *s) {
    while (isspace((unsigned char)*s)) memmove(s, s + 1, strlen(s));
    int len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) s[--len] = '\0';
}

static inline int ini_load(IniFile *ini, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    memset(ini, 0, sizeof(IniFile));
    IniSection *cur = NULL;
    char line[1024];

    while (fgets(line, sizeof(line), f)) {
        ini_trim(line);
        if (line[0] == '\0' || line[0] == '#' || line[0] == ';') continue;

        if (line[0] == '[') {
            char *end = strchr(line, ']');
            if (end && ini->section_count < INI_MAX_SECTIONS) {
                *end = '\0';
                cur = &ini->sections[ini->section_count++];
                strncpy(cur->name, line + 1, INI_KEY_LEN - 1);
                cur->key_count = 0;
            }
            continue;
        }

        char *eq = strchr(line, '=');
        if (eq && cur && cur->key_count < INI_MAX_KEYS) {
            *eq = '\0';
            IniKV *kv = &cur->keys[cur->key_count++];
            strncpy(kv->name, line, INI_KEY_LEN - 1);
            strncpy(kv->value, eq + 1, INI_VAL_LEN - 1);
            ini_trim(kv->name);
            ini_trim(kv->value);
        }
    }
    fclose(f);
    return 0;
}

static inline const char* ini_get(IniFile *ini, const char *section, const char *key) {
    for (int i = 0; i < ini->section_count; i++) {
        if (strcmp(ini->sections[i].name, section) == 0) {
            for (int j = 0; j < ini->sections[i].key_count; j++) {
                if (strcmp(ini->sections[i].keys[j].name, key) == 0)
                    return ini->sections[i].keys[j].value;
            }
        }
    }
    return NULL;
}

static inline int ini_get_int(IniFile *ini, const char *section, const char *key, int def) {
    const char *val = ini_get(ini, section, key);
    return val ? atoi(val) : def;
}

#endif
