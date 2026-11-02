#pragma once
#include <stdint.h>
/* Exact truncating integer-division interval, evaluated without division.
 * low/high refer to the projected coordinate relative to the centre. */
static int r83_quotient_inside(int numerator,int depth,int64_t low,int64_t high){
 int64_t lower=low>0?low*depth:(low-1)*depth+1;
 int64_t upper=high<0?high*depth:(high+1)*depth-1;
 return numerator>=lower&&numerator<=upper;
}
static int r83_screen_inside(int nx,int ny,int depth,int cx,int cy,int mx,int my,int rx,int ry){
 return r83_quotient_inside(nx,depth,(int64_t)mx-cx-rx,(int64_t)mx-cx+rx)&&r83_quotient_inside(ny,depth,(int64_t)my-cy-ry,(int64_t)my-cy+ry);
}
/* Conservative precheck for the existing variable-radius model pick box.
 * Final acceptance still uses the original projected coordinate and reach.
 * Integer truncation can move either axis by <one pixel, hence65 not64. */
static int r83_model_maybe(int nx,int ny,int depth,int cx,int cy,int mx,int my,int radius){
 int r=radius>0?radius:64;if(r>4194303)return 1;
 int64_t bound=(int64_t)65*depth+(int64_t)r*512;
 int64_t x=(int64_t)nx+((int64_t)cx-mx)*depth,y=(int64_t)ny+((int64_t)cy-my)*depth;
 return x>=-bound&&x<=bound&&y>=-bound&&y<=bound;
}
