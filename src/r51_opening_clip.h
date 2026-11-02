#pragma once
typedef struct {R16Point2 p[3];float height;int level;float xl,xh,zl,zh;} R51Hole;
typedef struct {int n;Vertex p[16];} R51Poly;
static Vertex r51_mix(Vertex a,Vertex b,float t){
 Vertex v=a;v.x=a.x+t*(b.x-a.x);v.y=a.y+t*(b.y-a.y);v.z=a.z+t*(b.z-a.z);v.u=a.u+t*(b.u-a.u);v.v=a.v+t*(b.v-a.v);
 v.r=(unsigned char)(a.r+t*((int)b.r-a.r)+.5f);v.g=(unsigned char)(a.g+t*((int)b.g-a.g)+.5f);v.b=(unsigned char)(a.b+t*((int)b.b-a.b)+.5f);v.a=(unsigned char)(a.a+t*((int)b.a-a.a)+.5f);return v;
}
static float r51_area(const R51Poly*p,float ox,float oz){R16Point2 q[16];for(int i=0;i<p->n;i++)q[i]=(R16Point2){p->p[i].x-ox,p->p[i].z-oz};return p->n>=3?fabsf(r16_area2(q,p->n))*.5f:0;}
static int r51_append(R51Poly*p,Vertex v){
 if(p->n&&fabsf(p->p[p->n-1].x-v.x)<.0001f&&fabsf(p->p[p->n-1].z-v.z)<.0001f)return 1;
 if(p->n>=16)return 0;p->p[p->n++]=v;return 1;
}
static void r51_clean(R51Poly*p){
 if(p->n>1&&fabsf(p->p[0].x-p->p[p->n-1].x)<.0001f&&fabsf(p->p[0].z-p->p[p->n-1].z)<.0001f)p->n--;
 for(int i=0;p->n>=3&&i<p->n;){int a=i?i-1:p->n-1,b=(i+1)%p->n;float ax=p->p[i].x-p->p[a].x,az=p->p[i].z-p->p[a].z,bx=p->p[b].x-p->p[i].x,bz=p->p[b].z-p->p[i].z;
  if(fabsf(ax*bz-az*bx)<.0001f&&ax*bx+az*bz>=0){memmove(p->p+i,p->p+i+1,(p->n-i-1)*sizeof(Vertex));p->n--;if(i)i--;}
  else i++;
 }
}
static int r51_clip_half(const R51Poly*in,R51Poly*out,R16Point2 a,R16Point2 b,float sign,float ox,float oz){
 out->n=0;
 for(int i=0;i<in->n;i++){
  Vertex p=in->p[i],q=in->p[i?i-1:in->n-1];
  float dp=sign*((b.x-a.x)*(p.z-oz-a.y)-(b.y-a.y)*(p.x-ox-a.x));
  float dq=sign*((b.x-a.x)*(q.z-oz-a.y)-(b.y-a.y)*(q.x-ox-a.x));
  if((dp>=0)!=(dq>=0))if(!r51_append(out,r51_mix(q,p,dq/(dq-dp))))return 0;
  if(dp>=0&&!r51_append(out,p))return 0;
 }
 r51_clean(out);
 return 1;
}
/* -1 = byte-identical original, -2 = bounded-work fallback, >=0 = remaining
 * vertices. Repeated/overlapping aperture triangles subtract their union. */
static int r51_cut_triangle_limit(const Vertex input[3],const R51Hole*holes,int count,int level,float ox,float oz,Vertex*out,int capacity,float tolerance){
 static R51Poly parts[2][128];int bank=0,n=1,changed=0;parts[0][0].n=3;memcpy(parts[0][0].p,input,3*sizeof(Vertex));
 float xl=fminf(input[0].x,fminf(input[1].x,input[2].x)),xh=fmaxf(input[0].x,fmaxf(input[1].x,input[2].x));
 float zl=fminf(input[0].z,fminf(input[1].z,input[2].z)),zh=fmaxf(input[0].z,fmaxf(input[1].z,input[2].z));
 R16Point2 original[3];for(int k=0;k<3;k++)original[k]=(R16Point2){input[k].x-ox,input[k].z-oz};
 for(int h=0;h<count&&n;h++){
  const R51Hole*hole=holes+h;if(hole->level!=level||hole->xh<=xl||hole->xl>=xh||hole->zh<=zl||hole->zl>=zh)continue;
  if(fabsf(input[0].y-hole->height)>tolerance||fabsf(input[1].y-hole->height)>tolerance||fabsf(input[2].y-hole->height)>tolerance)continue;
  R16Point2 tri[3];for(int k=0;k<3;k++)tri[k]=(R16Point2){hole->p[k].x-ox,hole->p[k].y-oz};
  if(r16_triangle_overlap(original,tri)<=.01f)continue;changed=1;
  float sign=r16_area2(tri,3)>=0?1.f:-1.f;int next=0;
  for(int p=0;p<n;p++){
   R51Poly remainder=parts[bank][p];
   for(int e=0;e<3&&remainder.n>=3;e++){
    R51Poly outside,inside;
    if(!r51_clip_half(&remainder,&outside,tri[e],tri[(e+1)%3],-sign,ox,oz)||!r51_clip_half(&remainder,&inside,tri[e],tri[(e+1)%3],sign,ox,oz))return -2;
    if(r51_area(&outside,ox,oz)>.01f){if(next>=128)return -2;parts[1-bank][next++]=outside;}
    remainder=inside;
   }
  }
  n=next;bank=1-bank;
 }
 if(!changed)return -1;
 int used=0;
 for(int p=0;p<n;p++)for(int k=1;k+1<parts[bank][p].n;k++){
  if(used+3>capacity)return -2;out[used++]=parts[bank][p].p[0];out[used++]=parts[bank][p].p[k];out[used++]=parts[bank][p].p[k+1];
 }
 return used;
}

static int r51_cut_triangle(const Vertex input[3],const R51Hole*holes,int count,int level,float ox,float oz,Vertex*out,int capacity){return r51_cut_triangle_limit(input,holes,count,level,ox,oz,out,capacity,32.f);}
