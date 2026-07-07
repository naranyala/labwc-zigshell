/*
 * fonts-mgr.c — Entry point for OCWS Fonts Manager GUI
 */

#include "fonts-mgr-common.h"

/* Defined in fonts-mgr-ui.c */
extern void fonts_mgr_activate(GtkApplication *app, gpointer user_data);

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new(APP_ID, G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(fonts_mgr_activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
