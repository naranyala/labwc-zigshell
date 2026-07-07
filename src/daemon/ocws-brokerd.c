#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "../libocws/ipc.h"

// ocws-brokerd: Native C daemon to replace ocws-daemon.sh and ocws-state.sh
// Handles inotify loops, pipewire state, and persistent state across sleep/resume

volatile sig_atomic_t running = 1;

void handle_signal(int sig) {
    running = 0;
}

int main(int argc, char* argv[]) {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    printf("Starting ocws-brokerd (C Native State Daemon)...\n");

    // TODO: Initialize State KV Store mapping
    // TODO: Setup inotify watchers for battery, backlight, thermal
    // TODO: Connect to PipeWire API for audio events
    // TODO: Connect to MPRIS/DBus for media player events

    while (running) {
        // Main event loop
        sleep(1); 
    }

    printf("Shutting down ocws-brokerd...\n");
    return 0;
}
