#pragma once
#include <stdint.h>
/* One VFP reciprocal for two screen coordinates. The integer remainder
 * proves the quotient; an inaccurate estimate falls back to exact division.
 * Keep signed C truncation and all original projection pixels unchanged. */
static int r82_quotient(int n,int d,float reciprocal){
 uint32_t magnitude=n<0?0u-(uint32_t)n:(uint32_t)n;
 uint32_t q=(uint32_t)((float)magnitude*reciprocal);
 uint64_t product=(uint64_t)q*(unsigned)d;
 if(product>magnitude){if(!q)return n/d;q--;product-=d;}
 else if((uint64_t)magnitude-product>=(unsigned)d){q++;product+=d;}
 if(product>magnitude||(uint64_t)magnitude-product>=(unsigned)d)return n/d;
 return n<0?-(int)q:(int)q;
}
static void r82_project_divide(int x,int y,int depth,int*outx,int*outy){
 if(depth<50||depth>65535){*outx=x/depth;*outy=y/depth;return;}
 float reciprocal=1.0f/(float)depth;
 *outx=r82_quotient(x,depth,reciprocal);*outy=r82_quotient(y,depth,reciprocal);
}
