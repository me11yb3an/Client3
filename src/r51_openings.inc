#include "r51_opening_clip.h"
#include "r51_opening_types.h"
#define R51_MAX_HOLES 2048
static R51Hole r51_holes[R51_MAX_HOLES];static int r51_count,r51_failed,r51_clipped;
static unsigned char r51_tiles[4][104][104];static World3D*r51_world;
static void r51_reset(void){r51_world=NULL;r51_count=r51_clipped=0;}
static void r51_add(Location*l,int level,const float q[3][3],float sn,float cs){
  if(r51_count>=R51_MAX_HOLES){r51_failed=1;return;}R51Hole*h=r51_holes+r51_count++;
  h->height=l->y;h->level=level;h->xl=h->zl=1e30f;h->xh=h->zh=-1e30f;
  for(int k=0;k<3;k++){float x=q[k][0]*cs+q[k][2]*sn+l->x,z=q[k][2]*cs-q[k][0]*sn+l->z;h->p[k]=(R16Point2){x,z};if(x<h->xl)h->xl=x;if(x>h->xh)h->xh=x;if(z<h->zl)h->zl=z;if(z>h->zh)h->zh=z;}
  int x0=(int)floorf(h->xl/128),x1=(int)floorf(h->xh/128),z0=(int)floorf(h->zl/128),z1=(int)floorf(h->zh/128);
  for(int x=x0;x<=x1;x++)for(int z=z0;z<=z1;z++)if(x>=0&&z>=0&&x<104&&z<104)r51_tiles[level][x][z]=1;
}
static int r51_rim_corner(Model*m,int x,int y,int z){for(int f=0;f<m->face_count;f++){int ids[]={m->face_indices_a[f],m->face_indices_b[f],m->face_indices_c[f]};for(int k=0;k<3;k++){int v=ids[k];if(m->vertices_x[v]==x&&m->vertices_y[v]==y&&m->vertices_z[v]==z)return 1;}}return 0;}
static void r51_collect(Location*l,int level){
 if(!l||!l->model||l->entity||l->r10_temporary||(l->info&31)!=10)return;
 int id=((unsigned)l->bitset>>14)&32767;R51Type type=r51_type(id);if(!type.kind)return;
 Model*m=l->model;float sn=(float)_Pix3D.sin_table[l->yaw&2047]/65536.f,cs=(float)_Pix3D.cos_table[l->yaw&2047]/65536.f;
 /* These downward ladders author a complete square rim but rely on the
  * map for its void. Cut only a verified intact 128-unit rim; loose/broken
  * ladders and unframed ship stairs do not qualify. */
 if(type.kind==4){
  int a=type.color;
  for(int x=-a;x<=a;x+=2*a)for(int z=-a;z<=a;z+=2*a)if(!r51_rim_corner(m,x,0,z)||!r51_rim_corner(m,x,type.depth,z))return;
  float q[2][3][3]={{{-a,0,-a},{a,0,-a},{a,0,a}},{{-a,0,-a},{a,0,a},{-a,0,a}}};
  r51_add(l,level,q[0],sn,cs);r51_add(l,level,q[1],sn,cs);return;
 }
 for(int f=0;f<m->face_count;f++){
  if(!r26_opaque_face(m,f))continue;
  int ids[]={m->face_indices_a[f],m->face_indices_b[f],m->face_indices_c[f]};float q[3][3];
  for(int k=0;k<3;k++){int v=ids[k];q[k][0]=m->vertices_x[v];q[k][1]=m->vertices_y[v];q[k][2]=m->vertices_z[v];}
  float ny=(q[1][2]-q[0][2])*(q[2][0]-q[0][0])-(q[1][0]-q[0][0])*(q[2][2]-q[0][2]);if(ny>=-.1f)continue;
  if(type.kind==1){if((m->face_colors&&m->face_colors[f]!=0)||q[0][1]<64||q[0][1]!=q[1][1]||q[0][1]!=q[2][1])continue;}
  else if(type.kind==2){if((m->face_colors&&m->face_colors[f]!=type.color)||q[0][1]!=type.depth||q[1][1]!=type.depth||q[2][1]!=type.depth)continue;}
  else if(q[0][1]<0||q[1][1]<0||q[2][1]<0)continue;
  r51_add(l,level,q,sn,cs);
 }
}
static void r51_collect_tile(Ground*t,int x,int z){
 if(!t||t->occludeLevel<0||t->occludeLevel>3)return;
 for(int i=0;i<t->locCount;i++){Location*l=t->locs[i];if(l&&l->minSceneTileX==x&&l->minSceneTileZ==z)r51_collect(l,t->occludeLevel);}
}
static void r51_prepare(World3D*w){
 /* These are structural apertures in this landscape. Keep them through a
  * ladder's temporary remove/restore animation and roof/floor cache swaps.
  * r16_dirty clears the plan before a landscape can be replaced. */
 if(r51_world==w)return;
 r51_world=w;r51_count=r51_failed=0;memset(r51_tiles,0,sizeof r51_tiles);
 for(int l=w->minLevel;l<w->maxLevel;l++)for(int x=0;x<w->maxTileX;x++)for(int z=0;z<w->maxTileZ;z++){Ground*t=w->levelTiles[l][x][z];r51_collect_tile(t,x,z);if(t)r51_collect_tile(t->bridge,x,z);}
 if(r51_failed){r51_count=0;memset(r51_tiles,0,sizeof r51_tiles);}
}
static int r51_has(World3D*w,int level,int x,int z){return w==r51_world&&level>=0&&level<4&&x>=0&&x<104&&z>=0&&z<104&&r51_tiles[level][x][z];}
static int r51_point_open(World3D*w,int level,int x,int z,const Vertex*p){
 if(!r51_has(w,level,x,z))return 0;
 for(int i=0;i<r51_count;i++){R51Hole*h=r51_holes+i;if(h->level!=level||fabsf(p->y-h->height)>32)continue;float sign=((h->p[1].x-h->p[0].x)*(h->p[2].y-h->p[0].y)-(h->p[1].y-h->p[0].y)*(h->p[2].x-h->p[0].x))>0?1.f:-1.f;int inside=1;
  for(int k=0;k<3;k++){R16Point2 a=h->p[k],b=h->p[(k+1)%3];if(sign*((b.x-a.x)*(p->z-a.y)-(b.y-a.y)*(p->x-a.x))<-.001f){inside=0;break;}}if(inside)return 1;
 }
 return 0;
}
static void r51_clip_floor(int level,int x,int z,int first,int first_element){
 if(!r51_has(trim_world,level,x,z))return;
 static Vertex result[3072],piece[1536];int count=0,changed=0;
 for(int i=first_element;i+2<elementcount;i+=3){Vertex tri[3];for(int k=0;k<3;k++){unsigned at=indices[i+k];if(at<(unsigned)first||at>=(unsigned)vertcount)return;tri[k]=verts[at];}
  int n=r51_cut_triangle(tri,r51_holes,r51_count,level,x*128,z*128,piece,1536);if(n==-2)return;if(n<0){n=3;memcpy(piece,tri,sizeof tri);}else changed=1;
  if(count+n>3072)return;memcpy(result+count,piece,n*sizeof(Vertex));count+=n;
 }
 if(!changed||first+count>R16_CHUNK_VERTS||first_element+count>R16_CHUNK_VERTS)return;
 r51_clipped++;memcpy(verts+first,result,count*sizeof(Vertex));vertcount=first+count;elementcount=first_element;
 for(int i=0;i<count;i++)indices[elementcount++]=first+i;
}
static float r51_cap_overlap(const R16Point2 cap[3],const R16Point2 floor[3],float ox,float oz,float height,int level,int x,int z){
 if(!r51_has(trim_world,level,x,z))return r16_triangle_overlap(cap,floor);
 Vertex in[3];static Vertex out[1536];for(int k=0;k<3;k++)in[k]=(Vertex){255,255,255,255,floor[k].x+ox,height,floor[k].y+oz,0,0};
 int n=r51_cut_triangle(in,r51_holes,r51_count,level,x*128,z*128,out,1536);if(n<0)return r16_triangle_overlap(cap,floor);float area=0;
 for(int i=0;i+2<n;i+=3){R16Point2 p[3];for(int k=0;k<3;k++)p[k]=(R16Point2){out[i+k].x-ox,out[i+k].z-oz};area+=r16_triangle_overlap(cap,p);}return area;
}
