#pragma once
/* Bake/cache-miss only. Fit hidden furniture volume to the union of actual
 * opaque wall triangles. Never infer a solid wall across an opening. */
typedef struct {float p[3][3];} R50WallFace;
typedef struct {int n;R16Point2 p[16];} R50Poly;
static int r50_clip(const R50Poly*in,R50Poly*out,R16Point2 a,R16Point2 b,float side){
 out->n=0;
 for(int i=0;i<in->n;i++){
  R16Point2 p=in->p[i],q=in->p[i?i-1:in->n-1];
  float dp=side*((b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x));
  float dq=side*((b.x-a.x)*(q.y-a.y)-(b.y-a.y)*(q.x-a.x));
  if((dp>=0)!=(dq>=0)){
   if(out->n>=16)return 0;float t=dq/(dq-dp);
   out->p[out->n++]=(R16Point2){q.x+t*(p.x-q.x),q.y+t*(p.y-q.y)};
  }
  if(dp>=0){if(out->n>=16)return 0;out->p[out->n++]=p;}
 }
 return 1;
}
/* Conservative broad phase only. Preserve the original clipping and
 * coverage arithmetic for touching/nearby faces, with a full unit margin. */
static void r114_bounds(const R16Point2*p,int n,float out[4]){
 out[0]=out[1]=p[0].x;out[2]=out[3]=p[0].y;
 for(int i=1;i<n;i++){if(p[i].x<out[0])out[0]=p[i].x;if(p[i].x>out[1])out[1]=p[i].x;if(p[i].y<out[2])out[2]=p[i].y;if(p[i].y>out[3])out[3]=p[i].y;}
}
static int r114_separate(const float b[4],const R16Point2*p,int n){
 float q[4];r114_bounds(p,n,q);
 return q[1]<b[0]-1.f||q[0]>b[1]+1.f||q[3]<b[2]-1.f||q[2]>b[3]+1.f;
}
static int r50_polygon_covered(const R50WallFace*faces,int count,float nx,float nz,float plane,float ul,float yl,R50Poly input,float tolerance){
 static R50Poly parts[2][128];int bank=0,n=1;
 parts[0][0]=input;float input_bounds[4];r114_bounds(input.p,input.n,input_bounds);
 for(int f=0;f<count&&n;f++){
  R16Point2 tri[3];int flat=1;
  for(int k=0;k<3;k++){
   const float*p=faces[f].p[k];if(fabsf(p[0]*nx+p[2]*nz-plane)>.05f){flat=0;break;}
   tri[k]=(R16Point2){-p[0]*nz+p[2]*nx-ul,p[1]-yl};
  }
  if(!flat||r114_separate(input_bounds,tri,3)||fabsf(r16_area2(tri,3))<.01f)continue;
  float orientation=r16_area2(tri,3)>0?1.f:-1.f;int next=0;
  for(int j=0;j<n;j++){
   R50Poly remainder=parts[bank][j];
   for(int edge=0;edge<3&&remainder.n>=3;edge++){
    R50Poly outside,inside;R16Point2 a=tri[edge],b=tri[(edge+1)%3];
    if(!r50_clip(&remainder,&outside,a,b,-orientation)||!r50_clip(&remainder,&inside,a,b,orientation))return 0;
    if(outside.n>=3&&fabsf(r16_area2(outside.p,outside.n))>.01f){if(next>=128)return 0;parts[1-bank][next++]=outside;}
    remainder=inside;
   }
  }
  n=next;bank=1-bank;
 }
 if(!n)return 1;
 /* World-space float vertices can lose <.002 unit when a diagonal model
  * is translated thousands of units. Bound the residual by that perimeter
  * error; a real opening is not replaced by summed/duplicated face area. */
 float residual=0;for(int i=0;i<n;i++)residual+=fabsf(r16_area2(parts[bank][i].p,parts[bank][i].n))*.5f;
 return residual<tolerance;
}
static int r50_wall_covers(const R50WallFace*faces,int count,float nx,float nz,float plane,float ul,float uh,float yl,float yh){
 R50Poly input={4,{{0,0},{uh-ul,0},{uh-ul,yh-yl},{0,yh-yl}}};
 return r50_polygon_covered(faces,count,nx,nz,plane,ul,yl,input,.002f*(uh-ul+yh-yl));
}
/* A fixture can span solid wall and a window. Prove a real intersection
 * with solid backing, then fit the hidden rear hull consistently. Merely
 * projecting into an opening cannot trigger this path. Wall/window meshes
 * and the fixture's front, colors and UVs are never changed. */
static int r50_fit_intersecting_back(Vertex*v,int n,const R50WallFace*faces,int count,float nx,float nz,float plane,float ox,float oy,float oz){
 if(n%3)return 0;
 int hit=0;
 for(int i=0;i<n&&!hit;i+=3){
  if(v[i].a!=255||v[i+1].a!=255||v[i+2].a!=255)continue;
  float p[3][3];
  for(int k=0;k<3;k++){float x=v[i+k].x-ox,z=v[i+k].z-oz;p[k][0]=-x*nz+z*nx;p[k][1]=v[i+k].y-oy;p[k][2]=plane-x*nx-z*nz-.001f;}
  /* Keep only the triangle portion actually behind the supporting plane. */
  R50Poly behind={0};
  for(int k=0;k<3;k++){
   int j=k?k-1:2;
   if((p[k][2]>=0)!=(p[j][2]>=0)){float t=p[j][2]/(p[j][2]-p[k][2]);behind.p[behind.n++]=(R16Point2){p[j][0]+t*(p[k][0]-p[j][0])-p[0][0],p[j][1]+t*(p[k][1]-p[j][1])-p[0][1]};}
   if(p[k][2]>=0)behind.p[behind.n++]=(R16Point2){p[k][0]-p[0][0],p[k][1]-p[0][1]};
  }
  if(behind.n<3||fabsf(r16_area2(behind.p,behind.n))<.01f)continue;
  float behind_bounds[4];r114_bounds(behind.p,behind.n,behind_bounds);
  for(int f=0;f<count&&!hit;f++){
   R16Point2 wall[3];int flat=1;
   for(int k=0;k<3;k++){
    const float*q=faces[f].p[k];if(fabsf(q[0]*nx+q[2]*nz-plane)>.05f){flat=0;break;}
    wall[k]=(R16Point2){-q[0]*nz+q[2]*nx-p[0][0],q[1]-p[0][1]};
   }
   if(!flat||r114_separate(behind_bounds,wall,3))continue;
   for(int t=1;t+1<behind.n;t++){R16Point2 a[3]={behind.p[0],behind.p[t],behind.p[t+1]};if(r16_triangle_overlap(a,wall)>.01f){hit=1;break;}}
  }
 }
 if(!hit)return 0;
 for(int i=0;i<n;i++)if(v[i].a==255){float d=(v[i].x-ox)*nx+(v[i].z-oz)*nz,shift=plane+1.f-d;if(shift>0){v[i].x+=nx*shift;v[i].z+=nz*shift;}}
 return 1;
}
/* Coordinates are relative to the fixture origin, avoiding far-scene area
 * cancellation. Returned planes are interior surfaces, not exterior faces. */
static int r50_fit_vertices_mode(Vertex*v,int n,const R50WallFace*faces,int count,float ox,float oy,float oz,int partial){
 float cx=0,cz=0,xl=1e30f,xh=-1e30f,zl=1e30f,zh=-1e30f;
 for(int i=0;i<n;i++)if(v[i].a==255){float x=v[i].x-ox,z=v[i].z-oz;if(x<xl)xl=x;if(x>xh)xh=x;if(z<zl)zl=z;if(z>zh)zh=z;}
 if(xl>xh||xh-xl>1024||zh-zl>1024)return 0;cx=(xl+xh)*.5f;cz=(zl+zh)*.5f;
 /* Cardinal planes have exact extrema on the existing opaque-vertex box.
  * Refresh it after any accepted fit; diagonal planes retain the old scan. */
 float bx0=0,bx1=0,bz0=0,bz1=0,by0=0,by1=0;int bounds_valid=0;
 float seen[64][3];int used=0,fitted=0;
 for(int f=0;f<count;f++){
  const float*a=faces[f].p[0],*b=faces[f].p[1],*c=faces[f].p[2];
  float abx=b[0]-a[0],aby=b[1]-a[1],abz=b[2]-a[2],acx=c[0]-a[0],acy=c[1]-a[1],acz=c[2]-a[2];
  float nx=aby*acz-abz*acy,ny=abz*acx-abx*acz,nz=abx*acy-aby*acx;
  float length=sqrtf(nx*nx+nz*nz);if(length<.01f||fabsf(ny)>length*.0001f)continue;
  nx/=length;nz/=length;float plane=a[0]*nx+a[2]*nz;
  if(cx*nx+cz*nz<plane){nx=-nx;nz=-nz;plane=-plane;}
  int duplicate=0;for(int j=0;j<used;j++)if(fabsf(seen[j][0]-nx)<.0001f&&fabsf(seen[j][1]-nz)<.0001f&&fabsf(seen[j][2]-plane)<.05f)duplicate=1;
  if(duplicate)continue;if(used>=64)return fitted;seen[used][0]=nx;seen[used][1]=nz;seen[used++][2]=plane;
  float lo=1e30f,hi=-1e30f,ul=1e30f,uh=-1e30f,yl=1e30f,yh=-1e30f;
  int cardinal=(nz==0.f&&(nx==1.f||nx==-1.f))||(nx==0.f&&(nz==1.f||nz==-1.f));
  if(cardinal){
   if(!bounds_valid){
    bx0=bz0=by0=1e30f;bx1=bz1=by1=-1e30f;
    for(int i=0;i<n;i++)if(v[i].a==255){float x=v[i].x-ox,y=v[i].y-oy,z=v[i].z-oz;if(x<bx0)bx0=x;if(x>bx1)bx1=x;if(y<by0)by0=y;if(y>by1)by1=y;if(z<bz0)bz0=z;if(z>bz1)bz1=z;}
    bounds_valid=1;
   }
   if(nz==0.f){lo=nx>0?bx0:-bx1;hi=nx>0?bx1:-bx0;ul=nx>0?bz0:-bz1;uh=nx>0?bz1:-bz0;}
   else{lo=nz>0?bz0:-bz1;hi=nz>0?bz1:-bz0;ul=nz>0?-bx1:bx0;uh=nz>0?-bx0:bx1;}
   yl=by0;yh=by1;
  }else for(int i=0;i<n;i++)if(v[i].a==255){
   float x=v[i].x-ox,y=v[i].y-oy,z=v[i].z-oz,d=x*nx+z*nz,u=-x*nz+z*nx;
   if(d<lo)lo=d;if(d>hi)hi=d;if(u<ul)ul=u;if(u>uh)uh=u;if(y<yl)yl=y;if(y>yh)yh=y;
  }
  float penetration=plane-lo;
  if(penetration<-.001f||penetration>24.f||hi-plane<1.f||uh-ul<.01f||yh-yl<.01f)continue;
  if(!r50_wall_covers(faces,count,nx,nz,plane,ul,uh,yl,yh)){
   if(partial&&r50_fit_intersecting_back(v,n,faces,count,nx,nz,plane,ox,oy,oz)){fitted++;bounds_valid=0;}
   continue;
  }
  for(int i=0;i<n;i++)if(v[i].a==255){float d=(v[i].x-ox)*nx+(v[i].z-oz)*nz,shift=plane+1.f-d;if(shift>0){v[i].x+=nx*shift;v[i].z+=nz*shift;}}
  fitted++;bounds_valid=0;
 }
 return fitted;
}
static int r50_fit_vertices(Vertex*v,int n,const R50WallFace*faces,int count,float ox,float oy,float oz){return r50_fit_vertices_mode(v,n,faces,count,ox,oy,oz,1);}
