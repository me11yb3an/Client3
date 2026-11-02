#pragma once
#include <limits.h>
/* A conservative height interval for visible physical floors above each
 * source plane. Rebuilt for every bake. Openings and exact triangle tests
 * remain in r47; live doors keep the original uncached path. */
typedef struct{int lo,hi;}R111Range;
static R111Range r111_ranges[3][104][104];static World3D*r111_world;
static int r111_top;static unsigned r111_queries,r111_rejected;
static void r111_prepare(World3D*w,int top){
 r111_world=w;r111_top=top;r111_queries=r111_rejected=0;
 for(int p=0;p<3;p++)for(int x=0;x<104;x++)for(int z=0;z<104;z++)r111_ranges[p][x][z]=(R111Range){INT_MAX,INT_MIN};
 for(int l=w->minLevel;l<w->maxLevel;l++)for(int x=0;x<w->maxTileX;x++)for(int z=0;z<w->maxTileZ;z++){
  Ground*g=w->levelTiles[l][x][z];if(!g||g->drawLevel>top||g->occludeLevel<1||g->occludeLevel>3)continue;
  int lo=INT_MAX,hi=INT_MIN;
  if(g->underlay){int h=g->occludeLevel;for(int dx=0;dx<2;dx++)for(int dz=0;dz<2;dz++){int y=w->levelHeightmaps[h][x+dx][z+dz];if(y<lo)lo=y;if(y>hi)hi=y;}}
  else if(g->overlay){TileOverlay*o=g->overlay;for(int f=0;f<o->triangleCount;f++){int ids[]={o->triangleVertexA[f],o->triangleVertexB[f],o->triangleVertexC[f]};for(int k=0;k<3;k++){int y=o->vertexY[ids[k]];if(y<lo)lo=y;if(y>hi)hi=y;}}}
  if(lo>hi)continue;
  for(int p=0;p<g->occludeLevel;p++){R111Range*c=&r111_ranges[p][x][z];if(lo<c->lo)c->lo=lo;if(hi>c->hi)c->hi=hi;}
 }
}
static int r111_may_contact(const Vertex*p,int source){
 if(!baking||r111_world!=trim_world||r111_top!=trim_top_level||source<0||source>=3)return 1;
 int tx=(int)floorf(p->x/128.f),tz=(int)floorf(p->z/128.f);r111_queries++;
 for(int x=tx-1;x<=tx;x++)for(int z=tz-1;z<=tz;z++)if(x>=0&&z>=0&&x<104&&z<104){R111Range*c=&r111_ranges[source][x][z];if(c->lo<=c->hi&&p->y>=c->lo-.01f&&p->y<=c->hi+.01f)return 1;}
 r111_rejected++;return 0;
}
