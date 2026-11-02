#pragma once
#include <math.h>

typedef struct { float x, y; } R16Point2;

static float r16_area2(const R16Point2 *p, int n) {
    float a = 0;
    for (int i = 0; i < n; i++) {
        int j = i ? i - 1 : n - 1;
        a += p[j].x * p[i].y - p[i].x * p[j].y;
    }
    return a;
}

/* Triangle intersection in a local plane. Used only while a mesh is baked. */
static float r16_triangle_overlap(const R16Point2 a[3], const R16Point2 b[3]) {
    R16Point2 input[8], output[8];
    int n = 3;
    for (int i = 0; i < 3; i++) input[i] = a[i];
    float orientation = r16_area2(b, 3) >= 0 ? 1 : -1;
    for (int edge = 0; edge < 3 && n; edge++) {
        R16Point2 start = b[(edge + 2) % 3], end = b[edge];
        int count = 0;
        for (int i = 0; i < n; i++) {
            R16Point2 p = input[i], q = input[i ? i - 1 : n - 1];
            float dp = orientation * ((end.x-start.x)*(p.y-start.y) - (end.y-start.y)*(p.x-start.x));
            float dq = orientation * ((end.x-start.x)*(q.y-start.y) - (end.y-start.y)*(q.x-start.x));
            if ((dp >= 0) != (dq >= 0)) {
                float t = dq / (dq - dp);
                output[count++] = (R16Point2){q.x + t*(p.x-q.x), q.y + t*(p.y-q.y)};
            }
            if (dp >= 0) output[count++] = p;
        }
        n = count;
        for (int i = 0; i < n; i++) input[i] = output[i];
    }
    return n >= 3 ? fabsf(r16_area2(input, n)) * .5f : 0;
}
