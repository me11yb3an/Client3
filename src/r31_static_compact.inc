/* Exact, per-upload vertex interning. Every indexed24-byte vertex is
 * preserved bit-for-bit; only identical vertices share an index. Alpha,
 * texture coordinates, face order and the already-partitioned index ranges
 * remain unchanged. Unreferenced vertices need no GPU storage. */
typedef char R31VertexMustBe24[(sizeof(Vertex)==24)?1:-1];
#define R31_TABLE_SIZE 131072u
static uint32_t r31_table[R31_TABLE_SIZE];
static uint16_t r31_remap[R16_CHUNK_VERTS];
static uint16_t r31_span_indices[R16_CHUNK_VERTS];
static unsigned r31_max_probe;
/* Compare the six stored words directly. Float equality would merge distinct
 * signed zeros/NaNs; copying to integer words keeps the original bit identity.
 * Vita GCC otherwise calls libc memcmp for each24-byte vertex comparison. */
static inline int r31_vertex_equal(const Vertex*a,const Vertex*b){
 uint32_t x[6],y[6];memcpy(x,a,sizeof x);memcpy(y,b,sizeof y);
 return x[0]==y[0]&&x[1]==y[1]&&x[2]==y[2]&&x[3]==y[3]&&x[4]==y[4]&&x[5]==y[5];
}
static uint32_t r31_hash(const Vertex*v){
 uint32_t words[6],h=2166136261u;memcpy(words,v,sizeof words);
 for(int i=0;i<6;i++)h=(h^words[i])*16777619u;
 /* Float positions often have zero low bits; fold all bits into the slot. */
 h^=h>>16;h*=0x85ebca6bu;h^=h>>13;h*=0xc2b2ae35u;return h^(h>>16);
}
static int r31_compact(Vertex*stream,int nv,uint16_t*ix,int ni){
 if(nv<0||nv>R16_CHUNK_VERTS||ni<0||ni>R16_CHUNK_VERTS)return -1;
 if(!nv&&!ni){r31_max_probe=0;return 0;}
 if(!stream||!ix)return -1;
 for(int i=0;i<ni;i++)if(ix[i]>=nv)return -1;
 unsigned slots=16;while(slots<(unsigned)nv*2)slots*=2;
 memset(r31_table,0,(size_t)slots*sizeof(*r31_table));memset(r31_remap,255,(size_t)nv*sizeof(*r31_remap));r31_max_probe=0;
 for(int i=0;i<ni;i++)r31_remap[ix[i]]=0;
 int count=0;
 for(int i=0;i<nv;i++){
  if(r31_remap[i]==65535u)continue;
  Vertex value=stream[i];uint32_t slot=r31_hash(&value)&(slots-1);unsigned probes=1;
  while(r31_table[slot]&&!r31_vertex_equal(stream+r31_table[slot]-1,&value)){
   slot=(slot+1)&(slots-1);probes++;
  }
  if(probes>r31_max_probe)r31_max_probe=probes;
  if(!r31_table[slot]){stream[count]=value;r31_table[slot]=(uint32_t)++count;}
  r31_remap[i]=(uint16_t)(r31_table[slot]-1);
 }
 for(int i=0;i<ni;i++)ix[i]=r31_remap[ix[i]];
 return count;
}
/* Compact a completed model/tile after its layer and water captures. Small
 * spans keep the hash working set in cache instead of scattering accesses
 * over a complete60000-vertex upload. Global index order stays identical. */
static int r31_compact_span(Vertex*stream,int first,int nv,uint32_t*ix,int first_index,int ni){
 if(first<0||nv<0||nv>R16_CHUNK_VERTS||first>R16_CHUNK_VERTS-nv||first_index<0||ni<0||ni>R16_CHUNK_VERTS||first_index>R16_CHUNK_VERTS-ni)return -1;
 if(!nv&&!ni){r31_max_probe=0;return 0;}
 if(!stream||!ix)return -1;
 for(int i=0;i<ni;i++){
  uint32_t v=ix[first_index+i];if(v<(uint32_t)first||v>=(uint32_t)(first+nv))return -1;
  r31_span_indices[i]=(uint16_t)(v-(uint32_t)first);
 }
 int count=r31_compact(stream+first,nv,r31_span_indices,ni);if(count<0)return -1;
 for(int i=0;i<ni;i++)ix[first_index+i]=(uint32_t)first+r31_span_indices[i];
 return count;
}
