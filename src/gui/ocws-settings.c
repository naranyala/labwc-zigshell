/*
 * ocws-settings.c — OCWS Settings Panel (Main Entry Point)
 *
 * A comprehensive GTK3 control center for the OCWS desktop shell.
 * Split into modules: settings-ui.c (widgets), settings-tabs.c (pages)
 *
 * Build:
 *   gcc -O2 -o ocws-settings src/ocws-settings.c src/settings/settings-ui.c src/settings/settings-tabs.c \
 *       $(pkg-config --cflags --libs gtk+-3.0)
 */

#include "settings/settings-ui.h"
#include "settings/settings-tabs.h"
#include <gtk/gtk.h>
#include "../libocws/gtk.h"

static void activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;
    init_paths();
    
    // Abstracted premium GTK injection
    ocws_gtk_enforce_premium_theme();
    ocws_gtk_apply_dynamic_css(app, NULL); // NULL defaults to standard mocha accent

    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "OCWS Settings");
    gtk_window_set_default_size(GTK_WINDOW(window), 900, 600);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

    // Header Bar
    GtkWidget *header = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), TRUE);
    gtk_header_bar_set_title(GTK_HEADER_BAR(header), "OCWS Settings");
    gtk_header_bar_set_subtitle(GTK_HEADER_BAR(header), "C-Written Desktop Control Center");
    gtk_window_set_titlebar(GTK_WINDOW(window), header);

    // Main layout
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(window), hbox);

    // Sidebar & Stack
    GtkWidget *sidebar = gtk_stack_sidebar_new();
    gtk_widget_set_size_request(sidebar, 200, -1);
    GtkWidget *stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_SLIDE_UP_DOWN);
    gtk_stack_sidebar_set_stack(GTK_STACK_SIDEBAR(sidebar), GTK_STACK(stack));

    gtk_box_pack_start(GTK_BOX(hbox), sidebar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_separator_new(GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), stack, TRUE, TRUE, 0);

    // Add tabs
    gtk_stack_add_titled(GTK_STACK(stack), build_shell_tab(),           "shell",           "Shell Modes");
    gtk_stack_add_titled(GTK_STACK(stack), build_appearance_tab(),      "appearance",      "Appearance");
    gtk_stack_add_titled(GTK_STACK(stack), build_bar_config_tab(),      "bar",             "Bar Config");
    gtk_stack_add_titled(GTK_STACK(stack), build_widgets_tab(),         "widgets",         "Widgets");
    gtk_stack_add_titled(GTK_STACK(stack), build_workspaces_tab(),      "workspaces",      "Workspaces");
    gtk_stack_add_titled(GTK_STACK(stack), build_notifications_tab(),   "notifications",   "Notifications");
    gtk_stack_add_titled(GTK_STACK(stack), build_diagnostics_tab(),     "diagnostics",     "System Health");
    gtk_stack_add_titled(GTK_STACK(stack), build_quick_settings_tab(),  "quick_settings",  "Quick Settings");
    gtk_stack_add_titled(GTK_STACK(stack), build_keybinds_tab(),        "keybinds",        "Keybinds");
    gtk_stack_add_titled(GTK_STACK(stack), build_credits_tab(),         "credits",         "Credits");
    gtk_stack_add_titled(GTK_STACK(stack), build_about_tab(),           "about",           "About");

    gtk_widget_show_all(window);
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("org.ocws.settings", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
