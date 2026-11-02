/* The animated stove cannot use the retained static detail pass. Separate
 * only opaque priority-2 grate faces overlapping its own priority-1
 * surface. The half-unit offset is applied to this frame's emitted copy;
 * animation models, UVs, colors, gameplay positions, and other actors stay intact. */
static unsigned stove_grate_triangles;
/* The authored charcoal back of this stove is exactly on the adjoining
 * wall plane. Preserve its original shade while bringing its inward-facing
 * surface half a unit into the fire chamber, just ahead of that wall. */
static void stove_back_separate(Model*model,int bitset,int first,int x,int z){
#if defined(GL11) && defined(__vita__)
 if(baking||!model||!model->face_colors||(((unsigned)bitset>>14)&32767)!=2727||((unsigned)bitset&0x60000000u)!=0x40000000u)return;
 int visible=0;for(int f=0;f<model->face_count;f++)if(!model->face_infos||model->face_infos[f]!=-1)visible++;
 if(first<0||vertcount-first!=visible*3)return;
 int at=first;
 for(int f=0;f<model->face_count;f++){
  if(model->face_infos&&model->face_infos[f]==-1)continue;Vertex*t=verts+at;at+=3;
  int priority=model->face_priorities?model->face_priorities[f]:model->model_priority;
  if(priority||model->face_colors[f]!=4||(model->face_infos&&(model->face_infos[f]&2))||t[0].a!=255||t[1].a!=255||t[2].a!=255)continue;
  float p[3][3]={{t[0].x,t[0].y,t[0].z},{t[1].x,t[1].y,t[1].z},{t[2].x,t[2].y,t[2].z}};
  float a[3],b[3],n[3];for(int k=0;k<3;k++){a[k]=p[1][k]-p[0][k];b[k]=p[2][k]-p[0][k];}
  n[0]=a[1]*b[2]-a[2]*b[1];n[1]=a[2]*b[0]-a[0]*b[2];n[2]=a[0]*b[1]-a[1]*b[0];
  for(int axis=0;axis<=2;axis+=2){
   float center=axis==0?x:z,edge=p[0][axis];
   if(edge!=p[1][axis]||edge!=p[2][axis]||fabsf(edge-center)!=64||n[axis]*(center-edge)<=0)continue;
   float shift=edge>center?-.5f:.5f;
   for(int k=0;k<3;k++){if(axis==0)t[k].x+=shift;else t[k].z+=shift;}break;
  }
 }
#else
 (void)model;(void)bitset;(void)first;(void)x;(void)z;
#endif
}
static void stove_grate_separate(Model*model,int bitset,int first){
#if defined(GL11) && defined(__vita__)
 if(baking||!model||(((unsigned)bitset>>14)&32767)!=2727||((unsigned)bitset&0x60000000u)!=0x40000000u)return;
 int visible=0;for(int f=0;f<model->face_count;f++)if(!model->face_infos||model->face_infos[f]!=-1)visible++;
 if(first<0||vertcount-first!=visible*3)return;
 int at=first;
 for(int f=0;f<model->face_count;f++){
  if(model->face_infos&&model->face_infos[f]==-1)continue;Vertex*t=verts+at;at+=3;
  int priority=model->face_priorities?model->face_priorities[f]:model->model_priority;
  if(priority!=2||(model->face_infos&&(model->face_infos[f]&2))||t[0].a!=255||t[1].a!=255||t[2].a!=255)continue;
  float xyz[3][3]={{t[0].x,t[0].y,t[0].z},{t[1].x,t[1].y,t[1].z},{t[2].x,t[2].y,t[2].z}};int axis=-1;
  for(int k=0;k<3;k++)if(xyz[0][k]==xyz[1][k]&&xyz[1][k]==xyz[2][k])axis=k;
  if(axis<0)continue;int u=(axis+1)%3,w=(axis+2)%3;R16Point2 a[3];
  for(int k=0;k<3;k++)a[k]=(R16Point2){xyz[k][u]-xyz[0][u],xyz[k][w]-xyz[0][w]};
  float area=fabsf(r16_area2(a,3))*.5f,covered=0;if(area<.01f)continue;int other=first;
  for(int g=0;g<model->face_count;g++){
   if(model->face_infos&&model->face_infos[g]==-1)continue;Vertex*q=verts+other;other+=3;
   int lower=model->face_priorities?model->face_priorities[g]:model->model_priority;
   if(lower!=1||(model->face_infos&&(model->face_infos[g]&2))||q[0].a!=255||q[1].a!=255||q[2].a!=255)continue;
   float p[3][3]={{q[0].x,q[0].y,q[0].z},{q[1].x,q[1].y,q[1].z},{q[2].x,q[2].y,q[2].z}};
   if(p[0][axis]!=xyz[0][axis]||p[1][axis]!=xyz[0][axis]||p[2][axis]!=xyz[0][axis])continue;
   R16Point2 b[3];for(int k=0;k<3;k++)b[k]=(R16Point2){p[k][u]-xyz[0][u],p[k][w]-xyz[0][w]};covered+=r16_triangle_overlap(a,b);
  }
  if(covered<.01f)continue;
  float ab[3],ac[3],n[3];for(int k=0;k<3;k++){ab[k]=xyz[1][k]-xyz[0][k];ac[k]=xyz[2][k]-xyz[0][k];}
  n[0]=ab[1]*ac[2]-ab[2]*ac[1];n[1]=ab[2]*ac[0]-ab[0]*ac[2];n[2]=ab[0]*ac[1]-ab[1]*ac[0];
  float length=sqrtf(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);if(length<.01f)continue;
  for(int k=0;k<3;k++){t[k].x+=.5f*n[0]/length;t[k].y+=.5f*n[1]/length;t[k].z+=.5f*n[2]/length;}
  stove_grate_triangles++;
 }
#else
 (void)model;(void)bitset;(void)first;
#endif
}
