/* Scoped native geometry, derived from PacketStream.method1792 / Class181.method2532.
 * No sprites, packets, scripts, player state or title dependencies. */
#include "layoutcore.h"
#include "circular_minimap_geometry.h"
extern bool ve_minimap_screen_covers(int x,int y);
#include <limits.h>
#include <string.h>
static int32_t bits(uint32_t n){return n<=INT32_MAX?(int32_t)n:(int32_t)((int64_t)n-4294967296LL);}
static int32_t add(int32_t a,int32_t b){return bits((uint32_t)a+(uint32_t)b);}
static int32_t sub(int32_t a,int32_t b){return bits((uint32_t)a-(uint32_t)b);}
static int32_t mul(int32_t a,int32_t b){return bits((uint32_t)a*(uint32_t)b);}
static int32_t divj(int32_t a,int32_t b){return a==INT32_MIN&&b==-1?INT32_MIN:a/b;}
static int32_t q14(int32_t a,int32_t b){int32_t n=mul(a,b);return n>=0?n/16384:(int32_t)(-((-(int64_t)n+16383)/16384));}
static int32_t axis(int32_t parent,int32_t raw,int32_t size,int mode){
 int32_t gap=sub(parent,size),q=q14(parent,raw);
 switch(mode){case 0:return raw;case 1:return add(raw,gap/2);case 2:return sub(gap,raw);case 3:return q;case 4:return add(q,gap/2);default:return sub(gap,q);}
}
static int32_t dimension(int parent,int raw,int previous,int mode,int type,int gap,bool horizontal){
 switch(mode){case 0:return raw;case 1:return sub(parent,raw);case 2:return q14(parent,raw);case 3:if(type==2||type==7)return add(mul(type==2?32:horizontal?115:12,raw),mul(gap,sub(raw,1)));return previous;default:return previous;}
}
static LcRect intersection(LcRect a,LcRect b){
 int64_t x=a.x>b.x?a.x:b.x,y=a.y>b.y?a.y:b.y;
 int64_t r=(int64_t)a.x+a.w<(int64_t)b.x+b.w?(int64_t)a.x+a.w:(int64_t)b.x+b.w;
 int64_t d=(int64_t)a.y+a.h<(int64_t)b.y+b.h?(int64_t)a.y+a.h:(int64_t)b.y+b.h;
 return (LcRect){(int)x,(int)y,(int)(r>x?r-x:0),(int)(d>y?d-y:0)};
}
bool lc_resolve(const LcConstraints*c,LcRect p,LcRect clip,LcGeometry*g){
 if(!c||!g)return false;
 LcGeometry next=*g;next.size_changed=next.position_changed=false;
 if(p.w<0||p.h<0||c->x_mode<0||c->x_mode>5||c->y_mode<0||c->y_mode>5||c->w_mode<0||c->w_mode>4||c->h_mode<0||c->h_mode>4||(c->w_mode==4&&c->aspect_h==0)||(c->h_mode==4&&c->aspect_w==0)){
  g->valid=g->visible=g->actionable=false;return false;
 }
 int32_t w=dimension(p.w,c->raw_w,g->rect.w,c->w_mode,c->type,c->gap_x,true);
 int32_t h=dimension(p.h,c->raw_h,g->rect.h,c->h_mode,c->type,c->gap_y,false);
 if(c->w_mode==4)w=divj(mul(c->aspect_w,h),c->aspect_h);
 if(c->h_mode==4)h=divj(mul(w,c->aspect_h),c->aspect_w);
 next.rect=(LcRect){add(p.x,axis(p.w,c->raw_x,w,c->x_mode)),add(p.y,axis(p.h,c->raw_y,h,c->y_mode)),w,h};
 next.clip=intersection(next.rect,clip);
 next.scroll_x=c->scroll_x;next.scroll_y=c->scroll_y;
 int mx=c->scroll_w>w?sub(c->scroll_w,w):0,my=c->scroll_h>h?sub(c->scroll_h,h):0;
 if(next.scroll_x>mx)next.scroll_x=mx;if(next.scroll_x<0)next.scroll_x=0;
 if(next.scroll_y>my)next.scroll_y=my;if(next.scroll_y<0)next.scroll_y=0;
 next.size_changed=w!=g->rect.w||h!=g->rect.h;
 next.position_changed=next.rect.x!=g->rect.x||next.rect.y!=g->rect.y;
 next.valid=true;next.visible=!c->hidden&&w>0&&h>0&&next.clip.w>0&&next.clip.h>0;
 next.actionable=next.visible&&!c->disabled&&!c->decorative;
 if(!g->valid||memcmp(&next.rect,&g->rect,sizeof(LcRect))||memcmp(&next.clip,&g->clip,sizeof(LcRect))||next.visible!=g->visible||next.actionable!=g->actionable||next.scroll_x!=g->scroll_x||next.scroll_y!=g->scroll_y)next.generation=g->generation+1;
 *g=next;return true;
}
bool lc_contains(const LcGeometry*g,int x,int y,bool action){return g&&g->valid&&g->visible&&(!action||g->actionable)&&x>=g->clip.x&&y>=g->clip.y&&(int64_t)x<(int64_t)g->clip.x+g->clip.w&&(int64_t)y<(int64_t)g->clip.y+g->clip.h;}
static bool chat_hidden;
static struct {int bw,bh,pw,ph,profile;bool ready;unsigned generation;LcConstraints constraints[LC_ROLES];LcGeometry geometry[LC_ROLES];LcRect baseline[LC_ROLES];} game;
void lc_game_init(int w,int h,bool native){
 if(game.ready)return;game.ready=true;game.bw=w;game.bh=h;
 int shift=native?171:0,bottom=native?12:0;
 game.baseline[LC_MINIMAP]=(LcRect){561+shift,0,228,165};
 game.baseline[LC_SIDEBAR]=(LcRect){520+shift,165,269,h-165};
 game.baseline[LC_CHAT]=(LcRect){0,345+bottom,540+shift,h-345-bottom};
 for(int i=0;i<LC_ROLES;i++){
  LcRect b=game.baseline[i];LcConstraints*c=&game.constraints[i];
  c->raw_w=b.w;c->raw_h=b.h;c->aspect_w=c->aspect_h=1;
  c->x_mode=i==LC_CHAT?0:2;c->raw_x=c->x_mode==2?w-b.x-b.w:b.x;
  c->y_mode=i==LC_MINIMAP?0:2;c->raw_y=c->y_mode==2?h-b.y-b.h:b.y;
  if(i==LC_CHAT){c->w_mode=1;c->raw_w=w-b.w;}
 }
 lc_game_parent(w,h);
}
bool lc_game_parent(int w,int h){
 if(!game.ready||w<711||w>game.bw||h<400||h>game.bh)return false;
 if(game.pw==w&&game.ph==h)return true;
 game.pw=w;game.ph=h;game.generation++;
 /* Logical parent anchors; physical-screen clip remains960x544. No framebuffer resize. */
 for(int i=0;i<LC_ROLES;i++)lc_resolve(&game.constraints[i],(LcRect){0,0,w,h},(LcRect){0,0,game.bw,game.bh},&game.geometry[i]);
 if(game.profile==2&&chat_hidden){LcGeometry*g=&game.geometry[LC_CHAT];g->clip=intersection(g->clip,(LcRect){g->rect.x,g->rect.y+147,421,22});}
 /* Fixed-height original art can overlap at reduced diagnostic parents. The
  * sidebar is painted above the map; its covered pixels must not accept map input. */
 LcGeometry*m=&game.geometry[LC_MINIMAP];const LcGeometry*s=&game.geometry[LC_SIDEBAR];
 if(s->rect.x<m->rect.x+m->rect.w&&s->rect.x+s->rect.w>m->rect.x&&s->rect.y<m->clip.y+m->clip.h){
  int h=s->rect.y-m->clip.y;m->clip.h=h>0?h:0;m->visible=m->visible&&m->clip.h>0;m->actionable=m->actionable&&m->visible;
 }
 return true;
}
const LcGeometry *lc_game_geometry(int i){return i>=0&&i<LC_ROLES?&game.geometry[i]:0;}
int lc_dx(int i){return game.ready?game.geometry[i].rect.x-game.baseline[i].x:0;}
int lc_dy(int i){return game.ready?game.geometry[i].rect.y-game.baseline[i].y:0;}
int lc_x(int i,int x){return x+lc_dx(i);}
int lc_y(int i,int y){return y+lc_dy(i);}
bool lc_game_covers(int x,int y){if(!game.ready)return false;for(int i=0;i<LC_ROLES;i++)if(lc_contains(&game.geometry[i],x,y,false)){if(i==LC_MINIMAP&&game.profile==2){if(ve_minimap_screen_covers(x,y))return true;}else return true;}return false;}
unsigned lc_game_generation(void){return game.generation;}
bool lc_game_altered(void){return game.ready&&(game.pw!=game.bw||game.ph!=game.bh);}
int lc_chat_text_right(int original){if(!game.ready)return original;int right=game.geometry[LC_CHAT].clip.w-38;return right<original?right:original;}

void lc_game_profile(int profile){
 int next=profile==2?2:0;if(!game.ready||game.profile==next)return;game.profile=next;
 for(int i=0;i<LC_ROLES;i++){
  /* Approved Option B placement. Render the original assembly as one unit at
   * 223x178: integer output dimensions for the requested 102.3% scale. */
  LcRect b=game.baseline[i];if(next==2){if(i==LC_MINIMAP)b=(LcRect){game.bw-231,9,VE_FRAME_W188,VE_FRAME_H184};else if(i==LC_SIDEBAR)b=(LcRect){game.bw-255,game.bh-349,249,343};else b=(LcRect){6,game.bh-176,519,170};}
  LcConstraints*c=&game.constraints[i];c->raw_w=b.w;c->raw_h=b.h;c->w_mode=0;c->x_mode=i==LC_CHAT?0:2;c->raw_x=c->x_mode==2?game.bw-b.x-b.w:b.x;c->y_mode=i==LC_MINIMAP?0:2;c->raw_y=c->y_mode==2?game.bh-b.y-b.h:b.y;
  if(i==LC_CHAT&&next==0){c->w_mode=1;c->raw_w=game.bw-b.w;}
 }
 int w=game.pw,h=game.ph;game.pw=game.ph=0;lc_game_parent(w,h);
}

void lc_game_chat_wide(int wide){if(!game.ready||game.profile!=2)return;(void)wide;int width=519;if(game.constraints[LC_CHAT].raw_w==width)return;game.constraints[LC_CHAT].raw_w=width;int w=game.pw,h=game.ph;game.pw=game.ph=0;lc_game_parent(w,h);}

void lc_game_chat_hidden(bool hidden){if(!game.ready||chat_hidden==hidden)return;chat_hidden=hidden;int w=game.pw,h=game.ph;game.pw=game.ph=0;lc_game_parent(w,h);}
