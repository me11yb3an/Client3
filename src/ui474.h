#pragma once
#include "client.h"
void ui474_begin(Client *c);
bool ui474_enabled(void);
int ui474_side_x(void);
int ui474_side_y(void);
void ui474_draw(Client *c);
void ui474_draw_sidebar(Client *c);
bool ui474_pointer(Client *c,int x,int y,int button);
bool ui474_menu(Client *c,int x,int y);
bool ui474_controller(Client *c,bool confirm,int direction,bool pointer_activity);
bool ui474_select_profile(Client *c,int profile);
int ui474_player_sequence(int source);
void client_ui474_invoke(Client*c,int action,int a,int b,int component);
void client_ui474_draw_interface(Client*c,Component*root,int x,int y,int scroll);
int ui474_focus_target(void);
int ui474_focus_kind(void);
int ui474_scroll_position(void);

Pix8 *ui474_original_sprite(int id);
void ui474_parchment_chat(Client*c);
void ui474_parchment_draw(Client*c);

bool ve_enabled(void);

bool ui474_energy_patch_ready(Client*c);
bool ui474_energy_patch(Client*c);
