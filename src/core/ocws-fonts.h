/*
 * ocws-fonts.h — Centralized Font Utilities for OCWS
 *
 * Shared between CLI (ocws-fonts) and GUI (ocws-fonts-mgr).
 * Provides: font scanning, online install, managed tracking, font scale.
 */

#ifndef OCWS_FONTS_H
#define OCWS_FONTS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>

/* ================================================================
 * Types
 * ================================================================ */

typedef struct {
    char *family;
    char *style;
    char *file;
    int is_managed;
} ocws_font_entry_t;

typedef struct {
    ocws_font_entry_t *entries;
    int count;
    int capacity;
} ocws_font_list_t;

typedef struct {
    const char *name;
    const char *family;
    const char *desc;
    const char *category;
    const char *url;
    const char *archive_name;
    const char *install_subdir;
    int is_cursor;
} ocws_font_pkg_t;

/* ================================================================
 * Path Constants
 * ================================================================ */

/* Resolve OCWS font paths. Call once at startup. */
typedef struct {
    char fonts_dir[512];
    char managed_dir[512];
    char cursors_dir[512];
    char fontconfig_path[512];
} ocws_font_paths_t;

void ocws_fonts_init_paths(ocws_font_paths_t *paths);
const char *ocws_fonts_get_home(void);

/* ================================================================
 * System Font Scanning
 * ================================================================ */

/* Scan all system fonts via fc-list. Caller must free with ocws_font_list_free(). */
ocws_font_list_t *ocws_fonts_scan(void);

/* Free a font list. */
void ocws_font_list_free(ocws_font_list_t *list);

/* ================================================================
 * Font Packages (online sources)
 * ================================================================ */

/* Built-in font packages. */
extern const ocws_font_pkg_t OCWS_FONT_PACKAGES[];
extern const int OCWS_FONT_PACKAGE_COUNT;

/* Check if a package is installed (via fc-list). */
int ocws_font_pkg_is_installed(const ocws_font_pkg_t *pkg);

/* ================================================================
 * Managed Fonts (OCWS-installed)
 * ================================================================ */

/* Mark a font package as OCWS-managed. */
void ocws_font_manage_mark(const char *pkg_name, const char *url,
                           const ocws_font_paths_t *paths);

/* Check if a font package is managed by OCWS. */
int ocws_font_manage_is_managed(const char *pkg_name,
                                const ocws_font_paths_t *paths);

/* List managed font package names. Returns count, fills names array (max max_names). */
int ocws_font_manage_list(const ocws_font_paths_t *paths,
                          char names[][256], int max_names);

/* Remove a managed font package (delete files + metadata). */
int ocws_font_manage_remove(const char *pkg_name,
                            const ocws_font_pkg_t *pkgs, int pkg_count,
                            const ocws_font_paths_t *paths);

/* ================================================================
 * Font Cache
 * ================================================================ */

/* Rebuild fc-cache. Returns 0 on success. */
int ocws_font_cache_rebuild(void);

/* ================================================================
 * Font Scale (global scaling across all surfaces)
 * ================================================================ */

/* Get current font size (from gsettings or gtk settings.ini). Returns -1 on error. */
double ocws_font_scale_get(void);

/* Get current font family. Caller must free(). */
char *ocws_font_scale_get_family(void);

/* Set font size across all surfaces. */
int ocws_font_scale_set(double size);

/* Step font size up/down by delta. */
int ocws_font_scale_step(double delta);

/* ================================================================
 * Fontconfig
 * ================================================================ */

/* Check if fontconfig is configured. Returns 1 if present. */
int ocws_fontconfig_exists(void);

/* ================================================================
 * Utility Helpers (shared between CLI/GUI)
 * ================================================================ */

int ocws_fonts_dir_exists(const char *path);
int ocws_fonts_file_exists(const char *path);
void ocws_fonts_make_dir_p(const char *path);

#endif /* OCWS_FONTS_H */
