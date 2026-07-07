/*
 * fonts-mgr-installer.c — Online install/remove workers + font-scale
 */

#include "fonts-mgr-common.h"

/* ============================================================
 * Install Worker
 * ============================================================ */

gpointer fonts_mgr_install_worker(gpointer user_data) {
    InstallRowData *d = (InstallRowData *)user_data;
    FontPackage *fp = &g_packages[d->pkg_index];
    const ocws_font_pkg_t *pkg = fp->pkg;

    fonts_mgr_set_label_async(d->status_lbl, "Installing...");

    fonts_mgr_log_msg("=== Installing %s ===", pkg->name);
    fonts_mgr_log_msg("URL: %s", pkg->url);

    /* Determine install directory */
    const char *base_dir = pkg->is_cursor ? CURSORS_DIR : FONTS_DIR;
    char install_dir[512];
    snprintf(install_dir, sizeof(install_dir), "%s/%s", base_dir, pkg->install_subdir);
    fonts_mgr_make_dir_p(install_dir);

    /* Download to /tmp */
    char tmp_path[512];
    snprintf(tmp_path, sizeof(tmp_path), "/tmp/ocws-font-%s", pkg->archive_name);

    char cmd[2048];

    /* Download */
    fonts_mgr_set_label_async(d->status_lbl, "Downloading...");

    if (strstr(pkg->url, ".ttf") && !strstr(pkg->url, ".tar.") && !strstr(pkg->url, ".zip")) {
        /* Single TTF file — direct download */
        snprintf(cmd, sizeof(cmd), "curl -fLsS -o '%s/%s' '%s' 2>&1",
            install_dir, pkg->archive_name, pkg->url);
        fonts_mgr_run_cmd_logged(cmd);
    } else {
        /* Archive — download then extract */
        snprintf(cmd, sizeof(cmd),
            "curl -fLsS -o '%s' '%s' 2>&1 || wget -q -O '%s' '%s' 2>&1",
            tmp_path, pkg->url, tmp_path, pkg->url);
        fonts_mgr_run_cmd_logged(cmd);

        /* Extract */
        fonts_mgr_set_label_async(d->status_lbl, "Extracting...");

        const char *ext = strrchr(pkg->archive_name, '.');
        if (ext && strcmp(ext, ".zip") == 0) {
            snprintf(cmd, sizeof(cmd), "unzip -qo '%s' -d '%s' 2>&1", tmp_path, install_dir);
        } else if (strstr(pkg->archive_name, ".tar.xz")) {
            snprintf(cmd, sizeof(cmd), "tar -xJf '%s' -C '%s' 2>&1", tmp_path, install_dir);
        } else if (strstr(pkg->archive_name, ".tar.gz") || strstr(pkg->archive_name, ".tgz")) {
            snprintf(cmd, sizeof(cmd), "tar -xzf '%s' -C '%s' 2>&1", tmp_path, install_dir);
        } else {
            snprintf(cmd, sizeof(cmd), "cp '%s' '%s/' 2>&1", tmp_path, install_dir);
        }
        fonts_mgr_run_cmd_logged(cmd);

        /* Cleanup temp */
        snprintf(cmd, sizeof(cmd), "rm -f '%s'", tmp_path);
        system(cmd);
    }

    /* Mark as managed */
    fonts_mgr_set_label_async(d->status_lbl, "Tracking...");

    fonts_mgr_mark_font_managed(pkg->name, pkg->url);
    fonts_mgr_log_msg("Marked as OCWS-managed: %s", pkg->name);

    /* Rebuild font cache */
    fonts_mgr_set_label_async(d->status_lbl, "Rebuilding cache...");

    fonts_mgr_log_msg("Rebuilding font cache...");
    fonts_mgr_run_cmd_logged("fc-cache -fv 2>&1 | tail -5");

    /* Update UI */
    fonts_mgr_set_label_async(d->status_lbl, "Installed");

    if (d->btn) {
        fonts_mgr_set_sensitive_async(d->btn, TRUE);
    }

    fonts_mgr_log_msg("=== %s installation complete ===", pkg->name);

    /* Refresh managed list */
    gdk_threads_add_idle_full(G_PRIORITY_HIGH,
        (GSourceFunc)fonts_mgr_load_managed_list, g_managed_store, NULL);

    return NULL;
}

void fonts_mgr_on_install_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    InstallRowData *d = (InstallRowData *)data;
    gtk_widget_set_sensitive(d->btn, FALSE);
    g_thread_new("install-font", fonts_mgr_install_worker, d);
}

/* ============================================================
 * Remove Worker
 * ============================================================ */

gpointer fonts_mgr_remove_worker(gpointer user_data) {
    RemoveRowData *d = (RemoveRowData *)user_data;

    fonts_mgr_log_msg("=== Removing managed font: %s ===", d->pkg_name);

    /* Find which package this is */
    const ocws_font_pkg_t *pkg = NULL;
    for (int i = 0; i < OCWS_FONT_PACKAGE_COUNT; i++) {
        if (strcmp(g_packages[i].pkg->name, d->pkg_name) == 0) {
            pkg = g_packages[i].pkg;
            break;
        }
    }

    if (!pkg) {
        fonts_mgr_log_msg("ERROR: Package not found: %s", d->pkg_name);
        g_free(d);
        return NULL;
    }

    /* Remove the installed directory */
    const char *base_dir = pkg->is_cursor ? CURSORS_DIR : FONTS_DIR;
    char install_dir[512];
    snprintf(install_dir, sizeof(install_dir), "%s/%s", base_dir, pkg->install_subdir);

    char cmd[1024];
    if (fonts_mgr_dir_exists(install_dir)) {
        snprintf(cmd, sizeof(cmd), "rm -rf '%s'", install_dir);
        fonts_mgr_run_cmd_logged(cmd);
    }

    /* Remove managed metadata */
    char meta_dir[512];
    snprintf(meta_dir, sizeof(meta_dir), "%s/%s", MANAGED_DIR, d->pkg_name);
    if (fonts_mgr_dir_exists(meta_dir)) {
        snprintf(cmd, sizeof(cmd), "rm -rf '%s'", meta_dir);
        fonts_mgr_run_cmd_logged(cmd);
    }

    /* Rebuild font cache */
    fonts_mgr_log_msg("Rebuilding font cache...");
    fonts_mgr_run_cmd_logged("fc-cache -fv 2>&1 | tail -5");

    fonts_mgr_log_msg("=== %s removed ===", d->pkg_name);

    /* Refresh lists */
    gdk_threads_add_idle_full(G_PRIORITY_HIGH,
        (GSourceFunc)fonts_mgr_load_managed_list, g_managed_store, NULL);

    g_free(d);
    return NULL;
}

void fonts_mgr_on_remove_managed(GtkWidget *widget, gpointer tree_view) {
    (void)widget;
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (!gtk_tree_selection_get_selected(sel, &model, &iter)) {
        fonts_mgr_log_msg("No font selected for removal");
        return;
    }

    char *name = NULL;
    char *url = NULL;
    gtk_tree_model_get(model, &iter, 0, &name, 1, &url, -1);

    if (!name) return;

    RemoveRowData *d = g_new0(RemoveRowData, 1);
    strncpy(d->pkg_name, name, sizeof(d->pkg_name) - 1);
    if (url) strncpy(d->pkg_url, url, sizeof(d->pkg_url) - 1);

    g_free(name);
    if (url) g_free(url);

    g_thread_new("remove-font", fonts_mgr_remove_worker, d);
}

/* ============================================================
 * Font Scale Integration
 * ============================================================ */

void fonts_mgr_on_font_scale_up(GtkWidget *widget, gpointer data) {
    (void)widget; (void)data;
    fonts_mgr_log_msg("Running: font-scale up");
    fonts_mgr_run_cmd_logged("font-scale up 2>&1");
}

void fonts_mgr_on_font_scale_down(GtkWidget *widget, gpointer data) {
    (void)widget; (void)data;
    fonts_mgr_log_msg("Running: font-scale down");
    fonts_mgr_run_cmd_logged("font-scale down 2>&1");
}

void fonts_mgr_on_font_scale_status(GtkWidget *widget, gpointer data) {
    (void)widget; (void)data;
    fonts_mgr_log_msg("Running: font-scale status");
    fonts_mgr_run_cmd_logged("font-scale status 2>&1");
}

void fonts_mgr_on_font_scale_reset(GtkWidget *widget, gpointer data) {
    (void)widget; (void)data;
    fonts_mgr_log_msg("Running: font-scale reset");
    fonts_mgr_run_cmd_logged("font-scale reset 2>&1");
}

/* ============================================================
 * Rebuild Font Cache
 * ============================================================ */

gpointer fonts_mgr_rebuild_cache_worker(gpointer user_data) {
    GtkWidget *label = GTK_WIDGET(user_data);
    fonts_mgr_set_label_async(label, "Rebuilding...");
    fonts_mgr_log_msg("Rebuilding font cache...");
    fonts_mgr_run_cmd_logged("fc-cache -fv 2>&1 | tail -5");
    fonts_mgr_log_msg("Font cache rebuilt");
    fonts_mgr_set_label_async(label, "Done");
    return NULL;
}

void fonts_mgr_on_rebuild_cache(GtkWidget *widget, gpointer data) {
    (void)widget;
    g_thread_new("rebuild-cache", fonts_mgr_rebuild_cache_worker, data);
}
