#pragma once
#include "client.h"
#include "pixfont.h"
int classic_repeat_axis(int position, int source, int target, int cap);
void classic_extend_pix8(Pix8 *image, int width, int height, int cap_x, int cap_y);
void classic_extend_tab_backing(Pix8 *image, int height);
void classic_extend_pix24(Pix24 *image, int width, int height, int cap_x, int cap_y);
#define CLASSIC_CHAT_CAPACITY (CHAT_LENGTH + USERNAME_LENGTH + 24)
typedef struct { int start, end; } ClassicChatLine;
int classic_chat_wrap(const char *text, PixFont *font, int width, ClassicChatLine *lines, int capacity);
int classic_chat_rows(Client *c, int index, PixFont *font);
void classic_draw_chat(Client *c, PixFont *font);
void classic_draw_chat_input(PixFont *font, int x, int y, const char *text, int color);

void classic_extend_chat_controls(Pix8 *image);

void classic_draw_title_logo(Pix24 *logo, int y);

Pix24 *classic_wide_title_background(Pix24 *original);

void classic_extend_chat_border(Pix24 *image, int width);

Pix24 *classic_load_restored_title(void);

void classic_draw_title_logo_scaled(Pix24 *logo, int y);

void classic_repair_lower_corner(Pix8 *corner, const Pix8 *neighbor);
