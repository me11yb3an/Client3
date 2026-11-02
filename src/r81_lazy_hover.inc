/* Suppressed hover keeps the original face order and triangle-box hit rule.
 * Project each referenced vertex only on first use; stop at the first hit.
 * Rendering and pickable actor coarse-box selection keep their old paths. */
static uint32_t r81_seen[4096],r81_generation;
static int r81_lazy_hover(Model*m,int yaw,int sceneX,int sceneY,int sceneZ,
 int sy,int cy,int sp,int cp,int centreX,int centreY,int mouseX,int mouseY,
 int yawSin,int yawCos){
 if(m->vertex_count<0||m->vertex_count>4096||!m->vertices_x||!m->vertices_y||!m->vertices_z)return -1;
 if(++r81_generation==0){memset(r81_seen,0,sizeof r81_seen);r81_generation=1;}
 for(int f=0;f<m->face_count;f++){
  if(m->face_infos&&m->face_infos[f]==-1)continue;
  int ids[3]={m->face_indices_a[f],m->face_indices_b[f],m->face_indices_c[f]};
  for(int k=0;k<3;k++){
   int v=ids[k];if((unsigned)v>=(unsigned)m->vertex_count)return -1;
   if(r81_seen[v]==r81_generation)continue;
   r81_seen[v]=r81_generation;
   int x=m->vertices_x[v],y=m->vertices_y[v],z=m->vertices_z[v],t;
   if(yaw){t=(z*yawSin+x*yawCos)>>16;z=(z*yawCos-x*yawSin)>>16;x=t;}
   x+=sceneX;y+=sceneY;z+=sceneZ;
   t=(z*sy+x*cy)>>16;z=(z*cy-x*sy)>>16;x=t;
   t=(y*cp-z*sp)>>16;z=(y*sp+z*cp)>>16;
   if(z>=50){_Model.vertex_screen_x[v]=centreX+(x<<9)/z;_Model.vertex_screen_y[v]=centreY+(t<<9)/z;}
   else _Model.vertex_screen_x[v]=-5000;
  }
  int a=ids[0],b=ids[1],c=ids[2];
  int xa=_Model.vertex_screen_x[a],xb=_Model.vertex_screen_x[b],xc=_Model.vertex_screen_x[c];
  if(xa==-5000||xb==-5000||xc==-5000)continue;
  int ya=_Model.vertex_screen_y[a],yb=_Model.vertex_screen_y[b],yc=_Model.vertex_screen_y[c];
  if(mouseY<ya&&mouseY<yb&&mouseY<yc)continue;
  if(mouseY>ya&&mouseY>yb&&mouseY>yc)continue;
  if(mouseX<xa&&mouseX<xb&&mouseX<xc)continue;
  if(mouseX<=xa||mouseX<=xb||mouseX<=xc)return 1;
 }
 return 0;
}
