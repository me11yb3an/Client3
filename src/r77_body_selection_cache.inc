#include "r80_exact_words.h"
/* Pure memoization of the existing clothing selector. The key contains every
 * candidate pair and every coordinate/material/priority value it can read.
 * No model pointers, animation guesses or hash-only matches are accepted. */
#define R77_SLOTS 128
#define R77_PAIR_LIMIT 256
#define R77_CACHE_BYTES (2u*1024u*1024u)
typedef struct {int*key;unsigned char*selected;size_t bytes;int words,faces,result;uint32_t hash,seen;}R77Selection;
static R77Selection r77_entries[R77_SLOTS];
static int r77_key[3+26*R77_PAIR_LIMIT],r77_words;
static uint32_t r77_hash,r77_serial,r77_hits,r77_misses;
static size_t r77_bytes;
static void r77_clear(R77Selection*p){if(p->bytes<=r77_bytes)r77_bytes-=p->bytes;else r77_bytes=0;free(p->key);free(p->selected);memset(p,0,sizeof(*p));}
static void r77_reset(void){for(int i=0;i<R77_SLOTS;i++)r77_clear(r77_entries+i);r77_words=0;}
static int r77_build_key(Model*m,R57Layers*p){
 r77_words=0;if(!p||p->count>R77_PAIR_LIMIT||m->face_count<1||m->face_count>8192)return 0;
 int n=0;r77_key[n++]=m->face_count;r77_key[n++]=(int)p->count;r77_key[n++]=m->model_priority;
 for(unsigned i=0;i<p->count;i++){
  int h=p->pairs[i].high,l=p->pairs[i].low,first=n;r77_key[n++]=h;r77_key[n++]=l;
  for(int k=0;k<24;k++)r77_key[n++]=0;
  if(h>=m->face_count||l>=m->face_count)continue;
  int ids[2]={h,l},at=first+2;
  for(int j=0;j<2;j++){int f=ids[j];r77_key[at++]=m->face_infos?m->face_infos[f]:0;r77_key[at++]=m->face_alphas?m->face_alphas[f]:0;r77_key[at++]=r57_priority(m,f);}
  if(!r57_solid(m,h)||!r57_solid(m,l))continue;
  int hp=r57_priority(m,h),lp=r57_priority(m,l);if(hp<1||hp>9||lp<0||lp>=hp)continue;
  if(!m->vertices_x||!m->vertices_y||!m->vertices_z||!m->face_indices_a||!m->face_indices_b||!m->face_indices_c)return 0;
  for(int j=0;j<2;j++){int f=ids[j],v[3]={m->face_indices_a[f],m->face_indices_b[f],m->face_indices_c[f]};for(int k=0;k<3;k++){if(v[k]<0||v[k]>=m->vertex_count)return 0;r77_key[at++]=m->vertices_x[v[k]];r77_key[at++]=m->vertices_y[v[k]];r77_key[at++]=m->vertices_z[v[k]];}}
 }
 uint32_t hash=2166136261u;for(int i=0;i<n;i++)hash=(hash^(uint32_t)r77_key[i])*16777619u;r77_words=n;r77_hash=hash;return 1;
}
static int r77_find(Model*m,R57Layers*p,unsigned char*selected,int*result){
 if(!++r77_serial){r77_serial=1;for(int i=0;i<R77_SLOTS;i++)r77_entries[i].seen=0;}
 if(!r77_build_key(m,p))return 0;
 for(int i=0;i<R77_SLOTS;i++){R77Selection*s=r77_entries+i;if(s->key&&s->hash==r77_hash&&s->words==r77_words&&r80_words_equal(s->key,r77_key,(size_t)r77_words)){memcpy(selected,s->selected,(size_t)s->faces);*result=s->result;s->seen=r77_serial;r77_hits++;return 1;}}
 r77_misses++;return 0;
}
static void r77_store(Model*m,const unsigned char*selected,int result){
 if(!r77_words)return;size_t bytes=(size_t)r77_words*sizeof(int)+(size_t)m->face_count;if(bytes>R77_CACHE_BYTES)return;
 R77Selection*target=NULL;for(int i=0;i<R77_SLOTS;i++)if(!r77_entries[i].key){target=r77_entries+i;break;}
 if(!target){for(int i=0;i<R77_SLOTS;i++)if(!target||(uint32_t)(r77_serial-r77_entries[i].seen)>(uint32_t)(r77_serial-target->seen))target=r77_entries+i;r77_clear(target);}
 while(r77_bytes+bytes>R77_CACHE_BYTES){R77Selection*old=NULL;for(int i=0;i<R77_SLOTS;i++)if(r77_entries[i].key&&(!old||(uint32_t)(r77_serial-r77_entries[i].seen)>(uint32_t)(r77_serial-old->seen)))old=r77_entries+i;if(!old)return;r77_clear(old);}
 int*key=malloc((size_t)r77_words*sizeof(int));unsigned char*out=malloc((size_t)m->face_count);if(!key||!out){free(key);free(out);return;}
 memcpy(key,r77_key,(size_t)r77_words*sizeof(int));memcpy(out,selected,(size_t)m->face_count);*target=(R77Selection){key,out,bytes,r77_words,m->face_count,result,r77_hash,r77_serial};r77_bytes+=bytes;
}
