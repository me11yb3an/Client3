/* Same logical surface and exact pixels, bounded smaller GPU storage. Updating
 * a portrait must not make vitaGL rename/copy the complete 960x544 texture. */
#include "r80_exact_words.h"
typedef struct R19UiTile {
    int x,y,w,h;
    uint32_t texture;
    uint32_t *pixels;
} R19UiTile;

static void r19_tiled_free(PixMap*p){
    for(int i=0;i<p->ui_tile_count;i++){
        R19UiTile*t=p->ui_tiles+i;
        if(t->texture)glDeleteTextures(1,&t->texture);
        free(t->pixels);
    }
    free(p->ui_tiles);p->ui_tiles=NULL;p->ui_tile_count=0;
}

static int r19_tiled_new(PixMap*p){
    if(!((p->width==960&&p->height==544)||(p->width==683&&p->height==346)))return 0;
    p->ui_tiles=calloc(20,sizeof(R19UiTile));if(!p->ui_tiles)return 0;
    for(int y=0;y<p->height;y+=128)for(int x=0;x<p->width;x+=256){
        R19UiTile*t=p->ui_tiles+p->ui_tile_count++;
        t->x=x;t->y=y;t->w=p->width-x<256?p->width-x:256;t->h=p->height-y<128?p->height-y:128;
        t->pixels=calloc((size_t)t->w*t->h,4);
        if(!t->pixels){r19_tiled_free(p);return 0;}
        glGenTextures(1,&t->texture);
        if(!t->texture){r19_tiled_free(p);return 0;}
        glBindTexture(GL_TEXTURE_2D,t->texture);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D,0,GL_BGRA_EXT,t->w,t->h,0,GL_BGRA_EXT,GL_UNSIGNED_BYTE,t->pixels);
    }
    p->u=p->v=1;return 1;
}

/* Both layouts compare the same exact raw pixels, including unpadded rows.
 * Reuse the bounded native-self-tested word comparator for Enhanced too. */
static int r20_ui_row_equal(const uint32_t *a,const uint32_t *b,int n){
    return r80_words_equal((const int*)a,(const int*)b,(size_t)n);
}

static void r19_tiled_flush(PixMap*p,int x,int y,int w,int h){
    if(x<0){w+=x;x=0;}if(y<0){h+=y;y=0;}
    if(x>=p->width||y>=p->height||w<=0||h<=0)return;
    if(w>p->width-x)w=p->width-x;if(h>p->height-y)h=p->height-y;
    if(!p->retained_source){
        p->retained_source=malloc((size_t)p->width*p->height*4);
        if(p->retained_source){h6_shadow_live+=(uint64_t)p->width*p->height*4;if(h6_shadow_live>h6_shadow_peak)h6_shadow_peak=h6_shadow_live;}
    }
    overlay_probe();
    for(int i=0;i<p->ui_tile_count;i++){
        R19UiTile*t=p->ui_tiles+i;
        int l=x>t->x?x:t->x,top=y>t->y?y:t->y;
        int right=x+w<t->x+t->w?x+w:t->x+t->w,bottom=y+h<t->y+t->h?y+h:t->y+t->h;
        if(l>=right||top>=bottom)continue;
        int changed=!p->retained_source||!p->retained_valid;
        dp_begin(DP_PIX_COMPARE);
        for(int row=top;!changed&&row<bottom;row++){
            size_t at=(size_t)row*p->width+l;
            if(!r20_ui_row_equal(p->retained_source+at,(const uint32_t*)p->pixels+at,right-l))changed=1;
        }
        dp_end();if(!changed)continue;
        dp_begin(DP_PIX_CONVERT);
        for(int row=top;row<bottom;row++){
            size_t at=(size_t)row*p->width+l;
            overlay_convert((uint32_t*)p->pixels+at,t->pixels+(size_t)(row-t->y)*t->w+l-t->x,right-l,overlay_vector);
            if(p->retained_source)memcpy(p->retained_source+at,p->pixels+at,(size_t)(right-l)*4);
        }
        dp_end();dp_begin(DP_PIX_UPLOAD);dp_mark_upload();
        glBindTexture(GL_TEXTURE_2D,t->texture);
        /* Compact full-tile rows also avoid the driver's x=0 partial-stride path. */
        glTexSubImage2D(GL_TEXTURE_2D,0,0,0,t->w,t->h,GL_BGRA_EXT,GL_UNSIGNED_BYTE,t->pixels);
        dp_end();
    }
    if(x==0&&y==0&&w==p->width&&h==p->height&&p->retained_source)p->retained_valid=true;
}

static void r19_tiled_draw(PixMap*p,int x,int y){
    if(p->dirty){p->dirty=false;r19_tiled_flush(p,0,0,p->width,p->height);}
    dp_begin(DP_PIX_QUAD);glColor4ub(255,255,255,255);
    for(int i=0;i<p->ui_tile_count;i++){
        R19UiTile*t=p->ui_tiles+i;glBindTexture(GL_TEXTURE_2D,t->texture);
        int l=x+t->x,top=y+t->y;
        glBegin(GL_QUADS);
        glTexCoord2f(0,0);glVertex2i(l,top);
        glTexCoord2f(0,1);glVertex2i(l,top+t->h);
        glTexCoord2f(1,1);glVertex2i(l+t->w,top+t->h);
        glTexCoord2f(1,0);glVertex2i(l+t->w,top);
        glEnd();
    }
    dp_end();
}
