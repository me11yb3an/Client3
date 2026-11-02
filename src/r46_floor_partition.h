#pragma once
#include <stdint.h>
/* Preserve every triangle and its winding. Isolate opaque terrain so only
 * that range can occlude from below; actors, walls, glass and cutout models
 * keep their normal culling. Paving keeps its existing filtered texture. */
static int r46_partition_floors(const Vertex*v,int nv,const uint32_t*src,int n,
 const uint8_t*paving,const uint8_t*floor,uint16_t*out,int capacity,int counts[5]){
 if(n<0||nv<0||nv>65535||n>capacity||n>65535||n%3)return 0;
 static uint8_t kinds[65535/3];int sizes[5]={0};
 for(int i=0;i<n;i+=3){
  uint32_t a=src[i],b=src[i+1],c=src[i+2];
  if(a>=(uint32_t)nv||b>=(uint32_t)nv||c>=(uint32_t)nv)return 0;
  int alpha=v[a].a!=255||v[b].a!=255||v[c].a!=255;
  int filtered=paving[a]&&paving[b]&&paving[c];
  int upper=floor[a]&&floor[b]&&floor[c];
  int kind=alpha?4:(upper?(filtered?3:2):(filtered?1:0));
  kinds[i/3]=(uint8_t)kind;sizes[kind]+=3;
 }
 int cursor[5]={0,sizes[0],sizes[0]+sizes[1],sizes[0]+sizes[1]+sizes[2],sizes[0]+sizes[1]+sizes[2]+sizes[3]};
 for(int i=0;i<n;i+=3){int at=cursor[kinds[i/3]];for(int k=0;k<3;k++)out[at+k]=(uint16_t)src[i+k];cursor[kinds[i/3]]+=3;}
 for(int k=0;k<5;k++)counts[k]=sizes[k];return 1;
}
