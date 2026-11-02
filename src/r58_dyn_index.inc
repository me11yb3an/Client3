/* Unchanged live scenery uses an exact-key bucket index. Complete legacy
 * validation still decides every hit, including resource-stage revisions.
 * Rebuild only after a slot changes; no model pointer survives slot clearing. */
#define R58_BUCKETS 1024
static int16_t r58_heads[R58_BUCKETS],r58_next[R16_DYN_SLOTS];
static int r58_dirty=1;
static unsigned r58_key(Model*m,int x,int y,int z,int yaw){
 uintptr_t p=(uintptr_t)m;uint32_t h=(uint32_t)p;
#if UINTPTR_MAX > UINT32_MAX
 h^=(uint32_t)(p>>32);
#endif
 h=(h^(uint32_t)x)*16777619u;h=(h^(uint32_t)y)*16777619u;h=(h^(uint32_t)z)*16777619u;h=(h^(uint32_t)yaw)*16777619u;
 h^=h>>16;h*=0x7feb352du;h^=h>>15;return h&(R58_BUCKETS-1);
}
static void r58_rebuild(void){
 for(int i=0;i<R58_BUCKETS;i++)r58_heads[i]=-1;
 /* Descending insertion preserves legacy first-slot match semantics. */
 for(int i=R16_DYN_SLOTS-1;i>=0;i--){DynGpu*s=dyn_gpu+i;r58_next[i]=-1;if(!s->ready||s->owner)continue;unsigned h=r58_key(s->model,s->x,s->y,s->z,s->yaw);r58_next[i]=r58_heads[h];r58_heads[h]=(int16_t)i;}
 r58_dirty=0;
}
static int r58_can_promote(void){return dyn_promotions<1&&dyn_draw_n<R16_DYN_SLOTS;}
static void r58_begin_frame(void){
 if(!++dyn_cache_frame){dyn_cache_frame=1;for(int i=0;i<R16_DYN_SLOTS;i++)dyn_gpu[i].queued_frame=0;}
 dyn_draw_n=0;dyn_promotions=0;
}
