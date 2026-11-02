/* Correct the visible local player's sole contact, not authoritative movement,
 * tile heights, collision or the original shared/animated model. Y increases
 * downward. The GL floor uses triangle planes, not bilinear interpolation. */
static const void*player_contact_owner;
static int player_contact_offset;
int r16_player_visual_offset(const void*entity){return frame_ready&&entity==player_contact_owner?player_contact_offset:0;}

static int player_surface_y(World3D*w,int level,int x,int z,float*out){
 if(level<0||level>=w->maxLevel||x<0||z<0)return 0;
 int tx=x>>7,tz=z>>7;if(tx>=w->maxTileX||tz>=w->maxTileZ)return 0;
 Ground*t=w->levelTiles[level][tx][tz];if(!t)return 0;
 int h=t->occludeLevel;if(h<0||h>=w->maxLevel)return 0;
 float fx=(float)(x&127),fz=(float)(z&127);
 if(t->underlay){
  if(t->underlay->northeastColor==12345678||(fx+fz<=128.f&&t->underlay->southwestColor==12345678))return 0;
  float sw=w->levelHeightmaps[h][tx][tz],se=w->levelHeightmaps[h][tx+1][tz],ne=w->levelHeightmaps[h][tx+1][tz+1],nw=w->levelHeightmaps[h][tx][tz+1];
  *out=fx+fz<=128.f?sw+(se-sw)*fx/128.f+(nw-sw)*fz/128.f:ne+(nw-ne)*(128.f-fx)/128.f+(se-ne)*(128.f-fz)/128.f;
  return 1;
 }
 TileOverlay*o=t->overlay;if(!o)return 0;int found=0;float best=0;
 for(int j=0;j<o->triangleCount;j++){
  if(o->triangleColorA[j]==12345678&&(!o->triangleTextureIds||o->triangleTextureIds[j]<0))continue;
  int a=o->triangleVertexA[j],b=o->triangleVertexB[j],c=o->triangleVertexC[j];
  float ax=o->vertexX[a],az=o->vertexZ[a],bx=o->vertexX[b],bz=o->vertexZ[b],cx=o->vertexX[c],cz=o->vertexZ[c];
  float d=(bz-cz)*(ax-cx)+(cx-bx)*(az-cz);if(fabsf(d)<.0001f)continue;
  float u=((bz-cz)*(x-cx)+(cx-bx)*(z-cz))/d,v=((cz-az)*(x-cx)+(ax-cx)*(z-cz))/d,q=1.f-u-v;
  if(u<-.0001f||v<-.0001f||q<-.0001f)continue;
  float y=u*o->vertexY[a]+v*o->vertexY[b]+q*o->vertexY[c];
  if(!found||y<best)best=y;found=1;
 }
 if(found)*out=best;return found;
}
static int player_ground_lift(World3D*w,Location*loc,Model*m,int cycle){
#if defined(GL11) && defined(__vita__)
 if(!m||m->vertex_count<=0||!loc->entity||!loc->entity->type||strcmp(loc->entity->type,"player")||((unsigned)loc->bitset>>14)!=2047u)return 0;
 player_contact_owner=loc->entity;player_contact_offset=0;
 PlayerEntity*p=(PlayerEntity*)loc->entity;
 if(p->pathing_entity.spotanimId!=-1||(p->locModel&&cycle>=p->locStartCycle&&cycle<p->locStopCycle))return 0;
 int low=m->vertices_y[0];for(int i=1;i<m->vertex_count;i++)if(m->vertices_y[i]>low)low=m->vertices_y[i];
 /* Do not reinterpret large pose offsets (e.g. cutscene/attachment models)
  * as ordinary standing/walking sole contact. */
 if(low>32||low< -32)return 0;
 int yaw=(2048-loc->yaw)&2047,sin=_Pix3D.sin_table[yaw],cos=_Pix3D.cos_table[yaw];
 float penetration=-1.f;int contacts=0;
 for(int i=0;i<m->vertex_count;i++)if(m->vertices_y[i]>=low-24){
  int x=loc->x+((m->vertices_z[i]*sin+m->vertices_x[i]*cos)>>16),z=loc->z+((m->vertices_z[i]*cos-m->vertices_x[i]*sin)>>16);float y;
  if(!player_surface_y(w,loc->level,x,z,&y))continue;
  float depth=loc->y+m->vertices_y[i]-y;if(!contacts||depth>penetration)penetration=depth;contacts++;
 }
 if(!contacts||penetration<-.5f||penetration>32.f)return 0;
 player_contact_offset=-(int)ceilf(penetration+.5f);return player_contact_offset;
#else
 (void)w;(void)loc;(void)m;(void)cycle;return 0;
#endif
}
