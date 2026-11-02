/* Own the alternate roof view independently. No dynamic actors are cached here. */
typedef struct {
 int valid;
 World3D *world;
 int x,z,level,radius,cover,epoch,min_x,min_z,max_x,max_z;
 R16Chunk chunks[R16_MAX_CHUNKS],water_gpu[4];
 int chunk_n,static_count,static_verts,water_gpu_n,water_count,dyn_count,pick_count;
 R16Water water[R16_WATER_MAX];
 R16Dyn dyns[R16_DYN_MAX];
 GLuint trim_vbo,trim_ibo,clock_vbo,clock_ibo,floorcap_vbo,floorcap_ibo;
 int trim_count,clock_count,floorcap_count;
 GLuint decal_vbo,decal_ibo;
 int decal_count,decal_first[R26_LAYERS],decal_counts[R26_LAYERS];
} R16RoofSaved;
static R16RoofSaved roof_alternate,roof_swap_scratch;
static void roof_save(R16RoofSaved*s){
 memset(s,0,sizeof(*s));s->valid=key_ok;s->world=key_world;
 s->x=key_x;s->z=key_z;s->level=key_level;s->radius=key_radius;s->cover=key_cover;s->epoch=key_stamp;
 s->min_x=key_min_x;s->min_z=key_min_z;s->max_x=key_max_x;s->max_z=key_max_z;
 s->chunk_n=chunk_n;s->static_count=static_count;s->static_verts=static_verts;s->water_gpu_n=water_gpu_n;s->water_count=water_count;s->dyn_count=dyn_count;s->pick_count=pick_count;
 memcpy(s->chunks,chunks,sizeof chunks);memcpy(s->water_gpu,water_gpu,sizeof water_gpu);
 memcpy(s->water,water,(size_t)water_count*sizeof(*water));memcpy(s->dyns,dyns,(size_t)dyn_count*sizeof(*dyns));
 s->trim_vbo=trim_vbo;s->trim_ibo=trim_ibo;s->clock_vbo=clock_vbo;s->clock_ibo=clock_ibo;s->floorcap_vbo=floorcap_vbo;s->floorcap_ibo=floorcap_ibo;
 s->trim_count=trim_count;s->clock_count=clock_count;s->floorcap_count=floorcap_count;
 s->decal_vbo=r26_vbo;s->decal_ibo=r26_ibo;s->decal_count=r26_count;
 memcpy(s->decal_first,r26_first,sizeof r26_first);memcpy(s->decal_counts,r26_counts,sizeof r26_counts);
}
static void roof_detach(void){
 memset(chunks,0,sizeof chunks);memset(water_gpu,0,sizeof water_gpu);
 chunk_n=static_count=static_verts=water_gpu_n=water_count=dyn_count=pick_count=0;
 trim_vbo=trim_ibo=clock_vbo=clock_ibo=floorcap_vbo=floorcap_ibo=0;
 trim_count=clock_count=floorcap_count=0;key_ok=0;
 r26_vbo=r26_ibo=0;r26_count=0;memset(r26_first,0,sizeof r26_first);memset(r26_counts,0,sizeof r26_counts);
}
static void roof_restore(const R16RoofSaved*s){
 key_ok=s->valid;key_world=s->world;key_x=s->x;key_z=s->z;key_level=s->level;key_radius=s->radius;key_cover=s->cover;key_stamp=s->epoch;
 key_min_x=s->min_x;key_min_z=s->min_z;key_max_x=s->max_x;key_max_z=s->max_z;
 chunk_n=s->chunk_n;static_count=s->static_count;static_verts=s->static_verts;water_gpu_n=s->water_gpu_n;water_count=s->water_count;dyn_count=s->dyn_count;pick_count=s->pick_count;
 memcpy(chunks,s->chunks,sizeof chunks);memcpy(water_gpu,s->water_gpu,sizeof water_gpu);
 memcpy(water,s->water,(size_t)water_count*sizeof(*water));memcpy(dyns,s->dyns,(size_t)dyn_count*sizeof(*dyns));
 trim_vbo=s->trim_vbo;trim_ibo=s->trim_ibo;clock_vbo=s->clock_vbo;clock_ibo=s->clock_ibo;floorcap_vbo=s->floorcap_vbo;floorcap_ibo=s->floorcap_ibo;
 trim_count=s->trim_count;clock_count=s->clock_count;floorcap_count=s->floorcap_count;
 r26_vbo=s->decal_vbo;r26_ibo=s->decal_ibo;r26_count=s->decal_count;
 memcpy(r26_first,s->decal_first,sizeof r26_first);memcpy(r26_counts,s->decal_counts,sizeof r26_counts);
}
static void roof_drop_saved(R16RoofSaved*s){
 /* Region construction invalidates once per tile. An already empty saved
  * view owns no buffers and needs no repeated clearing of its large arrays.
  * Check ownership as well as valid: a partially saved/failed view must still
  * release its resources. The first real drop continues to zero everything. */
 if(!s->valid&&!s->chunk_n&&!s->water_gpu_n&&!s->trim_vbo&&!s->trim_ibo&&!s->clock_vbo&&!s->clock_ibo&&!s->floorcap_vbo&&!s->floorcap_ibo&&!s->decal_vbo&&!s->decal_ibo)return;
 for(int i=0;i<s->chunk_n;i++){if(s->chunks[i].vbo)glDeleteBuffers(1,&s->chunks[i].vbo);if(s->chunks[i].ibo)glDeleteBuffers(1,&s->chunks[i].ibo);}
 for(int i=0;i<s->water_gpu_n;i++){if(s->water_gpu[i].vbo)glDeleteBuffers(1,&s->water_gpu[i].vbo);if(s->water_gpu[i].ibo)glDeleteBuffers(1,&s->water_gpu[i].ibo);}
 GLuint ids[]={s->trim_vbo,s->trim_ibo,s->clock_vbo,s->clock_ibo,s->floorcap_vbo,s->floorcap_ibo,s->decal_vbo,s->decal_ibo};
 for(unsigned i=0;i<sizeof(ids)/sizeof(*ids);i++)if(ids[i])glDeleteBuffers(1,&ids[i]);
 memset(s,0,sizeof(*s));
}
static void roof_drop_alternate(void){roof_drop_saved(&roof_alternate);}
static int roof_try(World3D*w,int epoch,int level,int radius,int min_x,int min_z,int max_x,int max_z){
 R16RoofSaved*s=&roof_alternate;
 if(!s->valid||s->world!=w||s->epoch!=epoch||s->level!=level||s->radius!=radius||min_x<s->min_x||min_z<s->min_z||max_x>s->max_x||max_z>s->max_z)return 0;
 roof_save(&roof_swap_scratch);roof_restore(s);memcpy(s,&roof_swap_scratch,sizeof(*s));roof_cache_swaps++;return 1;
}
static void roof_stash_current(void){roof_drop_alternate();roof_save(&roof_alternate);roof_detach();}
