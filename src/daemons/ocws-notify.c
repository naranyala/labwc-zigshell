#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <glib.h>
#include <gio/gio.h>

#define MAX_NOTIFICATIONS 256
#define APPNAME "ocws-notify"

typedef struct {
    guint32 id;
    char *app_name;
    char *summary;
    char *body;
    char *icon;
    char *desktop_entry;
    int64_t timestamp;
    int timeout;
    int urgency;
    int closed;
} Notification;

static Notification *notifications[MAX_NOTIFICATIONS];
static int notif_count = 0;
static guint32 next_id = 1;
static GDBusConnection *bus_conn = NULL;
static guint bus_name_id = 0;

static void notif_free(Notification *n) {
    if (!n) return;
    free(n->app_name);
    free(n->summary);
    free(n->body);
    free(n->icon);
    free(n->desktop_entry);
    free(n);
}

static Notification *notif_new(void) {
    Notification *n = calloc(1, sizeof(Notification));
    n->id = next_id++;
    n->timestamp = g_get_real_time() / G_USEC_PER_SEC;
    n->timeout = 5000;
    n->urgency = 1;
    return n;
}

static void notif_store(Notification *n) {
    for (int i = 0; i < MAX_NOTIFICATIONS; i++) {
        if (notifications[i] == NULL) {
            notifications[i] = n;
            if (i + 1 > notif_count)
                notif_count = i + 1;
            return;
        }
    }
    notif_free(n);
}

static Notification *notif_find(guint32 id) {
    for (int i = 0; i < notif_count; i++) {
        if (notifications[i] && notifications[i]->id == id)
            return notifications[i];
    }
    return NULL;
}

static void notif_remove(guint32 id) {
    for (int i = 0; i < notif_count; i++) {
        if (notifications[i] && notifications[i]->id == id) {
            notif_free(notifications[i]);
            notifications[i] = NULL;
            return;
        }
    }
}

static int notif_active_count(void) {
    int count = 0;
    for (int i = 0; i < notif_count; i++) {
        if (notifications[i] && !notifications[i]->closed)
            count++;
    }
    return count;
}

static void emit_signal(const char *signal_name, GVariant *params) {
    if (!bus_conn) return;
    g_dbus_connection_emit_signal(bus_conn, NULL,
        "/org/freedesktop/Notifications",
        "org.freedesktop.Notifications",
        signal_name, params, NULL);
}

static void emit_notification_added(Notification *n) {
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("(susssasa{sv}i)"));
    g_variant_builder_add(&builder, "u", n->id);
    g_variant_builder_add(&builder, "s", n->app_name ? n->app_name : "");
    g_variant_builder_add(&builder, "u", (guint32)n->urgency);
    g_variant_builder_add(&builder, "s", n->summary ? n->summary : "");
    g_variant_builder_add(&builder, "s", n->body ? n->body : "");
    g_variant_builder_add(&builder, "s", n->icon ? n->icon : "");

    GVariantBuilder actions_builder;
    g_variant_builder_init(&actions_builder, G_VARIANT_TYPE("as"));
    g_variant_builder_add(&actions_builder, "s", "default");
    g_variant_builder_add(&actions_builder, "s", "Open");

    g_variant_builder_add(&builder, "as", &actions_builder);

    GVariantBuilder hints;
    g_variant_builder_init(&hints, G_VARIANT_TYPE("a{sv}"));
    if (n->desktop_entry) {
        g_variant_builder_add(&hints, "{sv}", "desktop-entry",
            g_variant_new_string(n->desktop_entry));
    }
    g_variant_builder_add(&builder, "a{sv}", &hints);
    g_variant_builder_add(&builder, "i", n->timeout);

    emit_signal("NotificationAdded", g_variant_new("(*)", g_variant_builder_end(&builder)));
}

static void emit_notification_closed(guint32 id, guint32 reason) {
    emit_signal("NotificationClosed", g_variant_new("(uu)", id, reason));
}

static gboolean notif_expire_cb(gpointer data) {
    Notification *exp = (Notification *)data;
    if (exp && !exp->closed) {
        exp->closed = 1;
        emit_notification_closed(exp->id, 2);
    }
    return G_SOURCE_REMOVE;
}

static void handle_get_server_information(GDBusMethodInvocation *invocation) {
    g_dbus_method_invocation_return_value(invocation,
        g_variant_new("(ssss)", APPNAME, "OCWS", "1.0", "1.2"));
}

static void handle_get_capabilities(GDBusConnection *connection, GDBusMethodInvocation *invocation, gpointer user_data) {
    (void)connection; (void)user_data;
    const char *caps[] = { "body", "body-markup", "icon-static", "persistence", NULL };
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("as"));
    for (int i = 0; caps[i]; i++)
        g_variant_builder_add(&builder, "s", caps[i]);
    g_dbus_method_invocation_return_value(invocation,
        g_variant_new("(as)", &builder));
}

static void handle_notify(GDBusMethodInvocation *invocation, GVariant *params) {
    const char *app_name = NULL;
    guint32 replaces_id = 0;
    const char *icon_str = NULL;
    const char *summary = NULL;
    const char *body_str = NULL;

    g_variant_get(params, "(&su&s&s&s@a{sv}i)",
        &app_name, &replaces_id, &icon_str, &summary, &body_str,
        NULL, NULL);

    Notification *n;

    if (replaces_id > 0 && (n = notif_find(replaces_id)) != NULL) {
        free(n->app_name);
        free(n->summary);
        free(n->body);
        free(n->icon);
        n->app_name = strdup(app_name ? app_name : "");
        n->summary = strdup(summary ? summary : "");
        n->body = strdup(body_str ? body_str : "");
        n->icon = strdup(icon_str ? icon_str : "");
        n->timestamp = g_get_real_time() / G_USEC_PER_SEC;
    } else {
        n = notif_new();
        n->app_name = strdup(app_name ? app_name : "");
        n->summary = strdup(summary ? summary : "");
        n->body = strdup(body_str ? body_str : "");
        n->icon = strdup(icon_str ? icon_str : "");
        notif_store(n);
    }

    /* Print to terminal for visibility */
    time_t now = n->timestamp;
    struct tm *tm = localtime(&now);
    char timebuf[32];
    strftime(timebuf, sizeof(timebuf), "%H:%M:%S", tm);

    fprintf(stderr, "\033[0;36m[%s]\033[0m \033[1m%s\033[0m\n", timebuf,
        n->summary ? n->summary : "(no summary)");
    if (n->body && *n->body)
        fprintf(stderr, "  %s\n", n->body);

    /* Emit D-Bus signal */
    emit_notification_added(n);

    /* Auto-expire if timeout > 0 */
    if (n->timeout > 0) {
        g_timeout_add_seconds(n->timeout / 1000, notif_expire_cb, n);
    }

    g_dbus_method_invocation_return_value(invocation,
        g_variant_new("u", n->id));
}

static void handle_close_notification(GDBusMethodInvocation *invocation, GVariant *params) {
    guint32 id;
    g_variant_get(params, "(u)", &id);

    Notification *n = notif_find(id);
    if (n && !n->closed) {
        n->closed = 1;
        emit_notification_closed(id, 3);
    }

    g_dbus_method_invocation_return_value(invocation, NULL);
}

static void handle_clear_notifications(GDBusMethodInvocation *invocation) {
    for (int i = 0; i < notif_count; i++) {
        if (notifications[i] && !notifications[i]->closed) {
            notifications[i]->closed = 1;
            emit_notification_closed(notifications[i]->id, 3);
        }
    }
    fprintf(stderr, "All notifications cleared\n");
    g_dbus_method_invocation_return_value(invocation, NULL);
}

static const gchar introspection_xml[] =
    "<node>"
    "  <interface name='org.freedesktop.Notifications'>"
    "    <method name='GetServerInformation'>"
    "      <arg direction='out' type='s' name='name'/>"
    "      <arg direction='out' type='s' name='vendor'/>"
    "      <arg direction='out' type='s' name='version'/>"
    "      <arg direction='out' type='s' name='spec_version'/>"
    "    </method>"
    "    <method name='GetCapabilities'>"
    "      <arg direction='out' type='as' name='capabilities'/>"
    "    </method>"
    "    <method name='Notify'>"
    "      <arg direction='in' type='s' name='app_name'/>"
    "      <arg direction='in' type='u' name='replaces_id'/>"
    "      <arg direction='in' type='s' name='app_icon'/>"
    "      <arg direction='in' type='s' name='summary'/>"
    "      <arg direction='in' type='s' name='body'/>"
    "      <arg direction='in' type='as' name='actions'/>"
    "      <arg direction='in' type='a{sv}' name='hints'/>"
    "      <arg direction='in' type='i' name='expire_timeout'/>"
    "      <arg direction='out' type='u' name='id'/>"
    "    </method>"
    "    <method name='CloseNotification'>"
    "      <arg direction='in' type='u' name='id'/>"
    "    </method>"
    "    <method name='Clear'/>"
    "    <signal name='NotificationAdded'>"
    "      <arg type='u' name='id'/>"
    "      <arg type='s' name='app_name'/>"
    "      <arg type='u' name='urgency'/>"
    "      <arg type='s' name='summary'/>"
    "      <arg type='s' name='body'/>"
    "      <arg type='as' name='actions'/>"
    "      <arg type='a{sv}' name='hints'/>"
    "      <arg type='i' name='expire_timeout'/>"
    "    </signal>"
    "    <signal name='NotificationClosed'>"
    "      <arg type='u' name='id'/>"
    "      <arg type='u' name='reason'/>"
    "    </signal>"
    "  </interface>"
    "</node>";

static GDBusNodeInfo *introspection_data = NULL;

static void handle_method_call(GDBusConnection *connection, const gchar *sender,
    const gchar *object_path, const gchar *interface_name, const gchar *method_name,
    GVariant *parameters, GDBusMethodInvocation *invocation, gpointer user_data)
{
    (void)connection; (void)sender; (void)object_path;
    (void)interface_name; (void)user_data;

    if (strcmp(method_name, "GetServerInformation") == 0) {
        handle_get_server_information(invocation);
    } else if (strcmp(method_name, "GetCapabilities") == 0) {
        handle_get_capabilities(NULL, invocation, NULL);
    } else if (strcmp(method_name, "Notify") == 0) {
        handle_notify(invocation, parameters);
    } else if (strcmp(method_name, "CloseNotification") == 0) {
        handle_close_notification(invocation, parameters);
    } else if (strcmp(method_name, "Clear") == 0) {
        handle_clear_notifications(invocation);
    } else {
        g_dbus_method_invocation_return_dbus_error(invocation,
            "org.freedesktop.DBus.Error.UnknownMethod", "Unknown method");
    }
}

static const GDBusInterfaceVTable interface_vtable = {
    handle_method_call, NULL, NULL, {0}
};

static void on_bus_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data) {
    (void)name; (void)user_data;
    bus_conn = connection;

    GError *error = NULL;
    GDBusInterfaceInfo *iface = g_dbus_node_info_lookup_interface(introspection_data,
        "org.freedesktop.Notifications");

    g_dbus_connection_register_object(connection,
        "/org/freedesktop/Notifications",
        iface, &interface_vtable, NULL, NULL, &error);

    if (error) {
        fprintf(stderr, "error registering object: %s\n", error->message);
        g_error_free(error);
    } else {
        fprintf(stderr, "ocws-notify: listening on D-Bus as %s\n", name);
    }
}

static void on_name_lost(GDBusConnection *connection, const gchar *name, gpointer user_data) {
    (void)connection; (void)name; (void)user_data;
    fprintf(stderr, "ocws-notify: lost bus name (another notification daemon running?)\n");
}

static void on_name_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data) {
    (void)connection; (void)name; (void)user_data;
}

static volatile int running = 1;

static void on_signal(int sig) {
    (void)sig;
    running = 0;
}

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [OPTIONS]\n\n"
        "Native notification daemon for OCWS desktop shell.\n"
        "Implements the org.freedesktop.Notifications D-Bus interface.\n\n"
        "Options:\n"
        "  -d           Daemonize (run in background)\n"
        "  -h           Show this help\n\n"
        "Sends notifications to stdout and emits D-Bus signals.\n"
        "Replaces mako/dunst as a lightweight C-native daemon.\n",
        prog);
}

int main(int argc, char *argv[]) {
    int daemonize = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) daemonize = 1;
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        }
    }

    umask(0077);
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    if (daemonize) {
        pid_t pid = fork();
        if (pid > 0) { printf("ocws-notify started (PID %d)\n", pid); return 0; }
        if (pid < 0) { perror("fork"); return 1; }
        setsid();
        fclose(stdin);
    }

    GMainLoop *loop = g_main_loop_new(NULL, FALSE);

    bus_name_id = g_bus_own_name(G_BUS_TYPE_SESSION,
        "org.freedesktop.Notifications",
        G_BUS_NAME_OWNER_FLAGS_NONE,
        on_bus_acquired, on_name_acquired, on_name_lost,
        NULL, NULL);

    fprintf(stderr, "ocws-notify: starting notification daemon...\n");

    while (running) g_main_loop_run(loop);

    g_bus_unown_name(bus_name_id);
    g_main_loop_unref(loop);

    if (introspection_data) g_dbus_node_info_unref(introspection_data);

    for (int i = 0; i < notif_count; i++)
        notif_free(notifications[i]);

    fprintf(stderr, "ocws-notify: shutdown\n");
    return 0;
}
