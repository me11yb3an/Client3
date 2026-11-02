/* Stepped roofs on different map levels can share a vertical gable plane.
 * Upper gable faces take precedence only over real coplanar lower roofs.
 * Replay uses original vertices/materials and the existing roof visibility tags. */
static int r69_roof_type(int id){
 switch(id){
 case 1038:case 1604:case 1605:case 1606:case 1632:case 1633:case 1634:
 case 1637:case 1638:case 1639:case 1640:case 1641:case 1642:case 1643:
 case 1665:case 1666:case 1669:case 1670:case 1671:case 1683:case 1686:
 case 1776:case 1777:case 1924:case 1925:case 1926:case 1927:case 1928:
 case 1929:case 1930:case 1931:case 1932:case 1933:case 3496:case 3497:
 case 3737:case 3781:case 3782:case 3783:case 3815:return 1;
 default:return 0;
 }
}
#include "r110_roof_bounds.h"
#include "r112_roof_planes.h"
static unsigned r69_roof_faces;
static int r69_lower_backing(const Vertex*t,Ground*g){
 if(!g||g->occludeLevel>=trim_source_plane)return 0;
 float ux=t[1].x-t[0].x,uy=t[1].y-t[0].y,uz=t[1].z-t[0].z;
 float vx=t[2].x-t[0].x,vy=t[2].y-t[0].y,vz=t[2].z-t[0].z;
 float nx=uy*vz-uz*vy,nz=ux*vy-uy*vx;
 for(int i=0;i<g->locCount;i++){
  Location*l=g->locs[i];
  if(!l||!l->model||l->entity||l->r10_temporary||l->yaw||(l->info&31)<12||(l->info&31)>17||!r69_roof_type(((unsigned)l->bitset>>14)&32767)||r16_live_location(l->bitset))continue;
  Model*m=l->model;if(m->r10_animated)continue;
  if(!r110_roof_candidate(t,m,l->x,l->y,l->z))continue;
  for(int f=0;f<m->face_count;f++){
   if(!r26_opaque_face(m,f))continue;
   int ids[]={m->face_indices_a[f],m->face_indices_b[f],m->face_indices_c[f]};float q[3][3];
   for(int k=0;k<3;k++){int v=ids[k];q[k][0]=m->vertices_x[v]+l->x;q[k][1]=m->vertices_y[v]+l->y;q[k][2]=m->vertices_z[v]+l->z;}
   float ax=q[1][0]-q[0][0],ay=q[1][1]-q[0][1],az=q[1][2]-q[0][2],bx=q[2][0]-q[0][0],by=q[2][1]-q[0][1],bz=q[2][2]-q[0][2];
   if(nx*(ay*bz-az*by)+nz*(ax*by-ay*bx)<=.01f)continue;
   if(r53_backed(t,q))return 1;
  }
 }return 0;
}
static void r69_roof_layers(Model*m,int bitset,int first){
 if(!baking||!trim_world||!m||!r69_roof_type(((unsigned)bitset>>14)&32767)||trim_source_plane<1||trim_source_plane>3||first<0||first>=vertcount)return;
 int visible=0;for(int f=0;f<m->face_count;f++)if(!m->face_infos||m->face_infos[f]!=-1)visible++;
 if(vertcount-first!=visible*3)return;
 int v=first;
 for(int f=0;f<m->face_count;f++){
  if(m->face_infos&&m->face_infos[f]==-1)continue;Vertex*t=verts+v;v+=3;
  if(!r26_opaque_face(m,f)||t[0].a!=255||t[1].a!=255||t[2].a!=255)continue;
  float ny=(t[1].z-t[0].z)*(t[2].x-t[0].x)-(t[1].x-t[0].x)*(t[2].z-t[0].z);
  if(fabsf(ny)>.01f)continue;
  float xl=fminf(t[0].x,fminf(t[1].x,t[2].x)),xh=fmaxf(t[0].x,fmaxf(t[1].x,t[2].x)),zl=fminf(t[0].z,fminf(t[1].z,t[2].z)),zh=fmaxf(t[0].z,fmaxf(t[1].z,t[2].z));
  if(xh-xl>256||zh-zl>256)continue;
  int backed=r112_query(t,trim_source_plane,(int)floorf(xl/128)-1,(int)floorf(xh/128)+1,(int)floorf(zl/128)-1,(int)floorf(zh/128)+1);
  if(backed<0){backed=0;
  for(int level=trim_world->minLevel;level<trim_world->maxLevel&&!backed;level++)
  for(int x=(int)floorf(xl/128)-1;x<=(int)floorf(xh/128)+1&&!backed;x++)for(int z=(int)floorf(zl/128)-1;z<=(int)floorf(zh/128)+1&&!backed;z++){
   if(x<0||z<0||x>=trim_world->maxTileX||z>=trim_world->maxTileZ)continue;
   Ground*g=trim_world->levelTiles[level][x][z];if(!g)continue;
   backed=r69_lower_backing(t,g)||r69_lower_backing(t,g->bridge);
  }
  }
  if(!backed)continue;
  if(r26_count+3>R26_DECAL_VERTS){layer_dropped++;continue;}
  memcpy(r26_vertices+r26_count,t,3*sizeof(Vertex));r26_priorities[r26_count/3]=(uint8_t)trim_source_plane;r26_count+=3;r69_roof_faces++;
 }
}
