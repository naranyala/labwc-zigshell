/*
 * ocws-dock-mgr.c — Dock Manager GUI
 *
 * GTK3 application for managing dock pinned apps across shells:
 *   - DankMaterialShell (session.json)
 *   - Noctalia (config.toml)
 *   - Crystal Dock (panel_1.conf)
 */

#include <gtk/gtk.h>
#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <json-c/json.h>
#include "ocws-theme-utils.h"
#include "../libocws/gtk.h"

#define APP_ID "org.ocws.dock-mgr"

typedef struct {
    char name[128];
} DockApp;

typedef struct {
    char shell[32];
    char config_path[512];
    DockApp apps[64];
    int app_count;
} DockConfig;

static DockConfig current_config = {0};
static GtkWidget *app_list = NULL;
static GtkWidget *status_label = NULL;
static GtkWidget *undo_btn = NULL;

/* Undo stack */
typedef struct {
    DockApp apps[64];
    int app_count;
    char removed_app[128];
} UndoEntry;

static UndoEntry undo_stack[32];
static int undo_count = 0;

/* Forward declarations */
static void refresh_app_list(void);
static void remove_app(GtkWidget *widget, gpointer data);

/* ============================================================
 * Detect Shell
 * ============================================================ */

static void detect_shell(void) {
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    char path[512];

    /* DMS */
    snprintf(path, sizeof(path), "%s/.local/state/DankMaterialShell/session.json", home);
    if (g_file_test(path, G_FILE_TEST_EXISTS)) {
        strcpy(current_config.shell, "dms");
        strcpy(current_config.config_path, path);
        return;
    }
    /* Noctalia */
    snprintf(path, sizeof(path), "%s/.config/noctalia/config.toml", home);
    if (g_file_test(path, G_FILE_TEST_EXISTS)) {
        strcpy(current_config.shell, "noctalia");
        strcpy(current_config.config_path, path);
        return;
    }
    /* Crystal Dock */
    snprintf(path, sizeof(path), "%s/.config/crystal-dock/panel_1.conf", home);
    if (g_file_test(path, G_FILE_TEST_EXISTS)) {
        strcpy(current_config.shell, "crystaldock");
        strcpy(current_config.config_path, path);
        return;
    }
    /* sfwbar */
    snprintf(path, sizeof(path), "%s/.config/ocws/sfwbar-dock.config", home);
    if (g_file_test(path, G_FILE_TEST_EXISTS)) {
        strcpy(current_config.shell, "sfwbar");
        strcpy(current_config.config_path, path);
        return;
    }
    strcpy(current_config.shell, "unknown");
}

/* ============================================================
 * Parse Pinned Apps
 * ============================================================ */

static void parse_dms_pinned(void) {
    FILE *f = fopen(current_config.config_path, "r");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *content = malloc(size + 1);
    fread(content, 1, size, f);
    fclose(f);
    content[size] = '\0';

    json_object *json = json_tokener_parse(content);
    free(content);
    if (!json) return;

    current_config.app_count = 0;
    json_object *apps_json;
    if (json_object_object_get_ex(json, "pinnedApps", &apps_json)) {
        int len = json_object_array_length(apps_json);
        for (int i = 0; i < len && i < 64; i++) {
            const char *app = json_object_get_string(json_object_array_get_idx(apps_json, i));
            strncpy(current_config.apps[current_config.app_count].name, app, 127);
            current_config.app_count++;
        }
    }
    json_object_put(json);
}

static void parse_noctalia_pinned(void) {
    FILE *f = fopen(current_config.config_path, "r");
    if (!f) return;
    current_config.app_count = 0;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (!strstr(line, "pinned")) continue;
        char *start = strchr(line, '[');
        char *end = strchr(line, ']');
        if (!start || !end) continue;
        start++;
        *end = '\0';
        char *token = strtok(start, "\", ");
        while (token && current_config.app_count < 64) {
            if (strlen(token) > 0) {
                strncpy(current_config.apps[current_config.app_count].name, token, 127);
                current_config.app_count++;
            }
            token = strtok(NULL, "\", ");
        }
        break;
    }
    fclose(f);
}

static void parse_crystaldock_pinned(void) {
    FILE *f = fopen(current_config.config_path, "r");
    if (!f) return;
    current_config.app_count = 0;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (!strstr(line, "launchers")) continue;
        char *start = strchr(line, '"');
        char *end = strrchr(line, '"');
        if (!start || !end) continue;
        start++;
        *end = '\0';
        char *token = strtok(start, ";");
        while (token && current_config.app_count < 64) {
            if (strcmp(token, "separator") != 0 &&
                strcmp(token, "show-desktop") != 0 &&
                strlen(token) > 0) {
                strncpy(current_config.apps[current_config.app_count].name, token, 127);
                current_config.app_count++;
            }
            token = strtok(NULL, ";");
        }
        break;
    }
    fclose(f);
}

static void parse_sfwbar_pinned(void) {
    current_config.app_count = 0;
    const char *home = getenv("HOME");
    char path[512];
    snprintf(path, sizeof(path), "%s/.config/ocws/sfwbar-dock.json", home ? home : "/tmp");
    FILE *f = fopen(path, "r");
    if (!f) {
        /* Defaults */
        const char *apps[] = {"chromium-browser", "org.gnome.Nautilus", "utilities-terminal", "org.kde.kate", "ocws-settings", "ocws-shot", NULL};
        for (int i = 0; apps[i]; i++) {
            strncpy(current_config.apps[current_config.app_count].name, apps[i], 127);
            current_config.app_count++;
        }
        return;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *content = malloc(size + 1);
    fread(content, 1, size, f);
    fclose(f);
    content[size] = '\0';

    json_object *json = json_tokener_parse(content);
    free(content);
    if (!json) return;

    json_object *apps_json;
    if (json_object_object_get_ex(json, "pinned", &apps_json)) {
        int len = json_object_array_length(apps_json);
        for (int i = 0; i < len && i < 64; i++) {
            const char *app = json_object_get_string(json_object_array_get_idx(apps_json, i));
            strncpy(current_config.apps[current_config.app_count].name, app, 127);
            current_config.app_count++;
        }
    }
    json_object_put(json);
}

static void load_pinned_apps(void) {
    if (strcmp(current_config.shell, "dms") == 0) parse_dms_pinned();
    else if (strcmp(current_config.shell, "noctalia") == 0) parse_noctalia_pinned();
    else if (strcmp(current_config.shell, "crystaldock") == 0) parse_crystaldock_pinned();
    else if (strcmp(current_config.shell, "sfwbar") == 0) parse_sfwbar_pinned();
}

/* ============================================================
 * Save Pinned Apps
 * ============================================================ */

static void save_dms_pinned(void) {
    FILE *f = fopen(current_config.config_path, "r");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *content = malloc(size + 1);
    fread(content, 1, size, f);
    fclose(f);
    content[size] = '\0';

    json_object *json = json_tokener_parse(content);
    free(content);
    if (!json) return;

    /* Replace pinnedApps array */
    json_object *new_apps = json_object_new_array();
    for (int i = 0; i < current_config.app_count; i++) {
        json_object_array_add(new_apps, json_object_new_string(current_config.apps[i].name));
    }
    json_object_object_add(json, "pinnedApps", new_apps);

    f = fopen(current_config.config_path, "w");
    if (f) {
        fprintf(f, "%s", json_object_to_json_string_ext(json, JSON_C_TO_STRING_PRETTY));
        fclose(f);
    }
    json_object_put(json);
}

static void save_noctalia_pinned(void) {
    FILE *f = fopen(current_config.config_path, "r");
    if (!f) return;
    char content[8192] = {0};
    fread(content, 1, sizeof(content) - 1, f);
    fclose(f);

    char pinned_str[2048] = "pinned = [";
    for (int i = 0; i < current_config.app_count; i++) {
        if (i > 0) strcat(pinned_str, ", ");
        strcat(pinned_str, "\"");
        strcat(pinned_str, current_config.apps[i].name);
        strcat(pinned_str, "\"");
    }
    strcat(pinned_str, "]");

    char *start = strstr(content, "pinned");
    if (start) {
        char *end = strchr(start, '\n');
        if (end) {
            memmove(start + strlen(pinned_str), end, strlen(end) + 1);
            memcpy(start, pinned_str, strlen(pinned_str));
        }
    }

    f = fopen(current_config.config_path, "w");
    if (f) { fwrite(content, 1, strlen(content), f); fclose(f); }
}

static void save_crystaldock_pinned(void) {
    FILE *f = fopen(current_config.config_path, "r");
    if (!f) return;
    char content[4096] = {0};
    fread(content, 1, sizeof(content) - 1, f);
    fclose(f);

    char launchers_str[2048] = "launchers=\"show-desktop;";
    for (int i = 0; i < current_config.app_count; i++) {
        strcat(launchers_str, current_config.apps[i].name);
        strcat(launchers_str, ";");
    }
    strcat(launchers_str, "separator;lxqt-lockscreen;lxqt-logout;separator\"");

    char *start = strstr(content, "launchers");
    if (start) {
        char *end = strchr(start, '\n');
        if (end) {
            memmove(start + strlen(launchers_str), end, strlen(end) + 1);
            memcpy(start, launchers_str, strlen(launchers_str));
        }
    }

    f = fopen(current_config.config_path, "w");
    if (f) { fwrite(content, 1, strlen(content), f); fclose(f); }
}

static void save_sfwbar_pinned(void) {
    const char *home = getenv("HOME");
    if (!home) return;
    
    char json_path[512];
    snprintf(json_path, sizeof(json_path), "%s/.config/ocws/sfwbar-dock.json", home);
    
    json_object *json = json_object_new_object();
    json_object *apps_arr = json_object_new_array();
    for (int i = 0; i < current_config.app_count; i++) {
        json_object_array_add(apps_arr, json_object_new_string(current_config.apps[i].name));
    }
    json_object_object_add(json, "pinned", apps_arr);
    
    FILE *f_json = fopen(json_path, "w");
    if (f_json) {
        fprintf(f_json, "%s", json_object_to_json_string_ext(json, JSON_C_TO_STRING_PRETTY));
        fclose(f_json);
    }
    json_object_put(json);

    /* Generate dock-apps.widget */
    char widget_path[512];
    snprintf(widget_path, sizeof(widget_path), "%s/.config/ocws/dock-apps.widget", home);
    FILE *f_widget = fopen(widget_path, "w");
    if (f_widget) {
        fprintf(f_widget, "#Api2\n# Auto-generated by ocws-dock-mgr\n\n");
        for (int i = 0; i < current_config.app_count; i++) {
            const char *app = current_config.apps[i].name;
            fprintf(f_widget, "button {\n");
            fprintf(f_widget, "  style = \"dock_app\"\n");
            fprintf(f_widget, "  tooltip = \"%s\"\n", app);
            
            /* Very basic action/icon fallback logic */
            const char *exec = app;
            const char *icon = app;
            if (strcmp(app, "utilities-terminal") == 0) exec = "xdg-terminal-exec";
            if (strcmp(app, "org.gnome.Nautilus") == 0) exec = "nautilus";
            if (strcmp(app, "org.kde.kate") == 0) exec = "kate";
            
            fprintf(f_widget, "  action = Exec(\"%s\")\n", exec);
            fprintf(f_widget, "  image {\n");
            fprintf(f_widget, "    icon = \"%s\"\n", icon);
            fprintf(f_widget, "    css = \"* { min-width: 32px; min-height: 32px; }\"\n");
            fprintf(f_widget, "  }\n");
            fprintf(f_widget, "}\n\n");
        }
        fclose(f_widget);
        
        /* Tell sfwbar to reload */
        system("killall -SIGUSR1 sfwbar 2>/dev/null");
    }
}

static void save_pinned_apps(void) {
    if (strcmp(current_config.shell, "dms") == 0) save_dms_pinned();
    else if (strcmp(current_config.shell, "noctalia") == 0) save_noctalia_pinned();
    else if (strcmp(current_config.shell, "crystaldock") == 0) save_crystaldock_pinned();
    else if (strcmp(current_config.shell, "sfwbar") == 0) save_sfwbar_pinned();
}

/* ============================================================
 * UI Callbacks
 * ============================================================ */

static void push_undo(void) {
    if (undo_count >= 32) {
        /* Shift stack */
        for (int i = 0; i < 31; i++)
            undo_stack[i] = undo_stack[i + 1];
        undo_count = 31;
    }
    UndoEntry *entry = &undo_stack[undo_count];
    memcpy(entry->apps, current_config.apps, sizeof(current_config.apps));
    entry->app_count = current_config.app_count;
    entry->removed_app[0] = '\0';
    undo_count++;
}

static void undo_remove(GtkWidget *widget, gpointer data) {
    (void)widget; (void)data;
    if (undo_count == 0) {
        gtk_label_set_text(GTK_LABEL(status_label), "Nothing to undo");
        return;
    }
    undo_count--;
    UndoEntry *entry = &undo_stack[undo_count];
    memcpy(current_config.apps, entry->apps, sizeof(current_config.apps));
    current_config.app_count = entry->app_count;
    refresh_app_list();
    char msg[256];
    snprintf(msg, sizeof(msg), "Undid removal of '%s'", entry->removed_app);
    gtk_label_set_text(GTK_LABEL(status_label), msg);
    gtk_widget_set_sensitive(undo_btn, undo_count > 0);
}

static void refresh_app_list(void) {
    if (!app_list) return;
    GList *children = gtk_container_get_children(GTK_CONTAINER(app_list));
    for (GList *l = children; l; l = l->next) gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(children);

    for (int i = 0; i < current_config.app_count; i++) {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_set_margin_top(row, 4);
        gtk_widget_set_margin_bottom(row, 4);

        GtkWidget *handle = gtk_label_new("⋮");
        gtk_widget_set_size_request(handle, 20, -1);
        gtk_box_pack_start(GTK_BOX(row), handle, FALSE, FALSE, 0);

        GtkWidget *name = gtk_label_new(current_config.apps[i].name);
        gtk_widget_set_halign(name, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(row), name, TRUE, TRUE, 0);

        GtkWidget *remove_btn = gtk_button_new_from_icon_name("list-remove-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
        g_object_set_data(G_OBJECT(remove_btn), "index", GINT_TO_POINTER(i));
        g_signal_connect(remove_btn, "clicked", G_CALLBACK(remove_app), NULL);
        gtk_box_pack_start(GTK_BOX(row), remove_btn, FALSE, FALSE, 0);

        gtk_container_add(GTK_CONTAINER(app_list), row);
    }
    gtk_widget_show_all(app_list);
}

static void remove_app(GtkWidget *widget, gpointer data) {
    (void)data;
    int index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "index"));
    if (index < 0 || index >= current_config.app_count) return;

    const char *app_name = current_config.apps[index].name;

    /* Confirmation dialog */
    GtkWidget *dialog = gtk_message_dialog_new(NULL,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_WARNING, GTK_BUTTONS_NONE,
        "Remove '%s' from dock?", app_name);
    gtk_dialog_add_buttons(GTK_DIALOG(dialog),
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Remove", GTK_RESPONSE_ACCEPT, NULL);

    char secondary[256];
    snprintf(secondary, sizeof(secondary),
        "This will remove '%s' from your pinned apps. You can undo this action.", app_name);
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", secondary);

    int response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    if (response != GTK_RESPONSE_ACCEPT) return;

    /* Push to undo stack */
    push_undo();
    strncpy(undo_stack[undo_count - 1].removed_app, app_name, 127);

    /* Remove app */
    for (int i = index; i < current_config.app_count - 1; i++)
        current_config.apps[i] = current_config.apps[i + 1];
    current_config.app_count--;

    refresh_app_list();

    char msg[256];
    snprintf(msg, sizeof(msg), "Removed '%s' (undo available)", app_name);
    gtk_label_set_text(GTK_LABEL(status_label), msg);
    gtk_widget_set_sensitive(undo_btn, TRUE);
}

static void add_app(GtkWidget *widget, gpointer data) {
    (void)widget; (void)data;
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Add App", NULL, GTK_DIALOG_MODAL,
        "_Cancel", GTK_RESPONSE_CANCEL, "_Add", GTK_RESPONSE_ACCEPT, NULL);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 20);
    
    GtkWidget *header = ocws_gtk_label_header("Add Application");
    gtk_box_pack_start(GTK_BOX(content), header, FALSE, FALSE, 0);

    GtkWidget *label = gtk_label_new("App ID (e.g., firefox):");
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(content), label, FALSE, FALSE, 4);

    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "firefox");
    gtk_box_pack_start(GTK_BOX(content), entry, FALSE, FALSE, 12);

    GtkWidget *hint = ocws_gtk_label_subtext("The identifier used by your shell.");
    gtk_widget_set_halign(hint, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(content), hint, FALSE, FALSE, 0);

    gtk_widget_show_all(dialog);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        const char *app_id = gtk_entry_get_text(GTK_ENTRY(entry));
        if (strlen(app_id) > 0 && current_config.app_count < 64) {
            strncpy(current_config.apps[current_config.app_count].name, app_id, 127);
            current_config.app_count++;
            refresh_app_list();
            gtk_label_set_text(GTK_LABEL(status_label), "App added (save to apply)");
        }
    }
    gtk_widget_destroy(dialog);
}

static void save_clicked(GtkWidget *widget, gpointer data) {
    (void)widget; (void)data;
    save_pinned_apps();
    gtk_label_set_text(GTK_LABEL(status_label), "Saved successfully");
}

static void backup_dock(GtkWidget *widget, gpointer data) {
    (void)widget;
    const char *name = (const char *)data;
    const char *home = getenv("HOME");
    if (!home) return;

    char path[512];
    snprintf(path, sizeof(path), "%s/.local/share/ocws/dock-backups", home);
    g_mkdir_with_parents(path, 0755);
    snprintf(path, sizeof(path), "%s/.local/share/ocws/dock-backups/%s.json", home, name);

    json_object *json = json_object_new_object();
    json_object_object_add(json, "name", json_object_new_string(name));
    json_object_object_add(json, "shell", json_object_new_string(current_config.shell));
    json_object *apps = json_object_new_array();
    for (int i = 0; i < current_config.app_count; i++)
        json_object_array_add(apps, json_object_new_string(current_config.apps[i].name));
    json_object_object_add(json, "pinned", apps);

    FILE *f = fopen(path, "w");
    if (f) { fprintf(f, "%s", json_object_to_json_string_ext(json, JSON_C_TO_STRING_PRETTY)); fclose(f); }
    json_object_put(json);
    gtk_label_set_text(GTK_LABEL(status_label), "Backup saved");
}

static void restore_dock(GtkWidget *widget, gpointer data) {
    (void)widget;
    const char *name = (const char *)data;
    const char *home = getenv("HOME");
    if (!home) return;

    char path[512];
    snprintf(path, sizeof(path), "%s/.local/share/ocws/dock-backups/%s.json", home, name);
    FILE *f = fopen(path, "r");
    if (!f) { gtk_label_set_text(GTK_LABEL(status_label), "Backup not found"); return; }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *content = malloc(size + 1);
    fread(content, 1, size, f);
    fclose(f);
    content[size] = '\0';

    json_object *json = json_tokener_parse(content);
    free(content);
    if (!json) { gtk_label_set_text(GTK_LABEL(status_label), "Invalid backup"); return; }

    json_object *apps_json;
    if (json_object_object_get_ex(json, "pinned", &apps_json)) {
        current_config.app_count = 0;
        int len = json_object_array_length(apps_json);
        for (int i = 0; i < len && i < 64; i++) {
            const char *app = json_object_get_string(json_object_array_get_idx(apps_json, i));
            strncpy(current_config.apps[current_config.app_count].name, app, 127);
            current_config.app_count++;
        }
    }
    json_object_put(json);
    refresh_app_list();
    gtk_label_set_text(GTK_LABEL(status_label), "Restored (save to apply)");
}

/* ============================================================
 * Main Window
 * ============================================================ */

static void activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;
    ocws_load_tokens();

    GtkWidget *window = gtk_application_window_new(app);
    ocws_gtk_enforce_premium_theme();
    ocws_gtk_apply_dynamic_css(app, NULL);
    gtk_window_set_title(GTK_WINDOW(window), "OCWS Dock Manager");
    gtk_window_set_default_size(GTK_WINDOW(window), 500, 600);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

    GtkWidget *header = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), TRUE);
    gtk_header_bar_set_title(GTK_HEADER_BAR(header), "Dock Manager");
    
    GtkWidget *shell_label = ocws_gtk_label_subtext(current_config.shell);
    gtk_header_bar_set_subtitle(GTK_HEADER_BAR(header), gtk_label_get_text(GTK_LABEL(shell_label)));
    gtk_widget_destroy(shell_label); // We only needed the text

    gtk_window_set_titlebar(GTK_WINDOW(window), header);

    GtkWidget *vbox = ocws_gtk_vbox(0);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    /* Toolbar */
    GtkWidget *toolbar = ocws_gtk_hbox(8);
    ocws_gtk_set_margins(toolbar, 8, 8, 12, 12);
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);

    GtkWidget *add_btn = gtk_button_new_with_label("+ Add");
    g_signal_connect(add_btn, "clicked", G_CALLBACK(add_app), NULL);
    gtk_box_pack_start(GTK_BOX(toolbar), add_btn, FALSE, FALSE, 0);

    undo_btn = gtk_button_new_from_icon_name("edit-undo-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_widget_set_tooltip_text(undo_btn, "Undo last removal");
    gtk_widget_set_sensitive(undo_btn, FALSE);
    g_signal_connect(undo_btn, "clicked", G_CALLBACK(undo_remove), NULL);
    gtk_box_pack_start(GTK_BOX(toolbar), undo_btn, FALSE, FALSE, 0);

    GtkWidget *spacer = gtk_label_new("");
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_pack_start(GTK_BOX(toolbar), spacer, TRUE, TRUE, 0);

    GtkWidget *save_btn = ocws_gtk_button_primary("Save");
    g_signal_connect(save_btn, "clicked", G_CALLBACK(save_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(toolbar), save_btn, FALSE, FALSE, 0);

    /* App list */
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);
    app_list = ocws_gtk_vbox(0);
    gtk_container_add(GTK_CONTAINER(scroll), app_list);

    /* Backup section */
    GtkWidget *backup_box = ocws_gtk_hbox(8);
    ocws_gtk_set_margins(backup_box, 0, 8, 12, 12);
    gtk_box_pack_start(GTK_BOX(vbox), backup_box, FALSE, FALSE, 0);

    GtkWidget *backup_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(backup_entry), "Backup name");
    gtk_box_pack_start(GTK_BOX(backup_box), backup_entry, TRUE, TRUE, 0);

    GtkWidget *backup_btn = gtk_button_new_with_label("Backup");
    g_signal_connect_swapped(backup_btn, "clicked", G_CALLBACK(backup_dock), backup_entry);
    gtk_box_pack_start(GTK_BOX(backup_box), backup_btn, FALSE, FALSE, 0);

    GtkWidget *restore_btn = gtk_button_new_with_label("Restore");
    g_signal_connect_swapped(restore_btn, "clicked", G_CALLBACK(restore_dock), backup_entry);
    gtk_box_pack_start(GTK_BOX(backup_box), restore_btn, FALSE, FALSE, 0);

    /* Status */
    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 0);
    status_label = ocws_gtk_label_subtext("Ready");
    gtk_widget_set_halign(status_label, GTK_ALIGN_START);
    ocws_gtk_set_margins(status_label, 0, 8, 12, 0);
    gtk_box_pack_start(GTK_BOX(vbox), status_label, FALSE, FALSE, 0);

    load_pinned_apps();
    refresh_app_list();
    gtk_widget_show_all(window);
}

int main(int argc, char **argv) {
    detect_shell();
    GtkApplication *app = gtk_application_new(APP_ID, G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
