#include "ui_preview.h"
#include "ba_timing.h"
#include "vita_map_aperture.generated.h"
#include "circular_minimap_geometry.h"
#include "offline_paths.h"
/* CheckpointA: original-asset floating panels over the actual expanded native world. */
#include "vita_enhanced.h"
#include "layoutcore.h"
#include "pix3d.h"
#include "pix2d.h"
#include "world3d.h"
#include "pixfont.h"
#include "gl11.h"
#include "draw_phases.h"
#include "r16_fast.h"
#include "datastruct/jstring.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
extern Pix3D _Pix3D;
typedef struct{int id,w,h;unsigned*p;} Art;
static Art art[17];static bool loaded;
static PixMap*full,*classic;static int*full_offsets,*classic_offsets;static bool using_full;
static unsigned read32(FILE*f){unsigned v=0;for(int i=0;i<4;i++){int b=fgetc(f);assert(b>=0);v=(v<<8)|(unsigned)b;}return v;}
static int read16(FILE*f){int a=fgetc(f),b=fgetc(f);assert(a>=0&&b>=0);return a*256+b;}
static void preview_thin_art(Art*a){
 if(!ui_thin_preview())return;
 /* Supplied map and stone PNGs retain their own artwork and alpha. */
 if(a->id==1019||a->id==1024)ui_one_pixel_border(a->p,a->w,0,0,a->w,a->h);
}
static void load(void){if(loaded)return;FILE*f=fopen("rom/vita-enhanced.bin","rb");assert(f);assert(read32(f)==0x56454131&&read16(f)==17);for(int i=0;i<17;i++){Art*a=&art[i];a->id=read16(f);a->w=read16(f);a->h=read16(f);assert(a->w>0&&a->w<=1024&&a->h>0&&a->h<=1024);a->p=calloc((size_t)a->w*a->h,sizeof(unsigned));assert(a->p);for(int j=0;j<a->w*a->h;j++)a->p[j]=read32(f);preview_thin_art(a);}assert(fgetc(f)==EOF);fclose(f);
 for(int i=0;i<17;i++)if(art[i].id==2490||art[i].id==2491){
  Art*a=&art[i];free(a->p);a->w=VE_FRAME_W188;a->h=VE_FRAME_H184;a->p=calloc((size_t)a->w*a->h,sizeof(unsigned));assert(a->p);
  for(int j=0;j<a->w*a->h;j++)a->p[j]=a->id==2490?ve_frame_argb[j]:(ve_map_aperture[j]?0xff000000u:0);
 }
 loaded=true;}
static Art*get(int id){load();for(int i=0;i<17;i++)if(art[i].id==id)return &art[i];abort();}
static int blend(int dst,unsigned src,int alpha){
#ifdef GL11
 // The GPU overlay's clear marker is transparent, not a white backdrop.
 // Preserve straight alpha until the final GPU composition over the world.
 unsigned sa=(src>>24)*(unsigned)alpha/256u;
 if(sa>=255u)return (int)(src&0xffffffu);
 if(!sa)return dst;
 if((unsigned)dst==0xffffffffu)return (int)((src&0xffffffu)|(sa<<24));
 unsigned da=(unsigned)dst>>24;
 if((unsigned)dst==0xffffffffu || (da>0 && da<255)){
  if((unsigned)dst==0xffffffffu)da=0;
  unsigned outa=sa+da*(255u-sa)/255u;if(!outa)return -1;
  unsigned rgb=0;
  for(int shift=0;shift<=16;shift+=8){
   unsigned channel=(((src>>shift)&255u)*sa+((unsigned)dst>>shift&255u)*da*(255u-sa)/255u)/outa;
   rgb|=channel<<shift;
  }
  return (int)(rgb|(outa<255?outa<<24:0));
 }
#endif
int a=(int)(src>>24)*alpha/255,b=256-a;return ((((src&0xff00ff)*a+(unsigned)(dst&0xff00ff)*b)&0xff00ff00)+(((src&0x00ff00)*a+(unsigned)(dst&0x00ff00)*b)&0x00ff0000))>>8;}
static void picture(PixMap*dst,Art*a,int x,int y,int alpha){for(int yy=0;yy<a->h;yy++)for(int xx=0;xx<a->w;xx++){int dx=x+xx,dy=y+yy;if(dx>=0&&dy>=0&&dx<dst->width&&dy<dst->height){unsigned s=a->p[yy*a->w+xx];if(ui_water_preview()&&(a->id==1033||a->id==1034)&&xx==a->w-1&&(s>>24)&&(s&0xffffffu)<=1)s=a->p[yy*a->w+xx-1];if(ui_water_preview()&&((a->id==1034&&yy<121&&xx<8)||(a->id==1033&&yy>=12&&xx<11)))continue;if(s>>24)dst->pixels[dy*dst->width+dx]=blend(dst->pixels[dy*dst->width+dx],s,alpha);}}}
/* Permanent two-pixel inner contour on the original chat stones only.
 * Follow their alpha silhouette without extending artwork or input bounds. */
static void chat_stone_outline(PixMap*dst,Art*a,int x,int y){
 for(int yy=0;yy<a->h;yy++)for(int xx=0;xx<a->w;xx++){
  if((a->p[yy*a->w+xx]>>24)<128)continue;
  bool edge=false;
  for(int step=1;step<=(ui_thin_preview()?1:2)&&!edge;step++){
   edge=xx<step||yy<step||xx>=a->w-step||yy>=a->h-step;
   if(!edge)edge=(a->p[yy*a->w+xx-step]>>24)<128||(a->p[yy*a->w+xx+step]>>24)<128||(a->p[(yy-step)*a->w+xx]>>24)<128||(a->p[(yy+step)*a->w+xx]>>24)<128;
  }
  int dx=x+xx,dy=y+yy;if(edge&&dx>=0&&dy>=0&&dx<dst->width&&dy<dst->height)dst->pixels[dy*dst->width+dx]=0;
 }
}
void ve_art(PixMap*dst,int id,int x,int y){picture(dst,get(id),x,y,256);}
/* Base silhouettes define the socket; red artwork keeps its existing transform. */
bool ve_run_base_point(int x,int y){Art*a=get(2032);return x>=0&&x<42&&y>=0&&y<36&&(a->p[y*a->w+x]>>24)>=128;}
void ve_bottom_stones(PixMap*dst,bool run_on,int selected_tab){
 Art*a=get(2032);for(int y=0;y<a->h;y++)for(int x=0;x<a->w;x++){
  if(y<36){
   if(run_on&&x<42)continue;
   int selected_x=selected_tab==8?42:selected_tab==9?76:selected_tab>=10&&selected_tab<=13?110+(selected_tab-10)*33:-1;
   int selected_w=selected_tab==8||selected_tab==9?34:selected_tab==13?40:33;
   if(selected_x>=0&&x>=selected_x&&x<selected_x+selected_w)continue;
  }
  unsigned color=a->p[y*a->w+x];if(color>>24)dst->pixels[(y+306)*dst->width+x]=blend(dst->pixels[(y+306)*dst->width+x],color,256);
 }
}
int ve_side_x(void){return lc_game_geometry(LC_SIDEBAR)->rect.x;}
int ve_side_y(void){return lc_game_geometry(LC_SIDEBAR)->rect.y;}
int ve_chat_text_x(void){return lc_game_geometry(LC_CHAT)->rect.x+8;}
int ve_chat_text_y(void){return lc_game_geometry(LC_CHAT)->rect.y+34;}
bool ve_world_input(int x,int y){return x>=0&&y>=0&&x<960&&y<544&&!lc_game_covers(x,y);}
void ve_prepare(Client*c){
 bool active=ve_enabled();if(active!=using_full){
  if(active){classic=c->area_viewport;classic_offsets=c->area_viewport_offsets;if(!full){full=pixmap_new(960,544);full_offsets=calloc(544,sizeof(int));assert(full_offsets);for(int y=0;y<544;y++)full_offsets[y]=960*y;}c->area_viewport=full;c->area_viewport_offsets=full_offsets;}
  else{c->area_viewport=classic;c->area_viewport_offsets=classic_offsets;}
  using_full=active;int distances[9];for(int i=0;i<9;i++){int a=i*32+143;distances[i]=(a*3+600)*_Pix3D.sin_table[a]>>16;}
  world3d_init(active?960:683,active?544:346,500,800,distances);c->redraw_background=true;
 }
 if(c->area_viewport){pixmap_bind(c->area_viewport);_Pix3D.line_offset=c->area_viewport_offsets;_Pix3D.center_x=c->area_viewport->width/2;_Pix3D.center_y=c->area_viewport->height/2;_Pix3D.renderWidth=c->area_viewport->width;_Pix3D.renderHeight=c->area_viewport->height;}
}
void ve_release(Client*c){if(!full)return;if(using_full){c->area_viewport=classic;c->area_viewport_offsets=classic_offsets;}pixmap_free(full);free(full_offsets);full=classic=NULL;full_offsets=classic_offsets=NULL;using_full=false;}
/* Thicken only each column's existing outside black edge inward by one pixel. */
static void column_outer_outline(PixMap*dst,Art*a,int x,int y,bool left){
 for(int yy=0;yy<a->h;yy++){
  int edge=left?((ui_water_preview()&&a->id==1034&&yy<121)?8:(ui_water_preview()&&a->id==1033&&yy>=12)?11:0):a->w-1,step=left?1:-1;
  while(edge>=0&&edge<a->w&&(a->p[yy*a->w+edge]>>24)<128)edge+=step;
  if(edge<0||edge>=a->w||(a->p[yy*a->w+edge]&0xffffffu)>1)continue;
  for(int n=0;n<(ui_thin_preview()?1:2);n++){int sx=edge-n*step,dx=x+sx,dy=y+yy;if(sx>=0&&sx<a->w&&dx>=0&&dx<dst->width&&dy>=0&&dy<dst->height)dst->pixels[dy*dst->width+dx]=0;}
 }
}
static void joint_backing(PixMap*area,int first,int last,int left,int right,int source_y){Art*a=get(1031);for(int y=first;y<last;y++)for(int x=left;x<right;x++){int sx=12+(x+144)%166,sy=source_y+(y-first);area->pixels[y*area->width+x]=a->p[sy*a->w+sx]&0xffffff;}}
void ve_side_backdrop(Client*c,PixMap*area){int x=ve_side_x(),y=ve_side_y();for(int yy=0;yy<area->height;yy++)memcpy(area->pixels+yy*area->width,c->area_viewport->pixels+(y+yy)*960+x,(size_t)area->width*sizeof(int));
 /* Original panel texture behind the joints, within the housing footprint.
  * Capitals, pillars, their dark outlines and the tab sockets draw over it. */
 joint_backing(area,41,45,11,243,3);joint_backing(area,45,52,31,221,3);joint_backing(area,300,313,11,241,240);
 picture(area,get(1031),31,45,256);picture(area,get(1033),0,45,256);picture(area,get(1034),3,178,256);picture(area,get(1035),221,45,256);column_outer_outline(area,get(1033),0,45,true);column_outer_outline(area,get(1034),3,178,true);column_outer_outline(area,get(1035),221,45,false);}
/* Original left-foot colors and the matching mirrored right-foot silhouette.
 * The masks differ only at the nine rejected spur pixels; no art is invented. */
static bool foot_mask(Art*right,int x,int y){return x>=0&&x<28&&y>=0&&y<128&&(right->p[(right->h-128+y)*right->w+27-x]>>24);}
void ve_side_caps(PixMap*area){
 Art*a=get(1034),*right=get(1035);
 /* Exact opposite-foot silhouette and outside-border construction.
  * Keep original left stone colors; no added inner rim or bottom stripe. */
 for(int yy=108;yy<128;yy++){
  int edge=0;while(edge<28&&!foot_mask(right,edge,yy))edge++;
  for(int xx=0;xx<28;xx++)if(foot_mask(right,xx,yy)){
   unsigned color=a->p[yy*a->w+xx];
   if(xx==27&&(color&0xffffffu)<=1)color=a->p[yy*a->w+26];
   int at=(178+yy)*area->width+3+xx;area->pixels[at]=blend(area->pixels[at],color,256);
  }
  /* Same extra outside pixel as the harp-side column, except one corner pixel. */
  if(edge>0&&edge<28&&yy!=127&&!ui_thin_preview())area->pixels[(178+yy)*area->width+3+edge-1]=0;
 }
}

void ve_side_finish(Client*c,PixMap*area){
#ifdef GL11
 /* Clear stray overlay pixels outside the straight shaft. The full flared
  * foot begins at299 and remains untouched; GPU world renders underneath. */
 if(ui_water_preview())for(int yy=57;yy<299;yy++)for(int xx=0;xx<10;xx++)area->pixels[yy*area->width+xx]=(int)0xffffffffu;
#endif
 /* Single continuous inside shaft line matching the inventory-facing right edge. */
 for(int yy=52;yy<299;yy++)area->pixels[yy*area->width+30]=0;
 int x=ve_side_x(),y=ve_side_y();for(int yy=0;yy<area->height;yy++)memcpy(c->area_viewport->pixels+(y+yy)*960+x,area->pixels+yy*area->width,(size_t)area->width*sizeof(int));}
static void privacy_path(Client*c,char*path,size_t size){
 char name[64];size_t n=0;for(const unsigned char*p=(const unsigned char*)c->username;*p&&n<sizeof(name)-1;p++){unsigned char ch=*p;if(ch>='A'&&ch<='Z')ch+=32;name[n++]=(ch>='a'&&ch<='z')||(ch>='0'&&ch<='9')?ch:'_';}name[n]=0;snprintf(path,size,BA_PREFERENCE("chat-modes-%s.txt"),name);
}
void ve_privacy_save(Client*c){if(!ve_enabled())return;char path[160];privacy_path(c,path,sizeof(path));FILE*f=fopen(path,"w");if(f){fprintf(f,"%d %d %d\n",c->public_chat_setting,c->private_chat_setting,c->trade_chat_setting);fclose(f);}}
void ve_privacy_restore(Client*c){
 if(!ve_enabled())return;char path[160];privacy_path(c,path,sizeof(path));FILE*f=fopen(path,"r");int a,b,d;bool valid=f&&fscanf(f,"%d%d%d",&a,&b,&d)==3&&a>=0&&a<4&&b>=0&&b<3&&d>=0&&d<3;if(f)fclose(f);
 if(valid){c->public_chat_setting=a;c->private_chat_setting=b;c->trade_chat_setting=d;p1isaac(c->out,244);p1(c->out,a);p1(c->out,b);p1(c->out,d);}
}
static bool chat_hidden_requested,chat_preference_loaded;
static const int chat_button_left[6]={62,124,186,310,0,248};
static const int chat_button_width[6]={56,56,56,111,56,56};
static void chat_preference(void){if(chat_preference_loaded)return;chat_preference_loaded=true;FILE*f=fopen(BA_PREFERENCE("ui-chat-hidden.txt"),"r");if(f){int n=0;if(fscanf(f,"%d",&n)==1)chat_hidden_requested=n==1;fclose(f);}}
bool ve_chat_hidden(Client*c){chat_preference();return ve_enabled()&&chat_hidden_requested&&c->chat_interface_id<0&&c->sticky_chat_interface_id<0&&!c->show_social_input&&!c->chatback_input_open&&!c->modal_message[0];}
void ve_chat_toggle(Client*c){chat_preference();chat_hidden_requested=!chat_hidden_requested;FILE*f=fopen(BA_PREFERENCE("ui-chat-hidden.txt"),"w");if(f){fprintf(f,"%d\n",chat_hidden_requested?1:0);fclose(f);}lc_game_chat_hidden(ve_chat_hidden(c));c->redraw_chatback=c->redraw_privacy_settings=true;}
int ve_chat_button_at(int x,int y){const LcRect*r=&lc_game_geometry(LC_CHAT)->rect;if(y<r->y+147||y>=r->y+169)return -1;for(int i=0;i<6;i++)if(x>=r->x+chat_button_left[i]&&x<r->x+chat_button_left[i]+chat_button_width[i])return i;return -1;}

static void ve_chat_content(Client*c){
 const LcRect*r=&lc_game_geometry(LC_CHAT)->rect;PixMap*dst=c->area_viewport;
 int tx=ve_chat_text_x(),ty=ve_chat_text_y();for(int yy=0;yy<c->area_chatback->height;yy++)for(int xx=0;xx<c->area_chatback->width&&xx<r->w-16;xx++){int p=c->area_chatback->pixels[yy*c->area_chatback->width+xx];if(p!=0xff00ff)dst->pixels[(ty+yy)*960+tx+xx]=p;}
}
static void ve_chat_draw_layers(Client*c,int content){
 const LcRect*r=&lc_game_geometry(LC_CHAT)->rect;PixMap*dst=c->area_viewport;Art*a=get(1017);
 if(!ve_chat_hidden(c)){
 static int chat_x[960],chat_w,chat_source_w;assert(r->w>0&&r->w<=960);
 if(chat_w!=r->w||chat_source_w!=a->w){chat_w=r->w;chat_source_w=a->w;for(int x=0;x<chat_w;x++)chat_x[x]=x<12?x:x>=chat_w-12?a->w-(chat_w-x):12+(x-12)%(a->w-24);}
 for(int yy=0;yy<142;yy++)for(int xx=0;xx<r->w;xx++){int sx=chat_x[xx];int at=(r->y+yy)*960+r->x+xx;dst->pixels[at]=blend(dst->pixels[at],a->p[yy*a->w+sx],256);}
 if(content)ve_chat_content(c);
  for(int yy=0;yy<142;yy++)for(int xx=0;xx<r->w;xx++)if(yy<(ui_thin_preview()?1:2)||yy>=142-(ui_thin_preview()?1:2)||xx<(ui_thin_preview()?1:2)||xx>=r->w-(ui_thin_preview()?1:2))dst->pixels[(r->y+yy)*dst->width+r->x+xx]=0;
 }
 int xp=c->varps[323];const char*xp_label=xp==1?"XP: x1.5":xp==2?"XP: x2":"XP: x1";
 pixmap_bind(dst);const char*names[]={"Public","Private","Trade","Report",chat_hidden_requested?"Show":"Hide",xp_label};int mode[]={c->public_chat_setting,c->private_chat_setting,c->trade_chat_setting};const int colors[]={0x00ff00,0xffff00,0xff0000,0x00ffff};
 for(int i=0;i<6;i++){int x=r->x+chat_button_left[i],y=r->y+147,w=chat_button_width[i];Art*b=get(i==3?1024:1019);picture(dst,b,x,y,256);chat_stone_outline(dst,b,x,y);drawStringTaggableCenter(c->font_plain11,names[i],x+w/2,y+14,i<3?colors[mode[i]]:i==5?(xp==1?0xffff00:xp==2?0xff0000:0x00ff00):0xffffff,true);}
}

void ve_chat_draw(Client*c){ve_chat_draw_layers(c,1);}
#include "r19_chat_background.inc"

static Pix24*compass;
/* Uniform visual transform; world-coordinate and movement packet math stay original. */
static int map_source_x(int sx){return 73+(sx-VE_MAP_CX)*100/VE_MAP_SCALE_PERCENT;}
static int map_source_y(int sy){return 74+(sy-VE_MAP_CY)*100/VE_MAP_SCALE_PERCENT;}
static bool map_frame_position(int x,int y,int*sx,int*sy){
 const LcRect*r=&lc_game_geometry(LC_MINIMAP)->rect;
 if(!r->w||!r->h||x<r->x||y<r->y||x>=r->x+r->w||y>=r->y+r->h)return false;
 *sx=(x-r->x)*VE_FRAME_W188/r->w;*sy=(y-r->y)*VE_FRAME_H184/r->h;return true;
}
bool ve_minimap_screen_covers(int x,int y){
 int sx,sy;if(!map_frame_position(x,y,&sx,&sy))return false;int pixel=sy*VE_FRAME_W188+sx;
 return (get(2490)->p[pixel]>>24)>=64||(get(2491)->p[pixel]>>24)||ve_compass_aperture[pixel];
}
bool ve_map_point(int x,int y){return x>=0&&y>=0&&x<146&&y<151;}
bool ve_map_input_position(int screen_x,int screen_y,int*map_x,int*map_y){
 int sx,sy;if(!map_frame_position(screen_x,screen_y,&sx,&sy))return false;
 int pixel=sy*VE_FRAME_W188+sx;
 if(!(get(2491)->p[pixel]>>24)||(get(2490)->p[pixel]>>24)>=128)return false;
 if(map_x)*map_x=map_source_x(sx);
 /* Original marker/icon raster center is y83; original input center is y75. */
 if(map_y)*map_y=map_source_y(sy)+1;return true;
}
void ve_clear_world(Client *c) {
    PixMap *dst = c->area_viewport;
    if (!dst || !dst->pixels || dst->width <= 0 || dst->height <= 0) return;
    const LcRect *panel[3] = {
        &lc_game_geometry(LC_CHAT)->rect,
        &lc_game_geometry(LC_SIDEBAR)->rect,
        &lc_game_geometry(LC_MINIMAP)->rect
    };
    for (int y = 0; y < dst->height; y++) {
        int left[4] = {0}, right[4] = {dst->width}, count = 1;
        for (int p = 0; p < 3; p++) {
            const LcRect *r = panel[p];
            if (!r || r->w <= 0 || r->h <= 0 || y < r->y || y >= r->y + r->h) continue;
            int next_l[8], next_r[8], next = 0;
            for (int s = 0; s < count; s++) {
                int cut_l = r->x > left[s] ? r->x : left[s];
                int cut_r = r->x + r->w < right[s] ? r->x + r->w : right[s];
                if (cut_l >= cut_r) {
                    next_l[next] = left[s];
                    next_r[next] = right[s];
                    next++;
                    continue;
                }
                if (left[s] < cut_l) {
                    next_l[next] = left[s];
                    next_r[next] = cut_l;
                    next++;
                }
                if (cut_r < right[s]) {
                    next_l[next] = cut_r;
                    next_r[next] = right[s];
                    next++;
                }
            }
            count = next;
            for (int s = 0; s < count && s < 4; s++) {
                left[s] = next_l[s];
                right[s] = next_r[s];
            }
            if (count > 4) count = 4;
        }
        uint32_t *row = (uint32_t *)dst->pixels + (size_t)y * dst->width;
        for (int s = 0; s < count; s++) {
            if (right[s] > left[s]) memset(row + left[s], 0xff, (size_t)(right[s] - left[s]) * sizeof(uint32_t));
        }
    }
}

void ve_map_draw(Client*c){
    BA_PROFILE_SCOPE(BA_COMPASS);
 const LcRect*r=&lc_game_geometry(LC_MINIMAP)->rect;PixMap*dst=c->area_viewport;Art*housing=get(2490),*mapmask=get(2491);
 assert(r->w>0&&r->w<=960&&r->h>0&&r->h<=544);
 static int sample_x[960],sample_y[544],source_x[VE_FRAME_W188],source_y[VE_FRAME_H184],cached_w,cached_h;
 if(cached_w!=r->w||cached_h!=r->h){cached_w=r->w;cached_h=r->h;
  for(int x=0;x<r->w;x++)sample_x[x]=x*housing->w/r->w;
  for(int y=0;y<r->h;y++)sample_y[y]=y*housing->h/r->h;
  for(int x=0;x<housing->w;x++)source_x[x]=map_source_x(x);
  for(int y=0;y<housing->h;y++)source_y[y]=map_source_y(y);
 }
 static unsigned preview_frames;static int frame_only;
 if(preview_frames%300==0){FILE*state=fopen("ux0:/data/RS2Vita/minimap-state.csv","a");if(state){fprintf(state,"%llu,%d,%d,%d,%d,%d,%d,%d\n",(unsigned long long)ba_timing_now(),c->sceneBaseTileX+c->local_player->pathing_entity.x/128,c->sceneBaseTileZ+c->local_player->pathing_entity.z/128,c->orbit_camera_yaw,r->x,r->y,r->w,r->h);fclose(state);}}
 if(preview_frames++%60==0){frame_only=0;FILE*f=fopen("ux0:/data/RS2Vita/minimap-frame-only.txt","r");if(f){fscanf(f,"%d",&frame_only);fclose(f);}}
 if(!compass){Art*a=get(169);compass=pix24_new(a->w,a->h,false);for(int i=0;i<a->w*a->h;i++)compass->pixels[i]=a->p[i]>>24?a->p[i]&0xffffff:0;}
 int co[37],cl[37];for(int y=0;y<37;y++){co[y]=0;cl[y]=37;}
 static int compass_pixels[37*37];PixMap scratch={0};scratch.width=37;scratch.height=37;scratch.pixels=compass_pixels;PixMap*compass_unit=&scratch;memset(compass_pixels,0,sizeof(compass_pixels));pixmap_bind(compass_unit);
 pix24_draw_rotated_masked(compass,0,0,37,37,co,cl,23,23,c->orbit_camera_yaw,226);
 for(int dy=0;dy<r->h;dy++){
  int sy=sample_y[dy],screen_y=r->y+dy;if(screen_y<0||screen_y>=dst->height)continue;
  for(int dx=0;dx<r->w;dx++){
   int sx=sample_x[dx],screen_x=r->x+dx;if(screen_x<0||screen_x>=dst->width)continue;int pixel=sy*housing->w+sx;
   if(!frame_only){
    int mx=source_x[sx],my=source_y[sy];
    if(mx>=0&&mx<146&&my>=0&&my<151&&(mapmask->p[pixel]>>24))dst->pixels[screen_y*dst->width+screen_x]=c->area_mapback->pixels[(my+9)*168+mx+21];
    int cx=(int)((sx-VE_COMPASS_CX)*37.f/32.f+18.5f),cy=(int)((sy-VE_COMPASS_CY)*37.f/31.f+18.5f);
    if(cx>=0&&cx<37&&cy>=0&&cy<37&&ve_compass_aperture[pixel])dst->pixels[screen_y*dst->width+screen_x]=compass_unit->pixels[cy*37+cx];
   }
   unsigned frame=housing->p[pixel];if(frame>>24)dst->pixels[screen_y*dst->width+screen_x]=blend(dst->pixels[screen_y*dst->width+screen_x],frame,256);
  }
 }
 pixmap_bind(dst);_Pix3D.line_offset=c->area_viewport_offsets;
}

#ifdef GL11
static int map_src_gen, map_uploaded_gen = -1, map_hole_ready, map_logged;
static GLuint map_tex, frame_tex, compass_tex;
static int map_tex_w, map_tex_h, frame_w, frame_h, compass_w, compass_h;
typedef struct { float x, y; } MapFan;
static MapFan map_fan[66];
static int map_fan_n, map_fan_w, map_fan_h;
typedef struct { const Pix24 *src; GLuint tex; } MapSprite;
static MapSprite map_sprites[48];
static int map_sprite_n;

void ve_map_gl_invalidate(void) { map_src_gen++; }
void ve_map_gl_hole_reset(void) { map_hole_ready = 0; }

/* mode 0: keep the source word (frame alpha). mode 1: pixel 0 is clear and
 * every other pixel is opaque. mode 2: every pixel is opaque (minimap image). */
static GLuint map_upload(const unsigned *argb, int w, int h, int mode) {
    unsigned *buf = malloc((size_t)w * (size_t)h * 4u);
    if (!buf) return 0;
    for (int i = 0; i < w * h; i++) {
        unsigned p = argb[i];
        if (mode == 1) buf[i] = p == 0u ? 0u : p | 0xff000000u;
        else if (mode == 2) buf[i] = p | 0xff000000u;
        else buf[i] = p;
    }
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA_EXT, w, h, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, buf);
    free(buf);
    return tex;
}

static void map_build_fan(void) {
    Art *mask = get(2491);
    map_fan_w = mask->w;
    map_fan_h = mask->h;
    double sx = 0, sy = 0;
    int n = 0;
    for (int y = 0; y < mask->h; y++) {
        for (int x = 0; x < mask->w; x++) {
            if (mask->p[y * mask->w + x] >> 24) { sx += x; sy += y; n++; }
        }
    }
    float cx = n ? (float)(sx / n) : 100.f, cy = n ? (float)(sy / n) : 95.f;
    map_fan[0].x = cx;
    map_fan[0].y = cy;
    const int seg = 64;
    for (int i = 0; i < seg; i++) {
        double a = (double)i / (double)seg * 6.283185307179586;
        float dx = (float)cos(a), dy = (float)sin(a), bx = cx, by = cy;
        for (float t = 1.f; t < 220.f; t += 1.f) {
            int ix = (int)(cx + dx * t + 0.5f), iy = (int)(cy + dy * t + 0.5f);
            if (ix < 0 || iy < 0 || ix >= mask->w || iy >= mask->h) break;
            if ((mask->p[iy * mask->w + ix] >> 24) == 0) break;
            bx = cx + dx * t;
            by = cy + dy * t;
        }
        map_fan[1 + i].x = bx;
        map_fan[1 + i].y = by;
    }
    map_fan_n = seg;
}

static void map_uv(float sx, float sy, int anchor_x, int anchor_y, int theta, int zoom, int tw, int th, float *u, float *v) {
    float mx = 73.f + (sx - VE_MAP_CX) * (100.f / 108.f);
    float my = 74.f + (sy - VE_MAP_CY) * (100.f / 108.f);
    double thv = (double)theta / 326.11;
    double s = sin(thv), c = cos(thv), z = (double)zoom / 256.0;
    double sz = s * z, cz = c * z;
    const double center_x = -73.0, center_y = -75.0;
    double src_x = (double)anchor_x + center_y * sz + center_x * cz + sz * (double)my + cz * (double)mx;
    double src_y = (double)anchor_y + center_y * cz - center_x * sz + cz * (double)my - sz * (double)mx;
    *u = (float)(src_x / (double)tw);
    *v = (float)(src_y / (double)th);
}

static void map_quad(float x, float y, float w, float h, float u0, float v0, float u1, float v1) {
    glBegin(GL_QUADS);
    glTexCoord2f(u0, v0); glVertex2f(x, y);
    glTexCoord2f(u0, v1); glVertex2f(x, y + h);
    glTexCoord2f(u1, v1); glVertex2f(x + w, y + h);
    glTexCoord2f(u1, v0); glVertex2f(x + w, y);
    glEnd();
}

static GLuint map_sprite(const Pix24 *p) {
    if (!p || !p->pixels || p->width <= 0 || p->height <= 0) return 0;
    for (int i = 0; i < map_sprite_n; i++) if (map_sprites[i].src == p) return map_sprites[i].tex;
    if (map_sprite_n >= 48) return 0;
    GLuint tex = map_upload((const unsigned *)p->pixels, p->width, p->height, 1);
    if (!tex) return 0;
    map_sprites[map_sprite_n].src = p;
    map_sprites[map_sprite_n].tex = tex;
    map_sprite_n++;
    return tex;
}

void ve_world_rect(int x,int y,int w,int h,int rgb){
    if(w<=0||h<=0)return;glDisable(GL_CULL_FACE);glDisable(GL_DEPTH_TEST);glDisable(GL_ALPHA_TEST);glDisable(GL_TEXTURE_2D);
    glColor4ub((rgb>>16)&255,(rgb>>8)&255,rgb&255,255);
    glBegin(GL_QUADS);glVertex2i(x,y);glVertex2i(x,y+h);glVertex2i(x+w,y+h);glVertex2i(x+w,y);glEnd();
    glEnable(GL_TEXTURE_2D);glEnable(GL_CULL_FACE);glColor4ub(255,255,255,255);
}

void ve_world_sprite(Pix24 *image, int x, int y) {
    GLuint tex = map_sprite(image);
    if (!tex) return;
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_ALPHA_TEST);
    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glColor4ub(255,255,255,255);
    glBindTexture(GL_TEXTURE_2D,tex);
    map_quad((float)(x+image->crop_x),(float)(y+image->crop_y),
        (float)image->width,(float)image->height,0.f,0.f,1.f,1.f);
    glEnable(GL_CULL_FACE);
}

static void map_clipped_icon(const LcRect*r,float left,float top,float width,float height) {
    float sx=r->w/(float)VE_FRAME_W188,sy=r->h/(float)VE_FRAME_H184;
    glBegin(GL_QUADS);
    for(int y=(int)floorf(top);y<(int)ceilf(top+height);y++) {
        if(y<0||y>=VE_FRAME_H184)continue;
        float x0=fmaxf(left,ve_map_spans[y*2]),x1=fminf(left+width,ve_map_spans[y*2+1]);
        float y0=fmaxf(top,y),y1=fminf(top+height,y+1);
        if(x1<=x0||y1<=y0)continue;
        float u0=(x0-left)/width,u1=(x1-left)/width,v0=(y0-top)/height,v1=(y1-top)/height;
        glTexCoord2f(u0,v0);glVertex2f(r->x+x0*sx,r->y+y0*sy);
        glTexCoord2f(u0,v1);glVertex2f(r->x+x0*sx,r->y+y1*sy);
        glTexCoord2f(u1,v1);glVertex2f(r->x+x1*sx,r->y+y1*sy);
        glTexCoord2f(u1,v0);glVertex2f(r->x+x1*sx,r->y+y0*sy);
    }
    glEnd();
}

static void map_icon(Client *c, const LcRect *r, Pix24 *image, int dx, int dy) {
    if (!image || !_Pix3D.sin_table || !_Pix3D.cos_table) return;
    if (dx * dx + dy * dy > 6400) return;
    int angle = (c->orbit_camera_yaw + c->minimap_anticheat_angle) & 0x7ff;
    int zoom = c->minimap_zoom + 256;
    if (zoom == 0) return;
    int sin_a = _Pix3D.sin_table[angle] * 256 / zoom;
    int cos_a = _Pix3D.cos_table[angle] * 256 / zoom;
    int x = (dy * sin_a + dx * cos_a) >> 16;
    int y = (dy * cos_a - dx * sin_a) >> 16;
    float left = (float)(x + 94 - image->crop_w / 2 + image->crop_x);
    float top = (float)(83 - y - image->crop_h / 2 + image->crop_y);
    float sx = r->w / (float)VE_FRAME_W188, sy = r->h / (float)VE_FRAME_H184;
    float fx = r->x + (VE_MAP_CX + (left - 94.f) * (108.f / 100.f)) * sx;
    float fy = r->y + (VE_MAP_CY + (top - 83.f) * (108.f / 100.f)) * sy;
    GLuint tex = map_sprite(image);
    if (!tex) return;
    glBindTexture(GL_TEXTURE_2D, tex);
    map_clipped_icon(r,(fx-r->x)/sx,(fy-r->y)/sy,image->width*1.08f,image->height*1.08f);
}

static void map_gl_prepare(Client *c,int cold) {
    if (map_hole_ready || !c || !c->area_viewport || !c->area_viewport->pixels) return;
    const LcRect *r = &lc_game_geometry(LC_MINIMAP)->rect;
    PixMap *dst = c->area_viewport;
    if (r->w <= 0 || r->h <= 0) return;
    for (int y = 0; y < r->h; y++) {
        int sy = r->y + y;
        if (sy < 0 || sy >= dst->height) continue;
        unsigned *row = (unsigned *)dst->pixels + (size_t)sy * (size_t)dst->width;
        int x0 = r->x < 0 ? 0 : r->x;
        int x1 = r->x + r->w > dst->width ? dst->width : r->x + r->w;
        for (int x = x0; x < x1; x++) row[x] = 0xffffffffu;
    }
    if(cold)dst->dirty=true;
    else {pixmap_flush_rect(dst,r->x,r->y,r->w,r->h);dst->dirty=false;}
    map_hole_ready = 1;
}

void ve_map_gl_prepare(Client*c){map_gl_prepare(c,0);}
void ve_map_gl_prepare_cold(Client*c){map_gl_prepare(c,1);}

void ve_map_gl_draw(Client *c) {
    if (!c || !c->local_player || !c->image_minimap || !c->image_minimap->pixels) return;
    const LcRect *r = &lc_game_geometry(LC_MINIMAP)->rect;
    if (r->w <= 0 || r->h <= 0) return;
    if (!map_fan_n) map_build_fan();
    if (map_uploaded_gen != map_src_gen || !map_tex) {
        int w = c->image_minimap->width, h = c->image_minimap->height;
        if (w <= 0 || h <= 0) return;
        if (map_tex && (w != map_tex_w || h != map_tex_h)) {
            glDeleteTextures(1, &map_tex);
            map_tex = 0;
        }
        if (!map_tex) {
            map_tex = map_upload((const unsigned *)c->image_minimap->pixels, w, h, 2);
            map_tex_w = w;
            map_tex_h = h;
        } else {
            unsigned *buf = malloc((size_t)w * (size_t)h * 4u);
            if (!buf) return;
            const unsigned *src = (const unsigned *)c->image_minimap->pixels;
            for (int i = 0; i < w * h; i++) buf[i] = src[i] | 0xff000000u;
            glBindTexture(GL_TEXTURE_2D, map_tex);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, buf);
            free(buf);
        }
        if (!map_tex) return;
        map_uploaded_gen = map_src_gen;
    }
    if (!frame_tex) {
        Art *frame = get(2490);
        frame_tex = map_upload(frame->p, frame->w, frame->h, 0);
        frame_w = frame->w;
        frame_h = frame->h;
    }
    if (!compass) {
        Art *a = get(169);
        compass = pix24_new(a->w, a->h, false);
        for (int i = 0; i < a->w * a->h; i++) compass->pixels[i] = a->p[i] >> 24 ? (int)(a->p[i] & 0xffffffu) : 0;
    }
    if (!compass_tex && compass && compass->width > 0) {
        compass_tex = map_upload((const unsigned *)compass->pixels, compass->width, compass->height, 1);
        compass_w = compass->width;
        compass_h = compass->height;
    }
    if (!map_logged) {
        map_logged = 1;
        FILE *f = fopen("ux0:/data/RS2Vita/minimap-gl.txt", "w");
        if (f) { fprintf(f, "fan=%d map=%d,%d frame=%d,%d\n", map_fan_n, map_tex_w, map_tex_h, frame_w, frame_h); fclose(f); }
    }
    int angle = (c->orbit_camera_yaw + c->minimap_anticheat_angle) & 0x7ff;
    int zoom = c->minimap_zoom + 256;
    int anchor_x = c->local_player->pathing_entity.x / 32 + 48;
    int anchor_y = 464 - c->local_player->pathing_entity.z / 32;
    int px = c->local_player->pathing_entity.x / 32;
    int pz = c->local_player->pathing_entity.z / 32;
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_ALPHA_TEST);
    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4ub(255, 255, 255, 255);
    if (map_tex && map_fan_n > 2 && zoom != 0) {
        float sx = r->w / (float)map_fan_w, sy = r->h / (float)map_fan_h;
        glBindTexture(GL_TEXTURE_2D, map_tex);
        glBegin(GL_QUADS);
        for(int y=0;y<VE_FRAME_H184;y++) {
            int x0=ve_map_spans[y*2],x1=ve_map_spans[y*2+1];if(x1<=x0)continue;
            float corner[4][2]={{x0,y},{x0,y+1},{x1,y+1},{x1,y}};
            for(int k=0;k<4;k++) {
                float u,v;map_uv(corner[k][0],corner[k][1],anchor_x,anchor_y,angle,zoom,map_tex_w,map_tex_h,&u,&v);
                glTexCoord2f(u,v);glVertex2f(r->x+corner[k][0]*sx,r->y+corner[k][1]*sy);
            }
        }
        glEnd();
    }
    for (int i = 0; i < c->activeMapFunctionCount; i++)
        map_icon(c, r, c->activeMapFunctions[i], c->activeMapFunctionX[i] * 4 + 2 - px, c->activeMapFunctionZ[i] * 4 + 2 - pz);
    if (c->level_obj_stacks) {
        for (int ltx = 0; ltx < 104; ltx++) {
            for (int ltz = 0; ltz < 104; ltz++) {
                if (c->level_obj_stacks[c->currentLevel][ltx][ltz])
                    map_icon(c, r, c->image_mapdot0, ltx * 4 + 2 - px, ltz * 4 + 2 - pz);
            }
        }
    }
    for (int i = 0; i < c->npc_count; i++) {
        NpcEntity *npc = c->npcs[c->npc_ids[i]];
        if (npc && npcentity_is_visible(npc) && npc->type->minimap)
            map_icon(c, r, c->image_mapdot1, npc->pathing_entity.x / 32 - px, npc->pathing_entity.z / 32 - pz);
    }
    for (int i = 0; i < c->player_count; i++) {
        PlayerEntity *player = c->players[c->player_ids[i]];
        if (!player || !playerentity_is_visible(player)) continue;
        int friend = 0;
        int64_t name37 = jstring_to_base37(player->name);
        for (int j = 0; j < c->friend_count; j++) {
            if (name37 == c->friendName37[j] && c->friendWorld[j] != 0) { friend = 1; break; }
        }
        map_icon(c, r, friend ? c->image_mapdot3 : c->image_mapdot2, player->pathing_entity.x / 32 - px, player->pathing_entity.z / 32 - pz);
    }
    if (c->flagSceneTileX != 0)
        map_icon(c, r, c->image_mapflag, c->flagSceneTileX * 4 + 2 - px, c->flagSceneTileZ * 4 + 2 - pz);
    {
        float sx = r->w / (float)VE_FRAME_W188, sy = r->h / (float)VE_FRAME_H184;
        float left = 93.f, top = 82.f;
        glDisable(GL_TEXTURE_2D);
        glColor4ub(255, 255, 255, 255);
        map_quad(r->x + (VE_MAP_CX + (left - 94.f) * 1.08f) * sx, r->y + (VE_MAP_CY + (top - 83.f) * 1.08f) * sy, 3.f * 1.08f * sx, 3.f * 1.08f * sy, 0.f, 0.f, 1.f, 1.f);
        glEnable(GL_TEXTURE_2D);
    }
    if (compass_tex && compass_w > 0 && compass_h > 0) {
        float sx = r->w / (float)VE_FRAME_W188, sy = r->h / (float)VE_FRAME_H184;
        int yaw=c->orbit_camera_yaw;
        glBindTexture(GL_TEXTURE_2D,compass_tex);
        double thv=(double)yaw/326.11,sz=sin(thv)*226.0/256.0,cz=cos(thv)*226.0/256.0;
        glBegin(GL_QUADS);
        for(int y=0;y<VE_FRAME_H184;y++) {
            int x0=ve_compass_spans[y*2],x1=ve_compass_spans[y*2+1];if(x1<=x0)continue;
            float corner[4][2]={{x0,y},{x0,y+1},{x1,y+1},{x1,y}};
            for(int k=0;k<4;k++) {
                double lx=(corner[k][0]-VE_COMPASS_CX)*37.0/32.0;
                double ly=(corner[k][1]-VE_COMPASS_CY)*37.0/31.0;
                double src_x=23.0+sz*ly+cz*lx,src_y=23.0+cz*ly-sz*lx;
                glTexCoord2f((float)(src_x/compass_w),(float)(src_y/compass_h));
                glVertex2f(r->x+corner[k][0]*sx,r->y+corner[k][1]*sy);
            }
        }
        glEnd();
    }
    if (frame_tex && frame_w > 0 && frame_h > 0) {
        glBindTexture(GL_TEXTURE_2D, frame_tex);
        map_quad((float)r->x, (float)r->y, (float)r->w, (float)r->h, 0.f, 0.f, 1.f, 1.f);
    }
    glEnable(GL_CULL_FACE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    dp_mark_map();
}

static PixMap *hud_pm;
static GLuint hud_tex;
static int hud_hole, hud_ready;
static char hud_text[7][128];
static int hud_draw_ms,hud_vertices,hud_indices,hud_world_ms,hud_pixmap_ms;
void ve_hud_draw_metrics(int ms,int vertices,int indices){hud_draw_ms=ms;hud_vertices=vertices;hud_indices=indices;}
void ve_hud_scene_metrics(int world_ms,int pixmap_ms){if(world_ms>=0)hud_world_ms=world_ms;if(pixmap_ms>=0)hud_pixmap_ms=pixmap_ms;}

void ve_hud_gl_reset(void) { hud_hole = 0; }

void ve_hud_gl_prepare(Client *c) {
    PixMap *dst = c ? c->area_viewport : NULL;
    int x = 4, y = 28, w = 640, h = 80;
    if (hud_hole || !dst || !dst->pixels) return;
    if (x >= dst->width || y >= dst->height) return;
    if (x + w > dst->width) w = dst->width - x;
    if (y + h > dst->height) h = dst->height - y;
    for (int row = 0; row < h; row++) {
        unsigned *line = (unsigned *)dst->pixels + (size_t)(y + row) * (size_t)dst->width + (size_t)x;
        for (int col = 0; col < w; col++) line[col] = 0xffffffffu;
    }
    pixmap_flush_rect(dst, x, y, w, h);
    dst->dirty = false;
    hud_hole = 1;
}

void ve_hud_gl_update(Client *c, char lines[3][128]) {
    /* Keep collecting every frame; rasterize the human-readable display at
     * 4 Hz so observing the renderer does not upload a new HUD every frame. */
    static uint64_t display_at;
    uint64_t display_now=ba_timing_now();
    if(hud_ready&&display_now-display_at<250000u)return;
    display_at=display_now;
    char all[7][128];
    memcpy(all,lines,3*128);
    snprintf(all[3],128,"World3D: %d ms",hud_world_ms);
    snprintf(all[4],128,"Viewport pixmap: %d ms",hud_pixmap_ms);
    snprintf(all[5],128,"glDrawElements: %d ms",hud_draw_ms);
    snprintf(all[6],128,"verts/indices: %d %d",hud_vertices,hud_indices);
    int same = hud_ready;
    unsigned *buf;
    int count;
    uint64_t began;
    if (!c || !c->font_plain11 || !c->area_viewport) return;
    for (int i = 0; i < 7; i++) if (strcmp(all[i], hud_text[i]) != 0) same = 0;
    if (same) return;
    if (!hud_pm) hud_pm = pixmap_new(512, 112);
    if (!hud_pm || !hud_pm->pixels) return;
    pixmap_bind(hud_pm);
    memset(hud_pm->pixels, 0, (size_t)hud_pm->width * (size_t)hud_pm->height * sizeof(int));
    for (int i = 0; i < 7; i++) {
        drawString(c->font_plain11, 8, 17 + i * 14, all[i], 0xffff00);
        memcpy(hud_text[i], all[i], 128);
    }
    began = ba_timing_now();
    count = hud_pm->width * hud_pm->height;
    buf = malloc((size_t)count * 4u);
    if (buf) {
        const unsigned *src = (const unsigned *)hud_pm->pixels;
        for (int i = 0; i < count; i++) {
            unsigned pixel = src[i];
            /* Cleared pixels and the punch color stay transparent. Glyphs
             * are untagged RGB and become opaque. */
            if (pixel == 0u || pixel == 0xffffffffu) buf[i] = 0u;
            else buf[i] = (pixel >> 24) == 0u ? pixel | 0xff000000u : pixel;
        }
        if (!hud_tex) {
            glGenTextures(1, &hud_tex);
            glBindTexture(GL_TEXTURE_2D, hud_tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA_EXT, hud_pm->width, hud_pm->height, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, buf);
        } else {
            glBindTexture(GL_TEXTURE_2D, hud_tex);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, hud_pm->width, hud_pm->height, GL_BGRA_EXT, GL_UNSIGNED_BYTE, buf);
        }
        free(buf);
        hud_ready = hud_tex != 0;
    }
    r16_io_add(6, ba_timing_now() - began);
    pixmap_bind(c->area_viewport);
}

void ve_hud_gl_draw(void) {
    if (!hud_ready || !hud_tex) return;
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_ALPHA_TEST);
    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4ub(255, 255, 255, 255);
    glBindTexture(GL_TEXTURE_2D, hud_tex);
    map_quad(4.f, 28.f, 512.f, 112.f, 0.f, 0.f, 1.f, 1.f);
    glEnable(GL_CULL_FACE);
}
#else
void ve_hud_draw_metrics(int ms,int vertices,int indices){(void)ms;(void)vertices;(void)indices;}
void ve_hud_scene_metrics(int world_ms,int pixmap_ms){(void)world_ms;(void)pixmap_ms;}
void ve_world_rect(int x,int y,int w,int h,int rgb){(void)x;(void)y;(void)w;(void)h;(void)rgb;}
void ve_world_sprite(Pix24 *image, int x, int y) { (void)image; (void)x; (void)y; }
void ve_map_gl_invalidate(void) {}
void ve_map_gl_hole_reset(void) {}
void ve_map_gl_prepare(Client *c) { (void)c; }
void ve_map_gl_prepare_cold(Client *c) { (void)c; }
void ve_map_gl_draw(Client *c) { (void)c; }
void ve_hud_gl_reset(void) {}
void ve_hud_gl_prepare(Client *c) { (void)c; }
void ve_hud_gl_update(Client *c, char lines[3][128]) { (void)c; (void)lines; }
void ve_hud_gl_draw(void) {}
#endif

#include "classic_ui.h"
#include "vita_wide_layout.h"
static bool ve_chat_message(Client *c, int i, char *text, int *prefix, int *prefix_color, int *body_color) {
    int type=c->message_type[i];
    bool friend=client_is_friend(c,c->message_sender[i]);
    *prefix=0;*prefix_color=BLACK;*body_color=BLACK;
    if (!c->message_text[i][0]) return false;
    if (type==2 && !(c->public_chat_setting==0 || (c->public_chat_setting==1 && friend))) return false;
    if ((type==3 || type==7) && (c->split_private_chat || !(type==7 || c->private_chat_setting==0 || (c->private_chat_setting==1 && friend)))) return false;
    if ((type==5 || type==6) && (c->split_private_chat || c->private_chat_setting>=2)) return false;
    if ((type==4 || type==8) && !(c->trade_chat_setting==0 || (c->trade_chat_setting==1 && friend))) return false;
    text[0]=0;
    if (type==1 || type==2) {
        snprintf(text,CLASSIC_CHAT_CAPACITY,"%s: ",c->message_sender[i]);
        *prefix_color=type==1 ? WHITE : BLACK;*body_color=BLUE;
    } else if (type==3 || type==7) {
        snprintf(text,CLASSIC_CHAT_CAPACITY,"From %s: ",c->message_sender[i]);*body_color=DARKRED;
    } else if (type==6) {
        snprintf(text,CLASSIC_CHAT_CAPACITY,"To %s: ",c->message_sender[i]);*body_color=DARKRED;
    } else if (type==4 || type==8) {
        snprintf(text,CLASSIC_CHAT_CAPACITY,"%s ",c->message_sender[i]);*prefix_color=*body_color=type==4 ? TRADE_MESSAGE : DUEL_MESSAGE;
    } else if (type==5) *body_color=DARKRED;
    else if (type!=0) return false;
    *prefix=strlen(text);
    snprintf(text+*prefix,CLASSIC_CHAT_CAPACITY-*prefix,"%s",c->message_text[i]);
    return true;
}
int ve_chat_rows(Client *c, int index, PixFont *font) {
    char text[CLASSIC_CHAT_CAPACITY];int prefix,pc,bc;
    if (!ve_chat_message(c,index,text,&prefix,&pc,&bc)) return 0;
    ClassicChatLine lines[CLASSIC_CHAT_CAPACITY];
    return classic_chat_wrap(text,font,lc_chat_text_right(CHAT_TEXT_RIGHT)-4,lines,CLASSIC_CHAT_CAPACITY);
}
void ve_draw_chat(Client *c, PixFont *font) {
    int row=0;
    for (int i=0;i<100;i++) {
        char text[CLASSIC_CHAT_CAPACITY];int prefix,pc,bc;
        if (!ve_chat_message(c,i,text,&prefix,&pc,&bc)) continue;
        ClassicChatLine lines[CLASSIC_CHAT_CAPACITY];
        int count=classic_chat_wrap(text,font,lc_chat_text_right(CHAT_TEXT_RIGHT)-4,lines,CLASSIC_CHAT_CAPACITY);
        for (int j=0;j<count;j++) {
            int y=c->chat_scroll_offset+70-(row+count-1-j)*14;
            if (y<=0 || y>=110) continue;
            ClassicChatLine line=lines[j];char part[CLASSIC_CHAT_CAPACITY];int x=4;
            int split=line.end<prefix ? line.end : prefix;
            if (split>line.start) {
                int n=split-line.start;memcpy(part,text+line.start,n);part[n]=0;
                drawString(font,x,y,part,pc);x+=stringWidth(font,part);
            }
            int start=line.start>prefix ? line.start : prefix;
            if (line.end>start) {
                int n=line.end-start;memcpy(part,text+start,n);part[n]=0;
                drawString(font,x,y,part,bc);
            }
        }
        row+=count;
    }
    c->chat_scroll_height=row*14+7;
    if(c->chat_scroll_height<78)c->chat_scroll_height=78;
}

void ve_draw_chat_input(PixFont *font, int x, int y, const char *text, int color) {
#ifdef VITA_NATIVE_WIDESCREEN
    while (*text && stringWidth(font,text)>lc_chat_text_right(CHAT_TEXT_RIGHT)-x) text++;
#endif
    drawString(font,x,y,text,color);
}

