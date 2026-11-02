#pragma once
/* The supplied layout places a small tab outside the upper-right rim.
 * The existing 488x188 keyboard art is sampled unchanged at 100% or 125%.
 * Rendering and hit testing use the same integral destination rectangle. */
typedef struct {int x,y,w,h,tab_x,tab_y,tab_w,tab_h;} R63KeyboardBox;
static R63KeyboardBox r63_keyboard_box(int screen_w,int screen_h,int ingame,int large){
 R63KeyboardBox b;
 b.w=large?610:488;b.h=large?235:188;
 b.x=ingame?20:(screen_w-b.w)/2;
 b.y=ingame?10:screen_h-b.h-4;
 b.tab_x=b.x+b.w;b.tab_y=b.y+(large?6:4);
 b.tab_w=44;b.tab_h=36;
 return b;
}
static int r63_keyboard_tab(R63KeyboardBox b,int x,int y){
 return x>=b.tab_x&&x<b.tab_x+b.tab_w&&y>=b.tab_y&&y<b.tab_y+b.tab_h;
}
static int r63_keyboard_key(R63KeyboardBox b,int x,int y,int*row,int*col){
 if(x<b.x||y<b.y||x>=b.x+b.w||y>=b.y+b.h)return 0;
 /* Map the drawn rim and cell edges back into the original art. */
 int px=(x-b.x)*488/b.w-4,py=(y-b.y)*188/b.h-4;
 if(px<0||py<0||px>=480||py>=180)return 0;
 *row=py/36;*col=px/48;return 1;
}
