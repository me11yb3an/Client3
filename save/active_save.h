#ifndef HW_R17G_ACTIVE_SAVE_H
#define HW_R17G_ACTIVE_SAVE_H

#include <stdint.h>

/* 60 seconds of active play. A single gap larger than the max is treated as
 * suspend or a stalled process and does not count toward the interval.
 * World ticks are 600 ms, so a 2000 ms ceiling still accepts a slow tick.
 */
#define ACTIVE_SAVE_INTERVAL_MS 60000
#define ACTIVE_SAVE_MAX_GAP_MS 2000

typedef struct ActiveSaveClock {
    int64_t last_ms;
    int64_t accumulated_ms;
    int started;
} ActiveSaveClock;

/* Returns 1 when a checkpoint is due. The first sample only stores `now_ms`. */
int active_save_due(ActiveSaveClock *clock, int64_t now_ms, int64_t interval_ms, int64_t max_gap_ms);

#endif
