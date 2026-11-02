#pragma once
#include <stdint.h>
/* Passive counters only. The existing optional observer reports them; no
 * per-effect file writes or HUD changes occur on the playback path. */
typedef struct {uint64_t calls,total_ms,max_ms;} R94AudioCost;
static R94AudioCost r94_audio_cost[2];
static void r94_audio_record(int stage,uint64_t elapsed){
    R94AudioCost*c=&r94_audio_cost[stage];c->calls++;c->total_ms+=elapsed;
    if(elapsed>c->max_ms)c->max_ms=elapsed;
}
