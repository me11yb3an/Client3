#include "offline_paths.h"
#include "gameshell.h"
/* Isolated local presentation preference + proposed title overlay.
 * No login credentials, server state, world routing or protocol dependencies. */
#include "title_theme.h"
#include "vita_title.h"
#include "pix2d.h"
#include "pixmap.h"
#include "pix8.h"
#include "pixfont.h"
#include "vita_wide_layout.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
extern Pix2D _Pix2D;
static int selected=2;static bool loaded,held;static unsigned revision;
static const int bx=24,by=496,bw=132,bh=38;
static PixMap*button,*source;
static void load_preference(void){
 if(loaded)return;loaded=true;FILE*f=fopen(BA_PREFERENCE("ui-profile.txt"),"r");if(!f)return;
 char text[64],*end;if(!fgets(text,sizeof(text),f)){fclose(f);return;}bool extra=fgetc(f)!=EOF;fclose(f);if(extra)return;
 long n=strtol(text,&end,10);if(end==text)return;while(isspace((unsigned char)*end))end++;if(*end)return;
 if(n>=0&&n<=2)selected=(int)n;
}
int title_theme_profile(void){load_preference();return selected;}
unsigned title_theme_revision(void){load_preference();return revision;}
bool title_theme_set_profile(int next){
 load_preference();if(next<0||next>2)return false;if(next==selected)return true;
 FILE*f=fopen(BA_PREFERENCE("ui-profile.txt.tmp"),"w");if(!f)return false;
 int failed=fprintf(f,"%d\n",next)<0;if(fflush(f))failed=1;if(fclose(f))failed=1;
 if(failed||rename(BA_PREFERENCE("ui-profile.txt.tmp"),BA_PREFERENCE("ui-profile.txt")))return false;
 selected=next;revision++;return true;
}
static bool enabled(Client*c){const char*e=getenv("VITA_THEME_SELECTOR");return (!e||strcmp(e,"0"))&&!c->ingame&&c->title_restored&&c->image_titlebutton&&c->font_bold12;}
static const char*name(int p){return p==0?"Classic Brown":p==1?"474 Enhanced":p==3?"Classic Grey":"Vita Enhanced";}
/* Local rounded stone treatment. Only interior texture is resized.
 * Fonts are drawn directly at native1x after all stone composition. */
static int inset(int y){static const int edge[6]={7,4,2,1,1,0};int d=y<bh/2?y:bh-1-y;return d<6?edge[d]:0;}
static bool inside(int x,int y){return x>=0&&x<bw&&y>=0&&y<bh&&x>=inset(y)&&x<bw-inset(y);}
static bool rim(int x,int y){return !inside(x-1,y)||!inside(x+1,y)||!inside(x,y-1)||!inside(x,y+1);}
static void draw_label(Client*c,int profile,bool unavailable){
 static Client*owner;static const void*art,*background,*bold,*plain;
 static unsigned generation;static int last_profile,last_unavailable,ready;
 unsigned current=vita_title_generation();
 if(ready&&button&&!button->dirty&&owner==c&&art==c->image_titlebutton&&background==c->image_title3&&bold==c->font_bold12&&plain==c->font_plain12&&generation==current&&last_profile==profile&&last_unavailable==unavailable){pixmap_draw(button,bx,by);return;}
 Pix2D saved=_Pix2D;if(!source)source=pixmap_new(146,40);if(!button)button=pixmap_new(bw,bh);
 pixmap_bind(source);pix2d_clear();pix8_draw(c->image_titlebutton,0,0);pixmap_bind(button);
 for(int y=0;y<bh;y++)for(int x=0;x<bw;x++){
  int bg=(by+y-TITLE_BOTTOM_Y)*c->image_title3->width+bx+x-TITLE_BOTTOM_X;
  int color=c->image_title3->pixels[bg];
  if(inside(x,y)){
   /* Exclude ALL original border pixels from the texture source. */
   int sx=8+x*130/bw,sy=6+y*28/bh;color=source->pixels[sy*146+sx];
   if(rim(x,y)||rim(x-1,y)||rim(x+1,y)||rim(x,y-1)||rim(x,y+1))color=0x080808;
   else if(rim(x,y-1)||rim(x-1,y))color=source->pixels[6*146+73];
   else if(rim(x,y+1)||rim(x+1,y))color=source->pixels[33*146+73];
  }
  button->pixels[y*bw+x]=color;
 }
 drawStringTaggableCenter(c->font_bold12,name(profile),bw/2,16,unavailable?0xa0a0a0:0xffffff,true);
 drawStringTaggableCenter(c->font_plain12,unavailable?"Unavailable":"Tap to switch",bw/2,31,unavailable?0xa0a0a0:0xffffff,true);
 pixmap_draw(button,bx,by);_Pix2D=saved;
 owner=c;art=c->image_titlebutton;background=c->image_title3;bold=c->font_bold12;plain=c->font_plain12;generation=current;last_profile=profile;last_unavailable=unavailable;ready=1;
}
void title_theme_draw(Client*c){if(enabled(c))draw_label(c,title_theme_profile(),false);}
void title_theme_review_draw(Client*c,int profile,bool unavailable){if(getenv("CLASSIC_MIGRATION_TEST")&&enabled(c))draw_label(c,profile,unavailable);}
static bool cycle(Client*c){int p=title_theme_profile();bool saved=title_theme_set_profile(p==2?0:2);if(saved)c->redraw_background=true;return saved;}
bool title_theme_pointer(Client*c){
 if(!enabled(c)||c->shell->mouse_click_button!=1)return false;
 int x=c->shell->mouse_click_x,y=c->shell->mouse_click_y;
 if(!inside(x-bx,y-by))return false;
 cycle(c);c->shell->mouse_click_button=0;return true;
}
bool title_theme_controller(Client*c,bool blocked,bool confirm,int direction,bool pointer_activity){
 (void)direction;(void)pointer_activity;bool edge=confirm&&!held;held=confirm;
 if(!enabled(c)||blocked||!inside(c->shell->mouse_x-bx,c->shell->mouse_y-by))return false;
 if(confirm){if(edge)cycle(c);return true;}return false;
}
