#ifndef OCWS_PROCFS_H
#define OCWS_PROCFS_H

#include <stdio.h>
#include <string.h>

typedef struct {
    long user, nice, system, idle, iowait, irq, softirq, steal;
    long total;
} ProcCpu;

typedef struct {
    long total, free, available, buffers, cached, swap_total, swap_free;
} ProcMem;

typedef struct {
    char name[64];
    unsigned long rx_bytes, tx_bytes;
} ProcNetDev;

static inline int proc_cpu_read(ProcCpu *cpu) {
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return -1;
    int n = fscanf(f, "cpu %ld %ld %ld %ld %ld %ld %ld %ld",
        &cpu->user, &cpu->nice, &cpu->system, &cpu->idle,
        &cpu->iowait, &cpu->irq, &cpu->softirq, &cpu->steal);
    fclose(f);
    if (n < 4) return -1;
    cpu->total = cpu->user + cpu->nice + cpu->system + cpu->idle +
                 cpu->iowait + cpu->irq + cpu->softirq + cpu->steal;
    return 0;
}

static inline int proc_mem_read(ProcMem *mem) {
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return -1;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "MemTotal: %ld kB", &mem->total) == 1) continue;
        if (sscanf(line, "MemFree: %ld kB", &mem->free) == 1) continue;
        if (sscanf(line, "MemAvailable: %ld kB", &mem->available) == 1) continue;
        if (sscanf(line, "Buffers: %ld kB", &mem->buffers) == 1) continue;
        if (sscanf(line, "Cached: %ld kB", &mem->cached) == 1) continue;
        if (sscanf(line, "SwapTotal: %ld kB", &mem->swap_total) == 1) continue;
        if (sscanf(line, "SwapFree: %ld kB", &mem->swap_free) == 1) continue;
    }
    fclose(f);
    return 0;
}

static inline int proc_net_dev_read(const char *iface, ProcNetDev *dev) {
    FILE *f = fopen("/proc/net/dev", "r");
    if (!f) return -1;
    char line[512];
    fgets(line, sizeof(line), f); /* skip header */
    fgets(line, sizeof(line), f); /* skip header */
    while (fgets(line, sizeof(line), f)) {
        char name[64];
        unsigned long rx, tx;
        if (sscanf(line, " %63[^:]: %lu %*u %*u %*u %*u %*u %*u %*u %lu",
                   name, &rx, &tx) >= 2) {
            if (strcmp(name, iface) == 0) {
                strncpy(dev->name, name, sizeof(dev->name) - 1);
                dev->rx_bytes = rx;
                dev->tx_bytes = tx;
                fclose(f);
                return 0;
            }
        }
    }
    fclose(f);
    return -1;
}

static inline double proc_cpu_usage(ProcCpu *prev, ProcCpu *cur) {
    long total_d = cur->total - prev->total;
    long idle_d = cur->idle - prev->idle;
    if (total_d == 0) return 0.0;
    return ((double)(total_d - idle_d) / total_d) * 100.0;
}

static inline long proc_mem_used_pct(ProcMem *mem) {
    if (mem->total == 0) return 0;
    return ((mem->total - mem->available) * 100) / mem->total;
}

#endif
