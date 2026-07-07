#ifndef OCWS_SYSFS_H
#define OCWS_SYSFS_H

#include <stdio.h>
#include <string.h>
#include <dirent.h>

static inline int sysfs_read_int(const char *path, int default_val) {
    FILE *f = fopen(path, "r");
    if (!f) return default_val;
    int val = default_val;
    fscanf(f, "%d", &val);
    fclose(f);
    return val;
}

static inline int sysfs_write_int(const char *path, int val) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "%d\n", val);
    fclose(f);
    return 0;
}

/*
 * sysfs_find_device — find first device in a sysfs class directory
 *
 * Scans /sys/class/<class_dir>/ for the first non-dot entry whose
 * <filename> attribute exists and is readable.
 *
 * @param class_dir  e.g. "backlight", "power_supply", "net", "rfkill"
 * @param filename   attribute to read, e.g. "brightness", "capacity"
 * @param name_out   buffer to receive the device name
 * @param name_len   size of name_out buffer
 * @return 0 on success, -1 on failure
 */
static inline int sysfs_find_device(const char *class_dir, const char *filename,
                                    char *name_out, size_t name_len) {
    char base[256];
    snprintf(base, sizeof(base), "/sys/class/%s", class_dir);

    DIR *d = opendir(base);
    if (!d) return -1;

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        char path[512];
        snprintf(path, sizeof(path), "%s/%s/%s", base, ent->d_name, filename);
        FILE *f = fopen(path, "r");
        if (f) {
            fclose(f);
            strncpy(name_out, ent->d_name, name_len - 1);
            name_out[name_len - 1] = '\0';
            closedir(d);
            return 0;
        }
    }
    closedir(d);
    return -1;
}

/*
 * sysfs_read_device_int — read an integer attribute from a sysfs device
 *
 * If device is NULL, auto-discovers via sysfs_find_device().
 *
 * @param class_dir  e.g. "backlight"
 * @param device     device name (NULL = auto-detect)
 * @param attr       attribute file (e.g. "brightness", "max_brightness")
 * @param default_val value to return on failure
 * @return the integer value, or default_val on failure
 */
static inline int sysfs_read_device_int(const char *class_dir, const char *device,
                                        const char *attr, int default_val) {
    char dev_buf[64];
    if (!device || device[0] == '\0') {
        if (sysfs_find_device(class_dir, attr, dev_buf, sizeof(dev_buf)) != 0)
            return default_val;
        device = dev_buf;
    }

    char path[512];
    snprintf(path, sizeof(path), "/sys/class/%s/%s/%s", class_dir, device, attr);
    return sysfs_read_int(path, default_val);
}

/*
 * sysfs_write_device_int — write an integer attribute to a sysfs device
 *
 * If device is NULL, auto-discovers via sysfs_find_device().
 *
 * @param class_dir  e.g. "backlight"
 * @param device     device name (NULL = auto-detect)
 * @param attr       attribute file (e.g. "brightness")
 * @param val        value to write
 * @return 0 on success, -1 on failure
 */
static inline int sysfs_write_device_int(const char *class_dir, const char *device,
                                         const char *attr, int val) {
    char dev_buf[64];
    if (!device || device[0] == '\0') {
        if (sysfs_find_device(class_dir, attr, dev_buf, sizeof(dev_buf)) != 0)
            return -1;
        device = dev_buf;
    }

    char path[512];
    snprintf(path, sizeof(path), "/sys/class/%s/%s/%s", class_dir, device, attr);
    return sysfs_write_int(path, val);
}

#endif
