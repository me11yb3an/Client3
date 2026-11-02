/* Adjacent solid wall caps occupy the same plane with opposite normals.
 * Remove only a completely buried vertical cap; leave exposed/partial caps,
 * transparent walls, decorations and every source model unchanged. */
static int wall_seam_context;
static unsigned r33_buried_caps;
static int r33_cap_triangle(const Vertex*t,int*axis,float*sign,float*area){
 if(t[0].a!=255||t[1].a!=255||t[2].a!=255)return 0;
 float ax=t[1].x-t[0].x,ay=t[1].y-t[0].y,az=t[1].z-t[0].z;
 float bx=t[2].x-t[0].x,by=t[2].y-t[0].y,bz=t[2].z-t[0].z;
 float n[3]={ay*bz-az*by,az*bx-ax*bz,ax*by-ay*bx};
 *axis=fabsf(n[0])>fabsf(n[2])?0:2;
 if(fabsf(n[1])>.01f||fabsf(n[*axis])<.01f)return 0;
 for(int i=1;i<3;i++)if(fabsf((*axis==0?t[i].x:t[i].z)-(*axis==0?t[0].x:t[0].z))>.01f)return 0;
 float u[3]={*axis==0?t[0].z:t[0].x,*axis==0?t[1].z:t[1].x,*axis==0?t[2].z:t[2].x};
 float width=fmaxf(u[0],fmaxf(u[1],u[2]))-fminf(u[0],fminf(u[1],u[2]));
 if(width<=.01f||width>32.01f)return 0;
 *sign=n[*axis];*area=fabsf(*sign)*.5f;return 1;
}
static float r33_opposed_overlap(const Vertex*t,const Vertex*q,int axis,float sign){
 float plane=axis==0?t[0].x:t[0].z;
 for(int i=0;i<3;i++)if(q[i].a!=255||fabsf((axis==0?q[i].x:q[i].z)-plane)>.01f)return 0;
 float a[3]={q[1].x-q[0].x,q[1].y-q[0].y,q[1].z-q[0].z};
 float b[3]={q[2].x-q[0].x,q[2].y-q[0].y,q[2].z-q[0].z};
 float normal=axis==0?a[1]*b[2]-a[2]*b[1]:a[0]*b[1]-a[1]*b[0];
 if(normal*sign>=0)return 0;
 R16Point2 p[3],r[3];float origin=axis==0?t[0].z:t[0].x,y=t[0].y;
 for(int i=0;i<3;i++){p[i]=(R16Point2){(axis==0?t[i].z:t[i].x)-origin,t[i].y-y};r[i]=(R16Point2){(axis==0?q[i].z:q[i].x)-origin,q[i].y-y};}
 return r16_triangle_overlap(p,r);
}
static void r33_wall_seams(Model*model,int first,int x,int y,int z){
 if(!baking||!wall_seam_context||!trim_world||first<0)return;
 int visible=0;for(int f=0;f<model->face_count;f++)if(!model->face_infos||model->face_infos[f]!=-1)visible++;
 if(vertcount-first!=visible*3)return;
 int face=0;
 for(int at=first;at+2<vertcount;at+=3){
  while(face<model->face_count&&model->face_infos&&model->face_infos[face]==-1)face++;
  if(face>=model->face_count)break;
  if(!r26_opaque_face(model,face++))continue;
  Vertex*t=verts+at;int axis;float sign,area;if(!r33_cap_triangle(t,&axis,&sign,&area))continue;
  int buried=0;
  for(int tx=x/128-1;tx<=x/128+1&&!buried;tx++)for(int tz=z/128-1;tz<=z/128+1&&!buried;tz++){
   if(tx<0||tz<0||tx>=trim_world->maxTileX||tz>=trim_world->maxTileZ)continue;
   Ground*g=trim_world->levelTiles[trim_level][tx][tz];if(!g||!g->wall)continue;
   Wall*w=g->wall;Model*parts[]={w->modelA,w->modelB};
   for(int part=0;part<2&&!buried;part++){
    Model*m=parts[part];if(!m||m->r10_animated||(m==model&&w->x==x&&w->y==y&&w->z==z))continue;
    float sum=0;
    for(int f=0;f<m->face_count;f++){
     if(!r26_opaque_face(m,f))continue;
     int ids[]={m->face_indices_a[f],m->face_indices_b[f],m->face_indices_c[f]};Vertex q[3];memset(q,0,sizeof q);
     for(int k=0;k<3;k++){int v=ids[k];q[k].a=255;q[k].x=m->vertices_x[v]+w->x;q[k].y=m->vertices_y[v]+w->y;q[k].z=m->vertices_z[v]+w->z;}
     sum+=r33_opposed_overlap(t,q,axis,sign);
    }
    if(sum>=area-.01f)buried=1;
   }
  }
  if(buried){t[1].x=t[2].x=t[0].x;t[1].y=t[2].y=t[0].y;t[1].z=t[2].z=t[0].z;r33_buried_caps++;}
 }
}
