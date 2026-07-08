/*
 * settings-ui.c — UI Helper Widgets
 * Implementation of reusable GTK3 widget builders.
 */

#include "settings-ui.h"
#include "../../core/utils.h"
#include "../../libocws/gtk.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================
// Configuration
// ============================================================

const char *OCWS_HOME = NULL;

void init_paths(void) {
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    static char buf[512];
    snprintf(buf, sizeof(buf), "%s/%s", home, OCWS_DIR);
    OCWS_HOME = buf;
}

// ============================================================
// KV Store Helpers
// ============================================================

char* kv_get(const char *key) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "%s get %s 2>/dev/null", KV_BIN, key);
    FILE *fp = popen(cmd, "r");
    if (!fp) return g_strdup("");
    char buf[256] = {0};
    if (fgets(buf, sizeof(buf), fp)) {
        buf[strcspn(buf, "\n")] = 0;
    }
    pclose(fp);
    return g_strdup(buf);
}

void kv_set(const char *key, const char *value) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "%s set %s '%s'", KV_BIN, key, value);
    run_cmd_async(cmd);
}

// ============================================================
// Command Execution
// ============================================================

void execute_command(GtkWidget *widget, gpointer data) {
    (void)widget;
    const char *cmd = (const char *)data;
    if (cmd && cmd[0]) run_cmd_async(cmd);
}

static void free_ptr(gpointer data, GClosure *closure);

// ============================================================
// Card Widgets
// ============================================================

GtkWidget* create_card(const char *title, const char *icon) {
    GtkWidget *frame = gtk_frame_new(NULL);
    GtkStyleContext *ctx = gtk_widget_get_style_context(frame);
    gtk_style_context_add_class(ctx, "settings-card");

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 16);
    gtk_container_add(GTK_CONTAINER(frame), vbox);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    if (icon) {
        GtkWidget *lbl_icon = gtk_label_new(icon);
        gtk_box_pack_start(GTK_BOX(hbox), lbl_icon, FALSE, FALSE, 0);
    }

    char *markup = g_strdup_printf("<b>%s</b>", title);
    GtkWidget *lbl = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl), markup);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);
    gtk_box_pack_start(GTK_BOX(hbox), lbl, TRUE, TRUE, 0);
    g_free(markup);

    return vbox;
}

// ============================================================
// Collapsible Card — per-submenu display switch
// ============================================================

typedef struct {
    GtkWidget *content_box;
    GtkWidget *expand_icon;
    gboolean expanded;
} CollapsibleData;

static gboolean on_collapse_clicked(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    (void)widget; (void)event;
    CollapsibleData *data = (CollapsibleData *)user_data;
    data->expanded = !data->expanded;

    if (data->expanded) {
        gtk_widget_show(data->content_box);
        gtk_label_set_markup(GTK_LABEL(data->expand_icon), "▼");
    } else {
        gtk_widget_hide(data->content_box);
        gtk_label_set_markup(GTK_LABEL(data->expand_icon), "▶");
    }
    return GDK_EVENT_STOP;
}

GtkWidget* create_collapsible_card(const char *title, const char *icon, gboolean start_expanded) {
    GtkWidget *frame = gtk_frame_new(NULL);
    GtkStyleContext *ctx = gtk_widget_get_style_context(frame);
    gtk_style_context_add_class(ctx, "settings-card");

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 0);
    gtk_container_add(GTK_CONTAINER(frame), vbox);

    // Header (clickable)
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_top(header, 12);
    gtk_widget_set_margin_bottom(header, 12);
    gtk_widget_set_margin_start(header, 16);
    gtk_widget_set_margin_end(header, 16);
    gtk_box_pack_start(GTK_BOX(vbox), header, FALSE, FALSE, 0);

    CollapsibleData *data = g_new0(CollapsibleData, 1);
    data->expanded = start_expanded;

    if (icon) {
        GtkWidget *lbl_icon = gtk_label_new(icon);
        gtk_box_pack_start(GTK_BOX(header), lbl_icon, FALSE, FALSE, 0);
    }

    char *markup = g_strdup_printf("<b>%s</b>", title);
    GtkWidget *lbl = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl), markup);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);
    gtk_box_pack_start(GTK_BOX(header), lbl, TRUE, TRUE, 0);
    g_free(markup);

    // Expand/collapse indicator
    data->expand_icon = gtk_label_new(start_expanded ? "▼" : "▶");
    gtk_box_pack_start(GTK_BOX(header), data->expand_icon, FALSE, FALSE, 0);

    // Make header clickable
    GtkWidget *event_box = gtk_event_box_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(event_box), "collapsible-header");
    gtk_container_add(GTK_CONTAINER(event_box), header);
    gtk_box_pack_start(GTK_BOX(vbox), event_box, FALSE, FALSE, 0);
    g_signal_connect(event_box, "button-release-event", G_CALLBACK(on_collapse_clicked), data);

    // Separator
    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 0);

    // Content area (collapsible)
    data->content_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_top(data->content_box, 8);
    gtk_widget_set_margin_bottom(data->content_box, 12);
    gtk_widget_set_margin_start(data->content_box, 16);
    gtk_widget_set_margin_end(data->content_box, 16);
    gtk_box_pack_start(GTK_BOX(vbox), data->content_box, TRUE, TRUE, 0);

    if (!start_expanded) {
        gtk_widget_hide(data->content_box);
    }

    // Store data on the frame for later access
    g_object_set_data(G_OBJECT(frame), "collapsible_data", data);
    g_object_set_data(G_OBJECT(frame), "content_box", data->content_box);

    return frame;
}

GtkWidget* get_collapsible_content(GtkWidget *card) {
    return GTK_WIDGET(g_object_get_data(G_OBJECT(card), "content_box"));
}

// ============================================================
// Toggle Row
// ============================================================

typedef struct {
    char cmd_on[256];
    char cmd_off[256];
} ToggleRowData;

static void on_toggle_row_changed(GObject *gobject, GParamSpec *pspec, gpointer user_data) {
    (void)pspec;
    ToggleRowData *d = (ToggleRowData *)user_data;
    gboolean active = gtk_switch_get_active(GTK_SWITCH(gobject));
    const char *cmd = active ? d->cmd_on : d->cmd_off;
    if (cmd && cmd[0]) system(cmd);
}

GtkWidget* create_toggle_row(const char *label, const char *description, gboolean active) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_bottom(row, 6);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *lbl = gtk_label_new(label);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), lbl, FALSE, FALSE, 0);

    if (description) {
        GtkWidget *desc = gtk_label_new(description);
        gtk_label_set_xalign(GTK_LABEL(desc), 0.0);
        gtk_style_context_add_class(gtk_widget_get_style_context(desc), "dim-label");
        gtk_box_pack_start(GTK_BOX(vbox), desc, FALSE, FALSE, 0);
    }

    GtkWidget *toggle = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(toggle), active);
    gtk_widget_set_valign(toggle, GTK_ALIGN_CENTER);

    gtk_box_pack_start(GTK_BOX(row), vbox, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row), toggle, FALSE, FALSE, 0);

    return row;
}

GtkWidget* create_live_toggle_row(const char *label, const char *description, gboolean active, const char *cmd_on, const char *cmd_off) {
    GtkWidget *row = create_toggle_row(label, description, active);

    // Get the switch widget (last child of row)
    GList *children = gtk_container_get_children(GTK_CONTAINER(row));
    GtkWidget *toggle = GTK_WIDGET(g_list_last(children)->data);
    g_list_free(children);

    ToggleRowData *d = g_new0(ToggleRowData, 1);
    if (cmd_on) g_strlcpy(d->cmd_on, cmd_on, sizeof(d->cmd_on));
    if (cmd_off) g_strlcpy(d->cmd_off, cmd_off, sizeof(d->cmd_off));

    g_signal_connect_data(toggle, "notify::active", G_CALLBACK(on_toggle_row_changed), d, (GClosureNotify)free_ptr, 0);

    return row;
}

// ============================================================
// Slider Row
// ============================================================

typedef struct {
    char cmd_template[256];
    GtkWidget *val_label;
    const char *unit;
    double step;
} SliderData;

static void on_slider_changed(GtkRange *range, gpointer user_data) {
    SliderData *d = (SliderData *)user_data;
    double val = gtk_range_get_value(range);

    // Update value label
    char val_text[32];
    if (val == (int)val)
        snprintf(val_text, sizeof(val_text), "%d%s", (int)val, d->unit ? d->unit : "");
    else
        snprintf(val_text, sizeof(val_text), "%.1f%s", val, d->unit ? d->unit : "");
    gtk_label_set_text(GTK_LABEL(d->val_label), val_text);

    // Execute command
    char cmd[512];
    if (d->step < 1.0) {
        // Float-capable command (e.g. font-scale.sh) — pass double for %f
        snprintf(cmd, sizeof(cmd), d->cmd_template, val);
    } else {
        // Integer-only command
        snprintf(cmd, sizeof(cmd), d->cmd_template, (int)(val + 0.5));
    }
    if (cmd[0]) system(cmd);
}

GtkWidget* create_slider_row(const char *label, int value, int min, int max, const char *unit) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_bottom(row, 6);

    GtkWidget *lbl = gtk_label_new(label);
    gtk_widget_set_size_request(lbl, 140, -1);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);

    GtkWidget *scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, min, max, 1);
    gtk_scale_set_value_pos(GTK_SCALE(scale), GTK_POS_RIGHT);
    gtk_range_set_value(GTK_RANGE(scale), value);
    gtk_widget_set_hexpand(scale, TRUE);

    char val_text[32];
    snprintf(val_text, sizeof(val_text), "%d%s", value, unit ? unit : "");
    GtkWidget *val_lbl = gtk_label_new(val_text);
    gtk_widget_set_size_request(val_lbl, 60, -1);
    gtk_label_set_xalign(GTK_LABEL(val_lbl), 1.0);

    gtk_box_pack_start(GTK_BOX(row), lbl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), scale, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row), val_lbl, FALSE, FALSE, 0);

    return row;
}

GtkWidget* create_live_slider_row(const char *label, int value, int min, int max, const char *unit, const char *cmd_template) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_bottom(row, 6);

    GtkWidget *lbl = gtk_label_new(label);
    gtk_widget_set_size_request(lbl, 140, -1);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);

    double step = (min >= 6 && max <= 24 && value >= 6 && value <= 24) ? 0.5 : 1.0;
    GtkWidget *scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, min, max, step);
    gtk_scale_set_value_pos(GTK_SCALE(scale), GTK_POS_RIGHT);
    gtk_range_set_value(GTK_RANGE(scale), value);
    gtk_widget_set_hexpand(scale, TRUE);

    char val_text[32];
    snprintf(val_text, sizeof(val_text), "%d%s", value, unit ? unit : "");
    GtkWidget *val_lbl = gtk_label_new(val_text);
    gtk_widget_set_size_request(val_lbl, 60, -1);
    gtk_label_set_xalign(GTK_LABEL(val_lbl), 1.0);

    SliderData *d = g_new0(SliderData, 1);
    if (cmd_template) g_strlcpy(d->cmd_template, cmd_template, sizeof(d->cmd_template));
    d->val_label = val_lbl;
    d->unit = unit;
    d->step = step;

    g_signal_connect_data(scale, "value-changed", G_CALLBACK(on_slider_changed), d, (GClosureNotify)free_ptr, 0);

    gtk_box_pack_start(GTK_BOX(row), lbl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), scale, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row), val_lbl, FALSE, FALSE, 0);

    return row;
}

// ============================================================
// Button Group
// ============================================================

GtkWidget* create_button_group(const char *label, const char **options, int count, int active) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_bottom(row, 6);

    GtkWidget *lbl = gtk_label_new(label);
    gtk_widget_set_size_request(lbl, 140, -1);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);
    gtk_box_pack_start(GTK_BOX(row), lbl, FALSE, FALSE, 0);

    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    for (int i = 0; i < count; i++) {
        GtkWidget *btn = gtk_button_new_with_label(options[i]);
        GtkStyleContext *ctx = gtk_widget_get_style_context(btn);
        if (i == active) gtk_style_context_add_class(ctx, "suggested-action");
        gtk_box_pack_start(GTK_BOX(btn_box), btn, TRUE, TRUE, 0);
    }
    gtk_box_pack_start(GTK_BOX(row), btn_box, TRUE, TRUE, 0);
    return row;
}

// ============================================================
// Action Row
// ============================================================

GtkWidget* create_action_row(const char *title, const char *subtitle, const char *button_label, const char *command) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_bottom(row, 10);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    char *markup = g_strdup_printf("<b>%s</b>", title);
    GtkWidget *lbl_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_title), markup);
    gtk_label_set_xalign(GTK_LABEL(lbl_title), 0.0);
    g_free(markup);

    GtkWidget *lbl_sub = gtk_label_new(subtitle);
    gtk_label_set_xalign(GTK_LABEL(lbl_sub), 0.0);
    gtk_style_context_add_class(gtk_widget_get_style_context(lbl_sub), "dim-label");

    gtk_box_pack_start(GTK_BOX(vbox), lbl_title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), lbl_sub, FALSE, FALSE, 0);

    GtkWidget *btn = gtk_button_new_with_label(button_label);
    gtk_widget_set_valign(btn, GTK_ALIGN_CENTER);
    g_signal_connect(btn, "clicked", G_CALLBACK(execute_command), (gpointer)command);

    gtk_box_pack_start(GTK_BOX(row), vbox, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row), btn, FALSE, FALSE, 0);

    return row;
}

// ============================================================
// Keybind Row
// ============================================================

GtkWidget* create_keybind_row(const char *label, const char *command) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_bottom(row, 6);

    GtkWidget *lbl = gtk_label_new(label);
    gtk_widget_set_size_request(lbl, 140, -1);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);

    char *markup = g_strdup_printf("<span font_family='monospace' foreground='#a6adc8'>%s</span>", command);
    GtkWidget *cmd_lbl = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(cmd_lbl), markup);
    gtk_label_set_ellipsize(GTK_LABEL(cmd_lbl), PANGO_ELLIPSIZE_END);
    g_free(markup);

    GtkWidget *btn = gtk_button_new_from_icon_name("edit-copy-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_widget_set_valign(btn, GTK_ALIGN_CENTER);

    gtk_box_pack_start(GTK_BOX(row), lbl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), cmd_lbl, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row), btn, FALSE, FALSE, 0);

    return row;
}

// ============================================================
// Shell Card
// ============================================================

GtkWidget* create_shell_card(const char *title, const char *desc, const char *mode, const char *icon_name) {
    GtkWidget *btn = gtk_button_new();
    gtk_widget_set_size_request(btn, 200, 140);
    GtkStyleContext *ctx = gtk_widget_get_style_context(btn);
    gtk_style_context_add_class(ctx, "shell-card");

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_halign(vbox, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(vbox, GTK_ALIGN_CENTER);

    GtkWidget *icon = gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_DIALOG);
    gtk_box_pack_start(GTK_BOX(vbox), icon, FALSE, FALSE, 0);

    char *markup = g_strdup_printf("<span size='large' weight='bold'>%s</span>", title);
    GtkWidget *lbl_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_title), markup);
    g_free(markup);
    gtk_box_pack_start(GTK_BOX(vbox), lbl_title, FALSE, FALSE, 0);

    GtkWidget *lbl_desc = gtk_label_new(desc);
    gtk_label_set_line_wrap(GTK_LABEL(lbl_desc), TRUE);
    gtk_label_set_justify(GTK_LABEL(lbl_desc), GTK_JUSTIFY_CENTER);
    gtk_style_context_add_class(gtk_widget_get_style_context(lbl_desc), "dim-label");
    gtk_box_pack_start(GTK_BOX(vbox), lbl_desc, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(btn), vbox);
    char *cmd = g_strdup_printf("toggle-shell %s", mode);
    g_signal_connect_data(btn, "clicked", G_CALLBACK(execute_command), cmd, (GClosureNotify)free_ptr, 0);
    return btn;
}

// ============================================================
// Healthcheck Loader
// ============================================================

void load_healthcheck(GtkWidget *textview) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
    gtk_text_buffer_set_text(buffer, "Running system validation...\n", -1);
    gtk_widget_queue_draw(textview);

    FILE *fp = popen("ocws-validate 2>&1", "r");
    if (!fp) {
        gtk_text_buffer_set_text(buffer, "Failed to run ocws-validate. Is it in your PATH?", -1);
        return;
    }

    char result[16384] = {0};
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        // Strip ANSI escape codes
        char clean[512] = {0};
        int j = 0;
        for (int i = 0; line[i]; i++) {
            if (line[i] == '\033') { while (line[i] && line[i] != 'm') i++; continue; }
            if (j < (int)sizeof(clean) - 1) clean[j++] = line[i];
        }
        strncat(result, clean, sizeof(result) - strlen(result) - 1);
    }
    pclose(fp);
    gtk_text_buffer_set_text(buffer, result, -1);
}

// ============================================================
// System Info Loader
// ============================================================

void load_system_info(GtkWidget *textview) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
    char info[4096] = {0};

    // Kernel
    FILE *fp = popen("uname -r 2>/dev/null", "r");
    if (fp) { char buf[128] = {0}; fgets(buf, sizeof(buf), fp); pclose(fp);
        buf[strcspn(buf, "\n")] = 0; strcat(info, "Kernel: "); strcat(info, buf); strcat(info, "\n"); }

    // labwc version
    fp = popen("labwc --version 2>/dev/null | head -1", "r");
    if (fp) { char buf[128] = {0}; fgets(buf, sizeof(buf), fp); pclose(fp);
        buf[strcspn(buf, "\n")] = 0; strcat(info, "Compositor: "); strcat(info, buf); strcat(info, "\n"); }

    // sfwbar version
    fp = popen("sfwbar --version 2>/dev/null | head -1", "r");
    if (fp) { char buf[128] = {0}; fgets(buf, sizeof(buf), fp); pclose(fp);
        buf[strcspn(buf, "\n")] = 0; strcat(info, "Bar Engine: "); strcat(info, buf); strcat(info, "\n"); }

    // Memory
    fp = popen("free -h 2>/dev/null | awk '/Mem:/{print $3\"/\"$2}'", "r");
    if (fp) { char buf[128] = {0}; fgets(buf, sizeof(buf), fp); pclose(fp);
        buf[strcspn(buf, "\n")] = 0; strcat(info, "Memory: "); strcat(info, buf); strcat(info, "\n"); }

    // Disk
    fp = popen("df -h / 2>/dev/null | awk 'NR==2{print $3\"/\"$2\" (\"$5\" used)\"}'", "r");
    if (fp) { char buf[128] = {0}; fgets(buf, sizeof(buf), fp); pclose(fp);
        buf[strcspn(buf, "\n")] = 0; strcat(info, "Disk: "); strcat(info, buf); strcat(info, "\n"); }

    // CPU
    fp = popen("nproc 2>/dev/null", "r");
    if (fp) { char buf[32] = {0}; fgets(buf, sizeof(buf), fp); pclose(fp);
        buf[strcspn(buf, "\n")] = 0; strcat(info, "CPU Cores: "); strcat(info, buf); strcat(info, "\n"); }

    // OCWS version
    strcat(info, "OCWS Version: "); strcat(info, VERSION); strcat(info, "\n");
    strcat(info, "OCWS Home: "); strcat(info, OCWS_HOME); strcat(info, "\n");

    gtk_text_buffer_set_text(buffer, info, -1);
}

// ============================================================
// CSS
// ============================================================

static void free_ptr(gpointer data, GClosure *closure) {
    (void)closure;
    g_free(data);
}

void apply_css(GtkApplication *app) {
    (void)app;
    GtkCssProvider *provider = gtk_css_provider_new();
    char css[8192] = {0};

    /* Fallback tokens — used when tokens.css is absent, overridden if file exists */
    snprintf(css, sizeof(css),
        "@define-color ocws_bg #1e1e2e;"
        "@define-color ocws_fg #cdd6f4;"
        "@define-color ocws_mantle #181825;"
        "@define-color ocws_surface0 #313244;"
        "@define-color ocws_surface1 #45475a;"
        "@define-color ocws_accent #89b4fa;"
        "@define-color ocws_subtext0 #a6adc8;"
        "@define-color ocws_sapphire #74c7ec;"
    );
    int pos = (int)strlen(css);

    /* Load tokens.css if available (overrides fallbacks) */
    char tokpath[1024];
    snprintf(tokpath, sizeof(tokpath), "%s/tokens.css", OCWS_HOME);
    FILE *f = fopen(tokpath, "r");
    if (f) {
        size_t n = fread(css + pos, 1, sizeof(css) - pos - 2048, f);
        fclose(f);
        pos += (int)n;
    }

    /* Application CSS — references @ocws_* variables defined above */
    snprintf(css + pos, sizeof(css) - pos,
        ".settings-card { background-color: alpha(@ocws_bg, 0.85); border: 1px solid alpha(@ocws_fg, 0.08); border-radius: 16px; padding: 16px; box-shadow: 0 4px 12px alpha(black, 0.2); transition: all 200ms ease; }"
        ".settings-card:hover { box-shadow: 0 6px 16px alpha(black, 0.3); border-color: alpha(@ocws_accent, 0.2); }"
        ".collapsible-header { cursor: pointer; transition: all 150ms ease; }"
        ".collapsible-header:hover { background-color: alpha(@ocws_accent, 0.1); border-radius: 8px; }"
        ".shell-card { padding: 20px; border-radius: 12px; border: 1px solid alpha(@ocws_fg, 0.08); background-color: alpha(@ocws_surface0, 0.75); transition: all 200ms ease; box-shadow: 0 4px 12px alpha(black, 0.2); }"
        ".shell-card:hover { background-color: alpha(@ocws_accent, 0.15); border-color: alpha(@ocws_accent, 0.4); box-shadow: 0 8px 24px alpha(black, 0.4); }"
        ".dim-label { opacity: 0.7; font-size: 0.9em; }"
        "textview.terminal { font-family: 'Noto Sans Mono', monospace; font-size: 12px; background-color: @ocws_mantle; color: @ocws_fg; padding: 12px; border-radius: 8px; border: 1px solid alpha(@ocws_fg, 0.1); box-shadow: inset 0 2px 4px alpha(black, 0.2); }"
        "notebook tab { padding: 8px 16px; }"
        "switch { min-width: 48px; min-height: 24px; }"
        "scale trough { min-height: 6px; border-radius: 3px; }"
        "scale slider { min-width: 18px; min-height: 18px; border-radius: 9px; }"
        "* { font-family: 'Noto Sans', sans-serif; }"
    );

    gtk_css_provider_load_from_data(provider, css, -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}
