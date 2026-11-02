/* Reuse exact duplicate-vertex patterns within one bake only. Every hit
 * validates all referenced24-byte vertices before changing the stream.
 * A failed validation falls back to the existing exact interner. */
#define R35_SLOTS 64
#define R35_VERTICES 4096
typedef struct {const void*key[3];int nv,ni,count;uint16_t map[R35_VERTICES],source[R35_VERTICES];}R35Pattern;
static R35Pattern r35_patterns[R35_SLOTS];
static const void*r35_key[3];static unsigned r35_used,r35_next;
static uint64_t r35_hits,r35_misses,r35_rejected;
static void r35_reset(void){r35_used=r35_next=0;memset(r35_key,0,sizeof r35_key);r35_hits=r35_misses=r35_rejected=0;}
static int r35_compact_span(Vertex*stream,int first,int nv,uint32_t*ix,int first_index,int ni){
 if(!r35_key[0]||nv<=0||nv>R35_VERTICES||first<0||first>R16_CHUNK_VERTS-nv||first_index<0||ni<0||ni>R16_CHUNK_VERTS||first_index>R16_CHUNK_VERTS-ni||!stream||!ix)return r31_compact_span(stream,first,nv,ix,first_index,ni);
 R35Pattern*entry=NULL;
 for(unsigned slot=0;slot<r35_used;slot++){R35Pattern*p=r35_patterns+slot;if(p->nv==nv&&p->ni==ni&&!memcmp(p->key,r35_key,sizeof r35_key)){entry=p;break;}}
 if(entry){
  int valid=1;
  for(int i=0;i<ni;i++){
   uint32_t old=ix[first_index+i];
   if(old<(unsigned)first||old>=(unsigned)(first+nv)){valid=0;break;}
   unsigned local=old-first,mapped=entry->map[local];
   if(mapped>=entry->count||(entry->source[mapped]!=local&&!r31_vertex_equal(stream+old,stream+first+entry->source[mapped]))){valid=0;break;}
  }
  if(valid){
   for(int i=0;i<entry->count;i++)stream[first+i]=stream[first+entry->source[i]];
   for(int i=0;i<ni;i++)ix[first_index+i]=(uint32_t)first+entry->map[ix[first_index+i]-first];
   r35_hits++;return entry->count;
  }
  r35_rejected++;
 }
 r35_misses++;
 int count=r31_compact_span(stream,first,nv,ix,first_index,ni);if(count<0)return count;
 if(!entry){unsigned slot=r35_used<R35_SLOTS?r35_used++:r35_next++%R35_SLOTS;entry=r35_patterns+slot;}
 memcpy(entry->key,r35_key,sizeof r35_key);entry->nv=nv;entry->ni=ni;entry->count=count;memcpy(entry->map,r31_remap,(size_t)nv*sizeof(uint16_t));
 for(int i=0;i<count;i++)entry->source[i]=65535u;
 for(int i=0;i<nv;i++)if(entry->map[i]<(unsigned)count&&entry->source[entry->map[i]]==65535u)entry->source[entry->map[i]]=(uint16_t)i;
 return count;
}
