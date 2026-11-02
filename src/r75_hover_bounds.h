#pragma once
#include <stdint.h>
#include "r82_project_divide.h"
/* Reject only a pointer outside the projection of the complete model box.
 * Near-plane crossings/unknown bounds keep the original picker. Pickable
 * actors keep their original coarse-box selection semantics at the caller. */
static int r75_hover_bounds(int radius,int above,int below,int x,int y,int z,
 int sy,int cy,int sp,int cp,int cx,int centre_y,int mx,int my){
 if(radius<0||radius>8192||above<0||below<0||above>8192||below>8192||x< -16384||x>16384||y< -16384||y>16384||z< -16384||z>16384)return 1;
 int lo_x=0x7fffffff,hi_x=-0x7fffffff,lo_y=0x7fffffff,hi_y=-0x7fffffff;
 for(int i=0;i<8;i++){
  int64_t xx=(int64_t)x+((i&1)?radius+2:-radius-2),yy=(int64_t)y+((i&2)?below+2:-above-2),zz=(int64_t)z+((i&4)?radius+2:-radius-2);
  int64_t horizontal=(zz*cy-xx*sy)>>16,sx=(zz*sy+xx*cy)>>16;
  int64_t depth=(yy*sp+horizontal*cp)>>16,vertical=(yy*cp-horizontal*sp)>>16;
  if(depth<50)return 1;
  int64_t nx=sx*512,ny=vertical*512;
  if(nx<INT32_MIN||nx>INT32_MAX||ny<INT32_MIN||ny>INT32_MAX||depth>INT32_MAX)return 1;
  int px,py;r82_project_divide((int)nx,(int)ny,(int)depth,&px,&py);px+=cx;py+=centre_y;
  if(px<lo_x)lo_x=px;if(px>hi_x)hi_x=px;if(py<lo_y)lo_y=py;if(py>hi_y)hi_y=py;
 }
 return mx>=lo_x-2&&mx<=hi_x+2&&my>=lo_y-2&&my<=hi_y+2;
}
