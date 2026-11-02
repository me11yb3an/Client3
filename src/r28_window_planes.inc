/* Shallow wall-mounted panels include paintings as well as window glass.
 * Fit them in their authored normal/tangent basis, with full opaque support.
 * Structural shape9 diagonal walls live in Ground.locs rather than Ground.wall.
 * Deep shelves/heads and other3D fixtures fail the34-unit depth guard. */
static void r28_window_attach(int x,int z,int bitset,int first,int yaw){
#if defined(GL11) && defined(__vita__)
 int id=((unsigned)bitset>>14)&32767;
 if(!baking||!window_decor_context||!trim_world||first<0||first>=vertcount)return;
 /* Cardinal windows already use the accepted specialized fit. */
 if(r25_window_inset(id)&&(yaw&511)!=256)return;
 float nx=(float)_Pix3D.cos_table[yaw&2047],nz=-(float)_Pix3D.sin_table[yaw&2047];
 float length=sqrtf(nx*nx+nz*nz);if(length<1)return;nx/=length;nz/=length;
 float ux=-nz,uz=nx,lo=1e30f,hi=-1e30f,ul=1e30f,uh=-1e30f,yl=1e30f,yh=-1e30f;
 for(int v=first;v<vertcount;v++){
  Vertex*p=verts+v;float d=p->x*nx+p->z*nz,u=p->x*ux+p->z*uz;
  if(d<lo)lo=d;if(d>hi)hi=d;if(u<ul)ul=u;if(u>uh)uh=u;if(p->y<yl)yl=p->y;if(p->y>yh)yh=p->y;
 }
 if(hi-lo>34.f||uh-ul<.01f||yh-yl<.01f)return;
 R16Point2 box[2][3]={{{0,0},{uh-ul,0},{uh-ul,yh-yl}},{{0,0},{uh-ul,yh-yl},{0,yh-yl}}};
 float total=(uh-ul)*(yh-yl),nearest=25.f,shift=0;
 for(int tx=x/128-1;tx<=x/128+1;tx++)for(int tz=z/128-1;tz<=z/128+1;tz++){
  if(tx<0||tz<0||tx>=trim_world->maxTileX||tz>=trim_world->maxTileZ)continue;
  Ground*t=trim_world->levelTiles[trim_level][tx][tz];if(!t)continue;
  for(int source=-2;source<t->locCount;source++){
   Model*m=NULL;int px=0,py=0,pz=0;
   if(source<0){
    if(!t->wall)continue;Wall*w=t->wall;m=source==-2?w->modelA:w->modelB;px=w->x;py=w->y;pz=w->z;
   }else{
    Location*l=t->locs[source];
    if(!l||!l->model||l->entity||l->r10_temporary||l->yaw||(l->info&31)!=9)continue;
    m=l->model;px=l->x;py=l->y;pz=l->z;
   }
   if(!m||m->r10_animated)continue;
   /* Unused model vertices must not inflate the supporting wall extent. */
   float outer=-1e30f;
   for(int f=0;f<m->face_count;f++){
    int ids[]={m->face_indices_a[f],m->face_indices_b[f],m->face_indices_c[f]};
    for(int k=0;k<3;k++){int v=ids[k];float d=(m->vertices_x[v]+px)*nx+(m->vertices_z[v]+pz)*nz;if(d>outer)outer=d;}
   }
   float gap=fabsf(outer-lo);if(gap>24.f||gap>=nearest)continue;
   float covered=0;
   for(int f=0;f<m->face_count;f++){
    if(!r26_opaque_face(m,f))continue;
    int ids[]={m->face_indices_a[f],m->face_indices_b[f],m->face_indices_c[f]},flat=1;R16Point2 tri[3];
    for(int k=0;k<3;k++){
     int v=ids[k];float xx=m->vertices_x[v]+px,zz=m->vertices_z[v]+pz;
     if(fabsf(xx*nx+zz*nz-outer)>.05f){flat=0;break;}
     tri[k]=(R16Point2){xx*ux+zz*uz-ul,m->vertices_y[v]+py-yl};
    }
    if(flat)covered+=r16_triangle_overlap(box[0],tri)+r16_triangle_overlap(box[1],tri);
   }
   if(covered>=total-.05f){nearest=gap;shift=outer-lo+.5f;}
  }
 }
 if(nearest<=24.f){
  for(int v=first;v<vertcount;v++){verts[v].x+=nx*shift;verts[v].z+=nz*shift;}
  shutter_attached++;
  if(fit_file)fprintf(fit_file,"%d,%d,%d,3,%.3f,%.3f,%.3f\n",id,x,z,lo,lo+shift-.5f,nearest);
 }
#else
 (void)x;(void)z;(void)bitset;(void)first;(void)yaw;
#endif
}
