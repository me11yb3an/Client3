#pragma once
#include <stdint.h>
#include <string.h>
#define R32_SIDE 104
#define R32_LEVELS 4
#define R32_APPROACH_MARGIN 2
typedef struct {
 uint16_t roof[R32_LEVELS][R32_SIDE][R32_SIDE];
 uint16_t owner[R32_LEVELS][R32_SIDE][R32_SIDE];
 unsigned components;
} R32RoofZones;
static int r32_in_map(int x,int z){return x>=0&&z>=0&&x<R32_SIDE&&z<R32_SIDE;}
static uint16_t r32_tile_tag(int level,int x,int z){
 return level>=0&&level<R32_LEVELS&&r32_in_map(x,z)?(uint16_t)(x|(z<<7)|(level<<14)):65535u;
}
static void r32_zones_build(R32RoofZones*out,const int8_t flags[R32_LEVELS][R32_SIDE][R32_SIDE]){
 memset(out,0,sizeof(*out));if(!flags)return;
 uint16_t queue[R32_SIDE*R32_SIDE];const int dx[]={-1,1,0,0},dz[]={0,0,-1,1};
 for(int level=0;level<R32_LEVELS;level++)for(int x=0;x<R32_SIDE;x++)for(int z=0;z<R32_SIDE;z++){
  if(!(flags[level][x][z]&4)||out->roof[level][x][z])continue;
  uint16_t id=(uint16_t)++out->components;int read=0,write=0;
  out->roof[level][x][z]=id;queue[write++]=(uint16_t)(x|(z<<7));
  while(read<write){
   unsigned tile=queue[read++];int px=tile&127,pz=tile>>7;
   for(int d=0;d<4;d++){
    int nx=px+dx[d],nz=pz+dz[d];
    if(!r32_in_map(nx,nz)||!(flags[level][nx][nz]&4)||out->roof[level][nx][nz])continue;
    out->roof[level][nx][nz]=id;queue[write++]=(uint16_t)(nx|(nz<<7));
   }
  }
 }
 /* An upper wall/window can be anchored just beyond its roof floor tile.
  * Give that one-tile edge to its unique nearest roof, never both roofs.
  * Ambiguous equally-near edges stay visible rather than hiding a neighbor. */
 for(int level=0;level<R32_LEVELS;level++)for(int x=0;x<R32_SIDE;x++)for(int z=0;z<R32_SIDE;z++){
  uint16_t id=out->roof[level][x][z];if(id){out->owner[level][x][z]=id;continue;}
  int best=9,ambiguous=0;
  for(int ox=-1;ox<=1;ox++)for(int oz=-1;oz<=1;oz++){
   int nx=x+ox,nz=z+oz;if(!r32_in_map(nx,nz))continue;
   uint16_t candidate=out->roof[level][nx][nz];if(!candidate)continue;int distance=ox*ox+oz*oz;
   if(distance<best){best=distance;id=candidate;ambiguous=0;}else if(distance==best&&candidate!=id)ambiguous=1;
  }
  out->owner[level][x][z]=ambiguous?0:id;
 }
}
static uint16_t r32_zones_pick(const R32RoofZones*z,int level,int x,int y,uint16_t previous,int margin){
 if(level<0||level>=3||!r32_in_map(x,y))return 0;
 uint16_t inside=z->roof[level][x][y];if(inside)return inside;
 if(margin<0)margin=0;if(margin>4)margin=4;
 int best=100;uint16_t chosen=0;
 for(int dx=-margin;dx<=margin;dx++)for(int dy=-margin;dy<=margin;dy++){
  int nx=x+dx,ny=y+dy;if(!r32_in_map(nx,ny))continue;
  uint16_t id=z->roof[level][nx][ny];if(!id)continue;int distance=dx*dx+dy*dy;
  if(distance<best){best=distance;chosen=id;}
  else if(distance==best&&(id==previous||(chosen!=previous&&id<chosen)))chosen=id;
 }
 return chosen;
}
static int r32_zone_hidden(const R32RoofZones*z,uint16_t selected,int player_level,uint16_t tag){
 if(!selected||player_level<0||player_level>=3||tag==65535u||(tag>>14)<=player_level)return 0;
 int x=tag&127,y=(tag>>7)&127;
 return r32_in_map(x,y)&&z->owner[player_level][x][y]==selected;
}
