#include "r53_ship_priority.h"
static void r53_ship_rail_capture(Model*m,int bitset,int first){
 if(!baking||!trim_world||(((unsigned)bitset>>14)&32767)!=249||trim_source_plane<0||trim_source_plane>3||first<0||first>=vertcount)return;
 int visible=0;for(int f=0;f<m->face_count;f++)if(!m->face_infos||m->face_infos[f]!=-1)visible++;
 if(vertcount-first!=visible*3)return;
 float xl=1e30f,xh=-1e30f,zl=1e30f,zh=-1e30f;
 for(int i=first;i<vertcount;i++){xl=fminf(xl,verts[i].x);xh=fmaxf(xh,verts[i].x);zl=fminf(zl,verts[i].z);zh=fmaxf(zh,verts[i].z);}
 if(xh-xl>512||zh-zl>512)return;
 R50WallRef refs[96];int nr=0;
 for(int level=trim_world->minLevel;level<trim_world->maxLevel;level++)
 for(int x=(int)floorf(xl/128)-1;x<=(int)floorf(xh/128)+1;x++)for(int z=(int)floorf(zl/128)-1;z<=(int)floorf(zh/128)+1;z++){
  if(x<0||z<0||x>=trim_world->maxTileX||z>=trim_world->maxTileZ)continue;
  Ground*t=trim_world->levelTiles[level][x][z];if(!t)continue;
  if(!r50_tile_walls(refs,&nr,t,trim_source_plane)||!r50_tile_walls(refs,&nr,t->bridge,trim_source_plane))return;
 }
 int v=first;
 for(int f=0;f<m->face_count;f++){
  if(m->face_infos&&m->face_infos[f]==-1)continue;Vertex*t=verts+v;v+=3;int priority=r26_priority(m,f);
  if(priority<1||priority>=R26_LAYERS||!r26_opaque_face(m,f)||t[0].a!=255||t[1].a!=255||t[2].a!=255)continue;
  int backed=0;
  for(int i=0;i<nr&&!backed;i++){
   R50WallRef*r=refs+i;Model*b=r->m;float sn=(float)_Pix3D.sin_table[r->yaw&2047]/65536.f,cs=(float)_Pix3D.cos_table[r->yaw&2047]/65536.f;
   for(int g=0;g<b->face_count;g++){
    int lower=r26_priority(b,g);if(lower<0||lower>=priority||!r26_opaque_face(b,g))continue;
    int ids[]={b->face_indices_a[g],b->face_indices_b[g],b->face_indices_c[g]};float q[3][3];
    for(int k=0;k<3;k++){int at=ids[k];float x=b->vertices_x[at],z=b->vertices_z[at];q[k][0]=x*cs+z*sn+r->x;q[k][1]=b->vertices_y[at]+r->y;q[k][2]=z*cs-x*sn+r->z;}
    if(r53_backed(t,q)){backed=1;break;}
   }
  }
  if(!backed)continue;
  if(r26_count+3>R26_DECAL_VERTS){layer_dropped++;continue;}
  memcpy(r26_vertices+r26_count,t,3*sizeof(Vertex));r26_priorities[r26_count/3]=(uint8_t)priority;r26_count+=3;r53_replayed++;
 }
}
