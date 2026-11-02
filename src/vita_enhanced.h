#pragma once
#include "client.h"
#include "pixmap.h"
#if defined(__vita__) && defined(VITA_NATIVE_WIDESCREEN)
#include "vita_wide_layout.h"
#endif
bool ve_enabled(void);
/* The 960-wide view keeps the original focal length of 512, so it shows
 * 960/683 of the classic ground. ve_screen maps a classic offset onto that
 * screen. ve_pointer maps a screen point back into classic space. */
static inline int ve_screen(int offset) {
#if defined(__vita__) && defined(VITA_NATIVE_WIDESCREEN)
    if (ve_enabled()) return (int)(((long long)offset * SCREEN_FB_WIDTH) / GAME_VIEW_WIDTH);
#endif
    return offset;
}
static inline int ve_pointer(int center, int screen) {
#if defined(__vita__) && defined(VITA_NATIVE_WIDESCREEN)
    if (ve_enabled()) return center + (int)(((long long)(screen - center) * GAME_VIEW_WIDTH) / SCREEN_FB_WIDTH);
#endif
    (void)center;
    return screen;
}
void ve_prepare(Client*c);
void ve_release(Client*c);
int ve_side_x(void);
int ve_side_y(void);
int ve_chat_text_x(void);
int ve_chat_text_y(void);
bool ve_world_input(int x,int y);
void ve_side_backdrop(Client*c,PixMap*area);
void ve_side_caps(PixMap*area);
void ve_side_finish(Client*c,PixMap*area);
void ve_chat_draw(Client*c);
/* Caller has cleared the complete chat rectangle to transparent. */
void ve_chat_draw_clean(Client*c);
void ve_map_draw(Client*c);
/* Yaw must not call glTexSubImage2D on the viewport texture. That call is a
 * fixed stall. The map is a GPU fan over a texture uploaded when the 512
 * image is rebuilt. */
void ve_map_gl_prepare(Client *c);
void ve_map_gl_prepare_cold(Client *c);
void ve_map_gl_draw(Client *c);
void ve_map_gl_hole_reset(void);
void ve_map_gl_invalidate(void);
/* HUD text is a small texture. The viewport texture is punched once and is
 * not uploaded again when the fps line changes. */
void ve_hud_gl_reset(void);
void ve_hud_gl_prepare(Client *c);
void ve_hud_gl_update(Client *c, char lines[3][128]);
void ve_hud_gl_draw(void);
/* Leave chat, sidebar and the minimap in place and clear only the world. */
void ve_clear_world(Client *c);
/* True while this frame is reusing the composed interface pixmap. */
int ve_overlay_held(void);

void ve_draw_chat(Client*c,PixFont*font);
void ve_draw_chat_input(PixFont*font,int x,int y,const char*text,int color);

void ve_art(PixMap*dst,int id,int x,int y);

bool ve_map_point(int x,int y);
bool ve_map_input_position(int screen_x,int screen_y,int *map_x,int *map_y);

bool ve_chat_hidden(Client*c);
void ve_chat_toggle(Client*c);
int ve_chat_button_at(int x,int y);

void ve_privacy_save(Client*c);
void ve_privacy_restore(Client*c);

void ve_bottom_stones(PixMap*dst,bool run_on,int selected_tab);
bool ve_run_base_point(int x,int y);

bool ve_minimap_screen_covers(int x,int y);

/* Native world markers compose under the retained interface panels. */
void ve_world_sprite(Pix24 *image, int x, int y);

void ve_hud_draw_metrics(int ms,int vertices,int indices);
void ve_hud_scene_metrics(int world_ms,int pixmap_ms);

void ve_world_rect(int x,int y,int w,int h,int rgb);
