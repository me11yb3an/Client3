/* Profile-local recovered474paper. No outer frame, shared sprite or font changes. */
#include "ui474.h"
#include "vita_enhanced.h"
#include "layoutcore.h"
#include "layoutcore_draw.h"
#include "pix2d.h"
#include "pix24.h"
#include "pixmap.h"
#include <assert.h>
static PixMap*paper;
static Pix24*chat;
static int source_x(int x){if(x<20)return x;if(x>=691)return 519-(711-x);int n=(x-20)%958;return 20+(n<479?n:957-n);}
static int source_y(int y){if(y<24)return y;if(y>=104)return 142-(126-y);return 24+(y-24)%96;}
static void prepare(void){
 if(paper)return;
 Pix8*s=ui474_original_sprite(1017);assert(s&&s->crop_w==519&&s->crop_h==142);
 paper=pixmap_new(711,126);chat=pix24_new(650,96,false);
 for(int y=0;y<126;y++)for(int x=0;x<711;x++){
  int sx=source_x(x)-s->crop_x,sy=source_y(y)-s->crop_y;
  assert(sx>=0&&sy>=0&&sx<s->width&&sy<s->height);
  paper->pixels[y*711+x]=s->palette[(unsigned char)s->pixels[sy*s->width+sx]];
 }
 for(int y=0;y<96;y++)for(int x=0;x<650;x++)chat->pixels[y*650+x]=paper->pixels[(y+30)*711+x+22];
}
void ui474_parchment_chat(Client*c){
 if(ve_enabled()){pixmap_bind(c->area_chatback);pix2d_fill_rect(0,0,0xff00ff,650,96);return;}
 if(!ui474_enabled()){pix8_draw(c->image_chatback,0,0);return;}
 prepare();pixmap_bind(c->area_chatback);pix24_blit_opaque(chat,0,0);
}
void ui474_parchment_draw(Client*c){
 if(ve_enabled()){ve_chat_draw(c);return;}
 if(!ui474_enabled())return;
 prepare();lc_draw(paper,LC_CHAT,lc_x(LC_CHAT,0),lc_y(LC_CHAT,357));lc_draw(c->area_chatback,LC_CHAT,lc_x(LC_CHAT,22),lc_y(LC_CHAT,387));
}
