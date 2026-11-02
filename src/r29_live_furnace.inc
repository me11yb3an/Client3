/* Furnace1192's outlet is priority6 trim and priority7 black opening on its
 * priority4 shell. Furnaces stay live to preserve stall-free skilling. Separate
 * only those proved opaque layers in the emitted copy used by the live cache. */
static void r29_furnace_layers(Model*m,int bitset,int first){
#if defined(GL11) && defined(__vita__)
 int id=((unsigned)bitset>>14)&32767;
 if(baking||!m||((unsigned)bitset&0x60000000u)!=0x40000000u||(id!=165&&id!=2781&&id!=3044)||first<0)return;
 int visible=0;for(int f=0;f<m->face_count;f++)if(!m->face_infos||m->face_infos[f]!=-1)visible++;
 if(vertcount-first!=visible*3)return;
 int at=first;
 for(int f=0;f<m->face_count;f++){
  if(m->face_infos&&m->face_infos[f]==-1)continue;Vertex*t=verts+at;at+=3;
  int priority=r26_priority(m,f);
  if((priority!=6&&priority!=7)||!r26_opaque_face(m,f)||t[0].a!=255||t[1].a!=255||t[2].a!=255)continue;
  float p[3][3];r26_local_triangle(m,f,p);double ab[3],ac[3],n[3];
  for(int k=0;k<3;k++){ab[k]=p[1][k]-p[0][k];ac[k]=p[2][k]-p[0][k];}
  n[0]=ab[1]*ac[2]-ab[2]*ac[1];n[1]=ab[2]*ac[0]-ab[0]*ac[2];n[2]=ab[0]*ac[1]-ab[1]*ac[0];
  int axis=0;for(int k=1;k<3;k++)if(fabs(n[k])>fabs(n[axis]))axis=k;if(fabs(n[axis])<.01)continue;
  int u=(axis+1)%3,w=(axis+2)%3;R16Point2 a[3];for(int i=0;i<3;i++)a[i]=(R16Point2){p[i][u]-p[0][u],p[i][w]-p[0][w]};
  float area=fabsf(r16_area2(a,3))*.5f,covered[8]={0};if(area<.01f)continue;
  for(int g=0;g<m->face_count;g++){
   if(!r26_opaque_face(m,g))continue;int lower=r26_priority(m,g);if(lower<4||lower>=priority)continue;
   float q[3][3];r26_local_triangle(m,g,q);int flat=1;
   for(int i=0;i<3;i++){double d=0;for(int k=0;k<3;k++)d+=n[k]*(q[i][k]-p[0][k]);if(fabs(d)>1e-6*(fabs(n[0])+fabs(n[1])+fabs(n[2])))flat=0;}
   if(!flat)continue;R16Point2 b[3];for(int i=0;i<3;i++)b[i]=(R16Point2){q[i][u]-p[0][u],q[i][w]-p[0][w]};covered[lower]+=r16_triangle_overlap(a,b);
  }
  int backed=0;for(int lower=4;lower<priority;lower++)if(covered[lower]>=area-.01f)backed=1;if(!backed)continue;
  float abw[3]={t[1].x-t[0].x,t[1].y-t[0].y,t[1].z-t[0].z},acw[3]={t[2].x-t[0].x,t[2].y-t[0].y,t[2].z-t[0].z};
  float nx=abw[1]*acw[2]-abw[2]*acw[1],ny=abw[2]*acw[0]-abw[0]*acw[2],nz=abw[0]*acw[1]-abw[1]*acw[0];
  float len=sqrtf(nx*nx+ny*ny+nz*nz);if(len<.01f)continue;float distance=.5f*(priority-4);
  for(int i=0;i<3;i++){t[i].x+=distance*nx/len;t[i].y+=distance*ny/len;t[i].z+=distance*nz/len;}
 }
#else
 (void)m;(void)bitset;(void)first;
#endif
}
