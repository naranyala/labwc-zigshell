#include "utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>

/* ============================================================
 * Command execution
 * ============================================================ */

void run_cmd(GtkWidget *widget, gpointer data) {
    (void)widget;
    const char *cmd = (const char *)data;
    run_cmd_async(cmd);
}

/* ============================================================
 * Themes directory scanner
 * ============================================================ */

int scan_themes(const char *dir, char ***out_names, int max) {
    *out_names = calloc((size_t)max, sizeof(char *));
    if (!*out_names) return 0;
    int count = 0;
    DIR *d = opendir(dir);
    if (!d) return 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL && count < max) {
        const char *name = ent->d_name;
        const char *dot  = strrchr(name, '.');
        if (dot && strcmp(dot, ".ini") == 0) {
            size_t base_len = (size_t)(dot - name);
            (*out_names)[count] = strndup(name, base_len);
            count++;
        }
    }
    closedir(d);
    return count;
}

/* ============================================================
 * Shared Theme Data — single source of truth
 * ============================================================ */

const ocws_theme_entry_t OCWS_THEMES[] = {
    { "catppuccin-mocha", "#89b4fa", NULL },
    { "dracula",          "#bd93f9", NULL },
    { "tokyo-night",      "#7aa2f7", NULL },
    { "nord",             "#88c0d0", NULL },
    { "gruvbox",          "#d79921", NULL },
    { "rose-pine",        "#c4a7e7", NULL },
    { "everforest",       "#a7c080", NULL },
    { "one-dark",         "#61afef", NULL },
    { "solarized-dark",   "#268bd2", NULL },
    { "kanagawa",         "#7e9cd8", NULL },
    { "flexoki",          "#d0a215", NULL },
};

const int OCWS_THEME_COUNT = sizeof(OCWS_THEMES) / sizeof(OCWS_THEMES[0]);

/* ============================================================
 * Shared Shell Data — single source of truth
 * ============================================================ */

const ocws_shell_entry_t OCWS_SHELLS[] = {
    { "OCWS Double Panel", "doublepanel",
      "Default OCWS — dual panel\nsfwbar-based desktop",
      "preferences-desktop-display" },
    { "Crystal Dock",      "crystaldock",
      "SFWBar + macOS-style dock\nSmooth animations",
      "preferences-desktop-wallpaper" },
    { "DankMaterialShell", "dms",
      "Material 3 bar + dock\nDynamic color theming",
      "accessories-text-editor" },
    { "Noctalia",          "noctalia",
      "Minimal config shell\nClean & focused",
      "preferences-system" },
};

const int OCWS_SHELL_COUNT = sizeof(OCWS_SHELLS) / sizeof(OCWS_SHELLS[0]);

/* ============================================================
 * GTK Helpers
 * ============================================================ */

void highlight_selected(GtkWidget *btn) {
    GtkWidget *parent = gtk_widget_get_parent(btn);
    if (!parent) return;
    GList *children = gtk_container_get_children(GTK_CONTAINER(parent));
    for (GList *l = children; l; l = l->next) {
        GtkStyleContext *ctx = gtk_widget_get_style_context(GTK_WIDGET(l->data));
        gtk_style_context_remove_class(ctx, "suggested-action");
    }
    g_list_free(children);
    gtk_style_context_add_class(gtk_widget_get_style_context(btn), "suggested-action");
}
