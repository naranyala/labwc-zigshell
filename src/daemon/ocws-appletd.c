#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include "../libocws/ipc.h"

/* 
 * ocws-appletd: Unified Native Applet Daemon
 * Replaces multiple bash sleep loops with a single C event loop.
 */

volatile sig_atomic_t running = 1;

void handle_signal(int sig) {
    running = 0;
}

// --- MODULE: POMODORO ---
static int pomodoro_running = 0;
static time_t pomodoro_end_time = 0;
static const char* state_file = "/tmp/ocws-pomodoro.state";

static void pomodoro_init(void) {
    FILE* f = fopen(state_file, "r");
    if (f) {
        long end_val = 0;
        if (fscanf(f, "%ld", &end_val) == 1) {
            pomodoro_end_time = (time_t)end_val;
            pomodoro_running = 1;
            printf("[Pomodoro] Resumed timer from state file.\n");
        }
        fclose(f);
    }
}

static void pomodoro_tick(void) {
    if (!pomodoro_running) {
        // Just to check if another process started it via CLI
        pomodoro_init();
        if (!pomodoro_running) return;
    }
    
    time_t now = time(NULL);
    if (now >= pomodoro_end_time) {
        pomodoro_running = 0;
        remove(state_file);
        printf("[Pomodoro] Timer finished!\n");
        
        // Use system to call native ocws-notify, or emit to sfwbar
        system("ocws-notify --app 'Pomodoro' --title 'Time is Up!' --body 'Take a break' --icon 'timer' 2>/dev/null || "
               "notify-send 'Pomodoro' 'Time is up! Take a break.'");
               
        // ocws_emit("Applet.PomodoroState", "finished");
    } else {
        int rem = (int)(pomodoro_end_time - now);
        int mins = rem / 60;
        int secs = rem % 60;
        // In a real integration, we'd call ocws_emit() here:
        // char buf[16]; snprintf(buf, sizeof(buf), "%02d:%02d", mins, secs);
        // ocws_emit("Applet.PomodoroTime", buf);
        
        // For debugging output when run in terminal:
        if (rem % 10 == 0) { // log every 10s to not spam
            printf("[Pomodoro] %02d:%02d remaining\n", mins, secs);
        }
    }
}

// --- MODULE: CRYPTO ---
static void crypto_tick(void) {
    printf("[Crypto] Fetching prices...\n");
    // We use popen to avoid linking full libcurl + json-c for now,
    // while still gaining the benefit of a single unified daemon loop.
    FILE* fp = popen("curl -s -m 10 'https://api.coingecko.com/api/v3/simple/price?ids=bitcoin,ethereum&vs_currencies=usd' 2>/dev/null | jq -r '.bitcoin.usd, .ethereum.usd' 2>/dev/null", "r");
    if (fp) {
        char btc[32] = {0}, eth[32] = {0};
        if (fgets(btc, sizeof(btc), fp) && fgets(eth, sizeof(eth), fp)) {
            btc[strcspn(btc, "\n")] = 0;
            eth[strcspn(eth, "\n")] = 0;
            printf("[Crypto] BTC: $%s | ETH: $%s\n", btc, eth);
            
            // ocws_emit("Applet.CryptoBTC", btc);
            // ocws_emit("Applet.CryptoETH", eth);
        }
        pclose(fp);
    }
}

// --- MODULE: GITHUB ---
static void github_tick(void) {
    char token_path[512];
    snprintf(token_path, sizeof(token_path), "%s/.config/ocws/tokens/github", getenv("HOME"));
    
    FILE* tf = fopen(token_path, "r");
    if (!tf) {
        printf("[GitHub] No token found, skipping.\n");
        return;
    }
    char token[128] = {0};
    if (!fgets(token, sizeof(token), tf)) {
        fclose(tf);
        return;
    }
    token[strcspn(token, "\n")] = 0;
    fclose(tf);

    printf("[GitHub] Fetching notifications...\n");
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "curl -s -m 10 -H 'Authorization: token %s' https://api.github.com/notifications 2>/dev/null | jq 'length' 2>/dev/null", token);
    
    FILE* fp = popen(cmd, "r");
    if (fp) {
        char count[16] = {0};
        if (fgets(count, sizeof(count), fp)) {
            count[strcspn(count, "\n")] = 0;
            if (strcmp(count, "") != 0 && strstr(count, "Bad credentials") == NULL) {
                printf("[GitHub] Unread: %s\n", count);
                // ocws_emit("Applet.GitHubUnread", count);
            }
        }
        pclose(fp);
    }
}

// --- MODULAR APPLET REGISTRY ---
typedef struct {
    const char* name;
    int tick_interval_sec;
    time_t last_tick;
    void (*init)(void);
    void (*on_tick)(void);
    void (*shutdown)(void);
} OcwsApplet;

OcwsApplet applets[] = {
    {"Pomodoro", 1,   0, pomodoro_init, pomodoro_tick, NULL},
    {"Crypto",   300, 0, NULL,          crypto_tick,   NULL},
    {"GitHub",   600, 0, NULL,          github_tick,   NULL}
};
#define NUM_APPLETS (sizeof(applets)/sizeof(applets[0]))

int main(int argc, char* argv[]) {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    printf("=== OCWS Unified Applet Daemon Started ===\n");
    
    for (int i = 0; i < NUM_APPLETS; i++) {
        if (applets[i].init) {
            applets[i].init();
        }
    }
    
    while(running) {
        time_t now = time(NULL);
        for (int i = 0; i < NUM_APPLETS; i++) {
            if (now - applets[i].last_tick >= applets[i].tick_interval_sec) {
                applets[i].last_tick = now;
                if (applets[i].on_tick) {
                    applets[i].on_tick();
                }
            }
        }
        sleep(1);
    }
    
    printf("=== OCWS Unified Applet Daemon Shutting Down ===\n");
    for (int i = 0; i < NUM_APPLETS; i++) {
        if (applets[i].shutdown) {
            applets[i].shutdown();
        }
    }

    return 0;
}
