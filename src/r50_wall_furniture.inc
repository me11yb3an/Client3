#include "r50_wall_fit.h"
#include "r50_furniture_types.h"
typedef struct {Model*m;int x,y,z,yaw;} R50WallRef;
static int r50_add_wall(R50WallRef*refs,int*n,Model*m,int x,int y,int z,int yaw){
 if(!m||m->r10_animated)return 1;
 for(int i=0;i<*n;i++)if(refs[i].m==m&&refs[i].x==x&&refs[i].y==y&&refs[i].z==z&&refs[i].yaw==yaw)return 1;
 if(*n>=96)return 0;refs[(*n)++]=(R50WallRef){m,x,y,z,yaw};return 1;
}
static int r50_tile_walls(R50WallRef*refs,int*n,Ground*t,int plane){
 if(!t||t->occludeLevel!=plane)return 1;
 if(t->wall&&!r16_live_location(t->wall->bitset)){Wall*w=t->wall;if(!r50_add_wall(refs,n,w->modelA,w->x,w->y,w->z,0)||!r50_add_wall(refs,n,w->modelB,w->x,w->y,w->z,0))return 0;}
 for(int i=0;i<t->locCount;i++){
  Location*l=t->locs[i];if(!l||l->entity||l->r10_temporary||(l->info&31)!=9||r16_live_location(l->bitset))continue;
  if(!r50_add_wall(refs,n,l->model,l->x,l->y,l->z,l->yaw))return 0;
 }
 return 1;
}
static void r50_furniture_wall_fit(int x,int y,int z,int bitset,int first){
#if defined(GL11) && defined(__vita__)
 int id=((unsigned)bitset>>14)&32767,plane=baking?trim_source_plane:live_door_floor_level;
 if(!trim_world||plane<0||plane>3||wall_floor_context||window_decor_context||first<0||first>=vertcount||!r50_furniture_type(id)||((unsigned)bitset&0x60000000u)!=0x40000000u)return;
 float xl=1e30f,xh=-1e30f,zl=1e30f,zh=-1e30f;
 for(int i=first;i<vertcount;i++)if(verts[i].a==255){if(verts[i].x<xl)xl=verts[i].x;if(verts[i].x>xh)xh=verts[i].x;if(verts[i].z<zl)zl=verts[i].z;if(verts[i].z>zh)zh=verts[i].z;}
 if(xl>xh||xh-xl>1024||zh-zl>1024)return;
 R50WallRef refs[96];int nr=0;
 int x0=(int)floorf(xl/128.f)-1,x1=(int)floorf(xh/128.f)+1,z0=(int)floorf(zl/128.f)-1,z1=(int)floorf(zh/128.f)+1;
 for(int l=trim_world->minLevel;l<trim_world->maxLevel;l++)for(int tx=x0;tx<=x1;tx++)for(int tz=z0;tz<=z1;tz++){
  if(tx<0||tz<0||tx>=trim_world->maxTileX||tz>=trim_world->maxTileZ)continue;
  Ground*t=trim_world->levelTiles[l][tx][tz];if(!t)continue;
  if(!r50_tile_walls(refs,&nr,t,plane)||!r50_tile_walls(refs,&nr,t->bridge,plane))return;
 }
 static R50WallFace faces[1024];int nf=0;
 for(int i=0;i<nr;i++){
  R50WallRef*r=refs+i;Model*m=r->m;float sn=(float)_Pix3D.sin_table[r->yaw&2047]/65536.f,cs=(float)_Pix3D.cos_table[r->yaw&2047]/65536.f;
  for(int f=0;f<m->face_count;f++){
   if(!r26_opaque_face(m,f))continue;if(nf>=1024)return;
   int ids[]={m->face_indices_a[f],m->face_indices_b[f],m->face_indices_c[f]};
   for(int k=0;k<3;k++){int v=ids[k];float vx=m->vertices_x[v],vz=m->vertices_z[v];faces[nf].p[k][0]=vx*cs+vz*sn+r->x-x;faces[nf].p[k][1]=m->vertices_y[v]+r->y-y;faces[nf].p[k][2]=vz*cs-vx*sn+r->z-z;}
   nf++;
  }
 }
 /* Uncached animated scenery keeps the bounded whole-footprint path.
  * Detailed partial-wall fitting is bake/cache-miss work only. */
 if(nf)r50_fit_vertices_mode(verts+first,vertcount-first,faces,nf,(float)x,(float)y,(float)z,baking||!(live_loc_flags(bitset)&R16_LIVE_ANIM));
#else
 (void)x;(void)y;(void)z;(void)bitset;(void)first;
#endif
}
