#ifndef BA_TIMING_H
#define BA_TIMING_H
#include "ba_hw_r5.h"
#include "ba_hw_r6.h"
#include "ba_hw_r7.h"
#include "ba_hw_r5_build.h"
#ifdef BUILD_A_EMBEDDED
#include <stdint.h>
enum { BA_LOGIC, BA_RENDER, BA_PRESENT, BA_SERVER_PUMP, BA_FLAME_STEP, BA_TEXTURE, BA_SCENE_DELTA, BA_WORLD_RASTER, BA_ENTITY_PREP, BA_SCENE_CONSUMED, BA_ENTITY_STEP, BA_DISPLAY_ACK, BA_DISPLAY_MISMATCH, BA_DISPLAY_WAIT_FAIL, BA_DISPLAY_GET_FAIL, BA_DISPLAY_SET_FAIL, BA_PIXEL_CONVERT, BA_PIXEL_UPLOAD, BA_RESOURCE_LOOKUP, BA_RESOURCE_READ, BA_MODEL, BA_INPUT, BA_SERVER_JS, BA_SERVER_NPC, BA_PATHFIND, BA_COLLISION, BA_PACKET, BA_GC, BA_BZIP, BA_SAVE, BA_SERVER_TICK, BA_UI_COMPOSE, BA_COMPASS, BA_GL_SUBMIT, BA_STORAGE_FLUSH, BA_SCENE_BUILD, BA_TIMING_COUNT };
uint64_t ba_timing_now(void);
void ba_timing_record(unsigned stage,uint64_t elapsed);
void ba_timing_flush(void);
void ba_timing_flush_final(void);
void ba_profile_gl_limits(int maximum,int low_memory);
int ba_profile_detail(void);
void ba_profile_render(void);
void ba_profile_scene(int radius,int x,int z,int yaw,int pitch,int vertices,int indices);
typedef struct {unsigned stage;uint64_t begin;} BaProfileScope;
static inline void ba_profile_scope_end(BaProfileScope*s){if(s->begin)ba_timing_record(s->stage,ba_timing_now()-s->begin);}
#define BA_PROFILE_SCOPE(stage) BaProfileScope ba_scope __attribute__((cleanup(ba_profile_scope_end)))={(stage),ba_profile_detail()?ba_timing_now():0}
#define BA_PROFILE_ALWAYS(stage) BaProfileScope ba_scope __attribute__((cleanup(ba_profile_scope_end)))={(stage),ba_timing_now()}

#else
#define BA_PROFILE_SCOPE(stage) ((void)0)
#define BA_PROFILE_ALWAYS(stage) ((void)0)
#define ba_timing_now() 0
#define ba_timing_record(stage,elapsed) ((void)0)
#define ba_timing_flush() ((void)0)
#define ba_timing_flush_final() ((void)0)
#endif
#endif
