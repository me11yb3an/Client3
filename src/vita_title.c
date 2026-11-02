
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "vita_title.h"
#include "vita_title_layout.h"
#include "vita_wide_layout.h"
#include "pix2d.h"
#include "pixmap.h"
#include "platform.h"
#include "datastruct/jstring.h"
extern ClientData _Client;
extern int client_title_new_player(void);

static int backing_pixel(Client *c,int x,int y,bool *opaque) {
    Pix8 *box=c->image_titlebox;
    x-=box->crop_x;y-=box->crop_y;
    int index=(x>=0 && x<box->width && y>=0 && y<box->height)?(unsigned char)box->pixels[y*box->width+x]:0;
    *opaque=index!=0;
    return box->palette[index];
}

static void reduce_control(Client *c,const ApprovedTitleControl *box,bool bar) {
    // Restore genuine titlebox pixels, not a flat patch, under the old control.
    for(int y=box->old_y;y<box->old_y+box->old_height;y++)for(int x=box->old_x;x<box->old_x+box->old_width;x++){
        bool opaque;
        int color=backing_pixel(c,approved_panel_x[x],approved_panel_y[y],&opaque);
        if(opaque)c->title_panel_scaled->pixels[(y+10)*360+x+19]=color;
    }
    for(int dy=0;dy<box->height;dy++)for(int dx=0;dx<box->width;dx++){
        int px=box->old_x+(bar?approved_bar_x[dx]:approved_button_x[dx]);
        int py=box->old_y+(bar?approved_bar_y[dy]:approved_button_y[dy]);
        c->title_panel_scaled->pixels[(box->y+dy+10)*360+box->x+dx+19]=
            c->image_title4->pixels[approved_panel_y[py]*360+approved_panel_x[px]];
    }
}

static void native_label(PixFont *font,const char *text,int center_x,int center_y,int color,bool shadow) {
    int min_y=999,max_y=-999;
    for(const char *s=text;*s;s++){
        int glyph=indexof_chr(CHARSET,(unsigned char)*s);
        if(glyph<0)glyph=74;
        if(glyph==94)continue;
        for(int y=0;y<font->charMaskHeight[glyph];y++)for(int x=0;x<font->charMaskWidth[glyph];x++)
            if(font->charMask[glyph][y*font->charMaskWidth[glyph]+x]){
                int yy=y+font->charOffsetY[glyph]-font->height;
                if(yy<min_y)min_y=yy;
                if(yy>max_y)max_y=yy;
            }
    }
    if(min_y>max_y)return;
    int baseline=(int)floor(center_y-(min_y+max_y+(shadow?1:0))/2.0+0.5);
    drawStringTaggableCenter(font,text,center_x,baseline,color,shadow);
}

#include "r20_title_cache.inc"

static void vita_title_raster_panel(Client *c,bool loading,const char *message,int progress) {
    // Graphics alone are resampled. Glyph masks are drawn at native resolution.
    pixmap_bind(c->image_title4);
    /* The approved CPU title compositor uses zero for uncovered artwork.
       GL's transparent sentinel would overwrite that recovered backing. */
    memset(c->image_title4->pixels,0,360*200*sizeof(int));
    pix8_draw(c->image_titlebox,0,0);
    if(loading){
        pix2d_draw_rect(28,62,PROGRESS_RED,304,34);
        pix2d_draw_rect(29,63,BLACK,302,32);
        pix2d_fill_rect(30,64,PROGRESS_RED,progress*3,30);
        pix2d_fill_rect(30+progress*3,64,BLACK,300-progress*3,30);
    }else if(c->title_screen_state==0 || c->title_screen_state==2){
        int y=c->title_screen_state==0?100:130;
        pix8_draw(c->image_titlebutton,27,y);
        pix8_draw(c->image_titlebutton,187,y);
    }else if(c->title_screen_state==3)pix8_draw(c->image_titlebutton,107,130);
    memcpy(c->title_panel_scaled->pixels,c->title_panel_background->pixels,360*200*sizeof(int));
    for(int dy=0;dy<APPROVED_TITLE_HEIGHT;dy++)for(int dx=0;dx<APPROVED_TITLE_WIDTH;dx++){
        int sx=approved_panel_x[dx],sy=approved_panel_y[dy];
        int color=c->image_title4->pixels[sy*360+sx];bool opaque;
        backing_pixel(c,sx,sy,&opaque);
        if(color || opaque)c->title_panel_scaled->pixels[(dy+10)*360+dx+19]=color;
    }
    if(loading)reduce_control(c,&approved_loading_bar,true);
    else if(c->title_screen_state==0 || c->title_screen_state==2){
        const ApprovedTitleControl *boxes=c->title_screen_state==0?approved_welcome_controls:approved_login_controls;
        for(int i=0;i<2;i++)reduce_control(c,&boxes[i],false);
    }else if(c->title_screen_state==3)reduce_control(c,&approved_create_control,false);

    pixmap_bind(c->title_panel_scaled);
    // Native coordinates are local to the original title tile at (299,192).
    PixFont *font=c->font_bold12;
    if(loading){
        drawStringCenter(font,181,59,"RuneScape is loading - please wait...",WHITE);
        native_label(font,message,181,81,WHITE,false);
    }else if(c->title_screen_state==0){
        drawStringTaggableCenter(font,"Welcome to RuneScape",181,82,YELLOW,true);
        native_label(font,"New user",109,118,WHITE,true);
        native_label(font,"Existing User",253,118,WHITE,true);
    }else if(c->title_screen_state==2){
        if(c->login_message0[0]){
            drawStringTaggableCenter(font,c->login_message0,181,51,YELLOW,true);
            drawStringTaggableCenter(font,c->login_message1,181,64,YELLOW,true);
        }else drawStringTaggableCenter(font,c->login_message1,181,58,YELLOW,true);
        char username[MAX_STR],password[MAX_STR];
        sprintf(username,"Username: %s%s",c->username,c->title_login_field==0 && _Client.loop_cycle%40<20?"@yel@|":"");
        drawStringTaggable(font,100,91,username,WHITE,true);
        char *censored=jstring_to_asterisk(c->password);
        sprintf(password,"Password: %s%s",censored,c->title_login_field==1 && _Client.loop_cycle%40<20?"@yel@|":"");
        free(censored);
        drawStringTaggable(font,102,105,password,WHITE,true);
        native_label(font,client_title_new_player()?"Create Player":"Login",109,145,WHITE,true);
        native_label(font,"Cancel",253,145,WHITE,true);
    }else if(c->title_screen_state==3){
        drawStringTaggableCenter(font,"Create a free account",181,46,YELLOW,true);
        drawStringTaggableCenter(font,"To create a new account you need to",181,69,WHITE,true);
        drawStringTaggableCenter(font,"go back to the main RuneScape webpage",181,82,WHITE,true);
        drawStringTaggableCenter(font,"and choose the red 'create account'",181,96,WHITE,true);
        drawStringTaggableCenter(font,"button at the top right of that page.",181,109,WHITE,true);
        native_label(font,"Cancel",181,145,WHITE,true);
    }
    pixmap_draw(c->title_panel_scaled,TITLE_X(214),TITLE_Y(186));
}

void vita_title_draw_panel(Client*c,bool loading,const char*message,int progress){
 R20TitleKey key;int cacheable=title_make_key(c,loading,message,progress,&key);
 int slot=key.caret?1:0;R20TitleEntry*entry=&title_cached[slot];
 if(cacheable&&entry->valid&&entry->pixels&&!memcmp(&key,&entry->key,sizeof key)){
  if(title_cache_active!=slot||c->title_panel_scaled->dirty){
   /* Both original blink images are retained as CPU pixels. Use the normal
    * full-surface upload, avoiding unsafe subrectangle-stride assumptions. */
   memcpy(c->title_panel_scaled->pixels,entry->pixels,360u*200u*sizeof(int));
   pixmap_bind(c->title_panel_scaled);
  }else{
   bool dirty=c->title_panel_scaled->dirty;pixmap_bind(c->title_panel_scaled);c->title_panel_scaled->dirty=dirty;
  }
  pixmap_draw(c->title_panel_scaled,TITLE_X(214),TITLE_Y(186));title_cache_active=slot;return;
 }
 vita_title_raster_panel(c,loading,message,progress);
 title_cache_active=-1;
 if(cacheable){
  if(!entry->pixels)entry->pixels=malloc(360u*200u*sizeof(int));
  if(entry->pixels){memcpy(entry->pixels,c->title_panel_scaled->pixels,360u*200u*sizeof(int));entry->key=key;entry->valid=1;title_cache_active=slot;}
  else entry->valid=0;
 }
}
