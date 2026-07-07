#ifndef OCWS_AUDIO_H
#define OCWS_AUDIO_H

#include <stdio.h>
#include <string.h>

/*
 * audio_get_volume — get current volume percentage for a sink
 *
 * @param sink  PulseAudio sink name (e.g. "@DEFAULT_SINK@")
 * @return volume 0-150, or -1 on failure
 */
static inline int audio_get_volume(const char *sink) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
        "pactl get-sink-volume %s 2>/dev/null | grep -oP '\\d+%%' | head -1 | tr -d '%%'",
        sink);
    FILE *f = popen(cmd, "r");
    if (!f) return -1;
    int vol = -1;
    fscanf(f, "%d", &vol);
    pclose(f);
    return vol;
}

/*
 * audio_is_muted — check if a sink is muted
 *
 * @param sink  PulseAudio sink name
 * @return 1 if muted, 0 if not, -1 on error
 */
static inline int audio_is_muted(const char *sink) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "pactl get-sink-mute %s 2>/dev/null", sink);
    FILE *f = popen(cmd, "r");
    if (!f) return -1;
    char buf[64] = {0};
    fgets(buf, sizeof(buf), f);
    pclose(f);
    return strstr(buf, "yes") != NULL ? 1 : 0;
}

/*
 * audio_set_volume — set volume percentage for a sink
 *
 * @param sink  PulseAudio sink name
 * @param pct   Volume percentage (0-150)
 * @return 0 on success, -1 on failure
 */
static inline int audio_set_volume(const char *sink, int pct) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "pactl set-sink-volume %s %d%% 2>/dev/null", sink, pct);
    return system(cmd);
}

/*
 * audio_toggle_mute — toggle mute on a sink
 *
 * @param sink  PulseAudio sink name
 * @return 0 on success, -1 on failure
 */
static inline int audio_toggle_mute(const char *sink) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "pactl set-sink-mute %s toggle 2>/dev/null", sink);
    return system(cmd);
}

/*
 * audio_set_default_sink — set the default audio sink
 *
 * @param name  Sink name to set as default
 * @return 0 on success, -1 on failure
 */
static inline int audio_set_default_sink(const char *name) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "pactl set-default-sink %s 2>/dev/null", name);
    return system(cmd);
}

/*
 * audio_list_sinks — list all available sinks
 */
static inline void audio_list_sinks(void) {
    system("pactl list sinks short 2>/dev/null");
}

/*
 * audio_get_icon — get icon name for volume level
 *
 * @param vol    Volume percentage
 * @param muted  Whether muted
 * @return static icon name string
 */
static inline const char* audio_get_icon(int vol, int muted) {
    if (muted || vol == 0) return "audio-volume-muted-symbolic";
    if (vol < 33) return "audio-volume-low-symbolic";
    if (vol < 66) return "audio-volume-medium-symbolic";
    return "audio-volume-high-symbolic";
}

#endif
