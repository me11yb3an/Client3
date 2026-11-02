#pragma once
#include <stdint.h>
#include <string.h>
/* Exact source-pixel comparisons, no hashes. Bounded vertical bands limit GL calls.
 * Caller retains the complete converted backing and a last-uploaded source copy. */
typedef struct {int x,y,w,h;} H6Rect;
static inline unsigned h6_rectangles(const uint32_t*previous,const uint32_t*current,int w,int h,int valid,int regions,H6Rect out[32],uint64_t*checked){
 if(!valid){out[0]=(H6Rect){0,0,w,h};return 1;}
 unsigned n=0;
 for(int by=0;by<h;by+=64){
  int left=w,right=0,top=h,bottom=0,end=by+64<h?by+64:h;
  for(int y=by;y<end;y++){
   const uint32_t*a=previous+(size_t)y*w,*b=current+(size_t)y*w;*checked+=(uint64_t)w*4;
   if(!memcmp(a,b,(size_t)w*4))continue;
   if(!regions){out[0]=(H6Rect){0,0,w,h};return 1;}
   int x=0,z=w;
   while(x<z&&a[x]==b[x])x++;
   while(z>x&&a[z-1]==b[z-1])z--;
   if(x<left)left=x;if(z>right)right=z;if(y<top)top=y;bottom=y+1;
  }
  if(right>left){
   /* Pinned vitaGL's fast-store path ignores source row stride for x=0
    * partial-width rectangles. Full-width bands keep that path correct. */
   if(left==0)right=w;
   if(n==32){out[0]=(H6Rect){0,0,w,h};return 1;}
   out[n++]=(H6Rect){left,top,right-left,bottom-top};
  }
 }
 uint64_t pixels=0;for(unsigned i=0;i<n;i++)pixels+=(uint64_t)out[i].w*out[i].h;
 if(pixels>(uint64_t)w*h*3/4){out[0]=(H6Rect){0,0,w,h};return 1;}
 return n;
}
