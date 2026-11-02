#pragma once

#include "platform.h"

struct PixMap {
    int width;
    int height;
    int *pixels;
#ifndef GL11
    Surface *image;
#else
    float u;
    float v;
    int *gl_pixels;
    uint32_t gl_texture;
    bool dirty;
    uint32_t *retained_source;
    bool retained_valid;
    unsigned r6_metric;
    struct R19UiTile *ui_tiles;
    int ui_tile_count;
#endif
};

PixMap *pixmap_new(int width, int height);
void pixmap_free(PixMap *pixmap);
void pixmap_bind(PixMap *pixmap);
void pixmap_draw(PixMap *pixmap, int x, int y);
/* Upload one already-drawn rectangle. Skips the full-frame compare.
 * x == 0 is widened to the full row because vitaGL's fast store ignores a
 * partial row stride in that case. */
void pixmap_flush_rect(PixMap *pixmap, int x, int y, int w, int h);

int pixmap_flush_changed_rect(PixMap *pixmap,int x,int y,int w,int h);
