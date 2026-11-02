#include <stdlib.h>

#include "pix2d.h"
#include "pixmap.h"
#include "gl11.h"

#include "ba_timing.h"
#include "draw_phases.h"
#include "r16_fast.h"
#include "ba_hw_r3.h"
#include "ba_retained.h"
#include <stdio.h>
#include <assert.h>
#if defined(__ARM_NEON)
#include <arm_neon.h>
#endif
static void overlay_convert(const uint32_t *src,uint32_t *dst,int count,int vector){
    int i=0;
#if defined(__ARM_NEON)
    if(vector){
        uint32x4_t opaque=vdupq_n_u32(0xff000000u),clear=vdupq_n_u32(0xffffffffu),zero=vdupq_n_u32(0);
        for(;i+4<=count;i+=4){
            uint32x4_t pixels=vld1q_u32(src+i);
            uint32x4_t untagged=vceqq_u32(vshrq_n_u32(pixels,24),zero);
            uint32x4_t result=vbslq_u32(untagged,vorrq_u32(pixels,opaque),pixels);
            result=vbicq_u32(result,vceqq_u32(pixels,clear));
            vst1q_u32(dst+i,result);
        }
    }
#else
    (void)vector;
#endif
    for(;i<count;i++){uint32_t rgb=src[i];dst[i]=rgb==0xffffffffu?0u:((rgb>>24)==0?rgb|0xff000000u:rgb);}
}
#if defined(__ARM_NEON)
static int overlay_vector=1;
#else
static int overlay_vector=0;
#endif
static void overlay_probe(void){
#ifdef BUILD_A_EMBEDDED
    static int tested;static uint64_t next;
    if(!tested){
        uint32_t src[259],scalar[259],vector[259];
        for(unsigned i=0;i<259;i++)src[i]=i<256?(i<<24)|0x205080u: i==256?0xffffffffu:i==257?0x00ffffffu:0u;
        overlay_convert(src,scalar,259,0);overlay_convert(src,vector,259,1);
        for(int i=0;i<259;i++)assert(scalar[i]==vector[i]);
        FILE*f=fopen("ux0:/data/RS2Vita/hardware-pixel-selftest.log","a");if(f){fprintf(f,"PASS scalar/vector exact ARGB equivalence: all 256 alpha values, transparent clear, opaque white/black, non-multiple-of-four tail\n");fclose(f);}tested=1;
    }
    /* Normal play latches this diagnostic conversion switch once. Reopening
     * its file on the render thread caused a recurring storage stall. */
    if(next&&!r16_diag_verbose())return;
    uint64_t now=ba_timing_now();if(now>=next){next=now+1000000u;int mode=overlay_vector;
        FILE*f=fopen("ux0:/data/RS2Vita/pixel-vector.txt","r");if(f){if(fscanf(f,"%d",&mode)!=1)mode=overlay_vector;fclose(f);}
        if((mode==0||mode==1)&&mode!=overlay_vector){overlay_vector=mode;f=fopen("ux0:/data/RS2Vita/pixel-vector.csv","a");if(f){fprintf(f,"%llu,%d\n",(unsigned long long)now,mode);fclose(f);}}
    }
#endif
}
#ifdef GL11
typedef struct {int w,h;uint64_t binds,dirty,checks,check_us,skipped,rects,pixels,upload_bytes,convert_us,upload_us,failures;} H6Surface;
static H6Surface h6_surfaces[128];static unsigned h6_surface_count;
static uint64_t h6_shadow_live,h6_shadow_peak;
static H6Surface*h6_surface(PixMap*p){return p->r6_metric&&p->r6_metric<=128?&h6_surfaces[p->r6_metric-1]:NULL;}
void h6_pixmap_boundary(int begin){
 if(begin){for(unsigned i=0;i<128;i++){int w=h6_surfaces[i].w,h=h6_surfaces[i].h;memset(&h6_surfaces[i],0,sizeof(H6Surface));h6_surfaces[i].w=w;h6_surfaces[i].h=h;}h6_shadow_peak=h6_shadow_live;return;}
 FILE*f=fopen("ux0:/data/RS2Vita/r6-surfaces.csv","w");if(!f)return;
 fprintf(f,"id,width,height,binds,dirty_draws,compared_bytes,compare_us,unchanged_skips,upload_rectangles,converted_pixels,upload_bytes,convert_us,upload_us,shadow_alloc_failures,shadow_live_bytes,shadow_peak_bytes,total_surface_ids\n");
 for(unsigned i=0;i<h6_surface_count&&i<128;i++){H6Surface*s=&h6_surfaces[i];fprintf(f,"%u,%d,%d,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%u\n",i+1,s->w,s->h,(unsigned long long)s->binds,(unsigned long long)s->dirty,(unsigned long long)s->checks,(unsigned long long)s->check_us,(unsigned long long)s->skipped,(unsigned long long)s->rects,(unsigned long long)s->pixels,(unsigned long long)s->upload_bytes,(unsigned long long)s->convert_us,(unsigned long long)s->upload_us,(unsigned long long)s->failures,(unsigned long long)h6_shadow_live,(unsigned long long)h6_shadow_peak,h6_surface_count);}fclose(f);
}
#else
void h6_pixmap_boundary(int begin){(void)begin;}
#endif
#ifdef GL11
static inline int next_po2(int v) {
    int p = 1;
    while (p < v) p <<= 1;
    return p;
}
#endif

#if defined(GL11) && defined(__vita__)
#include "r19_tiled_ui.inc"
#endif

PixMap *pixmap_new(int width, int height) {
    PixMap *pixmap = calloc(1, sizeof(PixMap));
    pixmap->width = width;
    pixmap->height = height;
#ifdef GL11
    pixmap->r6_metric=++h6_surface_count;H6Surface*metric=h6_surface(pixmap);if(metric){metric->w=width;metric->h=height;}
#endif
    pixmap->pixels = calloc(pixmap->width * pixmap->height, sizeof(int));
    pixmap_bind(pixmap);
#ifndef GL11
    pixmap->image = platform_create_surface(pixmap->pixels, width, height, false);
#else
    if(h5_no_init() || (h5_silent() && h5_measuring()))return pixmap;
#ifdef __vita__
    if((width==960&&height==544)||(width==683&&height==346)){
        static int tiles=-1;if(tiles<0){tiles=1;FILE*f=fopen("ux0:/data/RS2Vita/ui-tiles.txt","r");if(f){int n;if(fscanf(f,"%d",&n)==1)tiles=n!=0;fclose(f);}}
        if(tiles&&r19_tiled_new(pixmap))return pixmap;
    }
#endif
    glGenTextures(1, &pixmap->gl_texture);
    glBindTexture(GL_TEXTURE_2D, pixmap->gl_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

#ifdef __vita__
    /* vitaGL/GXM supports linear NPOT textures. Keep the logical pixel size;
       avoid allocating/copying a padded 1024x1024 backing for 960x544 UI. */
    int pot_width = width;
    int pot_height = height;
#else
    int pot_width = next_po2(width);
    int pot_height = next_po2(height);
#endif
    pixmap->u = (float)pixmap->width / pot_width;
    pixmap->v = (float)pixmap->height / pot_height;
    pixmap->gl_pixels = calloc(pot_width * pot_height, sizeof(int));
#ifdef __vita__
    /* vitaGL supports BGRA storage: same sampled RGBA colors, no callback
       conversion on every later BGRA subimage upload. */
    glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA_EXT, pot_width, pot_height, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, pixmap->gl_pixels);
#else
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pot_width, pot_height, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, pixmap->gl_pixels);
#endif
#endif
    return pixmap;
}

void pixmap_free(PixMap *pixmap) {
#if defined(GL11) && defined(__vita__)
    r19_tiled_free(pixmap);
#endif
#ifdef GL11
    if(pixmap->retained_source){h6_shadow_live-=(uint64_t)pixmap->width*pixmap->height*4;free(pixmap->retained_source);}
    free(pixmap->gl_pixels);
    if(!h5_no_init() && pixmap->gl_texture)glDeleteTextures(1, &pixmap->gl_texture);
#else
    platform_free_surface(pixmap->image);
#endif
    free(pixmap->pixels);
    free(pixmap);
}

void pixmap_bind(PixMap *pixmap) {
#ifdef GL11
    pixmap->dirty = true;
    H6Surface*metric=h6_surface(pixmap);if(metric&&h5_measuring())metric->binds++;
#endif
    pix2d_bind(pixmap->width, pixmap->height, pixmap->pixels);
}

void pixmap_flush_rect(PixMap *pixmap, int x, int y, int w, int h) {
#ifdef GL11
#ifdef __vita__
    if(pixmap&&pixmap->ui_tile_count){r19_tiled_flush(pixmap,x,y,w,h);return;}
#endif
    if (!pixmap || !pixmap->pixels || !pixmap->gl_pixels || pixmap->width <= 0 || pixmap->height <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x >= pixmap->width || y >= pixmap->height) return;
    if (x + w > pixmap->width) w = pixmap->width - x;
    if (y + h > pixmap->height) h = pixmap->height - y;
    if (w <= 0 || h <= 0) return;
    if (x == 0 && w != pixmap->width) w = pixmap->width;
    overlay_probe();
    dp_begin(DP_PIX_CONVERT);
    for (int row = y; row < y + h; row++) {
        size_t off = (size_t)row * (size_t)pixmap->width + (size_t)x;
        overlay_convert((const uint32_t *)pixmap->pixels + off, (uint32_t *)pixmap->gl_pixels + off, w, overlay_vector);
        if (pixmap->retained_source && pixmap->retained_valid)
            memcpy(pixmap->retained_source + off, (const uint32_t *)pixmap->pixels + off, (size_t)w * 4u);
    }
    dp_end();
    dp_begin(DP_PIX_UPLOAD);
    dp_mark_upload();
    glBindTexture(GL_TEXTURE_2D, pixmap->gl_texture);
    if (w != pixmap->width) glPixelStorei(GL_UNPACK_ROW_LENGTH, pixmap->width);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, pixmap->gl_pixels + (size_t)y * (size_t)pixmap->width + (size_t)x);
    if (w != pixmap->width) glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    dp_end();
#else
    (void)pixmap;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
#endif
}

#ifdef GL11
#include "r19_region_diff.inc"
#endif
int pixmap_flush_changed_rect(PixMap*p,int x,int y,int w,int h){
#ifdef GL11
 if(!p||!p->pixels||!p->retained_source||!p->retained_valid)return 0;
 H6Rect r;dp_begin(DP_PIX_COMPARE);
 int changed=r19_changed_region((const uint32_t*)p->retained_source,(const uint32_t*)p->pixels,p->width,p->height,x,y,w,h,&r);
 dp_end();if(changed)pixmap_flush_rect(p,r.x,r.y,r.w,r.h);
 return changed;
#else
 (void)p;(void)x;(void)y;(void)w;(void)h;return 0;
#endif
}

void pixmap_draw(PixMap *pixmap, int x, int y) {
    if(h5_silent())return;
#ifdef GL11
#ifdef __vita__
    x += SCREEN_CENTER_XOFF;
#endif

    dp_begin(DP_PIX_OTHER);
#ifdef __vita__
    if(pixmap->ui_tile_count){r19_tiled_draw(pixmap,x,y);dp_end();return;}
#endif
    glBindTexture(GL_TEXTURE_2D, pixmap->gl_texture);
    if (pixmap->dirty) {
        // rs2_log("dirty pixmap id %d width %d height %d\n", pixmap->gl_texture, pixmap->width, pixmap->height);
        pixmap->dirty = false;

        overlay_probe();
        H6Surface*metric=h6_surface(pixmap);int measured=h5_measuring(),policy=h6_ui_policy();
        if(metric&&measured)metric->dirty++;
        H6Rect rect[32];unsigned count=1;rect[0]=(H6Rect){0,0,pixmap->width,pixmap->height};
        if(policy){
            if(!pixmap->retained_source){
                pixmap->retained_source=malloc((size_t)pixmap->width*pixmap->height*4);
                if(pixmap->retained_source){h6_shadow_live+=(uint64_t)pixmap->width*pixmap->height*4;if(h6_shadow_live>h6_shadow_peak)h6_shadow_peak=h6_shadow_live;}
                else if(metric&&measured)metric->failures++;
            }
            if(pixmap->retained_source){dp_begin(DP_PIX_COMPARE);uint64_t checked=0,at=ba_timing_now();count=h6_rectangles(pixmap->retained_source,(const uint32_t*)pixmap->pixels,pixmap->width,pixmap->height,pixmap->retained_valid,policy==2,rect,&checked);dp_end();if(metric&&measured){metric->checks+=checked;metric->check_us+=ba_timing_now()-at;if(!count)metric->skipped++;}}
        }
        for(unsigned i=0;i<count;i++){
            H6Rect v=rect[i];dp_begin(DP_PIX_CONVERT);uint64_t converted_at=ba_timing_now();
            {H3_SCOPE(H3_CONVERT);
              if(v.x==0&&v.y==0&&v.w==pixmap->width&&v.h==pixmap->height){
                overlay_convert((const uint32_t*)pixmap->pixels,(uint32_t*)pixmap->gl_pixels,pixmap->width*pixmap->height,overlay_vector);
                if(pixmap->retained_source)memcpy(pixmap->retained_source,pixmap->pixels,(size_t)pixmap->width*pixmap->height*4);
              }else{
                for(int row=v.y;row<v.y+v.h;row++){
                    size_t off=(size_t)row*pixmap->width+v.x;
                    overlay_convert((const uint32_t*)pixmap->pixels+off,(uint32_t*)pixmap->gl_pixels+off,v.w,overlay_vector);
                    if(pixmap->retained_source)memcpy(pixmap->retained_source+off,pixmap->pixels+off,(size_t)v.w*4);
                }
              }
            }
            uint64_t converted=ba_timing_now()-converted_at;ba_timing_record(BA_PIXEL_CONVERT,converted);dp_end();
            dp_begin(DP_PIX_UPLOAD);dp_mark_upload();uint64_t uploaded_at=ba_timing_now();
            {H3_SCOPE(H3_UPLOAD);
                int partial=v.w!=pixmap->width;
                if(partial)glPixelStorei(GL_UNPACK_ROW_LENGTH,pixmap->width);
                glTexSubImage2D(GL_TEXTURE_2D,0,v.x,v.y,v.w,v.h,GL_BGRA_EXT,GL_UNSIGNED_BYTE,pixmap->gl_pixels+(size_t)v.y*pixmap->width+v.x);
                if(partial)glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
            }
            uint64_t uploaded=ba_timing_now()-uploaded_at;ba_timing_record(BA_PIXEL_UPLOAD,uploaded);dp_end();
            if(metric&&measured){metric->rects++;metric->pixels+=(uint64_t)v.w*v.h;metric->upload_bytes+=(uint64_t)v.w*v.h*4;metric->convert_us+=converted;metric->upload_us+=uploaded;}
        }
        if(count&&pixmap->retained_source)pixmap->retained_valid=true;
    }

    dp_begin(DP_PIX_QUAD);
    glColor4ub(0xff, 0xff, 0xff, 0xff);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2i(x, y);
    glTexCoord2f(0, pixmap->v); glVertex2i(x, y + pixmap->height);
    glTexCoord2f(pixmap->u, pixmap->v); glVertex2i(x + pixmap->width, y + pixmap->height);
    glTexCoord2f(pixmap->u, 0); glVertex2i(x + pixmap->width, y);
    glEnd();
    dp_end();
    dp_end();
#else
    platform_blit_surface(pixmap->image, x, y);
#endif
}
