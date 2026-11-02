#pragma once

// Pure presentation transform: logical game dimensions never change.
typedef struct {
    double scale;
    int x, y, width, height;
    int game_width, game_height;
} VitaAspectViewport;

static inline VitaAspectViewport vita_aspect_viewport(int sw, int sh, int gw, int gh) {
    double sx = (double)sw / gw, sy = (double)sh / gh;
    VitaAspectViewport v;
    v.scale = sx < sy ? sx : sy;
    v.width = (int)(gw * v.scale + 0.5);
    v.height = (int)(gh * v.scale + 0.5);
    v.x = (sw - v.width) / 2;
    v.y = (sh - v.height) / 2;
    v.game_width = gw;
    v.game_height = gh;
    return v;
}

// Pixel-center nearest-neighbor inverse; use exactly this for display and touch.
static inline int vita_aspect_axis(int pixel, int origin, int length, int game_length, double scale) {
    if (pixel < origin || pixel >= origin + length) return -1;
    int source = (int)(((double)(pixel - origin) + 0.5) / scale);
    return source < game_length ? source : game_length - 1;
}
