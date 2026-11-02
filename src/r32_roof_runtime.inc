#include "r32_roof_zones.h"
static R32RoofZones r32_zones;
static const void*r32_flags_source;
static int r32_zones_epoch=-1,r32_focus_ready,r32_player_level;
static uint16_t r32_selected,r32_current_tag=65535u;
#define R32_HAS_SELECTION (r32_focus_ready&&r32_selected!=0)
#define R32_HIDDEN(tag) r32_zone_hidden(&r32_zones,r32_selected,r32_player_level,(tag))
#include "r32_index_masks.inc"
static uint16_t r32_vertex_tags[R16_CHUNK_VERTS],r32_water_vertex_tags[R16_CHUNK_VERTS];
static uint16_t r32_sorted_tags[R32_MAX_INDICES/3];
static uint16_t r32_decal_tags[65532/3],r32_trim_tags[32760/3],r32_clock_tags[8190/3],r32_floorcap_tags[8190/3];
static void r32_mark_vertices(int first,int end){
 if(!baking)return;if(first<0||end<first||end>R16_CHUNK_VERTS){overflow=1;return;}
 for(int i=first;i<end;i++)r32_vertex_tags[i]=r32_current_tag;
}
static void r32_mark_faces(uint16_t*tags,int capacity,int first,int end){
 if(!baking)return;if(first<0||end<first||first%3||end%3||end/3>capacity){overflow=1;return;}
 for(int i=first/3;i<end/3;i++)tags[i]=r32_current_tag;
}
static int r32_vertex_triangle_tags(const uint16_t*ix,int count,const uint16_t*tags,int nv){
 if(count<0||count>R32_MAX_INDICES||count%3)return 0;
 for(int i=0;i<count;i+=3){
  unsigned a=ix[i],b=ix[i+1],c=ix[i+2];
  if(a>=(unsigned)nv||b>=(unsigned)nv||c>=(unsigned)nv||tags[a]!=tags[b]||tags[a]!=tags[c])return 0;
  r32_sorted_tags[i/3]=tags[a];
 }
 return 1;
}
static int r32_register_detail(GLuint ibo,const uint16_t*ix,int count,int groups,const int*counts,const uint16_t*face_tags){
 if(!r32_focus_ready)return 1;
 if(count<0||count>R32_MAX_INDICES||count%3)return 0;
 for(int i=0;i<count;i+=3){
  unsigned a=ix[i],b=ix[i+1],c=ix[i+2];
  if(a>=(unsigned)count||b>=(unsigned)count||c>=(unsigned)count||a/3!=b/3||a/3!=c/3)return 0;
  r32_sorted_tags[i/3]=face_tags[a/3];
 }
 return r32_register_indices(ibo,ix,r32_sorted_tags,count,groups,counts);
}
static int r32_tile_visible(int draw_level,int x,int z,int old_top){
 return r32_focus_ready?!R32_HIDDEN(r32_tile_tag(draw_level,x,z)):draw_level<=old_top;
}
#define R26_REGISTER_INDICES(ibo,ix,n,counts) r32_register_detail((ibo),(ix),(n),R26_LAYERS,(counts),r32_decal_tags)
#define R18_WATER_TAG_COPY(dst,src) (r32_water_vertex_tags[(dst)]=r32_vertex_tags[(src)])
