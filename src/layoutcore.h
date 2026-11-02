#ifndef LAYOUTCORE_H
#define LAYOUTCORE_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {int32_t x,y,w,h;} LcRect;
typedef struct {
 int32_t raw_x,raw_y,raw_w,raw_h;
 int x_mode,y_mode,w_mode,h_mode,type,gap_x,gap_y,aspect_w,aspect_h;
 int scroll_w,scroll_h,scroll_x,scroll_y;
 bool hidden,disabled,decorative;
} LcConstraints;
typedef struct {
 LcRect rect,clip;int scroll_x,scroll_y;unsigned generation;
 bool valid,visible,actionable,size_changed,position_changed;
} LcGeometry;
bool lc_resolve(const LcConstraints*,LcRect parent,LcRect parent_clip,LcGeometry*);
bool lc_contains(const LcGeometry*,int x,int y,bool action);
enum {LC_MINIMAP,LC_SIDEBAR,LC_CHAT,LC_ROLES};
void lc_game_init(int width,int height,bool native);
bool lc_game_parent(int width,int height);
const LcGeometry *lc_game_geometry(int role);
int lc_x(int role,int baseline_x);
int lc_y(int role,int baseline_y);
int lc_dx(int role);
int lc_dy(int role);
bool lc_game_covers(int x,int y);
unsigned lc_game_generation(void);
bool lc_game_altered(void);
int lc_chat_text_right(int original);
#endif

void lc_game_profile(int profile);

void lc_game_chat_wide(int wide);

void lc_game_chat_hidden(bool hidden);
