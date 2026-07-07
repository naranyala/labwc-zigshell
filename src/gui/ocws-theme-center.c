/*
 * ocws-theme-center.c — GTK3 Theme Center GUI
 * Browse, preview, and apply OCWS themes across all config surfaces.
 *
 * Pages:
 *   1. Browser  — grid of theme cards with live selection
 *   2. Preview  — full color palette + config surface breakdown
 *   3. Edit     — interactive color editor for selected theme
 *   4. Import   — import/export theme files
 */

#include <gtk/gtk.h>
#include "../libocws/gtk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>
#include <errno.h>

#define VERSION "1.0.0"
#define OCWS_DIR ".config/ocws"
#define MAX_THEMES 64
#define MAX_SECTIONS 16
#define MAX_KEY_LEN 64
#define MAX_VAL_LEN 128
#define MAX_COLORS 32
#define MAX_OUTPUT_FILES 14

/* ============================================================
 * Data Structures
 * ============================================================ */

typedef struct {
    char name[MAX_KEY_LEN];
    char value[MAX_VAL_LEN];
} ThemeKV;

typedef struct {
    char name[MAX_KEY_LEN];
    ThemeKV keys[64];
    int key_count;
} ThemeSection;

typedef struct {
    char filepath[1024];
    char name[128];
    char description[256];
    char author[128];
    char version_str[32];
    ThemeSection sections[MAX_SECTIONS];
    int section_count;
    char colors[MAX_COLORS][2][MAX_VAL_LEN];
    int color_count;
} Theme;

typedef struct {
    Theme themes[MAX_THEMES];
    int count;
    int selected;
    char ocws_home[512];
    char project_dir[1024];
    /* UI widgets for live updates */
    GtkWidget *preview_colors_grid;
    GtkWidget *preview_source_view;
    GtkWidget *preview_sections_box;
    GtkWidget *combo_themes;
    GtkWidget *combo_preview;
    GtkWidget *combo_source;
    GtkWidget *combo_export;
    GtkWidget *lbl_status;
    GtkWidget *lbl_active;
    GtkWidget *browser_flow;
    /* Surface toggles */
    GtkWidget *surface_toggles[14];
    const char *surface_names[14];
    int surface_count;
} ThemeCenter;

static ThemeCenter app = {0};

/* Output file map (template -> destination) */
static const char *output_files[][2] = {
    {"tokens.css.tmpl",    "ocws/tokens.css"},
    {"ocws.css.tmpl",      "ocws/ocws.css"},
    {"sfwbar.css.tmpl",    "ocws/theme.css"},
    {"themerc-override.tmpl", "labwc/themerc-override"},
    {"environment.tmpl",   "labwc/environment"},
    {"gtk.css.tmpl",       "gtk-3.0/gtk.css"},
    {"gtk3-settings.ini.tmpl", "gtk-3.0/settings.ini"},
    {"gtk4-settings.ini.tmpl", "gtk-4.0/settings.ini"},
    {"fuzzel.ini.tmpl",    "fuzzel/fuzzel.ini"},
    {"foot.ini.tmpl",      "foot/foot.ini"},
    {"rofi.rasi.tmpl",     "rofi/config.rasi"},
    {"mako.ini.tmpl",      "mako/config"},
    {"qt6ct.conf.tmpl",    "qt6ct/qt6ct.conf"},
};
static const int OUTPUT_FILE_COUNT = 14;

/* ============================================================
 * INI Parser
 * ============================================================ */

static void trim(char *s) {
    while (isspace((unsigned char)*s)) memmove(s, s + 1, strlen(s));
    int len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) s[--len] = '\0';
}

static void parse_theme_ini(Theme *theme, const char *filepath) {
    FILE *f = fopen(filepath, "r");
    if (!f) return;

    memset(theme, 0, sizeof(Theme));
    strncpy(theme->filepath, filepath, sizeof(theme->filepath) - 1);

    ThemeSection *cur = NULL;
    char line[1024];

    while (fgets(line, sizeof(line), f)) {
        trim(line);
        if (line[0] == '\0' || line[0] == '#') continue;

        if (line[0] == '[') {
            char *end = strchr(line, ']');
            if (end && theme->section_count < MAX_SECTIONS) {
                *end = '\0';
                cur = &theme->sections[theme->section_count++];
                strncpy(cur->name, line + 1, sizeof(cur->name) - 1);
                cur->key_count = 0;
            }
            continue;
        }

        char *eq = strchr(line, '=');
        if (eq && cur && cur->key_count < 64) {
            *eq = '\0';
            ThemeKV *kv = &cur->keys[cur->key_count++];
            strncpy(kv->name, line, sizeof(kv->name) - 1);
            strncpy(kv->value, eq + 1, sizeof(kv->value) - 1);
            trim(kv->name);
            trim(kv->value);

            if (strcmp(cur->name, "meta") == 0) {
                if (strcmp(kv->name, "name") == 0)
                    strncpy(theme->name, kv->value, sizeof(theme->name) - 1);
                else if (strcmp(kv->name, "description") == 0)
                    strncpy(theme->description, kv->value, sizeof(theme->description) - 1);
                else if (strcmp(kv->name, "author") == 0)
                    strncpy(theme->author, kv->value, sizeof(theme->author) - 1);
                else if (strcmp(kv->name, "version") == 0)
                    strncpy(theme->version_str, kv->value, sizeof(theme->version_str) - 1);
            }

            if (strcmp(cur->name, "colors") == 0 && theme->color_count < MAX_COLORS) {
                strncpy(theme->colors[theme->color_count][0], kv->name, MAX_KEY_LEN - 1);
                strncpy(theme->colors[theme->color_count][1], kv->value, MAX_VAL_LEN - 1);
                theme->color_count++;
            }
        }
    }
    fclose(f);

    if (theme->name[0] == '\0') {
        const char *base = strrchr(filepath, '/');
        base = base ? base + 1 : filepath;
        strncpy(theme->name, base, sizeof(theme->name) - 1);
        char *dot = strrchr(theme->name, '.');
        if (dot) *dot = '\0';
    }
}

static const char* theme_get(Theme *t, const char *section, const char *key) {
    for (int i = 0; i < t->section_count; i++) {
        if (strcmp(t->sections[i].name, section) == 0) {
            for (int j = 0; j < t->sections[i].key_count; j++) {
                if (strcmp(t->sections[i].keys[j].name, key) == 0)
                    return t->sections[i].keys[j].value;
            }
        }
    }
    return NULL;
}

static const char* theme_color_hex(Theme *t, const char *name) {
    for (int i = 0; i < t->color_count; i++) {
        if (strcmp(t->colors[i][0], name) == 0)
            return t->colors[i][1];
    }
    return "#888888";
}

/* ============================================================
 * Paths & Theme Discovery
 * ============================================================ */

static void init_paths(void) {
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(app.ocws_home, sizeof(app.ocws_home), "%s/%s", home, OCWS_DIR);

    const char *env_proj = getenv("LABWC_PROJECT");
    if (env_proj && env_proj[0]) {
        char p[1024];
        snprintf(p, sizeof(p), "%s/themes", env_proj);
        struct stat st;
        if (stat(p, &st) == 0) {
            strncpy(app.project_dir, env_proj, sizeof(app.project_dir) - 1);
            return;
        }
    }

    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd))) {
        strncpy(app.project_dir, cwd, sizeof(app.project_dir) - 1);
        while (app.project_dir[0]) {
            char p[1024];
            snprintf(p, sizeof(p), "%s/themes", app.project_dir);
            struct stat st;
            if (stat(p, &st) == 0) return;
            char *slash = strrchr(app.project_dir, '/');
            if (slash && slash != app.project_dir) *slash = '\0';
            else break;
        }
    }
    strncpy(app.project_dir, app.ocws_home, sizeof(app.project_dir) - 1);
}

static void discover_themes(void) {
    app.count = 0;
    char dirpath[1024];
    snprintf(dirpath, sizeof(dirpath), "%s/themes", app.project_dir);

    DIR *dir = opendir(dirpath);
    if (!dir) return;

    struct dirent *ent;
    while ((ent = readdir(dir)) && app.count < MAX_THEMES) {
        if (ent->d_name[0] == '.') continue;
        char *dot = strrchr(ent->d_name, '.');
        if (!dot || strcmp(dot, ".ini") != 0) continue;

        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", dirpath, ent->d_name);
        parse_theme_ini(&app.themes[app.count], path);
        app.count++;
    }
    closedir(dir);
}

static const char* get_current_theme_name(void) {
    static char current[256] = {0};
    char path[1024];
    snprintf(path, sizeof(path), "%s/labwc/.current-theme", app.ocws_home);
    FILE *f = fopen(path, "r");
    if (f) {
        if (fgets(current, sizeof(current), f))
            current[strcspn(current, "\n")] = '\0';
        fclose(f);
    }
    return current;
}

/* ============================================================
 * Shell Execution
 * ============================================================ */

static char* run_cmd_output(const char *cmd) {
    char full[2048];
    snprintf(full, sizeof(full), "%s 2>&1", cmd);
    FILE *fp = popen(full, "r");
    if (!fp) return g_strdup("");

    char buf[4096] = {0};
    size_t total = 0;
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        if (total + len < sizeof(buf) - 1) {
            memcpy(buf + total, line, len);
            total += len;
        }
    }
    pclose(fp);
    return g_strdup(buf);
}

static void run_theme_engine(const char *action, const char *theme_path) {
    char cmd[2048];
    if (theme_path)
        snprintf(cmd, sizeof(cmd), "theme-engine.sh %s '%s'", action, theme_path);
    else
        snprintf(cmd, sizeof(cmd), "theme-engine.sh %s", action);

    char *output = run_cmd_output(cmd);
    if (app.lbl_status) {
        gtk_label_set_text(GTK_LABEL(app.lbl_status), output);
    }
    g_free(output);
}



/* ============================================================
 * Color Swatch Widget
 * ============================================================ */

static GtkWidget* make_swatch(const char *hex, int size) {
    char css[128];
    snprintf(css, sizeof(css),
        "background-color: %s; min-width: %dpx; min-height: %dpx; border-radius: 6px;",
        hex, size, size);

    GtkWidget *area = gtk_drawing_area_new();
    GtkStyleContext *ctx = gtk_widget_get_style_context(area);
    gtk_style_context_add_class(ctx, "color-swatch");

    GtkCssProvider *p = gtk_css_provider_new();
    gtk_css_provider_load_from_data(p, css, -1, NULL);
    gtk_style_context_add_provider(ctx, GTK_STYLE_PROVIDER(p),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(p);

    gtk_widget_set_size_request(area, size, size);
    return area;
}

/* ============================================================
 * Page 1: Theme Browser
 * ============================================================ */

static void on_theme_card_clicked(GtkWidget *event_box, GdkEventButton *event, gpointer data) {
    (void)event;
    int index = GPOINTER_TO_INT(data);
    if (index < 0 || index >= app.count) return;

    app.selected = index;

    /* Update card selection visuals */
    GList *children = gtk_container_get_children(GTK_CONTAINER(
        gtk_widget_get_parent(event_box)));
    for (GList *l = children; l; l = l->next) {
        GtkWidget *child = l->data;
        GtkStyleContext *ctx = gtk_widget_get_style_context(child);
        gtk_style_context_remove_class(ctx, "selected");
    }
    g_list_free(children);
    gtk_style_context_add_class(gtk_widget_get_style_context(event_box), "selected");

    /* Update status */
    char status[512];
    snprintf(status, sizeof(status), "Selected: %s (%s)",
             app.themes[index].name, app.themes[index].filepath);
    if (app.lbl_status)
        gtk_label_set_text(GTK_LABEL(app.lbl_status), status);
}

static GtkWidget* build_browser_page(void) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(vbox, 24);
    gtk_widget_set_margin_end(vbox, 24);
    gtk_widget_set_margin_top(vbox, 20);
    gtk_widget_set_margin_bottom(vbox, 16);

    /* Title row */
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    GtkWidget *lbl = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl),
        "<span size='x-large' weight='bold'>Theme Browser</span>");
    gtk_box_pack_start(GTK_BOX(hbox), lbl, FALSE, FALSE, 0);

    GtkWidget *spacer = gtk_label_new(NULL);
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_pack_start(GTK_BOX(hbox), spacer, TRUE, TRUE, 0);

    /* Active theme indicator */
    const char *cur = get_current_theme_name();
    char active_text[256];
    snprintf(active_text, sizeof(active_text), "Active: %s", cur[0] ? cur : "(none)");
    app.lbl_active = gtk_label_new(active_text);
    gtk_style_context_add_class(gtk_widget_get_style_context(app.lbl_active), "dim-label");
    gtk_box_pack_start(GTK_BOX(hbox), app.lbl_active, FALSE, FALSE, 0);

    /* Theme cards flow */
    app.browser_flow = gtk_flow_box_new();
    GtkWidget *flow = app.browser_flow;
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(flow), TRUE);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(flow), 12);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(flow), 12);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(flow), 3);

    for (int i = 0; i < app.count; i++) {
        Theme *t = &app.themes[i];

        GtkWidget *frame = gtk_frame_new(NULL);
        GtkStyleContext *fctx = gtk_widget_get_style_context(frame);
        gtk_style_context_add_class(fctx, "theme-card");

        /* Highlight current */
        if (strstr(cur, t->name))
            gtk_style_context_add_class(fctx, "selected");

        GtkWidget *inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
        gtk_container_set_border_width(GTK_CONTAINER(inner), 8);
        gtk_container_add(GTK_CONTAINER(frame), inner);

        /* Name */
        char *markup = g_strdup_printf("<b>%s</b>", t->name);
        GtkWidget *n = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(n), markup);
        gtk_label_set_xalign(GTK_LABEL(n), 0.0);
        gtk_box_pack_start(GTK_BOX(inner), n, FALSE, FALSE, 0);
        g_free(markup);

        /* Description */
        if (t->description[0]) {
            GtkWidget *d = gtk_label_new(t->description);
            gtk_label_set_xalign(GTK_LABEL(d), 0.0);
            gtk_label_set_line_wrap(GTK_LABEL(d), TRUE);
            gtk_style_context_add_class(gtk_widget_get_style_context(d), "dim-label");
            gtk_box_pack_start(GTK_BOX(inner), d, FALSE, FALSE, 0);
        }

        /* Color palette strip */
        GtkWidget *strip = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
        gtk_widget_set_margin_top(strip, 4);
        const char *palette[] = {
            "base", "mantle", "surface0", "surface1", "surface2",
            "text", "subtext0", "blue", "green", "red", "yellow", "mauve"
        };
        for (int p = 0; p < 12; p++) {
            const char *hex = theme_color_hex(t, palette[p]);
            gtk_box_pack_start(GTK_BOX(strip), make_swatch(hex, 20), FALSE, FALSE, 0);
        }
        gtk_box_pack_start(GTK_BOX(inner), strip, FALSE, FALSE, 0);

        /* Author */
        if (t->author[0]) {
            char info[256];
            snprintf(info, sizeof(info), "%s v%s", t->author, t->version_str[0] ? t->version_str : "?");
            GtkWidget *ai = gtk_label_new(info);
            gtk_style_context_add_class(gtk_widget_get_style_context(ai), "dim-label");
            gtk_label_set_xalign(GTK_LABEL(ai), 0.0);
            gtk_box_pack_start(GTK_BOX(inner), ai, FALSE, FALSE, 0);
        }

        /* Make clickable */
        GtkWidget *evt = gtk_event_box_new();
        gtk_container_add(GTK_CONTAINER(evt), frame);
        g_signal_connect(evt, "button-release-event",
                         G_CALLBACK(on_theme_card_clicked), GINT_TO_POINTER(i));

        gtk_container_add(GTK_CONTAINER(flow), evt);
    }

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), flow);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

    return vbox;
}

/* ============================================================
 * Page 2: Color Preview (full palette + sections)
 * ============================================================ */

static void update_preview(GtkComboBox *combo, gpointer data) {
    (void)data;
    int idx = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
    if (idx < 0 || idx >= app.count) return;

    Theme *t = &app.themes[idx];

    /* Update color grid */
    if (app.preview_colors_grid) {
        gtk_container_foreach(GTK_CONTAINER(app.preview_colors_grid),
                              (GtkCallback)gtk_widget_destroy, NULL);

        for (int i = 0; i < t->color_count; i++) {
            int row = i / 3;
            int col = i % 3;

            GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
            gtk_widget_set_halign(hbox, GTK_ALIGN_START);

            gtk_box_pack_start(GTK_BOX(hbox), make_swatch(t->colors[i][1], 28), FALSE, FALSE, 0);

            char lbl_text[128];
            snprintf(lbl_text, sizeof(lbl_text), "<b>%s</b>\n%s", t->colors[i][0], t->colors[i][1]);
            GtkWidget *l = gtk_label_new(NULL);
            gtk_label_set_markup(GTK_LABEL(l), lbl_text);
            gtk_label_set_xalign(GTK_LABEL(l), 0.0);
            gtk_box_pack_start(GTK_BOX(hbox), l, FALSE, FALSE, 0);

            gtk_grid_attach(GTK_GRID(app.preview_colors_grid), hbox, col, row, 1, 1);
        }
        gtk_widget_show_all(app.preview_colors_grid);
    }

    /* Update sections breakdown */
    if (app.preview_sections_box) {
        gtk_container_foreach(GTK_CONTAINER(app.preview_sections_box),
                              (GtkCallback)gtk_widget_destroy, NULL);
        for (int s = 0; s < t->section_count; s++) {
            ThemeSection *sec = &t->sections[s];
            char sec_markup[256];
            snprintf(sec_markup, sizeof(sec_markup), "<b>[%s]</b>  %d keys", sec->name, sec->key_count);
            GtkWidget *sl = gtk_label_new(NULL);
            gtk_label_set_markup(GTK_LABEL(sl), sec_markup);
            gtk_label_set_xalign(GTK_LABEL(sl), 0.0);
            gtk_box_pack_start(GTK_BOX(app.preview_sections_box), sl, FALSE, FALSE, 0);
        }
        gtk_widget_show_all(app.preview_sections_box);
    }

    /* Update source view */
    if (app.preview_source_view) {
        GtkTextBuffer *buf = gtk_text_view_get_buffer(
            GTK_TEXT_VIEW(app.preview_source_view));
        FILE *f = fopen(t->filepath, "r");
        if (f) {
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            char *content = g_malloc(sz + 1);
            size_t actual = fread(content, 1, sz, f);
            content[actual] = '\0';
            fclose(f);
            gtk_text_buffer_set_text(buf, content, -1);
            g_free(content);
        }
    }
}

static GtkWidget* build_preview_page(void) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(vbox, 24);
    gtk_widget_set_margin_end(vbox, 24);
    gtk_widget_set_margin_top(vbox, 20);
    gtk_widget_set_margin_bottom(vbox, 16);

    GtkWidget *lbl = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl),
        "<span size='x-large' weight='bold'>Theme Preview</span>");
    gtk_box_pack_start(GTK_BOX(vbox), lbl, FALSE, FALSE, 0);

    /* Theme selector */
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("Theme:"), FALSE, FALSE, 0);

    app.combo_preview = gtk_combo_box_text_new();
    for (int i = 0; i < app.count; i++)
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.combo_preview),
                                       app.themes[i].name);
    if (app.count > 0)
        gtk_combo_box_set_active(GTK_COMBO_BOX(app.combo_preview), 0);
    gtk_box_pack_start(GTK_BOX(hbox), app.combo_preview, TRUE, TRUE, 0);
    g_signal_connect(app.combo_preview, "changed", G_CALLBACK(update_preview), NULL);

    /* Split: colors grid (left) + source (right) */
    GtkWidget *hpaned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);

    /* Left: color palette */
    GtkWidget *left_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(left_scroll),
        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    GtkWidget *left_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);

    GtkWidget *lbl_colors = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_colors), "<b>Color Palette</b>");
    gtk_label_set_xalign(GTK_LABEL(lbl_colors), 0.0);
    gtk_box_pack_start(GTK_BOX(left_vbox), lbl_colors, FALSE, FALSE, 0);

    app.preview_colors_grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(app.preview_colors_grid), 16);
    gtk_grid_set_row_spacing(GTK_GRID(app.preview_colors_grid), 8);
    gtk_box_pack_start(GTK_BOX(left_vbox), app.preview_colors_grid, FALSE, FALSE, 0);

    /* Sections breakdown */
    GtkWidget *lbl_sections = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_sections), "<b>Config Sections</b>");
    gtk_label_set_xalign(GTK_LABEL(lbl_sections), 0.0);
    gtk_box_pack_start(GTK_BOX(left_vbox), lbl_sections, FALSE, FALSE, 0);

    app.preview_sections_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_box_pack_start(GTK_BOX(left_vbox), app.preview_sections_box, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(left_scroll), left_vbox);
    gtk_paned_pack1(GTK_PANED(hpaned), left_scroll, TRUE, TRUE);

    /* Right: source view */
    GtkWidget *right_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(right_scroll),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    GtkWidget *source_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(source_view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(source_view), TRUE);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(source_view), 12);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(source_view), 12);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(source_view), 8);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(source_view), 8);
    GtkStyleContext *sctx = gtk_widget_get_style_context(source_view);
    gtk_style_context_add_class(sctx, "source");
    app.preview_source_view = source_view;

    gtk_container_add(GTK_CONTAINER(right_scroll), source_view);
    gtk_paned_pack2(GTK_PANED(hpaned), right_scroll, TRUE, TRUE);

    gtk_box_pack_start(GTK_BOX(vbox), hpaned, TRUE, TRUE, 0);

    /* Initial load */
    update_preview(GTK_COMBO_BOX(app.combo_preview), NULL);

    return vbox;
}

/* ============================================================
 * Page 3: Surface Manager
 * ============================================================ */

static GtkWidget* build_surfaces_page(void) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(vbox, 24);
    gtk_widget_set_margin_end(vbox, 24);
    gtk_widget_set_margin_top(vbox, 20);
    gtk_widget_set_margin_bottom(vbox, 16);

    GtkWidget *lbl = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl),
        "<span size='x-large' weight='bold'>Config Surfaces</span>");
    gtk_box_pack_start(GTK_BOX(vbox), lbl, FALSE, FALSE, 0);

    GtkWidget *desc = gtk_label_new(
        "Toggle which config surfaces are written when applying a theme.\n"
        "All surfaces are enabled by default. Disable specific ones to preserve manual edits.");
    gtk_label_set_xalign(GTK_LABEL(desc), 0.0);
    gtk_label_set_line_wrap(GTK_LABEL(desc), TRUE);
    gtk_style_context_add_class(gtk_widget_get_style_context(desc), "dim-label");
    gtk_box_pack_start(GTK_BOX(vbox), desc, FALSE, FALSE, 0);

    struct { const char *name; const char *desc; } surfaces[] = {
        {"labwc",       "Window manager theme (themerc-override, environment)"},
        {"tokens.css",  "CSS @define-color tokens (single source of truth)"},
        {"ocws.css",    "OCWS glassmorphic panel CSS"},
        {"sfwbar.css",  "SFWBar structural CSS (layout, geometry)"},
        {"gtk3",        "GTK3 settings + CSS overrides"},
        {"gtk4",        "GTK4 settings + CSS overrides"},
        {"fuzzel",      "Fuzzel app launcher config"},
        {"foot",        "Foot terminal colors + config"},
        {"rofi",        "Rofi launcher colors + config"},
        {"mako",        "Mako notification daemon config"},
        {"qt6ct",       "Qt6 theming config (icon theme, fonts)"},
    };
    app.surface_count = 11;

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 16);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_box_pack_start(GTK_BOX(vbox), grid, FALSE, FALSE, 0);

    for (int i = 0; i < app.surface_count; i++) {
        int row = i / 2;
        int col = (i % 2) * 2;

        GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);

        GtkWidget *sw = gtk_switch_new();
        gtk_switch_set_active(GTK_SWITCH(sw), TRUE);
        gtk_widget_set_valign(sw, GTK_ALIGN_CENTER);
        gtk_box_pack_start(GTK_BOX(hbox), sw, FALSE, FALSE, 0);
        app.surface_toggles[i] = sw;

        char txt[256];
        snprintf(txt, sizeof(txt), "<b>%s</b>\n<span size='small'>%s</span>",
                 surfaces[i].name, surfaces[i].desc);
        GtkWidget *l = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(l), txt);
        gtk_label_set_xalign(GTK_LABEL(l), 0.0);
        gtk_box_pack_start(GTK_BOX(hbox), l, FALSE, FALSE, 0);

        app.surface_names[i] = surfaces[i].name;
        gtk_grid_attach(GTK_GRID(grid), hbox, col, row, 2, 1);
    }

    return vbox;
}

/* ============================================================
 * Page 4: Import / Export
 * ============================================================ */

static void on_import_clicked(GtkButton *button, gpointer data) {
    (void)button; (void)data;
    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Import Theme INI",
        NULL,
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Import", GTK_RESPONSE_ACCEPT,
        NULL);

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Theme INI files");
    gtk_file_filter_add_pattern(filter, "*.ini");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    int res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (filename) {
            char dest[1024];
            const char *base = strrchr(filename, '/');
            base = base ? base + 1 : filename;
            snprintf(dest, sizeof(dest), "%s/themes/%s", app.project_dir, base);

            GError *err = NULL;
            GFile *src = g_file_new_for_path(filename);
            GFile *dst = g_file_new_for_path(dest);
            g_file_copy(src, dst, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &err);

            if (!err) {
                /* Refresh theme list */
                discover_themes();

                /* Rebuild browser grid */
                if (app.browser_flow) {
                    gtk_container_foreach(GTK_CONTAINER(app.browser_flow),
                                          (GtkCallback)gtk_widget_destroy, NULL);
                    for (int i = 0; i < app.count; i++) {
                        Theme *t = &app.themes[i];
                        GtkWidget *frame = gtk_frame_new(NULL);
                        GtkStyleContext *fctx = gtk_widget_get_style_context(frame);
                        gtk_style_context_add_class(fctx, "theme-card");

                        GtkWidget *inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
                        gtk_container_set_border_width(GTK_CONTAINER(inner), 8);
                        gtk_container_add(GTK_CONTAINER(frame), inner);

                        char *markup = g_strdup_printf("<b>%s</b>", t->name);
                        GtkWidget *n = gtk_label_new(NULL);
                        gtk_label_set_markup(GTK_LABEL(n), markup);
                        gtk_label_set_xalign(GTK_LABEL(n), 0.0);
                        gtk_box_pack_start(GTK_BOX(inner), n, FALSE, FALSE, 0);
                        g_free(markup);

                        if (t->description[0]) {
                            GtkWidget *d = gtk_label_new(t->description);
                            gtk_label_set_xalign(GTK_LABEL(d), 0.0);
                            gtk_label_set_line_wrap(GTK_LABEL(d), TRUE);
                            gtk_style_context_add_class(gtk_widget_get_style_context(d), "dim-label");
                            gtk_box_pack_start(GTK_BOX(inner), d, FALSE, FALSE, 0);
                        }

                        GtkWidget *strip = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
                        const char *palette[] = {"base", "surface0", "blue", "green", "red"};
                        for (int p = 0; p < 5; p++) {
                            const char *hex = theme_color_hex(t, palette[p]);
                            gtk_box_pack_start(GTK_BOX(strip), make_swatch(hex, 20), FALSE, FALSE, 0);
                        }
                        gtk_box_pack_start(GTK_BOX(inner), strip, FALSE, FALSE, 0);

                        GtkWidget *evt = gtk_event_box_new();
                        gtk_container_add(GTK_CONTAINER(evt), frame);
                        g_signal_connect(evt, "button-release-event",
                                         G_CALLBACK(on_theme_card_clicked), GINT_TO_POINTER(i));
                        gtk_container_add(GTK_CONTAINER(app.browser_flow), evt);
                    }
                    gtk_widget_show_all(app.browser_flow);
                }

                if (app.lbl_status)
                    gtk_label_set_text(GTK_LABEL(app.lbl_status), "Theme imported.");
            } else {
                if (app.lbl_status)
                    gtk_label_set_text(GTK_LABEL(app.lbl_status), err->message);
                g_error_free(err);
            }
            g_object_unref(src);
            g_object_unref(dst);
            g_free(filename);
        }
    }
    gtk_widget_destroy(dialog);
}

static void on_export_clicked(GtkButton *button, gpointer data) {
    (void)button;
    (void)data;
    int idx = app.combo_export ? gtk_combo_box_get_active(GTK_COMBO_BOX(app.combo_export)) : -1;
    if (idx < 0 || idx >= app.count) idx = 0;

    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Export Theme INI",
        NULL,
        GTK_FILE_CHOOSER_ACTION_SAVE,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Export", GTK_RESPONSE_ACCEPT,
        NULL);

    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog),
        app.themes[idx].name);

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Theme INI files");
    gtk_file_filter_add_pattern(filter, "*.ini");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    int res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (filename) {
            GError *err = NULL;
            GFile *src = g_file_new_for_path(app.themes[idx].filepath);
            GFile *dst = g_file_new_for_path(filename);
            g_file_copy(src, dst, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &err);
            if (err) {
                if (app.lbl_status)
                    gtk_label_set_text(GTK_LABEL(app.lbl_status), err->message);
                g_error_free(err);
            } else {
                if (app.lbl_status)
                    gtk_label_set_text(GTK_LABEL(app.lbl_status), "Theme exported.");
            }
            g_object_unref(src);
            g_object_unref(dst);
            g_free(filename);
        }
    }
    gtk_widget_destroy(dialog);
}

static GtkWidget* build_import_page(void) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(vbox, 24);
    gtk_widget_set_margin_end(vbox, 24);
    gtk_widget_set_margin_top(vbox, 20);
    gtk_widget_set_margin_bottom(vbox, 16);

    GtkWidget *lbl = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl),
        "<span size='x-large' weight='bold'>Import / Export</span>");
    gtk_box_pack_start(GTK_BOX(vbox), lbl, FALSE, FALSE, 0);

    /* Import section */
    GtkWidget *import_card = gtk_frame_new(NULL);
    GtkStyleContext *ictx = gtk_widget_get_style_context(import_card);
    gtk_style_context_add_class(ictx, "theme-card");
    gtk_container_set_border_width(GTK_CONTAINER(import_card), 16);
    gtk_box_pack_start(GTK_BOX(vbox), import_card, FALSE, FALSE, 0);

    GtkWidget *ivbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_add(GTK_CONTAINER(import_card), ivbox);

    GtkWidget *ilbl = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(ilbl), "<b>Import Theme</b>");
    gtk_label_set_xalign(GTK_LABEL(ilbl), 0.0);
    gtk_box_pack_start(GTK_BOX(ivbox), ilbl, FALSE, FALSE, 0);

    GtkWidget *idesc = gtk_label_new(
        "Import an external .ini theme file into the project themes directory.");
    gtk_label_set_xalign(GTK_LABEL(idesc), 0.0);
    gtk_style_context_add_class(gtk_widget_get_style_context(idesc), "dim-label");
    gtk_box_pack_start(GTK_BOX(ivbox), idesc, FALSE, FALSE, 0);

    GtkWidget *ibtn = gtk_button_new_with_label("Import Theme File...");
    g_signal_connect(ibtn, "clicked", G_CALLBACK(on_import_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(ivbox), ibtn, FALSE, FALSE, 0);

    /* Export section */
    GtkWidget *export_card = gtk_frame_new(NULL);
    GtkStyleContext *ectx = gtk_widget_get_style_context(export_card);
    gtk_style_context_add_class(ectx, "theme-card");
    gtk_container_set_border_width(GTK_CONTAINER(export_card), 16);
    gtk_box_pack_start(GTK_BOX(vbox), export_card, FALSE, FALSE, 0);

    GtkWidget *evbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_add(GTK_CONTAINER(export_card), evbox);

    GtkWidget *elbl = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(elbl), "<b>Export Theme</b>");
    gtk_label_set_xalign(GTK_LABEL(elbl), 0.0);
    gtk_box_pack_start(GTK_BOX(evbox), elbl, FALSE, FALSE, 0);

    GtkWidget *edescl = gtk_label_new("Export the selected theme to a standalone .ini file:");
    gtk_label_set_xalign(GTK_LABEL(edescl), 0.0);
    gtk_style_context_add_class(gtk_widget_get_style_context(edescl), "dim-label");
    gtk_box_pack_start(GTK_BOX(evbox), edescl, FALSE, FALSE, 0);

    /* Theme selector for export */
    GtkWidget *ehbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(evbox), ehbox, FALSE, FALSE, 0);

    app.combo_export = gtk_combo_box_text_new();
    for (int i = 0; i < app.count; i++)
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.combo_export), app.themes[i].name);
    if (app.count > 0)
        gtk_combo_box_set_active(GTK_COMBO_BOX(app.combo_export), 0);
    gtk_box_pack_start(GTK_BOX(ehbox), app.combo_export, TRUE, TRUE, 0);

    GtkWidget *ebtn = gtk_button_new_with_label("Export...");
    gtk_box_pack_start(GTK_BOX(ehbox), ebtn, FALSE, FALSE, 0);
    g_signal_connect(ebtn, "clicked", G_CALLBACK(on_export_clicked), NULL);

    /* Output file map */
    GtkWidget *map_card = gtk_frame_new(NULL);
    GtkStyleContext *mctx = gtk_widget_get_style_context(map_card);
    gtk_style_context_add_class(mctx, "theme-card");
    gtk_container_set_border_width(GTK_CONTAINER(map_card), 16);
    gtk_box_pack_start(GTK_BOX(vbox), map_card, FALSE, FALSE, 0);

    GtkWidget *mvbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_add(GTK_CONTAINER(map_card), mvbox);

    GtkWidget *mlbl = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(mlbl), "<b>Output File Map</b>");
    gtk_label_set_xalign(GTK_LABEL(mlbl), 0.0);
    gtk_box_pack_start(GTK_BOX(mvbox), mlbl, FALSE, FALSE, 0);

    for (int i = 0; i < OUTPUT_FILE_COUNT; i++) {
        char row_text[256];
        snprintf(row_text, sizeof(row_text),
            "<span font_family='monospace' size='small'>%-28s → ~/.config/%s</span>",
            output_files[i][0], output_files[i][1]);
        GtkWidget *rl = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(rl), row_text);
        gtk_label_set_xalign(GTK_LABEL(rl), 0.0);
        gtk_box_pack_start(GTK_BOX(mvbox), rl, FALSE, FALSE, 0);
    }

    return vbox;
}

/* ============================================================
 * Apply Action
 * ============================================================ */

static void on_apply_clicked(GtkButton *button, gpointer data) {
    (void)button; (void)data;

    if (app.selected < 0 || app.selected >= app.count) {
        if (app.lbl_status)
            gtk_label_set_text(GTK_LABEL(app.lbl_status), "No theme selected.");
        return;
    }

    /* Build surface filter (for future per-surface apply) */
    char surfaces_str[512] = {0};
    int has_disabled = 0;
    for (int i = 0; i < app.surface_count; i++) {
        if (!gtk_switch_get_active(GTK_SWITCH(app.surface_toggles[i]))) {
            has_disabled = 1;
        }
    }

    /* Apply via theme-engine */
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "theme-engine.sh apply '%s'",
             app.themes[app.selected].filepath);

    if (app.lbl_status)
        gtk_label_set_text(GTK_LABEL(app.lbl_status), "Applying theme...");

    char *output = run_cmd_output(cmd);

    if (app.lbl_status) {
        if (strstr(output, "error") || strstr(output, "fail"))
            gtk_label_set_text(GTK_LABEL(app.lbl_status), output);
        else {
            char msg[256];
            snprintf(msg, sizeof(msg), "Theme applied: %s",
                     app.themes[app.selected].name);
            gtk_label_set_text(GTK_LABEL(app.lbl_status), msg);
        }
    }

    g_free(output);

    /* Update active indicator */
    if (app.lbl_active) {
        char active_text[256];
        snprintf(active_text, sizeof(active_text), "Active: %s",
                 app.themes[app.selected].name);
        gtk_label_set_text(GTK_LABEL(app.lbl_active), active_text);
    }
}

/* ============================================================
 * Main Window
 * ============================================================ */

static void activate(GtkApplication *gtk_app, gpointer user_data) {
    (void)user_data;
    ocws_gtk_enforce_premium_theme();
    ocws_gtk_apply_dynamic_css(gtk_app, NULL);

    GtkWidget *window = gtk_application_window_new(gtk_app);
    gtk_window_set_title(GTK_WINDOW(window), "OCWS Theme Center");
    gtk_window_set_default_size(GTK_WINDOW(window), 1024, 680);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

    /* Header bar */
    GtkWidget *header = gtk_header_bar_new();
    gtk_header_bar_set_title(GTK_HEADER_BAR(header), "OCWS Theme Center");
    gtk_header_bar_set_subtitle(GTK_HEADER_BAR(header),
        "Browse, preview, and apply themes across all surfaces");
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), TRUE);
    gtk_window_set_titlebar(GTK_WINDOW(window), header);

    /* Apply button in header */
    GtkWidget *btn_apply = gtk_button_new_with_label("  Apply Theme  ");
    GtkStyleContext *actx = gtk_widget_get_style_context(btn_apply);
    gtk_style_context_add_class(actx, "suggested-action");
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), btn_apply);
    g_signal_connect(btn_apply, "clicked", G_CALLBACK(on_apply_clicked), NULL);

    /* Root: sidebar + stack */
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(window), hbox);

    GtkWidget *stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    gtk_stack_set_transition_duration(GTK_STACK(stack), 150);

    GtkWidget *sidebar = gtk_stack_sidebar_new();
    gtk_stack_sidebar_set_stack(GTK_STACK_SIDEBAR(sidebar), GTK_STACK(stack));
    gtk_widget_set_size_request(sidebar, 170, -1);
    gtk_box_pack_start(GTK_BOX(hbox), sidebar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox),
        gtk_separator_new(GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 0);

    /* Pages */
    gtk_stack_add_titled(GTK_STACK(stack), build_browser_page(),
                         "browser", "Themes");
    gtk_stack_add_titled(GTK_STACK(stack), build_preview_page(),
                         "preview", "Preview");
    gtk_stack_add_titled(GTK_STACK(stack), build_surfaces_page(),
                         "surfaces", "Surfaces");
    gtk_stack_add_titled(GTK_STACK(stack), build_import_page(),
                         "import", "Import/Export");

    gtk_box_pack_start(GTK_BOX(hbox), stack, TRUE, TRUE, 0);

    /* Status bar at bottom */
    GtkWidget *vroot = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    /* Rebuild: status at bottom, content above */
    gtk_container_remove(GTK_CONTAINER(window), hbox);
    gtk_box_pack_start(GTK_BOX(vroot), hbox, TRUE, TRUE, 0);

    GtkWidget *status_frame = gtk_frame_new(NULL);
    GtkStyleContext *sctx = gtk_widget_get_style_context(status_frame);
    gtk_style_context_add_class(sctx, "status-bar");
    gtk_widget_set_margin_start(status_frame, 12);
    gtk_widget_set_margin_end(status_frame, 12);
    gtk_widget_set_margin_bottom(status_frame, 8);

    app.lbl_status = gtk_label_new("Select a theme and click Apply to activate it.");
    gtk_label_set_xalign(GTK_LABEL(app.lbl_status), 0.0);
    gtk_container_add(GTK_CONTAINER(status_frame), app.lbl_status);
    gtk_box_pack_start(GTK_BOX(vroot), status_frame, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(window), vroot);

    gtk_widget_show_all(window);
}

/* ============================================================
 * Entry Point
 * ============================================================ */

int main(int argc, char *argv[]) {
    init_paths();
    discover_themes();

    fprintf(stderr, "ocws-theme-center: found %d themes in %s\n",
            app.count, app.project_dir);

    app.selected = app.count > 0 ? 0 : -1;

    GtkApplication *gtk_app = gtk_application_new(
        "org.ocws.theme-center", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(gtk_app, "activate", G_CALLBACK(activate), NULL);

    int status = g_application_run(G_APPLICATION(gtk_app), argc, argv);
    g_object_unref(gtk_app);
    return status;
}
