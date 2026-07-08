#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define RED     "\033[0;31m"
#define GREEN   "\033[0;32m"
#define CYAN    "\033[0;36m"
#define NC      "\033[0m"

#define MAX_ITEMS 50

void pass(const char *msg) { printf("%s✓%s %s\n", GREEN, NC, msg); }
void fail(const char *msg) { printf("%s✗%s %s\n", RED, NC, msg); exit(1); }

bool check_cmd(const char *cmd) {
    char buf[256];
    snprintf(buf, sizeof(buf), "command -v %s >/dev/null 2>&1", cmd);
    return system(buf) == 0;
}

void show_history() {
    if (check_cmd("cliphist")) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "cliphist list | head -n %d", MAX_ITEMS);
        system(cmd);
    } else if (check_cmd("wl-paste")) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "wl-paste --list 2>/dev/null | head -n %d", MAX_ITEMS);
        system(cmd);
    } else {
        printf("Clipboard history not available\nInstall: cliphist (recommended) or wl-clipboard\n");
        exit(1);
    }
}

void select_and_paste() {
    if (check_cmd("cliphist")) {
        const char *launcher = NULL;
        char cmd[512];
        
        if (check_cmd("fuzzel")) {
            launcher = "fuzzel -d -p 'Clipboard> '";
        } else if (check_cmd("rofi")) {
            launcher = "rofi -dmenu -p 'Clipboard'";
        } else if (check_cmd("wofi")) {
            launcher = "wofi --dmenu -p 'Clipboard'";
        } else if (check_cmd("fzf")) {
            launcher = "fzf --prompt='Clipboard> '";
        } else {
            show_history();
            exit(0);
        }

        snprintf(cmd, sizeof(cmd), 
            "selected=$(cliphist list | %s); "
            "if [ -n \"$selected\" ]; then "
            "  echo \"$selected\" | cliphist decode | wl-copy; "
            "  echo \"%s✓%s Copied to clipboard\"; "
            "fi", launcher, GREEN, NC);
        system(cmd);
    } else {
        printf("cliphist not installed. Cannot select history.\n");
    }
}

void clear_history() {
    if (check_cmd("cliphist")) {
        system("cliphist delete-all");
        pass("Clipboard history cleared");
    } else if (check_cmd("wl-copy")) {
        system("wl-copy -c");
        pass("Clipboard cleared");
    }
}

void copy_text(int argc, char *argv[]) {
    if (!check_cmd("wl-copy")) return;
    
    char text[4096] = {0};
    for (int i = 2; i < argc; i++) {
        strncat(text, argv[i], sizeof(text) - strlen(text) - 1);
        if (i < argc - 1) strncat(text, " ", sizeof(text) - strlen(text) - 1);
    }
    
    if (strlen(text) > 0) {
        pid_t pid = fork();
        if (pid == 0) {
            execlp("wl-copy", "wl-copy", text, NULL);
            _exit(1);
        } else if (pid > 0) {
            int status;
            waitpid(pid, &status, 0);
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
                printf("%s✓%s Copied: %s\n", GREEN, NC, text);
        }
    }
}

void paste_text() {
    if (check_cmd("wl-paste")) {
        pid_t pid = fork();
        if (pid == 0) {
            execlp("wl-paste", "wl-paste", NULL);
            _exit(1);
        } else if (pid > 0) {
            int status;
            waitpid(pid, &status, 0);
        }
    }
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    umask(0077);
    const char *mode = "show";
    if (argc > 1) mode = argv[1];
    
    if (strcmp(mode, "show") == 0 || strcmp(mode, "list") == 0 || strcmp(mode, "history") == 0) {
        show_history();
    } else if (strcmp(mode, "pick") == 0 || strcmp(mode, "select") == 0 || strcmp(mode, "rofi") == 0) {
        select_and_paste();
    } else if (strcmp(mode, "clear") == 0 || strcmp(mode, "delete") == 0) {
        clear_history();
    } else if (strcmp(mode, "copy") == 0) {
        copy_text(argc, argv);
    } else if (strcmp(mode, "paste") == 0) {
        paste_text();
    } else {
        printf("\nClipboard Manager\n\nUsage: %s <command>\n\nCommands:\n", argv[0]);
        printf("  show       Show clipboard history\n");
        printf("  pick       Select from history and paste (via fuzzel/rofi)\n");
        printf("  clear      Clear clipboard history\n");
        printf("  copy TEXT  Copy text to clipboard\n");
        printf("  paste      Paste from clipboard\n\n");
    }
    
    return 0;
}
