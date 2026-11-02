#include "r80_exact_words.h"
/* Share render vertices by original vertex ID and shade. Recompute positions
 * from the current posed model every frame. Exact material copies validate
 * templates; no pose, animation, model ownership or GPU buffer is cached. */
#define R79_SLOTS 32
#define R79_BYTES (4u*1024u*1024u)
typedef struct {int vertex,shade;}R79Vertex;
typedef struct {const int*ptr[6];int*key;R79Vertex*v;uint16_t*i;int source_vertices,faces,nv,ni;unsigned present,seen;size_t bytes;}R79Template;
static R79Template r79_templates[R79_SLOTS];
static size_t r79_bytes;
static unsigned r79_frame,r79_admitted,r79_hits,r79_misses,r79_saved;
static int r79_actor,r79_indexed;
static void r79_arrays(Model*m,const int**a){const int*p[]={m->face_indices_a,m->face_indices_b,m->face_indices_c,m->face_color_a,m->face_color_b,m->face_color_c,m->face_infos,m->face_alphas};for(int i=0;i<8;i++)a[i]=p[i];}
static void r79_clear(R79Template*t){if(t->bytes<=r79_bytes)r79_bytes-=t->bytes;else r79_bytes=0;free(t->key);free(t->v);free(t->i);memset(t,0,sizeof(*t));}
void gl_actor_cache_reset(void){for(int i=0;i<R79_SLOTS;i++)r79_clear(r79_templates+i);r79_actor=r79_indexed=0;}
void gl_actor_cache_frame(void){if(!++r79_frame){r79_frame=1;for(int i=0;i<R79_SLOTS;i++)r79_templates[i].seen=0;}r79_admitted=0;}
void gl_actor_span_begin(void){r79_actor=1;r79_indexed=0;}
int gl_actor_span_end(void){r79_actor=0;return r79_indexed;}
void gl_actor_cache_status(unsigned out[5]){out[4]=(unsigned)(r80_tested==1);out[0]=r79_hits;out[1]=r79_misses;out[2]=(unsigned)r79_bytes;out[3]=r79_saved;r79_hits=r79_misses=r79_saved=0;}
static int r79_match(R79Template*t,Model*m,const int**a,unsigned present){
 if(!t->key||t->faces!=m->face_count||t->source_vertices!=m->vertex_count||t->present!=present)return 0;
 for(int k=0;k<6;k++)if(t->ptr[k]!=a[k])return 0;
 const int*p=t->key;for(int k=0;k<8;k++)if(a[k]){if(!r80_words_equal(p,a[k],(size_t)m->face_count))return 0;p+=m->face_count;}return 1;
}
static R79Template*r79_build(Model*m,const int**a,unsigned present){
 if(r79_admitted)return NULL;
 for(int f=0;f<m->face_count;f++)if(a[7]&&a[7][f])return NULL;
 r79_admitted=1;
 static uint32_t hash_keys[32768];static uint16_t hash_values[32768],corners[12288];static R79Vertex unique[12288];
 memset(hash_keys,0,sizeof hash_keys);int nv=0,ni=0;
 for(int f=0;f<m->face_count;f++){
  int info=a[6]?a[6][f]:0;if(info==-1)continue;if(info&2)return NULL;
  int ids[3]={a[0][f],a[1][f],a[2][f]},shades[3]={a[3][f],a[4][f],a[5][f]};if((info&3)==1)shades[1]=shades[2]=shades[0];
  for(int k=0;k<3;k++){
   if(ids[k]<0||ids[k]>=m->vertex_count||shades[k]<0||shades[k]>65535||ni>=12288)return NULL;
   uint32_t key=((uint32_t)ids[k]<<16)|(unsigned)shades[k];key++;unsigned slot=(key*2654435761u)>>17;
   while(hash_keys[slot]&&hash_keys[slot]!=key)slot=(slot+1)&32767u;
   if(!hash_keys[slot]){if(nv>=12288)return NULL;hash_keys[slot]=key;hash_values[slot]=(uint16_t)nv;unique[nv++]=(R79Vertex){ids[k],shades[k]};}
   corners[ni++]=hash_values[slot];
  }
 }
 if(!nv||!ni)return NULL;
 int arrays=0;for(int k=0;k<8;k++)arrays+=a[k]!=NULL;
 size_t kb=(size_t)arrays*m->face_count*sizeof(int),vb=(size_t)nv*sizeof(R79Vertex),ib=(size_t)ni*sizeof(uint16_t),bytes=kb+vb+ib;if(bytes>R79_BYTES)return NULL;
 R79Template*t=NULL;for(int k=0;k<R79_SLOTS;k++)if(!r79_templates[k].key){t=r79_templates+k;break;}
 if(!t){for(int k=0;k<R79_SLOTS;k++)if(!t||(unsigned)(r79_frame-r79_templates[k].seen)>(unsigned)(r79_frame-t->seen))t=r79_templates+k;r79_clear(t);}
 while(r79_bytes+bytes>R79_BYTES){R79Template*old=NULL;for(int k=0;k<R79_SLOTS;k++)if(r79_templates[k].key&&(!old||(unsigned)(r79_frame-r79_templates[k].seen)>(unsigned)(r79_frame-old->seen)))old=r79_templates+k;if(!old)return NULL;r79_clear(old);}
 int*key=malloc(kb);R79Vertex*v=malloc(vb);uint16_t*ix=malloc(ib);if(!key||!v||!ix){free(key);free(v);free(ix);return NULL;}
 int*p=key;for(int k=0;k<8;k++)if(a[k]){memcpy(p,a[k],(size_t)m->face_count*sizeof(int));p+=m->face_count;}
 memcpy(v,unique,vb);memcpy(ix,corners,ib);*t=(R79Template){.key=key,.v=v,.i=ix,.source_vertices=m->vertex_count,.faces=m->face_count,.nv=nv,.ni=ni,.present=present,.seen=r79_frame,.bytes=bytes};for(int k=0;k<6;k++)t->ptr[k]=a[k];r79_bytes+=bytes;return t;
}
static int r79_emit(Model*m,const int*x,const int*y,const int*z){
 if(!r79_actor||!m||m->face_count<1||m->face_count>4096||m->vertex_count<1||m->vertex_count>8192)return 0;
 const int*a[8];r79_arrays(m,a);for(int k=0;k<6;k++)if(!a[k])return 0;
 unsigned present=0;for(int k=0;k<8;k++)if(a[k])present|=1u<<k;
 R79Template*t=NULL;for(int k=0;k<R79_SLOTS;k++)if(r79_match(r79_templates+k,m,a,present)){t=r79_templates+k;break;}
 if(t)r79_hits++;else{r79_misses++;t=r79_build(m,a,present);}if(!t)return 0;
 gl_reserve_geometry((unsigned)t->nv,(unsigned)t->ni);int base=vertcount;
 for(int k=0;k<t->nv;k++){int src=t->v[k].vertex,rgb=_Pix3D.palette[t->v[k].shade];Vertex*v=verts+vertcount++;*v=(Vertex){(rgb>>16)&255,(rgb>>8)&255,rgb&255,255,(float)x[src],(float)y[src],(float)z[src],0,0};}
 for(int k=0;k<t->ni;k++)indices[elementcount++]=(uint32_t)base+t->i[k];
 t->seen=r79_frame;r79_saved+=(unsigned)(t->ni-t->nv);r79_indexed=1;return 1;
}
