#include "r71_shore_types.h"
/* Source hill skew is bilinear; the rendered ground consists of triangles.
 * On steep banks these can differ substantially. Ground plants retain their
 * source height above ground, using the actual opaque floor under each vertex. */
static unsigned r71_shore_instances;
static float r71_new_y[4096];
typedef struct{uint32_t epoch;int x,z,plane,found;float y;}R71Height;
static R71Height r71_heights[1024];
static uint32_t r71_epoch=1;
static unsigned r71_height_queries,r71_height_hits;
static void r71_begin_bake(void){
 if(++r71_epoch==0){memset(r71_heights,0,sizeof r71_heights);r71_epoch=1;}
 r71_shore_instances=r71_height_queries=r71_height_hits=0;
}
static int r71_triangle_height(const float q[3][3],float x,float z,float*out){
 double ax=q[1][0]-q[0][0],az=q[1][2]-q[0][2],bx=q[2][0]-q[0][0],bz=q[2][2]-q[0][2],det=ax*bz-bx*az;
 if(fabs(det)<.01)return 0;
 double px=x-q[0][0],pz=z-q[0][2],u=(px*bz-bx*pz)/det,v=(ax*pz-px*az)/det;
 if(u<-.00001||v<-.00001||u+v>1.00001)return 0;
 *out=(float)(q[0][1]+u*(q[1][1]-q[0][1])+v*(q[2][1]-q[0][1]));return 1;
}
static int r71_ground_height(Ground*g,int tx,int tz,float x,float z,float*out){
 if(!g||g->occludeLevel!=trim_source_plane)return 0;
 float best=1e30f,value;int found=0;
 if(g->underlay){
  /* The two underlay triangles lie in this exact128-unit square. Their
   * barycentric tolerance can extend it by at most.00256 unit; retain a
   * wider.01 margin before skipping a neighboring square. */
  float bx=tx*128,bz=tz*128;
  if(x<bx-.01f||x>bx+128.01f||z<bz-.01f||z>bz+128.01f)return 0;
  TileUnderlay*u=g->underlay;if(!r55_floor_opaque(u->textureId))return 0;
  int h=g->occludeLevel;float xx=tx*128,zz=tz*128,y00=trim_world->levelHeightmaps[h][tx][tz],y10=trim_world->levelHeightmaps[h][tx+1][tz],y01=trim_world->levelHeightmaps[h][tx][tz+1],y11=trim_world->levelHeightmaps[h][tx+1][tz+1];
  float a[3][3]={{xx+128,y11,zz+128},{xx,y01,zz+128},{xx+128,y10,zz}},b[3][3]={{xx,y00,zz},{xx+128,y10,zz},{xx,y01,zz+128}};
  if(u->northeastColor!=12345678&&r71_triangle_height(a,x,z,&value)){best=value;found=1;}
  if(u->southwestColor!=12345678&&r71_triangle_height(b,x,z,&value)){if(value<best)best=value;found=1;}
 }else if(g->overlay){
  TileOverlay*o=g->overlay;
  for(int f=0;f<o->triangleCount;f++){
   int texture=o->triangleTextureIds?o->triangleTextureIds[f]:-1;if(!r55_floor_opaque(texture)||(texture<0&&o->triangleColorA[f]==12345678))continue;
   int ids[]={o->triangleVertexA[f],o->triangleVertexB[f],o->triangleVertexC[f]};float q[3][3];
   for(int k=0;k<3;k++){int v=ids[k];q[k][0]=o->vertexX[v];q[k][1]=o->vertexY[v];q[k][2]=o->vertexZ[v];}
   if(r71_triangle_height(q,x,z,&value)){if(value<best)best=value;found=1;}
  }
 }
 if(found)*out=best;return found;
}
static int r71_surface_uncached(float x,float z,float*out){
 int tx=(int)floorf(x/128),tz=(int)floorf(z/128),found=0;float best=1e30f,y;
 for(int level=trim_world->minLevel;level<trim_world->maxLevel;level++)
 for(int xx=tx-1;xx<=tx;xx++)for(int zz=tz-1;zz<=tz;zz++){
  if(xx<0||zz<0||xx>=trim_world->maxTileX||zz>=trim_world->maxTileZ)continue;
  Ground*g=trim_world->levelTiles[level][xx][zz];if(!g)continue;
  if(r71_ground_height(g,xx,zz,x,z,&y)){if(y<best)best=y;found=1;}
  if(r71_ground_height(g->bridge,xx,zz,x,z,&y)){if(y<best)best=y;found=1;}
 }
 if(found)*out=best;return found;
}
static int r71_surface(float x,float z,float*out){
 if(!isfinite(x)||!isfinite(z)||fabsf(x)>20000||fabsf(z)>20000)return 0;
 int ix=(int)lroundf(x),iz=(int)lroundf(z);
 if(fabsf(x-ix)>.0001f||fabsf(z-iz)>.0001f)return r71_surface_uncached(x,z,out);
 unsigned key=(uint32_t)ix*73856093u^(uint32_t)iz*19349663u^(uint32_t)trim_source_plane*83492791u;
 R71Height*c=r71_heights+(key&1023u);r71_height_queries++;
 if(c->epoch==r71_epoch&&c->x==ix&&c->z==iz&&c->plane==trim_source_plane){r71_height_hits++;if(c->found)*out=c->y;return c->found;}
 float y=0;int found=r71_surface_uncached(x,z,&y);
 *c=(R71Height){r71_epoch,ix,iz,trim_source_plane,found,y};if(found)*out=y;return found;
}
static void r71_ground_attachment(Model*m,int first,int x,int y,int z,int yaw,int kind,unsigned*instances){
 if(!baking||!trim_world||!m||!kind||m->r10_animated||yaw||trim_source_plane<0||trim_source_plane>3||first<0||first>=vertcount)return;
 int n=vertcount-first;if(n<3||n>4096)return;
 int tx=x/128,tz=z/128;if(tx<0||tz<0||tx>=trim_world->maxTileX||tz>=trim_world->maxTileZ)return;
 int plane=trim_source_plane,sw=trim_world->levelHeightmaps[plane][tx][tz],se=trim_world->levelHeightmaps[plane][tx+1][tz],ne=trim_world->levelHeightmaps[plane][tx+1][tz+1],nw=trim_world->levelHeightmaps[plane][tx][tz+1],offset=(kind&4)?-2:0;
 if(y!=(sw+se+ne+nw)/4+offset)return;
 int changed=0;
 for(int i=0;i<n;i++){
  Vertex*v=verts+first+i;float floor,raw;
  if(!r71_surface(v->x,v->z,&floor))return;
  if(kind&2){
   int lx=(int)lroundf(v->x)-x+64,lz=(int)lroundf(v->z)-z+64;
   if(lx< -64||lx>192||lz< -64||lz>192)return;
   int south=sw+(se-sw)*lx/128,north=nw+(ne-nw)*lx/128;
   raw=v->y-(south+(north-south)*lz/128)-offset;
  }else raw=v->y-y;
  float target=floor+raw-2.f;
  if(!isfinite(target)||raw< -512||raw>32||fabsf(target-v->y)>256)return;
  r71_new_y[i]=target;if(fabsf(target-v->y)>.05f)changed=1;
 }
 if(changed){for(int i=0;i<n;i++)verts[first+i].y=r71_new_y[i];(*instances)++;}
}

static void r71_shore_plants(Model*m,int bitset,int first,int x,int y,int z,int yaw){r71_ground_attachment(m,first,x,y,z,yaw,r71_plant_kind(((unsigned)bitset>>14)&32767),&r71_shore_instances);}
