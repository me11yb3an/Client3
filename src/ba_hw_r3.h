#pragma once
#include <stdint.h>
#include "ba_hw_r4.h"
enum { H3_FRAME,H3_INPUT,H3_LOGIC,H3_SERVER,H3_RENDER,H3_PRESENT,H3_DIAGNOSTICS,H3_SLEEP,
 H3_ENTITY,H3_WORLD,H3_POST_WORLD,H3_CHAT,H3_MINIMAP,H3_UI,H3_OVERLAY,
 H3_CONVERT,H3_UPLOAD,H3_SUBMIT,H3_SWAP,H3_ALLOC,H3_BUFFER,H3_SHADER,H3_COUNT };
typedef struct H3Scope {uint64_t wall,cpu,child_wall,child_cpu;unsigned stage,active,cpu_ok;struct H3Scope*parent;} H3Scope;
H3Scope h3_enter(unsigned stage);
void h3_link(H3Scope *scope);
void h3_leave(H3Scope *scope);
void h3_frame_begin(void);
void h3_summary(uint64_t now,unsigned serial);
int h3_mode(void);
void h3_init(void);
#define H3_SCOPE(stage) H3Scope h3_scope __attribute__((cleanup(h3_leave)))=h3_enter(stage);h3_link(&h3_scope)
