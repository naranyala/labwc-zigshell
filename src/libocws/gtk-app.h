#ifndef OCWS_GTK_APP_H
#define OCWS_GTK_APP_H

#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>

#ifndef OCWS_APP_CSS_MAX
#define OCWS_APP_CSS_MAX 8192
#endif

static inline void ocws_gtk_css_load(GtkCssProvider *provider, const char *tokens_path) {
    char css[OCWS_APP_CSS_MAX] = {0};

    /* Fallback tokens */
    snprintf(css, sizeof(css),
        "@define-color ocws_bg #1e1e2e;"
        "@define-color ocws_fg #cdd6f4;"
        "@define-color ocws_surface0 #313244;"
        "@define-color ocws_surface1 #45475a;"
        "@define-color ocws_accent #89b4fa;"
        "@define-color ocws_urgent #f38ba8;"
        "@define-color ocws_ok #a6e3a1;"
        "@define-color ocws_subtext0 #a6adc8;"
    );
    int pos = (int)strlen(css);

    /* Load tokens.css if available */
    if (tokens_path) {
        FILE *f = fopen(tokens_path, "r");
        if (f) {
            size_t n = fread(css + pos, 1, sizeof(css) - pos - 2048, f);
            fclose(f);
            pos += (int)n;
        }
    }

    gtk_css_provider_load_from_data(provider, css, -1, NULL);
}

static inline GtkCssProvider* ocws_gtk_apply_css(GtkApplication *app) {
    (void)app;
    GtkCssProvider *provider = gtk_css_provider_new();

    char tokpath[1024];
    const char *home = getenv("HOME");
    if (home) {
        snprintf(tokpath, sizeof(tokpath), "%s/.config/ocws/tokens.css", home);
    } else {
        tokpath[0] = '\0';
    }

    ocws_gtk_css_load(provider, tokpath[0] ? tokpath : NULL);

    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    return provider;
}

#endif
