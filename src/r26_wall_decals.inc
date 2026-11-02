/* Opaque authored wall detail is layered in model space. A45degree placement
 * must not disable the coplanarity test. Keep original emitted vertices/UVs;
 * replay each authored priority separately, without depth writes. */
#include "r42_planar_index.h"
#include "r54_crates.h"
#include "r65_bank_fixtures.h"
#include "r78_fur_types.h"
#include "r87_vendor_layers.h"
#include "r90_sewing_table.h"
/* Full424-map audit bounds the largest104x104 all-level fixture replay at
 * 56124 vertices. Retain16-bit indices with room for that dense scene. */
#define R26_DECAL_VERTS 65532
#define R26_LAYERS 12
static Vertex r26_vertices[R26_DECAL_VERTS];
static uint16_t r26_indices[R26_DECAL_VERTS];
static uint8_t r26_priorities[R26_DECAL_VERTS/3];
static int r26_count,r26_first[R26_LAYERS],r26_counts[R26_LAYERS];
static GLuint r26_vbo,r26_ibo;
static int r26_scope(int bitset){
 int id=((unsigned)bitset>>14)&32767;
 /* Exact configured desks1291/3644, workbench2147 and furnace1192
  * author their mats/ornaments/openings as ordered coplanar faces. Include
  * every configured alias, keeping translucent smoke in its original pass. */
 return r90_sewing_table_type(id)||r87_vendor_type(id)||r78_fur_type(id)||r54_crate_type(id)||window_decor_context||r65_bank_layers_type(id)||id==608||id==3546||id==609||id==610||id==165||id==2781||id==3044;
}
static void r26_local_triangle(Model*m,int f,float p[3][3]){
 int ids[]={m->face_indices_a[f],m->face_indices_b[f],m->face_indices_c[f]};
 for(int i=0;i<3;i++){int v=ids[i];p[i][0]=m->vertices_x[v];p[i][1]=m->vertices_y[v];p[i][2]=m->vertices_z[v];}
}
static int r26_opaque_face(Model*m,int f){
 if((m->face_infos&&m->face_infos[f]==-1)||(m->face_alphas&&m->face_alphas[f]))return 0;
 if(m->face_infos&&(m->face_infos[f]&2)){
  if(!m->face_colors||!_Pix3D.textureHasTransparency||!_Pix3D.textures)return 0;
  int id=m->face_colors[f];
  return id>=0&&id<_Pix3D.textureCount&&_Pix3D.textures[id]&&!_Pix3D.textureHasTransparency[id];
 }
 return 1;
}
static int r26_priority(Model*m,int f){return m->face_priorities?m->face_priorities[f]:m->model_priority;}
/* Model-space backing does not change between placements during a bake.
 * Cache geometry only; emitted opacity/colors/UV checks remain per instance.
 * Scene models own these arrays until the bake ends. Release resets all keys
 * before scene memory can be reused; no model pointer survives that boundary. */
#define R26_CACHE_SLOTS 128
#define R26_CACHE_FACES 1024
typedef struct {int faces,priority;const void*arrays[10];} R26GeometryKey;
typedef struct {R26GeometryKey key;int flat;uint8_t flags[R26_CACHE_FACES];} R26GeometryCache;
static R26GeometryCache r26_geometry_cache[R26_CACHE_SLOTS];
static int r26_geometry_count,r26_geometry_next;
/* Only the immediately paired roof bake in the same frame may retain
 * these keys. Ordinary release and every later bake invalidate them. */
static int r26_preserve_geometry;
static R26GeometryCache*r26_geometry_entry(Model*m){
 if(m->face_count<=0||m->face_count>R26_CACHE_FACES)return NULL;
 R26GeometryKey key;memset(&key,0,sizeof key);key.faces=m->face_count;key.priority=m->model_priority;
 const void*arrays[]={m->vertices_x,m->vertices_y,m->vertices_z,m->face_indices_a,m->face_indices_b,m->face_indices_c,m->face_infos,m->face_priorities,m->face_alphas,m->face_colors};
 memcpy(key.arrays,arrays,sizeof arrays);
 for(int i=0;i<r26_geometry_count;i++)if(!memcmp(&key,&r26_geometry_cache[i].key,sizeof key))return &r26_geometry_cache[i];
 int slot=r26_geometry_count<R26_CACHE_SLOTS?r26_geometry_count++:r26_geometry_next++%R26_CACHE_SLOTS;
 R26GeometryCache*c=&r26_geometry_cache[slot];c->key=key;c->flat=-1;memset(c->flags,255,m->face_count);return c;
}
static void r26_capture(Model*m,int bitset,int first){
 if(!baking||!r26_scope(bitset))return;
 int visible=0;for(int f=0;f<m->face_count;f++)if(!m->face_infos||m->face_infos[f]!=-1)visible++;
 if(vertcount-first!=visible*3)return;
 R26GeometryCache*geometry=r26_geometry_entry(m);
 /* Zero-thickness wall art has no backing of its own: the building wall
  * supplies that backing. Its opaque faces need the same ordered replay. */
 int flat;
 if(geometry&&geometry->flat>=0)flat=geometry->flat;
 else{
 float lo[3]={0},hi[3]={0};int seen=0;
 for(int f=0;f<m->face_count;f++)if(!m->face_infos||m->face_infos[f]!=-1){
  float q[3][3];r26_local_triangle(m,f,q);
  for(int i=0;i<3;i++)for(int k=0;k<3;k++){
   if(!seen)lo[k]=hi[k]=q[i][k];else{if(q[i][k]<lo[k])lo[k]=q[i][k];if(q[i][k]>hi[k])hi[k]=q[i][k];}
   if(i==0&&k==2)seen=1;
  }
 }
 flat=seen&&(lo[0]==hi[0]||lo[1]==hi[1]||lo[2]==hi[2]);
 if(geometry)geometry->flat=flat;
 }
 /* A solid model with a single authored priority cannot have any lower-
  * priority backing. Avoid its quadratic face scan. Flat wall art still
  * replays every opaque face, including uniform-priority charts. */
 if(!flat){
  int priority=-1,mixed=0;
  for(int f=0;f<m->face_count;f++)if(!m->face_infos||m->face_infos[f]!=-1){
   int p=r26_priority(m,f);if(priority<0)priority=p;else if(p!=priority){mixed=1;break;}
  }
  if(!mixed)return;
 }
 /* Most detailed wall models have hundreds of priority1 faces backed by
  * only a few priority0 faces. Index opaque backing by priority once,
  * rather than rescanning every detail face for every other face. Lists
  * retain ascending face order within each priority, preserving float
  * coverage sums exactly. Oversize models keep the original bounded walk. */
 enum { R26_BACKING_MAX=4096 };
 static int backing_next[R26_BACKING_MAX];
 int backing_head[R26_LAYERS],indexed=!flat&&m->face_count<=R26_BACKING_MAX;
 if(indexed)r42_begin();
 for(int p=0;p<R26_LAYERS;p++)backing_head[p]=-1;
 if(indexed)for(int g=m->face_count-1;g>=0;g--){
  int p=r26_priority(m,g);
  if(p<0||p>=R26_LAYERS||!r26_opaque_face(m,g))continue;
  backing_next[g]=backing_head[p];backing_head[p]=g;
  int a=m->face_indices_a[g],b=m->face_indices_b[g],c=m->face_indices_c[g];
  const int*coordinates[]={m->vertices_x,m->vertices_y,m->vertices_z};
  for(int axis=0;axis<3;axis++){const int*v=coordinates[axis];float plane=(float)v[a];if(plane==(float)v[b]&&plane==(float)v[c])r42_insert(g,axis,p,plane);}
 }
 int v=first;
 for(int f=0;f<m->face_count;f++){
  if(m->face_infos&&m->face_infos[f]==-1)continue;
  Vertex*t=verts+v;v+=3;int priority=r26_priority(m,f);
  if(priority<(flat?0:1)||priority>=R26_LAYERS||!r26_opaque_face(m,f)||t[0].a!=255||t[1].a!=255||t[2].a!=255)continue;
  int flags=geometry?geometry->flags[f]:255;
  if(flags==255){
  if(geometry)geometry->flags[f]=0;
  float p[3][3];r26_local_triangle(m,f,p);
  double ab[3],ac[3],normal[3];for(int k=0;k<3;k++){ab[k]=p[1][k]-p[0][k];ac[k]=p[2][k]-p[0][k];}
  normal[0]=ab[1]*ac[2]-ab[2]*ac[1];normal[1]=ab[2]*ac[0]-ab[0]*ac[2];normal[2]=ab[0]*ac[1]-ab[1]*ac[0];
  int axis=0;for(int k=1;k<3;k++)if(fabs(normal[k])>fabs(normal[axis]))axis=k;
  if(fabs(normal[axis])<.01)continue;int u=(axis+1)%3,w=(axis+2)%3;
  /* For an axis-aligned detail, a backing vertex on another integer
   * model-coordinate plane cannot pass the existing1e-6 coplanarity test.
   * Reject it before expanding nine coordinates and doing nine double
   * products; candidates on the plane still use the unchanged full test. */
  const int*axis_coordinates=axis==0?m->vertices_x:axis==1?m->vertices_y:m->vertices_z;
  int axis_aligned=normal[u]==0&&normal[w]==0;
  R16Point2 a[3];for(int i=0;i<3;i++)a[i]=(R16Point2){p[i][u]-p[0][u],p[i][w]-p[0][w]};
  float area=fabsf(r16_area2(a,3))*.5f,coverage[R26_LAYERS]={0},front_coverage[R26_LAYERS]={0};if(area<.01f)continue;
  int planar=indexed&&axis_aligned;
  for(int bucket=0;!flat&&bucket<(indexed?priority:1);bucket++){
  for(int node=planar?r42_head(axis,bucket,p[0][axis]):indexed?backing_head[bucket]:0;node>=0&&(indexed||node<m->face_count);node=planar?r42_links[node]:indexed?backing_next[node]:node+1){
   int g=planar?node/3:node;if(planar&&node%3!=axis)continue;
   if(!r26_opaque_face(m,g))continue;
   int lower=r26_priority(m,g);if(lower<0||lower>=priority)continue;
   if(planar&&lower!=bucket)continue;
   if(axis_aligned&&((float)axis_coordinates[m->face_indices_a[g]]!=p[0][axis]||(float)axis_coordinates[m->face_indices_b[g]]!=p[0][axis]||(float)axis_coordinates[m->face_indices_c[g]]!=p[0][axis]))continue;
   float q[3][3];r26_local_triangle(m,g,q);
   /* Exact axis-plane equality was already proven above. The remaining
    * two normal components are zero, so all three dot products are zero.
    * Diagonal faces retain the original double-precision plane test. */
   int coplanar=1;if(!axis_aligned)for(int i=0;i<3;i++){
    double d=0;for(int k=0;k<3;k++)d+=normal[k]*(q[i][k]-p[0][k]);
    if(fabs(d)>1e-6*(fabs(normal[0])+fabs(normal[1])+fabs(normal[2])))coplanar=0;
   }
   if(!coplanar)continue;
   R16Point2 b[3];for(int i=0;i<3;i++)b[i]=(R16Point2){q[i][u]-p[0][u],q[i][w]-p[0][w]};
   if(fabsf(r16_area2(b,3))<.02f)continue; /* A collapsed face supplies no backing. */
   float overlap=r16_triangle_overlap(a,b);coverage[lower]+=overlap;
   double qb[3],qc[3],qn[3];for(int k=0;k<3;k++){qb[k]=q[1][k]-q[0][k];qc[k]=q[2][k]-q[0][k];}
   qn[0]=qb[1]*qc[2]-qb[2]*qc[1];qn[1]=qb[2]*qc[0]-qb[0]*qc[2];qn[2]=qb[0]*qc[1]-qb[1]*qc[0];
   if(normal[0]*qn[0]+normal[1]*qn[1]+normal[2]*qn[2]>0)front_coverage[lower]+=overlap;
  }
  }
  /* The sewing tabletop has an opposite-facing underside on the same
   * plane. Only same-facing backing may promote its visible decoration. */
  int sewing=r90_sewing_table_type(((unsigned)bitset>>14)&32767);
  int covered=flat;for(int k=0;k<priority;k++)if((sewing?front_coverage[k]:coverage[k])>=area-.01f)covered=1;
  int partial=0;for(int k=0;k<priority;k++)if(front_coverage[k]>.01f)partial=1;
  flags=(covered?1:0)|(partial?2:0);if(geometry)geometry->flags[f]=(uint8_t)flags;
  }
  int covered=flags&1;
  /* A broken shutter's slat can overhang its own backing. It still needs
   * its authored priority wherever they overlap. Copy the whole original
   * opaque flat face; do not clip/recolor its overhang or promote textures,
   * varying-color faces, transparent material, or merely touching edges. */
  int constant=(!m->face_infos||!(m->face_infos[f]&2));
  for(int i=1;i<3;i++)if(t[i].r!=t[0].r||t[i].g!=t[0].g||t[i].b!=t[0].b||t[i].a!=t[0].a||t[i].u!=t[0].u||t[i].v!=t[0].v)constant=0;
  /* Crate frame boards overhang their panel backing and use smooth
   * vertex lighting. Preserve their full authored face and interpolation. */
  int detail_id=((unsigned)bitset>>14)&32767;
  int crate_detail=(r54_crate_type(detail_id)||r87_vendor_type(detail_id)||r90_sewing_table_type(detail_id))&&(!m->face_infos||!(m->face_infos[f]&2));
  if(!covered&&(constant||crate_detail)&&(flags&2))covered=1;
  if(!covered)continue;
  if(r26_count+3>R26_DECAL_VERTS){overflow=1;return;}
  memcpy(r26_vertices+r26_count,t,3*sizeof(Vertex));r26_priorities[r26_count/3]=(uint8_t)(priority+1);r26_count+=3;
 }
}
static void r26_release(void){
 if(!r26_preserve_geometry)r26_geometry_count=r26_geometry_next=0;
 if(r26_vbo)glDeleteBuffers(1,&r26_vbo);if(r26_ibo)glDeleteBuffers(1,&r26_ibo);
 r26_vbo=r26_ibo=0;r26_count=0;memset(r26_first,0,sizeof r26_first);memset(r26_counts,0,sizeof r26_counts);
}
static void r26_upload(void){
 if(!r26_count||overflow)return;
 int n=0;for(int layer=1;layer<=R26_LAYERS;layer++){
  r26_first[layer-1]=n;
  for(int v=0;v<r26_count;v+=3)if(r26_priorities[v/3]==layer){for(int k=0;k<3;k++)r26_indices[n++]=(uint16_t)(v+k);}
  r26_counts[layer-1]=n-r26_first[layer-1];
 }
 glGenBuffers(1,&r26_vbo);glGenBuffers(1,&r26_ibo);
 glBindBuffer(GL_ARRAY_BUFFER,r26_vbo);glBufferData(GL_ARRAY_BUFFER,r26_count*sizeof(Vertex),r26_vertices,GL_STATIC_DRAW);
 glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,r26_ibo);glBufferData(GL_ELEMENT_ARRAY_BUFFER,r26_count*sizeof(uint16_t),r26_indices,GL_STATIC_DRAW);
#ifdef R26_REGISTER_INDICES
 if(!R26_REGISTER_INDICES(r26_ibo,r26_indices,r26_count,r26_counts)){r26_release();overflow=1;return;}
#endif
 GLenum error=glGetError();glBindBuffer(GL_ARRAY_BUFFER,0);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
 if(!r26_vbo||!r26_ibo||error!=GL_NO_ERROR){r26_release();overflow=1;}
}
static void r26_draw(void){
 if(!r26_count||!r26_vbo||!r26_ibo)return;
 glBindBuffer(GL_ARRAY_BUFFER,r26_vbo);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,r26_ibo);
 glVertexPointer(3,GL_FLOAT,sizeof(Vertex),(const void*)offsetof(Vertex,x));
 glTexCoordPointer(2,GL_FLOAT,sizeof(Vertex),(const void*)offsetof(Vertex,u));
 glColorPointer(4,GL_UNSIGNED_BYTE,sizeof(Vertex),(const void*)offsetof(Vertex,r));
 glDepthMask(GL_FALSE);glEnable(GL_POLYGON_OFFSET_FILL);
 for(int layer=1;layer<=R26_LAYERS;layer++)if(r26_counts[layer-1]){
  glPolygonOffset(-(float)layer,-(float)layer);
  glDrawElements(GL_TRIANGLES,r26_counts[layer-1],GL_UNSIGNED_SHORT,(const void*)(size_t)(r26_first[layer-1]*sizeof(uint16_t)));
 }
 glDisable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(0,0);glDepthMask(GL_TRUE);
}
