#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
/* Immutable decompressed map bytes only. Terrain placement, live locations,
 * collision, shading and roof ownership are rebuilt through their original
 * paths. Exact compressed-source comparison rejects stale/different assets.
 * All allocations are owned here, outside the scene's resettable allocator. */
#define R109_SLOTS 24
#define R109_BUDGET (2u*1024u*1024u)
#ifndef R109_ALLOC
#define R109_ALLOC malloc
#define R109_FREE free
#endif
typedef struct {uint32_t key;size_t source_n,decoded_n;unsigned char*bytes;uint64_t age;}R109Map;
static R109Map r109_maps[R109_SLOTS];
static size_t r109_bytes,r109_peak;
static uint64_t r109_age,r109_hits,r109_misses,r109_evictions;
static void r109_drop(unsigned i){R109Map*m=r109_maps+i;if(m->bytes){r109_bytes-=m->source_n+m->decoded_n;R109_FREE(m->bytes);}memset(m,0,sizeof(*m));}
static void r109_clear(void){for(unsigned i=0;i<R109_SLOTS;i++)r109_drop(i);}
static unsigned r109_oldest(void){unsigned at=0;for(unsigned i=1;i<R109_SLOTS;i++)if(r109_maps[i].age<r109_maps[at].age)at=i;return at;}
static int r109_get(uint32_t key,const void*source,size_t source_n,void*out,size_t decoded_n){
 if(!source||!out||!source_n||!decoded_n||source_n>R109_BUDGET||decoded_n>R109_BUDGET-source_n)return 0;
 for(unsigned i=0;i<R109_SLOTS;i++){
  R109Map*m=r109_maps+i;
  if(m->bytes&&m->key==key&&m->source_n==source_n&&m->decoded_n==decoded_n&&!memcmp(m->bytes,source,source_n)){
   memcpy(out,m->bytes+source_n,decoded_n);m->age=++r109_age;r109_hits++;return 1;
  }
 }
 r109_misses++;return 0;
}
static void r109_put(uint32_t key,const void*source,size_t source_n,const void*out,size_t decoded_n){
 if(!source||!out||!source_n||!decoded_n||source_n>R109_BUDGET||decoded_n>R109_BUDGET-source_n)return;
 unsigned slot=R109_SLOTS;
 for(unsigned i=0;i<R109_SLOTS;i++)if(r109_maps[i].bytes&&r109_maps[i].key==key){slot=i;break;}
 if(slot==R109_SLOTS)for(unsigned i=0;i<R109_SLOTS;i++)if(!r109_maps[i].bytes){slot=i;break;}
 if(slot==R109_SLOTS){slot=r109_oldest();r109_evictions++;}
 r109_drop(slot);size_t n=source_n+decoded_n;
 while(r109_bytes>R109_BUDGET-n){unsigned at=r109_oldest();
  /* Empty records have age0; choose the oldest occupied record. */
  uint64_t age=UINT64_MAX;for(unsigned i=0;i<R109_SLOTS;i++)if(r109_maps[i].bytes&&r109_maps[i].age<age){age=r109_maps[i].age;at=i;}
  r109_drop(at);r109_evictions++;
 }
 unsigned char*copy=R109_ALLOC(n);if(!copy)return;
 memcpy(copy,source,source_n);memcpy(copy+source_n,out,decoded_n);
 r109_maps[slot]=(R109Map){key,source_n,decoded_n,copy,++r109_age};r109_bytes+=n;if(r109_bytes>r109_peak)r109_peak=r109_bytes;
}
