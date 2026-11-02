#pragma once
#include <stdint.h>
int h4_world_enabled(void);
void h4_poll(void);
void h4_init(void);
void h4_record(unsigned stage,uint64_t us);
void h4_scope(unsigned stage,uint64_t wall,uint64_t exclusive,unsigned cpu_valid,uint64_t cpu,uint64_t exclusive_cpu);
void h4_sample(unsigned frames,unsigned ticks);
void h4_graph(unsigned kind,uint64_t value);
void h4_world_enter(void);
void h4_geometry(unsigned vertices,unsigned indices,unsigned vertex_capacity,unsigned index_capacity,unsigned grew);
void h4_context(int radius,int camera_x,int camera_y,int camera_z,int yaw,int pitch,int player_x,int player_z,int ready);
void h4_summary(uint64_t now,unsigned serial);
void h4_hud_lines(char lines[3][128]);
