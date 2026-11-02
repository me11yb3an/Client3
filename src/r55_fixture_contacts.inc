/* Same-plane fixture faces win over their authored deck backing. */
static int r55_depth_type(int id){return id==766||id==767||id==768||id==769||id==770||id==1003;}
static int r55_floor_opaque(int texture){return texture<0||(_Pix3D.textures&&_Pix3D.textureHasTransparency&&texture<_Pix3D.textureCount&&_Pix3D.textures[texture]&&!_Pix3D.textureHasTransparency[texture]);}
static int r55_terrain_face(const Vertex*t,Ground*g,int x,int z){
 if(!g||g->occludeLevel!=trim_source_plane)return 0;
 if(g->underlay){
  TileUnderlay*u=g->underlay;if(!r55_floor_opaque(u->textureId))return 0;int h=g->occludeLevel;
  float xx=x*128,zz=z*128,y00=trim_world->levelHeightmaps[h][x][z],y10=trim_world->levelHeightmaps[h][x+1][z],y01=trim_world->levelHeightmaps[h][x][z+1],y11=trim_world->levelHeightmaps[h][x+1][z+1];
  float a[3][3]={{xx+128,y11,zz+128},{xx,y01,zz+128},{xx+128,y10,zz}},b[3][3]={{xx,y00,zz},{xx+128,y10,zz},{xx,y01,zz+128}};
  return (u->northeastColor!=12345678&&r53_backed(t,a))||(u->southwestColor!=12345678&&r53_backed(t,b));
 }
 if(g->overlay){TileOverlay*o=g->overlay;
  for(int f=0;f<o->triangleCount;f++){
   int texture=o->triangleTextureIds?o->triangleTextureIds[f]:-1;if(!r55_floor_opaque(texture)||(texture<0&&o->triangleColorA[f]==12345678))continue;
   int ids[]={o->triangleVertexA[f],o->triangleVertexB[f],o->triangleVertexC[f]};float q[3][3];
   for(int k=0;k<3;k++){int v=ids[k];q[k][0]=o->vertexX[v];q[k][1]=o->vertexY[v];q[k][2]=o->vertexZ[v];}if(r53_backed(t,q))return 1;
  }
 }return 0;
}
static int r55_depth_face(const Vertex*t,Ground*g){
 if(!g||g->occludeLevel!=trim_source_plane)return 0;
 for(int i=0;i<g->locCount;i++){
  Location*l=g->locs[i];if(!l||!l->model||l->entity||l->r10_temporary||l->model->r10_animated||r16_live_location(l->bitset)||!r55_depth_type(((unsigned)l->bitset>>14)&32767))continue;
  Model*m=l->model;float sn=(float)_Pix3D.sin_table[l->yaw&2047]/65536.f,cs=(float)_Pix3D.cos_table[l->yaw&2047]/65536.f;
  for(int f=0;f<m->face_count;f++){
   if(!r26_opaque_face(m,f))continue;int ids[]={m->face_indices_a[f],m->face_indices_b[f],m->face_indices_c[f]};float q[3][3];
   for(int k=0;k<3;k++){int v=ids[k];float x=m->vertices_x[v],z=m->vertices_z[v];q[k][0]=x*cs+z*sn+l->x;q[k][1]=m->vertices_y[v]+l->y;q[k][2]=z*cs-x*sn+l->z;}if(r53_backed(t,q))return 1;
  }
 }return 0;
}
static void r55_fixture_contacts(Model*m,int bitset,int first){
 int id=((unsigned)bitset>>14)&32767,crane=id==2520||id==2521;
 if(!baking||!trim_world||(!crane&&id!=994)||trim_source_plane<0||trim_source_plane>3||first<0||first>=vertcount)return;
 int visible=0;for(int f=0;f<m->face_count;f++)if(!m->face_infos||m->face_infos[f]!=-1)visible++;if(vertcount-first!=visible*3)return;
 int v=first;
 for(int f=0;f<m->face_count;f++){
  if(m->face_infos&&m->face_infos[f]==-1)continue;Vertex*t=verts+v;v+=3;
  if(!r26_opaque_face(m,f)||t[0].a!=255||t[1].a!=255||t[2].a!=255)continue;
  if(crane){int a=m->face_indices_a[f],b=m->face_indices_b[f],c=m->face_indices_c[f];if(m->vertices_y[a]||m->vertices_y[b]||m->vertices_y[c])continue;
   float ny=(t[1].z-t[0].z)*(t[2].x-t[0].x)-(t[1].x-t[0].x)*(t[2].z-t[0].z);if(ny>=-.01f)continue;}
  float xl=fminf(t[0].x,fminf(t[1].x,t[2].x)),xh=fmaxf(t[0].x,fmaxf(t[1].x,t[2].x)),zl=fminf(t[0].z,fminf(t[1].z,t[2].z)),zh=fmaxf(t[0].z,fmaxf(t[1].z,t[2].z));
  if(xh-xl>512||zh-zl>512)continue;int backed=0;
  for(int level=trim_world->minLevel;level<trim_world->maxLevel&&!backed;level++)
  for(int x=(int)floorf(xl/128)-1;x<=(int)floorf(xh/128)+1&&!backed;x++)for(int z=(int)floorf(zl/128)-1;z<=(int)floorf(zh/128)+1&&!backed;z++){
   if(x<0||z<0||x>=trim_world->maxTileX||z>=trim_world->maxTileZ)continue;Ground*g=trim_world->levelTiles[level][x][z];if(!g)continue;
   if(crane)backed=r55_terrain_face(t,g,x,z)||r55_terrain_face(t,g->bridge,x,z);
   if(!backed)backed=r55_depth_face(t,g)||r55_depth_face(t,g->bridge);
  }
  if(!backed)continue;if(trim_count+3>R16_TRIM_VERTS){layer_dropped++;continue;}
  memcpy(trim_cpu+trim_count,t,3*sizeof(Vertex));trim_count+=3;r55_contacts++;
 }
}
