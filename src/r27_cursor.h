#pragma once
#include <stdint.h>
typedef struct {int width,height,hotspot_x,hotspot_y;const uint32_t*pixels;} R27Cursor;
int r27_cursor_choice(void);
int r27_cursor_cycle(void);
const R27Cursor*r27_cursor_at(int choice);
void r27_cursor_preview(int x,int y,int width,int height);
