#pragma once
/* Bake-local bounds only. Cached models are immutable for the duration of
 * a bake; the epoch invalidates all pointers before another bake. Collisions
 * recompute bounds. No placement, source geometry or replay rule changes. */
typedef struct {uint32_t epoch;const Model*model;int lo[3],hi[3];}R110RoofBound;
static R110RoofBound r110_roof_bounds[512];static uint32_t r110_epoch;
static unsigned r110_queries,r110_rejected;
static void r110_begin(void){if(!++r110_epoch){memset(r110_roof_bounds,0,sizeof r110_roof_bounds);r110_epoch=1;}r110_queries=r110_rejected=0;}
static int r110_roof_candidate(const Vertex*t,Model*m,int x,int y,int z){
 if(!m||m->face_count<=0)return 1;
 R110RoofBound*c=r110_roof_bounds+(((uintptr_t)m>>4)&511u);
 if(c->epoch!=r110_epoch||c->model!=m){
  int first=1;
  for(int f=0;f<m->face_count;f++){
   int ids[]={m->face_indices_a[f],m->face_indices_b[f],m->face_indices_c[f]};
   for(int k=0;k<3;k++){int v=ids[k],q[]={m->vertices_x[v],m->vertices_y[v],m->vertices_z[v]};
    for(int a=0;a<3;a++){if(first||q[a]<c->lo[a])c->lo[a]=q[a];if(first||q[a]>c->hi[a])c->hi[a]=q[a];}first=0;
   }
  }
  c->model=m;c->epoch=r110_epoch;
 }
 float lo[]={t[0].x,t[0].y,t[0].z},hi[]={t[0].x,t[0].y,t[0].z};int offset[]={x,y,z};
 for(int k=1;k<3;k++){float q[]={t[k].x,t[k].y,t[k].z};for(int a=0;a<3;a++){if(q[a]<lo[a])lo[a]=q[a];if(q[a]>hi[a])hi[a]=q[a];}}
 r110_queries++;
 for(int a=0;a<3;a++)if((float)(c->hi[a]+offset[a])<lo[a]-.001f||(float)(c->lo[a]+offset[a])>hi[a]+.001f){r110_rejected++;return 0;}
 return 1;
}
