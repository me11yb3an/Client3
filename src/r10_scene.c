#include "r13_capture.h"
#include "r12_waits.h"
#ifndef BA_R12_REBASE
#define BA_R12_REBASE 0
#endif
#include "r11_diag.h"
/* Independently authored retained geometry. No Bren code or GXP copied.
 * Preserve original visibility/picking/order by tagging the existing indices.
 * The six high bits identify one 16x16-tile chunk; low 24 are resident vertices.
 * GL/vitaGL owns in-flight GPU allocation retirement. No raw GPU writes. */
#include "r10_scene.h"
#ifndef BA_R11_SHARED_STATIC
#define BA_R11_SHARED_STATIC 0
#endif
#if BA_R10_CACHE
#include "gl11.h"
#include "custom.h"
#include "pix3d.h"
#include "platform.h"
#include "ba_timing.h"
#include "h8_remote.h"
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define GRID 7
#define CHUNKS (GRID*GRID)
#define ENTRIES 8192
#define LOOKUP_BUCKETS 16384
#define BUDGET (6u*1024u*1024u)
#define PER_CHUNK (512u*1024u)
#define TAG 0x80000000u
#define LOW 0x00ffffffu
extern SceneData _World3D;
extern Pix3D _Pix3D;
extern int recovery_water_reference;
typedef struct {
 const void *identity;
 const void *arrays[16];
 int kind,x,y,z,yaw,level;
 int minx,minz,maxx,maxz;
 unsigned first,count,nindices;
 uint32_t *pattern;
 int next,hash_next;
 uint64_t generation;
} Entry;
typedef struct {
 Vertex *vertices;
 unsigned count,capacity,vbo,uploaded_count;
 int dirty,gpu_valid,head,blocked;
 unsigned seen_frame,built_frame;
} Chunk;
typedef struct {
 uint64_t frame,hits,misses,builds,chunk_rebuilds,uploads,upload_bytes,draws,fallbacks;
 uint64_t emit_us,hit_us,store_us,upload_us,submit_us,invalidations;
 uint64_t generation,lookup_probes,project_us,faces_us,projected_vertices,model_selected_indices,generated_vertices,dynamic_upload_bytes,submitted_indices;
} Stats;
static Chunk chunks[CHUNKS];
#if BA_R11_SHARED_STATIC
static unsigned shared_vbo,shared_offsets[CHUNKS];static int shared_dirty=1;
#endif
static Entry entries[ENTRIES];
static int lookup_heads[LOOKUP_BUCKETS]; /* entry index + 1; zero is empty */
static uint64_t scene_generation=1;
static const void *model_pointers[65536];
static int pointer_table_full;
static int initialized,eligible,index_start,free_entry,animation_update;
static unsigned frame;
static int policy_valid,policy_lowmem,policy_reference;
static size_t cpu_capacity,pattern_bytes;
static Stats stats;
static struct {Stats value;size_t cpu,patterns;unsigned visible;} samples[128];
static unsigned sample_count,sample_drops,log_started;
void r10_flush(void){
#ifndef R10_HOST_TEST
 if(!sample_count)return;
 FILE*f=fopen("ux0:/data/RS2Vita/r10-scene.csv",log_started?"a":"w");
 if(!f)return;
 if(!log_started)fputs("frame,hits,misses,primitive_builds,chunk_rebuilds,uploads,upload_bytes,draws,fallbacks,cpu_capacity,pattern_bytes,visible_chunks,emit_us,lookup_us,store_us,upload_us,submit_us,invalidations,dropped_samples,generation,lookup_probes,project_us,faces_us,projected_vertices,model_selected_indices,generated_vertices,dynamic_upload_bytes,submitted_indices\n",f);
 for(unsigned i=0;i<sample_count;i++){Stats*s=&samples[i].value;fprintf(f,"%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%u,%u,%u,%llu,%llu,%llu,%llu,%llu,%llu,%u,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu\n",(unsigned long long)s->frame,(unsigned long long)s->hits,(unsigned long long)s->misses,(unsigned long long)s->builds,(unsigned long long)s->chunk_rebuilds,(unsigned long long)s->uploads,(unsigned long long)s->upload_bytes,(unsigned long long)s->draws,(unsigned long long)s->fallbacks,(unsigned)samples[i].cpu,(unsigned)samples[i].patterns,samples[i].visible,(unsigned long long)s->emit_us,(unsigned long long)s->hit_us,(unsigned long long)s->store_us,(unsigned long long)s->upload_us,(unsigned long long)s->submit_us,(unsigned long long)s->invalidations,sample_drops,(unsigned long long)s->generation,(unsigned long long)s->lookup_probes,(unsigned long long)s->project_us,(unsigned long long)s->faces_us,(unsigned long long)s->projected_vertices,(unsigned long long)s->model_selected_indices,(unsigned long long)s->generated_vertices,(unsigned long long)s->dynamic_upload_bytes,(unsigned long long)s->submitted_indices);}
 fclose(f);sample_count=0;log_started=1;
#endif
}
static struct {const void *key;int kind,x,z,level,base,indices,active;} pending;
static int slot(int x,int z){return x<0||z<0||x>=104||z>=104?-1:(x>>4)*GRID+(z>>4);}
static uint32_t tagged(int chunk,unsigned vertex){return TAG|((uint32_t)chunk<<24)|vertex;}
static unsigned hashptr(const void*p){uintptr_t n=(uintptr_t)p;return (unsigned)((n>>4)^(n>>15)^(n>>25))&65535u;}
static unsigned entry_hash(const void*key,int kind,int x,int y,int z,int yaw,int level){
 uint32_t h=hashptr(key)^((uint32_t)kind*0x9e3779b9u);
 const int values[]={x,y,z,yaw,level};
 for(unsigned i=0;i<5;i++){h^=(uint32_t)values[i]+0x9e3779b9u+(h<<6)+(h>>2);}
 return (h^(h>>16))&(LOOKUP_BUCKETS-1);
}
static void index_entry(int n){Entry*e=&entries[n];unsigned h=entry_hash(e->identity,e->kind,e->x,e->y,e->z,e->yaw,e->level);e->hash_next=lookup_heads[h];lookup_heads[h]=n+1;}
static void remember(const void*p){if(p){unsigned h=hashptr(p);for(unsigned n=0;n<65536;n++,h=(h+1)&65535u){if(!model_pointers[h]||model_pointers[h]==p){model_pointers[h]=p;return;}}pointer_table_full=1;}}
static int remembered(const void*p){if(!p)return 0;if(pointer_table_full)return 1;unsigned h=hashptr(p);for(unsigned n=0;n<65536;n++,h=(h+1)&65535u){if(model_pointers[h]==p)return 1;if(!model_pointers[h])return 0;}return 1;}
static void model_arrays(Model*m,const void **a){
 a[0]=m;a[1]=m->vertices_x;a[2]=m->vertices_y;a[3]=m->vertices_z;
 a[4]=m->face_color_a;a[5]=m->face_color_b;a[6]=m->face_color_c;
 a[7]=m->face_infos;a[8]=m->face_alphas;a[9]=m->face_colors;
 a[10]=m->face_indices_a;a[11]=m->textured_p_coordinate;
 a[12]=m->textured_m_coordinate;a[13]=m->textured_n_coordinate;
 a[14]=m->face_indices_b;a[15]=m->face_indices_c;
}
static void init(void){if(initialized)return;initialized=1;for(int i=0;i<CHUNKS;i++)chunks[i].head=-1;for(int i=0;i<ENTRIES;i++)entries[i].next=i+1;entries[ENTRIES-1].next=-1;free_entry=0;}
static void clear_chunk(int id){
 Chunk*c=&chunks[id];int n=c->head;
#if BA_R11_SHARED_STATIC
 shared_dirty=1;
#endif
 while(n>=0){Entry*e=&entries[n];int next=e->next;pattern_bytes-=e->nindices*sizeof(uint32_t);free(e->pattern);memset(e,0,sizeof(*e));e->next=free_entry;free_entry=n;n=next;}
 if(c->vbo)r11_delete_buffers(1,&c->vbo);
 cpu_capacity-=(size_t)c->capacity*sizeof(Vertex);free(c->vertices);
 memset(c,0,sizeof(*c));c->head=-1;
}
void r10_shutdown(void){
#if BA_R11_SHARED_STATIC
 if(shared_vbo)r11_delete_buffers(1,&shared_vbo);shared_vbo=0;shared_dirty=1;
#endif
if(!initialized)return;r10_flush();for(int i=0;i<CHUNKS;i++)clear_chunk(i);memset(lookup_heads,0,sizeof(lookup_heads));memset(model_pointers,0,sizeof(model_pointers));pointer_table_full=0;initialized=0;pending.active=0;if(++scene_generation==0)scene_generation=1;}
void r10_reset(void){init();if(++scene_generation==0)scene_generation=1;for(int i=0;i<CHUNKS;i++){chunks[i].dirty=1;chunks[i].blocked=1;}stats.invalidations++;r16_dirty();}
void r10_frame_begin(void){
 init();frame++;stats.frame++;stats.generation=scene_generation;eligible=0;pending.active=0;
 if(policy_valid&&(policy_lowmem!=_Pix3D.lowMemory||policy_reference!=recovery_water_reference))r10_reset();
 policy_valid=1;policy_lowmem=_Pix3D.lowMemory;policy_reference=recovery_water_reference;
 int cleared=0;for(int i=0;i<CHUNKS;i++)if(chunks[i].dirty){clear_chunk(i);cleared=1;}
 if(cleared){memset(lookup_heads,0,sizeof(lookup_heads));memset(model_pointers,0,sizeof(model_pointers));pointer_table_full=0;for(int i=0;i<CHUNKS;i++)for(int n=chunks[i].head;n>=0;n=entries[n].next){index_entry(n);if(entries[n].kind==3)for(int k=0;k<16;k++)remember(entries[n].arrays[k]);}}
 if(frame%30u==0){unsigned visible=0;for(int i=0;i<CHUNKS;i++)visible+=chunks[i].seen_frame+1u==frame;
 if(sample_count<128){samples[sample_count].value=stats;samples[sample_count].cpu=cpu_capacity;samples[sample_count].patterns=pattern_bytes;samples[sample_count++].visible=visible;}else sample_drops++;
#ifndef R10_HOST_TEST
 if(!h8_is_running()&&!h5_measuring())r10_flush();
#endif
 }
}
void r10_dirty_rect(int x0,int z0,int x1,int z1){
 /* pushLocs replaces only animated model instances. Their geometry is dynamic;
  * the ordinary traversal still selects them each frame. Neighboring terrain
  * and unrelated static vertices do not change when their frame advances. */
 if(animation_update)return;
 r16_dirty();
 init();if(x1<x0||z1<z0)return;
 for(int i=0;i<CHUNKS;i++){
  Chunk*c=&chunks[i];int cx=(i/GRID)*16,cz=(i%GRID)*16;
  int touch=cx<=x1&&cx+15>=x0&&cz<=z1&&cz+15>=z0;
  for(int n=c->head;!touch&&n>=0;n=entries[n].next){Entry*e=&entries[n];touch=e->minx<=x1&&e->maxx>=x0&&e->minz<=z1&&e->maxz>=z0;}
  if(touch){c->dirty=1;c->blocked=1;}
 }
 stats.invalidations++;
}
void r10_model_changed(Model*m){r10_model_changed_mask(m,0xffffu);}
void r10_model_changed_mask(Model*m,unsigned mask){
 if(!initialized||!m)return;const void *a[16];model_arrays(m,a);int maybe=0;
 for(int k=0;k<16;k++)if(mask&(1u<<k))maybe|=remembered(a[k]);if(!maybe)return;
 for(int i=0;i<CHUNKS;i++)for(int n=chunks[i].head;n>=0;n=entries[n].next){Entry*e=&entries[n];if(e->kind!=3)continue;
  int match=0;for(int k=0;k<16&&!match;k++)if(a[k]&&(mask&(1u<<k)))for(int j=0;j<16;j++)if(a[k]==e->arrays[j]){match=1;break;}
  if(match){chunks[i].dirty=1;chunks[i].blocked=1;break;}
 }
}
void r10_hint(int value){eligible=value;}
void r10_animation_update(int value){animation_update=value;}
void r10_emit_time(uint64_t elapsed){stats.emit_us+=elapsed;}
void r10_model_work(uint64_t project_us,uint64_t faces_us,unsigned vertices,unsigned selected){stats.project_us+=project_us;stats.faces_us+=faces_us;stats.projected_vertices+=vertices;stats.model_selected_indices+=selected;}
void r10_model_indices(int start){index_start=start;}
static Entry *find(int id,const void *key,int kind,int x,int y,int z,int yaw,int level){
 Chunk*c=&chunks[id];if(c->blocked)return NULL;
 unsigned h=entry_hash(key,kind,x,y,z,yaw,level);
 for(int link=lookup_heads[h];link;link=entries[link-1].hash_next){stats.lookup_probes++;Entry*e=&entries[link-1];if(e->generation==scene_generation&&e->identity==key&&e->kind==kind&&e->x==x&&e->y==y&&e->z==z&&e->yaw==yaw&&e->level==level)return e;}
 return NULL;
}
static int animated_model(Model*m){
 if(!m||m->r10_animated||m->face_count<=0||m->face_count>20000||!m->face_color_a||!m->face_color_b||!m->face_color_c)return 1;
 if(m->model_priority)return 1;
 for(int f=0;f<m->face_count;f++)if((m->face_alphas&&m->face_alphas[f])||(m->face_priorities&&m->face_priorities[f]))return 1;
 for(int f=0;f<m->face_count;f++)if(m->face_infos&&m->face_colors&&((m->face_infos[f]&3)==2||(m->face_infos[f]&3)==3)&&(m->face_colors[f]==17||m->face_colors[f]==24))return 1;
 return 0;
}
static int map_model(Entry*e,int id,int base){
 for(int n=index_start;n<elementcount;n++)if(indices[n]<(unsigned)base||indices[n]-(unsigned)base>=e->count)return 0;
 for(int n=index_start;n<elementcount;n++)indices[n]=tagged(id,e->first+indices[n]-(unsigned)base);
 chunks[id].seen_frame=frame;return 1;
}
int r10_model_hit(Model*m,int x,int y,int z,int yaw){
 if(!eligible||m->r10_animated||x<0||z<0)return 0;int id=slot(x/128,z/128);if(id<0)return 0;
 uint64_t at=ba_timing_now();Entry*e=find(id,m,3,x,y,z,yaw,0);
 if(e&&map_model(e,id,vertcount)){stats.hits++;stats.hit_us+=ba_timing_now()-at;return 1;}
 stats.misses++;stats.hit_us+=ba_timing_now()-at;return 0;
}
static Entry*store_impl(int id,const void *key,int kind,int x,int y,int z,int yaw,int level,int base,int first,Model*m){
 Chunk*c=&chunks[id];unsigned count=(unsigned)(vertcount-base),ni=(unsigned)(elementcount-first);
 if(c->blocked||!count||free_entry<0||count>PER_CHUNK/sizeof(Vertex)){stats.fallbacks++;return NULL;}
 for(int k=first;k<elementcount;k++)if(indices[k]<(unsigned)base||indices[k]-(unsigned)base>=count){stats.fallbacks++;return NULL;}
 uint32_t *pattern=NULL;
 if(kind!=3&&ni){
  if(pattern_bytes+ni*sizeof(uint32_t)>256u*1024u){stats.fallbacks++;return NULL;}
  pattern=malloc(ni*sizeof(uint32_t));if(!pattern){stats.fallbacks++;return NULL;}
  for(unsigned k=0;k<ni;k++)pattern[k]=indices[first+k]-(unsigned)base;
 }
 unsigned need=c->count+count;
 if(need>c->capacity){
  unsigned capacity=(need+255u)&~255u;size_t extra=(size_t)(capacity-c->capacity)*sizeof(Vertex);
  if(capacity>PER_CHUNK/sizeof(Vertex)||cpu_capacity+extra>BUDGET){free(pattern);stats.fallbacks++;return NULL;}
  Vertex*p=realloc(c->vertices,(size_t)capacity*sizeof(Vertex));if(!p){free(pattern);stats.fallbacks++;return NULL;}
  c->vertices=p;cpu_capacity+=extra;c->capacity=capacity;
 }
 Entry*e=&entries[free_entry];int n=free_entry;free_entry=e->next;memset(e,0,sizeof(*e));
 e->identity=key;e->kind=kind;e->x=x;e->y=y;e->z=z;e->yaw=yaw;e->level=level;e->first=c->count;e->count=count;e->pattern=pattern;e->nindices=kind==3?0:ni;
 e->minx=e->maxx=x/128;e->minz=e->maxz=z/128;
 if(m){model_arrays(m,e->arrays);for(int k=0;k<16;k++)remember(e->arrays[k]);int radius=m->radius/128+2;e->minx-=radius;e->maxx+=radius;e->minz-=radius;e->maxz+=radius;}
 else {e->maxx++;e->maxz++;}
 #if BA_R11_SHARED_STATIC
 shared_dirty=1;
#endif
 memcpy(c->vertices+c->count,verts+base,(size_t)count*sizeof(Vertex));c->count=need;e->next=c->head;c->head=n;e->generation=scene_generation;index_entry(n);pattern_bytes+=e->nindices*sizeof(uint32_t);
 for(int k=first;k<elementcount;k++)indices[k]=tagged(id,e->first+indices[k]-(unsigned)base);
 vertcount=base;c->seen_frame=frame;stats.generated_vertices+=count;stats.builds++;if(c->built_frame!=frame){stats.chunk_rebuilds++;c->built_frame=frame;}return e;
}
static Entry*store(int id,const void*key,int kind,int x,int y,int z,int yaw,int level,int base,int first,Model*m){uint64_t at=ba_timing_now();Entry*e=store_impl(id,key,kind,x,y,z,yaw,level,base,first,m);stats.store_us+=ba_timing_now()-at;return e;}
void r10_model_store(Model*m,int x,int y,int z,int yaw,int base){
 if(!eligible||x<0||z<0||animated_model(m))return;int id=slot(x/128,z/128);if(id>=0)store(id,m,3,x,y,z,yaw,0,base,index_start,m);
}
int r10_surface_begin(const void *key,int kind,int x,int z,int level,int animated){
 pending.active=0;if(animated)return 0;int id=slot(x,z);if(id<0)return 0;
 uint64_t at=ba_timing_now();
 Entry*e=find(id,key,kind,x*128,0,z*128,0,level);
 if(e){gl_reserve_geometry(0,e->nindices);for(unsigned k=0;k<e->nindices;k++)indices[elementcount++]=tagged(id,e->first+e->pattern[k]);chunks[id].seen_frame=frame;stats.hits++;stats.hit_us+=ba_timing_now()-at;return 1;}
 stats.misses++;pending.key=key;pending.kind=kind;pending.x=x;pending.z=z;pending.level=level;pending.base=vertcount;pending.indices=elementcount;pending.active=1;stats.hit_us+=ba_timing_now()-at;return 0;
}
void r10_surface_end(void){if(pending.active){store(slot(pending.x,pending.z),pending.key,pending.kind,pending.x*128,0,pending.z*128,0,pending.level,pending.base,pending.indices,NULL);pending.active=0;}}
static void arrays(unsigned vbo,const Vertex*data){
 glBindBuffer(GL_ARRAY_BUFFER,vbo);
 glColorPointer(4,GL_UNSIGNED_BYTE,sizeof(Vertex),vbo?(const void*)offsetof(Vertex,r):(const void*)&data[0].r);
 glVertexPointer(3,GL_FLOAT,sizeof(Vertex),vbo?(const void*)offsetof(Vertex,x):(const void*)&data[0].x);
 glTexCoordPointer(2,GL_FLOAT,sizeof(Vertex),vbo?(const void*)offsetof(Vertex,u):(const void*)&data[0].u);
}
#if BA_R11_SHARED_STATIC
/* One logical static address space. Ordering and dynamic boundaries remain.
 * New allocation is published only after successful upload. Old VBO is retired
 * through GL, never overwritten while potentially in flight. */
static int shared_prepare(void){
 if(!shared_dirty)return shared_vbo!=0;
 unsigned offsets[CHUNKS],total=0;for(int i=0;i<CHUNKS;i++){offsets[i]=total;total+=chunks[i].count;}
 if(!total||total>BUDGET/sizeof(Vertex))return 0;
 Vertex*copy=malloc((size_t)total*sizeof(Vertex));if(!copy)return 0;
 for(int i=0;i<CHUNKS;i++)if(chunks[i].count)memcpy(copy+offsets[i],chunks[i].vertices,(size_t)chunks[i].count*sizeof(Vertex));
 unsigned candidate=0;r11_gen_buffers(1,&candidate);int ok=0;
 if(candidate){glBindBuffer(GL_ARRAY_BUFFER,candidate);r11_buffer_data(GL_ARRAY_BUFFER,(size_t)total*sizeof(Vertex),copy,GL_STATIC_DRAW);ok=glGetError()==GL_NO_ERROR;}
 free(copy);stats.uploads++;stats.upload_bytes+=(uint64_t)total*sizeof(Vertex);
 if(!ok){if(candidate)r11_delete_buffers(1,&candidate);return 0;}
 if(shared_vbo)r11_delete_buffers(1,&shared_vbo);shared_vbo=candidate;memcpy(shared_offsets,offsets,sizeof(offsets));shared_dirty=0;
 for(int i=0;i<CHUNKS;i++)if(chunks[i].vbo){r11_delete_buffers(1,&chunks[i].vbo);chunks[i].vbo=0;chunks[i].gpu_valid=0;}
 return 1;
}
#endif
void r10_draw(unsigned dynamic_vbo,int use_vbo){
 /* Validate every tagged reference before dereference or index mutation. */
 if(vertcount<0||elementcount<0||elementcount%3)return;
 for(int k=0;k<elementcount;k++){
  uint32_t ix=indices[k];
  if(ix&TAG){unsigned id=(ix>>24)&63u;
   if(id>=CHUNKS||!chunks[id].vertices||(ix&LOW)>=chunks[id].count){rs2_log("R13 invalid static index\n");return;}
  }else if(ix>=(unsigned)vertcount){rs2_log("R13 invalid dynamic index\n");return;}
 }

 if(r14_selected_begin((unsigned)elementcount)){
  for(int k=0;k<elementcount;k++){uint32_t ix=indices[k];const Vertex*v=(ix&TAG)?chunks[(ix>>24)&63].vertices+(ix&LOW):verts+ix;r14_selected_vertex(v);}
  r14_selected_end();
 }
 stats.generated_vertices+=(unsigned)vertcount;stats.submitted_indices+=(unsigned)elementcount;
 if(use_vbo)stats.dynamic_upload_bytes+=(uint64_t)(unsigned)vertcount*sizeof(Vertex);
 int shared=0;
#if BA_R11_SHARED_STATIC
 {uint64_t a=ba_timing_now();shared=shared_prepare();stats.upload_us+=ba_timing_now()-a;}
#endif
 if(!shared)for(int i=0;i<CHUNKS;i++){Chunk*c=&chunks[i];if(c->seen_frame!=frame||!c->count)continue;
  if(!c->gpu_valid||c->uploaded_count!=c->count){
   uint64_t at=ba_timing_now();if(!c->vbo)r11_gen_buffers(1,&c->vbo);
   if(c->vbo){glBindBuffer(GL_ARRAY_BUFFER,c->vbo);r11_buffer_data(GL_ARRAY_BUFFER,c->count*sizeof(Vertex),c->vertices,GL_STATIC_DRAW);c->gpu_valid=glGetError()==GL_NO_ERROR;}
   c->uploaded_count=c->count;stats.uploads++;stats.upload_bytes+=(uint64_t)c->count*sizeof(Vertex);stats.upload_us+=ba_timing_now()-at;
  }
 }
 r11_cache(stats.hits,stats.misses,stats.builds,stats.upload_bytes,stats.dynamic_upload_bytes,cpu_capacity,pattern_bytes);
 uint64_t at=ba_timing_now();
 for(int first=0;first<elementcount;){
  uint32_t tag=indices[first]&0xff000000u;int end=first;
  if(shared&&(tag&TAG)){
   while(end<elementcount&&(indices[end]&TAG))end++;
#if BA_R11_SHARED_STATIC
   arrays(shared_vbo,NULL);
   for(int k=first;k<end;k++){unsigned id=(indices[k]>>24)&63;indices[k]=(indices[k]&LOW)+shared_offsets[id];}
#endif
   tag=TAG;
  }else{
   while(end<elementcount&&(indices[end]&0xff000000u)==tag)end++;
   if(tag&TAG){int id=(tag>>24)&63;Chunk*c=&chunks[id];arrays(c->gpu_valid?c->vbo:0,c->vertices);for(int k=first;k<end;k++)indices[k]&=LOW;}
   else arrays(use_vbo?dynamic_vbo:0,verts);
  }
  unsigned low=UINT32_MAX,high=0;
  for(int k=first;k<end;k++){if(indices[k]<low)low=indices[k];if(indices[k]>high)high=indices[k];}
#if BA_R12_REBASE
  if(low){
   if(tag&TAG){int id=(tag>>24)&63;Chunk*c=&chunks[id];
    if(c->gpu_valid){glBindBuffer(GL_ARRAY_BUFFER,c->vbo);glColorPointer(4,GL_UNSIGNED_BYTE,sizeof(Vertex),(const void*)((size_t)low*sizeof(Vertex)+offsetof(Vertex,r)));glVertexPointer(3,GL_FLOAT,sizeof(Vertex),(const void*)((size_t)low*sizeof(Vertex)+offsetof(Vertex,x)));glTexCoordPointer(2,GL_FLOAT,sizeof(Vertex),(const void*)((size_t)low*sizeof(Vertex)+offsetof(Vertex,u)));}
    else arrays(0,c->vertices+low);
   }else if(use_vbo){glBindBuffer(GL_ARRAY_BUFFER,dynamic_vbo);glColorPointer(4,GL_UNSIGNED_BYTE,sizeof(Vertex),(const void*)((size_t)low*sizeof(Vertex)+offsetof(Vertex,r)));glVertexPointer(3,GL_FLOAT,sizeof(Vertex),(const void*)((size_t)low*sizeof(Vertex)+offsetof(Vertex,x)));glTexCoordPointer(2,GL_FLOAT,sizeof(Vertex),(const void*)((size_t)low*sizeof(Vertex)+offsetof(Vertex,u)));}
   else arrays(0,verts+low);
   for(int k=first;k<end;k++)indices[k]-=low;high-=low;low=0;
  }
#endif
  r13_draw(low,high,end-first);
  r12_index_range(low,high,end-first);
  r11_run(tag,first,end-first);
  glDrawElements(GL_TRIANGLES,end-first,GL_UNSIGNED_INT,indices+first);stats.draws++;first=end;
 }
 glBindBuffer(GL_ARRAY_BUFFER,0);stats.submit_us+=ba_timing_now()-at;
}
#endif
