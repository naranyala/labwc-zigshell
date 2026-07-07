#include <gtk/gtk.h>
#include "../libocws/gtk.h"
#include <json-c/json.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

static GtkWidget *chat_view = NULL;
static GtkTextBuffer *chat_buffer = NULL;
static GtkWidget *input_entry = NULL;
static GtkWidget *send_button = NULL;
static GtkWidget *ocr_button = NULL;
static GtkWidget *model_combo = NULL;
static GtkWidget *load_button = NULL;
static GtkWidget *eject_button = NULL;
static GtkWidget *clear_button = NULL;

static int server_stdin_fd = -1;
static GPid server_pid = 0;

static void append_to_chat(const char *sender, const char *message, const char *tag_name) {
    GtkTextIter iter;
    gtk_text_buffer_get_end_iter(chat_buffer, &iter);
    
    if (sender) {
        char *header = g_strdup_printf("\n[%s]\n", sender);
        gtk_text_buffer_insert_with_tags_by_name(chat_buffer, &iter, header, -1, "bold", NULL);
        g_free(header);
    }
    
    if (message) {
        if (tag_name) {
            gtk_text_buffer_insert_with_tags_by_name(chat_buffer, &iter, message, -1, tag_name, NULL);
        } else {
            gtk_text_buffer_insert(chat_buffer, &iter, message, -1);
        }
    }
    
    /* Scroll to bottom */
    gtk_text_buffer_get_end_iter(chat_buffer, &iter);
    GtkTextMark *mark = gtk_text_buffer_create_mark(chat_buffer, NULL, &iter, FALSE);
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(chat_view), mark, 0.0, TRUE, 0.0, 1.0);
    gtk_text_buffer_delete_mark(chat_buffer, mark);
}

static void send_command(const char *cmd_type, const char *key, const char *value) {
    if (server_stdin_fd == -1) return;
    
    json_object *jobj = json_object_new_object();
    json_object_object_add(jobj, "type", json_object_new_string(cmd_type));
    if (key && value) {
        json_object_object_add(jobj, key, json_object_new_string(value));
    }
    
    const char *json_str = json_object_to_json_string(jobj);
    dprintf(server_stdin_fd, "%s\n", json_str);
    
    json_object_put(jobj);
}

static gboolean on_server_stdout(GIOChannel *source, GIOCondition condition, gpointer data) {
    char *str_return = NULL;
    gsize length = 0;
    gsize terminator_pos = 0;
    GError *error = NULL;

    GIOStatus status = g_io_channel_read_line(source, &str_return, &length, &terminator_pos, &error);
    
    if (status == G_IO_STATUS_EOF || status == G_IO_STATUS_ERROR) {
        append_to_chat("System", "LLM Server disconnected.", "error");
        return FALSE;
    }
    
    if (status == G_IO_STATUS_NORMAL && str_return != NULL) {
        json_object *jobj = json_tokener_parse(str_return);
        if (jobj) {
            json_object *type_obj;
            if (json_object_object_get_ex(jobj, "type", &type_obj)) {
                const char *type = json_object_get_string(type_obj);
                if (g_strcmp0(type, "chunk") == 0) {
                    json_object *text_obj;
                    if (json_object_object_get_ex(jobj, "text", &text_obj)) {
                        append_to_chat(NULL, json_object_get_string(text_obj), "bot");
                    }
                } else if (g_strcmp0(type, "done") == 0) {
                    gtk_widget_set_sensitive(input_entry, TRUE);
                    gtk_widget_set_sensitive(send_button, TRUE);
                    gtk_widget_set_sensitive(ocr_button, TRUE);
                    gtk_widget_grab_focus(input_entry);
                    append_to_chat(NULL, "\n", NULL);
                } else if (g_strcmp0(type, "status") == 0) {
                    json_object *text_obj;
                    if (json_object_object_get_ex(jobj, "text", &text_obj)) {
                        append_to_chat("System", json_object_get_string(text_obj), "italic");
                    }
                } else if (g_strcmp0(type, "error") == 0) {
                    json_object *text_obj;
                    if (json_object_object_get_ex(jobj, "text", &text_obj)) {
                        append_to_chat("Error", json_object_get_string(text_obj), "error");
                    }
                }
            }
            json_object_put(jobj);
        }
        g_free(str_return);
    }
    
    return TRUE;
}

static void on_send_clicked(GtkButton *button, gpointer user_data) {
    const char *text = gtk_entry_get_text(GTK_ENTRY(input_entry));
    if (!text || strlen(text) == 0) return;
    
    append_to_chat("User", text, "user");
    append_to_chat("Assistant", "", "bot");
    
    gtk_widget_set_sensitive(input_entry, FALSE);
    gtk_widget_set_sensitive(send_button, FALSE);
    gtk_widget_set_sensitive(ocr_button, FALSE);
    
    send_command("prompt", "text", text);
    gtk_entry_set_text(GTK_ENTRY(input_entry), "");
}

static void on_input_activate(GtkEntry *entry, gpointer user_data) {
    on_send_clicked(GTK_BUTTON(send_button), NULL);
}

static void on_ocr_clicked(GtkButton *button, gpointer user_data) {
    gtk_widget_set_sensitive(ocr_button, FALSE);
    
    char *std_out = NULL;
    char *std_err = NULL;
    int status = 0;
    
    GtkWidget *window = gtk_widget_get_toplevel(GTK_WIDGET(button));
    gtk_widget_hide(window);
    
    while (gtk_events_pending()) gtk_main_iteration();
    g_usleep(200000);
    
    g_spawn_command_line_sync("ocws-ocr", &std_out, &std_err, &status, NULL);
    
    gtk_widget_show(window);
    gtk_widget_set_sensitive(ocr_button, TRUE);
    
    if (std_out && strlen(std_out) > 0) {
        const char *current_text = gtk_entry_get_text(GTK_ENTRY(input_entry));
        int len = strlen(std_out);
        while(len > 0 && (std_out[len-1] == '\n' || std_out[len-1] == '\r')) {
            std_out[--len] = '\0';
        }
        char *new_text = g_strdup_printf("%s%s%s", current_text, strlen(current_text) > 0 ? "\n" : "", std_out);
        gtk_entry_set_text(GTK_ENTRY(input_entry), new_text);
        g_free(new_text);
    }
    
    g_free(std_out);
    g_free(std_err);
}

static void on_load_clicked(GtkButton *button, gpointer user_data) {
    const char *model_path = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(model_combo));
    if (!model_path) return;
    
    send_command("load", "path", model_path);
}

static void on_eject_clicked(GtkButton *button, gpointer user_data) {
    send_command("eject", NULL, NULL);
}

static void on_clear_clicked(GtkButton *button, gpointer user_data) {
    gtk_text_buffer_set_text(chat_buffer, "", -1);
    append_to_chat("System", "Session cleared.", "italic");
    send_command("clear", NULL, NULL);
}

static void scan_models(GtkComboBoxText *combo) {
    const char *home = getenv("HOME");
    char paths[2][512];
    snprintf(paths[0], sizeof(paths[0]), "%s/Models", home);
    snprintf(paths[1], sizeof(paths[1]), "%s/.local/share/ocws/models", home);
    
    int count = 0;
    for (int i = 0; i < 2; i++) {
        DIR *d = opendir(paths[i]);
        if (d) {
            struct dirent *dir;
            while ((dir = readdir(d)) != NULL) {
                if (strstr(dir->d_name, ".gguf")) {
                    char full_path[1024];
                    snprintf(full_path, sizeof(full_path), "%s/%s", paths[i], dir->d_name);
                    gtk_combo_box_text_append_text(combo, full_path);
                    count++;
                }
            }
            closedir(d);
        }
    }
    if (count > 0) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
    } else {
        gtk_combo_box_text_append_text(combo, "No .gguf models found in ~/Models");
        gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
    }
}

static void activate(GtkApplication *app, gpointer user_data) {
    ocws_gtk_enforce_premium_theme();
    ocws_gtk_apply_dynamic_css(app, NULL);
    
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 700);
    
    GtkWidget *header = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), TRUE);
    gtk_header_bar_set_title(GTK_HEADER_BAR(header), "OCWS Assistant");
    
    model_combo = gtk_combo_box_text_new();
    scan_models(GTK_COMBO_BOX_TEXT(model_combo));
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), model_combo);
    
    load_button = gtk_button_new_with_label("Load");
    g_signal_connect(load_button, "clicked", G_CALLBACK(on_load_clicked), NULL);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), load_button);
    
    eject_button = gtk_button_new_with_label("Eject");
    g_signal_connect(eject_button, "clicked", G_CALLBACK(on_eject_clicked), NULL);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), eject_button);
    
    clear_button = gtk_button_new_with_label("Clear Session");
    g_signal_connect(clear_button, "clicked", G_CALLBACK(on_clear_clicked), NULL);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), clear_button);
    
    gtk_window_set_titlebar(GTK_WINDOW(window), header);
    
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);
    
    chat_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(chat_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(chat_view), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(chat_view), 12);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(chat_view), 12);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(chat_view), 12);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(chat_view), 12);
    gtk_container_add(GTK_CONTAINER(scrolled), chat_view);
    
    chat_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(chat_view));
    
    gtk_text_buffer_create_tag(chat_buffer, "bold", "weight", PANGO_WEIGHT_BOLD, NULL);
    gtk_text_buffer_create_tag(chat_buffer, "italic", "style", PANGO_STYLE_ITALIC, "foreground", "gray", NULL);
    gtk_text_buffer_create_tag(chat_buffer, "user", "foreground", "#89b4fa", NULL);
    gtk_text_buffer_create_tag(chat_buffer, "bot", "foreground", "#a6e3a1", NULL);
    gtk_text_buffer_create_tag(chat_buffer, "error", "foreground", "#f38ba8", NULL);
    
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(hbox, 12);
    gtk_widget_set_margin_end(hbox, 12);
    gtk_widget_set_margin_top(hbox, 12);
    gtk_widget_set_margin_bottom(hbox, 12);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    
    ocr_button = gtk_button_new_with_label("OCR 📷");
    g_signal_connect(ocr_button, "clicked", G_CALLBACK(on_ocr_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), ocr_button, FALSE, FALSE, 0);
    
    input_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(input_entry), "Ask the assistant...");
    g_signal_connect(input_entry, "activate", G_CALLBACK(on_input_activate), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), input_entry, TRUE, TRUE, 0);
    
    send_button = gtk_button_new_with_label("Send 🚀");
    g_signal_connect(send_button, "clicked", G_CALLBACK(on_send_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), send_button, FALSE, FALSE, 0);
    
    /* Spawn server */
    int server_stdout_fd = -1;
    const char *home = getenv("HOME");
    char server_path[512];
    snprintf(server_path, sizeof(server_path), "%s/.local/bin/ocws-llm-server", home);
    
    if (access(server_path, X_OK) != 0) {
        snprintf(server_path, sizeof(server_path), "ocws-llm-server");
    }
    
    char *argv[] = { (char*)server_path, NULL };
    GError *error = NULL;
    
    if (g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                                 &server_pid, &server_stdin_fd, &server_stdout_fd, NULL, &error)) {
        GIOChannel *stdout_channel = g_io_channel_unix_new(server_stdout_fd);
        g_io_add_watch(stdout_channel, G_IO_IN | G_IO_HUP, on_server_stdout, NULL);
        g_io_channel_unref(stdout_channel);
        append_to_chat("System", "Starting LLM server...", "italic");
    } else {
        char python_path[512];
        snprintf(python_path, sizeof(python_path), "/media/naranyala/Data/projects-remote/labwc-fuzzel-sfwbar/src/daemons/ocws-llm-server.py");
        char *argv2[] = { "python3", python_path, NULL };
        g_error_free(error);
        error = NULL;
        if (g_spawn_async_with_pipes(NULL, argv2, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                                     &server_pid, &server_stdin_fd, &server_stdout_fd, NULL, &error)) {
            GIOChannel *stdout_channel = g_io_channel_unix_new(server_stdout_fd);
            g_io_add_watch(stdout_channel, G_IO_IN | G_IO_HUP, on_server_stdout, NULL);
            g_io_channel_unref(stdout_channel);
            append_to_chat("System", "Starting LLM server (dev mode)...", "italic");
        } else {
            append_to_chat("System", "Failed to start LLM server. Is ocws-llm-server installed?", "error");
        }
    }
    
    gtk_widget_show_all(window);
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("org.ocws.llm_runner", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    
    if (server_pid > 0) {
        kill(server_pid, SIGTERM);
        g_spawn_close_pid(server_pid);
    }
    
    g_object_unref(app);
    return status;
}
