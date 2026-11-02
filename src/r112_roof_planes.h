#pragma once
/* Immutable bake-local roof candidates indexed by their exact integer X/Z
 * plane. Every hit still uses the original normal, spatial, physical-level
 * and positive-area coverage checks. Diagonal/fractional targets and index
 * overflow fall back to the original search. No scene pointers survive. */
#define R112_NODES 65536
#define R112_BUCKETS 65536
typedef struct{float q[3][3],nx,nz;int next,plane,texture;short x,z,level,axis;}R112Node;
static R112Node r112_nodes[R112_NODES];static int r112_heads[R112_BUCKETS],r112_count,r112_failed;
static unsigned r112_queries,r112_candidates;
static uint8_t r112_cell_levels[104][104];
static unsigned r112_bucket(int axis,int plane,int x,int z,int level){unsigned h=(uint32_t)plane*2654435761u^((unsigned)axis*2246822519u)^((unsigned)x*73856093u)^((unsigned)z*19349663u)^((unsigned)level*83492791u);h^=h>>16;h*=0x85ebca6bu;return (h^(h>>13))&(R112_BUCKETS-1);}

static void r112_collect(Ground*g,int x,int z){
 /* Physical plane3 can never back a target above it (targets end at3). */
 if(!g||g->occludeLevel==3)return;if(g->occludeLevel<0||g->occludeLevel>3){r112_failed=1;return;}
 for(int i=0;i<g->locCount;i++){
  Location*l=g->locs[i];if(!l||!l->model||l->entity||l->r10_temporary||l->yaw||(l->info&31)<12||(l->info&31)>17||!r69_roof_type(((unsigned)l->bitset>>14)&32767)||r16_live_location(l->bitset))continue;
  Model*m=l->model;if(m->r10_animated)continue;
  for(int f=0;f<m->face_count;f++){
   /* Texture transparency is populated lazily. Cache geometry and the
    * static alpha/hidden guards, then evaluate current opacity at query. */
   if((m->face_infos&&m->face_infos[f]==-1)||(m->face_alphas&&m->face_alphas[f]))continue;
   int texture=-1;if(m->face_infos&&(m->face_infos[f]&2)){if(!m->face_colors||m->face_colors[f]<0)continue;texture=m->face_colors[f];}
   int ids[]={m->face_indices_a[f],m->face_indices_b[f],m->face_indices_c[f]};float q[3][3];
   for(int k=0;k<3;k++){int v=ids[k];q[k][0]=m->vertices_x[v]+l->x;q[k][1]=m->vertices_y[v]+l->y;q[k][2]=m->vertices_z[v]+l->z;}
   int axis=q[0][0]==q[1][0]&&q[0][0]==q[2][0]?0:q[0][2]==q[1][2]&&q[0][2]==q[2][2]?2:-1;if(axis<0)continue;
   float ax=q[1][0]-q[0][0],ay=q[1][1]-q[0][1],az=q[1][2]-q[0][2],bx=q[2][0]-q[0][0],by=q[2][1]-q[0][1],bz=q[2][2]-q[0][2];
   float nx=ay*bz-az*by,nz=ax*by-ay*bx;if(nx==0&&nz==0)continue;
   if(r112_count==R112_NODES){r112_failed=1;return;}
   int plane=(int)q[0][axis];unsigned bucket=r112_bucket(axis,plane,x,z,g->occludeLevel);R112Node*n=r112_nodes+r112_count;memcpy(n->q,q,sizeof q);n->nx=nx;n->nz=nz;n->x=x;n->z=z;n->level=g->occludeLevel;n->axis=axis;n->plane=plane;n->texture=texture;n->next=r112_heads[bucket];r112_heads[bucket]=r112_count++;r112_cell_levels[x][z]|=(uint8_t)(1u<<g->occludeLevel);
  }
 }
}
static void r112_prepare(World3D*w){
 memset(r112_cell_levels,0,sizeof r112_cell_levels);r112_count=r112_failed=0;r112_queries=r112_candidates=0;for(int i=0;i<R112_BUCKETS;i++)r112_heads[i]=-1;
 for(int l=w->minLevel;l<w->maxLevel&&!r112_failed;l++)for(int x=0;x<w->maxTileX&&!r112_failed;x++)for(int z=0;z<w->maxTileZ&&!r112_failed;z++){Ground*g=w->levelTiles[l][x][z];if(g){r112_collect(g,x,z);r112_collect(g->bridge,x,z);}}
}
static int r112_query(const Vertex*t,int source,int x0,int x1,int z0,int z1){
 if(r112_failed||source<0||source>3)return -1;
 int axis=t[0].x==t[1].x&&t[0].x==t[2].x?0:t[0].z==t[1].z&&t[0].z==t[2].z?2:-1;
 if(axis<0)return -1;float value=axis==0?t[0].x:t[0].z;
 if(value< -1000000||value>1000000||value!=floorf(value))return -1;
 int plane=(int)value;r112_queries++;
 float ux=t[1].x-t[0].x,uy=t[1].y-t[0].y,uz=t[1].z-t[0].z,vx=t[2].x-t[0].x,vy=t[2].y-t[0].y,vz=t[2].z-t[0].z;
 float nx=uy*vz-uz*vy,nz=ux*vy-uy*vx;
 if(x0<0)x0=0;if(z0<0)z0=0;if(x1>103)x1=103;if(z1>103)z1=103;
 for(int x=x0;x<=x1;x++)for(int z=z0;z<=z1;z++){
  unsigned levels=r112_cell_levels[x][z]&((1u<<source)-1u);if(!levels)continue;
  for(int level=0;level<source;level++)if(levels&(1u<<level))
  for(int at=r112_heads[r112_bucket(axis,plane,x,z,level)];at>=0;at=r112_nodes[at].next){R112Node*n=r112_nodes+at;
   if(n->axis!=axis||n->plane!=plane||n->level!=level||n->x!=x||n->z!=z||nx*n->nx+nz*n->nz<=.01f)continue;
   if(n->texture>=0&&(!_Pix3D.textures||!_Pix3D.textureHasTransparency||n->texture>=_Pix3D.textureCount||!_Pix3D.textures[n->texture]||_Pix3D.textureHasTransparency[n->texture]))continue;
   r112_candidates++;if(r53_backed(t,n->q))return 1;
  }
 }
 return 0;
}
