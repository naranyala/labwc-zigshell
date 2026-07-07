#ifndef OCWS_EASING_H
#define OCWS_EASING_H

#include <math.h>
#include <unistd.h>

static inline double ease_out_cubic(double t) {
    t--;
    return t * t * t + 1.0;
}

static inline double ease_in_out_cubic(double t) {
    if (t < 0.5) {
        return 4.0 * t * t * t;
    } else {
        t = 2.0 * t - 2.0;
        return 0.5 * t * t * t + 1.0;
    }
}

static inline double ease_in_out(double t) {
    return t < 0.5 ? 2.0 * t * t : 1.0 - pow(-2.0 * t + 2.0, 2) / 2.0;
}

/*
 * animate_int — smooth integer animation with easing
 *
 * Interpolates from `start` to `target` over `duration_ms`,
 * calling `apply(value, ctx)` at each step.
 *
 * @param start      Starting integer value
 * @param target     Target integer value
 * @param duration_ms Animation duration in milliseconds
 * @param step_ms    Sleep between steps (8=brightness, 10=volume)
 * @param clamp_min  Minimum value clamp
 * @param clamp_max  Maximum value clamp
 * @param apply      Callback: apply(value, ctx) — called each step
 * @param ctx        User data passed to apply callback
 */
static inline void animate_int(int start, int target, int duration_ms, int step_ms,
                               int clamp_min, int clamp_max,
                               void (*apply)(int value, void *ctx), void *ctx) {
    if (start == target) return;
    if (duration_ms <= 0) {
        int v = target < clamp_min ? clamp_min : (target > clamp_max ? clamp_max : target);
        apply(v, ctx);
        return;
    }

    int steps = duration_ms / step_ms;
    if (steps < 1) steps = 1;

    double sv = (double)start;
    double ev = (double)target;

    for (int i = 1; i <= steps; i++) {
        double t = (double)i / steps;
        double eased = ease_out_cubic(t);
        int val = (int)(sv + (ev - sv) * eased + 0.5);
        if (val < clamp_min) val = clamp_min;
        if (val > clamp_max) val = clamp_max;
        apply(val, ctx);
        usleep(step_ms * 1000);
    }

    int final_v = target < clamp_min ? clamp_min : (target > clamp_max ? clamp_max : target);
    apply(final_v, ctx);
}

#endif
