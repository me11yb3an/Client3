#pragma once
#include <stdint.h>
/* Four-fifths visual time for map-authored looping scenery. Carry fractional
 * logic tick per instance so low FPS and variable scene deltas keep the rate. */
static int r106_scenery_delta(int delta,int ambient,unsigned*remainder){
 if(!ambient)return delta;
 if(delta<=0)return 0;
 uint64_t ticks=(uint64_t)(unsigned)delta*4u+(*remainder%5u);
 *remainder=(unsigned)(ticks%5u);return (int)(ticks/5u);
}
/* Original water is100 rows/second. Use80percent of the rate with a bounded integer
 * phase; static water and animated fountain meshes use the same clock. */
static float r106_water_offset(uint64_t now_ms){
 const uint64_t period=1600u;
 return -(float)(now_ms%period)/(float)period;
}
