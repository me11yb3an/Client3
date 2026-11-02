/* Shared, translation-independent prepared poses. All animation/model work
 * still runs. Only exact opaque untextured inputs reuse their emitted bytes.
 * One new entry per frame; no GPU allocation, source mutation or owner pointer. */
#include "r74_actor_pose.h"
#define R76_SLOTS 32
#define R76_BYTES (4u*1024u*1024u)
typedef struct {R74Pose*key;Vertex*v;uint32_t*i,*layers;int nv,ni,yaw,sig,epoch,layer_count,layer_counts[10];unsigned seen;size_t bytes;}R76Pose;
static R76Pose r76_poses[R76_SLOTS];
static size_t r76_bytes;
static unsigned r76_frame,r76_created,r76_hits,r76_misses;
static unsigned r76_pose_hint;
/* A cheap rejection key only. Every possible hit still compares all inputs. */
static int r76_signature(const Model*m){
 uint32_t h=2166136261u^(uint32_t)m->face_count^(uint32_t)m->vertex_count^r76_pose_hint;
 for(int v=0;v<m->vertex_count;v+=m->vertex_count/16+1){h=(h^(uint32_t)(m->vertices_x?m->vertices_x[v]:0))*16777619u;h=(h^(uint32_t)(m->vertices_y?m->vertices_y[v]:0))*16777619u;h=(h^(uint32_t)(m->vertices_z?m->vertices_z[v]:0))*16777619u;}
 for(int f=0;f<m->face_count;f+=m->face_count/16+1){h=(h^(uint32_t)(m->face_color_a?m->face_color_a[f]:0))*16777619u;h=(h^(uint32_t)(m->face_color_b?m->face_color_b[f]:0))*16777619u;h=(h^(uint32_t)(m->face_color_c?m->face_color_c[f]:0))*16777619u;}
 return (int)h;
}
static void r76_clear(R76Pose*p){if(p->bytes<=r76_bytes)r76_bytes-=p->bytes;else r76_bytes=0;free(p->key);free(p->v);free(p->i);free(p->layers);memset(p,0,sizeof(*p));}
static void r76_reset(void){for(int j=0;j<R76_SLOTS;j++)r76_clear(r76_poses+j);r76_created=0;}
static void r76_begin(void){if(!++r76_frame){r76_frame=1;for(int i=0;i<R76_SLOTS;i++)r76_poses[i].seen=0;}r76_created=0;}
static int r76_eligible(const Model*m){
 if(!m||geometry_diag||m->face_count<1||m->face_count>4096)return 0;
 for(int f=0;f<m->face_count;f++)if((m->face_infos&&m->face_infos[f]!=-1&&(m->face_infos[f]&2))||(m->face_alphas&&m->face_alphas[f]))return 0;
 return 1;
}
static int r76_emit(Model*m,int yaw,int x,int y,int z){
 if(!m||geometry_diag||m->face_count<1||m->face_count>4096)return 0;
 if(!model_in_view(m,x-_World3D.eyeX,y-_World3D.eyeY,z-_World3D.eyeZ)){r57_pending_count=0;memset(r57_pending_counts,0,sizeof r57_pending_counts);return 1;}
 int sig=r76_signature(m);
 for(int j=0;j<R76_SLOTS;j++){
  R76Pose*p=r76_poses+j;if(!p->key||p->epoch!=stamp||p->yaw!=yaw||p->sig!=sig||!r74_pose_equal(p->key,m))continue;
  if(p->nv>R16_VERT_BUDGET-vertcount||p->ni>R16_VERT_BUDGET-elementcount)return 0;
  int base=vertcount;memcpy(verts+base,p->v,(size_t)p->nv*sizeof(Vertex));for(int k=0;k<p->nv;k++){Vertex*v=verts+base+k;v->x+=x;v->y+=y;v->z+=z;}vertcount+=p->nv;
  for(int k=0;k<p->ni;k++)indices[elementcount++]=(uint32_t)base+p->i[k];
  r57_pending_count=p->layer_count;memcpy(r57_pending_counts,p->layer_counts,sizeof r57_pending_counts);
  for(int k=0;k<p->layer_count;k++)r57_pending[k]=(uint32_t)base+p->layers[k];
  p->seen=r76_frame;r76_hits++;return 1;
 }r76_misses++;return 0;
}
static void r76_store(Model*m,int yaw,int x,int y,int z,int v0,int e0){
 if(r76_created||overflow||!r76_eligible(m))return;r76_created=1;
 int nv=vertcount-v0,ni=elementcount-e0;if(nv<1||ni<1||nv>12288||ni>12288)return;
 for(int k=e0;k<elementcount;k++)if(indices[k]<(unsigned)v0||indices[k]>=(unsigned)vertcount)return;
 if(r57_pending_count<0||r57_pending_count>24576)return;int layer_sum=0;
 for(int k=0;k<10;k++){if(r57_pending_counts[k]<0||r57_pending_counts[k]>24576)return;layer_sum+=r57_pending_counts[k];}
 if(layer_sum!=r57_pending_count)return;
 for(int k=0;k<r57_pending_count;k++)if(r57_pending[k]<(unsigned)v0||r57_pending[k]>=(unsigned)vertcount)return;
 R74Pose*key=r74_pose_new(m);if(!key)return;
 size_t bytes=key->bytes+(size_t)nv*sizeof(Vertex)+(size_t)(ni+r57_pending_count)*sizeof(uint32_t);if(bytes>R76_BYTES){free(key);return;}
 /* Invalidate old scene/palette generations before choosing an LRU entry. */
 for(int j=0;j<R76_SLOTS;j++)if(r76_poses[j].key&&r76_poses[j].epoch!=stamp)r76_clear(r76_poses+j);
 R76Pose*target=NULL;
 for(int j=0;j<R76_SLOTS;j++)if(!r76_poses[j].key){target=r76_poses+j;break;}
 if(!target){for(int j=0;j<R76_SLOTS;j++)if(!target||(unsigned)(r76_frame-r76_poses[j].seen)>(unsigned)(r76_frame-target->seen))target=r76_poses+j;r76_clear(target);}
 while(r76_bytes+bytes>R76_BYTES){R76Pose*old=NULL;for(int j=0;j<R76_SLOTS;j++)if(r76_poses[j].key&&(!old||(unsigned)(r76_frame-r76_poses[j].seen)>(unsigned)(r76_frame-old->seen)))old=r76_poses+j;if(!old){free(key);return;}r76_clear(old);}
 Vertex*v=malloc((size_t)nv*sizeof(Vertex));uint32_t*ix=malloc((size_t)ni*sizeof(uint32_t)),*layers=r57_pending_count?malloc((size_t)r57_pending_count*sizeof(uint32_t)):NULL;if(!v||!ix||(r57_pending_count&&!layers)){free(v);free(ix);free(layers);free(key);return;}
 for(int k=0;k<nv;k++){v[k]=verts[v0+k];v[k].x-=x;v[k].y-=y;v[k].z-=z;}
 for(int k=0;k<ni;k++)ix[k]=indices[e0+k]-(uint32_t)v0;
 for(int k=0;k<r57_pending_count;k++)layers[k]=r57_pending[k]-(uint32_t)v0;
 *target=(R76Pose){.key=key,.v=v,.i=ix,.layers=layers,.nv=nv,.ni=ni,.yaw=yaw,.sig=r76_signature(m),.epoch=stamp,.seen=r76_frame,.bytes=bytes,.layer_count=r57_pending_count};memcpy(target->layer_counts,r57_pending_counts,sizeof r57_pending_counts);r76_bytes+=bytes;
}
