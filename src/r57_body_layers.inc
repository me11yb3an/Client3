#include <stdint.h>
#include "r57_body_layers.h"
#include "r16_coplanar.h"
#define R57_MAX_FACES 8192
#define R57_MAX_PAIRS 4096
#define R57_MAX_BYTES (1024u*1024u)
static size_t r57_bytes;
static R57Layers *r57_allocations;
typedef struct { int face; int64_t plane[4]; } R57Plane;
static int r57_priority(Model*m,int f){return m->face_priorities?m->face_priorities[f]:m->model_priority;}
static int r57_solid(Model*m,int f){
 return f>=0&&f<m->face_count&&(!m->face_infos||(m->face_infos[f]!=-1&&!(m->face_infos[f]&2)))&&(!m->face_alphas||m->face_alphas[f]==0);
}
static int r57_backing_solid(Model*m,int f,uint64_t textures){
 if(r57_solid(m,f))return 1;
 if(!textures||f<0||f>=m->face_count||!m->face_infos||m->face_infos[f]==-1||!(m->face_infos[f]&2)||!m->face_colors||(m->face_alphas&&m->face_alphas[f]))return 0;
 int id=m->face_colors[f];return id>=0&&id<64&&(textures&(UINT64_C(1)<<id))!=0;
}
static int r57_points(Model*m,int f,double p[3][3],double n[3]){
 int ids[3]={m->face_indices_a[f],m->face_indices_b[f],m->face_indices_c[f]};
 for(int i=0;i<3;i++){int v=ids[i];if(v<0||v>=m->vertex_count)return 0;p[i][0]=m->vertices_x[v];p[i][1]=m->vertices_y[v];p[i][2]=m->vertices_z[v];}
 double a[3],b[3];for(int k=0;k<3;k++){a[k]=p[1][k]-p[0][k];b[k]=p[2][k]-p[0][k];}
 n[0]=a[1]*b[2]-a[2]*b[1];n[1]=a[2]*b[0]-a[0]*b[2];n[2]=a[0]*b[1]-a[1]*b[0];
 return fabs(n[0])+fabs(n[1])+fabs(n[2])>.01;
}
static int r57_overlap_backing(Model*m,int h,int l,double tolerance,uint64_t textures){
 if(!r57_solid(m,h)||!r57_backing_solid(m,l,textures))return 0;
 int hp=r57_priority(m,h),lp=r57_priority(m,l);
 /* Priorities10/11 use special depth interleaving in the original client. */
 if(hp<1||hp>9||lp<0||lp>=hp)return 0;
 double p[3][3],q[3][3],n[3],b[3];if(!r57_points(m,h,p,n)||!r57_points(m,l,q,b))return 0;
 if(n[0]*b[0]+n[1]*b[1]+n[2]*b[2]<=0)return 0;
 double mag=sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
 for(int i=0;i<3;i++){double d=0;for(int k=0;k<3;k++)d+=n[k]*(q[i][k]-p[0][k]);if(fabs(d)>tolerance*mag)return 0;}
 int drop=0;for(int k=1;k<3;k++)if(fabs(n[k])>fabs(n[drop]))drop=k;
 int u=(drop+1)%3,v=(drop+2)%3;R16Point2 a2[3],b2[3];
 for(int i=0;i<3;i++){a2[i]=(R16Point2){p[i][u]-p[0][u],p[i][v]-p[0][v]};b2[i]=(R16Point2){q[i][u]-p[0][u],q[i][v]-p[0][v]};}
 return r16_triangle_overlap(a2,b2)>.01f;
}
static int r57_overlap(Model*m,int h,int l,double tolerance){return r57_overlap_backing(m,h,l,tolerance,0);}
#include "r77_body_selection_cache.inc"
static int64_t r57_gcd(int64_t a,int64_t b){if(a<0)a=-a;if(b<0)b=-b;while(b){int64_t c=a%b;a=b;b=c;}return a;}
static int r57_plane_cmp(const void*a,const void*b){const R57Plane*x=a,*y=b;for(int k=0;k<4;k++)if(x->plane[k]!=y->plane[k])return x->plane[k]<y->plane[k]?-1:1;return 0;}
static R57Layers*r57_alloc(unsigned count){
 if(!count||count>R57_MAX_PAIRS)return NULL;
 size_t bytes=sizeof(R57Layers)+count*sizeof(R57Pair);if(r57_bytes+bytes>R57_MAX_BYTES)return NULL;
 R57Layers*p=malloc(bytes);if(p){p->refs=1;p->count=count;p->backing_textures=0;p->prev=NULL;p->next=r57_allocations;if(p->next)p->next->prev=p;r57_allocations=p;r57_bytes+=bytes;}return p;
}
R57Layers*r57_retain_body_layers(R57Layers*p){if(p)p->refs++;return p;}
void r57_release_body_layers(R57Layers*p){if(p&&!--p->refs){if(p->prev)p->prev->next=p->next;else r57_allocations=p->next;if(p->next)p->next->prev=p->prev;r57_bytes-=sizeof(*p)+p->count*sizeof(R57Pair);free(p);}}
/* Cached actor bases live in the existing bump arena, whose LRU eviction
 * does not call model_free. Reclaim their candidates at the same arena reset. */
void r57_reset_body_layers(void){r77_reset();while(r57_allocations){R57Layers*p=r57_allocations;r57_allocations=p->next;free(p);}r57_bytes=0;}
void r57_prepare_fixture_layers(Model*m,uint64_t backing_textures){
 if(!m||!m->face_priorities||m->face_count<1||m->face_count>R57_MAX_FACES||!m->vertices_x||!m->vertices_y||!m->vertices_z)return;
 if(m->r57_body_layers){if(!backing_textures||m->r57_body_layers->backing_textures==backing_textures)return;r57_release_body_layers(m->r57_body_layers);m->r57_body_layers=NULL;}
 /* Cached actor and fixture base models call this. Sort integer planes once;
  * animation frames never repeat the all-face search. */
 for(int v=0;v<m->vertex_count;v++)if(abs(m->vertices_x[v])>16384||abs(m->vertices_y[v])>16384||abs(m->vertices_z[v])>16384)return;
 R57Plane*planes=malloc(m->face_count*sizeof(*planes));R57Pair*pairs=malloc(R57_MAX_PAIRS*sizeof(*pairs));
 if(!planes||!pairs){free(planes);free(pairs);return;}int n=0,count=0,checks=0,failed=0;
 for(int f=0;f<m->face_count;f++)if(r57_backing_solid(m,f,backing_textures)&&r57_priority(m,f)>=0&&r57_priority(m,f)<=9){
  double p[3][3],normal[3];if(!r57_points(m,f,p,normal))continue;
  int64_t a=(int64_t)normal[0],b=(int64_t)normal[1],c=(int64_t)normal[2],g=r57_gcd(r57_gcd(a,b),c);if(!g)continue;
  a/=g;b/=g;c/=g;if(a<0||(!a&&b<0)||(!a&&!b&&c<0)){a=-a;b=-b;c=-c;}
  planes[n++]=(R57Plane){f,{a,b,c,-(a*(int64_t)p[0][0]+b*(int64_t)p[0][1]+c*(int64_t)p[0][2])}};
 }
 qsort(planes,n,sizeof(*planes),r57_plane_cmp);
 for(int first=0;first<n&&!failed;){int end=first+1;while(end<n&&!r57_plane_cmp(planes+first,planes+end))end++;
  for(int i=first;i<end&&!failed;i++)for(int j=i+1;j<end;j++){
   int h=planes[i].face,l=planes[j].face,hp=r57_priority(m,h),lp=r57_priority(m,l);if(hp==lp)continue;if(hp<lp){int t=h;h=l;l=t;}
   if(++checks>262144){failed=1;break;}
   if(r57_overlap_backing(m,h,l,.00001,backing_textures)){if(count==R57_MAX_PAIRS){failed=1;break;}pairs[count++]=(R57Pair){h,l};}
  }first=end;
 }
 if(!failed&&count){m->r57_body_layers=r57_alloc(count);if(m->r57_body_layers){m->r57_body_layers->backing_textures=backing_textures;memcpy(m->r57_body_layers->pairs,pairs,count*sizeof(*pairs));}}
 free(planes);free(pairs);
}
void r57_prepare_body_layers(Model*m){r57_prepare_fixture_layers(m,0);}
void r57_merge_body_layers(Model*m,Model**parts,int count){
 unsigned n=0;for(int i=0;i<count;i++)if(parts[i]&&parts[i]->r57_body_layers)n+=parts[i]->r57_body_layers->count;
 if(!n||m->face_count>R57_MAX_FACES)return;R57Layers*out=r57_alloc(n);if(!out)return;
 int offset=0,at=0;for(int i=0;i<count;i++)if(parts[i]){R57Layers*p=parts[i]->r57_body_layers;if(p){out->backing_textures|=p->backing_textures;for(unsigned j=0;j<p->count;j++)out->pairs[at++]=(R57Pair){p->pairs[j].high+offset,p->pairs[j].low+offset};}offset+=parts[i]->face_count;}
 m->r57_body_layers=out;
}
int r57_select_body_layers(Model*m,unsigned char*selected,int capacity){
 if(!m||!selected||m->face_count<1||m->face_count>capacity)return 0;
 memset(selected,0,m->face_count);R57Layers*p=m->r57_body_layers;if(!p)return 0;int count=0;
 /* The actor memo intentionally excludes texture IDs. Vendor backing checks
  * stay outside that memo; unchanged vendor meshes retain GPU replay indices. */
 int memo=!p->backing_textures;if(memo&&r77_find(m,p,selected,&count))return count;
 for(unsigned i=0;i<p->count;i++){int h=p->pairs[i].high,l=p->pairs[i].low;if(h>=m->face_count||l>=m->face_count||selected[h])continue;
  /* Integer skeletal rotations can put an authored coplanar pair slightly
   * apart. More widely separated or reversed posed faces are never replayed. */
  if(r57_overlap_backing(m,h,l,.75,p->backing_textures)){selected[h]=(unsigned char)r57_priority(m,h);count++;}
 }if(memo)r77_store(m,selected,count);return count;
}

void r77_body_cache_status(unsigned out[3]){out[0]=r77_hits;out[1]=r77_misses;out[2]=(unsigned)r77_bytes;r77_hits=r77_misses=0;}
