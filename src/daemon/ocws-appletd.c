#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <dirent.h>
#include <glib.h>
#include <sys/stat.h>
#include "../libocws/plugin_api.h"

/* 
 * ocws-appletd: Unified Native Applet Daemon
 * Modular architecture loading .so plugins dynamically and running via GMainLoop.
 */

#define MAX_PLUGINS 64
static OcwsPlugin* loaded_plugins[MAX_PLUGINS];
static void* plugin_handles[MAX_PLUGINS];
static int plugin_count = 0;

static GMainLoop *main_loop = NULL;
static volatile sig_atomic_t got_signal = 0;

static gboolean plugin_tick_cb(gpointer user_data) {
    OcwsPlugin* plugin = (OcwsPlugin*)user_data;
    if (plugin && plugin->on_tick) {
        plugin->on_tick();
    }
    return G_SOURCE_CONTINUE;
}

static void load_plugin(const char* filepath) {
    if (plugin_count >= MAX_PLUGINS) {
        fprintf(stderr, "[Appletd] Max plugins reached.\n");
        return;
    }

    void* handle = dlopen(filepath, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        fprintf(stderr, "[Appletd] Failed to load plugin %s: %s\n", filepath, dlerror());
        return;
    }

    OcwsPlugin* plugin = (OcwsPlugin*)dlsym(handle, "OCWS_PLUGIN_ENTRY");
    if (!plugin) {
        fprintf(stderr, "[Appletd] Missing OCWS_PLUGIN_ENTRY in %s\n", filepath);
        dlclose(handle);
        return;
    }

    if (plugin->api_version != OCWS_PLUGIN_API_VERSION) {
        fprintf(stderr, "[Appletd] API version mismatch in %s (expected %d, got %d)\n", 
                filepath, OCWS_PLUGIN_API_VERSION, plugin->api_version);
        dlclose(handle);
        return;
    }

    if (plugin->init) {
        if (plugin->init() != 0) {
            fprintf(stderr, "[Appletd] Plugin %s failed to initialize.\n", plugin->name);
            dlclose(handle);
            return;
        }
    }

    if (plugin->tick_interval_sec > 0 && plugin->on_tick) {
        g_timeout_add_seconds(plugin->tick_interval_sec, plugin_tick_cb, plugin);
    }

    loaded_plugins[plugin_count] = plugin;
    plugin_handles[plugin_count] = handle;
    plugin_count++;

    printf("[Appletd] Loaded plugin: %s\n", plugin->name);
}

static void discover_plugins(void) {
    char plugin_dir[1024];
    snprintf(plugin_dir, sizeof(plugin_dir), "%s/.local/lib/ocws/plugins", getenv("HOME"));
    
    // Also check local build dir for development
    const char* dev_dir = "zig-out/lib/ocws/plugins";

    const char* dirs_to_check[] = { plugin_dir, dev_dir, NULL };
    
    for (int d = 0; dirs_to_check[d] != NULL; d++) {
        DIR *dir = opendir(dirs_to_check[d]);
        if (!dir) continue;
        
        printf("[Appletd] Scanning %s for plugins...\n", dirs_to_check[d]);
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            if (strstr(ent->d_name, ".so")) {
                char filepath[2048];
                snprintf(filepath, sizeof(filepath), "%s/%s", dirs_to_check[d], ent->d_name);
                load_plugin(filepath);
            }
        }
        closedir(dir);
    }
}

static void handle_signal(int sig) {
    (void)sig;
    got_signal = 1;
}

static gboolean signal_check_cb(gpointer user_data) {
    (void)user_data;
    if (got_signal && main_loop) {
        g_main_loop_quit(main_loop);
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    umask(0077);

    printf("=== OCWS Unified Applet Daemon (Modular) ===\n");
    
    discover_plugins();

    if (plugin_count == 0) {
        printf("[Appletd] No plugins loaded. Still running to accept IPC events...\n");
    }

    main_loop = g_main_loop_new(NULL, FALSE);
    g_timeout_add(200, signal_check_cb, NULL);
    printf("[Appletd] Entering GLib main loop.\n");
    g_main_loop_run(main_loop);

    printf("=== OCWS Unified Applet Daemon Shutting Down ===\n");
    
    for (int i = 0; i < plugin_count; i++) {
        if (loaded_plugins[i]->shutdown) {
            loaded_plugins[i]->shutdown();
        }
        dlclose(plugin_handles[i]);
    }

    g_main_loop_unref(main_loop);
    return 0;
}
