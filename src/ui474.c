#include "ba_timing.h"
#include "ui_preview.h"
#include "title_theme.h"
/* Isolated gameplay profile adapter. Original title rendering never calls this. */
#include "ui474.h"
#include "packet.h"
#include "r23_tutorial.h"
#include "r27_cursor.h"
extern ClientData _Client;
#include "vita_enhanced.h"
#include "layoutcore.h"
#include "layoutcore_draw.h"
#include "ui474_widgets.generated.h"
#include "pix8.h"
#include "pix24.h"
#include "pixmap.h"
#include "pix2d.h"
#include "pix3d.h"
#include "pixfont.h"
#include "objtype.h"
#include "component.h"
#include "gameshell.h"
#include "vita_wide_layout.h"
#include "vita_dialogue.h"
#include "runner_icon.generated.h"
#include "custom_run_stone.generated.h"
extern Pix2D _Pix2D;
#include "seqtype.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
extern ComponentData _Component;
extern Pix3D _Pix3D;
extern SeqTypeData _SeqType;

static bool active=false,loaded=false,initialized=false,confirm_held=false;
static int profile=2,scroll=0,focus=-1,previous_tab=-1;
static Pix8 *sprites[2048];
static PixMap *area;
static int offsets[343];
static unsigned ui_rect_calls,item_outline_calls,audit_frame;
static int energy_clean[44*36],energy_clean_ready,energy_clean_run;
void ui474_note_rect(const int *pixels){if(area&&pixels==area->pixels&&ve_enabled())ui_rect_calls++;}
static void supplied_runner(int x,int y){
 for(int yy=0;yy<RUNNER_HEIGHT;yy++)for(int xx=0;xx<RUNNER_WIDTH;xx++){
  int dx=x+xx,dy=y+yy;if(dx<_Pix2D.left||dx>=_Pix2D.right||dy<_Pix2D.top||dy>=_Pix2D.bottom)continue;
  unsigned src=runner_pixels[yy*RUNNER_WIDTH+xx],a=src>>24;if(!a)continue;
  int *dst=&_Pix2D.pixels[dy*_Pix2D.width+dx];unsigned old=(unsigned)*dst,result=0;
  for(int shift=0;shift<=16;shift+=8)result|=(((((src>>shift)&255)*a+((old>>shift)&255)*(255-a)+127)/255)<<shift);
  *dst=(int)result;
 }
}
/* Scale only the existing orange font raster; keep the stone and icon intact. */
static void run_energy(PixFont *font,const char *text){
 int raster[64*20];for(int i=0;i<64*20;i++)raster[i]=-1;
 Pix2D saved=_Pix2D;pix2d_bind(64,20,raster);
 drawStringTaggableCenter(font,text,32,14,0xff981f,true);
 _Pix2D=saved;
 int left=64,right=-1,top=20,bottom=-1;
 for(int y=0;y<20;y++)for(int x=0;x<64;x++)if(raster[y*64+x]!=-1){if(x<left)left=x;if(x>right)right=x;if(y<top)top=y;if(y>bottom)bottom=y;}
 if(right<left)return;
 int sw=right-left+1,sh=bottom-top+1,dw=sw,dh=sh-1;
 int ox=26-dw/2,oy=334-dh;
 for(int y=0;y<dh;y++)for(int x=0;x<dw;x++){
  int color=raster[(top+y*sh/dh)*64+left+x*sw/dw];
  int dx=ox+x,dy=oy+y;if(color!=-1&&dx>=saved.left&&dx<saved.right&&dy>=saved.top&&dy<saved.bottom)saved.pixels[dy*saved.width+dx]=color;
 }
}
/* Supplied red stone, mutually exclusive with the gray strip background. */
static void custom_run_background(void){
 /* Approved second supplied stone is the package default; no preview flag required. */
 const unsigned*art=custom_run_stone_2;int width=40;
 for(int y=0;y<36;y++)for(int x=0;x<width;x++){
  unsigned color=art[y*width+x],alpha=color>>24;if(!alpha)continue;
  int*dst=&_Pix2D.pixels[(306+y)*_Pix2D.width+x+4];unsigned old=(unsigned)*dst,result=0;
  /* GPU overlay clear is transparent, not a white RGB backdrop. */
  if(old==0xffffffffu){*dst=(int)(alpha<255?color:(color&0xffffffu));continue;}
  unsigned da=old>>24;
  if(da>0&&da<255){
   unsigned outa=alpha+da*(255-alpha)/255;
   for(int shift=0;shift<=16;shift+=8)result|=((((color>>shift&255)*alpha+(old>>shift&255)*da*(255-alpha)/255)/outa)<<shift);
   *dst=(int)(result|(outa<255?outa<<24:0));continue;
  }
  for(int shift=0;shift<=16;shift+=8)result|=(((((color>>shift)&255)*alpha+((old>>shift)&255)*(255-alpha)+127)/255)<<shift);
  *dst=(int)result;
 }
}
static void widened_selected_stone(int id,int x,int y,int width){
 Pix8*a=sprites[id];for(int yy=0;yy<a->height;yy++)for(int xx=0;xx<width;xx++){
  int index=(unsigned char)a->pixels[yy*a->width+xx*a->width/width];if(index)_Pix2D.pixels[(y+yy)*_Pix2D.width+x+xx]=a->palette[index];
 }
}
static void content_audit(Client*c){
 static int enabled=-1;if(enabled<0){FILE*f=fopen("ux0:/data/RS2Vita/ui-content-audit.txt","r");enabled=f!=NULL;if(f)fclose(f);}
 if(!enabled||!ve_enabled())return;
 FILE*f=fopen("ux0:/data/RS2Vita/ui-content-frames.csv","a");if(f){fprintf(f,"%u,%d,%u,%u,%d,%d,%d,%d\n",++audit_frame,c->selected_tab,ui_rect_calls,item_outline_calls,c->obj_selected,focus,c->varps[173],c->energy);fclose(f);}
}

typedef struct{int x,y,w,h,target,kind,arg,item; } Hit;
static Hit hits[180];static int hit_count;
static int u16(FILE*f){int a=fgetc(f),b=fgetc(f);if(a<0||b<0)abort();return a*256+b;}
static unsigned u32(FILE*f){unsigned a=u16(f),b=u16(f);return(a<<16)|b;}
static void preview_thin_sprite(Pix8*a,int id){
 if(!ui_thin_preview()||!((id>=1026&&id<=1030)||id==761||id==762))return;
 int n=a->width*a->height;unsigned*rgba=malloc((size_t)n*sizeof(unsigned));if(!rgba)abort();
 for(int i=0;i<n;i++){int index=(unsigned char)a->pixels[i];rgba[i]=index?0xff000000u|(unsigned)a->palette[index]:0;}
 ui_one_pixel_border(rgba,a->width,0,0,a->width,a->height);
 for(int i=0;i<n;i++)if(rgba[i]>>24){for(int k=1;k<a->palette_count;k++)if((rgba[i]&0xffffffu)==((unsigned)a->palette[k]&0xffffffu)){a->pixels[i]=(int8_t)k;break;}}
 free(rgba);
}
static void load(void){
 if(loaded)return;
 FILE*f=fopen("rom/ui474.bin","rb");if(!f){fprintf(stderr,"UI474 missing isolated sprite bundle\n");abort();}
 unsigned char magic[6];if(fread(magic,1,6,f)!=6||memcmp(magic,"U474\0\1",6))abort();int count=u16(f);if(count>2048)abort();
 for(int i=0;i<count;i++){
  int id=u16(f),w=u16(f),h=u16(f),x=u16(f),y=u16(f),cw=u16(f),ch=u16(f),pc=u16(f);
  if(id>=2048||sprites[id]||w<1||h<1||w>1024||h>1024||pc<1||pc>256||x+w>cw||y+h>ch)abort();
  int*palette=calloc(pc,sizeof(int));for(int p=0;p<pc;p++)palette[p]=(int)u32(f);
  Pix8*s=pix8_new(w,h,palette,pc);s->pixels=calloc((size_t)w*h,1);if(!s->pixels)abort();s->crop_x=x;s->crop_y=y;s->crop_w=cw;s->crop_h=ch;if(fread(s->pixels,1,(size_t)w*h,f)!=(size_t)w*h)abort();
  for(int p=0;p<w*h;p++)if((unsigned char)s->pixels[p]>=pc)abort();preview_thin_sprite(s,id);sprites[id]=s;
 }
 if(fgetc(f)!=EOF)abort();fclose(f);area=pixmap_new(249,343);for(int y=0;y<343;y++)offsets[y]=249*y;loaded=true;
}
Pix8 *ui474_original_sprite(int id){load();return id>=0&&id<2048?sprites[id]:NULL;}
void ui474_begin(Client*c){
 if(!c->ingame){active=false;return;}
 lc_game_init(SCREEN_WIDTH,SCREEN_HEIGHT,
 #ifdef VITA_NATIVE_WIDESCREEN
 true
 #else
 false
 #endif
 );
 static unsigned seen=0;static LcRect previous[LC_ROLES];if(seen!=lc_game_generation()){
 if(seen){platform_set_color(0);for(int i=0;i<LC_ROLES;i++)platform_fill_rect(previous[i].x,previous[i].y,previous[i].w,previous[i].h);}
 seen=lc_game_generation();for(int i=0;i<LC_ROLES;i++)previous[i]=lc_game_geometry(i)->rect;
 c->redraw_background=c->redraw_sidebar=c->redraw_sideicons=c->redraw_chatback=c->redraw_privacy_settings=true;}
 if(!initialized){
  initialized=true;profile=title_theme_profile();
  const char*p=getenv("UI474_PROFILE");if(p&&getenv("CLASSIC_MIGRATION_TEST"))profile=!strcmp(p,"vita")?2:strcmp(p,"classic")!=0;
 }
 static unsigned theme_seen=0;if(theme_seen!=title_theme_revision()){profile=title_theme_profile();theme_seen=title_theme_revision();}
 vita_dialogue_layout(profile==2);
 static int logged_dialogue=-1;
 if(c->chat_interface_id!=logged_dialogue){logged_dialogue=c->chat_interface_id;if(logged_dialogue>=0){Component*d=_Component.instances[logged_dialogue];FILE*f=fopen("ux0:/data/RS2Vita/dialogue-layout.csv","a");if(f&&d){fprintf(f,"root,%d,%d,%d,%d\n",profile,d->id,d->width,d->height);for(int i=0;i<d->childCount;i++){Component*q=_Component.instances[d->childId[i]];if(q)fprintf(f,"child,%d,%d,%d,%d,%d,%d\n",q->id,q->type,d->childX[i],d->childY[i],q->width,q->height);} }if(f)fclose(f);}}
 active=c->ingame&&profile>0;lc_game_profile(profile);lc_game_chat_wide(c->chat_interface_id>=0||c->sticky_chat_interface_id>=0||c->show_social_input||c->chatback_input_open||c->modal_message[0]);lc_game_chat_hidden(ve_chat_hidden(c));
 if(c->selected_tab!=previous_tab){focus=-1;previous_tab=c->selected_tab;}
}
bool ui474_enabled(void){return active;}
bool ve_enabled(void){return active&&profile==2;}
static int content_y(void){return 45;}
static int area_x(void){return ve_enabled()?ve_side_x():lc_x(LC_SIDEBAR,active?SCREEN_WIDTH-249:UI_RIGHT(562));}
static int area_y(void){return ve_enabled()?ve_side_y():lc_y(LC_SIDEBAR,active?SCREEN_HEIGHT-343:UI_BOTTOM(231));}
int ui474_side_x(void){return ve_enabled()?ve_side_x()+31:lc_x(LC_SIDEBAR,active?SCREEN_WIDTH-218:UI_RIGHT(562));}
int ui474_side_y(void){return ve_enabled()?ve_side_y()+45:lc_y(LC_SIDEBAR,active?SCREEN_HEIGHT-298:UI_BOTTOM(231));}
int ui474_player_sequence(int source){
 // Asset compatibility is independent of the selected gameplay interface.
 // Untagged older caches keep their original sequence interpretation.
 if(source==866&&_SeqType.count>UI474_DANCE_SEQUENCE&&_SeqType.instances[UI474_DANCE_SEQUENCE]&&_SeqType.instances[UI474_DANCE_SEQUENCE]->compatibilityMetadata)return UI474_DANCE_SEQUENCE;
 return source;
}
int ui474_focus_target(void){return focus>=0&&focus<hit_count?hits[focus].target:-1;}
int ui474_focus_kind(void){return focus>=0&&focus<hit_count?hits[focus].kind:-1;}
int ui474_scroll_position(void){return scroll;}
static bool inside(int x,int y,const Hit*h){return x>=h->x&&x<h->x+h->w&&y>=h->y&&y<h->y+h->h;}
static void hit(int x,int y,int w,int h,int target,int kind,int arg){if(hit_count>=180)abort();hits[hit_count++]=(Hit){x,y,w,h,target,kind,arg,-1};}
static void sprite(int id,int x,int y){if(id>=0&&id<2048&&sprites[id])pix8_draw(sprites[id],x,y);}
static int selected(Client*c,const Ui474Widget*w){
 if(w->group!=261)return 0;
 for(unsigned i=0;i<sizeof(ui474_conditions)/sizeof(ui474_conditions[0]);i++){const Ui474Condition*q=&ui474_conditions[i];if(q->child==w->child)return c->varps[q->varp]==q->value;}
 return 0;
}
static void drawslots(Client*c,int component,const Ui474Slot*slots,int count,bool equipment){
 Component*v=_Component.instances[component];if(!v||!v->invSlotObjId||!v->invSlotObjCount)return;
 int available=v->width*v->height;if(count>available)count=available;
 for(int i=0;i<count;i++){
  int x=31+slots[i].x,y=content_y()+slots[i].y,id=v->invSlotObjId[i]-1,n=v->invSlotObjCount[i];
  if(id>=0){bool use=c->obj_selected==1&&c->objSelectedSlot==i&&c->objSelectedInterface==component;Pix24*icon=use?objtype_get_icon_outline(id,n,WHITE):objtype_get_icon(id,n);if(icon){pix24_draw(icon,x,y);if(use)item_outline_calls++;}if(use&&icon)pix24_free(icon);if(n>1){char text[32];snprintf(text,sizeof(text),"%d",n);drawString(c->font_plain11,x+1,y+10,text,0);drawString(c->font_plain11,x,y+9,text,0xffff00);}}
  else if(equipment)sprite(ui474_equipment_backgrounds[i],x,y);
  if(id>=0){hit(x,y,32,32,component,equipment?4:3,i);hits[hit_count-1].item=id;}
 }
}
static void style_button(Client*c,int x,int y){
 drawStringTaggableCenter(c->font_plain11,profile?(profile==2?"Vita Enhanced / Classic Original":"474 Enhanced / Classic Brown"):"Style: Classic Brown  /  474",x+95,y+12,0xffff00,true);
 hit(x,y,190,14,-1,2,profile?0:2);
}
bool ui474_select_profile(Client*c,int next){
 if(next<0||next>2)return false;
 if(!c->ingame||c->viewport_interface_id>=0||c->sidebar_interface_id>=0||c->obj_drag_area||c->menu_visible||(c->local_player&&c->local_player->pathing_entity.primarySeqId>=0))return false;
 profile=next;active=next>0;focus=-1;scroll=0;confirm_held=true;c->redraw_background=true;c->redraw_sidebar=c->redraw_sideicons=true;
 title_theme_set_profile(profile);return true;
}
/* Composite the recovered frame masks over existing native stone surfaces.
 * The historical panel sprites retain their own transparent pixels; no new art. */
static void backdrop_copy(PixMap*p,int sx,int sy){
 if(!p)return;int ox=SCREEN_WIDTH-249,oy=SCREEN_HEIGHT-343;
 for(int y=0;y<p->height;y++){int dy=sy+y-oy;if(dy<0||dy>=343)continue;
  for(int x=0;x<p->width;x++){int dx=sx+x-ox;if(dx>=0&&dx<249)area->pixels[dy*249+dx]=p->pixels[y*p->width+x];}
 }
}
static void historical_backdrop(Client*c){
 pix2d_fill_rect(0,0,0x302d27,249,343);
 backdrop_copy(c->area_backvmid1,UI_RIGHT(520),11);
 backdrop_copy(c->area_backvmid2,UI_RIGHT(520),UI_BOTTOM(231));
 backdrop_copy(c->area_backvmid3,UI_RIGHT(501),UI_BOTTOM(375));
 backdrop_copy(c->area_backright1,UI_RIGHT(729),5);
 backdrop_copy(c->area_backright2,UI_RIGHT(752),UI_BOTTOM(231));
 backdrop_copy(c->area_backhmid2,0,UI_BOTTOM(345));
 backdrop_copy(c->area_backbase1,0,UI_BOTTOM(471));
 // Original blank native framing, not the buffers containing old tab icons.
 pix8_draw(c->image_backhmid1,UI_RIGHT(520)-(SCREEN_WIDTH-249),165-(SCREEN_HEIGHT-343));
 pix8_draw(c->image_backbase2,UI_RIGHT(501)-(SCREEN_WIDTH-249),UI_BOTTOM(492)-(SCREEN_HEIGHT-343));
}
static int frame_tab(int child){if(child>=47&&child<=53)return child-47;if(child>=30&&child<=36)return child-30+7;return -1;}
static int icon_tab(int child){if(child>=54&&child<=60)return child-54;if(child>=37&&child<=43)return child-37+7;return -1;}

static void ui474_draw_panels(Client*c,int paint_chat){
    BA_PROFILE_SCOPE(BA_UI_COMPOSE);
 ui_rect_calls=item_outline_calls=0;
 ui474_begin(c);Hit selected_hit={0};bool had_focus=focus>=0&&focus<hit_count;if(had_focus)selected_hit=hits[focus];hit_count=0;
 if(!c->ingame)return;
 if(!active){
  if(c->selected_tab==11&&c->sidebar_interface_id<0){pixmap_bind(c->area_sidebar);style_button(c,0,247);/* Focus remains tracked; original stone artwork supplies visual state. */pixmap_draw(c->area_sidebar,ui474_side_x(),ui474_side_y());pixmap_bind(c->area_viewport);}
  return;
 }
 load();pixmap_bind(area);_Pix3D.line_offset=offsets;if(ve_enabled()){ve_side_backdrop(c,area);ve_art(area,2036,0,0);ve_bottom_stones(area,c->varps[173]!=0,c->sidebar_interface_id<0?c->selected_tab:-1);ve_side_caps(area);}else historical_backdrop(c);
 // Native outer structure stays intact; only the recovered content fill is replaced.
 if(!ve_enabled())sprite(1031,31,45);
 for(unsigned i=0;i<sizeof(ui474_widgets)/sizeof(ui474_widgets[0]);i++){
  const Ui474Widget*w=&ui474_widgets[i];if(w->group!=548||w->x<516||w->y<160)continue;
  int tab=frame_tab(w->child),icon=icon_tab(w->child);int shift_y=ve_enabled()?0:0;
  if(tab<0&&icon<0)continue; /* No recovered outer474border/housing overlays. */
  if(tab>=0){
   if(c->tab_interface_id[tab]>=0&&c->sidebar_interface_id<0){
    // Original CS2 script71 clears all14 underlays, then assigns one selected sprite.
    if(tab==c->selected_tab){int x=w->x-516,y=w->y-160+shift_y,width=w->w;if(ve_enabled()&&w->child==31){x=42;width=34;}if(ve_enabled()&&w->child==32){x=76;width=34;}if(ve_enabled()&&w->child==31)x+=1;if(ve_enabled()&&w->child==35)x-=1;if(ve_enabled()&&w->child==36)x-=2;if(ve_enabled()&&(tab==5||tab==6))x-=2;if(ve_enabled()){if(tab==0||tab==1)x+=2;else if(tab==2)x+=1;if(tab==6)width+=1;if(tab==0){x-=2;width+=2;}}pix2d_set_clipping(y+w->h,x+width,y,x);if(width!=w->w)widened_selected_stone(w->sprite,x,y,width);else sprite(w->sprite,x,y);pix2d_reset_clipping();}
    int hx=w->x-516,hw=w->w;if(ve_enabled()&&w->child==31){hx=42;hw=34;}if(ve_enabled()&&w->child==32){hx=76;hw=34;}if(ve_enabled()&&w->child==36)hw=40;hit(hx,w->y-160+shift_y,hw,w->h,-1,1,tab);
   }
  }else if((icon<0||c->tab_interface_id[icon]>=0)&&r23_tutorial_icon_visible(c->flashing_tab,icon,_Client.loop_cycle)){int ix=w->x-516;if(ve_enabled()&&w->child==59)ix-=2;if(ve_enabled()&&w->child==41)ix-=1;if(ve_enabled()&&w->child==42)ix-=2;sprite(w->sprite,ix,w->y-160+shift_y);}
 }
 if(ve_enabled()){
  /* Review-only shortcut in the unused bottom-left socket beside Friends.
   * Highlight and text follow received authoritative varp/energy values. */
  if(c->varps[173])custom_run_background();
  supplied_runner(25-RUNNER_WIDTH/2,309);
  for(int row=0;row<36;row++)memcpy(energy_clean+row*44,area->pixels+(306+row)*area->width,44*sizeof(int));
  energy_clean_run=c->varps[173];energy_clean_ready=1;
  char energy[16];int value=c->energy;if(value<0)value=0;if(value>100)value=100;snprintf(energy,sizeof(energy),"%d%%",value);
  run_energy(c->font_plain11,energy);
  hit(0,306,42,36,-1,6,0);
 }
 int tab=c->selected_tab;
 if(c->sidebar_interface_id>=0)client_ui474_draw_interface(c,_Component.instances[c->sidebar_interface_id],31,content_y(),0);
 else if(tab==3)drawslots(c,3214,ui474_inventory_slots,28,false);
 else if(tab==4){
  for(unsigned i=0;i<sizeof(ui474_widgets)/sizeof(ui474_widgets[0]);i++){const Ui474Widget*w=&ui474_widgets[i];if(w->group==387)sprite(w->sprite,31+w->x,content_y()+w->y);}
  drawslots(c,1688,ui474_equipment_slots,15,true);
 }
 else if(tab==11||tab==12){
  int group=tab==11?261:464;pix2d_set_clipping(content_y()+261,221,content_y(),31);
  for(unsigned i=0;i<sizeof(ui474_widgets)/sizeof(ui474_widgets[0]);i++){
   const Ui474Widget*w=&ui474_widgets[i];if(w->group!=group)continue;
   int x=31+w->x,y=content_y()+w->y-(group==464?scroll:0);if(y+w->h<=content_y()||y>=content_y()+261)continue;
   int target=w->target;
   if(group==261){if(w->child==0)target=ve_enabled()?-1:(c->varps[173]?UI474_RUN_OFF:UI474_RUN_ON);if(w->child==3)target=c->varps[170]?UI474_MOUSE_OFF:UI474_MOUSE_ON;if(w->child==1)target=c->varps[171]?UI474_CHAT_OFF:UI474_CHAT_ON;if(w->child==2)target=c->varps[287]?UI474_PM_OFF:UI474_PM_ON;}
   int id=selected(c,w)&&w->active>=0?w->active:w->sprite;if(ve_enabled()&&group==261&&w->child==0)id=w->sprite;if(!(ve_enabled()&&group==261&&w->child==28))sprite(id,x,y);
   if(ve_enabled()&&group==261&&w->child==0){r27_cursor_preview(x,y,w->w,w->h);hit(x,y,w->w,w->h,-1,7,0);}
   if(target>=0){int hx=x<31?31:x,hy=y<content_y()?content_y():y,hr=x+w->w>221?221:x+w->w,hb=y+w->h>content_y()+261?content_y()+261:y+w->h;if(hr>hx&&hb>hy)hit(hx,hy,hr-hx,hb-hy,target,0,0);}
  }
  pix2d_reset_clipping();
  if(group==261){if(!ve_enabled()){char text[32];snprintf(text,sizeof(text),"%d%%",c->energy);int baseline=content_y()+227+(18-c->font_plain12->height)/2+c->font_plain12->height;drawStringTaggableCenter(c->font_plain12,text,126,baseline,0xff981f,true);}style_button(c,31,content_y()+247);}
  else{
   client_draw_scrollbar(c,204,content_y(),scroll,503,260);hit(204,content_y(),16,16,-1,5,-49);hit(204,content_y()+244,16,16,-1,5,49);
  }
 }else if(c->tab_interface_id[tab]>=0)client_ui474_draw_interface(c,_Component.instances[c->tab_interface_id[tab]],31,content_y(),0);
 if(had_focus){focus=-1;for(int i=0;i<hit_count;i++){Hit*h=&hits[i];if(h->target==selected_hit.target&&h->kind==selected_hit.kind&&h->arg==selected_hit.arg&&h->item==selected_hit.item){focus=i;break;}}}
 /* Internal controller focus is retained without a generic selector rectangle. */
 if(c->menu_visible&&c->menu_area==1){int x=c->menu_x,y=c->menu_y;c->menu_x+=31;c->menu_y+=content_y();client_draw_menu(c);c->menu_x=x;c->menu_y=y;}
 content_audit(c);
 if(ve_enabled())ve_side_finish(c,area);else lc_draw(area,LC_SIDEBAR,lc_x(LC_SIDEBAR,SCREEN_WIDTH-249),lc_y(LC_SIDEBAR,SCREEN_HEIGHT-343));if(paint_chat||!ve_enabled())ui474_parchment_draw(c);pixmap_bind(c->area_viewport);_Pix3D.line_offset=c->area_viewport_offsets;
}
void ui474_draw(Client*c){ui474_draw_panels(c,1);}
void ui474_draw_sidebar(Client*c){ui474_draw_panels(c,0);}
/* Energy changes touch only the existing 44x36 footer. Preserve the original
 * artwork and rasterizer, and avoid repainting/comparing the entire interface. */
bool ui474_energy_patch_ready(Client*c){
 return active&&ve_enabled()&&area&&energy_clean_ready&&c->varps&&energy_clean_run==c->varps[173]&&c->area_viewport&&c->area_viewport->width==960&&c->area_viewport->height==544;
}
bool ui474_energy_patch(Client*c){
 if(!ui474_energy_patch_ready(c))return false;
 Pix2D saved=_Pix2D;pixmap_bind(area);
 for(int row=0;row<36;row++)memcpy(area->pixels+(306+row)*area->width,energy_clean+row*44,44*sizeof(int));
 char text[16];int value=c->energy;if(value<0)value=0;if(value>100)value=100;snprintf(text,sizeof text,"%d%%",value);run_energy(c->font_plain11,text);
 int x=ve_side_x(),y=ve_side_y()+306;PixMap*dst=c->area_viewport;
 if(x<0||y<0||x+44>dst->width||y+36>dst->height){_Pix2D=saved;return false;}
 for(int row=0;row<36;row++)memcpy(dst->pixels+(y+row)*dst->width+x,area->pixels+(306+row)*area->width,44*sizeof(int));
 pixmap_flush_rect(dst,x,y,44,36);_Pix2D=saved;return true;
}
static bool invoke(Client*c,Hit*h){
 if(h->kind==7){r27_cursor_cycle();c->redraw_sidebar=true;return true;}
 if(h->kind==1){c->selected_tab=h->arg;c->redraw_sidebar=c->redraw_sideicons=true;focus=-1;return true;}
 if(h->kind==2){if(!ui474_select_profile(c,h->arg))client_add_message(c,0,"Finish the current action and close any open windows before changing UI style.","");return true;}
 if(h->kind==6){if(ve_enabled())client_vita_dialogue_action(c,c->varps[173]?UI474_RUN_OFF:UI474_RUN_ON,951);return true;}
 if(h->kind==5){scroll+=h->arg;if(scroll<0)scroll=0;if(scroll>243)scroll=243;focus=-1;return true;}
 if(h->kind==3||h->kind==4){Component*v=_Component.instances[h->target];int id=v->invSlotObjId[h->arg]-1;if(id<0||id!=h->item)return true;if(h->kind==4)client_ui474_invoke(c,602,id,h->arg,h->target);else{ObjType*o=objtype_get(id);int actions[]={405,38,422,478,347};for(int i=0;i<5;i++)if(o->iop&&o->iop[i]){client_ui474_invoke(c,actions[i],id,h->arg,h->target);break;}}return true;}
 if(h->target>=0){client_vita_dialogue_action(c,h->target,951);return true;}return false;
}
bool ui474_pointer(Client*c,int x,int y,int button){
 if(!c->ingame||c->menu_visible||c->viewport_interface_id>=0)return false;
 if(button!=1)return false;
 int ox=area_x(),oy=area_y();
 for(int i=hit_count-1;i>=0;i--)if(inside(x-ox,y-oy,&hits[i])){
  /* Presentation changes are safe during dialogue; gameplay widget actions
   * retain their existing dialogue guard. The conversation stays server-owned. */
  if(c->chat_interface_id>=0&&hits[i].kind!=2&&hits[i].kind!=7&&!(hits[i].kind==1&&hits[i].arg==11))return false;
  focus=i;if(hits[i].kind==3||hits[i].kind==4)return false;return invoke(c,&hits[i]);
 }
 if(c->chat_interface_id>=0)return false;
 if(active&&(c->selected_tab==11||c->selected_tab==12)&&x-ox>=31&&x-ox<221&&y-oy>=content_y()&&y-oy<content_y()+261){client_add_message(c,0,"That control is not yet restored in this isolated UI prototype.","");return true;}
 if(active&&c->selected_tab==4&&x-ox>=31&&x-ox<221&&y-oy>=content_y()+204&&y-oy<content_y()+261){client_add_message(c,0,"That control is not yet restored in this isolated UI prototype.","");return true;}
 return false;
}
bool ui474_menu(Client*c,int x,int y){
 if(!active||c->sidebar_interface_id>=0)return false;
 if(c->selected_tab==11||c->selected_tab==12)return true;
 if(c->selected_tab!=3&&c->selected_tab!=4)return false;
 int id=c->selected_tab==3?3214:1688,rootid=c->selected_tab==3?3213:1644,count=c->selected_tab==3?28:15;
 Component*original=_Component.instances[id];Component*parent=_Component.instances[rootid];if(!original||!parent)return false;
 Component child=*original,root=*parent;int childid=id,cx=c->selected_tab==3?16:24,cy=c->selected_tab==3?8:44,dx[20]={0},dy[20]={0};
 child.marginX=c->selected_tab==3?10:24;child.marginY=c->selected_tab==3?4:8;const Ui474Slot*slots=c->selected_tab==3?ui474_inventory_slots:ui474_equipment_slots;
 for(int i=0;i<count&&i<20;i++){dx[i]=slots[i].x-cx-(i%child.width)*(32+child.marginX);dy[i]=slots[i].y-cy-(i/child.width)*(32+child.marginY);}
 child.invSlotOffsetX=dx;child.invSlotOffsetY=dy;root.childCount=1;root.childId=&childid;root.childX=&cx;root.childY=&cy;root.width=190;root.height=261;
 _Component.instances[id]=&child;handleInterfaceInput(c,&root,x,y,ui474_side_x(),ui474_side_y(),0);_Component.instances[id]=original;return true;
}
bool ui474_controller(Client*c,bool confirm,int direction,bool pointer_activity){
 bool edge=confirm&&!confirm_held;confirm_held=confirm;
 if(!c->ingame||c->chat_interface_id>=0||c->viewport_interface_id>=0||c->sidebar_interface_id>=0||c->menu_visible||(!active&&c->selected_tab!=11))return false;
 int x=c->shell->mouse_x-area_x(),y=c->shell->mouse_y-area_y();
 if(pointer_activity)focus=-1;
 if(x<0||x>=(active?249:190)||y<0||y>=(active?343:261))return false;
 // Preserve existing content navigation in tabs that have not yet been ported.
 if(active&&x>=31&&x<221&&y>=content_y()&&y<content_y()+261&&c->selected_tab!=3&&c->selected_tab!=4&&c->selected_tab!=11&&c->selected_tab!=12)return false;
 if(confirm&&!edge)return true;
 if(direction&&hit_count){focus=focus<0?(direction>0?0:hit_count-1):(focus+(direction>0?1:-1)+hit_count)%hit_count;c->redraw_sidebar=true;}
 if(confirm&&focus>=0&&focus<hit_count){if(edge)invoke(c,&hits[focus]);return true;}
 return direction!=0;
}
