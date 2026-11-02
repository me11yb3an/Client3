/* Bounded, explicitly enabled geometry observation. No pose or height writes. */
static void actor_contact_probe(World3D*world,Location*loc,Model*m,int loop_cycle,int visual_lift){
#if defined(__vita__) && defined(GL11)
 static int enabled=-1,samples;static uint64_t next;
 if(enabled<0){enabled=0;FILE*f=fopen("ux0:/data/RS2Vita/actor-contact.txt","r");if(f){fscanf(f,"%d",&enabled);fclose(f);}}
 if(!enabled||samples>=160||!loc->entity||!loc->entity->type||strcmp(loc->entity->type,"player")||((unsigned)loc->bitset>>14)!=2047u||!m||m->vertex_count<=0)return;
 uint64_t now=ba_timing_now();if(now<next)return;next=now+250000u;
 FILE*f=fopen("ux0:/data/RS2Vita/actor-contact.csv",samples?"a":"w");if(!f)return;
 PathingEntity*p=(PathingEntity*)loc->entity;int low=m->vertices_y[0],high=low;
 for(int i=1;i<m->vertex_count;i++){if(m->vertices_y[i]<low)low=m->vertices_y[i];if(m->vertices_y[i]>high)high=m->vertices_y[i];}
 fprintf(f,"actor,%d,%llu,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",samples,(unsigned long long)now,loop_cycle,loc->level,loc->x,loc->y,loc->z,loc->yaw,p->secondarySeqId,p->secondarySeqFrame,low,high,m->vertex_count,visual_lift);
 int yaw=(2048-loc->yaw)&2047,sin=_Pix3D.sin_table[yaw],cos=_Pix3D.cos_table[yaw];
 for(int i=0;i<m->vertex_count;i++)if(m->vertices_y[i]>=high-24){
  int xx=loc->x+((m->vertices_z[i]*sin+m->vertices_x[i]*cos)>>16),zz=loc->z+((m->vertices_z[i]*cos-m->vertices_x[i]*sin)>>16),tx=xx>>7,tz=zz>>7;
  if(tx<0||tz<0||tx>=world->maxTileX||tz>=world->maxTileZ)continue;
  Ground*t=world->levelTiles[loc->level][tx][tz];int lev=t?t->occludeLevel:loc->level;
  fprintf(f,"foot,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",samples,i,xx,loc->y+m->vertices_y[i],zz,tx,tz,lev,world->levelHeightmaps[lev][tx][tz],world->levelHeightmaps[lev][tx+1][tz],world->levelHeightmaps[lev][tx+1][tz+1],world->levelHeightmaps[lev][tx][tz+1]);
  if(t&&t->overlay&&!t->underlay){TileOverlay*o=t->overlay;for(int j=0;j<o->triangleCount;j++){
   int ids[]={o->triangleVertexA[j],o->triangleVertexB[j],o->triangleVertexC[j]};fprintf(f,"surface,%d,%d,%d",samples,i,j);for(int k=0;k<3;k++){int a=ids[k];fprintf(f,",%d,%d,%d",o->vertexX[a],o->vertexY[a],o->vertexZ[a]);}fputc('\n',f);
  }}
 }
 fclose(f);samples++;
#else
 (void)world;(void)loc;(void)m;(void)loop_cycle;(void)visual_lift;
#endif
}
