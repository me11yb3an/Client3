#pragma once

#include <stdint.h>

/* Static world mesh for the Vita. The 3 fps hardware frames were the CPU
 * painter walk: every model was projected and sorted, then transformed again
 * into a fresh vertex buffer. Depth testing orders opaque faces. Model-face
 * alpha is a later draw, with depth writes off, so a translucent pane cannot
 * hide the exterior or the coplanar post. Texture cutout stays in the opaque
 * draw. The uploaded square stays in world space. It is reused while the camera
 * eye's draw radius remains inside it, and rebuilt when the player walks
 * out of that margin, the roof level changes, or the scene is edited.
 * Players, NPCs, projectiles, animated models and scrolling water stay dynamic.
 * Create ux0:/data/RS2Vita/r16-off.txt to force the original walk. */
void r16_set_scene_region(int world_z);
void r16_set_anchor(int tile_x, int tile_z);
void r16_set_roof_focus(const int8_t flags[4][104][104],int level,int tile_x,int tile_z);
void r16_roof_status(int status[8]);
void r16_opening_status(int status[3]);
unsigned r16_underground_cuts(void);
unsigned r16_ship_rail_faces(void);
unsigned r16_crane_duplicates(void);
unsigned r16_fixture_contacts(void);
unsigned r16_hull_layers(void);
int r16_player_visual_offset(const void *entity);

int r16_active(void);
int r16_fallback_reason(void);
int r16_baking(void);
int r16_emitting(void);
int r16_suppress_emit(void);
int r16_freeze_scroll(void);
void r16_dirty(void);
int r16_live_door(int bitset);
int r16_live_location(int bitset);
int r16_location_changed(int bitset);
unsigned r16_skill_updates(void);
void r16_grounditem_changed(int x,int z);
unsigned r16_item_updates(void);
int r16_door_changed(int bitset);
unsigned r16_door_updates(void);
int r16_prepare(void *world, int top_level, int loop_cycle);
int r16_has_static(void);
int r16_fast_frame(void);
void r16_draw_static(void);
void r16_draw_static_alpha(void);
void r16_draw_water(void);
/* Scenery whose model pointer and vertex signature are unchanged is drawn
 * from a retained buffer. Create dyn-cache.txt containing 1 before launch.
 * Players and NPCs stay on the per-frame path. Absent file keeps every
 * dynamic model on that path. */
void r16_draw_dynamic(void);
/* kind: 0 ui-hold, 1 draw-distance, 2 bake log, 3 guest-fps, 4 gpu-memory,
 * 5 phase log, 6 hud texture, 7 dyn log. */
void r16_io_add(int kind, uint64_t us);
int r16_diag_verbose(void);
void r16_dyn_flush(void);

unsigned r16_bake_count(void);
unsigned r16_roof_swaps(void);

unsigned r16_body_layer_faces(void);
void r16_draw_body_layers(void);

void r16_crop_status(int out[4]);
