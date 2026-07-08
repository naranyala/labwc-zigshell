#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/stat.h>

const char* map_namespace(const char* ns) {
    if (strcmp(ns, "System.Volume") == 0) return "XVolLevel";
    if (strcmp(ns, "System.VolumeMuted") == 0) return "XVolMuted";
    if (strcmp(ns, "System.Brightness") == 0) return "XBrightness";
    if (strcmp(ns, "System.Battery") == 0) return "XBatLvl";
    if (strcmp(ns, "System.BatteryState") == 0) return "XBatStat";
    if (strcmp(ns, "System.Cpu") == 0) return "XCpuLoad";
    if (strcmp(ns, "System.Memory") == 0) return "XMemPct";
    if (strcmp(ns, "System.Disk") == 0) return "XDiskPct";
    if (strcmp(ns, "System.DND") == 0) return "XDndState";
    if (strcmp(ns, "Network.WiFi") == 0) return "XNetState";
    if (strcmp(ns, "Network.Bluetooth") == 0) return "XBtState";
    if (strcmp(ns, "Media.Title") == 0) return "XMediaTitle";
    if (strcmp(ns, "Media.Artist") == 0) return "XMediaArtist";
    if (strcmp(ns, "Media.Status") == 0) return "XMediaStatus";
    return ns;
}

int is_numeric(const char* val) {
    int i = 0;
    int has_dot = 0;
    int has_digit = 0;
    
    // Optional leading minus sign
    if (val[0] == '-') i++;
    
    for (; val[i] != '\0'; i++) {
        if (isdigit(val[i])) {
            has_digit = 1;
        } else if (val[i] == '.' && !has_dot) {
            has_dot = 1;
        } else {
            return 0; // Not a number
        }
    }
    return has_digit;
}

void print_help(const char* prog) {
    printf("Usage: %s <variable_namespace> <value...>\n", prog);
    printf("Example: %s System.Volume 75\n", prog);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    umask(0077);
    if (argc < 3) {
        print_help(argv[0]);
        return 1;
    }

    const char* ns = argv[1];
    const char* engine_var = map_namespace(ns);
    
    // Concatenate all remaining arguments into a single value string (space separated)
    char val[4096] = {0};
    int offset = 0;
    for (int i = 2; i < argc; i++) {
        int len = snprintf(val + offset, sizeof(val) - offset, "%s%s", i > 2 ? " " : "", argv[i]);
        if (len > 0) {
            offset += len;
        }
        if (offset >= sizeof(val)) break;
    }

    char ipc_cmd[8192];
    if (is_numeric(val)) {
        snprintf(ipc_cmd, sizeof(ipc_cmd), "SetVal %s = %s", engine_var, val);
    } else {
        // Escape quotes
        char escaped_val[4096] = {0};
        int j = 0;
        for (int i = 0; val[i] != '\0' && j < sizeof(escaped_val) - 2; i++) {
            if (val[i] == '"') {
                escaped_val[j++] = '\\';
            }
            escaped_val[j++] = val[i];
        }
        snprintf(ipc_cmd, sizeof(ipc_cmd), "SetVal %s = \"%s\"", engine_var, escaped_val);
    }

    // Fork and run sfwbar -R
    int pid = fork();
    if (pid == 0) {
        // Redirect stderr to /dev/null
        freopen("/dev/null", "w", stderr);
        execlp("sfwbar", "sfwbar", "-R", ipc_cmd, NULL);
        exit(1);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            printf("OCWS Event Emitted: %s -> %s\n", ns, val);
            return 0;
        } else {
            fprintf(stderr, "Failed to connect to OCWS engine (sfwbar might not be running).\n");
            return 1;
        }
    } else {
        fprintf(stderr, "Fork failed\n");
        return 1;
    }
}
