/*
 * ocws-fonts-cli.c — OCWS Fonts CLI
 *
 * Centralized command-line interface for font management.
 * Subcommands: scan, list, info, install, remove, managed, scale, cache, config
 *
 * Build: zig build (handled by build.zig)
 */

#include "ocws-fonts.h"
#include <getopt.h>
#include <ctype.h>

static ocws_font_paths_t g_paths;

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s <command> [args]\n\n"
        "Commands:\n"
        "  scan                    Scan and list all system fonts\n"
        "  list [filter]           List fonts (optional family filter)\n"
        "  info <family>           Show details for a font family\n"
        "  packages                List available online font packages\n"
        "  install <package>       Install a font package from online source\n"
        "  remove <package>        Remove an OCWS-managed font package\n"
        "  managed                 List OCWS-managed font packages\n"
        "  scale                   Show current font scale\n"
        "  scale set <size>        Set global font size (6-24)\n"
        "  scale up [step]         Increase font size (default: 0.5)\n"
        "  scale down [step]       Decrease font size (default: 0.5)\n"
        "  scale reset             Reset to default size (10)\n"
        "  cache                   Rebuild font cache (fc-cache)\n"
        "  config                  Show fontconfig status\n\n"
        "Examples:\n"
        "  %s scan                 # List all installed fonts\n"
        "  %s list Noto            # Filter fonts by family name\n"
        "  %s install \"FiraCode NF\" # Install FiraCode Nerd Font\n"
        "  %s scale up             # Increase font size globally\n"
        "  %s scale set 12         # Set all fonts to 12pt\n",
        prog, prog, prog, prog, prog, prog);
}

/* ================================================================
 * Commands
 * ================================================================ */

static int cmd_scan(void) {
    ocws_font_list_t *list = ocws_fonts_scan();
    if (!list) {
        fprintf(stderr, "ERROR: fc-list failed\n");
        return 1;
    }

    printf("Family                              Style               Source   File\n");
    printf("─────────────────────────────────── ─────────────────── ──────── ──────────────────────────\n");

    for (int i = 0; i < list->count; i++) {
        ocws_font_entry_t *e = &list->entries[i];
        const char *src = e->is_managed ? "OCWS" : "System";
        const char *short_file = strrchr(e->file, '/');
        printf("%-35s %-19s %-8s %s\n",
               e->family, e->style, src,
               short_file ? short_file + 1 : e->file);
    }

    printf("\n%d font entries found.\n", list->count);
    ocws_font_list_free(list);
    return 0;
}

static int cmd_list(const char *filter) {
    ocws_font_list_t *list = ocws_fonts_scan();
    if (!list) {
        fprintf(stderr, "ERROR: fc-list failed\n");
        return 1;
    }

    int shown = 0;
    for (int i = 0; i < list->count; i++) {
        ocws_font_entry_t *e = &list->entries[i];
        if (filter && filter[0] != '\0') {
            char lower_family[256] = {0};
            char lower_filter[256] = {0};
            strncpy(lower_family, e->family, sizeof(lower_family) - 1);
            strncpy(lower_filter, filter, sizeof(lower_filter) - 1);
            for (char *p = lower_family; *p; p++) *p = tolower(*p);
            for (char *p = lower_filter; *p; p++) *p = tolower(*p);
            if (!strstr(lower_family, lower_filter)) continue;
        }
        printf("%-35s %-19s %s\n", e->family, e->style,
               e->is_managed ? "[OCWS]" : "");
        shown++;
    }

    if (filter && filter[0] != '\0') {
        printf("\n%d matches for \"%s\".\n", shown, filter);
    } else {
        printf("\n%d font entries.\n", shown);
    }

    ocws_font_list_free(list);
    return 0;
}

static int cmd_info(const char *family) {
    if (!family || family[0] == '\0') {
        fprintf(stderr, "Usage: ocws-fonts info <family>\n");
        return 1;
    }

    ocws_font_list_t *list = ocws_fonts_scan();
    if (!list) {
        fprintf(stderr, "ERROR: fc-list failed\n");
        return 1;
    }

    int found = 0;
    printf("Font: %s\n", family);
    printf("──────────────────────────────────────\n");

    for (int i = 0; i < list->count; i++) {
        ocws_font_entry_t *e = &list->entries[i];
        if (strcmp(e->family, family) == 0) {
            printf("  Style:  %s\n", e->style);
            printf("  File:   %s\n", e->file);
            printf("  Source: %s\n", e->is_managed ? "OCWS-managed" : "System");
            printf("\n");
            found++;
        }
    }

    if (!found) {
        /* Try case-insensitive partial match */
        for (int i = 0; i < list->count; i++) {
            ocws_font_entry_t *e = &list->entries[i];
            char lower_fam[256] = {0};
            char lower_q[256] = {0};
            strncpy(lower_fam, e->family, sizeof(lower_fam) - 1);
            strncpy(lower_q, family, sizeof(lower_q) - 1);
            for (char *p = lower_fam; *p; p++) *p = tolower(*p);
            for (char *p = lower_q; *p; p++) *p = tolower(*p);
            if (strstr(lower_fam, lower_q)) {
                printf("  Style:  %s\n", e->style);
                printf("  File:   %s\n", e->file);
                printf("  Source: %s\n", e->is_managed ? "OCWS-managed" : "System");
                printf("\n");
                found++;
            }
        }
    }

    printf("%d style(s) found.\n", found);
    ocws_font_list_free(list);
    return found > 0 ? 0 : 1;
}

static int cmd_packages(void) {
    printf("Package                  Category       Installed  Source\n");
    printf("────────────────────── ────────────── ────────── ─────────────────\n");

    for (int i = 0; i < OCWS_FONT_PACKAGE_COUNT; i++) {
        const ocws_font_pkg_t *pkg = &OCWS_FONT_PACKAGES[i];
        int installed = ocws_font_pkg_is_installed(pkg);
        int managed = ocws_font_manage_is_managed(pkg->name, &g_paths);
        const char *status = managed ? "OCWS" : (installed ? "Yes" : "No");
        printf("%-22s %-14s %-10s %s\n", pkg->name, pkg->category, status, pkg->desc);
    }

    printf("\n%d packages available.\n", OCWS_FONT_PACKAGE_COUNT);
    return 0;
}

static int cmd_install(const char *pkg_name) {
    if (!pkg_name || pkg_name[0] == '\0') {
        fprintf(stderr, "Usage: ocws-fonts install <package>\n");
        fprintf(stderr, "Run 'ocws-fonts packages' to see available packages.\n");
        return 1;
    }

    /* Find package */
    const ocws_font_pkg_t *pkg = NULL;
    for (int i = 0; i < OCWS_FONT_PACKAGE_COUNT; i++) {
        if (strcmp(OCWS_FONT_PACKAGES[i].name, pkg_name) == 0) {
            pkg = &OCWS_FONT_PACKAGES[i];
            break;
        }
    }

    if (!pkg) {
        fprintf(stderr, "ERROR: Unknown package: %s\n", pkg_name);
        fprintf(stderr, "Run 'ocws-fonts packages' to see available packages.\n");
        return 1;
    }

    const char *base = pkg->is_cursor ? g_paths.cursors_dir : g_paths.fonts_dir;
    char install_dir[512];
    snprintf(install_dir, sizeof(install_dir), "%s/%s", base, pkg->install_subdir);
    ocws_fonts_make_dir_p(install_dir);

    char tmp_path[512];
    snprintf(tmp_path, sizeof(tmp_path), "/tmp/ocws-font-%s", pkg->archive_name);
    char cmd[2048];

    printf("Installing %s...\n", pkg->name);
    printf("URL: %s\n", pkg->url);

    /* Download */
    printf("Downloading...\n");
    if (strstr(pkg->url, ".ttf") && !strstr(pkg->url, ".tar.") && !strstr(pkg->url, ".zip")) {
        snprintf(cmd, sizeof(cmd), "curl -fLsS -o '%s/%s' '%s'",
                 install_dir, pkg->archive_name, pkg->url);
    } else {
        snprintf(cmd, sizeof(cmd),
                 "curl -fLsS -o '%s' '%s' || wget -q -O '%s' '%s'",
                 tmp_path, pkg->url, tmp_path, pkg->url);
    }
    if (system(cmd) != 0) {
        fprintf(stderr, "ERROR: Download failed\n");
        return 1;
    }

    /* Extract if archive */
    if (!(strstr(pkg->url, ".ttf") && !strstr(pkg->url, ".tar.") && !strstr(pkg->url, ".zip"))) {
        printf("Extracting...\n");
        if (strstr(pkg->archive_name, ".zip")) {
            snprintf(cmd, sizeof(cmd), "unzip -qo '%s' -d '%s'", tmp_path, install_dir);
        } else if (strstr(pkg->archive_name, ".tar.xz")) {
            snprintf(cmd, sizeof(cmd), "tar -xJf '%s' -C '%s'", tmp_path, install_dir);
        } else if (strstr(pkg->archive_name, ".tar.gz") || strstr(pkg->archive_name, ".tgz")) {
            snprintf(cmd, sizeof(cmd), "tar -xzf '%s' -C '%s'", tmp_path, install_dir);
        } else {
            snprintf(cmd, sizeof(cmd), "cp '%s' '%s/'", tmp_path, install_dir);
        }
        system(cmd);
        snprintf(cmd, sizeof(cmd), "rm -f '%s'", tmp_path);
        system(cmd);
    }

    /* Mark as managed */
    ocws_font_manage_mark(pkg->name, pkg->url, &g_paths);
    printf("Tracked as OCWS-managed.\n");

    /* Rebuild cache */
    printf("Rebuilding font cache...\n");
    ocws_font_cache_rebuild();

    printf("✓ %s installed successfully.\n", pkg->name);
    return 0;
}

static int cmd_remove(const char *pkg_name) {
    if (!pkg_name || pkg_name[0] == '\0') {
        fprintf(stderr, "Usage: ocws-fonts remove <package>\n");
        fprintf(stderr, "Run 'ocws-fonts managed' to see managed packages.\n");
        return 1;
    }

    if (!ocws_font_manage_is_managed(pkg_name, &g_paths)) {
        fprintf(stderr, "ERROR: %s is not an OCWS-managed package.\n", pkg_name);
        return 1;
    }

    printf("Removing %s...\n", pkg_name);
    ocws_font_manage_remove(pkg_name, OCWS_FONT_PACKAGES, OCWS_FONT_PACKAGE_COUNT, &g_paths);
    printf("✓ %s removed.\n", pkg_name);
    return 0;
}

static int cmd_managed(void) {
    char names[128][256];
    int count = ocws_font_manage_list(&g_paths, names, 128);

    if (count == 0) {
        printf("No OCWS-managed fonts installed.\n");
        return 0;
    }

    printf("OCWS-Managed Font Packages:\n");
    printf("───────────────────────────\n");
    for (int i = 0; i < count; i++) {
        /* Try to find package info */
        const char *desc = "";
        for (int j = 0; j < OCWS_FONT_PACKAGE_COUNT; j++) {
            if (strcmp(OCWS_FONT_PACKAGES[j].name, names[i]) == 0) {
                desc = OCWS_FONT_PACKAGES[j].desc;
                break;
            }
        }
        printf("  • %s — %s\n", names[i], desc[0] ? desc : "custom");
    }

    printf("\n%d package(s) managed by OCWS.\n", count);
    return 0;
}

static int cmd_scale(int argc, char **argv) {
    if (argc < 1) {
        double sz = ocws_font_scale_get();
        char *family = ocws_font_scale_get_family();
        printf("Font: %s %.1f\n", family, sz);
        free(family);
        return 0;
    }

    const char *sub = argv[0];

    if (strcmp(sub, "set") == 0) {
        if (argc < 2) {
            fprintf(stderr, "Usage: ocws-fonts scale set <size>\n");
            return 1;
        }
        double sz = atof(argv[1]);
        if (sz < 6 || sz > 24) {
            fprintf(stderr, "Size must be between 6 and 24.\n");
            return 1;
        }
        ocws_font_scale_set(sz);
        printf("✓ Font size set to %.1f\n", sz);
        return 0;
    }

    if (strcmp(sub, "up") == 0) {
        double step = argc >= 2 ? atof(argv[1]) : 0.5;
        double before = ocws_font_scale_get();
        ocws_font_scale_step(step);
        printf("✓ Font size: %.1f → %.1f\n", before, ocws_font_scale_get());
        return 0;
    }

    if (strcmp(sub, "down") == 0) {
        double step = argc >= 2 ? atof(argv[1]) : 0.5;
        double before = ocws_font_scale_get();
        ocws_font_scale_step(-step);
        printf("✓ Font size: %.1f → %.1f\n", before, ocws_font_scale_get());
        return 0;
    }

    if (strcmp(sub, "reset") == 0) {
        ocws_font_scale_set(10.0);
        printf("✓ Font size reset to 10.0\n");
        return 0;
    }

    fprintf(stderr, "Unknown scale command: %s\n", sub);
    fprintf(stderr, "Available: set, up, down, reset\n");
    return 1;
}

static int cmd_cache(void) {
    printf("Rebuilding font cache...\n");
    int ret = ocws_font_cache_rebuild();
    if (ret == 0) {
        printf("✓ Font cache rebuilt.\n");
    } else {
        fprintf(stderr, "ERROR: fc-cache failed (exit code %d)\n", ret);
    }
    return ret;
}

static int cmd_config(void) {
    printf("Font Configuration Status\n");
    printf("─────────────────────────\n");

    const char *home = ocws_fonts_get_home();
    char path[512];

    /* fontconfig */
    snprintf(path, sizeof(path), "%s/.config/fontconfig/fonts.conf", home);
    printf("  fontconfig:  %s\n", ocws_fonts_file_exists(path) ? path : "not found");

    /* GTK3 */
    snprintf(path, sizeof(path), "%s/.config/gtk-3.0/settings.ini", home);
    if (ocws_fonts_file_exists(path)) {
        char buf[256] = {0};
        snprintf(buf, sizeof(buf), "grep '^gtk-font-name=' '%s'", path);
        char line[256] = {0};
        FILE *fp = popen(buf, "r");
        if (fp) { fgets(line, sizeof(line), fp); pclose(fp); }
        line[strcspn(line, "\n")] = 0;
        printf("  GTK3:        %s\n", line[0] ? line : "(no font set)");
    } else {
        printf("  GTK3:        not found\n");
    }

    /* GTK4 */
    snprintf(path, sizeof(path), "%s/.config/gtk-4.0/settings.ini", home);
    if (ocws_fonts_file_exists(path)) {
        char buf[256] = {0};
        snprintf(buf, sizeof(buf), "grep '^gtk-font-name=' '%s'", path);
        char line[256] = {0};
        FILE *fp = popen(buf, "r");
        if (fp) { fgets(line, sizeof(line), fp); pclose(fp); }
        line[strcspn(line, "\n")] = 0;
        printf("  GTK4:        %s\n", line[0] ? line : "(no font set)");
    } else {
        printf("  GTK4:        not found\n");
    }

    /* Font dirs */
    printf("\nFont Directories:\n");
    snprintf(path, sizeof(path), "%s/.local/share/fonts", home);
    printf("  User fonts:  %s %s\n", path, ocws_fonts_dir_exists(path) ? "✓" : "✗");
    snprintf(path, sizeof(path), "%s/.local/share/fonts/ocws-managed", home);
    printf("  Managed:     %s %s\n", path, ocws_fonts_dir_exists(path) ? "✓" : "✗");
    snprintf(path, sizeof(path), "%s/.local/share/icons", home);
    printf("  Cursors:     %s %s\n", path, ocws_fonts_dir_exists(path) ? "✓" : "✗");

    /* Managed count */
    char names[128][256];
    int managed_count = ocws_font_manage_list(&g_paths, names, 128);
    printf("\n  OCWS-managed packages: %d\n", managed_count);

    return 0;
}

/* ================================================================
 * Main
 * ================================================================ */

int main(int argc, char **argv) {
    ocws_fonts_init_paths(&g_paths);

    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "-h") == 0 || strcmp(cmd, "--help") == 0) {
        usage(argv[0]);
        return 0;
    }

    if (strcmp(cmd, "scan") == 0) return cmd_scan();
    if (strcmp(cmd, "list") == 0) return cmd_list(argc >= 3 ? argv[2] : NULL);
    if (strcmp(cmd, "info") == 0) return cmd_info(argc >= 3 ? argv[2] : NULL);
    if (strcmp(cmd, "packages") == 0) return cmd_packages();
    if (strcmp(cmd, "install") == 0) return cmd_install(argc >= 3 ? argv[2] : NULL);
    if (strcmp(cmd, "remove") == 0) return cmd_remove(argc >= 3 ? argv[2] : NULL);
    if (strcmp(cmd, "managed") == 0) return cmd_managed();
    if (strcmp(cmd, "scale") == 0) return cmd_scale(argc - 2, argv + 2);
    if (strcmp(cmd, "cache") == 0) return cmd_cache();
    if (strcmp(cmd, "config") == 0) return cmd_config();

    fprintf(stderr, "Unknown command: %s\n\n", cmd);
    usage(argv[0]);
    return 1;
}
