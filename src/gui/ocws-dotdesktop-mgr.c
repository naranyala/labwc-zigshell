/*
 * ocws-dotdesktop-mgr.c — Desktop Entry (.desktop) Manager GUI
 *
 * GTK3 application to manage, edit, backup, and restore .desktop files.
 */

#include <gtk/gtk.h>
#include "../libocws/gtk.h"
#include <glib.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>

#define APP_ID "org.ocws.dotdesktop-mgr"
#define BACKUP_DIR "/.local/share/ocws/dotdesktop-backups"

enum {
    COL_ICON,
    COL_NAME,
    COL_FILE,
    COL_PATH,
    NUM_COLS
};

static GtkListStore *store = NULL;
static GtkWidget *tree_view = NULL;
static GtkWidget *entry_name = NULL;
static GtkWidget *entry_exec = NULL;
static GtkWidget *entry_icon = NULL;
static GtkWidget *entry_comment = NULL;
static GtkWidget *entry_categories = NULL;
static GtkWidget *check_terminal = NULL;
static GtkWidget *status_label = NULL;
static GtkWidget *search_entry = NULL;
static GtkTreeModelFilter *filter_model = NULL;

static char current_file_path[512] = {0};

/* ============================================================
 * Backend Functions
 * ============================================================ */

static void ensure_backup_dir(void) {
    const char *home = getenv("HOME");
    if (!home) return;
    char path[512];
    snprintf(path, sizeof(path), "%s%s", home, BACKUP_DIR);
    g_mkdir_with_parents(path, 0755);
}

static void load_desktop_files_from_dir(const char *dir_path) {
    GDir *dir = g_dir_open(dir_path, 0, NULL);
    if (!dir) return;

    const char *filename;
    while ((filename = g_dir_read_name(dir)) != NULL) {
        if (g_str_has_suffix(filename, ".desktop")) {
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s", dir_path, filename);

            GKeyFile *key_file = g_key_file_new();
            if (g_key_file_load_from_file(key_file, path, G_KEY_FILE_NONE, NULL)) {
                gchar *name = g_key_file_get_locale_string(key_file, "Desktop Entry", "Name", NULL, NULL);
                gchar *icon_name = g_key_file_get_string(key_file, "Desktop Entry", "Icon", NULL);
                if (!name) name = g_strdup(filename);
                if (!icon_name) icon_name = g_strdup("application-x-executable");

                GtkTreeIter iter;
                gtk_list_store_append(store, &iter);
                gtk_list_store_set(store, &iter,
                    COL_ICON, icon_name,
                    COL_NAME, name,
                    COL_FILE, filename,
                    COL_PATH, path,
                    -1);

                g_free(name);
                g_free(icon_name);
            }
            g_key_file_free(key_file);
        }
    }
    g_dir_close(dir);
}

static void load_all_desktop_files(void) {
    gtk_list_store_clear(store);
    
    /* Load system wide */
    load_desktop_files_from_dir("/usr/share/applications");
    
    /* Load user local (overrides system) */
    const char *home = getenv("HOME");
    if (home) {
        char path[512];
        snprintf(path, sizeof(path), "%s/.local/share/applications", home);
        load_desktop_files_from_dir(path);
    }
}

static void clear_editor(void) {
    gtk_entry_set_text(GTK_ENTRY(entry_name), "");
    gtk_entry_set_text(GTK_ENTRY(entry_exec), "");
    gtk_entry_set_text(GTK_ENTRY(entry_icon), "");
    gtk_entry_set_text(GTK_ENTRY(entry_comment), "");
    gtk_entry_set_text(GTK_ENTRY(entry_categories), "");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_terminal), FALSE);
    current_file_path[0] = '\0';
}

static void load_file_to_editor(const char *path) {
    GKeyFile *key_file = g_key_file_new();
    if (g_key_file_load_from_file(key_file, path, G_KEY_FILE_NONE, NULL)) {
        gchar *name = g_key_file_get_locale_string(key_file, "Desktop Entry", "Name", NULL, NULL);
        gchar *exec = g_key_file_get_string(key_file, "Desktop Entry", "Exec", NULL);
        gchar *icon = g_key_file_get_string(key_file, "Desktop Entry", "Icon", NULL);
        gchar *comment = g_key_file_get_locale_string(key_file, "Desktop Entry", "Comment", NULL, NULL);
        gchar *categories = g_key_file_get_string(key_file, "Desktop Entry", "Categories", NULL);
        gboolean terminal = g_key_file_get_boolean(key_file, "Desktop Entry", "Terminal", NULL);

        gtk_entry_set_text(GTK_ENTRY(entry_name), name ? name : "");
        gtk_entry_set_text(GTK_ENTRY(entry_exec), exec ? exec : "");
        gtk_entry_set_text(GTK_ENTRY(entry_icon), icon ? icon : "");
        gtk_entry_set_text(GTK_ENTRY(entry_comment), comment ? comment : "");
        gtk_entry_set_text(GTK_ENTRY(entry_categories), categories ? categories : "");
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_terminal), terminal);

        g_free(name);
        g_free(exec);
        g_free(icon);
        g_free(comment);
        g_free(categories);
        
        strncpy(current_file_path, path, sizeof(current_file_path) - 1);
        char msg[1024];
        snprintf(msg, sizeof(msg), "Loaded: %s", g_path_get_basename(path));
        gtk_label_set_text(GTK_LABEL(status_label), msg);
    }
    g_key_file_free(key_file);
}

/* ============================================================
 * UI Callbacks
 * ============================================================ */

static void on_selection_changed(GtkTreeSelection *selection, gpointer data) {
    (void)data;
    GtkTreeIter iter;
    GtkTreeModel *model;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gchar *path;
        gtk_tree_model_get(model, &iter, COL_PATH, &path, -1);
        if (path) {
            load_file_to_editor(path);
            g_free(path);
        }
    }
}

static void on_save_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    (void)data;
    
    if (strlen(current_file_path) == 0) {
        gtk_label_set_text(GTK_LABEL(status_label), "No file selected to save!");
        return;
    }

    const char *name = gtk_entry_get_text(GTK_ENTRY(entry_name));
    const char *exec = gtk_entry_get_text(GTK_ENTRY(entry_exec));
    const char *icon = gtk_entry_get_text(GTK_ENTRY(entry_icon));
    const char *comment = gtk_entry_get_text(GTK_ENTRY(entry_comment));
    const char *categories = gtk_entry_get_text(GTK_ENTRY(entry_categories));
    gboolean terminal = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_terminal));

    /* If saving a system file, we must save to ~/.local/share/applications instead to avoid sudo */
    char save_path[1024];
    strncpy(save_path, current_file_path, sizeof(save_path));
    
    if (g_str_has_prefix(current_file_path, "/usr/")) {
        const char *home = getenv("HOME");
        if (home) {
            char *basename = g_path_get_basename(current_file_path);
            snprintf(save_path, sizeof(save_path), "%s/.local/share/applications/%s", home, basename);
            g_free(basename);
            g_mkdir_with_parents(g_path_get_dirname(save_path), 0755);
        }
    }

    GKeyFile *key_file = g_key_file_new();
    g_key_file_load_from_file(key_file, current_file_path, G_KEY_FILE_NONE, NULL);
    
    /* Ensure group exists */
    if (!g_key_file_has_group(key_file, "Desktop Entry")) {
        g_key_file_set_string(key_file, "Desktop Entry", "Type", "Application");
    }

    g_key_file_set_string(key_file, "Desktop Entry", "Name", name);
    g_key_file_set_string(key_file, "Desktop Entry", "Exec", exec);
    g_key_file_set_string(key_file, "Desktop Entry", "Icon", icon);
    if (strlen(comment) > 0) g_key_file_set_string(key_file, "Desktop Entry", "Comment", comment);
    if (strlen(categories) > 0) g_key_file_set_string(key_file, "Desktop Entry", "Categories", categories);
    g_key_file_set_boolean(key_file, "Desktop Entry", "Terminal", terminal);

    gsize length;
    gchar *content = g_key_file_to_data(key_file, &length, NULL);
    if (content) {
        if (g_file_set_contents(save_path, content, length, NULL)) {
            char msg[1024];
            snprintf(msg, sizeof(msg), "Saved to: %s", save_path);
            gtk_label_set_text(GTK_LABEL(status_label), msg);
            /* Update current path */
            strncpy(current_file_path, save_path, sizeof(current_file_path) - 1);
            load_all_desktop_files();
        } else {
            gtk_label_set_text(GTK_LABEL(status_label), "Failed to save file.");
        }
        g_free(content);
    }
    g_key_file_free(key_file);
}

static void on_backup_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    (void)data;

    if (strlen(current_file_path) == 0) {
        gtk_label_set_text(GTK_LABEL(status_label), "No file selected to backup!");
        return;
    }

    ensure_backup_dir();
    
    char *basename = g_path_get_basename(current_file_path);
    const char *home = getenv("HOME");
    if (!home) return;

    char backup_path[1024];
    snprintf(backup_path, sizeof(backup_path), "%s%s/%s", home, BACKUP_DIR, basename);

    gchar *content;
    gsize length;
    if (g_file_get_contents(current_file_path, &content, &length, NULL)) {
        if (g_file_set_contents(backup_path, content, length, NULL)) {
            char msg[1024];
            snprintf(msg, sizeof(msg), "Backed up to: %s", backup_path);
            gtk_label_set_text(GTK_LABEL(status_label), msg);
        }
        g_free(content);
    }
    g_free(basename);
}

static void on_restore_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    (void)data;

    if (strlen(current_file_path) == 0) {
        gtk_label_set_text(GTK_LABEL(status_label), "Select an app first to restore its backup!");
        return;
    }

    char *basename = g_path_get_basename(current_file_path);
    const char *home = getenv("HOME");
    if (!home) return;

    char backup_path[1024];
    snprintf(backup_path, sizeof(backup_path), "%s%s/%s", home, BACKUP_DIR, basename);

    if (g_file_test(backup_path, G_FILE_TEST_EXISTS)) {
        char dest_path[1024];
        snprintf(dest_path, sizeof(dest_path), "%s/.local/share/applications/%s", home, basename);

        gchar *content;
        gsize length;
        if (g_file_get_contents(backup_path, &content, &length, NULL)) {
            if (g_file_set_contents(dest_path, content, length, NULL)) {
                char msg[1024];
                snprintf(msg, sizeof(msg), "Restored from backup: %s", dest_path);
                gtk_label_set_text(GTK_LABEL(status_label), msg);
                load_all_desktop_files();
                load_file_to_editor(dest_path);
            }
            g_free(content);
        }
    } else {
        gtk_label_set_text(GTK_LABEL(status_label), "No backup found for this app!");
    }
    g_free(basename);
}

static void on_new_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    (void)data;
    clear_editor();
    
    const char *home = getenv("HOME");
    if (home) {
        snprintf(current_file_path, sizeof(current_file_path), "%s/.local/share/applications/new_app.desktop", home);
    }
    gtk_label_set_text(GTK_LABEL(status_label), "Created new template. Fill details and save.");
}

static gboolean search_filter_func(GtkTreeModel *model, GtkTreeIter *iter, gpointer data) {
    (void)data;
    const char *search_text = gtk_entry_get_text(GTK_ENTRY(search_entry));
    if (!search_text || strlen(search_text) == 0) return TRUE;

    gchar *name = NULL;
    gchar *file = NULL;
    gtk_tree_model_get(model, iter, COL_NAME, &name, COL_FILE, &file, -1);

    gboolean visible = FALSE;
    if (name) {
        gchar *name_lower = g_utf8_strdown(name, -1);
        gchar *search_lower = g_utf8_strdown(search_text, -1);
        if (strstr(name_lower, search_lower) != NULL) visible = TRUE;
        g_free(name_lower);
        g_free(search_lower);
    }
    if (!visible && file) {
        gchar *file_lower = g_utf8_strdown(file, -1);
        gchar *search_lower = g_utf8_strdown(search_text, -1);
        if (strstr(file_lower, search_lower) != NULL) visible = TRUE;
        g_free(file_lower);
        g_free(search_lower);
    }

    g_free(name);
    g_free(file);

    return visible;
}

static void on_search_changed(GtkEditable *editable, gpointer data) {
    (void)editable;
    (void)data;
    if (filter_model) {
        gtk_tree_model_filter_refilter(filter_model);
    }
}

/* ============================================================
 * UI Construction
 * ============================================================ */

static GtkWidget* create_editor_row(const char *label_text, GtkWidget **entry_widget) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_bottom(box, 8);
    
    GtkWidget *label = gtk_label_new(label_text);
    gtk_widget_set_size_request(label, 100, -1);
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);

    *entry_widget = gtk_entry_new();
    gtk_widget_set_hexpand(*entry_widget, TRUE);
    gtk_box_pack_start(GTK_BOX(box), *entry_widget, TRUE, TRUE, 0);

    return box;
}

static void activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;
    
    ocws_gtk_enforce_premium_theme();
    ocws_gtk_apply_dynamic_css(app, NULL);

    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "OCWS DotDesktop Manager");
    gtk_window_set_default_size(GTK_WINDOW(window), 900, 600);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

    /* Header bar */
    GtkWidget *header = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), TRUE);
    gtk_header_bar_set_title(GTK_HEADER_BAR(header), "Desktop Entries");
    gtk_window_set_titlebar(GTK_WINDOW(window), header);

    GtkWidget *new_btn = gtk_button_new_from_icon_name("document-new-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_widget_set_tooltip_text(new_btn, "New Desktop File");
    g_signal_connect(new_btn, "clicked", G_CALLBACK(on_new_clicked), NULL);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), new_btn);

    /* Main split container */
    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_container_add(GTK_CONTAINER(window), paned);
    gtk_paned_set_position(GTK_PANED(paned), 350);

    /* Left pane: Search + TreeView */
    GtkWidget *left_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_paned_pack1(GTK_PANED(paned), left_box, TRUE, FALSE);

    /* Search */
    search_entry = gtk_search_entry_new();
    gtk_widget_set_margin_top(search_entry, 8);
    gtk_widget_set_margin_bottom(search_entry, 8);
    gtk_widget_set_margin_start(search_entry, 8);
    gtk_widget_set_margin_end(search_entry, 8);
    g_signal_connect(search_entry, "changed", G_CALLBACK(on_search_changed), NULL);
    gtk_box_pack_start(GTK_BOX(left_box), search_entry, FALSE, FALSE, 0);

    /* TreeView */
    store = gtk_list_store_new(NUM_COLS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    filter_model = GTK_TREE_MODEL_FILTER(gtk_tree_model_filter_new(GTK_TREE_MODEL(store), NULL));
    gtk_tree_model_filter_set_visible_func(filter_model, search_filter_func, NULL, NULL);

    tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(filter_model));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree_view), FALSE);

    GtkCellRenderer *renderer_icon = gtk_cell_renderer_pixbuf_new();
    g_object_set(renderer_icon, "stock-size", GTK_ICON_SIZE_DND, NULL);
    GtkTreeViewColumn *col_icon = gtk_tree_view_column_new_with_attributes("Icon", renderer_icon, "icon-name", COL_ICON, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), col_icon);

    GtkCellRenderer *renderer_text = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *col_text = gtk_tree_view_column_new_with_attributes("Name", renderer_text, "text", COL_NAME, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), col_text);

    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));
    g_signal_connect(selection, "changed", G_CALLBACK(on_selection_changed), NULL);

    GtkWidget *scroll_tree = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_vexpand(scroll_tree, TRUE);
    gtk_container_add(GTK_CONTAINER(scroll_tree), tree_view);
    gtk_box_pack_start(GTK_BOX(left_box), scroll_tree, TRUE, TRUE, 0);

    /* Right pane: Editor */
    GtkWidget *right_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(right_box, 16);
    gtk_widget_set_margin_end(right_box, 16);
    gtk_widget_set_margin_top(right_box, 16);
    gtk_widget_set_margin_bottom(right_box, 16);
    gtk_paned_pack2(GTK_PANED(paned), right_box, TRUE, FALSE);

    GtkWidget *title = gtk_label_new("<span size='large' weight='bold'>Desktop Entry Editor</span>");
    gtk_label_set_use_markup(GTK_LABEL(title), TRUE);
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_widget_set_margin_bottom(title, 24);
    gtk_box_pack_start(GTK_BOX(right_box), title, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(right_box), create_editor_row("Name:", &entry_name), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(right_box), create_editor_row("Command:", &entry_exec), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(right_box), create_editor_row("Icon:", &entry_icon), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(right_box), create_editor_row("Comment:", &entry_comment), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(right_box), create_editor_row("Categories:", &entry_categories), FALSE, FALSE, 0);

    GtkWidget *term_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *spacer = gtk_label_new("");
    gtk_widget_set_size_request(spacer, 100, -1);
    gtk_box_pack_start(GTK_BOX(term_box), spacer, FALSE, FALSE, 0);
    
    check_terminal = gtk_check_button_new_with_label("Run in Terminal");
    gtk_box_pack_start(GTK_BOX(term_box), check_terminal, FALSE, FALSE, 0);
    gtk_widget_set_margin_bottom(term_box, 24);
    gtk_box_pack_start(GTK_BOX(right_box), term_box, FALSE, FALSE, 0);

    /* Buttons */
    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(right_box), btn_box, FALSE, FALSE, 0);

    GtkWidget *save_btn = gtk_button_new_with_label("Save");
    GtkStyleContext *ctx = gtk_widget_get_style_context(save_btn);
    gtk_style_context_add_class(ctx, "suggested-action");
    g_signal_connect(save_btn, "clicked", G_CALLBACK(on_save_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(btn_box), save_btn, FALSE, FALSE, 0);

    GtkWidget *backup_btn = gtk_button_new_with_label("Backup");
    g_signal_connect(backup_btn, "clicked", G_CALLBACK(on_backup_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(btn_box), backup_btn, FALSE, FALSE, 0);

    GtkWidget *restore_btn = gtk_button_new_with_label("Restore");
    g_signal_connect(restore_btn, "clicked", G_CALLBACK(on_restore_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(btn_box), restore_btn, FALSE, FALSE, 0);

    /* Spacer to push status to bottom */
    GtkWidget *expand = gtk_label_new("");
    gtk_widget_set_vexpand(expand, TRUE);
    gtk_box_pack_start(GTK_BOX(right_box), expand, TRUE, TRUE, 0);

    status_label = gtk_label_new("Ready");
    gtk_widget_set_halign(status_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(right_box), status_label, FALSE, FALSE, 0);

    load_all_desktop_files();

    gtk_widget_show_all(window);
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new(APP_ID, G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
