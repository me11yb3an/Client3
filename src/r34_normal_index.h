/* Exact lighting-normal coordinate index; source coordinate arrays remain
 * immutable during one world lighting pass. Chains preserve ascending order. */
#define R34_MAX_VERTICES 10000
#define R34_BUCKETS 32768u
#define R34_CACHE_SLOTS 16
#define R34_CACHE_VERTICES 512
typedef struct{const int*x,*y,*z;int count;unsigned slots;int heads[1024],next[R34_CACHE_VERTICES];}R34CoordinateCache;
static R34CoordinateCache r34_cache[R34_CACHE_SLOTS];static unsigned r34_used,r34_next_slot,r34_cache_hits,r34_builds;
static int r34_scratch_heads[R34_BUCKETS],r34_scratch_next[R34_MAX_VERTICES];
static int*r34_heads,*r34_next;static unsigned r34_slots;
static void r34_reset(void){r34_used=r34_next_slot=r34_cache_hits=r34_builds=0;}
static unsigned r34_hash(int x,int y,int z){
 unsigned h=(unsigned)x*0x9e3779b1u^(unsigned)y*0x85ebca6bu^(unsigned)z*0xc2b2ae35u;
 h^=h>>16;h*=0x7feb352du;return (h^(h>>15))&(r34_slots-1);
}
static int r34_prepare(const int*x,const int*y,const int*z,int na,int nb){
 if(na<4||nb<4||nb>R34_MAX_VERTICES||(uint64_t)na*(unsigned)nb<128u)return 0;
 if(nb<=R34_CACHE_VERTICES){
  for(unsigned i=0;i<r34_used;i++){R34CoordinateCache*c=r34_cache+i;if(c->x==x&&c->y==y&&c->z==z&&c->count==nb){r34_heads=c->heads;r34_next=c->next;r34_slots=c->slots;r34_cache_hits++;return 1;}}
  unsigned slot=r34_used<R34_CACHE_SLOTS?r34_used++:r34_next_slot++%R34_CACHE_SLOTS;R34CoordinateCache*c=r34_cache+slot;
  c->x=x;c->y=y;c->z=z;c->count=nb;c->slots=16;while(c->slots<(unsigned)nb*2)c->slots*=2;
  r34_heads=c->heads;r34_next=c->next;r34_slots=c->slots;
 }else{r34_heads=r34_scratch_heads;r34_next=r34_scratch_next;r34_slots=16;while(r34_slots<(unsigned)nb*2)r34_slots*=2;}
 memset(r34_heads,255,r34_slots*sizeof(*r34_heads));
 for(int i=nb-1;i>=0;i--){unsigned h=r34_hash(x[i],y[i],z[i]);r34_next[i]=r34_heads[h];r34_heads[h]=i;}
 r34_builds++;return 1;
}
