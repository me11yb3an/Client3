#pragma once
/* Exclusive client_draw partitions. Nested begins move time out of the
 * open bucket, so a parent total is not added to its children. */
enum {
    DP_SETUP, DP_CLEAR, DP_PRESCENE, DP_ENTITY, DP_ORBIT, DP_WORLD,
    DP_GL_PREP, DP_GL_SUBMIT, DP_GL_TAIL, DP_POST,
    DP_CHAT, DP_OVCLEAR, DP_OV2D, DP_OVHINT, DP_OV3D, DP_MINIMAP, DP_COLDUI, DP_HUD,
    DP_HOLDIO,
    DP_PIX_OTHER, DP_PIX_COMPARE, DP_PIX_CONVERT, DP_PIX_UPLOAD, DP_PIX_QUAD,
    DP_TAIL, DP_N
};
void dp_switch(int bucket);
void dp_begin(int bucket);
void dp_end(void);
void dp_mark_overlay(void);
void dp_mark_map(void);
void dp_mark_upload(void);
