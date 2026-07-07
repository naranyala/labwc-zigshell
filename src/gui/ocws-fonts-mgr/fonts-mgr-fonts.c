/*
 * fonts-mgr-fonts.c — System font scanning, managed fonts tracking, search filter
 */

#include "fonts-mgr-common.h"

/* ============================================================
 * Managed Fonts Tracking
 * ============================================================ */

void fonts_mgr_mark_font_managed(const char *pkg_name, const char *url) {
    char meta_dir[512];
    snprintf(meta_dir, sizeof(meta_dir), "%s/%s", MANAGED_DIR, pkg_name);
    fonts_mgr_make_dir_p(meta_dir);

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

int fonts_mgr_is_font_managed(const char *pkg_name) {
    char meta_path[512];
    snprintf(meta_path, sizeof(meta_path), "%s/%s/.ocws-meta", MANAGED_DIR, pkg_name);
    return fonts_mgr_file_exists(meta_path);
}

void fonts_mgr_load_managed_list(GtkListStore *store) {
    gtk_list_store_clear(store);

    if (!fonts_mgr_dir_exists(MANAGED_DIR)) return;

    DIR *d = opendir(MANAGED_DIR);
    if (!d) return;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char meta_path[512];
        snprintf(meta_path, sizeof(meta_path), "%s/%s/.ocws-meta", MANAGED_DIR, entry->d_name);

        if (!fonts_mgr_file_exists(meta_path)) continue;

        FILE *f = fopen(meta_path, "r");
        if (!f) continue;

        char pkg_name[256] = {0};
        char url[512] = {0};
        char installed_time[128] = {0};
        char line[512];

        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\n")] = 0;
            if (sscanf(line, "package=%255s", pkg_name) == 1) continue;
            if (sscanf(line, "url=%511s", url) == 1) continue;
            if (sscanf(line, "installed=%127s", installed_time) == 1) continue;
        }
        fclose(f);

        GtkTreeIter iter;
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
            0, pkg_name[0] ? pkg_name : entry->d_name,
            1, url[0] ? url : "-",
            2, installed_time[0] ? installed_time : "-",
            -1);
    }
    closedir(d);
}

/* ============================================================
 * System Font Scanner
 * ============================================================ */

void fonts_mgr_free_system_fonts(void) {
    for (int i = 0; i < g_system_font_count; i++) {
        g_free(g_system_fonts[i].family);
        g_free(g_system_fonts[i].style);
        g_free(g_system_fonts[i].file);
    }
    g_free(g_system_fonts);
    g_system_fonts = NULL;
    g_system_font_count = 0;
    g_system_font_capacity = 0;
}

void fonts_mgr_scan_system_fonts(void) {
    fonts_mgr_free_system_fonts();

    fonts_mgr_log_msg("Scanning system fonts...");

    gchar *stdout_buf = NULL;
    gint exit_status;

    const gchar *argv[] = {"fc-list", ":", "file", "family", "style", NULL};

    if (!g_spawn_sync(NULL, (gchar **)argv, NULL, G_SPAWN_SEARCH_PATH,
                       NULL, NULL, &stdout_buf, NULL, &exit_status, NULL)) {
        fonts_mgr_log_msg("ERROR: fc-list failed");
        return;
    }

    gchar **lines = g_strsplit(stdout_buf, "\n", -1);
    for (int i = 0; lines[i] != NULL; i++) {
        if (strlen(lines[i]) == 0) continue;

        gchar **parts = g_strsplit(lines[i], ":", 3);
        if (g_strv_length(parts) >= 2) {
            if (g_system_font_count >= g_system_font_capacity) {
                g_system_font_capacity = g_system_font_capacity ? g_system_font_capacity * 2 : 512;
                g_system_fonts = g_realloc(g_system_fonts, g_system_font_capacity * sizeof(SystemFont));
            }

            SystemFont *sf = &g_system_fonts[g_system_font_count++];
            sf->file = g_strstrip(g_strdup(parts[0]));
            sf->family = g_strstrip(g_strdup(parts[1]));
            sf->style = g_strv_length(parts) == 3 ? g_strstrip(g_strdup(parts[2])) : g_strdup("Regular");

            sf->is_managed = 0;
            if (sf->file && strstr(sf->file, "ocws-managed")) {
                sf->is_managed = 1;
            }
        }
        g_strfreev(parts);
    }
    g_strfreev(lines);
    if (stdout_buf) g_free(stdout_buf);

    fonts_mgr_log_msg("Found %d font entries", g_system_font_count);
}

void fonts_mgr_populate_font_list(void) {
    gtk_list_store_clear(g_font_list_store);

    for (int i = 0; i < g_system_font_count; i++) {
        GtkTreeIter iter;
        gtk_list_store_append(g_font_list_store, &iter);
        gtk_list_store_set(g_font_list_store, &iter,
            0, g_system_fonts[i].family,
            1, g_system_fonts[i].style,
            2, g_system_fonts[i].file,
            3, g_system_fonts[i].is_managed ? "OCWS" : "System",
            -1);
    }

    if (g_stats_label) {
        char buf[128];
        int managed_count = 0;
        for (int i = 0; i < g_system_font_count; i++) {
            if (g_system_fonts[i].is_managed) managed_count++;
        }
        snprintf(buf, sizeof(buf), "%d fonts total, %d OCWS-managed", g_system_font_count, managed_count);
        gtk_label_set_text(g_stats_label, buf);
    }
}

/* ============================================================
 * Font Filter (search)
 * ============================================================ */

gboolean fonts_mgr_font_filter_func(GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data) {
    (void)user_data;
    const gchar *query = gtk_entry_get_text(GTK_ENTRY(g_search_entry));
    if (!query || query[0] == '\0') return TRUE;

    gchar *family = NULL;
    gchar *style = NULL;
    gchar *file = NULL;
    gtk_tree_model_get(model, iter, 0, &family, 1, &style, 2, &file, -1);

    gchar *query_lower = g_utf8_strdown(query, -1);
    gboolean match = FALSE;

    if (family) {
        gchar *f_lower = g_utf8_strdown(family, -1);
        if (strstr(f_lower, query_lower)) match = TRUE;
        g_free(f_lower);
    }
    if (!match && style) {
        gchar *s_lower = g_utf8_strdown(style, -1);
        if (strstr(s_lower, query_lower)) match = TRUE;
        g_free(s_lower);
    }
    if (!match && file) {
        gchar *fl_lower = g_utf8_strdown(file, -1);
        if (strstr(fl_lower, query_lower)) match = TRUE;
        g_free(fl_lower);
    }

    g_free(query_lower);
    if (family) g_free(family);
    if (style) g_free(style);
    if (file) g_free(file);

    return match;
}

void fonts_mgr_on_search_changed(GtkSearchEntry *entry, gpointer user_data) {
    (void)entry;
    (void)user_data;
    gtk_tree_model_filter_refilter(g_font_filter);
}
