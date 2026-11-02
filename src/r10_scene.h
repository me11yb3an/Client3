#ifndef R10_SCENE_H
#define R10_SCENE_H
#include <stdint.h>
#include "r16_fast.h"
#ifndef BA_R10_CACHE
#define BA_R10_CACHE 0
#endif
struct Model;
void r10_upload_flush(void);
#if BA_R10_CACHE
void r10_frame_begin(void);
void r10_reset(void);
void r10_shutdown(void);
void r10_flush(void);
void r10_dirty_rect(int x0,int z0,int x1,int z1);
void r10_model_changed(struct Model *m);
void r10_model_changed_mask(struct Model *m,unsigned mask);
void r10_hint(int eligible);
void r10_animation_update(int active);
void r10_model_indices(int start);
void r10_emit_time(uint64_t elapsed);
void r10_model_work(uint64_t project_us,uint64_t faces_us,unsigned vertices,unsigned selected);
int r10_model_hit(struct Model *m,int x,int y,int z,int yaw);
void r10_model_store(struct Model *m,int x,int y,int z,int yaw,int base);
int r10_surface_begin(const void *key,int kind,int x,int z,int level,int animated);
void r10_surface_end(void);
void r10_draw(unsigned dynamic_vbo,int use_vbo);
#else
static inline void r10_frame_begin(void){}
static inline void r10_reset(void){r16_dirty();}
static inline void r10_shutdown(void){}
static inline void r10_flush(void){}
static inline void r10_dirty_rect(int a,int b,int c,int d){(void)a;(void)b;(void)c;(void)d;r16_dirty();}
static inline void r10_model_changed(struct Model*m){(void)m;}
static inline void r10_model_changed_mask(struct Model*m,unsigned a){(void)m;(void)a;}
static inline void r10_hint(int a){(void)a;}
static inline void r10_animation_update(int a){(void)a;}
static inline void r10_model_indices(int a){(void)a;}
static inline void r10_emit_time(uint64_t a){(void)a;}
static inline void r10_model_work(uint64_t a,uint64_t b,unsigned c,unsigned d){(void)a;(void)b;(void)c;(void)d;}
static inline int r10_model_hit(struct Model*m,int x,int y,int z,int a){(void)m;(void)x;(void)y;(void)z;(void)a;return 0;}
static inline void r10_model_store(struct Model*m,int x,int y,int z,int a,int b){(void)m;(void)x;(void)y;(void)z;(void)a;(void)b;}
static inline int r10_surface_begin(const void*k,int a,int x,int z,int l,int d){(void)k;(void)a;(void)x;(void)z;(void)l;(void)d;return 0;}
static inline void r10_surface_end(void){}
#endif
#endif
