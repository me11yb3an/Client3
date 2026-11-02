/* Bake-local memoization of the original object-space UV calculation.
 * Source geometry is immutable during a bake. Rotation/translation, lighting,
 * alpha, material selection and atlas mapping remain on the original path. */
#define R37_SLOTS 64
#define R37_FACES 512
typedef struct{const void*key[10];int faces,textures;unsigned char valid[R37_FACES];UV uv[R37_FACES];}R37UVCache;
static R37UVCache r37_cache[R37_SLOTS];static unsigned r37_used,r37_next;
static uint64_t r37_hits,r37_misses;
void gl_bake_uv_reset(void){r37_used=r37_next=0;r37_hits=r37_misses=0;}
void gl_bake_uv_stats(uint64_t*hits,uint64_t*misses){*hits=r37_hits;*misses=r37_misses;}
static R37UVCache*r37_model(Model*m){
 if(!r16_baking()||m->face_count<=0||m->face_count>R37_FACES||m->textured_face_count<=0)return NULL;
 const void*key[]={m->vertices_x,m->vertices_y,m->vertices_z,m->face_indices_a,m->face_indices_b,m->face_indices_c,m->face_infos,m->textured_p_coordinate,m->textured_m_coordinate,m->textured_n_coordinate};
 for(unsigned i=0;i<r37_used;i++){R37UVCache*c=r37_cache+i;if(c->faces==m->face_count&&c->textures==m->textured_face_count&&!memcmp(c->key,key,sizeof key))return c;}
 unsigned slot=r37_used<R37_SLOTS?r37_used++:r37_next++%R37_SLOTS;R37UVCache*c=r37_cache+slot;
 memcpy(c->key,key,sizeof key);c->faces=m->face_count;c->textures=m->textured_face_count;memset(c->valid,0,(size_t)m->face_count);return c;
}
static int r37_get(R37UVCache*c,int face,UV*out){if(!c||!c->valid[face])return 0;memcpy(out,c->uv+face,sizeof(*out));r37_hits++;return 1;}
static void r37_put(R37UVCache*c,int face,const UV*uv){if(c){memcpy(c->uv+face,uv,sizeof(*uv));c->valid[face]=1;r37_misses++;}}
