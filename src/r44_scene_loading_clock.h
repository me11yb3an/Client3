#pragma once
#include <stdint.h>
/* Match the existing client scene-loading rule when retained scenery is
 * constructed during rendering. This owns only the client's next animation/
 * input update deadline; server clocks and measured frame times are untouched. */
static int r44_finish_scene_loading(uint64_t now,uint64_t draw_us,unsigned before,unsigned after,int ready,int tick_ms,uint64_t*client_due){
 if(!ready||before==after||draw_us<250000u||tick_ms<=0)return 0;
 uint64_t resume=now+(uint64_t)tick_ms*1000u;
 if(*client_due>=resume)return 0;
 *client_due=resume;return 1;
}
