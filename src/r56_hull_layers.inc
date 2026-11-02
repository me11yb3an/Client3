#include "r56_hull_types.h"
typedef struct{float p[3][3],lo[3],hi[3];int priority;}R56Face;
/* Honor opaque trim priority across modular hull sections. Existing replay
 * passes keep the original geometry, UVs, colors and physical roof owner. */
static void r56_hull_layers(Model*m,int x,int y,int z,int yaw,int bitset,int first){
 if(!baking||m->r10_animated||!trim_world||!r56_hull_type(((unsigned)bitset>>14)&32767)||trim_source_plane<0||trim_source_plane>3||first<0||first>=vertcount)return;
 int visible=0,highest=0;for(int f=0;f<m->face_count;f++)if(!m->face_infos||m->face_infos[f]!=-1){visible++;int p=r26_priority(m,f);if(p>highest)highest=p;}
 if(!highest||highest>=R26_LAYERS||vertcount-first!=visible*3)return;
 float xl=1e30f,xh=-1e30f,zl=1e30f,zh=-1e30f;
 for(int v=first;v<vertcount;v++){xl=fminf(xl,verts[v].x);xh=fmaxf(xh,verts[v].x);zl=fminf(zl,verts[v].z);zh=fmaxf(zh,verts[v].z);}
 if(xh-xl>512||zh-zl>512)return;
 R50WallRef refs[96];int nr=0;if(!r50_add_wall(refs,&nr,m,x,y,z,yaw))return;
 for(int level=trim_world->minLevel;level<trim_world->maxLevel;level++)
 for(int tx=(int)floorf(xl/128)-1;tx<=(int)floorf(xh/128)+1;tx++)for(int tz=(int)floorf(zl/128)-1;tz<=(int)floorf(zh/128)+1;tz++){
  if(tx<0||tz<0||tx>=trim_world->maxTileX||tz>=trim_world->maxTileZ)continue;Ground*tiles[2]={trim_world->levelTiles[level][tx][tz],NULL};if(tiles[0])tiles[1]=tiles[0]->bridge;
  for(int g=0;g<2;g++){Ground*t=tiles[g];if(!t||t->occludeLevel!=trim_source_plane)continue;
   for(int i=0;i<t->locCount;i++){Location*l=t->locs[i];if(!l||!l->model||l->entity||l->r10_temporary||!r56_hull_type(((unsigned)l->bitset>>14)&32767)||r16_live_location(l->bitset))continue;
    if(!r50_add_wall(refs,&nr,l->model,l->x,l->y,l->z,l->yaw))return;}
  }
 }
 static R56Face faces[4096];int nf=0;
 for(int i=0;i<nr;i++){
  R50WallRef*r=refs+i;Model*b=r->m;float sn=(float)_Pix3D.sin_table[r->yaw&2047]/65536.f,cs=(float)_Pix3D.cos_table[r->yaw&2047]/65536.f;
  for(int f=0;f<b->face_count;f++){
   int p=r26_priority(b,f);if(p<0||p>=highest||!r26_opaque_face(b,f))continue;if(nf>=4096)return;
   int ids[]={b->face_indices_a[f],b->face_indices_b[f],b->face_indices_c[f]};R56Face*q=faces+nf++;q->priority=p;
   for(int k=0;k<3;k++){int v=ids[k];float xx=b->vertices_x[v],zz=b->vertices_z[v];q->p[k][0]=xx*cs+zz*sn+r->x;q->p[k][1]=b->vertices_y[v]+r->y;q->p[k][2]=zz*cs-xx*sn+r->z;}
   for(int axis=0;axis<3;axis++){q->lo[axis]=fminf(q->p[0][axis],fminf(q->p[1][axis],q->p[2][axis]));q->hi[axis]=fmaxf(q->p[0][axis],fmaxf(q->p[1][axis],q->p[2][axis]));}
  }
 }
 int v=first;
 for(int f=0;f<m->face_count;f++){
  if(m->face_infos&&m->face_infos[f]==-1)continue;Vertex*t=verts+v;v+=3;int p=r26_priority(m,f);
  if(p<1||p>=R26_LAYERS||!r26_opaque_face(m,f)||t[0].a!=255||t[1].a!=255||t[2].a!=255)continue;
  float lo[3]={fminf(t[0].x,fminf(t[1].x,t[2].x)),fminf(t[0].y,fminf(t[1].y,t[2].y)),fminf(t[0].z,fminf(t[1].z,t[2].z))};
  float hi[3]={fmaxf(t[0].x,fmaxf(t[1].x,t[2].x)),fmaxf(t[0].y,fmaxf(t[1].y,t[2].y)),fmaxf(t[0].z,fmaxf(t[1].z,t[2].z))};
  int backed=0;for(int g=0;g<nf;g++){
   R56Face*q=faces+g;if(q->priority>=p)continue;int near=1;for(int axis=0;axis<3;axis++)if(q->hi[axis]<lo[axis]||q->lo[axis]>hi[axis])near=0;if(!near)continue;
   if(r53_backed(t,q->p)){backed=1;break;}
  }if(!backed)continue;
  if(p==1){if(trim_count+3>R16_TRIM_VERTS){layer_dropped++;continue;}memcpy(trim_cpu+trim_count,t,3*sizeof(Vertex));trim_count+=3;}
  else if(p==2){if(clock_count+3>R16_CLOCK_VERTS){layer_dropped++;continue;}memcpy(clock_cpu+clock_count,t,3*sizeof(Vertex));clock_count+=3;}
  else {if(r26_count+3>R26_DECAL_VERTS){layer_dropped++;continue;}memcpy(r26_vertices+r26_count,t,3*sizeof(Vertex));r26_priorities[r26_count/3]=(uint8_t)p;r26_count+=3;}
  r56_replayed++;
 }
}
