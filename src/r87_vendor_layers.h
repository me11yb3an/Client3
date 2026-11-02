#pragma once
#include <stdint.h>
/* Fifteen configured stall aliases,38 packed placements. The unrelated
 * market door and minimap symbols are excluded. Texture22 is the verified
 * fully opaque tabletop in all eight textured stall base models. */
static int r87_vendor_type(int id){return (id>=628&&id<=635)||id==1151||(id>=2560&&id<=2565);}
#define R87_VENDOR_BACKING_TEXTURES (UINT64_C(1)<<22)
