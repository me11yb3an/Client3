#ifndef R10_COMPACT_H
#define R10_COMPACT_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
/* Exact first-use compaction. No draw reordering, quantization or hash tests.
 * High-bit retained-chunk indices are preserved. Transactional on failure. */
static inline int r10_compact_stream(void *vertices,size_t stride,unsigned *nv,
 uint32_t *indices,unsigned ni,size_t scratch_budget){
 if(!*nv||!ni)return 1;
 if(!vertices||!indices||!stride||*nv>SIZE_MAX/(stride+sizeof(uint32_t)))return 0;
 size_t bytes=(size_t)*nv*(stride+sizeof(uint32_t));if(bytes>scratch_budget)return 0;
 for(unsigned k=0;k<ni;k++)if(!(indices[k]&0x80000000u)&&indices[k]>=*nv)return 0;
 uint32_t *map=malloc((size_t)*nv*sizeof(uint32_t));unsigned char *out=malloc((size_t)*nv*stride);
 if(!map||!out){free(map);free(out);return 0;}
 for(unsigned k=0;k<*nv;k++)map[k]=UINT32_MAX;
 unsigned count=0;
 for(unsigned k=0;k<ni;k++)if(!(indices[k]&0x80000000u)){
  unsigned at=indices[k];if(map[at]==UINT32_MAX){map[at]=count;memcpy(out+(size_t)count*stride,(unsigned char*)vertices+(size_t)at*stride,stride);count++;}
 }
 for(unsigned k=0;k<ni;k++)if(!(indices[k]&0x80000000u))indices[k]=map[indices[k]];
 memcpy(vertices,out,(size_t)count*stride);*nv=count;free(map);free(out);return 1;
}
#endif
