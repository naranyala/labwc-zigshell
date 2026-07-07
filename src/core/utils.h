#ifndef OCWS_UTILS_H
#define OCWS_UTILS_H

#include <stddef.h>
#include <glib.h>
#include <gtk/gtk.h>

/* Path helpers (now in libocws) */
#include "../libocws/fs.h"

/* Command execution (now in libocws) */
#include "../libocws/spawn.h"

/* String utilities (now in libocws) */
#include "../libocws/string.h"

/* GTK specific execution */
void run_cmd(GtkWidget *widget, gpointer data);

/* Themes directory scanner */
int scan_themes(const char *dir, char ***out_names, int max);

/* ============================================================
 * Shared Theme Data — single source of truth
 * ============================================================ */

typedef struct {
    const char *slug;
    const char *accent;
    const char *name;   /* pretty name, or NULL to auto-generate */
} ocws_theme_entry_t;

/* All built-in themes. Count = OCWS_THEME_COUNT. */
extern const ocws_theme_entry_t OCWS_THEMES[];
extern const int OCWS_THEME_COUNT;

/* ============================================================
 * Shared Shell Data — single source of truth
 * ============================================================ */

typedef struct {
    const char *name;
    const char *mode;
    const char *desc;
    const char *icon;
} ocws_shell_entry_t;

/* All shell modes. Count = OCWS_SHELL_COUNT. */
extern const ocws_shell_entry_t OCWS_SHELLS[];
extern const int OCWS_SHELL_COUNT;

/* ============================================================
 * GTK Helpers
 * ============================================================ */

/* Highlight the clicked button, remove highlight from siblings. */
void highlight_selected(GtkWidget *btn);

#endif
