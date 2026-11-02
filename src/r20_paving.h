#pragma once
#include <stdint.h>
/* Native floor capture identifies the courtyard cobblestones as texture46.
 * Classification runs only over vertices emitted by the terrain builder. */
#define R20_PAVING_TEXTURE 46
#define R20_ATLAS_SLOTS 64
static void r20_mark_paving(const Vertex*v,int first,int end,uint8_t*tags,int capacity){
 if(first<0||end<first||end>capacity)return;
 const float left=(R20_PAVING_TEXTURE+1.f)/R20_ATLAS_SLOTS;
 const float right=(R20_PAVING_TEXTURE+2.f)/R20_ATLAS_SLOTS;
 for(int i=first;i<end;i++)if(v[i].a==255&&v[i].u>=left&&v[i].u<right&&v[i].v>=0.f&&v[i].v<=1.f)tags[i]=1;
}
/* Partition existing indices, never duplicate, alter or drop geometry. The
 * glass range follows both opaque ranges, including when no paving is present. */
static int r20_partition_large(const Vertex*v,int nv,const uint32_t*src,int n,const uint8_t*tags,uint16_t*out,int capacity,int*ordinary,int*paving,int*blend){
 if(n<0||nv<0||nv>65535||n>capacity||n%3)return 0;
 for(int i=0;i<n;i++)if(src[i]>=(uint32_t)nv)return 0;
 int at=0,counts[3]={0};
 for(int pass=0;pass<3;pass++)for(int i=0;i<n;i+=3){
  uint32_t a=src[i],b=src[i+1],c=src[i+2];
  int alpha=v[a].a!=255||v[b].a!=255||v[c].a!=255;
  int material=alpha?2:(tags[a]&&tags[b]&&tags[c]?1:0);
  if(material!=pass)continue;
  out[at++]=(uint16_t)a;out[at++]=(uint16_t)b;out[at++]=(uint16_t)c;counts[pass]+=3;
 }
 *ordinary=counts[0];*paving=counts[1];*blend=counts[2];return at==n;
}

/* Classify each triangle once, then scatter stably into the same three
 * ranges. Large external callers retain the original bounded-work fallback. */
static int r20_partition_indices(const Vertex*v,int nv,const uint32_t*src,int n,const uint8_t*tags,uint16_t*out,int capacity,int*ordinary,int*paving,int*blend){
 if(n>65535)return r20_partition_large(v,nv,src,n,tags,out,capacity,ordinary,paving,blend);
 if(n<0||nv<0||nv>65535||n>capacity||n%3)return 0;
 static uint8_t material[65535/3];int counts[3]={0};
 for(int i=0;i<n;i+=3){
  uint32_t a=src[i],b=src[i+1],c=src[i+2];if(a>=(uint32_t)nv||b>=(uint32_t)nv||c>=(uint32_t)nv)return 0;
  int alpha=v[a].a!=255||v[b].a!=255||v[c].a!=255;
  int kind=alpha?2:(tags[a]&&tags[b]&&tags[c]?1:0);material[i/3]=(uint8_t)kind;counts[kind]+=3;
 }
 int cursor[3]={0,counts[0],counts[0]+counts[1]};
 for(int i=0;i<n;i+=3){int kind=material[i/3],at=cursor[kind];out[at]=(uint16_t)src[i];out[at+1]=(uint16_t)src[i+1];out[at+2]=(uint16_t)src[i+2];cursor[kind]+=3;}
 *ordinary=counts[0];*paving=counts[1];*blend=counts[2];return 1;
}
