#pragma once
#include <stdint.h>
#include <stddef.h>
void h8_init(void);
void h8_shutdown(void);
void h8_game_step(void*);
void h8_present(void);
void h8_stage(unsigned,uint64_t);
void h8_scope(unsigned,uint64_t,uint64_t);
void h8_scene(int,int,int,int,int);
int h8_is_running(void);
int h8_is_armed(void);
unsigned h8_filter_buttons(unsigned physical);
const char*h8_boot_path(const char*path,char*out,size_t cap);
/* Game-thread adapter: only fixed commands, no worker access to Client/QuickJS. */
int h8_take_run(unsigned*kind,unsigned*seconds,unsigned*preset);
void h8_begin(int x,int z,int yaw,int pitch,int radius,int lowmem);
int h8_action(unsigned *kind,unsigned *step);
void h8_action_result(unsigned kind,unsigned step,int x,int z,int accepted);
void h8_end_tick(int x,int z,int yaw,int pitch,int radius,int ready);
int h8_local_override(void);
