#include "r111_floor_bounds.h"
/* A lower wall's vertical edge can touch an upper floor even after its
 * horizontal cap is hidden. Recess only emitted points proven to be on an
 * actual higher-floor triangle. Source models, floor geometry, materials,
 * openings and all same-level dungeon wall tops remain unchanged. */
static int r47_on_floor(const Vertex*p,const float q[3][3]){
 float ax=q[1][0]-q[0][0],az=q[1][2]-q[0][2];
 float bx=q[2][0]-q[0][0],bz=q[2][2]-q[0][2];
 float px=p->x-q[0][0],pz=p->z-q[0][2];
 float det=ax*bz-az*bx;if(fabsf(det)<.01f)return 0;
 float a=(px*bz-pz*bx)/det,b=(ax*pz-az*px)/det;
 if(a<0||b<0||a+b>1)return 0;
 float height=q[0][1]+a*(q[1][1]-q[0][1])+b*(q[2][1]-q[0][1]);
 return fabsf(p->y-height)<.0001f;
}
static int r47_floor_contact(const Vertex*p,int source_level){
 if(!trim_world||source_level<0||source_level>=3)return 0;
 if(!r111_may_contact(p,source_level))return 0;
 int tx=(int)floorf(p->x/128.f),tz=(int)floorf(p->z/128.f);
 for(int level=trim_world->minLevel;level<trim_world->maxLevel;level++)
 for(int x=tx-1;x<=tx;x++)for(int z=tz-1;z<=tz;z++){
  if(x<0||z<0||x>=trim_world->maxTileX||z>=trim_world->maxTileZ)continue;
  if(p->x<x*128||p->x>(x+1)*128||p->z<z*128||p->z>(z+1)*128)continue;
  Ground*g=trim_world->levelTiles[level][x][z];if(!g||g->drawLevel>trim_top_level||g->occludeLevel<=source_level)continue;
  if(r51_point_open(trim_world,g->occludeLevel,x,z,p))continue;
  if(g->underlay){
   TileUnderlay*u=g->underlay;int h=g->occludeLevel;
   if(u->northeastColor==12345678)continue;
   float x0=x*128,x1=(x+1)*128,z0=z*128,z1=(z+1)*128;
   float y00=trim_world->levelHeightmaps[h][x][z],y10=trim_world->levelHeightmaps[h][x+1][z];
   float y01=trim_world->levelHeightmaps[h][x][z+1],y11=trim_world->levelHeightmaps[h][x+1][z+1];
   float a[3][3]={{x1,y11,z1},{x0,y01,z1},{x1,y10,z0}};
   if(r47_on_floor(p,a))return 1;
   if(u->southwestColor!=12345678){
    float b[3][3]={{x0,y00,z0},{x1,y10,z0},{x0,y01,z1}};
    if(r47_on_floor(p,b))return 1;
   }
  }else if(g->overlay){
   TileOverlay*o=g->overlay;
   for(int f=0;f<o->triangleCount;f++){
    int texture=o->triangleTextureIds?o->triangleTextureIds[f]:-1;
    if(texture==-1&&o->triangleColorA[f]==12345678)continue;
    int ids[]={o->triangleVertexA[f],o->triangleVertexB[f],o->triangleVertexC[f]};float q[3][3];
    for(int k=0;k<3;k++){int v=ids[k];q[k][0]=o->vertexX[v];q[k][1]=o->vertexY[v];q[k][2]=o->vertexZ[v];}
    if(r47_on_floor(p,q))return 1;
   }
  }
 }
 return 0;
}
static void r47_recess_contacts(Vertex*stream,int first,int end,int level){
 for(int i=first;i<end;i++)if(stream[i].a==255&&r47_floor_contact(stream+i,level))stream[i].y+=1.f;
}
