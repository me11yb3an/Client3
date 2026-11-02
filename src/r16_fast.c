#include "r106_scenery_speed.h"
#include "r83_project_bounds.h"
#include "r82_project_divide.h"
#include "r16_fast.h"

#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ba_timing.h"
#include "custom.h"
#include "entity.h"
#include "gl11.h"
#include "pathingentity.h"
#include "playerentity.h"
#include "npcentity.h"
#include "model.h"
#include "r57_body_layers.h"
#include "pix2d.h"
#include "pix3d.h"
#include "platform.h"
#include "tileoverlay.h"
#include "tileunderlay.h"
#include "world3d.h"
#include "r16_coplanar.h"
#include "r42_planar_index.h"
#include "r25_wall_decor.h"
#include "loctype.h"
extern LocTypeData _LocType;
#include "r16_live_doors.inc"
#include "r95_maze_cache.h"
#include "r96_maze_swing.h"
void r96_door_status(unsigned out[2]){out[0]=r96_transitions;out[1]=r96_animation_models;}

extern Custom _Custom;
extern SceneData _World3D;
extern Pix3D _Pix3D;
extern Pix2D _Pix2D;
extern ModelData _Model;

void world3d_draw_tileunderlay(World3D *world3d, TileUnderlay *underlay, int level, int tileX, int tileZ, int sinEyePitch, int cosEyePitch, int sinEyeYaw, int cosEyeYaw);
void world3d_draw_tileoverlay(int tileX, int tileZ, TileOverlay *overlay, int sinEyePitch, int cosEyePitch, int sinEyeYaw, int cosEyeYaw);

#define R16_VERT_BUDGET 260000
/* SGX543 index buffers are 16-bit. A chunk must stay under that so the
 * upload is stored as unsigned short and is not converted every frame. */
#define R16_CHUNK_VERTS 60000
/* Dense towns exceed24chunks even when their complete scenery fits RAM.
 * Keep individual16-bit uploads unchanged and retain the existing24MiB
 * free-memory check before selecting complete-scene coverage. */
#define R16_MAX_CHUNKS 64
#define R16_PICK_MAX 8192
#define R16_DYN_MAX 4096
#define R16_WATER_MAX 8192

typedef struct {
    Model *model;
    int x, y, z, yaw, bitset;
    int floor_level;
} R16Model;

typedef struct {
    TileUnderlay *underlay;
    TileOverlay *overlay;
    int level, x, z;
} R16Water;

typedef struct {
    Model **slot;
    int x, y, z, yaw, bitset;
    uint16_t roof_tag;
} R16Dyn;

static int stamp = 1;
static unsigned dirty_events,committed_dirty_events;
static uintptr_t last_dirty_caller;
static int switch_off;
static int budget_off;
static int fallback_reason;
int r16_fallback_reason(void){return fallback_reason;}
static int baking;
static int collecting_source_level=-1;
static int model_profile_enabled=-1;
static void bake_model_profile_init(void){
 if(model_profile_enabled>=0)return;
 model_profile_enabled=0;
 FILE*f=fopen("ux0:/data/RS2Vita/model-profile.txt","r");
 if(f){int requested=0;if(fscanf(f,"%d",&requested)==1&&(requested>=1&&requested<=3))model_profile_enabled=requested;fclose(f);}
}
/* One timed scope per baked model, excluding the preceding chunk upload.
 * Keep only eight slow entries; emit buffered text once at bake completion. */
typedef struct {uint64_t at;int id,faces,x,z;uint64_t phase[5];} R16ModelCost;
static R16ModelCost*profile_model;
typedef struct{int id;unsigned count;uint64_t total,phase[5];}R39ModelTotal;
static R39ModelTotal r39_models[128];static unsigned r39_used,r39_sequence;
static uint64_t r39_release_us,r39_loop_us,r39_aux_us,r39_upload_us;
static uint64_t r40_skip_models,r40_scan_models,r40_skip_vertices;
static unsigned r39_upload_calls;
typedef struct{uint64_t at;}R39UploadCost;
static void r39_upload_end(R39UploadCost*p){if(p->at){r39_upload_us+=ba_timing_now()-p->at;r39_upload_calls++;}}

static R16ModelCost bake_slow_models[8];
static uint64_t bake_models_us;
static unsigned bake_models_n;
static uint64_t bake_phase_us[5];
static void bake_phase_mark(uint64_t*at,int phase){
 if(!*at)return;uint64_t now=ba_timing_now();bake_phase_us[phase]+=now-*at;if(profile_model)profile_model->phase[phase]+=now-*at;*at=now;
}
static void bake_model_cost_end(R16ModelCost*p){
 if(!p->at)return;
 uint64_t elapsed=ba_timing_now()-p->at;bake_models_us+=elapsed;bake_models_n++;profile_model=NULL;
 if(model_profile_enabled==3){unsigned slot=0;while(slot<r39_used&&r39_models[slot].id!=p->id)slot++;if(slot<128){if(slot==r39_used){r39_models[slot].id=p->id;r39_used++;}R39ModelTotal*t=r39_models+slot;t->count++;t->total+=elapsed;for(int k=0;k<5;k++)t->phase[k]+=p->phase[k];}}

 int least=0;for(int i=1;i<8;i++)if(bake_slow_models[i].at<bake_slow_models[least].at)least=i;
 if(elapsed>bake_slow_models[least].at){bake_slow_models[least]=*p;bake_slow_models[least].at=elapsed;}
}
typedef struct {uint64_t at;int top;} R16BakeCost;
static void bake_cost_end(R16BakeCost*p){
 if(!p->at)return;
 char line[256];uint64_t now=ba_timing_now();
 snprintf(line,sizeof line,"# renderer_bake_cost %llu top %d models %u model_us %llu total %llu",(unsigned long long)now,p->top,bake_models_n,(unsigned long long)bake_models_us,(unsigned long long)(now-p->at));ba_cadence_diagnostic(line);
 snprintf(line,sizeof line,"# renderer_bake_phases %llu emit %llu fit %llu decals %llu legacy_layers %llu finish %llu",(unsigned long long)now,(unsigned long long)bake_phase_us[0],(unsigned long long)bake_phase_us[1],(unsigned long long)bake_phase_us[2],(unsigned long long)bake_phase_us[3],(unsigned long long)bake_phase_us[4]);ba_cadence_diagnostic(line);
 snprintf(line,sizeof line,"# bake_stages release %llu loop %llu aux %llu uploads %llu upload_calls %u profile %d",(unsigned long long)r39_release_us,(unsigned long long)r39_loop_us,(unsigned long long)r39_aux_us,(unsigned long long)r39_upload_us,r39_upload_calls,model_profile_enabled);ba_cadence_diagnostic(line);
 for(unsigned i=0;i<r39_used;i++){R39ModelTotal*t=r39_models+i;snprintf(line,sizeof line,"# bake_model_sample loc %d samples %u total %llu emit %llu fit %llu decals %llu legacy %llu finish %llu",t->id,t->count,(unsigned long long)t->total,(unsigned long long)t->phase[0],(unsigned long long)t->phase[1],(unsigned long long)t->phase[2],(unsigned long long)t->phase[3],(unsigned long long)t->phase[4]);ba_cadence_diagnostic(line);}
 for(int i=0;i<8;i++){R16ModelCost*m=&bake_slow_models[i];if(!m->at)continue;snprintf(line,sizeof line,"# renderer_model_cost %llu top %d loc %d faces %d at %d %d us %llu",(unsigned long long)now,p->top,m->id,m->faces,m->x,m->z,(unsigned long long)m->at);ba_cadence_diagnostic(line);}
}
static int collecting;
static int picking;
static int emitting;
static int suppress;
static int freeze_scroll;
static int overflow;
static int frame_static;
static int frame_ready;
static int static_count;
static unsigned bake_serial,roof_cache_swaps;
unsigned r16_bake_count(void){return bake_serial;}
unsigned r16_roof_swaps(void){return roof_cache_swaps;}
#if defined(GL11)&&defined(__vita__)
static void roof_drop_alternate(void);
#endif

static R16Model picks[R16_PICK_MAX];
static int pick_count;
static R16Model anims[R16_DYN_MAX];
static int anim_count;
static R16Model items[R16_DYN_MAX];
static int item_count;
static R16Water water[R16_WATER_MAX];
static int water_count;
static R16Dyn dyns[R16_DYN_MAX];
static int dyn_count;

static World3D *key_world;
static int key_x, key_z, key_level, key_radius, key_cover, key_stamp, key_ok;
static int key_min_x, key_min_z, key_max_x, key_max_z;
static int anchor_x = -1, anchor_z = -1;

static int chunk_n;
static int static_verts;

#ifdef GL11
#ifdef __vita__
typedef struct {
    GLuint vbo, ibo;
    int count;
    int paving;
    int floor,floor_paving;
    int blend;
} R16Chunk;
static R16Chunk chunks[R16_MAX_CHUNKS];
static uint16_t short_indices[R16_CHUNK_VERTS];
#include "r31_static_compact.inc"
#include "r35_compact_patterns.inc"
#include "r32_roof_runtime.inc"
#define R32_ENABLED (r32_focus_ready)
static uint64_t compact_source_vertices,compact_gpu_vertices;
static unsigned compact_max_probe;
#include "r20_paving.h"
#include "r46_floor_partition.h"
static uint8_t paving_tags[R16_CHUNK_VERTS],floor_tags[R16_CHUNK_VERTS];
static void r31_pack_span(int first,int first_index){
 if(!baking)return;
 int source_n=vertcount-first;
 int packed=r35_compact_span(verts,first,source_n,indices,first_index,elementcount-first_index);
 if(packed<0){overflow=1;return;}
 compact_source_vertices+=(unsigned)source_n;compact_gpu_vertices+=(unsigned)packed;
 if(r31_max_probe>compact_max_probe)compact_max_probe=r31_max_probe;
 vertcount=first+packed;
 memset(paving_tags+first,0,(size_t)packed);
 memset(floor_tags+first,0,(size_t)packed);
 r32_mark_vertices(first,vertcount);
}
extern GLuint texture_atlas;
/* Retain only covered window-jamb caps for a tiny, depth-biased opaque pass.
 * Authored vertices, materials, glass, and all other opaque surfaces stay put. */
#define R16_TRIM_VERTS 32760
static Vertex trim_cpu[R16_TRIM_VERTS];
static uint16_t trim_indices[R16_TRIM_VERTS];
static int trim_count;
static int rug_layer_triangles, dresser_back_triangles, shutter_layer_triangles, shutter_attached;
static GLuint trim_vbo, trim_ibo;
/* Clock markings need one additional layer above the clock face. */
#define R16_CLOCK_VERTS 8190
static Vertex clock_cpu[R16_CLOCK_VERTS];
static uint16_t clock_indices[R16_CLOCK_VERTS];
static int clock_count, clock_face_triangles, window_layer_triangles, window_layer_dropped;
static GLuint clock_vbo, clock_ibo;
static World3D *trim_world;
static int trim_source_plane,trim_level,trim_top_level,wall_floor_triangles,wall_floor_partial,wall_floor_dropped,wall_floor_context;
static int live_door_floor_context,live_door_floor_level=-1;
static int generic_layer_triangles,layer_dropped,window_decor_context;
#include "r26_wall_decals.inc"
#include "r65_bank_shading.inc"
#include "r66_scenery_visuals.h"
#include "r73_animated_view.h"
#define R16_FLOORCAP_VERTS 8190
static Vertex floorcap_cpu[R16_FLOORCAP_VERTS];
static uint16_t floorcap_indices[R16_FLOORCAP_VERTS];
static int floorcap_count;
static GLuint floorcap_vbo,floorcap_ibo;
static int geometry_diag,geometry_dumped,geometry_focus_x,geometry_focus_z;
#include "r48_source_ids.h"
#include "r49_floor_supports.h"
#include "r107_column_caps.h"
#include "r51_openings.inc"
#include "r52_underground.inc"
static unsigned r53_replayed,r54_duplicates,r55_contacts,r56_replayed;
static FILE *geometry_file,*fit_file,*bridge_file,*floor_file,*terrain_file;
static unsigned furniture_dynamic_shots;static uint64_t furniture_dynamic_at;
void r16_door_probe_status(int out[3]){out[0]=geometry_diag;out[1]=(int)furniture_dynamic_shots;out[2]=baking;}
#else
static Vertex *cpu_verts;
static uint32_t *cpu_indices;
static int cpu_vert_n;
#endif
static int flush_chunk(void);
static void release_chunks(void);
#else
static int flush_chunk(void) {
    R39UploadCost upload_cost __attribute__((cleanup(r39_upload_end)))={model_profile_enabled>=2?ba_timing_now():0}; return 0; }
static void release_chunks(void) {}
#endif

#ifndef GL11
static int vertcount;
static int elementcount;
#endif

#if !defined(GL11)||!defined(__vita__)
#define R32_ENABLED 0
static int r32_tile_visible(int level,int x,int z,int top){(void)x;(void)z;return level<=top;}
#endif

void r16_dirty(void) {
    dirty_events++;
    last_dirty_caller=(uintptr_t)__builtin_return_address(0);
#if defined(GL11)&&defined(__vita__)
    roof_drop_alternate();
#endif
    live_loc_log_ready=0;r51_reset();
    stamp++;
    if (stamp == 0) stamp = 1;
    /* The old scene is being freed. Do not keep those model pointers. */
    pick_count = 0;
}

void r16_set_scene_region(int world_z) {
#if defined(GL11)&&defined(__vita__)
 r52_world_z=world_z;
#else
 (void)world_z;
#endif
}
void r16_set_anchor(int tile_x, int tile_z) {
    anchor_x = tile_x;
    anchor_z = tile_z;
}
void r16_set_roof_focus(const int8_t flags[4][104][104],int level,int x,int z){
#if defined(GL11)&&defined(__vita__)
 int rebuilt=0;
 if(!flags){if(r32_focus_ready){r32_selected=0;r32_focus_ready=0;r32_change_visibility();}return;}
 if(r32_zones_epoch!=stamp||r32_flags_source!=flags){
  r32_zones_build(&r32_zones,flags);r32_zones_epoch=stamp;r32_flags_source=flags;rebuilt=1;
 }
 uint16_t selected=r32_zones_pick(&r32_zones,level,x,z,r32_selected,R32_APPROACH_MARGIN);
 if(rebuilt||!r32_focus_ready||selected!=r32_selected||level!=r32_player_level){
  r32_selected=selected;r32_player_level=level;r32_focus_ready=1;r32_change_visibility();
  char line[224];snprintf(line,sizeof line,"# roof_focus %llu plane %d selected %u components %u local %d %d margin %d revision %u",(unsigned long long)ba_timing_now(),level,selected,r32_zones.components,x,z,R32_APPROACH_MARGIN,r32_revision);ba_cadence_diagnostic(line);
 }
#else
 (void)flags;(void)level;(void)x;(void)z;
#endif
}
unsigned r16_hull_layers(void){
#if defined(GL11)&&defined(__vita__)
 return r56_replayed;
#else
 return 0;
#endif
}
unsigned r16_fixture_contacts(void){
#if defined(GL11)&&defined(__vita__)
 return r55_contacts;
#else
 return 0;
#endif
}
unsigned r16_crane_duplicates(void){
#if defined(GL11)&&defined(__vita__)
 return r54_duplicates;
#else
 return 0;
#endif
}
unsigned r16_ship_rail_faces(void){
#if defined(GL11)&&defined(__vita__)
 return r53_replayed;
#else
 return 0;
#endif
}
unsigned r16_underground_cuts(void){
#if defined(GL11)&&defined(__vita__)
 return r52_clipped_models;
#else
 return 0;
#endif
}
void r16_opening_status(int status[3]){
#if defined(GL11)&&defined(__vita__)
 status[0]=r51_count;status[1]=r51_clipped;status[2]=r51_failed;
#else
 memset(status,0,3*sizeof(int));
#endif
}
void r16_roof_status(int status[8]){
 memset(status,0,8*sizeof(*status));
#if defined(GL11)&&defined(__vita__)
 status[0]=r32_selected;status[1]=r32_player_level;status[2]=(int)r32_zones.components;
 status[4]=(int)r32_uploads;status[5]=r32_failed;status[6]=(int)r32_revision;
 for(int i=0;i<r32_used;i++)if(r32_masks[i].ibo){
  R32IndexMask*m=r32_masks+i;status[3]++;
  if(m->revision==r32_revision){int visible=0;for(int g=0;g<m->groups;g++)visible+=m->active_count[g];status[7]+=m->count-visible;}
 }
#endif
}


int r16_baking(void) { return baking; }
int r16_emitting(void) { return emitting; }
int r16_suppress_emit(void) { return suppress; }
int r16_freeze_scroll(void) { return freeze_scroll; }

static void note_dyn(Model **slot, int x, int y, int z, int yaw, int bitset) {
    if (!baking || !slot || !*slot || r16_live_location(bitset) || !(*slot)->r10_animated || dyn_count >= R16_DYN_MAX) return;
    dyns[dyn_count++] = (R16Dyn){slot, x, y, z, yaw, bitset,65535u};
#if defined(GL11)&&defined(__vita__)
    dyns[dyn_count-1].roof_tag=r32_current_tag;
#endif
}

static uint64_t io_us[8], io_max[8];
static unsigned io_n[8];

void r16_io_add(int kind, uint64_t us) {
    if (kind < 0 || kind >= 8) return;
    io_us[kind] += us;
    io_n[kind]++;
    if (us > io_max[kind]) io_max[kind] = us;
}

int r16_diag_verbose(void) {
    static int ready, verbose;
    if (!ready) {
        int value = 0;
        FILE *f = fopen("ux0:/data/RS2Vita/diag-level.txt", "r");
        if (f) {
            if (fscanf(f, "%d", &value) != 1) value = 0;
            fclose(f);
        }
        verbose = value != 0;
        ready = 1;
    }
    return verbose;
}

static int dyn_cache_enabled(void) {
    static int ready, enabled;
    if (!ready) {
        /* Retain unchanged scenery by default. Mode 1 also enables the older
         * actor cache; mode 2 keeps actors on the ordinary animation path. */
        int value = 2;
        FILE *f = fopen("ux0:/data/RS2Vita/dyn-cache.txt", "r");
        if (f) {
            if (fscanf(f, "%d", &value) != 1) value = 0;
            fclose(f);
        }
        enabled = (value == 1 || value == 2) ? value : 0;
        ready = 1;
    }
    return enabled;
}

static void r16_write(const char *fmt, ...) {
    char buf[384];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    rs2_log("%s\n", buf);
    uint64_t began = ba_timing_now();
    if (r16_diag_verbose()) {
        FILE *f = fopen("ux0:/data/RS2Vita/r16-bake.txt", "a");
        if (f) {
            fputs(buf, f);
            fputc('\n', f);
            fclose(f);
        }
    }
    r16_io_add(2, ba_timing_now() - began);
}

int r16_active(void) {
#ifdef GL11
    static int checks;
    if (!checks || (r16_diag_verbose() && (checks % 120) == 0)) {
        FILE *f = fopen("ux0:/data/RS2Vita/r16-off.txt", "r");
        switch_off = f != NULL;
        if (f) fclose(f);
    }
    checks++;
    return !switch_off && !budget_off && _Custom.use_opengl11;
#else
    return 0;
#endif
}

int r16_has_static(void) {
    return frame_static && static_count > 0;
}

int r16_fast_frame(void) {
    return frame_ready;
}

static int iabs(int v) { return v < 0 ? -v : v; }

static int over_budget(void) {
    return vertcount > R16_VERT_BUDGET || elementcount > R16_VERT_BUDGET;
}

static int project_camera(int x,int y,int z,int*nx,int*ny){
    int sin_yaw=_World3D.sinEyeYaw,cos_yaw=_World3D.cosEyeYaw;
    int sin_pitch=_World3D.sinEyePitch,cos_pitch=_World3D.cosEyePitch;
    int tx=(z*sin_yaw+x*cos_yaw)>>16;
    z=(z*cos_yaw-x*sin_yaw)>>16;
    int ty=(y*cos_pitch-z*sin_pitch)>>16;
    z=(y*sin_pitch+z*cos_pitch)>>16;
    if(z<50)return 0;*nx=tx<<9;*ny=ty<<9;return z;
}
static int project_point(int x,int y,int z,int*sx,int*sy){
    int nx,ny,depth=project_camera(x,y,z,&nx,&ny);if(!depth)return 0;
    int screenX,screenY;r82_project_divide(nx,ny,depth,&screenX,&screenY);
    *sx=_Pix3D.center_x+screenX;*sy=_Pix3D.center_y+screenY;return depth;
}
static int tile_pointer_near(int x,int y,int z){
    int nx,ny,depth=project_camera(x,y,z,&nx,&ny);
    return !depth||r83_screen_inside(nx,ny,depth,_Pix3D.center_x,_Pix3D.center_y,_Model.mouse_x,_Model.mouse_y,1200,900);
}

static void remember_pick(Model *model, int x, int y, int z, int yaw, int bitset) {
    if (!model || pick_count >= R16_PICK_MAX) return;
    picks[pick_count++] = (R16Model){model, x, y, z, yaw, bitset};
}

static int remember_anim(Model *model, int x, int y, int z, int yaw, int bitset) {
    if (!collecting) remember_pick(model, x, y, z, yaw, bitset);
    if (!model || anim_count >= R16_DYN_MAX) return 0;
    anims[anim_count++] = (R16Model){model, x, y, z, yaw, bitset,collecting_source_level};
    return 1;
}

static int remember_item(Model *model, int x, int y, int z, int bitset) {
    if (!collecting) remember_pick(model, x, y, z, 0, bitset);
    if (!model || item_count >= R16_DYN_MAX) return 0;
    items[item_count++] = (R16Model){model, x, y, z, 0, bitset};
    return 1;
}

static int water_under(TileUnderlay *underlay) {
    return underlay && (underlay->textureId == 17 || underlay->textureId == 24);
}

static int water_over(TileOverlay *overlay) {
    if (!overlay || !overlay->triangleTextureIds) return 0;
    for (int i = 0; i < overlay->triangleCount; i++) {
        int texture = overlay->triangleTextureIds[i];
        if (texture == 17 || texture == 24) return 1;
    }
    return 0;
}

static int remember_water(TileUnderlay *underlay, TileOverlay *overlay, int level, int x, int z) {
    if (water_count >= R16_WATER_MAX) return 0;
    water[water_count++] = (R16Water){underlay, overlay, level, x, z};
    return 1;
}

static int flush_if_full(int incoming) {
    if (incoming > R16_CHUNK_VERTS) return 0;
    if (vertcount + incoming <= R16_CHUNK_VERTS && elementcount + incoming <= R16_CHUNK_VERTS) return 1;
    return flush_chunk();
}

/* Opt-in, bake-only geometry evidence. No render state or vertices are changed. */
static void wood_geometry_capture(Model *model, int yaw, int x, int y, int z, int bitset, int first) {
#if defined(GL11) && defined(__vita__)
    if(!geometry_diag)return;
    FILE*dynamic_file=NULL;
    if(!baking){
        uint64_t now=ba_timing_now();int id=((unsigned)bitset>>14)&32767;
        int door_probe=geometry_diag==7&&r95_static_maze_door(bitset);
        if((!door_probe&&(geometry_diag!=3||id!=2727))||furniture_dynamic_shots>=8||now<furniture_dynamic_at||(!door_probe&&(iabs(x/128-geometry_focus_x)>5||iabs(z/128-geometry_focus_z)>5)))return;
        dynamic_file=fopen(door_probe?"ux0:/data/RS2Vita/door-geometry217.bin":"ux0:/data/RS2Vita/wood-geometry.bin","ab");if(!dynamic_file)return;
        furniture_dynamic_shots++;furniture_dynamic_at=now+250000ull;
    }else if(!geometry_file)return;
    if(geometry_diag==7){if(baking){if(dynamic_file)fclose(dynamic_file);return;}}
    else if(geometry_diag==3){
        int id=((unsigned)bitset>>14)&32767;
        if(iabs(x/128-geometry_focus_x)>5||iabs(z/128-geometry_focus_z)>5){if(dynamic_file)fclose(dynamic_file);return;}
        if(id!=380&&id!=381&&id!=2727&&!(id>=1600&&id<=1999))return;
    }else if(geometry_diag==5){
        int id=((unsigned)bitset>>14)&32767;
        if(iabs(x/128-anchor_x)>4||iabs(z/128-anchor_z)>4)return;
        if(!r25_window_inset(id)&&!r65_bank_layers_type(id)&&id!=1602&&id!=1902&&id!=609&&id!=610&&id!=165&&id!=2781&&id!=3044&&id!=2785)return;
    }else if(geometry_diag==2||geometry_diag==4||geometry_diag==6){if(iabs(x/128-anchor_x)>12||iabs(z/128-anchor_z)>12)return;}
    else if(x/128<49||x/128>61||z/128<47||z/128>57)return;
    int count=vertcount-first;if(count<=0||first<0){if(dynamic_file)fclose(dynamic_file);return;}
    FILE *file=dynamic_file?dynamic_file:geometry_file;
    int header[] = {0x574f4f44, ((unsigned)bitset >> 14) & 32767, x, y, z, yaw, model->face_count, count, bitset};
    fwrite(header, sizeof(header), 1, file);
    for (int f = 0; f < model->face_count; f++) {
        int face[] = {model->face_infos ? model->face_infos[f] : 0,
            model->face_priorities ? model->face_priorities[f] : model->model_priority,
            model->face_alphas ? model->face_alphas[f] : 0,
            model->face_colors ? model->face_colors[f] : -1,
            model->face_color_a ? model->face_color_a[f] : -1,
            model->face_color_b ? model->face_color_b[f] : -1,
            model->face_color_c ? model->face_color_c[f] : -1, f};
        fwrite(face, sizeof(face), 1, file);
    }
    fwrite(verts + first, sizeof(Vertex), count, file);
    if(dynamic_file)fclose(dynamic_file);
#else
    (void)model; (void)yaw; (void)x; (void)y; (void)z; (void)bitset; (void)first;
#endif
}

static void window_trim_capture(Model *model, int x, int y, int z, int bitset, int first) {
#if defined(GL11) && defined(__vita__)
    int id = ((unsigned)bitset >> 14) & 32767;
    /* Revision-225 shop-window wall types; never bias ordinary walls, floors,
     * actors, or glass. Geometric coverage below determines the actual caps. */
    if (!baking || !trim_world || (id != 1853 && id != 1854)) return;
    for (int v = first; v + 2 < vertcount; v += 3) {
        Vertex *t = verts + v;
        if (t[0].a != 255 || t[1].a != 255 || t[2].a != 255) continue;
        float p[3][3] = {{t[0].x,t[0].y,t[0].z},{t[1].x,t[1].y,t[1].z},{t[2].x,t[2].y,t[2].z}};
        int axis = -1;
        if (p[0][0] == p[1][0] && p[1][0] == p[2][0] && fabsf(p[0][0]-x) == 64) axis = 0;
        if (p[0][2] == p[1][2] && p[1][2] == p[2][2] && fabsf(p[0][2]-z) == 64) axis = 2;
        if (axis < 0) continue;
        int u = axis == 0 ? 1 : 0, w = axis == 0 ? 2 : 1;
        R16Point2 a[3];
        for (int i = 0; i < 3; i++) a[i] = (R16Point2){p[i][u]-p[0][u],p[i][w]-p[0][w]};
        float signed_area = r16_area2(a, 3), area = fabsf(signed_area)*.5f;
        if (area < 1) continue;
        int covered = 0;
        for (int tx = x/128-1; tx <= x/128+1 && !covered; tx++) {
            for (int tz = z/128-1; tz <= z/128+1 && !covered; tz++) {
                if (tx < 0 || tz < 0 || tx >= trim_world->maxTileX || tz >= trim_world->maxTileZ) continue;
                Ground *tile = trim_world->levelTiles[trim_level][tx][tz];
                if (!tile || !tile->wall) continue;
                Wall *wall = tile->wall;
                Model *neighbors[] = {wall->modelA,wall->modelB};
                for (int m = 0; m < 2 && !covered; m++) {
                    Model *n = neighbors[m];
                    if (!n || n->r10_animated || (n == model && wall->x == x && wall->y == y && wall->z == z)) continue;
                    float sum = 0;
                    for (int f = 0; f < n->face_count; f++) {
                        if ((n->face_infos && n->face_infos[f] == -1) || (n->face_alphas && n->face_alphas[f] != 0)) continue;
                        int ids[] = {n->face_indices_a[f],n->face_indices_b[f],n->face_indices_c[f]};
                        R16Point2 b[3]; int same_plane = 1;
                        for (int i = 0; i < 3; i++) {
                            int k = ids[i];
                            float q[] = {n->vertices_x[k]+wall->x,n->vertices_y[k]+wall->y,n->vertices_z[k]+wall->z};
                            if (q[axis] != p[0][axis]) { same_plane = 0; break; }
                            b[i] = (R16Point2){q[u]-p[0][u],q[w]-p[0][w]};
                        }
                        if (!same_plane || r16_area2(b, 3)*signed_area <= 0) continue;
                        sum += r16_triangle_overlap(a,b);
                    }
                    if (sum >= area-.01f) covered = 1;
                }
            }
        }
        if (covered && trim_count+3 <= R16_TRIM_VERTS) {
            memcpy(trim_cpu+trim_count,t,3*sizeof(Vertex));
            trim_count += 3;
        }
    }
#else
    (void)model; (void)x; (void)y; (void)z; (void)bitset; (void)first;
#endif
}

/* The drawer body was authored into the wall's thickness. Fit only the
 * hidden rear volume to an opaque wall covering its entire footprint.
 * Applying the same limit to side/top vertices also removes the inside-
 * corner overlap; the visible front, drawers and handles stay unchanged. */
static void dresser_wall_fit(int x, int z, int bitset, int first) {
#if defined(GL11) && defined(__vita__)
    if (!baking || !trim_world || (((unsigned)bitset >> 14) & 32767) != 350 || first >= vertcount) return;
    float lo[3] = {verts[first].x,verts[first].y,verts[first].z}, hi[3] = {lo[0],lo[1],lo[2]};
    for (int v = first; v < vertcount; v++) {
        float p[] = {verts[v].x,verts[v].y,verts[v].z};
        for (int k = 0; k < 3; k++) { if (p[k] < lo[k]) lo[k] = p[k]; if (p[k] > hi[k]) hi[k] = p[k]; }
    }
    for (int axis = 0; axis <= 2; axis += 2) for (int side = -1; side <= 1; side += 2) {
        int u = axis == 0 ? 1 : 0, w = axis == 0 ? 2 : 1;
        float edge = side > 0 ? hi[axis] : lo[axis], chosen = edge; int fitted = 0;
        R16Point2 box[2][3] = {{{0,0},{hi[u]-lo[u],0},{hi[u]-lo[u],hi[w]-lo[w]}},
            {{0,0},{hi[u]-lo[u],hi[w]-lo[w]},{0,hi[w]-lo[w]}}};
        float total = (hi[u]-lo[u])*(hi[w]-lo[w]);
        for (int tx = x/128-1; tx <= x/128+1; tx++) for (int tz = z/128-1; tz <= z/128+1; tz++) {
            if (tx < 0 || tz < 0 || tx >= trim_world->maxTileX || tz >= trim_world->maxTileZ) continue;
            Ground *tile = trim_world->levelTiles[trim_level][tx][tz]; if (!tile || !tile->wall) continue;
            Wall *wall = tile->wall; Model *models[] = {wall->modelA,wall->modelB};
            for (int m = 0; m < 2; m++) {
                Model *n = models[m]; if (!n || n->r10_animated) continue;
                for (int face = 0; face < n->face_count; face++) {
                    int vertex = n->face_indices_a[face];
                    float plane = axis == 0 ? n->vertices_x[vertex]+wall->x : n->vertices_z[vertex]+wall->z;
                    float penetration = (edge-plane)*side;
                    if (penetration < 0 || penetration > 24 || (fitted && (chosen-plane)*side <= 0)) continue;
                    float covered = 0;
                    for (int f = 0; f < n->face_count; f++) {
                        if ((n->face_infos && n->face_infos[f] == -1) || (n->face_alphas && n->face_alphas[f])) continue;
                        int ids[] = {n->face_indices_a[f],n->face_indices_b[f],n->face_indices_c[f]}, flat = 1;
                        R16Point2 b[3];
                        for (int i = 0; i < 3; i++) {
                            int k = ids[i]; float q[] = {n->vertices_x[k]+wall->x,n->vertices_y[k]+wall->y,n->vertices_z[k]+wall->z};
                            if (q[axis] != plane) { flat = 0; break; }
                            b[i] = (R16Point2){q[u]-lo[u],q[w]-lo[w]};
                        }
                        if (flat && r16_area2(b,3)*side < 0) covered += r16_triangle_overlap(box[0],b)+r16_triangle_overlap(box[1],b);
                    }
                    if (covered >= total-.01f) { chosen = plane; fitted = 1; }
                }
            }
        }
        if (!fitted) continue;
        float limit = chosen-side;
        for (int v = first; v+2 < vertcount; v += 3) {
            Vertex *t = verts+v;
            float p0 = axis == 0 ? t[0].x : t[0].z, p1 = axis == 0 ? t[1].x : t[1].z, p2 = axis == 0 ? t[2].x : t[2].z;
            if (p0 == edge && p1 == edge && p2 == edge) {
                t[1].x = t[2].x = t[0].x; t[1].y = t[2].y = t[0].y; t[1].z = t[2].z = t[0].z;
                dresser_back_triangles++;
            }
        }
        for (int v = first; v < vertcount; v++) {
            if (axis == 0 && (verts[v].x-limit)*side > 0) verts[v].x = limit;
            if (axis == 2 && (verts[v].z-limit)*side > 0) verts[v].z = limit;
        }
    }
#else
    (void)x; (void)z; (void)bitset; (void)first;
#endif
}

/* Fit a shallow window as a rigid panel to a nearby opaque supporting wall.
 * Require full footprint coverage; preserve its internal geometry and UVs. */
static void shutter_attach(int x, int z, int bitset, int first) {
#if defined(GL11) && defined(__vita__)
    int loc=((unsigned)bitset>>14)&32767;
    if(!baking||!window_decor_context||!trim_world||!r25_window_inset(loc)||first>=vertcount)return;
    /* Native bank pair measured19 and2-unit gaps. Keep the whole panel
     * rigid; only a fully covering opaque wall within24 units qualifies. */
    float max_gap=24.f;
    float lo[3] = {verts[first].x,verts[first].y,verts[first].z}, hi[3] = {lo[0],lo[1],lo[2]};
    for (int v = first; v < vertcount; v++) {
        float p[] = {verts[v].x,verts[v].y,verts[v].z};
        for (int k = 0; k < 3; k++) { if (p[k] < lo[k]) lo[k] = p[k]; if (p[k] > hi[k]) hi[k] = p[k]; }
    }
    int axis = hi[0]-lo[0] < hi[2]-lo[2] ? 0 : 2;
    if(hi[axis]-lo[axis]>32)return;
    int u = axis == 0 ? 1 : 0, w = axis == 0 ? 2 : 1;
    R16Point2 box[2][3] = {{{0,0},{hi[u]-lo[u],0},{hi[u]-lo[u],hi[w]-lo[w]}},
        {{0,0},{hi[u]-lo[u],hi[w]-lo[w]},{0,hi[w]-lo[w]}}};
    float total = (hi[u]-lo[u])*(hi[w]-lo[w]);
    float nearest = max_gap+1, old_edge = 0, wall_edge = 0, outside = 0;
    for (int tx = x/128-1; tx <= x/128+1; tx++) for (int tz = z/128-1; tz <= z/128+1; tz++) {
        if (tx < 0 || tz < 0 || tx >= trim_world->maxTileX || tz >= trim_world->maxTileZ) continue;
        Ground *tile = trim_world->levelTiles[trim_level][tx][tz];
        if (!tile || !tile->wall) continue;
        Wall *wall = tile->wall; Model *models[] = {wall->modelA,wall->modelB};
        for (int m = 0; m < 2; m++) {
            Model *n = models[m]; if (!n || n->r10_animated) continue;
            /* A panel can begin inside a thick wall. Select the wall's
             * exterior half-space, rather than leaving most of it buried. */
            float wall_lo=1e30f,wall_hi=-1e30f;
            for(int f=0;f<n->face_count;f++){
                int ids[]={n->face_indices_a[f],n->face_indices_b[f],n->face_indices_c[f]};
                for(int i=0;i<3;i++){
                    float q=axis==0?n->vertices_x[ids[i]]+wall->x:n->vertices_z[ids[i]]+wall->z;
                    if(q<wall_lo)wall_lo=q;if(q>wall_hi)wall_hi=q;
                }
            }
            for (int face = 0; face < n->face_count; face++) {
                int vertex = n->face_indices_a[face];
                float plane = axis == 0 ? n->vertices_x[vertex]+wall->x : n->vertices_z[vertex]+wall->z;
                float side=plane>hi[axis]?-1.f:1.f;
                float edge=plane>hi[axis]?hi[axis]:lo[axis];
                if(wall_hi-wall_lo>.01f){
                    if(plane==wall_lo){side=-1.f;edge=hi[axis];}
                    else if(plane==wall_hi){side=1.f;edge=lo[axis];}
                    else continue;
                }else if(plane>=lo[axis]&&plane<=hi[axis])continue;
                float gap=fabsf(plane-edge);
                if(gap>max_gap||gap>=nearest)continue;
                float covered = 0;
                for (int f = 0; f < n->face_count; f++) {
                    if ((n->face_infos && n->face_infos[f] == -1) || (n->face_alphas && n->face_alphas[f])) continue;
                    int ids[] = {n->face_indices_a[f],n->face_indices_b[f],n->face_indices_c[f]}, flat = 1;
                    R16Point2 b[3];
                    for (int i = 0; i < 3; i++) {
                        int k = ids[i]; float q[] = {n->vertices_x[k]+wall->x,n->vertices_y[k]+wall->y,n->vertices_z[k]+wall->z};
                        if (q[axis] != plane) { flat = 0; break; }
                        b[i] = (R16Point2){q[u]-lo[u],q[w]-lo[w]};
                    }
                    if (flat) covered += r16_triangle_overlap(box[0],b)+r16_triangle_overlap(box[1],b);
                }
                if (covered >= total-.01f) { nearest = gap; old_edge = edge; wall_edge = plane; outside=side; }
            }
        }
    }
    if (nearest <= max_gap) {
        for (int v = first; v < vertcount; v++) {
            /* Some authored panes (for example elfwindow2060) sit on the
             * panel's rear-most plane. Keep that plane just OUTSIDE the wall,
             * rather than burying the pane while its thicker frame is visible. */
            float shift=wall_edge-old_edge+outside*.5f;
            if(axis==0)verts[v].x+=shift;
            if(axis==2)verts[v].z+=shift;
        }
        shutter_attached++;
        if(fit_file)fprintf(fit_file,"%d,%d,%d,%d,%.3f,%.3f,%.3f\n",loc,x,z,axis,old_edge,wall_edge,nearest);
    }
#else
    (void)x; (void)z; (void)bitset; (void)first;
#endif
}

/* These models author priority-1 detail on priority-0 backing. The software
 * renderer orders them by priority; the depth renderer needs a small bias.
 * Only fully covered, axis-planar, opaque details enter the replay pass. */
static void rug_layer_capture(Model *model, int bitset, int first) {
#if defined(GL11) && defined(__vita__)
    int id = ((unsigned)bitset >> 14) & 32767;
    /* A replay needs different detail/backing priorities. Uniform-priority
     * models cannot supply both; skip their otherwise fruitless face scans. */
    if(!baking||!model->face_priorities||r26_scope(bitset))return;
    int visible = 0;unsigned priorities=0;
    for (int f = 0; f < model->face_count; f++) {
        if(model->face_infos&&model->face_infos[f]==-1)continue;
        visible++;int priority=model->face_priorities[f];
        if(priority>=0&&priority<=4)priorities|=1u<<priority;
    }
    if(id==683){if(!((priorities&4u)&&(priorities&8u))&&!((priorities&8u)&&(priorities&16u)))return;}
    else if((priorities&3u)!=3u)return;
    if (vertcount-first != visible*3) return;
    enum { RUG_BACKING_MAX=4096 };
    static int backing_vertex[RUG_BACKING_MAX];
    int indexed=model->face_count<=RUG_BACKING_MAX;
    if(indexed){
        r42_begin();
        int at=first+visible*3;
        for(int g=model->face_count-1;g>=0;g--){
            if(model->face_infos&&model->face_infos[g]==-1)continue;
            at-=3;int lower=model->face_priorities[g];Vertex*q=verts+at;
            if(lower<0||lower>=5||(id==683?(lower!=2&&lower!=3):lower!=0)||
               (model->face_infos&&(model->face_infos[g]&2))||q[0].a!=255||q[1].a!=255||q[2].a!=255)continue;
            backing_vertex[g]=at;
            if(q[0].x==q[1].x&&q[1].x==q[2].x)r42_insert(g,0,lower,q[0].x);
            if(q[0].y==q[1].y&&q[1].y==q[2].y)r42_insert(g,1,lower,q[0].y);
            if(q[0].z==q[1].z&&q[1].z==q[2].z)r42_insert(g,2,lower,q[0].z);
        }
    }
    int v = first;
    for (int f = 0; f < model->face_count; f++) {
        if (model->face_infos && model->face_infos[f] == -1) continue;
        Vertex *t = verts+v; v += 3;
        int priority = model->face_priorities ? model->face_priorities[f] : model->model_priority;
        /* The small bookcase authors textured books on its wood backing.
         * Retain their original UVs/colors; only proved covered detail is replayed. */
        if ((model->face_infos && (model->face_infos[f]&2) && id!=381) || (id == 683 ? (priority != 3 && priority != 4) : priority != 1) || t[0].a != 255 || t[1].a != 255 || t[2].a != 255) continue;
        int axis = -1;
        if (t[0].x == t[1].x && t[1].x == t[2].x) axis = 0;
        if (t[0].y == t[1].y && t[1].y == t[2].y) axis = 1;
        if (t[0].z == t[1].z && t[1].z == t[2].z) axis = 2;
        if(axis<0)continue;
        int u = (axis+1)%3, w = (axis+2)%3;
        float p[3][3] = {{t[0].x,t[0].y,t[0].z},{t[1].x,t[1].y,t[1].z},{t[2].x,t[2].y,t[2].z}};
        R16Point2 a[3];
        for (int i = 0; i < 3; i++) a[i] = (R16Point2){p[i][u]-p[0][u],p[i][w]-p[0][w]};
        float area = fabsf(r16_area2(a,3))*.5f, covered = 0;
        if (area < .01f) continue;
        int other = first;
        int backing_priority=id==683?priority-1:0;
        for (int node = indexed?r42_head(axis,backing_priority,p[0][axis]):0; node>=0&&(indexed||node<model->face_count); node=indexed?r42_links[node]:node+1) {
            int g=indexed?node/3:node;
            if(indexed&&node%3!=axis)continue;
            if (model->face_infos && model->face_infos[g] == -1) continue;
            Vertex *q = verts+(indexed?backing_vertex[g]:other); if(!indexed)other += 3;
            int lower = model->face_priorities ? model->face_priorities[g] : model->model_priority;
            if ((model->face_infos && (model->face_infos[g]&2)) || lower != (id == 683 ? priority-1 : 0) || q[0].a != 255 || q[1].a != 255 || q[2].a != 255) continue;
            float xyz[3][3] = {{q[0].x,q[0].y,q[0].z},{q[1].x,q[1].y,q[1].z},{q[2].x,q[2].y,q[2].z}};
            if (xyz[0][axis] != p[0][axis] || xyz[1][axis] != p[0][axis] || xyz[2][axis] != p[0][axis]) continue;
            R16Point2 b[3];
            for (int i = 0; i < 3; i++) b[i] = (R16Point2){xyz[i][u]-p[0][u],xyz[i][w]-p[0][w]};
            covered += r16_triangle_overlap(a,b);
        }
        if (covered < area-.01f) continue;
        if (id == 683 && priority == 4) {
            if (clock_count+3 > R16_CLOCK_VERTS) continue;
            memcpy(clock_cpu+clock_count,t,3*sizeof(Vertex)); clock_count += 3;
        } else {
            if (trim_count+3 > R16_TRIM_VERTS){layer_dropped++;continue;}
            memcpy(trim_cpu+trim_count,t,3*sizeof(Vertex));
            if (id == 683) {
                /* The packed clock dial is HSL 127 (white). Directional
                 * shading can reduce it to lightness 3 on this wall side.
                 * Keep a dial-only ambient floor: 127 * ambient 64 / 128.
                 * The markings and surrounding wood retain their shading. */
                int tones[] = {model->face_color_a[f],model->face_color_b[f],model->face_color_c[f]};
                for (int i = 0; i < 3; i++) {
                    int tone = tones[i];
                    if ((tone & 0xff80) == 0 && (tone & 127) < 63) {
                        int rgb = _Pix3D.palette[63];
                        trim_cpu[trim_count+i].r = (rgb >> 16) & 255;
                        trim_cpu[trim_count+i].g = (rgb >> 8) & 255;
                        trim_cpu[trim_count+i].b = rgb & 255;
                    }
                }
            }
            trim_count += 3;
            if (id == 683) clock_face_triangles++;
            else if (id == 1838) shutter_layer_triangles++;
            else if(id==107 || id==108 || id==109 || id==916 || id==917 || id==918 || id==919 || id==920 || id==921 || id==922 || id==923 || id==924 || id==925 || id==926 || id==927 || id==928 || id==929 || id==930 || id==931 || id==932 || id==933 || id==934 || id==935 || id==936 || id==937 || id==938 || id==939 || id==940 || id==941 || id==942 || id==943 || id==944 || id==945 || id==976 || id==977 || id==978 || id==2045 || id==2046 || id==2047 || id==3473 || id==3474 || id==3475 || id==3513 || id==3514)rug_layer_triangles++;
            else generic_layer_triangles++;
        }
    }
#else
    (void)model; (void)bitset; (void)first;
#endif
}

/* Window ornament is authored in priority layers on the same plane.
 * Replay only opaque, untextured detail fully covered by one lower-priority
 * layer of the same model. Glass, opening geometry, and placement stay intact. */
static void window_layer_capture(Model *model,int bitset,int first) {
#if defined(GL11) && defined(__vita__)
    int id=((unsigned)bitset>>14)&32767;
    if(!baking||r26_scope(bitset)||!((id>=1821&&id<=1856)||id==244||id==1615||id==2683||id==3153)||id==1838)return; /* accepted shutter path */
    int visible=0;
    for(int f=0;f<model->face_count;f++)if(!model->face_infos||model->face_infos[f]!=-1)visible++;
    if(vertcount-first!=visible*3)return;
    /* Same exact emitted planes, alpha/material guards and ascending face
     * order as the old scan. Oversize models retain that original scan. */
    enum{WINDOW_BACKING_MAX=4096};static int backing_vertex[WINDOW_BACKING_MAX];
    int indexed=model->face_count<=WINDOW_BACKING_MAX;
    if(indexed){
        r42_begin();int at=first+visible*3;
        for(int g=model->face_count-1;g>=0;g--){
            if(model->face_infos&&model->face_infos[g]==-1)continue;
            at-=3;int lower=model->face_priorities?model->face_priorities[g]:model->model_priority;Vertex*q=verts+at;
            if(lower<0||lower>=4||q[0].a!=255||q[1].a!=255||q[2].a!=255||(model->face_infos&&(model->face_infos[g]&2)))continue;
            backing_vertex[g]=at;
            if(q[0].x==q[1].x&&q[1].x==q[2].x)r42_insert(g,0,lower,q[0].x);
            if(q[0].y==q[1].y&&q[1].y==q[2].y)r42_insert(g,1,lower,q[0].y);
            if(q[0].z==q[1].z&&q[1].z==q[2].z)r42_insert(g,2,lower,q[0].z);
        }
    }
    int v=first;
    for(int f=0;f<model->face_count;f++) {
        if(model->face_infos&&model->face_infos[f]==-1)continue;
        Vertex*t=verts+v;v+=3;
        int priority=model->face_priorities?model->face_priorities[f]:model->model_priority;
        if(priority<=1||priority>4||t[0].a!=255||t[1].a!=255||t[2].a!=255||
           (model->face_infos&&(model->face_infos[f]&2)))continue;
        float p[3][3]={{t[0].x,t[0].y,t[0].z},{t[1].x,t[1].y,t[1].z},{t[2].x,t[2].y,t[2].z}};
        int axis=-1;
        for(int k=0;k<3;k++)if(p[0][k]==p[1][k]&&p[1][k]==p[2][k])axis=k;
        if(axis<0)continue;
        int u=(axis+1)%3,w=(axis+2)%3;
        R16Point2 a[3];for(int k=0;k<3;k++)a[k]=(R16Point2){p[k][u]-p[0][u],p[k][w]-p[0][w]};
        float area=fabsf(r16_area2(a,3))*.5f;if(area<.01f)continue;
        float covered[4]={0};int other=first;
        for(int bucket=0;bucket<(indexed?priority:1);bucket++)
        for(int node=indexed?r42_head(axis,bucket,p[0][axis]):0;node>=0&&(indexed||node<model->face_count);node=indexed?r42_links[node]:node+1) {
            int g=indexed?node/3:node;if(indexed&&node%3!=axis)continue;
            if(model->face_infos&&model->face_infos[g]==-1)continue;
            Vertex*q=verts+(indexed?backing_vertex[g]:other);if(!indexed)other+=3;
            int lower=model->face_priorities?model->face_priorities[g]:model->model_priority;
            if((indexed&&lower!=bucket)||lower<0||lower>=priority||q[0].a!=255||q[1].a!=255||q[2].a!=255||
               (model->face_infos&&(model->face_infos[g]&2)))continue;
            float xyz[3][3]={{q[0].x,q[0].y,q[0].z},{q[1].x,q[1].y,q[1].z},{q[2].x,q[2].y,q[2].z}};
            if(xyz[0][axis]!=p[0][axis]||xyz[1][axis]!=p[0][axis]||xyz[2][axis]!=p[0][axis])continue;
            R16Point2 b[3];for(int k=0;k<3;k++)b[k]=(R16Point2){xyz[k][u]-p[0][u],xyz[k][w]-p[0][w]};
            covered[lower]+=r16_triangle_overlap(a,b);
        }
        int found=0;for(int lower=0;lower<priority;lower++)if(covered[lower]>=area-.01f)found=1;
        if(!found)continue;
        if(priority>=3) {
            if(clock_count+3>R16_CLOCK_VERTS){window_layer_dropped++;continue;}
            memcpy(clock_cpu+clock_count,t,3*sizeof(Vertex));clock_count+=3;
        } else {
            if(trim_count+3>R16_TRIM_VERTS){window_layer_dropped++;continue;}
            memcpy(trim_cpu+trim_count,t,3*sizeof(Vertex));trim_count+=3;
        }
        window_layer_triangles++;
    }
#else
    (void)model;(void)bitset;(void)first;
#endif
}

static void wall_floor_cover(int bitset,int first) {
#if defined(GL11) && defined(__vita__)
    int id=((unsigned)bitset>>14)&32767;
    int live=!baking&&live_door_floor_context&&live_door_floor_level>=0&&live_door_floor_level<4;
    /* The appended maze doors occupy a separate underground scene beneath
     * overlapping level-3 crypt data. That crypt floor is not their ceiling
     * and must not erase the original horizontal top-corner faces. */
    if(live&&r95_static_maze_door(bitset))return;
    int source_level=live?live_door_floor_level:trim_source_plane;
    int bridge_support=id==1710;
    int legacy=bridge_support||id==1911||id==1912||id==1620||id==1621;
    /* This configured support family is also used inside castle walls.
     * Its cap can coincide with a normal upper floor, not just a bridge. */
    int upper_structural=wall_floor_context||r49_floor_support(id)||live;
    if((!baking&&!live)||!trim_world||(!legacy&&!upper_structural))return;
    for(int v=first;v+2<vertcount;v+=3){
        Vertex*t=verts+v;float height=t[0].y;
        if(t[1].y!=height||t[2].y!=height||t[0].a!=255||t[1].a!=255||t[2].a!=255)continue;
        /* Local coordinates avoid cancellation in area/overlap sums when
         * a small cap is placed thousands of units from the scene origin. */
        float origin_x=t[0].x,origin_z=t[0].z;
        R16Point2 a[3];for(int k=0;k<3;k++)a[k]=(R16Point2){t[k].x-origin_x,t[k].z-origin_z};
        float area=fabsf(r16_area2(a,3))*.5f;if(area<.01f)continue;
        int x0=(int)floorf(fminf(t[0].x,fminf(t[1].x,t[2].x))/128.f),x1=(int)floorf(fmaxf(t[0].x,fmaxf(t[1].x,t[2].x))/128.f);
        int z0=(int)floorf(fminf(t[0].z,fminf(t[1].z,t[2].z))/128.f),z1=(int)floorf(fmaxf(t[0].z,fmaxf(t[1].z,t[2].z))/128.f);
        int covered=0,partial=0;
        for(int level=trim_world->minLevel;level<trim_world->maxLevel&&!covered;level++){
            /* A lower-story structural cap can share its exact plane with
             * an upper floor. Only that actual upper floor may cover it.
             * Same-level terrain cannot erase exposed dungeon wall tops. */
            /* Bridge shifting changes the array slot, not the physical plane. */
            float sum=0;
            for(int x=x0;x<=x1;x++)for(int z=z0;z<=z1;z++){
                if(x<0||z<0||x>=trim_world->maxTileX||z>=trim_world->maxTileZ)continue;
                Ground*g=trim_world->levelTiles[level][x][z];if(!g||g->drawLevel>trim_top_level)continue;
                if(!legacy&&g->occludeLevel<=source_level)continue;
                /* Bridge piers author a horizontal cap exactly on the deck.
                 * Only a real bridge deck can cover this family; retain its
                 * sides, bottom, exposed ledges and non-bridge placements. */
                if(bridge_support&&(!g->bridge||g->occludeLevel<=0))continue;
                if(g->underlay){
                    TileUnderlay*u=g->underlay;if(u->southwestColor==12345678&&u->northeastColor==12345678)continue;
                    int h=g->occludeLevel;
                    if(trim_world->levelHeightmaps[h][x][z]!=height||trim_world->levelHeightmaps[h][x+1][z]!=height||trim_world->levelHeightmaps[h][x+1][z+1]!=height||trim_world->levelHeightmaps[h][x][z+1]!=height)continue;
                    float x0=x*128-origin_x,x1=(x+1)*128-origin_x,z0=z*128-origin_z,z1=(z+1)*128-origin_z;
                    R16Point2 b[2][3]={{{x0,z0},{x1,z0},{x1,z1}},{{x0,z0},{x1,z1},{x0,z1}}};
                    sum+=r51_cap_overlap(a,b[0],origin_x,origin_z,height,g->occludeLevel,x,z)+r51_cap_overlap(a,b[1],origin_x,origin_z,height,g->occludeLevel,x,z);
                }else if(g->overlay){
                    TileOverlay*o=g->overlay;
                    for(int f=0;f<o->triangleCount;f++){
                        int texture=o->triangleTextureIds?o->triangleTextureIds[f]:-1;if(texture==-1&&o->triangleColorA[f]==12345678)continue;
                        int ids[3]={o->triangleVertexA[f],o->triangleVertexB[f],o->triangleVertexC[f]};R16Point2 b[3];int flat=1;
                        for(int k=0;k<3;k++){int i=ids[k];if(o->vertexY[i]!=height){flat=0;break;}b[k]=(R16Point2){o->vertexX[i]-origin_x,o->vertexZ[i]-origin_z};}
                        if(flat)sum+=r51_cap_overlap(a,b,origin_x,origin_z,height,g->occludeLevel,x,z);
                    }
                }
            }
            if(sum>.01f)partial=1;
            if(sum>=area-.01f)covered=1;
        }
        /* Keep a wall/door/column's authored top when its cover can hide.
         * The following r47 contact pass recesses its coplanar points one
         * unit below that physical floor, so normal depth testing covers
         * the cap upstairs and reveals it when the roof/floor is hidden.
         * Column/bridge-pier tops use the same actual-floor depth separation;
         * unrelated non-column structural models retain their old policy. */
        if(covered&&(wall_floor_context||live||r107_column_cap(id)))continue;
        /* Per-frame doors cannot append to the bake-owned replay buffer.
         * Collapse only a fully covered cap, keeping opening/partial caps
         * unchanged. Their current open/closed pose is checked each frame. */
        if(live&&!covered)continue;
        if(bridge_support&&!covered)continue;
        if(!covered&&partial){
            if(floorcap_count+3<=R16_FLOORCAP_VERTS){memcpy(floorcap_cpu+floorcap_count,t,3*sizeof(Vertex));floorcap_count+=3;wall_floor_partial++;}
            else {wall_floor_dropped++;partial=0;}
        }
        if(covered||partial){t[1].x=t[2].x=t[0].x;t[1].y=t[2].y=t[0].y;t[1].z=t[2].z=t[0].z;if(covered)wall_floor_triangles++;}
    }
#else
    (void)bitset;(void)first;
#endif
}

#if defined(GL11) && defined(__vita__)
#include "r47_floor_contacts.inc"
#endif

#include "r16_furniture_layers.inc"

#if defined(GL11) && defined(__vita__)
static int take_model_water(int first_vertex,int first_element);
#endif

#include "r28_window_planes.inc"
#include "r29_live_furnace.inc"
#include "r50_wall_furniture.inc"
#if defined(GL11)&&defined(__vita__)
#include "r53_ship_priority.inc"
#include "r54_crane_duplicates.inc"
#include "r55_fixture_contacts.inc"
#include "r69_roof_layers.inc"
#include "r70_plant_contact.inc"
#include "r71_shore_plants.inc"
#include "r56_hull_layers.inc"
#endif

static void emit_placed(Model *model, int yaw, int x, int y, int z, int bitset) {
    if (!model || overflow) return;
    int need = model->face_count * 3;
    if (need < 0) return;
#if defined(GL11)&&defined(__vita__)
    if(r52_active(bitset)&&need<3072)need=3072;
#endif
    if (baking) {
        if (!flush_if_full(need)) {
            overflow = 1;
            return;
        }
    } else if (vertcount + need > R16_VERT_BUDGET || elementcount + need > R16_VERT_BUDGET) {
        overflow = 1;
        return;
    }
    int wood_first = vertcount;
#if defined(GL11)&&defined(__vita__)
    int roof_decal_first=r26_count,roof_trim_first=trim_count,roof_clock_first=clock_count,roof_cap_first=floorcap_count;
#endif
    R16ModelCost model_cost __attribute__((cleanup(bake_model_cost_end)))={baking&&(model_profile_enabled==1||(model_profile_enabled==3&&((r39_sequence++&63u)==0)))?ba_timing_now():0,((unsigned)bitset>>14)&32767,model->face_count,x,z};
    uint64_t model_phase_at=model_cost.at;profile_model=model_cost.at?&model_cost:NULL;
#if defined(GL11) && defined(__vita__)
    int water_first_element=elementcount,saved_scroll=freeze_scroll,model_water=0;
    if(baking)freeze_scroll=1;
#endif
    model_draw(model, yaw, _World3D.sinEyePitch, _World3D.cosEyePitch, _World3D.sinEyeYaw, _World3D.cosEyeYaw, x - _World3D.eyeX, y - _World3D.eyeY, z - _World3D.eyeZ, bitset);
#if defined(GL11) && defined(__vita__)
    freeze_scroll=saved_scroll;
#endif
#if defined(GL11)&&defined(__vita__)
    extern int gl_model_water_possible(void);model_water=baking&&gl_model_water_possible();
    if(model_water)r32_mark_vertices(wood_first,vertcount);
    if(baking){if(model_water)r40_scan_models++;else{r40_skip_models++;r40_skip_vertices+=(unsigned)(vertcount-wood_first);}}
#endif

#if defined(GL11)&&defined(__vita__)
    r54_crane_duplicates(model,bitset,wood_first);
#endif
    bake_phase_mark(&model_phase_at,0);
#if defined(GL11) && defined(__vita__)
    if(geometry_diag!=5&&geometry_diag!=6&&geometry_diag!=7)
#endif
    wood_geometry_capture(model, yaw, x, y, z, bitset, wood_first);
    stove_grate_separate(model,bitset,wood_first);
    stove_back_separate(model,bitset,wood_first,x,z);
    r29_furnace_layers(model,bitset,wood_first);
    wall_floor_cover(bitset,wood_first);
#if defined(GL11) && defined(__vita__)
    int contact_id=((unsigned)bitset>>14)&32767;
    if((baking&&(wall_floor_context||r49_floor_support(contact_id)))||(!baking&&live_door_floor_context)){
        int contact_level=baking?trim_source_plane:live_door_floor_level;
        r47_recess_contacts(verts,wood_first,vertcount,contact_level);
        if(baking)r47_recess_contacts(floorcap_cpu,roof_cap_first,floorcap_count,contact_level);
    }
#endif
    window_trim_capture(model, x, y, z, bitset, wood_first);
    dresser_wall_fit(x, z, bitset, wood_first);
    r50_furniture_wall_fit(x,y,z,bitset,wood_first);
    shutter_attach(x, z, bitset, wood_first);
    r28_window_attach(x,z,bitset,wood_first,yaw);
#if defined(GL11) && defined(__vita__)
    r65_bank_texture_lighting(model,bitset,wood_first);
#endif
#if defined(GL11) && defined(__vita__)
    if(geometry_diag==5)wood_geometry_capture(model,yaw,x,y,z,bitset,wood_first);
#endif
#if defined(GL11) && defined(__vita__)
    bake_phase_mark(&model_phase_at,1);
    if(r78_fur_type(((unsigned)bitset>>14)&32767))r71_ground_attachment(model,wood_first,x,y,z,yaw,5,&r78_fur_contacts);
    r26_capture(model,bitset,wood_first);
    r53_ship_rail_capture(model,bitset,wood_first);
    r55_fixture_contacts(model,bitset,wood_first);
    r69_roof_layers(model,bitset,wood_first);
    r70_plant_contact(model,bitset,wood_first);
    r71_shore_plants(model,bitset,wood_first,x,y,z,yaw);
    r56_hull_layers(model,x,y,z,yaw,bitset,wood_first);
    bake_phase_mark(&model_phase_at,2);
#endif
    rug_layer_capture(model, bitset, wood_first);
    window_layer_capture(model, bitset, wood_first);
#if defined(GL11)&&defined(__vita__)
    if(geometry_diag==7)wood_geometry_capture(model,yaw,x,y,z,bitset,wood_first);
#endif
#if defined(GL11)&&defined(__vita__)
    if(geometry_diag==6){
        int plane=baking?trim_source_plane:live_door_floor_level;
        if(plane<0||plane>3)plane=3;
        unsigned code=(((unsigned)bitset>>14)&32767u)|((unsigned)plane<<15);
        r48_source_colour(verts,wood_first,vertcount,code,1);
        r48_source_colour(r26_vertices,roof_decal_first,r26_count,code,1);
        r48_source_colour(trim_cpu,roof_trim_first,trim_count,code,1);
        r48_source_colour(clock_cpu,roof_clock_first,clock_count,code,1);
        r48_source_colour(floorcap_cpu,roof_cap_first,floorcap_count,code,1);
        wood_geometry_capture(model,yaw,x,y,z,bitset,wood_first);
    }
    r32_mark_faces(r32_decal_tags,65532/3,roof_decal_first,r26_count);
    r32_mark_faces(r32_trim_tags,32760/3,roof_trim_first,trim_count);
    r32_mark_faces(r32_clock_tags,8190/3,roof_clock_first,clock_count);
    r32_mark_faces(r32_floorcap_tags,8190/3,roof_cap_first,floorcap_count);
#endif
    bake_phase_mark(&model_phase_at,3);
#if defined(GL11) && defined(__vita__)
    if(baking&&model_water&&!take_model_water(wood_first,water_first_element))overflow=1;
    if(baking&&!overflow)r52_clip_top(bitset,wood_first,water_first_element);
    if(baking&&!overflow){
        r35_key[0]=model->face_indices_a;r35_key[1]=model->face_indices_b;r35_key[2]=model->face_indices_c;
        r31_pack_span(wood_first,water_first_element);memset(r35_key,0,sizeof r35_key);
    }
#endif
    bake_phase_mark(&model_phase_at,4);
    if (!baking && over_budget()) overflow = 1;
}

static void pick_one(Model *model, int yaw, int x, int y, int z, int bitset);

static void emit_static_model(Model *model, int yaw, int x, int y, int z, int bitset) {
    if (!model) return;
    if (picking) {
        pick_one(model, yaw, x, y, z, bitset);
        return;
    }
    if(r16_live_location(bitset))return;
    if (model->r10_animated) {
        if (!remember_anim(model, x, y, z, yaw, bitset) && !collecting) emit_placed(model, yaw, x, y, z, bitset);
        return;
    }
    if (collecting) return;
    remember_pick(model, x, y, z, yaw, bitset);
    emit_placed(model, yaw, x, y, z, bitset);
}

#ifdef GL11
#ifdef __vita__
static Vertex *water_cpu;
static uint16_t *water_cpu_idx;
static int water_vn, water_in;
static R16Chunk water_gpu[4];
static int water_gpu_n;

static void water_release(void) {
    for (int i = 0; i < water_gpu_n; i++) {
        if (water_gpu[i].vbo) glDeleteBuffers(1, &water_gpu[i].vbo);
        if (water_gpu[i].ibo) glDeleteBuffers(1, &water_gpu[i].ibo);
        water_gpu[i].vbo = 0;
        water_gpu[i].ibo = 0;
        water_gpu[i].count = 0;
    }
    water_gpu_n = 0;
    water_vn = 0;
    water_in = 0;
}

static int water_flush(void) {
    if (water_vn <= 0 || water_in <= 0) {
        water_vn = 0;
        water_in = 0;
        return 1;
    }
    if (water_gpu_n >= 4) return 0;
    GLuint vbo = 0, ibo = 0;
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ibo);
    if (!vbo || !ibo) {
        if (vbo) glDeleteBuffers(1, &vbo);
        if (ibo) glDeleteBuffers(1, &ibo);
        return 0;
    }
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizei)((size_t)water_vn * sizeof(Vertex)), water_cpu, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizei)((size_t)water_in * sizeof(uint16_t)), water_cpu_idx, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    if(r32_focus_ready&&(!r32_vertex_triangle_tags(water_cpu_idx,water_in,r32_water_vertex_tags,water_vn)||!r32_register_indices(ibo,water_cpu_idx,r32_sorted_tags,water_in,1,&water_in))){glDeleteBuffers(1,&vbo);glDeleteBuffers(1,&ibo);return 0;}
    water_gpu[water_gpu_n].vbo = vbo;
    water_gpu[water_gpu_n].ibo = ibo;
    water_gpu[water_gpu_n].count = water_in;
    water_gpu_n++;
    water_vn = 0;
    water_in = 0;
    return 1;
}

static int take_water(int v0, int e0) {
    int nv = vertcount - v0;
    int ne = elementcount - e0;
    if (nv <= 0 || ne <= 0) return 1;
    if (water_vn + nv > R16_CHUNK_VERTS || water_in + ne > R16_CHUNK_VERTS) {
        if (!water_flush()) return 0;
    }
    if (!water_cpu) {
        water_cpu = malloc((size_t)R16_CHUNK_VERTS * sizeof(Vertex));
        water_cpu_idx = malloc((size_t)R16_CHUNK_VERTS * sizeof(uint16_t));
        if (!water_cpu || !water_cpu_idx) return 0;
    }
    int base = water_vn;
    memcpy(water_cpu + base, verts + v0, (size_t)nv * sizeof(Vertex));
    memcpy(r32_water_vertex_tags+base,r32_vertex_tags+v0,(size_t)nv*sizeof(uint16_t));
    for (int i = 0; i < ne; i++) {
        uint32_t ix = indices[e0 + i];
        if (ix < (uint32_t)v0 || ix >= (uint32_t)vertcount) return 0;
        water_cpu_idx[water_in++] = (uint16_t)(ix - (uint32_t)v0 + (uint32_t)base);
    }
    water_vn += nv;
    return 1;
}
#include "r18_model_water.inc"
#else
static void water_release(void) {}
static int water_flush(void) { return 1; }
static int take_water(int v0, int e0) { (void)v0; (void)e0; return 1; }
#endif
#endif

static void emit_floor(World3D *world, TileUnderlay *underlay, TileOverlay *overlay, int level, int x, int z) {
    if (picking || collecting || overflow) return;
    int scrolling = (underlay && water_under(underlay)) || (!underlay && overlay && water_over(overlay));
#ifdef __vita__
    if (baking && scrolling) {
        if (vertcount > R16_CHUNK_VERTS - 1024 || elementcount > R16_CHUNK_VERTS - 1024) {
            if (!flush_chunk()) {
                overflow = 1;
                return;
            }
        }
        int v0 = vertcount, e0 = elementcount;
        freeze_scroll = 1;
        if (underlay)
            world3d_draw_tileunderlay(world, underlay, level, x, z, _World3D.sinEyePitch, _World3D.cosEyePitch, _World3D.sinEyeYaw, _World3D.cosEyeYaw);
        else if (overlay)
            world3d_draw_tileoverlay(x, z, overlay, _World3D.sinEyePitch, _World3D.cosEyePitch, _World3D.sinEyeYaw, _World3D.cosEyeYaw);
        freeze_scroll = 0;
        r32_mark_vertices(v0,vertcount);
        if (!take_water(v0, e0)) overflow = 1;
        vertcount = v0;
        elementcount = e0;
        return;
    }
#endif
    int floor_reserve=1024;
#if defined(GL11) && defined(__vita__)
    if(r51_has(world,level,x,z))floor_reserve=3072;
#endif
    if (baking && (vertcount > R16_CHUNK_VERTS - floor_reserve || elementcount > R16_CHUNK_VERTS - floor_reserve)) {
        if (!flush_chunk()) {
            overflow = 1;
            return;
        }
    }
    int terrain_first=vertcount,terrain_first_element=elementcount;
    if (underlay) {
        if (scrolling && remember_water(underlay, NULL, level, x, z)) return;
        world3d_draw_tileunderlay(world, underlay, level, x, z, _World3D.sinEyePitch, _World3D.cosEyePitch, _World3D.sinEyeYaw, _World3D.cosEyeYaw);
    } else if (overlay) {
        if (scrolling && remember_water(NULL, overlay, level, x, z)) return;
        world3d_draw_tileoverlay(x, z, overlay, _World3D.sinEyePitch, _World3D.cosEyePitch, _World3D.sinEyeYaw, _World3D.cosEyeYaw);
    }
#if defined(GL11) && defined(__vita__)
    if(baking)r51_clip_floor(level,x,z,terrain_first,terrain_first_element);
    if(baking&&terrain_file&&iabs(x-anchor_x)<=12&&iabs(z-anchor_z)<=12&&vertcount>terrain_first){
        int header[]={0x464c4f52,x,z,level,underlay?underlay->textureId:-2,vertcount-terrain_first,underlay?0:1};
        fwrite(header,sizeof(header),1,terrain_file);
        fwrite(verts+terrain_first,sizeof(Vertex),vertcount-terrain_first,terrain_file);
    }
    if(baking){
        if(geometry_diag==6)for(int v=terrain_first;v+2<vertcount;v+=3){
            unsigned code=(1u<<17)|((((unsigned)(v-terrain_first)/3u)&1u)<<16)|((unsigned)level<<14)|((unsigned)x<<7)|(unsigned)z;
            r48_source_colour(verts,v,v+3,code,1);
        }
        r31_pack_span(terrain_first,terrain_first_element);r20_mark_paving(verts,terrain_first,vertcount,paving_tags,R16_CHUNK_VERTS);if(level>0&&!overflow)memset(floor_tags+terrain_first,1,(size_t)(vertcount-terrain_first));}
#endif
    if (over_budget()) overflow = 1;
}

static void emit_wall(Wall *wall) {
    if (!wall) return;
#if defined(GL11) && defined(__vita__)
    int saved_wall_context=wall_floor_context;wall_floor_context=1;
#endif
    if (wall->modelA) {
        note_dyn(&wall->modelA, wall->x, wall->y, wall->z, 0, wall->bitset);
        emit_static_model(wall->modelA, 0, wall->x, wall->y, wall->z, wall->bitset);
    }
    if (wall->modelB) {
        note_dyn(&wall->modelB, wall->x, wall->y, wall->z, 0, wall->bitset);
        emit_static_model(wall->modelB, 0, wall->x, wall->y, wall->z, wall->bitset);
    }
#if defined(GL11) && defined(__vita__)
    wall_floor_context=saved_wall_context;
#endif
}

/* Keep the authored diagonal corner placement, including the opposite-facing
 * outset used by shape7. Both transforms are needed for shape8. The retained
 * mesh, animation records and picking all consume these same placements. */
static void emit_decor_placed(Decor *decor,int yaw,int x,int z) {
    note_dyn(&decor->model,x,decor->y,z,yaw,decor->bitset);
#ifdef __vita__
    window_decor_context=1;
#endif
    emit_static_model(decor->model,yaw,x,decor->y,z,decor->bitset);
#ifdef __vita__
    window_decor_context=0;
#endif
}
static void emit_decor(Decor *decor) {
    if(!decor||!decor->model)return;
    R25DecorPlacement p[2];
    int n=r25_decor_placements(decor->type,decor->angle,decor->x,decor->z,p);
    for(int i=0;i<n;i++)emit_decor_placed(decor,p[i].yaw,p[i].x,p[i].z);
}

static void emit_locs(Ground *tile, int x, int z) {
    for (int i = 0; i < tile->locCount; i++) {
        Location *loc = tile->locs[i];
        if (!loc || loc->entity || loc->r10_temporary || !loc->model) continue;
        if (loc->minSceneTileX != x || loc->minSceneTileZ != z) continue;
#if defined(GL11) && defined(__vita__)
        /* Shape9 is a structural diagonal wall stored as a Location.
         * It needs the same floor coverage/contact checks as Ground.wall. */
        int saved_wall_context=wall_floor_context;
        if((loc->info&31)==9)wall_floor_context=1;
#endif
        note_dyn(&loc->model, loc->x, loc->y, loc->z, loc->yaw, loc->bitset);
        emit_static_model(loc->model, loc->yaw, loc->x, loc->y, loc->z, loc->bitset);
#if defined(GL11) && defined(__vita__)
        wall_floor_context=saved_wall_context;
#endif
    }
}

static void emit_items(Ground *tile) {
    GroundObject *stack = tile->groundObj;
    if (!stack) return;
    int y = stack->offset ? stack->y - stack->offset : stack->y;
    Model *models[3] = {stack->bottomObj, stack->middleObj, stack->topObj};
    if (picking) {
        for (int i = 0; i < 3; i++) {
            if (models[i]) pick_one(models[i], 0, stack->x, y, stack->z, stack->bitset);
        }
        return;
    }
    for (int i = 0; i < 3; i++) {
        if (!models[i]) continue;
        if (!remember_item(models[i], stack->x, y, stack->z, stack->bitset) && !collecting)
            emit_placed(models[i], 0, stack->x, y, stack->z, stack->bitset);
    }
}

static void emit_tile(World3D *world, Ground *tile, int level, int x, int z) {
#if defined(GL11) && defined(__vita__)
    trim_world = world;
    trim_level = level;trim_source_plane=tile->occludeLevel;
#endif
    if (tile->bridge && (baking||!R32_ENABLED||r32_tile_visible(tile->bridge->drawLevel,x,z,_World3D.topLevel))) {
#if defined(GL11) && defined(__vita__)
        if(bridge_file)fprintf(bridge_file,"%d,%d,%d,%d,%d,%d,%d\n",x,z,level,tile->occludeLevel,world->levelHeightmaps[level][x][z],world->levelHeightmaps[tile->occludeLevel][x][z],tile->underlay!=NULL);
        trim_level = 0;trim_source_plane=tile->bridge->occludeLevel;
        r32_current_tag=r32_tile_tag(tile->bridge->drawLevel,x,z);
#endif
        Ground *bridge = tile->bridge;
        emit_floor(world, bridge->underlay, bridge->underlay ? NULL : bridge->overlay, 0, x, z);
        emit_wall(bridge->wall);
        emit_locs(bridge, x, z);
    }
#if defined(GL11) && defined(__vita__)
    trim_level = level;trim_source_plane=tile->occludeLevel;
    r32_current_tag=r32_tile_tag(tile->drawLevel,x,z);
#endif
    if(!baking&&R32_ENABLED&&!r32_tile_visible(tile->drawLevel,x,z,_World3D.topLevel))return;
#if defined(GL11) && defined(__vita__)
    if(baking&&floor_file&&iabs(x-anchor_x)<=12&&iabs(z-anchor_z)<=12&&(tile->underlay||tile->overlay)){
        int h=tile->occludeLevel;
        fprintf(floor_file,"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",x,z,level,h,world->levelHeightmaps[h][x][z],world->levelHeightmaps[h][x+1][z],world->levelHeightmaps[h][x+1][z+1],world->levelHeightmaps[h][x][z+1],tile->wall?((unsigned)tile->wall->bitset>>14)&32767:-1,tile->underlay!=NULL);
    }
#endif
    emit_floor(world, tile->underlay, tile->underlay ? NULL : tile->overlay, tile->occludeLevel, x, z);
    emit_wall(tile->wall);
    emit_decor(tile->decor);
    if (tile->groundDecor && tile->groundDecor->model) {
        note_dyn(&tile->groundDecor->model, tile->groundDecor->x, tile->groundDecor->y, tile->groundDecor->z, 0, tile->groundDecor->bitset);
        emit_static_model(tile->groundDecor->model, 0, tile->groundDecor->x, tile->groundDecor->y, tile->groundDecor->z, tile->groundDecor->bitset);
    }
    emit_locs(tile, x, z);
    emit_items(tile);
}

static int live_model(Model *model) {
    uintptr_t x;
    if (!model) return 0;
    if (model->vertex_count <= 0 || model->vertex_count > 20000) return 0;
    if (model->face_count < 0 || model->face_count > 20000) return 0;
    if (!model->vertices_x || !model->vertices_y || !model->vertices_z) return 0;
    x = (uintptr_t)model->vertices_x;
    if (x < 0x80000000u) return 0;
    x = (uintptr_t)model->vertices_y;
    if (x < 0x80000000u) return 0;
    x = (uintptr_t)model->vertices_z;
    if (x < 0x80000000u) return 0;
    return 1;
}

static void pick_one(Model *model, int yaw, int x, int y, int z, int bitset) {
    /* model_draw cannot record a hover hit for a nonpositive key. */
    if(bitset<=0)return;
    if (!live_model(model)) return;
    int nx,ny,sx,sy;
    int depth=project_camera(x-_World3D.eyeX,y-_World3D.eyeY,z-_World3D.eyeZ,&nx,&ny);
    if(!depth||!r83_model_maybe(nx,ny,depth,_Pix3D.center_x,_Pix3D.center_y,_Model.mouse_x,_Model.mouse_y,model->radius))return;
    r82_project_divide(nx,ny,depth,&sx,&sy);sx+=_Pix3D.center_x;sy+=_Pix3D.center_y;
    int reach = 64 + ((model->radius > 0 ? model->radius : 64) << 9) / depth;
    if (iabs(sx - _Model.mouse_x) > reach || iabs(sy - _Model.mouse_y) > reach) return;
    int saved_emit = emitting, saved_suppress = suppress;
    emitting = 0;
    suppress = 1;
    model_draw(model, yaw, _World3D.sinEyePitch, _World3D.cosEyePitch, _World3D.sinEyeYaw, _World3D.cosEyeYaw, x - _World3D.eyeX, y - _World3D.eyeY, z - _World3D.eyeZ, bitset);
    emitting = saved_emit;
    suppress = saved_suppress;
}

#include "r19_live_tile_index.inc"

/* Read models from the tiles that exist right now. The bake's pick list
 * still pointed at tutorial-island models after Lumbridge freed them. */
static void pick_live(World3D *world, int top_level) {
    picking = 1;
    if(live_tile_index(world,top_level)) {
        for(int i=0;i<live_pick_n;i++) {
            unsigned c=live_pick_tiles[i];int level=c>>16,x=(c>>8)&255,z=c&255;
            if(x<_World3D.minDrawTileX||x>=_World3D.maxDrawTileX||z<_World3D.minDrawTileZ||z>=_World3D.maxDrawTileZ)continue;
            Ground *tile=world->levelTiles[level][x][z];
            if(!tile||(!R32_ENABLED&&tile->drawLevel>top_level))continue;
            int height=world->levelHeightmaps[level][x][z];
            if(!tile_pointer_near(((x<<7)+64)-_World3D.eyeX,height-_World3D.eyeY,((z<<7)+64)-_World3D.eyeZ))continue;
            emit_tile(world,tile,level,x,z);
        }
        picking=0;return;
    }
    for (int level = world->minLevel; level < world->maxLevel; level++) {
        for (int x = _World3D.minDrawTileX; x < _World3D.maxDrawTileX; x++) {
            for (int z = _World3D.minDrawTileZ; z < _World3D.maxDrawTileZ; z++) {
                Ground *tile = world->levelTiles[level][x][z];
                if (!tile || (!R32_ENABLED&&tile->drawLevel > top_level)) continue;
                /* Hover only needs models near the pointer. Far tiles stay in the static mesh. */
                int height = world->levelHeightmaps[level][x][z];
                if(!tile_pointer_near(((x<<7)+64)-_World3D.eyeX,height-_World3D.eyeY,((z<<7)+64)-_World3D.eyeZ))continue;
                emit_tile(world, tile, level, x, z);
            }
        }
    }
    picking = 0;
}

static void pick_tiles(World3D *world) {
    int x0 = _World3D.minDrawTileX, x1 = _World3D.maxDrawTileX;
    int z0 = _World3D.minDrawTileZ, z1 = _World3D.maxDrawTileZ;
    for (int level = world->minLevel; level < world->maxLevel && _World3D.clickTileX == -1; level++) {
        for (int x = x0; x < x1 && _World3D.clickTileX == -1; x++) {
            for (int z = z0; z < z1; z++) {
                Ground *tile = world->levelTiles[level][x][z];
                if (!tile || !r32_tile_visible(tile->drawLevel,x,z,_World3D.topLevel)) continue;
                int sx, sy;
                int height = world->levelHeightmaps[level][x][z];
                if (!project_point(((x << 7) + 64) - _World3D.eyeX, height - _World3D.eyeY, ((z << 7) + 64) - _World3D.eyeZ, &sx, &sy)) continue;
                if (iabs(sx - _World3D.mouseX) > 220 || iabs(sy - _World3D.mouseY) > 220) continue;
                if (tile->underlay)
                    world3d_draw_tileunderlay(world, tile->underlay, level, x, z, _World3D.sinEyePitch, _World3D.cosEyePitch, _World3D.sinEyeYaw, _World3D.cosEyeYaw);
                else if (tile->overlay)
                    world3d_draw_tileoverlay(x, z, tile->overlay, _World3D.sinEyePitch, _World3D.cosEyePitch, _World3D.sinEyeYaw, _World3D.cosEyeYaw);
                if (_World3D.clickTileX != -1) return;
            }
        }
    }
}

typedef struct {
    uint32_t us;
    int faces, verts, tex, kind;
} DynHit;

static DynHit dyn_top[8];
static int dyn_top_n;
static uint64_t acc_water, acc_anim, acc_item, acc_loc, acc_entity, acc_pick, acc_refresh, acc_dynamic;
static uint64_t acc_actor_emit,acc_actor_layers;
static unsigned acc_actor_textured,acc_actor_plain;
static unsigned acc_anim_view_tested,acc_anim_view_skipped;
static int acc_frames, acc_water_n, acc_anim_n, acc_item_n, acc_loc_n;
static int acc_anim_faces, acc_item_faces, acc_loc_faces, acc_entity_faces, acc_same, acc_anim_ptrs;
static int acc_hits, acc_misses, acc_cached_draw;
static int acc_miss_new, acc_miss_sig, acc_miss_pose;
static uintptr_t prev_anim_ptr[64];
static int prev_anim_n;

static void dyn_consider(int kind, Model *model, uint64_t us) {
    DynHit hit;
    if (!model || us == 0) return;
    hit.us = (uint32_t)us;
    hit.faces = model->face_count;
    hit.verts = model->vertex_count;
    hit.tex = model->textured_face_count;
    hit.kind = kind;
    if (dyn_top_n < 8) {
        dyn_top[dyn_top_n++] = hit;
        return;
    }
    int small = 0;
    for (int i = 1; i < 8; i++) if (dyn_top[i].us < dyn_top[small].us) small = i;
    if (hit.us > dyn_top[small].us) dyn_top[small] = hit;
}

static int model_sig(Model *model) {
    int count = model->vertex_count;
    int sig = model->face_count * 1315423911 ^ count;
    if (count > 0 && model->vertices_x && model->vertices_y && model->vertices_z) {
        int mid = count / 2;
        int last = count - 1;
        sig ^= model->vertices_x[0] ^ model->vertices_y[0] ^ model->vertices_z[0];
        sig ^= model->vertices_x[mid] ^ model->vertices_y[mid] ^ model->vertices_z[mid];
        sig ^= model->vertices_x[last] ^ model->vertices_y[last] ^ model->vertices_z[last];
    }
    if (model->face_count > 0 && model->face_color_a)
        sig ^= model->face_color_a[0] ^ model->face_color_a[model->face_count / 2];
    return sig;
}

/* Player and NPC structs start with PathingEntity, which starts with Entity.
 * The animation frame is what actually changes the mesh. Three vertex samples
 * can stay put while the rest of an idle pose moves. */
static int entity_anim_sig(Entity *entity) {
    unsigned sig;
    PathingEntity *path;
    if (!entity || !entity->type) return 0;
    if (strcmp(entity->type, "player") != 0 && strcmp(entity->type, "npc") != 0) return 0;
    path = (PathingEntity *)entity;
    sig = (unsigned)path->primarySeqId * 1315423911u;
    sig ^= (unsigned)path->primarySeqFrame * 16777619u;
    sig ^= (unsigned)path->secondarySeqId * 2166136261u;
    sig ^= (unsigned)path->secondarySeqFrame * 374761393u;
    sig ^= (unsigned)path->spotanimId * 668265263u;
    sig ^= (unsigned)path->spotanimFrame * 2246822519u;
    return (int)sig;
}

static int model_in_view(Model *model, int scene_x, int scene_y, int scene_z) {
    int sin_yaw = _World3D.sinEyeYaw, cos_yaw = _World3D.cosEyeYaw;
    int sin_pitch = _World3D.sinEyePitch, cos_pitch = _World3D.cosEyePitch;
    int depth_y = (scene_z * cos_yaw - scene_x * sin_yaw) >> 16;
    int near_z = (scene_y * sin_pitch + depth_y * cos_pitch) >> 16;
    int radius_y = model->radius * cos_pitch >> 16;
    int far = near_z + radius_y;
    if (far <= 50 || near_z >= world3d_far_clip()) return 0;
    int screen_x = (scene_z * sin_yaw + scene_x * cos_yaw) >> 16;
    if (((screen_x - model->radius) << 9) / far >= _Pix2D.center_x) return 0;
    if (((screen_x + model->radius) << 9) / far <= -_Pix2D.center_x) return 0;
    int screen_y = (scene_y * cos_pitch - depth_y * sin_pitch) >> 16;
    int radius_x = model->radius * sin_pitch >> 16;
    if (((screen_y + radius_x) << 9) / far <= -_Pix2D.center_y) return 0;
    int top = radius_x + (model->max_y * cos_pitch >> 16);
    if (((screen_y - top) << 9) / far >= _Pix2D.center_y) return 0;
    return 1;
}

static unsigned r57_drawn;
unsigned r16_body_layer_faces(void){return r57_drawn;}

#if defined(GL11) && defined(__vita__)
#include "r57_body_render.inc"
typedef struct {
    Model *model;
    void *owner;
    uintptr_t verts;
    int x, y, z, yaw, sig, count;
    GLuint vbo, ibo;
    int ready;
    int loc_tag,loc_epoch;
    unsigned loc_version;
    size_t bytes;
    unsigned last_seen;
    unsigned queued_frame;
    int body_counts[10];
} DynGpu;

static unsigned dyn_cache_frame,dyn_promotions;
static uint64_t dyn_cache_bytes;
#define R16_DYN_CACHE_BYTES (6u*1024u*1024u)
#define R16_DYN_SLOTS 256
static DynGpu dyn_gpu[R16_DYN_SLOTS];
static DynGpu *dyn_drawq[R16_DYN_SLOTS];
static int dyn_draw_n;
#include "r58_dyn_index.inc"

static void dyn_slot_clear(DynGpu *slot) {
    r58_dirty=1;
    if(slot->bytes<=dyn_cache_bytes)dyn_cache_bytes-=slot->bytes;else dyn_cache_bytes=0;
    if (slot->vbo) glDeleteBuffers(1, &slot->vbo);
    if (slot->ibo) glDeleteBuffers(1, &slot->ibo);
    memset(slot, 0, sizeof *slot);
}

static int dyn_key_match(DynGpu *slot, Model *model, int x, int y, int z, int yaw, int sig, int bitset) {
    if((r16_live_location(bitset)||((unsigned)bitset&0x60000000u)==0x60000000u)&&(slot->loc_tag!=bitset||slot->loc_epoch!=stamp||slot->loc_version!=live_loc_revision[(unsigned)bitset&16383]))return 0;
    return slot->ready && !slot->owner && slot->model == model && slot->verts == (uintptr_t)model->vertices_x &&
        slot->x == x && slot->y == y && slot->z == z && slot->yaw == yaw && slot->sig == sig;
}

static int dyn_owner_match(DynGpu *slot, void *owner, int x, int y, int z, int yaw, int sig) {
    return slot->ready && owner && slot->owner == owner && slot->x == x && slot->y == y && slot->z == z && slot->yaw == yaw && slot->sig == sig;
}

static DynGpu *dyn_find(Model *model, int x, int y, int z, int yaw, int sig, int bitset) {
    if(r58_dirty)r58_rebuild();
    for(int i=r58_heads[r58_key(model,x,y,z,yaw)];i>=0;i=r58_next[i])if(dyn_key_match(&dyn_gpu[i],model,x,y,z,yaw,sig,bitset))return &dyn_gpu[i];
    return NULL;
}

static DynGpu *dyn_find_owner(void *owner, int x, int y, int z, int yaw, int sig) {
    for (int i = 0; i < R16_DYN_SLOTS; i++) if (dyn_owner_match(&dyn_gpu[i], owner, x, y, z, yaw, sig)) return &dyn_gpu[i];
    return NULL;
}

static int dyn_already_queued(DynGpu *slot) {
    return slot && dyn_cache_frame && slot->queued_frame==dyn_cache_frame;
}

static DynGpu *dyn_alloc_owner(void *owner, int x, int y, int z, int yaw) {
    DynGpu *loose = NULL;
    if (!owner) return NULL;
    for (int i = 0; i < R16_DYN_SLOTS; i++) {
        if (!dyn_gpu[i].ready || dyn_gpu[i].owner != owner) continue;
        if (dyn_gpu[i].x == x && dyn_gpu[i].y == y && dyn_gpu[i].z == z && dyn_gpu[i].yaw == yaw)
            return &dyn_gpu[i];
        if (!loose && !dyn_already_queued(&dyn_gpu[i])) loose = &dyn_gpu[i];
    }
    return loose;
}

/* 0 = first time this owner was seen, 1 = same pose but a new mesh, 2 = owner moved. */
static int dyn_owner_class(void *owner, int x, int y, int z, int yaw) {
    int pose = 0, any = 0;
    if (!owner) return 0;
    for (int i = 0; i < R16_DYN_SLOTS; i++) {
        if (!dyn_gpu[i].ready || dyn_gpu[i].owner != owner) continue;
        any = 1;
        if (dyn_gpu[i].x == x && dyn_gpu[i].y == y && dyn_gpu[i].z == z && dyn_gpu[i].yaw == yaw) pose = 1;
    }
    if (pose) return 1;
    if (any) return 2;
    return 0;
}

static DynGpu *dyn_alloc(Model *model, int x, int y, int z, int yaw) {
    for (int i = 0; i < R16_DYN_SLOTS; i++) {
        if (dyn_gpu[i].ready && !dyn_gpu[i].owner && dyn_gpu[i].model == model && dyn_gpu[i].x == x && dyn_gpu[i].y == y && dyn_gpu[i].z == z && dyn_gpu[i].yaw == yaw)
            return &dyn_gpu[i];
    }
    for (int i = 0; i < R16_DYN_SLOTS; i++) if (!dyn_gpu[i].ready) return &dyn_gpu[i];
    DynGpu *oldest=NULL;
    for(int i=0;i<R16_DYN_SLOTS;i++)if(!dyn_already_queued(&dyn_gpu[i])&&
       (!oldest||(unsigned)(dyn_cache_frame-dyn_gpu[i].last_seen)>(unsigned)(dyn_cache_frame-oldest->last_seen)))oldest=&dyn_gpu[i];
    return oldest;
}

/* Keep live-resource caching bounded and leave room for the title/runtime.
 * Visible queued meshes are never freed to make another cache entry fit.
 * A refused promotion leaves its CPU vertices intact for ordinary drawing. */
static int dyn_cache_reserve(DynGpu*target,size_t incoming){
 if(incoming>R16_DYN_CACHE_BYTES||dyn_already_queued(target))return 0;
 for(int pass=0;pass<=R16_DYN_SLOTS;pass++){
  uint64_t used=dyn_cache_bytes>=target->bytes?dyn_cache_bytes-target->bytes:0;
  uint64_t available=(uint64_t)vglMemFree(VGL_MEM_RAM)+vglMemFree(VGL_MEM_VRAM)+target->bytes;
  if(used+incoming<=R16_DYN_CACHE_BYTES&&available>=incoming+12u*1024u*1024u)return 1;
  DynGpu*victim=NULL;
  for(int i=0;i<R16_DYN_SLOTS;i++)if(&dyn_gpu[i]!=target&&dyn_gpu[i].ready&&!dyn_already_queued(&dyn_gpu[i])){victim=&dyn_gpu[i];break;}
  if(!victim)return 0;dyn_slot_clear(victim);
 }
 return 0;
}

static int dyn_promote(DynGpu *slot, Model *model, int x, int y, int z, int yaw, int sig, int v0, int e0, void *owner, int bitset, int body) {
    int nv = vertcount - v0;
    int ne = elementcount - e0;
    /* Cold scenery streams through the ordinary CPU path. Admit at most
     * one new GPU mesh per frame rather than synchronously allocating a
     * whole forest as the camera reveals it. Failed uploads use the same
     * bound; every refusal leaves emitted CPU geometry intact. */
    if(dyn_draw_n>=R16_DYN_SLOTS||dyn_promotions>=1)return 0;
    dyn_promotions++;
    if (nv <= 0 || ne <= 0 || nv > 65535 || ne > 65535) return 0;
    int extra=body?r57_pending_count:0;
    size_t gpu_bytes=(size_t)nv*sizeof(Vertex)+(size_t)(ne+extra)*sizeof(uint16_t);
    if(!dyn_cache_reserve(slot,gpu_bytes))return 0;
    uint16_t *short_idx = malloc((size_t)(ne+extra) * sizeof(uint16_t));
    if (!short_idx) return 0;
    for (int i = 0; i < ne; i++) {
        uint32_t ix = indices[e0 + i];
        if (ix < (uint32_t)v0 || ix >= (uint32_t)vertcount) {
            free(short_idx);
            return 0;
        }
        short_idx[i] = (uint16_t)(ix - (uint32_t)v0);
    }
    for(int i=0;i<extra;i++){uint32_t ix=r57_pending[i];if(ix<(uint32_t)v0||ix>=(uint32_t)vertcount){free(short_idx);return 0;}short_idx[ne+i]=(uint16_t)(ix-v0);}
    if (slot->ready) dyn_slot_clear(slot);
    GLuint vbo = 0, ibo = 0;
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ibo);
    if (!vbo || !ibo) {
        if (vbo) glDeleteBuffers(1, &vbo);
        if (ibo) glDeleteBuffers(1, &ibo);
        free(short_idx);
        return 0;
    }
    for (int i = 0; i < 8 && glGetError() != GL_NO_ERROR; i++) {}
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizei)((size_t)nv * sizeof(Vertex)), verts + v0, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizei)((size_t)(ne+extra) * sizeof(uint16_t)), short_idx, GL_STATIC_DRAW);
    GLenum err = glGetError();
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    free(short_idx);
    if (err != GL_NO_ERROR) {
        glDeleteBuffers(1, &vbo);
        glDeleteBuffers(1, &ibo);
        return 0;
    }
    slot->loc_tag=bitset;slot->loc_epoch=stamp;slot->loc_version=live_loc_revision[(unsigned)bitset&16383];
    slot->model = owner ? NULL : model;
    slot->owner = owner;
    slot->verts = owner ? 0 : (uintptr_t)model->vertices_x;
    slot->x = x;
    slot->y = y;
    slot->z = z;
    slot->yaw = yaw;
    slot->sig = sig;
    slot->count = ne;
    if(body)memcpy(slot->body_counts,r57_pending_counts,sizeof slot->body_counts);
    slot->vbo = vbo;
    slot->ibo = ibo;
    slot->bytes=gpu_bytes;dyn_cache_bytes+=gpu_bytes;
    slot->ready = 1;
    r58_dirty=1;
    vertcount = v0;
    elementcount = e0;
    return 1;
}

static void dyn_queue(DynGpu *slot) {
    slot->last_seen=dyn_cache_frame;
    if(dyn_already_queued(slot))return;
    if (dyn_draw_n < R16_DYN_SLOTS){dyn_drawq[dyn_draw_n++] = slot;slot->queued_frame=dyn_cache_frame;}
}

static int r62_queue(Model*m,int yaw,int x,int y,int z,int bitset);
static void emit_cached(Model *model, int yaw, int x, int y, int z, int bitset, int kind, uint64_t *spent, int *faces) {
    int sig = model_sig(model);
    int scene_x = x - _World3D.eyeX;
    int scene_y = y - _World3D.eyeY;
    int scene_z = z - _World3D.eyeZ;
    if (!model_in_view(model, scene_x, scene_y, scene_z)) return;
    if(kind==1&&r62_queue(model,yaw,x,y,z,bitset)){acc_hits++;if(model->face_count>0)*faces+=model->face_count;return;}
    DynGpu *slot = dyn_find(model, x, y, z, yaw, sig, bitset);
    if (slot) {
        dyn_queue(slot);
        acc_hits++;
        if (model->face_count > 0) *faces += model->face_count;
        return;
    }
    int v0 = vertcount, e0 = elementcount;
    int fixture_layers=kind==1&&(r87_vendor_type(((unsigned)bitset>>14)&32767)||r90_sewing_table_type(((unsigned)bitset>>14)&32767));
    uint64_t began = ba_timing_now();
    emit_placed(model, yaw, x, y, z, bitset);
    if(fixture_layers)r57_prepare_span(model,v0,e0);
    uint64_t us = ba_timing_now() - began;
    *spent += us;
    if (model->face_count > 0) *faces += model->face_count;
    dyn_consider(kind, model, us);
    acc_misses++;
    if (overflow) return;
    if(!r58_can_promote()){if(fixture_layers)r57_fallback_span();return;}
    slot = dyn_alloc(model, x, y, z, yaw);
    if (slot && dyn_promote(slot, model, x, y, z, yaw, sig, v0, e0, NULL, bitset, fixture_layers)) dyn_queue(slot);
    else if(fixture_layers)r57_fallback_span();
}
#else
static void emit_cached(Model *model, int yaw, int x, int y, int z, int bitset, int kind, uint64_t *spent, int *faces) {
    uint64_t began = ba_timing_now();
    emit_placed(model, yaw, x, y, z, bitset);
    uint64_t us = ba_timing_now() - began;
    *spent += us;
    if (model && model->face_count > 0) *faces += model->face_count;
    dyn_consider(kind, model, us);
}
#endif

#include "r18_actor_contact.inc"
#include "r18_player_ground.inc"
#include "r101_bloodworm_ground.inc"
#if defined(GL11)&&defined(__vita__)
#include "r62_shared_crops.inc"
void r16_crop_status(int out[4]){memset(out,0,4*sizeof(int));for(int i=0;i<R62_SLOTS;i++)if(r62_meshes[i].vbo&&r62_meshes[i].epoch==stamp)out[0]++;out[1]=r62_instance_n;out[2]=(int)r62_saved_vertices;out[3]=(int)r62_bytes;}
#else
void r16_crop_status(int out[4]){memset(out,0,4*sizeof(int));}
#endif

static void emit_dynamic(World3D *world, int loop_cycle) {
    player_contact_owner=NULL;player_contact_offset=0;
#if defined(GL11)&&defined(__vita__)
    gl_actor_cache_frame();
#endif
    int use_cache = dyn_cache_enabled();
    uint64_t began = ba_timing_now();
    int water_n = 0;
#if defined(GL11) && defined(__vita__)
    trim_world=world;
    r58_begin_frame();r57_cpu_count=0;r57_pending_count=0;r57_drawn=0;
    r62_prepare(use_cache);
#endif
    for (int i = 0; i < water_count && !overflow; i++) {
        water_n++;
        if (water[i].underlay)
            world3d_draw_tileunderlay(world, water[i].underlay, water[i].level, water[i].x, water[i].z, _World3D.sinEyePitch, _World3D.cosEyePitch, _World3D.sinEyeYaw, _World3D.cosEyeYaw);
        else if (water[i].overlay)
            world3d_draw_tileoverlay(water[i].x, water[i].z, water[i].overlay, _World3D.sinEyePitch, _World3D.cosEyePitch, _World3D.sinEyeYaw, _World3D.cosEyeYaw);
    }
    acc_water += ba_timing_now() - began;
    acc_water_n += water_n;
    int same = 0;
    for (int i = 0; i < anim_count && !overflow; i++) {
        Model *model = anims[i].model;
        int draw_x=anims[i].x,draw_z=anims[i].z,draw_yaw=anims[i].yaw;
        int door_moving=r96_swing(anims[i].bitset,anims[i].floor_level,stamp,ba_timing_now(),&draw_yaw,&draw_x,&draw_z,_Pix3D.sin_table,_Pix3D.cos_table);
#if defined(GL11) && defined(__vita__)
        /* Door poses are already resolved above. Skip only off-camera emitted
         * geometry; their live state, collision and independent picking remain. */
        if(model&&(model->r10_animated||r16_live_door(anims[i].bitset))){
            acc_anim_view_tested++;
            int hx=_Pix2D.center_x>_Pix3D.center_x?_Pix2D.center_x:_Pix3D.center_x;
            int hy=_Pix2D.center_y>_Pix3D.center_y?_Pix2D.center_y:_Pix3D.center_y;
            if(!r73_animated_in_view(model->radius,model->max_y,model->min_y,anims[i].x-_World3D.eyeX,anims[i].y-_World3D.eyeY,anims[i].z-_World3D.eyeZ,_World3D.sinEyeYaw,_World3D.cosEyeYaw,_World3D.sinEyePitch,_World3D.cosEyePitch,hx,hy,world3d_far_clip())){acc_anim_view_skipped++;continue;}
        }
#endif
#if defined(GL11) && defined(__vita__)
        live_door_floor_level=anims[i].floor_level;
        live_door_floor_context=r16_live_door(anims[i].bitset)&&live_door_floor_level>=0;
#endif
        int matched = 0;
        for (int p = 0; p < prev_anim_n; p++) if (prev_anim_ptr[p] == (uintptr_t)model) { matched = 1; break; }
        same += matched;
        if (use_cache && model && !door_moving && r95_cache_pose(anims[i].bitset, r16_live_door(anims[i].bitset), live_loc_flags(anims[i].bitset)&R16_LIVE_ANIM, model->r10_animated))
            emit_cached(model, draw_yaw, draw_x, anims[i].y, draw_z, anims[i].bitset, 1, &acc_anim, &acc_anim_faces);
        else {
            uint64_t one = ba_timing_now();
#if defined(GL11) && defined(__vita__)
            int pole_layers=r66_pole_type(((unsigned)anims[i].bitset>>14)&32767)||r87_vendor_type(((unsigned)anims[i].bitset>>14)&32767)||r90_sewing_table_type(((unsigned)anims[i].bitset>>14)&32767);
            int pole_v0=vertcount,pole_e0=elementcount;
#endif
            emit_placed(model, draw_yaw, draw_x, anims[i].y, draw_z, anims[i].bitset);
#if defined(GL11) && defined(__vita__)
            if(pole_layers&&!overflow){r57_prepare_span(model,pole_v0,pole_e0);r57_fallback_span();}
#endif
            uint64_t us = ba_timing_now() - one;
            acc_anim += us;
            if (model && model->face_count > 0) acc_anim_faces += model->face_count;
            dyn_consider(1, model, us);
        }
#if defined(GL11) && defined(__vita__)
        live_door_floor_context=0;live_door_floor_level=-1;
#endif
    }
    acc_anim_n += anim_count;
    acc_same += same;
    acc_anim_ptrs += anim_count;
    prev_anim_n = anim_count < 64 ? anim_count : 64;
    for (int i = 0; i < prev_anim_n; i++) prev_anim_ptr[i] = (uintptr_t)anims[i].model;
    for (int i = 0; i < item_count && !overflow; i++) {
        Model *model = items[i].model;
        if (use_cache && model)
            emit_cached(model, 0, items[i].x, items[i].y, items[i].z, items[i].bitset, 2, &acc_item, &acc_item_faces);
        else {
            uint64_t one = ba_timing_now();
            emit_placed(model, 0, items[i].x, items[i].y, items[i].z, items[i].bitset);
            uint64_t us = ba_timing_now() - one;
            acc_item += us;
            if (model && model->face_count > 0) acc_item_faces += model->face_count;
            dyn_consider(2, model, us);
        }
    }
    acc_item_n += item_count;
    for (int i = 0; i < world->temporaryLocCount && !overflow; i++) {
        Location *loc = world->temporaryLocs[i];
        if (!loc) continue;
        Model *model = loc->model;
        int free_model = 0;
        if (!model && loc->entity) {
            uint64_t one = ba_timing_now();
            model = entity_draw(loc->entity, loop_cycle);
            uint64_t us = ba_timing_now() - one;
            acc_entity += us;
            if (model && model->face_count > 0) acc_entity_faces += model->face_count;
            dyn_consider(4, model, us);
            free_model = 1;
        }
        int draw_y=loc->y+player_ground_lift(world,loc,model,loop_cycle)+r101_bloodworm_lift(world,loc,model);
        actor_contact_probe(world,loc,model,loop_cycle,draw_y-loc->y);
        if (model && _Model.check_hover) pick_one(model, loc->yaw, loc->x, draw_y, loc->z, loc->bitset);
        if (model) {
#if defined(GL11) && defined(__vita__)
            int sig = 0;
            int skip_emit = 0;
            /* Location is calloc'd and freed every frame, so it can never hit.
             * The entity pointer stays. A model-only loc uses its model pointer. */
            void *owner = loc->entity ? (void *)loc->entity : (void *)model;
            if (use_cache == 1) {
                sig = model_sig(model) ^ entity_anim_sig(loc->entity);
                if (!model_in_view(model, loc->x - _World3D.eyeX, draw_y - _World3D.eyeY, loc->z - _World3D.eyeZ)) skip_emit = 1;
                else {
                    DynGpu *slot = dyn_find_owner(owner, loc->x, draw_y, loc->z, loc->yaw, sig);
                    if (slot) {
                        if (!dyn_already_queued(slot)) dyn_queue(slot);
                        acc_hits++;
                        if (model->face_count > 0) acc_loc_faces += model->face_count;
                        acc_loc_n++;
                        skip_emit = 1;
                    }
                }
            }
            if (!skip_emit) {
                uint64_t one = ba_timing_now();
                int v0 = vertcount, e0 = elementcount;
                gl_actor_span_begin();
                emit_placed(model, loc->yaw, loc->x, draw_y, loc->z, loc->bitset);
                r101_bloodworm_emit_probe(loc,model,loop_cycle,draw_y,vertcount-v0,elementcount-e0);
                uint64_t actor_emitted=ba_timing_now();acc_actor_emit+=actor_emitted-one;
                if(model->textured_face_count>0)acc_actor_textured++;else acc_actor_plain++;
                if(gl_actor_span_end())r57_prepare_indexed_span(model,v0,e0);else r57_prepare_span(model,v0,e0);
                acc_actor_layers+=ba_timing_now()-actor_emitted;
                int promoted=0;
                uint64_t us = ba_timing_now() - one;
                acc_loc += us;
                if (model->face_count > 0) acc_loc_faces += model->face_count;
                dyn_consider(3, model, us);
                acc_loc_n++;
                if (use_cache == 1 && !overflow) {
                    int cls = dyn_owner_class(owner, loc->x, draw_y, loc->z, loc->yaw);
                    if (cls == 1) acc_miss_sig++;
                    else if (cls == 2) acc_miss_pose++;
                    else acc_miss_new++;
                    acc_misses++;
                    if(r58_can_promote()){
                        DynGpu *slot = dyn_alloc_owner(owner, loc->x, draw_y, loc->z, loc->yaw);
                        if (!slot) slot = dyn_alloc(NULL, loc->x, draw_y, loc->z, loc->yaw);
                        if (slot && dyn_promote(slot, model, loc->x, draw_y, loc->z, loc->yaw, sig, v0, e0, owner, loc->bitset, 1)){dyn_queue(slot);promoted=1;}
                    }
                }
                if(!promoted&&!overflow)r57_fallback_span();
            }
#else
            uint64_t one = ba_timing_now();
            emit_placed(model, loc->yaw, loc->x, draw_y, loc->z, loc->bitset);
            uint64_t us = ba_timing_now() - one;
            acc_loc += us;
            if (model->face_count > 0) acc_loc_faces += model->face_count;
            dyn_consider(3, model, us);
            acc_loc_n++;
#endif
        }
        if (free_model) entity_draw_free(loc->entity, model, loop_cycle);
    }
}

#if defined(GL11) && defined(__vita__)
void r16_draw_dynamic(void) {
    r62_draw();
    if (dyn_draw_n <= 0) return;
    for (int i = 0; i < dyn_draw_n; i++) {
        DynGpu *slot = dyn_drawq[i];
        if (!slot || !slot->ready || slot->count <= 0) continue;
        glBindBuffer(GL_ARRAY_BUFFER, slot->vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, slot->ibo);
        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Vertex), (const void *)offsetof(Vertex, r));
        glVertexPointer(3, GL_FLOAT, sizeof(Vertex), (const void *)offsetof(Vertex, x));
        glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex), (const void *)offsetof(Vertex, u));
        glDrawElements(GL_TRIANGLES, slot->count, GL_UNSIGNED_SHORT, (const void *)0);
        acc_cached_draw++;
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}
void r16_draw_body_layers(void){
    if(!frame_ready)return;
    glDepthMask(GL_FALSE);glEnable(GL_POLYGON_OFFSET_FILL);
    for(int i=0;i<dyn_draw_n;i++){
        DynGpu*slot=dyn_drawq[i];if(!slot||!slot->ready)continue;
        int extra=0;for(int layer=1;layer<=9;layer++)extra+=slot->body_counts[layer];if(!extra)continue;
        glBindBuffer(GL_ARRAY_BUFFER,slot->vbo);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,slot->ibo);
        glColorPointer(4,GL_UNSIGNED_BYTE,sizeof(Vertex),(const void*)offsetof(Vertex,r));glVertexPointer(3,GL_FLOAT,sizeof(Vertex),(const void*)offsetof(Vertex,x));glTexCoordPointer(2,GL_FLOAT,sizeof(Vertex),(const void*)offsetof(Vertex,u));
        int offset=slot->count;for(int layer=1;layer<=9;layer++){int n=slot->body_counts[layer];if(n){glPolygonOffset(-(float)layer,-(float)layer);glDrawElements(GL_TRIANGLES,n,GL_UNSIGNED_SHORT,(const void*)(size_t)(offset*sizeof(uint16_t)));r57_drawn+=n/3;}offset+=n;}
    }
    r57_draw_fallback();
    glBindBuffer(GL_ARRAY_BUFFER,0);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);glDisable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(0,0);glDepthMask(GL_TRUE);
}
#else
void r16_draw_dynamic(void) {}
void r16_draw_body_layers(void) {}
#endif

static void dyn_reset_tops(void) {
    dyn_top_n = 0;
    memset(dyn_top, 0, sizeof dyn_top);
}

void r16_dyn_flush(void) {
    static uint64_t due;
    static FILE *out;
    static int header;
    uint64_t now = ba_timing_now();
    if (!due) due = now + 1000000ull;
    if (now < due || acc_frames <= 0) return;
    due = now + 1000000ull;
    if(acc_anim_view_tested){char line[144];snprintf(line,sizeof line,"# animated_view frames %d tested %u skipped %u",acc_frames,acc_anim_view_tested,acc_anim_view_skipped);ba_cadence_diagnostic(line);}
    acc_anim_view_tested=acc_anim_view_skipped=0;
    {unsigned status[5];gl_actor_cache_status(status);char line[192];snprintf(line,sizeof line,"# actor_indexed hits %u misses %u bytes %u saved_vertices %u exact_compare_test %u",status[0],status[1],status[2],status[3],status[4]);ba_cadence_diagnostic(line);}
    {unsigned status[3];r77_body_cache_status(status);char line[160];snprintf(line,sizeof line,"# body_selection_cache hits %u misses %u bytes %u",status[0],status[1],status[2]);ba_cadence_diagnostic(line);}
#if defined(GL11) && defined(__vita__)
    {unsigned meshes=0,faces=0,silk=0,spice=0;for(int i=0;i<R16_DYN_SLOTS;i++){DynGpu*p=dyn_gpu+i;int id=((unsigned)p->loc_tag>>14)&32767;if(!p->ready||!dyn_already_queued(p)||!r87_vendor_type(id))continue;unsigned count=0;for(int k=1;k<=9;k++)count+=(unsigned)p->body_counts[k]/3;meshes++;faces+=count;if(id==629||id==2560)silk+=count;if(id==633||id==2564)spice+=count;}char line[160];snprintf(line,sizeof line,"# vendor_layers active_meshes %u replay_faces %u silk %u spice %u",meshes,faces,silk,spice);ba_cadence_diagnostic(line);}
#endif
    {char line[320];snprintf(line,sizeof line,"# dynamic_cost frames %d pick %llu refresh %llu total %llu water %llu anim %llu item %llu loc %llu entity %llu anim_n %d loc_n %d",acc_frames,(unsigned long long)acc_pick,(unsigned long long)acc_refresh,(unsigned long long)acc_dynamic,(unsigned long long)acc_water,(unsigned long long)acc_anim,(unsigned long long)acc_item,(unsigned long long)acc_loc,(unsigned long long)acc_entity,acc_anim_n,acc_loc_n);ba_cadence_diagnostic(line);}
    {char line[192];snprintf(line,sizeof line,"# actor_cost frames %d emit %llu layers %llu plain %u textured_metadata %u",acc_frames,(unsigned long long)acc_actor_emit,(unsigned long long)acc_actor_layers,acc_actor_plain,acc_actor_textured);ba_cadence_diagnostic(line);}
    for(int i=0;i<dyn_top_n;i++){DynHit*h=&dyn_top[i];char line[160];snprintf(line,sizeof line,"# dynamic_peak kind %d faces %d vertices %d textured %d us %u",h->kind,h->faces,h->verts,h->tex,h->us);ba_cadence_diagnostic(line);}
    acc_actor_emit=acc_actor_layers=0;acc_actor_plain=acc_actor_textured=0;

    if (r16_diag_verbose()) {
    uint64_t began = ba_timing_now();
    if (!out) {
        out = fopen("ux0:/data/RS2Vita/dyn-phases.txt", "a");
        if (out) setvbuf(out, NULL, _IOFBF, 4096);
    }
    if (!out) return;
    if (!header) {
        fputs("one second of dynamic emission. times are sums of microseconds. top is kind,faces,verts,textured,us for the slowest calls. kind 1 anim 2 item 3 temporary-loc emit 4 entity build. miss_new/miss_sig/miss_pose split loc misses: first sight, same pose with a new mesh or animation frame, or the same entity moved. io is calls/sum/max for hold,radius,bake,fpslog,gpumem,phaselog,hudup,dynlog.\n", out);
        header = 1;
    }
    fprintf(out, "frames=%d cache=%d pick=%llu refresh=%llu dynamic=%llu water=%llu/%d anim=%llu n=%d faces=%d same=%d/%d item=%llu n=%d faces=%d loc=%llu n=%d faces=%d entity=%llu faces=%d hits=%d misses=%d drawn=%d miss_new=%d miss_sig=%d miss_pose=%d",
        acc_frames, dyn_cache_enabled(),
        (unsigned long long)acc_pick, (unsigned long long)acc_refresh, (unsigned long long)acc_dynamic,
        (unsigned long long)acc_water, acc_water_n,
        (unsigned long long)acc_anim, acc_anim_n, acc_anim_faces, acc_same, acc_anim_ptrs,
        (unsigned long long)acc_item, acc_item_n, acc_item_faces,
        (unsigned long long)acc_loc, acc_loc_n, acc_loc_faces,
        (unsigned long long)acc_entity, acc_entity_faces,
        acc_hits, acc_misses, acc_cached_draw, acc_miss_new, acc_miss_sig, acc_miss_pose);
    r16_io_add(7, ba_timing_now() - began);
    fputs(" top=", out);
    for (int i = 0; i < dyn_top_n; i++)
        fprintf(out, "%s%d,%d,%d,%d,%u", i ? ";" : "", dyn_top[i].kind, dyn_top[i].faces, dyn_top[i].verts, dyn_top[i].tex, dyn_top[i].us);
    fputs(" io=", out);
    for (int i = 0; i < 8; i++)
        fprintf(out, "%s%u/%llu/%llu", i ? ";" : "", io_n[i], (unsigned long long)io_us[i], (unsigned long long)io_max[i]);
    fputc('\n', out);
    fflush(out);
    }
    acc_water = acc_anim = acc_item = acc_loc = acc_entity = acc_pick = acc_refresh = acc_dynamic = 0;
    acc_frames = acc_water_n = acc_anim_n = acc_item_n = acc_loc_n = 0;
    acc_anim_faces = acc_item_faces = acc_loc_faces = acc_entity_faces = acc_same = acc_anim_ptrs = 0;
    acc_hits = acc_misses = acc_cached_draw = 0;
    acc_miss_new = acc_miss_sig = acc_miss_pose = 0;
    memset(io_us, 0, sizeof io_us);
    memset(io_max, 0, sizeof io_max);
    memset(io_n, 0, sizeof io_n);
    dyn_reset_tops();
}

#ifdef GL11
static int upload_failed;

static void release_chunks(void) {
#if defined(__vita__)
    r62_release();
#endif
#ifdef __vita__
    r26_release();
    if(floorcap_vbo)glDeleteBuffers(1,&floorcap_vbo);if(floorcap_ibo)glDeleteBuffers(1,&floorcap_ibo);
    floorcap_vbo=floorcap_ibo=0;floorcap_count=0;generic_layer_triangles=layer_dropped=0;
    if (trim_vbo) glDeleteBuffers(1,&trim_vbo);
    if (trim_ibo) glDeleteBuffers(1,&trim_ibo);
    trim_vbo = trim_ibo = 0;
    if (clock_vbo) glDeleteBuffers(1,&clock_vbo);
    if (clock_ibo) glDeleteBuffers(1,&clock_ibo);
    clock_vbo = clock_ibo = 0;
    clock_count = clock_face_triangles = window_layer_triangles = window_layer_dropped = 0;
    trim_count = 0;
    rug_layer_triangles = dresser_back_triangles = shutter_layer_triangles = shutter_attached = 0;
#endif
#ifdef __vita__
    for (int i = 0; i < chunk_n; i++) {
        if (chunks[i].vbo) glDeleteBuffers(1, &chunks[i].vbo);
        if (chunks[i].ibo) glDeleteBuffers(1, &chunks[i].ibo);
        chunks[i].vbo = 0;
        chunks[i].ibo = 0;
        chunks[i].count = 0;
        chunks[i].paving = 0;
        chunks[i].floor = chunks[i].floor_paving = 0;
        chunks[i].blend = 0;
    }
#else
    free(cpu_verts);
    free(cpu_indices);
    cpu_verts = NULL;
    cpu_indices = NULL;
    cpu_vert_n = 0;
#endif
    chunk_n = 0;
    static_count = 0;
    static_verts = 0;
}

#ifdef __vita__
#include "r16_roof_cache.inc"
#endif

static int flush_chunk(void) {
    R39UploadCost upload_cost __attribute__((cleanup(r39_upload_end)))={model_profile_enabled>=2?ba_timing_now():0};
    if (vertcount <= 0 || elementcount <= 0) {
        vertcount = 0;
        elementcount = 0;
        return 1;
    }
    if (chunk_n >= R16_MAX_CHUNKS) return 0;
#ifdef __vita__
    GLuint vbo = 0, ibo = 0;
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ibo);
    if (!vbo || !ibo) {
        if (vbo) glDeleteBuffers(1, &vbo);
        if (ibo) glDeleteBuffers(1, &ibo);
        upload_failed = 1;
        return 0;
    }
    for (int i = 0; i < 8 && glGetError() != GL_NO_ERROR; i++) {}
    if (vertcount > 65535 || elementcount > R16_CHUNK_VERTS) {
        glDeleteBuffers(1, &vbo);
        glDeleteBuffers(1, &ibo);
        upload_failed = 1;
        return 0;
    }
    /* RS source alpha 0 is opaque and becomes GL alpha 255. Any lower GL
     * alpha is model-face transparency (source 208 becomes 47) and must be
     * drawn after the opaque faces. Texture cutout keeps GL alpha 255. */
    int groups[5]={0};
    if(!r46_partition_floors(verts,vertcount,indices,elementcount,paving_tags,floor_tags,short_indices,R16_CHUNK_VERTS,groups)){
        glDeleteBuffers(1,&vbo);glDeleteBuffers(1,&ibo);upload_failed=1;return 0;
    }
    if(r32_focus_ready&&!r32_vertex_triangle_tags(short_indices,elementcount,r32_vertex_tags,vertcount)){
        glDeleteBuffers(1,&vbo);glDeleteBuffers(1,&ibo);upload_failed=1;return 0;
    }
    glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,(GLsizei)((size_t)vertcount*sizeof(Vertex)),verts,GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizei)((size_t)elementcount * sizeof(uint16_t)), short_indices, GL_STATIC_DRAW);
    GLenum err = glGetError();
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    if (err != GL_NO_ERROR) {
        glDeleteBuffers(1, &vbo);
        glDeleteBuffers(1, &ibo);
        upload_failed = 1;
        return 0;
    }
    int opaque_n=groups[0],paving_n=groups[1],floor_n=groups[2],floor_paving_n=groups[3],blend_n=groups[4];
    int roof_groups[]={opaque_n,paving_n,floor_n,floor_paving_n,blend_n};
    if(r32_focus_ready&&!r32_register_indices(ibo,short_indices,r32_sorted_tags,elementcount,5,roof_groups)){
        glDeleteBuffers(1,&vbo);glDeleteBuffers(1,&ibo);upload_failed=1;return 0;
    }
    chunks[chunk_n].vbo = vbo;
    chunks[chunk_n].ibo = ibo;
    chunks[chunk_n].count = opaque_n;
    chunks[chunk_n].paving = paving_n;
    chunks[chunk_n].floor = floor_n;
    chunks[chunk_n].floor_paving = floor_paving_n;
    chunks[chunk_n].blend = blend_n;
#else
    size_t base = (size_t)cpu_vert_n;
    Vertex *nv = realloc(cpu_verts, (base + (size_t)vertcount) * sizeof(Vertex));
    uint32_t *ni = realloc(cpu_indices, ((size_t)static_count + (size_t)elementcount) * sizeof(uint32_t));
    if (!nv || !ni) {
        if (nv) cpu_verts = nv;
        if (ni) cpu_indices = ni;
        upload_failed = 1;
        return 0;
    }
    cpu_verts = nv;
    cpu_indices = ni;
    memcpy(cpu_verts + base, verts, (size_t)vertcount * sizeof(Vertex));
    for (int i = 0; i < elementcount; i++) cpu_indices[static_count + i] = indices[i] + (uint32_t)base;
    cpu_vert_n += vertcount;
#endif
#ifdef __vita__
    memset(paving_tags,0,sizeof paving_tags);
    memset(floor_tags,0,sizeof floor_tags);
#endif
    static_count += elementcount;
    static_verts += vertcount;
    chunk_n++;
    vertcount = 0;
    elementcount = 0;
    return 1;
}

void r16_draw_static(void) {
    if (!frame_static || static_count <= 0) return;
#ifdef __vita__
    for (int i = 0; i < chunk_n; i++) {
        if (chunks[i].count <= 0 && chunks[i].paving <= 0 && chunks[i].floor <= 0 && chunks[i].floor_paving <= 0) continue;
        glBindBuffer(GL_ARRAY_BUFFER, chunks[i].vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, chunks[i].ibo);
        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Vertex), (const void *)offsetof(Vertex, r));
        glVertexPointer(3, GL_FLOAT, sizeof(Vertex), (const void *)offsetof(Vertex, x));
        glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex), (const void *)offsetof(Vertex, u));
        if(chunks[i].count)glDrawElements(GL_TRIANGLES, chunks[i].count, GL_UNSIGNED_SHORT, (const void *)0);
        int offset=chunks[i].count;
        GLboolean floor_cull=GL_FALSE;
        for(int pass=0;pass<3;pass++){
            int n=pass==0?chunks[i].paving:pass==1?chunks[i].floor:chunks[i].floor_paving;
            if(pass==1&&(chunks[i].floor||chunks[i].floor_paving)){floor_cull=glIsEnabled(GL_CULL_FACE);glDisable(GL_CULL_FACE);}
            if(!n)continue;
            GLuint filtered=pass==1?0:gl_paving_texture();
            if(filtered){
                glBindTexture(GL_TEXTURE_2D,filtered);
                glMatrixMode(GL_TEXTURE);glPushMatrix();glLoadIdentity();
                glTranslatef(-(R20_PAVING_TEXTURE+1.f),0.f,0.f);glScalef(R20_ATLAS_SLOTS,1.f,1.f);
                glMatrixMode(GL_MODELVIEW);
            }
            glDrawElements(GL_TRIANGLES,n,GL_UNSIGNED_SHORT,(const void*)(size_t)(offset*sizeof(uint16_t)));
            if(filtered){glMatrixMode(GL_TEXTURE);glPopMatrix();glMatrixMode(GL_MODELVIEW);glBindTexture(GL_TEXTURE_2D,texture_atlas);}
            offset+=n;
        }
        if(floor_cull)glEnable(GL_CULL_FACE);
    }
    if(floorcap_count&&floorcap_vbo&&floorcap_ibo){
        glBindBuffer(GL_ARRAY_BUFFER,floorcap_vbo);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,floorcap_ibo);
        glColorPointer(4,GL_UNSIGNED_BYTE,sizeof(Vertex),(const void*)offsetof(Vertex,r));
        glVertexPointer(3,GL_FLOAT,sizeof(Vertex),(const void*)offsetof(Vertex,x));
        glTexCoordPointer(2,GL_FLOAT,sizeof(Vertex),(const void*)offsetof(Vertex,u));
        glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(1.f,1.f);
        glDrawElements(GL_TRIANGLES,floorcap_count,GL_UNSIGNED_SHORT,(const void*)0);
        glDisable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(0.f,0.f);
    }
    if (trim_count && trim_vbo && trim_ibo) {
        glBindBuffer(GL_ARRAY_BUFFER,trim_vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,trim_ibo);
        glColorPointer(4,GL_UNSIGNED_BYTE,sizeof(Vertex),(const void *)offsetof(Vertex,r));
        glVertexPointer(3,GL_FLOAT,sizeof(Vertex),(const void *)offsetof(Vertex,x));
        glTexCoordPointer(2,GL_FLOAT,sizeof(Vertex),(const void *)offsetof(Vertex,u));
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(-1.0f,-1.0f);
        glDrawElements(GL_TRIANGLES,trim_count,GL_UNSIGNED_SHORT,(const void *)0);
        glDisable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(0.0f,0.0f);
    }
    if (clock_count && clock_vbo && clock_ibo) {
        glBindBuffer(GL_ARRAY_BUFFER,clock_vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,clock_ibo);
        glColorPointer(4,GL_UNSIGNED_BYTE,sizeof(Vertex),(const void *)offsetof(Vertex,r));
        glVertexPointer(3,GL_FLOAT,sizeof(Vertex),(const void *)offsetof(Vertex,x));
        glTexCoordPointer(2,GL_FLOAT,sizeof(Vertex),(const void *)offsetof(Vertex,u));
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(-2.0f,-2.0f);
        glDrawElements(GL_TRIANGLES,clock_count,GL_UNSIGNED_SHORT,(const void *)0);
        glDisable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(0.0f,0.0f);
    }
    r26_draw();
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
#else
    if (!cpu_verts || !cpu_indices) return;
    glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Vertex), &cpu_verts[0].r);
    glVertexPointer(3, GL_FLOAT, sizeof(Vertex), &cpu_verts[0].x);
    glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex), &cpu_verts[0].u);
    glDrawElements(GL_TRIANGLES, static_count, GL_UNSIGNED_INT, cpu_indices);
#endif
}

void r16_draw_static_alpha(void) {
    if (!frame_static || static_count <= 0) return;
#ifdef __vita__
    int any = 0;
    for (int i = 0; i < chunk_n; i++) {
        if (chunks[i].blend > 0) any = 1;
    }
    if (!any) return;
    /* One depth unit behind the coplanar post. vitaGL applies this bias to
     * front faces only, which is the side that survives the cull. */
    glDepthMask(GL_FALSE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);
    for (int i = 0; i < chunk_n; i++) {
        if (chunks[i].blend <= 0) continue;
        glBindBuffer(GL_ARRAY_BUFFER, chunks[i].vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, chunks[i].ibo);
        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Vertex), (const void *)offsetof(Vertex, r));
        glVertexPointer(3, GL_FLOAT, sizeof(Vertex), (const void *)offsetof(Vertex, x));
        glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex), (const void *)offsetof(Vertex, u));
        glDrawElements(GL_TRIANGLES, chunks[i].blend, GL_UNSIGNED_SHORT, (const void *)(size_t)((chunks[i].count+chunks[i].paving+chunks[i].floor+chunks[i].floor_paving) * (int)sizeof(uint16_t)));
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(0.0f, 0.0f);
    glDepthMask(GL_TRUE);
#endif
}
#endif

void r16_draw_water(void) {
#if defined(GL11) && defined(__vita__)
    if (!frame_static || water_gpu_n <= 0) return;
    float offset = r106_water_offset(rs2_now());
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glTranslatef(0.0f, offset, 0.0f);
    for (int i = 0; i < water_gpu_n; i++) {
        if (water_gpu[i].count <= 0) continue;
        glBindBuffer(GL_ARRAY_BUFFER, water_gpu[i].vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, water_gpu[i].ibo);
        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Vertex), (const void *)offsetof(Vertex, r));
        glVertexPointer(3, GL_FLOAT, sizeof(Vertex), (const void *)offsetof(Vertex, x));
        glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex), (const void *)offsetof(Vertex, u));
        glDrawElements(GL_TRIANGLES, water_gpu[i].count, GL_UNSIGNED_SHORT, (const void *)0);
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
#else
    (void)0;
#endif
}

/* Bound the geometry the static emitter can actually retain. Doors, trees,
 * resources and animated fixtures use the live pass, so counting them here
 * can incorrectly reject a complete scene and force repeated walking bakes.
 * Hidden faces remain counted conservatively; actual caps/uploads keep their
 * existing budgets and the 24 MiB graphics headroom is unchanged. */
static uint64_t retained_model_vertices(Model*m,int bitset){
 return m&&!m->r10_animated&&m->face_count>0&&!r16_live_location(bitset)?(uint64_t)m->face_count*3u:0;
}
static uint64_t retained_tile_vertices(Ground*t,int x,int z,int bridge){
 uint64_t n=0;if(!t)return 0;
 if(t->underlay)n+=6;else if(t->overlay)n+=(uint64_t)t->overlay->triangleCount*3u;
 if(t->wall)n+=retained_model_vertices(t->wall->modelA,t->wall->bitset)+retained_model_vertices(t->wall->modelB,t->wall->bitset);
 /* emit_tile draws a bridge's floor, walls and locations, not its decoration. */
 if(!bridge&&t->decor){
  uint64_t vertices=retained_model_vertices(t->decor->model,t->decor->bitset);
  n+=vertices;
  if((t->decor->type&0x300)==0x300)n+=vertices;
 }
 if(!bridge&&t->groundDecor)n+=retained_model_vertices(t->groundDecor->model,t->groundDecor->bitset);
 for(int i=0;i<t->locCount;i++){Location*l=t->locs[i];if(l&&!l->entity&&!l->r10_temporary&&l->minSceneTileX==x&&l->minSceneTileZ==z)n+=retained_model_vertices(l->model,l->bitset);}
 return n;
}
static uint64_t retained_scene_vertices(World3D*w,int top){
 uint64_t n=0;for(int l=w->minLevel;l<w->maxLevel;l++)for(int x=0;x<w->maxTileX;x++)for(int z=0;z<w->maxTileZ;z++){
  Ground*t=w->levelTiles[l][x][z];if(!t||t->drawLevel>top)continue;
  n+=retained_tile_vertices(t,x,z,0);if(t->bridge)n+=retained_tile_vertices(t->bridge,x,z,1);
 }return n;
}

static int bake_square(World3D *world, int top_level) {
#ifdef __vita__
    memset(paving_tags,0,sizeof paving_tags);
    memset(floor_tags,0,sizeof floor_tags);
    compact_source_vertices=compact_gpu_vertices=0;compact_max_probe=0;r35_reset();
#endif
    bake_model_profile_init();
    R16BakeCost bake_cost __attribute__((cleanup(bake_cost_end)))={model_profile_enabled?ba_timing_now():0,top_level};
    r40_skip_models=r40_scan_models=r40_skip_vertices=0;r39_release_us=r39_loop_us=r39_aux_us=r39_upload_us=0;r39_upload_calls=r39_used=r39_sequence=0;memset(r39_models,0,sizeof r39_models);profile_model=NULL;
    uint64_t stage_at=ba_timing_now();
    memset(bake_slow_models,0,sizeof bake_slow_models);bake_models_us=0;bake_models_n=0;
    memset(bake_phase_us,0,sizeof bake_phase_us);
    pick_count = anim_count = item_count = water_count = dyn_count = 0;
    water_release();
    overflow = 0;
    upload_failed = 0;
    release_chunks();
    vertcount = 0;
    elementcount = 0;
#ifdef __vita__
    r51_prepare(world);r52_clipped_models=0;r53_replayed=0;r54_duplicates=0;r55_contacts=0;r56_replayed=0;r69_roof_faces=0;r110_begin();r70_plant_contacts=0;r71_begin_bake();r78_fur_contacts=0;
    geometry_diag=0;trim_top_level=top_level;r111_prepare(world,top_level);r112_prepare(world);wall_floor_triangles=wall_floor_partial=wall_floor_dropped=0;
    FILE*flag=fopen("ux0:/data/RS2Vita/wood-diagnostic.txt","r");
    if(flag){fscanf(flag,"%d",&geometry_diag);fclose(flag);}
    if(geometry_diag==3){
        int nearest=INT32_MAX;geometry_focus_x=anchor_x;geometry_focus_z=anchor_z;
        for(int l=world->minLevel;l<world->maxLevel;l++)for(int x=0;x<world->maxTileX;x++)for(int z=0;z<world->maxTileZ;z++){
            Ground*t=world->levelTiles[l][x][z];if(!t)continue;
            for(int j=0;j<t->locCount;j++){Location*loc=t->locs[j];if(!loc)continue;int id=((unsigned)loc->bitset>>14)&32767;if(id!=380&&id!=381)continue;
                int dx=x-anchor_x,dz=z-anchor_z,d=dx*dx+dz*dz;if(d<nearest){nearest=d;geometry_focus_x=x;geometry_focus_z=z;}}
        }
    }
    if(geometry_diag){
        const char*mode=geometry_dumped?"ab":"wb";
        geometry_file=fopen("ux0:/data/RS2Vita/wood-geometry.bin",mode);
        if(geometry_diag==2||geometry_diag==5){fit_file=fopen("ux0:/data/RS2Vita/window-fit.csv",mode);bridge_file=fopen("ux0:/data/RS2Vita/bridge-height.csv",mode);floor_file=fopen("ux0:/data/RS2Vita/floor-evidence.csv",mode);}
        if(geometry_diag==4||geometry_diag==6)terrain_file=fopen("ux0:/data/RS2Vita/courtyard-floor.bin",mode);
        geometry_dumped=1;
    }
#endif
    r39_release_us=ba_timing_now()-stage_at;stage_at=ba_timing_now();
    baking = 1;
    emitting = 1;
    for (int level = world->minLevel; level < world->maxLevel && !overflow; level++) {
        for (int x = _World3D.minDrawTileX; x < _World3D.maxDrawTileX && !overflow; x++) {
            for (int z = _World3D.minDrawTileZ; z < _World3D.maxDrawTileZ; z++) {
                Ground *tile = world->levelTiles[level][x][z];
                if (!tile || tile->drawLevel > top_level) continue;
                emit_tile(world, tile, level, x, z);
                if (overflow) break;
            }
        }
    }
#ifdef __vita__
    if(geometry_file)fclose(geometry_file);if(fit_file)fclose(fit_file);if(bridge_file)fclose(bridge_file);if(floor_file)fclose(floor_file);
    if(terrain_file)fclose(terrain_file);
    geometry_file=fit_file=bridge_file=floor_file=terrain_file=NULL;
#endif
    r39_loop_us=ba_timing_now()-stage_at;stage_at=ba_timing_now();
    baking = 0;
    emitting = 0;
    if (!overflow && !flush_chunk()) overflow = 1;
    if (!overflow && !water_flush()) overflow = 1;
#if defined(GL11) && defined(__vita__)
    if(!overflow&&floorcap_count){
        for(int i=0;i<floorcap_count;i++)floorcap_indices[i]=(uint16_t)i;
        glGenBuffers(1,&floorcap_vbo);glGenBuffers(1,&floorcap_ibo);
        glBindBuffer(GL_ARRAY_BUFFER,floorcap_vbo);glBufferData(GL_ARRAY_BUFFER,floorcap_count*sizeof(Vertex),floorcap_cpu,GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,floorcap_ibo);glBufferData(GL_ELEMENT_ARRAY_BUFFER,floorcap_count*sizeof(uint16_t),floorcap_indices,GL_STATIC_DRAW);
        GLenum error=glGetError();glBindBuffer(GL_ARRAY_BUFFER,0);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
        if(!floorcap_vbo||!floorcap_ibo||error!=GL_NO_ERROR){overflow=1;wall_floor_dropped+=wall_floor_partial;if(floorcap_vbo)glDeleteBuffers(1,&floorcap_vbo);if(floorcap_ibo)glDeleteBuffers(1,&floorcap_ibo);floorcap_vbo=floorcap_ibo=0;floorcap_count=0;}
    }
    if (!overflow && trim_count) {
        for (int i = 0; i < trim_count; i++) trim_indices[i] = (uint16_t)i;
        glGenBuffers(1,&trim_vbo);
        glGenBuffers(1,&trim_ibo);
        glBindBuffer(GL_ARRAY_BUFFER,trim_vbo);
        glBufferData(GL_ARRAY_BUFFER,trim_count*sizeof(Vertex),trim_cpu,GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,trim_ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,trim_count*sizeof(uint16_t),trim_indices,GL_STATIC_DRAW);
        GLenum error = glGetError();
        glBindBuffer(GL_ARRAY_BUFFER,0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
        if (!trim_vbo || !trim_ibo || error != GL_NO_ERROR) {
            if (trim_vbo) glDeleteBuffers(1,&trim_vbo);
            if (trim_ibo) glDeleteBuffers(1,&trim_ibo);
            trim_vbo = trim_ibo = 0;
            trim_count = 0;
        }
    }
    if (!overflow && clock_count) {
        for (int i = 0; i < clock_count; i++) clock_indices[i] = (uint16_t)i;
        glGenBuffers(1,&clock_vbo); glGenBuffers(1,&clock_ibo);
        glBindBuffer(GL_ARRAY_BUFFER,clock_vbo);
        glBufferData(GL_ARRAY_BUFFER,clock_count*sizeof(Vertex),clock_cpu,GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,clock_ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,clock_count*sizeof(uint16_t),clock_indices,GL_STATIC_DRAW);
        GLenum error = glGetError();
        glBindBuffer(GL_ARRAY_BUFFER,0); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
        if (!clock_vbo || !clock_ibo || error != GL_NO_ERROR) {
            if (clock_vbo) glDeleteBuffers(1,&clock_vbo);
            if (clock_ibo) glDeleteBuffers(1,&clock_ibo);
            clock_vbo = clock_ibo = 0; clock_count = 0;
        }
    }
    if(!overflow&&floorcap_count&&!r32_register_detail(floorcap_ibo,floorcap_indices,floorcap_count,1,&floorcap_count,r32_floorcap_tags))overflow=1;
    if(!overflow&&trim_count&&!r32_register_detail(trim_ibo,trim_indices,trim_count,1,&trim_count,r32_trim_tags))overflow=1;
    if(!overflow&&clock_count&&!r32_register_detail(clock_ibo,clock_indices,clock_count,1,&clock_count,r32_clock_tags))overflow=1;
    r26_upload();r39_aux_us=ba_timing_now()-stage_at;
    if(!overflow){char compact_line[224];snprintf(compact_line,sizeof compact_line,"# renderer_compact %llu top %d source_vertices %llu gpu_vertices %llu chunks %d max_probe %u",(unsigned long long)ba_timing_now(),top_level,(unsigned long long)compact_source_vertices,(unsigned long long)compact_gpu_vertices,chunk_n,compact_max_probe);ba_cadence_diagnostic(compact_line);}
    {char pattern_line[160];snprintf(pattern_line,sizeof pattern_line,"# compact_patterns hits %llu misses %llu rejected %llu",(unsigned long long)r35_hits,(unsigned long long)r35_misses,(unsigned long long)r35_rejected);ba_cadence_diagnostic(pattern_line);}
    {char line[176];snprintf(line,sizeof line,"# water_scan_gate skipped_models %llu scanned_models %llu skipped_vertices %llu",(unsigned long long)r40_skip_models,(unsigned long long)r40_scan_models,(unsigned long long)r40_skip_vertices);ba_cadence_diagnostic(line);}
    {char line[96];snprintf(line,sizeof line,"# roof_level_layers replayed_triangles %u",r69_roof_faces);ba_cadence_diagnostic(line);}
    {char line[128];snprintf(line,sizeof line,"# roof_candidate_bounds queries %u rejected %u",r110_queries,r110_rejected);ba_cadence_diagnostic(line);}
    {char line[128];snprintf(line,sizeof line,"# floor_contact_bounds queries %u rejected %u",r111_queries,r111_rejected);ba_cadence_diagnostic(line);}
    {char line[160];snprintf(line,sizeof line,"# roof_plane_index nodes %d failed %d queries %u candidates %u",r112_count,r112_failed,r112_queries,r112_candidates);ba_cadence_diagnostic(line);}
    {char line[96];snprintf(line,sizeof line,"# fur_floor_contacts adjusted_instances %u",r78_fur_contacts);ba_cadence_diagnostic(line);}
    {char line[96];snprintf(line,sizeof line,"# plant_surface_contacts adjusted_instances %u",r70_plant_contacts);ba_cadence_diagnostic(line);}
    {char line[160];snprintf(line,sizeof line,"# shore_plant_heights adjusted_instances %u samples %u reused %u",r71_shore_instances,r71_height_queries,r71_height_hits);ba_cadence_diagnostic(line);}
    FILE *trim_log = fopen("ux0:/data/RS2Vita/window-trim.txt","w");
    if(trim_log){fprintf(trim_log,"primary_replay_triangles=%d rug_layer_triangles=%d dresser_back_triangles=%d shutter_layer_triangles=%d shutter_attached=%d clock_face_triangles=%d second_layer_triangles=%d window_layer_triangles=%d window_layer_dropped=%d wall_floor_triangles=%d wall_floor_partial=%d wall_floor_dropped=%d generic_layers=%d layer_dropped=%d vertices=%d\n",trim_count/3,rug_layer_triangles,dresser_back_triangles,shutter_layer_triangles,shutter_attached,clock_face_triangles,clock_count/3,window_layer_triangles,window_layer_dropped,wall_floor_triangles,wall_floor_partial,wall_floor_dropped,generic_layer_triangles,layer_dropped,trim_count+clock_count+floorcap_count);fclose(trim_log);}
#endif
    if (overflow) {
        int built = static_verts + vertcount;
        int n = chunk_n;
        char failure[256];
        snprintf(failure,sizeof failure,"# renderer_bake_rejected verts %d chunks %d upload_failed %d anims %d dyn %d water %d",built,n,upload_failed,anim_count,dyn_count,water_count);
        ba_cadence_diagnostic(failure);
        release_chunks();
        water_release();
        vertcount = 0;
        elementcount = 0;
        if (upload_failed)
            r16_write("R16 static upload failed at %d vertices across %d chunks.", built, n);
        else
            r16_write("R16 bake exceeded the static budget at %d vertices across %d chunks.", built, n);
        return 0;
    }
    return 1;
}

/* Animated scenery swaps its model every few ticks. Read those pointers again
 * instead of uploading the whole static mesh. */
/* Resolve door models from the current tiles. No slot/model pointer survives
 * a scene mutation, including a door moving onto its adjacent tile. */
static void collect_live_locations(Ground*tile,int level,int x,int z){
 if(!tile)return;
 int previous_source_level=collecting_source_level;collecting_source_level=level;
 Wall*w=tile->wall;
 if(w&&r16_live_location(w->bitset)){
  if(w->modelA&&!remember_anim(w->modelA,w->x,w->y,w->z,0,w->bitset))overflow=1;
  if(w->modelB&&!remember_anim(w->modelB,w->x,w->y,w->z,0,w->bitset))overflow=1;
 }
 Decor*d=tile->decor;
 if(d&&d->model&&r16_live_location(d->bitset)){
  R25DecorPlacement p[2];
  int n=r25_decor_placements(d->type,d->angle,d->x,d->z,p);
  for(int i=0;i<n;i++)if(!remember_anim(d->model,p[i].x,d->y,p[i].z,p[i].yaw,d->bitset))overflow=1;
 }

 GroundDecor*g=tile->groundDecor;
 if(g&&g->model&&r16_live_location(g->bitset)&&!remember_anim(g->model,g->x,g->y,g->z,0,g->bitset))overflow=1;
 for(int i=0;i<tile->locCount;i++){
  Location*l=tile->locs[i];
  if(l&&!l->r10_temporary&&l->minSceneTileX==x&&l->minSceneTileZ==z&&r16_live_location(l->bitset))
   if(l->model&&!remember_anim(l->model,l->x,l->y,l->z,l->yaw,l->bitset))overflow=1;
 }
 collecting_source_level=previous_source_level;
}

static void refresh_scene_dynamics(World3D *world, int top_level) {
    anim_count = 0;
    item_count = 0;
    collecting = 1;
    for (int i = 0; i < dyn_count; i++) {
        Model *model = dyns[i].slot ? *dyns[i].slot : NULL;
#if defined(GL11)&&defined(__vita__)
        if(r32_focus_ready&&R32_HIDDEN(dyns[i].roof_tag))continue;
#endif
        if (model) remember_anim(model, dyns[i].x, dyns[i].y, dyns[i].z, dyns[i].yaw, dyns[i].bitset);
    }
    if(live_tile_index(world,top_level)) {
        for(int i=0;i<live_dynamic_n;i++) {
            unsigned c=live_dynamic_tiles[i];int level=c>>16,x=(c>>8)&255,z=c&255;
            if(x<_World3D.minDrawTileX||x>=_World3D.maxDrawTileX||z<_World3D.minDrawTileZ||z>=_World3D.maxDrawTileZ)continue;
            Ground *tile=world->levelTiles[level][x][z];
            if(!tile||(!R32_ENABLED&&tile->drawLevel>top_level))continue;
            int visible=r32_tile_visible(tile->drawLevel,x,z,top_level);
            int bridge_visible=tile->bridge&&(!R32_ENABLED||r32_tile_visible(tile->bridge->drawLevel,x,z,top_level));
            if(visible)collect_live_locations(tile,tile->occludeLevel,x,z);
            if(bridge_visible)collect_live_locations(tile->bridge,tile->bridge->occludeLevel,x,z);
            if(bridge_visible&&tile->bridge->groundObj)emit_items(tile->bridge);
            if(visible&&tile->groundObj)emit_items(tile);
        }
        collecting=0;return;
    }
    for (int level = world->minLevel; level < world->maxLevel; level++) {
        for (int x = _World3D.minDrawTileX; x < _World3D.maxDrawTileX; x++) {
            for (int z = _World3D.minDrawTileZ; z < _World3D.maxDrawTileZ; z++) {
                Ground *tile = world->levelTiles[level][x][z];
                if (!tile || (!R32_ENABLED&&tile->drawLevel > top_level)) continue;
                int visible=r32_tile_visible(tile->drawLevel,x,z,top_level);
                int bridge_visible=tile->bridge&&(!R32_ENABLED||r32_tile_visible(tile->bridge->drawLevel,x,z,top_level));
                if(visible)collect_live_locations(tile,tile->occludeLevel,x,z);
                if(bridge_visible)collect_live_locations(tile->bridge,tile->bridge->occludeLevel,x,z);
                if(bridge_visible&&tile->bridge->groundObj)emit_items(tile->bridge);
                if(visible&&tile->groundObj)emit_items(tile);
            }
        }
    }
    collecting = 0;
}

int r16_prepare(void *world_ptr, int top_level, int loop_cycle) {
    World3D *world = world_ptr;
    frame_static = 0;
    frame_ready = 0;
    if (!r16_active() || !world || !world->levelTiles) return 0;
#ifdef GL11
#if defined(__vita__)
    if(r32_failed){fallback_reason=4;return 0;}
    if(r32_focus_ready)top_level=3;
    dyn_draw_n = 0;
#endif
    int eye_x = _World3D.eyeTileX;
    int eye_z = _World3D.eyeTileZ;
    int radius = world3d_draw_radius();
    int ax = anchor_x >= 0 ? anchor_x : eye_x;
    int az = anchor_z >= 0 ? anchor_z : eye_z;
    int orbit = iabs(eye_x - ax);
    int orbit_z = iabs(eye_z - az);
    if (orbit_z > orbit) orbit = orbit_z;
    /* The static vertices are world-space. The eye tile only chooses which
     * square was uploaded. A camera orbit stays about eight tiles from the
     * player, so a twelve-tile margin keeps a radius of 25 covered without
     * rebaking on yaw. Four extra tiles absorb a short walk. */
    int margin = orbit + 4;
    if (margin < 12) margin = 12;
    if (margin > 16) {
        ax = eye_x;
        az = eye_z;
        margin = 12;
    }
    int cover = radius + margin;
    if (cover > WORLD_VIS_RADIUS - 1) cover = WORLD_VIS_RADIUS - 1;
    int need_min_x = eye_x - radius;
    int need_min_z = eye_z - radius;
    int need_max_x = eye_x + radius;
    int need_max_z = eye_z + radius;
    if (need_min_x < 0) need_min_x = 0;
    if (need_min_z < 0) need_min_z = 0;
    if (need_max_x > world->maxTileX) need_max_x = world->maxTileX;
    if (need_max_z > world->maxTileZ) need_max_z = world->maxTileZ;
    int inside = key_ok && key_world == world && key_level == top_level && key_radius == radius && key_stamp == stamp && need_min_x >= key_min_x && need_max_x <= key_max_x && need_min_z >= key_min_z && need_max_z <= key_max_z;
#ifdef __vita__
    if(!inside&&roof_try(world,stamp,top_level,radius,need_min_x,need_min_z,need_max_x,need_max_z))inside=1;
#endif
    uint64_t pick_at = ba_timing_now();
    if (_World3D.takingInput || _Model.check_hover) {
        suppress = 1;
        vertcount = 0;
        elementcount = 0;
        if (_World3D.takingInput) pick_tiles(world);
        if (_Model.check_hover) pick_live(world, top_level);
        suppress = 0;
        vertcount = 0;
        elementcount = 0;
        _World3D.takingInput = false;
    }
    uint64_t pick_us = ba_timing_now() - pick_at;
    static unsigned held_frames;
    uint64_t began = ba_timing_now();
    if (!inside) {
        char rebuild_line[448];
        snprintf(rebuild_line,sizeof rebuild_line,"# renderer_rebuild %llu key %d world_same %d level %d/%d radius %d/%d stamp %d/%d dirty %u caller %lx cached %d %d %d %d need %d %d %d %d",(unsigned long long)ba_timing_now(),key_ok,key_world==world,key_level,top_level,key_radius,radius,key_stamp,stamp,dirty_events-committed_dirty_events,(unsigned long)last_dirty_caller,key_min_x,key_min_z,key_max_x,key_max_z,need_min_x,need_min_z,need_max_x,need_max_z);
        ba_cadence_diagnostic(rebuild_line);
        int saved_min_x = _World3D.minDrawTileX;
        int saved_min_z = _World3D.minDrawTileZ;
        int saved_max_x = _World3D.maxDrawTileX;
        int saved_max_z = _World3D.maxDrawTileZ;
        int min_x = ax - cover;
        int min_z = az - cover;
        int max_x = ax + cover;
        int max_z = az + cover;
        if (min_x < 0) min_x = 0;
        if (min_z < 0) min_z = 0;
        if (max_x > world->maxTileX) max_x = world->maxTileX;
        if (max_z > world->maxTileZ) max_z = world->maxTileZ;
#ifdef __vita__
        /* Keep the complete loaded scene when it fits, so running cannot
         * repeatedly invalidate a small player-centered upload. Maintain a
         * hard geometry cap and 24 MiB shared graphics headroom. */
        uint64_t whole=retained_scene_vertices(world,top_level);
        uint64_t bytes=whole*(sizeof(Vertex)+sizeof(uint16_t));
        uint64_t available=(uint64_t)vglMemFree(VGL_MEM_RAM)+vglMemFree(VGL_MEM_VRAM);
        if(whole<=(uint64_t)(R16_MAX_CHUNKS-2)*R16_CHUNK_VERTS&&bytes+24u*1024u*1024u<available){
            min_x=min_z=0;max_x=world->maxTileX;max_z=world->maxTileZ;
        }
#endif
#ifdef __vita__
        snprintf(rebuild_line,sizeof rebuild_line,"# renderer_coverage %llu whole_vertices %llu whole_bytes %llu available %llu chosen %d %d %d %d",(unsigned long long)ba_timing_now(),(unsigned long long)whole,(unsigned long long)bytes,(unsigned long long)available,min_x,min_z,max_x,max_z);
        ba_cadence_diagnostic(rebuild_line);
#endif
        _World3D.minDrawTileX = min_x;
        _World3D.minDrawTileZ = min_z;
        _World3D.maxDrawTileX = max_x;
        _World3D.maxDrawTileZ = max_z;
#ifdef __vita__
        int retain=key_ok&&key_world==world&&key_stamp==stamp&&key_radius==radius&&key_level!=top_level&&need_min_x>=key_min_x&&need_min_z>=key_min_z&&need_max_x<=key_max_x&&need_max_z<=key_max_z;
        if(retain)roof_stash_current();else roof_drop_alternate();
#endif
        int baked = bake_square(world, top_level);
        int baked_min_x = min_x, baked_min_z = min_z, baked_max_x = max_x, baked_max_z = max_z;
        if (!baked) {
            _World3D.minDrawTileX = saved_min_x;
            _World3D.minDrawTileZ = saved_min_z;
            _World3D.maxDrawTileX = saved_max_x;
            _World3D.maxDrawTileZ = saved_max_z;
            baked = bake_square(world, top_level);
            if (!baked) {
                budget_off = 1;
                fallback_reason=1;
                ba_cadence_diagnostic("# renderer_fallback static_bake");
                r16_write("R16 eye bake failed. Original renderer stays on.");
                return 0;
            }
            ax = eye_x;
            az = eye_z;
            cover = radius;
            baked_min_x = saved_min_x;
            baked_min_z = saved_min_z;
            baked_max_x = saved_max_x;
            baked_max_z = saved_max_z;
            r16_write("R16 wide bake did not fit. Kept the eye-centered radius %d.", radius);
        }
        _World3D.minDrawTileX = saved_min_x;
        _World3D.minDrawTileZ = saved_min_z;
        _World3D.maxDrawTileX = saved_max_x;
        _World3D.maxDrawTileZ = saved_max_z;
        key_world = world;
        key_x = ax;
        key_z = az;
        key_min_x = baked_min_x;
        key_min_z = baked_min_z;
        key_max_x = baked_max_x;
        key_max_z = baked_max_z;
        key_level = top_level;
        key_radius = radius;
        key_cover = cover;
        key_stamp = stamp;
        key_ok = 1;
        held_frames = 0;
#ifdef __vita__
        if(!r32_focus_ready&&!roof_alternate.valid&&(top_level==0||top_level==3)){
            int other=top_level==0?3:0;
            uint64_t estimate=retained_scene_vertices(world,other)*(sizeof(Vertex)+sizeof(uint16_t));
            uint64_t free_graphics=(uint64_t)vglMemFree(VGL_MEM_RAM)+vglMemFree(VGL_MEM_VRAM);
            if(free_graphics>estimate+24u*1024u*1024u){
                roof_stash_current();
                _World3D.minDrawTileX=baked_min_x;_World3D.minDrawTileZ=baked_min_z;_World3D.maxDrawTileX=baked_max_x;_World3D.maxDrawTileZ=baked_max_z;
                /* Same-frame roof variants share immutable source models.
                 * Reuse their model-space detail classification, while
                 * rebuilding each view's emitted shading and GPU buffers. */
                r26_preserve_geometry=1;
                int warm=bake_square(world,other);
                r26_preserve_geometry=0;
                _World3D.minDrawTileX=saved_min_x;_World3D.minDrawTileZ=saved_min_z;_World3D.maxDrawTileX=saved_max_x;_World3D.maxDrawTileZ=saved_max_z;
                if(warm){key_world=world;key_x=ax;key_z=az;key_level=other;key_radius=radius;key_cover=cover;key_stamp=stamp;key_ok=1;key_min_x=baked_min_x;key_min_z=baked_min_z;key_max_x=baked_max_x;key_max_z=baked_max_z;bake_serial++;}
                else {release_chunks();water_release();key_ok=0;}
                roof_try(world,stamp,top_level,radius,need_min_x,need_min_z,need_max_x,need_max_z);
            }
        }
#endif
        committed_dirty_events=dirty_events;
        r16_write("R16 bake #%u anchor=%d,%d eye=%d,%d cover=%d level=%d chunks=%d static_vertices=%d static_indices=%d water=%d anim=%d items=%d picks=%d us=%llu", ++bake_serial, key_x, key_z, eye_x, eye_z, key_cover, top_level, chunk_n, static_verts, static_count, water_count, anim_count, item_count, pick_count, (unsigned long long)(ba_timing_now() - began));
    } else if (++held_frames == 1 || (held_frames % 300) == 0) {
        r16_write("R16 held anchor=%d,%d eye=%d,%d cover=%d frames=%u static_vertices=%d", key_x, key_z, eye_x, eye_z, key_cover, held_frames, static_verts);
    }
    uint64_t refresh_at = ba_timing_now();
    overflow = 0;
    refresh_scene_dynamics(world, top_level);
    uint64_t refresh_us = ba_timing_now() - refresh_at;
    vertcount = 0;
    elementcount = 0;
    emitting = 1;
    uint64_t dyn_at = ba_timing_now();
    if(!overflow)emit_dynamic(world, loop_cycle);
    uint64_t dyn_us = ba_timing_now() - dyn_at;
    emitting = 0;
    acc_pick += pick_us;
    acc_refresh += refresh_us;
    acc_dynamic += dyn_us;
    acc_frames++;
    static unsigned frame_log;
    if ((++frame_log % 60u) == 0)
        r16_write("R16 frame pick_us=%llu refresh_us=%llu dynamic_us=%llu anims=%d items=%d water_chunks=%d", (unsigned long long)pick_us, (unsigned long long)refresh_us, (unsigned long long)dyn_us, anim_count, item_count,
#if defined(__vita__) && defined(GL11)
            water_gpu_n
#else
            0
#endif
        );
    if (overflow) {
#if defined(__vita__)
        dyn_draw_n = 0;
#endif
        char failure[256];
        snprintf(failure,sizeof failure,"# renderer_fallback dynamic verts %d elements %d anims %d items %d dyn %d water %d",vertcount,elementcount,anim_count,item_count,dyn_count,water_count);
        ba_cadence_diagnostic(failure);
        release_chunks();
        key_ok = 0;
        budget_off = 1;
        fallback_reason=2;
        vertcount = 0;
        elementcount = 0;
#ifdef __vita__
        roof_drop_alternate();
#endif
        r16_write("R16 dynamic emit over budget. Original renderer stays on.");
        return 0;
    }
    frame_static = static_count > 0;
    frame_ready = 1;
    live_loc_log_ready=1;
    return 1;
#else
    (void)top_level;
    (void)loop_cycle;
    return 0;
#endif
}

#ifndef GL11
void r16_draw_static(void) {}
void r16_draw_static_alpha(void) {}
#endif
