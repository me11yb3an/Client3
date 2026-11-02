#include "active_save.h"

int active_save_due(ActiveSaveClock *clock, int64_t now_ms, int64_t interval_ms, int64_t max_gap_ms) {
    int64_t delta;
    if (!clock || interval_ms <= 0) return 0;
    if (!clock->started) {
        clock->started = 1;
        clock->last_ms = now_ms;
        clock->accumulated_ms = 0;
        return 0;
    }
    delta = now_ms - clock->last_ms;
    clock->last_ms = now_ms;
    if (delta < 0 || delta > max_gap_ms) delta = 0;
    clock->accumulated_ms += delta;
    if (clock->accumulated_ms < interval_ms) return 0;
    clock->accumulated_ms -= interval_ms;
    return 1;
}
