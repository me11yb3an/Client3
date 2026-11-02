/* Cave-top models are a fallback ceiling/ground surface. Where an actual
 * higher physical floor covers that footprint, retain the authored floor
 * and subtract only the redundant cave-top fragment. Never affect outdoors.
 * Source models, stone floors and roof masks are unchanged. */
static int r52_world_z;static unsigned r52_clipped_models;
static int r52_type(int id){return id==1434||id==1441||id==1460||id==1484||id==1485||id==1490||id==1503;}
static int r52_active(int bitset){return baking&&r52_world_z>=6400&&trim_world&&r52_type(((unsigned)bitset>>14)&32767);}
static int r52_add_floor(R51Hole*holes,int*n,const float q[3][3],float height){
 for(int k=0;k<3;k++)if(fabsf(q[k][1]-height)>96)return 1;
 if(*n>=256)return 0;R51Hole*h=holes+(*n)++;h->height=height;h->level=0;h->xl=h->zl=1e30f;h->xh=h->zh=-1e30f;
 for(int k=0;k<3;k++){h->p[k]=(R16Point2){q[k][0],q[k][2]};h->xl=fminf(h->xl,q[k][0]);h->xh=fmaxf(h->xh,q[k][0]);h->zl=fminf(h->zl,q[k][2]);h->zh=fmaxf(h->zh,q[k][2]);}return 1;
}
static void r52_clip_top(int bitset,int first,int first_element){
 if(!r52_active(bitset)||first>=vertcount)return;
 float xl=1e30f,xh=-1e30f,zl=1e30f,zh=-1e30f,height=0;
 for(int i=first;i<vertcount;i++){Vertex*v=verts+i;xl=fminf(xl,v->x);xh=fmaxf(xh,v->x);zl=fminf(zl,v->z);zh=fmaxf(zh,v->z);height+=v->y;}
 height/=(vertcount-first);
 int x0=(int)floorf(xl/128),x1=(int)floorf(xh/128),z0=(int)floorf(zl/128),z1=(int)floorf(zh/128);
 if(x1-x0>4||z1-z0>4)return;
 R51Hole holes[256];int n=0;
 for(int level=trim_world->minLevel;level<trim_world->maxLevel;level++)for(int x=x0;x<=x1;x++)for(int z=z0;z<=z1;z++){
  if(x<0||z<0||x>=trim_world->maxTileX||z>=trim_world->maxTileZ)continue;
  Ground*g=trim_world->levelTiles[level][x][z];if(!g||g->drawLevel>trim_top_level||g->occludeLevel<=trim_source_plane)continue;
  if(g->underlay){
   TileUnderlay*u=g->underlay;int h=g->occludeLevel;
   float xx=x*128,zz=z*128,y00=trim_world->levelHeightmaps[h][x][z],y10=trim_world->levelHeightmaps[h][x+1][z],y01=trim_world->levelHeightmaps[h][x][z+1],y11=trim_world->levelHeightmaps[h][x+1][z+1];
   float a[3][3]={{xx+128,y11,zz+128},{xx,y01,zz+128},{xx+128,y10,zz}};
   float b[3][3]={{xx,y00,zz},{xx+128,y10,zz},{xx,y01,zz+128}};
   if(u->northeastColor!=12345678&&!r52_add_floor(holes,&n,a,height))return;
   if(u->southwestColor!=12345678&&!r52_add_floor(holes,&n,b,height))return;
  }else if(g->overlay){
   TileOverlay*o=g->overlay;
   for(int f=0;f<o->triangleCount;f++){
    int texture=o->triangleTextureIds?o->triangleTextureIds[f]:-1;if(texture==-1&&o->triangleColorA[f]==12345678)continue;
    int ids[]={o->triangleVertexA[f],o->triangleVertexB[f],o->triangleVertexC[f]};float q[3][3];
    for(int k=0;k<3;k++){int v=ids[k];q[k][0]=o->vertexX[v];q[k][1]=o->vertexY[v];q[k][2]=o->vertexZ[v];}
    if(!r52_add_floor(holes,&n,q,height))return;
   }
  }
 }
 if(!n)return;
 static Vertex result[3072],piece[1536];int count=0,changed=0;
 for(int i=first_element;i+2<elementcount;i+=3){Vertex tri[3];for(int k=0;k<3;k++){unsigned at=indices[i+k];if(at<(unsigned)first||at>=(unsigned)vertcount)return;tri[k]=verts[at];}
  int used=r51_cut_triangle_limit(tri,holes,n,0,xl,zl,piece,1536,128.f);if(used==-2)return;if(used<0){used=3;memcpy(piece,tri,sizeof tri);}else changed=1;
  if(count+used>3072)return;memcpy(result+count,piece,used*sizeof(Vertex));count+=used;
 }
 if(!changed||first+count>R16_CHUNK_VERTS||first_element+count>R16_CHUNK_VERTS)return;
 r52_clipped_models++;memcpy(verts+first,result,count*sizeof(Vertex));vertcount=first+count;elementcount=first_element;for(int i=0;i<count;i++)indices[elementcount++]=first+i;
}
