/*
 * ocws-fonts.c — Centralized Font Utilities Implementation
 */

#include "ocws-fonts.h"

/* ================================================================
 * Paths
 * ================================================================ */

static char g_home_buf[512];

const char *ocws_fonts_get_home(void) {
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(g_home_buf, sizeof(g_home_buf), "%s", home);
    return g_home_buf;
}

void ocws_fonts_init_paths(ocws_font_paths_t *paths) {
    const char *home = ocws_fonts_get_home();
    snprintf(paths->fonts_dir, sizeof(paths->fonts_dir),
             "%s/.local/share/fonts", home);
    snprintf(paths->managed_dir, sizeof(paths->managed_dir),
             "%s/.local/share/fonts/ocws-managed", home);
    snprintf(paths->cursors_dir, sizeof(paths->cursors_dir),
             "%s/.local/share/icons", home);
    snprintf(paths->fontconfig_path, sizeof(paths->fontconfig_path),
             "%s/.config/fontconfig/fonts.conf", home);
}

/* ================================================================
 * Helpers
 * ================================================================ */

int ocws_fonts_dir_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

int ocws_fonts_file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

void ocws_fonts_make_dir_p(const char *path) {
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
}

static int run_silent(const char *cmd) {
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s 2>/dev/null", cmd);
    return system(buf);
}

static char *run_capture(const char *cmd, char *buf, size_t bufsz) {
    FILE *fp = popen(cmd, "r");
    if (!fp) { buf[0] = '\0'; return buf; }
    size_t n = fread(buf, 1, bufsz - 1, fp);
    buf[n] = '\0';
    pclose(fp);
    /* strip trailing newline */
    while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
    return buf;
}

/* ================================================================
 * System Font Scanning
 * ================================================================ */

ocws_font_list_t *ocws_fonts_scan(void) {
    ocws_font_list_t *list = calloc(1, sizeof(ocws_font_list_t));
    if (!list) return NULL;

    char buf[65536];
    run_capture("fc-list : file family style", buf, sizeof(buf));

    char *lines[4096];
    int nlines = 0;
    char *tok = strtok(buf, "\n");
    while (tok && nlines < 4096) {
        lines[nlines++] = tok;
        tok = strtok(NULL, "\n");
    }

    for (int i = 0; i < nlines; i++) {
        if (strlen(lines[i]) == 0) continue;

        /* Parse: file:family:style */
        char *p = lines[i];
        char *file_end = strchr(p, ':');
        if (!file_end) continue;
        *file_end = '\0';

        char *family_start = file_end + 1;
        char *style_start = strchr(family_start, ':');

        char *file = strdup(p);
        char *family = NULL;
        char *style = NULL;

        if (style_start) {
            *style_start = '\0';
            family = strdup(family_start);
            /* trim leading spaces */
            while (*family == ' ' || *family == '\t') memmove(family, family+1, strlen(family));
            style = strdup(style_start + 1);
            while (*style == ' ' || *style == '\t') memmove(style, style+1, strlen(style));
        } else {
            family = strdup(family_start);
            while (*family == ' ' || *family == '\t') memmove(family, family+1, strlen(family));
            style = strdup("Regular");
        }

        /* Ensure capacity */
        if (list->count >= list->capacity) {
            list->capacity = list->capacity ? list->capacity * 2 : 1024;
            list->entries = realloc(list->entries, list->capacity * sizeof(ocws_font_entry_t));
        }

        ocws_font_entry_t *e = &list->entries[list->count++];
        e->file = file;
        e->family = family;
        e->style = style;
        e->is_managed = (file && strstr(file, "ocws-managed")) ? 1 : 0;
    }

    return list;
}

void ocws_font_list_free(ocws_font_list_t *list) {
    if (!list) return;
    for (int i = 0; i < list->count; i++) {
        free(list->entries[i].family);
        free(list->entries[i].style);
        free(list->entries[i].file);
    }
    free(list->entries);
    free(list);
}

/* ================================================================
 * Font Packages
 * ================================================================ */

const ocws_font_pkg_t OCWS_FONT_PACKAGES[] = {
    /* UI Fonts */
    {"Noto Sans",      "Noto Sans",      "Google's default UI font",
     "UI Fonts",
     "https://github.com/googlefonts/noto-fonts/raw/main/hinted/ttf/NotoSans/NotoSans%5Bwdth%2Cwght%5D.ttf",
     "NotoSans.ttf", "noto-sans", 0},

    {"Noto Sans Mono", "Noto Sans Mono", "Monospace variant for terminals",
     "UI Fonts",
     "https://github.com/googlefonts/noto-fonts/raw/main/hinted/ttf/NotoSansMono/NotoSansMono%5Bwdth%2Cwght%5D.ttf",
     "NotoSansMono.ttf", "noto-sans-mono", 0},

    {"Inter",          "Inter",          "Modern UI alternative font",
     "UI Fonts",
     "https://github.com/rsms/inter/releases/download/v4.0/Inter-4.0.zip",
     "Inter-4.0.zip", "inter", 0},

    /* Nerd Fonts */
    {"JetBrains Mono NF", "JetBrainsMono Nerd Font", "Monospace with powerline glyphs",
     "Nerd Fonts",
     "https://github.com/ryanoasis/nerd-fonts/releases/latest/download/JetBrainsMono.tar.xz",
     "JetBrainsMono.tar.xz", "JetBrainsMono", 0},

    {"FiraCode NF",    "FiraCode Nerd Font", "Programming font with ligatures",
     "Nerd Fonts",
     "https://github.com/ryanoasis/nerd-fonts/releases/latest/download/FiraCode.tar.xz",
     "FiraCode.tar.xz", "FiraCode", 0},

    {"Hack NF",        "Hack Nerd Font",     "Source-code friendly font",
     "Nerd Fonts",
     "https://github.com/ryanoasis/nerd-fonts/releases/latest/download/Hack.tar.xz",
     "Hack.tar.xz", "Hack", 0},

    {"CascadiaCode NF","CascadiaCode Nerd Font", "Microsoft's coding font with glyphs",
     "Nerd Fonts",
     "https://github.com/ryanoasis/nerd-fonts/releases/latest/download/CascadiaCode.tar.xz",
     "CascadiaCode.tar.xz", "CascadiaCode", 0},

    {"MesloLGS NF",    "MesloLGS Nerd Font", "Powerline-patched font",
     "Nerd Fonts",
     "https://github.com/ryanoasis/nerd-fonts/releases/latest/download/Meslo.tar.xz",
     "Meslo.tar.xz", "MesloLGS NF", 0},

    /* Programming Fonts */
    {"Fira Code",      "Fira Code",          "Monospaced font with programming ligatures",
     "Programming",
     "https://github.com/tonsky/FiraCode/releases/download/6.2/Fira_Code_v6.2.zip",
     "Fira_Code_v6.2.zip", "fira-code", 0},

    {"Cascadia Code",  "Cascadia Code",      "Microsoft's modern coding font",
     "Programming",
     "https://github.com/microsoft/cascadia-code/releases/download/v2404.23/CascadiaCode-2404.23.zip",
     "CascadiaCode-2404.23.zip", "cascadia-code", 0},

    {"Source Code Pro","Source Code Pro",    "Adobe's monospaced font family",
     "Programming",
     "https://github.com/adobe-fonts/source-code-pro/releases/download/2.046R%2F%2F1.017R%2F%2F0.019R%2F%2FGR%2FOTF-source-code-pro-2.046R_1.017R_0.019R_20GR_OTF.zip",
     "source-code-pro.zip", "source-code-pro", 0},

    /* Cursor Themes */
    {"Bibata Modern Ice",  "Bibata-Modern-Ice",  "Modern cursor theme",
     "Cursors",
     "https://github.com/ful1e5/Bibata_Cursor/releases/latest/download/Bibata-Modern-Ice.tar.xz",
     "Bibata-Modern-Ice.tar.xz", "Bibata-Modern-Ice", 1},

    {"Bibata Classic Ice", "Bibata-Classic-Ice", "Classic cursor theme",
     "Cursors",
     "https://github.com/ful1e5/Bibata_Cursor/releases/latest/download/Bibata-Classic-Ice.tar.xz",
     "Bibata-Classic-Ice.tar.xz", "Bibata-Classic-Ice", 1},
};

const int OCWS_FONT_PACKAGE_COUNT = sizeof(OCWS_FONT_PACKAGES) / sizeof(OCWS_FONT_PACKAGES[0]);

int ocws_font_pkg_is_installed(const ocws_font_pkg_t *pkg) {
    if (!pkg || !pkg->family) return 0;
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "fc-list : family | grep -qi '%s'", pkg->family);
    return run_silent(cmd) == 0;
}

/* ================================================================
 * Managed Fonts
 * ================================================================ */

void ocws_font_manage_mark(const char *pkg_name, const char *url,
                           const ocws_font_paths_t *paths) {
    char meta_dir[512];
    snprintf(meta_dir, sizeof(meta_dir), "%s/%s", paths->managed_dir, pkg_name);
    ocws_fonts_make_dir_p(meta_dir);

    char meta_path[512];
    snprintf(meta_path, sizeof(meta_path), "%s/.ocws-meta", meta_dir);

    FILE *f = fopen(meta_path, "w");
    if (f) {
        time_t now = time(NULL);
        char timebuf[64];
        strftime(timebuf, sizeof(timebuf), "%Y-%m-%dT%H:%M:%S", localtime(&now));
        fprintf(f, "package=%s\n", pkg_name);
        fprintf(f, "url=%s\n", url);
        fprintf(f, "installed=%s\n", timebuf);
        fclose(f);
    }
}

int ocws_font_manage_is_managed(const char *pkg_name,
                                const ocws_font_paths_t *paths) {
    char meta_path[512];
    snprintf(meta_path, sizeof(meta_path), "%s/%s/.ocws-meta", paths->managed_dir, pkg_name);
    return ocws_fonts_file_exists(meta_path);
}

int ocws_font_manage_list(const ocws_font_paths_t *paths,
                          char names[][256], int max_names) {
    int count = 0;
    if (!ocws_fonts_dir_exists(paths->managed_dir)) return 0;

    DIR *d = opendir(paths->managed_dir);
    if (!d) return 0;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL && count < max_names) {
        if (entry->d_name[0] == '.') continue;
        char meta_path[512];
        snprintf(meta_path, sizeof(meta_path), "%s/%s/.ocws-meta",
                 paths->managed_dir, entry->d_name);
        if (ocws_fonts_file_exists(meta_path)) {
            strncpy(names[count], entry->d_name, 255);
            names[count][255] = '\0';
            count++;
        }
    }
    closedir(d);
    return count;
}

int ocws_font_manage_remove(const char *pkg_name,
                            const ocws_font_pkg_t *pkgs, int pkg_count,
                            const ocws_font_paths_t *paths) {
    /* Find package to get install_subdir */
    const ocws_font_pkg_t *pkg = NULL;
    for (int i = 0; i < pkg_count; i++) {
        if (strcmp(pkgs[i].name, pkg_name) == 0) {
            pkg = &pkgs[i];
            break;
        }
    }

    char cmd[1024];

    if (pkg) {
        const char *base = pkg->is_cursor ? paths->cursors_dir : paths->fonts_dir;
        char install_dir[512];
        snprintf(install_dir, sizeof(install_dir), "%s/%s", base, pkg->install_subdir);
        if (ocws_fonts_dir_exists(install_dir)) {
            snprintf(cmd, sizeof(cmd), "rm -rf '%s'", install_dir);
            system(cmd);
        }
    }

    /* Remove metadata */
    char meta_dir[512];
    snprintf(meta_dir, sizeof(meta_dir), "%s/%s", paths->managed_dir, pkg_name);
    if (ocws_fonts_dir_exists(meta_dir)) {
        snprintf(cmd, sizeof(cmd), "rm -rf '%s'", meta_dir);
        system(cmd);
    }

    /* Rebuild cache */
    ocws_font_cache_rebuild();
    return 0;
}

/* ================================================================
 * Font Cache
 * ================================================================ */

int ocws_font_cache_rebuild(void) {
    return run_silent("fc-cache -fv");
}

/* ================================================================
 * Font Scale
 * ================================================================ */

static char *scale_get_current_family(void) {
    char buf[256] = {0};
    FILE *fp = popen("gsettings get org.gnome.desktop.interface font-name 2>/dev/null", "r");
    if (fp) {
        fgets(buf, sizeof(buf), fp);
        pclose(fp);
    }
    /* Strip quotes */
    char *p = buf;
    while (*p == '\'' || *p == '"') p++;
    char *end = p + strlen(p);
    while (end > p && (*(end-1) == '\'' || *(end-1) == '"' || *(end-1) == '\n')) end--;
    *end = '\0';

    /* Extract family (everything before last space+number) */
    char *last_space = strrchr(p, ' ');
    if (last_space) {
        *last_space = '\0';
        return strdup(p);
    }
    return strdup("Noto Sans");
}

static double scale_get_current_size(void) {
    char buf[256] = {0};
    FILE *fp = popen("gsettings get org.gnome.desktop.interface font-name 2>/dev/null", "r");
    if (fp) {
        fgets(buf, sizeof(buf), fp);
        pclose(fp);
    }
    char *p = buf;
    while (*p == '\'' || *p == '"') p++;
    char *end = p + strlen(p);
    while (end > p && (*(end-1) == '\'' || *(end-1) == '"' || *(end-1) == '\n')) end--;
    *end = '\0';

    char *last_space = strrchr(p, ' ');
    if (last_space) {
        double sz = atof(last_space + 1);
        if (sz > 0) return sz;
    }
    return 10.0;
}

double ocws_font_scale_get(void) {
    return scale_get_current_size();
}

char *ocws_font_scale_get_family(void) {
    return scale_get_current_family();
}

int ocws_font_scale_set(double size) {
    if (size < 6) size = 6;
    if (size > 24) size = 24;

    char *family = ocws_font_scale_get_family();
    char cmd[512];

    /* gsettings */
    snprintf(cmd, sizeof(cmd),
             "gsettings set org.gnome.desktop.interface font-name '%s %.1f' 2>/dev/null",
             family, size);
    run_silent(cmd);

    /* GTK3 settings.ini */
    char gtk3[512];
    const char *home = ocws_fonts_get_home();
    snprintf(gtk3, sizeof(gtk3), "%s/.config/gtk-3.0/settings.ini", home);
    if (ocws_fonts_file_exists(gtk3)) {
        snprintf(cmd, sizeof(cmd),
                 "sed -i 's/^gtk-font-name=.*/gtk-font-name=%s, %.1f/' '%s' 2>/dev/null",
                 family, size, gtk3);
        run_silent(cmd);
    }

    /* GTK4 settings.ini */
    char gtk4[512];
    snprintf(gtk4, sizeof(gtk4), "%s/.config/gtk-4.0/settings.ini", home);
    if (ocws_fonts_file_exists(gtk4)) {
        snprintf(cmd, sizeof(cmd),
                 "sed -i 's/^gtk-font-name=.*/gtk-font-name=%s, %.1f/' '%s' 2>/dev/null",
                 family, size, gtk4);
        run_silent(cmd);
    }

    /* Live reload labwc */
    run_silent("kill -SIGHUP $(pidof labwc) 2>/dev/null");

    free(family);
    return 0;
}

int ocws_font_scale_step(double delta) {
    double cur = ocws_font_scale_get();
    double next = cur + delta;
    if (next < 6) next = 6;
    if (next > 24) next = 24;
    return ocws_font_scale_set(next);
}

/* ================================================================
 * Fontconfig
 * ================================================================ */

int ocws_fontconfig_exists(void) {
    const char *home = ocws_fonts_get_home();
    char path[512];
    snprintf(path, sizeof(path), "%s/.config/fontconfig/fonts.conf", home);
    return ocws_fonts_file_exists(path);
}
