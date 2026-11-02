#pragma once
void r13_scene(int x,int y,int z,int yaw,int pitch);
void r13_capture_before_present(void);
/* minimap-shot.txt containing 1 captures one framebuffer three seconds after the first in-game frame. */
void r13_note_ingame_frame(void);
void r13_draw(unsigned low,unsigned high,unsigned count);

int r14_selected_begin(unsigned count);
void r14_selected_vertex(const void *vertex);
void r14_selected_end(void);

void r13_request_snapshot(void);

void r13_request_snapshot_token(int token);
