#pragma once
#include <stdint.h>
/* Original indexed lava, half the220 speed at user request. Keep V periodic and
 * leave atlas U/material identity untouched.64/128 are actual GPU rows. */
static float r93_lava_offset(uint64_t now_ms,int rows){
    if(rows!=64&&rows!=128)return 0.f;
    uint64_t period=(uint64_t)rows*200u;
    return -(float)(((now_ms%period)*7u)%period)/(float)period;
}
