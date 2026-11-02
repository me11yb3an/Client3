#include "r62_crop_types.h"
#define R62_SLOTS 48
#define R62_INSTANCES R16_DYN_MAX
#define R62_BYTE_CAP (2u*1024u*1024u)
typedef struct{Model*m;int yaw,epoch,x,y,z,count;uint64_t signature;GLuint vbo,ibo;size_t bytes;unsigned seen,queued;}R62Mesh;
typedef struct{R62Mesh*mesh;int dx,dy,dz;}R62Instance;
typedef struct{Model*m;int yaw,x,y,z,bitset;unsigned score;uint64_t signature;R62Mesh*mesh;}R62Group;
static R62Mesh r62_meshes[R62_SLOTS];
static R62Instance r62_instances[R62_INSTANCES];
static R62Group r62_groups[R62_SLOTS];
static int r62_instance_n,r62_group_n;
static size_t r62_bytes;
static unsigned r62_saved_vertices;
/* These five configured crop families use the immutable location-model LRU:
 * no skeletal animation, terrain skew, deferred shared lighting, or world-
 * position fixture fitting. Full source signatures are checked once per
 * distinct model/yaw each frame; resource stages still come from live tiles. */
static int r62_eligible(Model*m,int bitset){
 if(!m||m->r10_animated||((unsigned)bitset&0x60000000u)!=0x40000000u)return 0;
 int id=((unsigned)bitset>>14)&32767;if(!r62_crop_type(id)||id>=_LocType.count)return 0;
 LocType*t=loctype_get(id);return t&&t->anim==-1&&!t->hillskew&&!t->sharelight&&m->face_count>0&&m->face_count*3<=R16_CHUNK_VERTS;
}
static uint64_t r62_words(uint64_t h,const int*p,int n){
 h=(h^(p?1u:0u))*1099511628211ull;if(p)for(int i=0;i<n;i++)h=(h^(uint32_t)p[i])*1099511628211ull;return h;
}
static uint64_t r62_signature(Model*m){
 uint64_t h=1469598103934665603ull;int nv=m->vertex_count,nf=m->face_count,nt=m->textured_face_count;
 h=r62_words(h,&nv,1);h=r62_words(h,&nf,1);h=r62_words(h,&nt,1);
 int*points[]={m->vertices_x,m->vertices_y,m->vertices_z};for(int i=0;i<3;i++)h=r62_words(h,points[i],nv);
 int*faces[]={m->face_indices_a,m->face_indices_b,m->face_indices_c,m->face_color_a,m->face_color_b,m->face_color_c,m->face_colors,m->face_infos,m->face_alphas,m->face_priorities};for(int i=0;i<10;i++)h=r62_words(h,faces[i],nf);
 int*axes[]={m->textured_p_coordinate,m->textured_m_coordinate,m->textured_n_coordinate};for(int i=0;i<3;i++)h=r62_words(h,axes[i],nt);
 return r62_words(h,&m->model_priority,1);
}
static void r62_clear(R62Mesh*s){
 if(s->vbo)glDeleteBuffers(1,&s->vbo);if(s->ibo)glDeleteBuffers(1,&s->ibo);
 if(s->bytes<=r62_bytes)r62_bytes-=s->bytes;else r62_bytes=0;memset(s,0,sizeof(*s));
}
static void r62_release(void){for(int i=0;i<R62_SLOTS;i++)r62_clear(r62_meshes+i);r62_group_n=r62_instance_n=0;r62_saved_vertices=0;}
static R62Mesh*r62_find(R62Group*g){
 for(int i=0;i<R62_SLOTS;i++){R62Mesh*s=r62_meshes+i;if(s->vbo&&s->m==g->m&&s->yaw==g->yaw&&s->epoch==stamp&&s->signature==g->signature)return s;}return NULL;
}
static R62Mesh*r62_upload(R62Group*g){
 if(!r58_can_promote())return NULL;dyn_promotions++;
 int v0=vertcount,e0=elementcount;emit_placed(g->m,g->yaw,g->x,g->y,g->z,g->bitset);
 int nv=vertcount-v0,ne=elementcount-e0;
 if(overflow||nv<=0||nv>R16_CHUNK_VERTS||ne<=0||ne>R16_CHUNK_VERTS){vertcount=v0;elementcount=e0;return NULL;}
 int packed=r31_compact_span(verts,v0,nv,indices,e0,ne);if(packed<0){vertcount=v0;elementcount=e0;return NULL;}nv=packed;
 size_t bytes=(size_t)nv*sizeof(Vertex)+(size_t)ne*sizeof(uint16_t);R62Mesh*slot=NULL;
 for(int i=0;i<R62_SLOTS;i++)if(!r62_meshes[i].vbo){slot=r62_meshes+i;break;}
 if(!slot)for(int i=0;i<R62_SLOTS;i++){R62Mesh*s=r62_meshes+i;if(s->queued!=dyn_cache_frame&&(!slot||(unsigned)(dyn_cache_frame-s->seen)>(unsigned)(dyn_cache_frame-slot->seen)))slot=s;}
 if(!slot||bytes>R62_BYTE_CAP){vertcount=v0;elementcount=e0;return NULL;}
 while(r62_bytes-slot->bytes+bytes>R62_BYTE_CAP){R62Mesh*old=NULL;for(int i=0;i<R62_SLOTS;i++){R62Mesh*s=r62_meshes+i;if(s!=slot&&s->vbo&&s->queued!=dyn_cache_frame&&(!old||s->seen<old->seen))old=s;}if(!old){vertcount=v0;elementcount=e0;return NULL;}r62_clear(old);}
 uint64_t available=(uint64_t)vglMemFree(VGL_MEM_RAM)+vglMemFree(VGL_MEM_VRAM)+slot->bytes;
 if(available<bytes+12u*1024u*1024u){vertcount=v0;elementcount=e0;return NULL;}
 for(int i=0;i<ne;i++){uint32_t ix=indices[e0+i];if(ix<(uint32_t)v0||ix>=(uint32_t)(v0+nv)){vertcount=v0;elementcount=e0;return NULL;}short_indices[i]=(uint16_t)(ix-v0);}
 r62_clear(slot);GLuint vbo=0,ibo=0;for(int i=0;i<8&&glGetError()!=GL_NO_ERROR;i++){}
 glGenBuffers(1,&vbo);glGenBuffers(1,&ibo);
 if(vbo&&ibo){glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,nv*sizeof(Vertex),verts+v0,GL_STATIC_DRAW);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ibo);glBufferData(GL_ELEMENT_ARRAY_BUFFER,ne*sizeof(uint16_t),short_indices,GL_STATIC_DRAW);}
 int good=vbo&&ibo&&glGetError()==GL_NO_ERROR;glBindBuffer(GL_ARRAY_BUFFER,0);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);vertcount=v0;elementcount=e0;
 if(!good){if(vbo)glDeleteBuffers(1,&vbo);if(ibo)glDeleteBuffers(1,&ibo);return NULL;}
 *slot=(R62Mesh){g->m,g->yaw,stamp,g->x,g->y,g->z,ne,g->signature,vbo,ibo,bytes,dyn_cache_frame,0};r62_bytes+=bytes;return slot;
}
static void r62_prepare(int enabled){
 r62_instance_n=r62_group_n=0;r62_saved_vertices=0;if(!enabled)return;
 for(int i=0;i<anim_count;i++){
  R16Model*a=anims+i;if(!r62_eligible(a->model,a->bitset)||!model_in_view(a->model,a->x-_World3D.eyeX,a->y-_World3D.eyeY,a->z-_World3D.eyeZ))continue;
  int at=0;for(;at<r62_group_n;at++)if(r62_groups[at].m==a->model&&r62_groups[at].yaw==a->yaw)break;
  if(at==r62_group_n){if(at==R62_SLOTS)continue;r62_groups[r62_group_n++]=(R62Group){a->model,a->yaw,a->x,a->y,a->z,a->bitset,0,0,NULL};}
  r62_groups[at].score+=(unsigned)a->model->face_count*3u;
 }
 R62Group*largest=NULL;
 for(int i=0;i<r62_group_n;i++){R62Group*g=r62_groups+i;g->signature=r62_signature(g->m);g->mesh=r62_find(g);if(g->mesh){g->mesh->seen=g->mesh->queued=dyn_cache_frame;}else if(!largest||g->score>largest->score)largest=g;}
 /* Warm the most duplicated visible crop before ordinary cache uploads.
  * The existing aggregate one-upload-per-frame limit remains in force. */
 if(largest)largest->mesh=r62_upload(largest);
}
static int r62_queue(Model*m,int yaw,int x,int y,int z,int bitset){
 if(!r62_eligible(m,bitset)||r62_instance_n==R62_INSTANCES)return 0;
 for(int i=0;i<r62_group_n;i++){R62Group*g=r62_groups+i;R62Mesh*s=g->mesh;if(g->m!=m||g->yaw!=yaw||!s||!s->vbo||s->epoch!=stamp||s->signature!=g->signature)continue;
  r62_instances[r62_instance_n++]=(R62Instance){s,x-s->x,y-s->y,z-s->z};s->seen=s->queued=dyn_cache_frame;r62_saved_vertices+=(unsigned)m->face_count*3u;return 1;
 }return 0;
}
static void r62_draw(void){
 if(!frame_ready||!r62_instance_n)return;R62Mesh*bound=NULL;
 for(int i=0;i<r62_instance_n;i++){R62Instance*instance=r62_instances+i;R62Mesh*s=instance->mesh;
  if(s!=bound){glBindBuffer(GL_ARRAY_BUFFER,s->vbo);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,s->ibo);glColorPointer(4,GL_UNSIGNED_BYTE,sizeof(Vertex),(const void*)offsetof(Vertex,r));glVertexPointer(3,GL_FLOAT,sizeof(Vertex),(const void*)offsetof(Vertex,x));glTexCoordPointer(2,GL_FLOAT,sizeof(Vertex),(const void*)offsetof(Vertex,u));bound=s;}
  glPushMatrix();glTranslatef((float)instance->dx,(float)instance->dy,(float)instance->dz);glDrawElements(GL_TRIANGLES,s->count,GL_UNSIGNED_SHORT,(const void*)0);glPopMatrix();
 }glBindBuffer(GL_ARRAY_BUFFER,0);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
}
