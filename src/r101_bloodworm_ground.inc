/* The recovered bloodworm is only ten units high. Its bilinear actor origin
 * can intersect the actual triangular floor while crossing a sloped tile.
 * Fit only this NPC's visible underside; never change the shared mesh,
 * server position, animation, other creatures, or airborne effect models. */
static unsigned r101_samples,r101_adjusted,r101_max_lift,r101_penetrations;
void r101_bloodworm_scene_probe(const NpcEntity*n,int nid,int scene,int cycle,int reason){
#if defined(GL11)&&defined(__vita__)
    static int enabled=-1,rows;static FILE*f;
    if(enabled<0){enabled=0;FILE*flag=fopen("ux0:/data/RS2Vita/bloodworm-probe.txt","r");if(flag){fscanf(flag,"%d",&enabled);fclose(flag);}}
    if(!enabled||rows>=6000)return;
    if(!f){f=fopen("ux0:/data/RS2Vita/bloodworm-scene.csv","w");if(!f)return;fputs("us,scene,cycle,nid,x,z,yaw,reason,primary,primary_frame,secondary,secondary_frame\n",f);}
    const PathingEntity*p=&n->pathing_entity;
    fprintf(f,"%llu,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",(unsigned long long)ba_timing_now(),scene,cycle,nid,p->x,p->z,p->yaw,reason,p->primarySeqId,p->primarySeqFrame,p->secondarySeqId,p->secondarySeqFrame);
    rows++;if((rows&31)==0)fflush(f);if(rows==6000){fclose(f);f=NULL;}
#else
    (void)n;(void)nid;(void)scene;(void)cycle;(void)reason;
#endif
}
void r101_bloodworm_status(unsigned out[4]){out[0]=r101_samples;out[1]=r101_adjusted;out[2]=r101_max_lift;out[3]=r101_penetrations;}
static int r101_bloodworm_lift(World3D*w,Location*loc,Model*m){
#if defined(GL11)&&defined(__vita__)
 if(!w||!loc||!m||m->vertex_count<=0||!loc->entity||!loc->entity->type||strcmp(loc->entity->type,"npc"))return 0;
 NpcEntity*n=(NpcEntity*)loc->entity;
 if(!n->type||n->type->index!=1049||n->pathing_entity.spotanimId!=-1)return 0;
 int low=m->vertices_y[0];for(int i=1;i<m->vertex_count;i++)if(m->vertices_y[i]>low)low=m->vertices_y[i];
 if(low>32||low< -32)return 0;
 int yaw=(2048-loc->yaw)&2047,sine=_Pix3D.sin_table[yaw],cosine=_Pix3D.cos_table[yaw],contacts=0;
 float penetration=0;
 for(int i=0;i<m->vertex_count;i++)if(m->vertices_y[i]>=low-16){
  int x=loc->x+((m->vertices_z[i]*sine+m->vertices_x[i]*cosine)>>16);
  int z=loc->z+((m->vertices_z[i]*cosine-m->vertices_x[i]*sine)>>16);float surface;
  if(!player_surface_y(w,loc->level,x,z,&surface))continue;
  float depth=loc->y+m->vertices_y[i]-surface;
  if(!contacts||depth>penetration)penetration=depth;contacts++;
 }
 if(!contacts)return 0;r101_samples++;if(penetration>0)r101_penetrations++;
 /* Three units of clearance also prevent shallow far-view depth fighting.
  * A large pose/terrain discrepancy is not treated as normal floor contact. */
 int lift=(int)ceilf(penetration+3.f);
 if(lift<=0||lift>32)return 0;
 r101_adjusted++;if((unsigned)lift>r101_max_lift)r101_max_lift=(unsigned)lift;
 return -lift;
#else
 (void)w;(void)loc;(void)m;return 0;
#endif
}

/* Opt-in, buffered and bounded; no effect on normal gameplay. Observe the
 * actual posed actor and emitted index count to separate floor contact,
 * animation collapse, frustum rejection and missing scene submissions. */
static void r101_bloodworm_emit_probe(Location*loc,Model*m,int cycle,int draw_y,int nv,int ni){
#if defined(GL11)&&defined(__vita__)
    static int enabled=-1,rows;static FILE*f;
    if(enabled<0){enabled=0;FILE*flag=fopen("ux0:/data/RS2Vita/bloodworm-probe.txt","r");if(flag){fscanf(flag,"%d",&enabled);fclose(flag);}}
    if(!enabled||rows>=6000||!loc||!m||!loc->entity||!loc->entity->type||strcmp(loc->entity->type,"npc"))return;
    NpcEntity*n=(NpcEntity*)loc->entity;if(!n->type||n->type->index!=1049)return;
    if(!f){f=fopen("ux0:/data/RS2Vita/bloodworm-emission.csv","w");if(!f)return;fputs("us,cycle,nid,x,y,z,yaw,visual_lift,primary,primary_frame,secondary,secondary_frame,vertices,faces,radius,max_y,min_y,emitted_vertices,emitted_indices,hidden_faces,max_alpha\n",f);}
    int hidden=0,alpha=0;for(int i=0;i<m->face_count;i++){if(m->face_infos&&m->face_infos[i]==-1)hidden++;if(m->face_alphas&&m->face_alphas[i]>alpha)alpha=m->face_alphas[i];}
    PathingEntity*p=&n->pathing_entity;
    fprintf(f,"%llu,%d,%u,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",(unsigned long long)ba_timing_now(),cycle,((unsigned)loc->bitset>>14)&32767,loc->x,loc->y,loc->z,loc->yaw,draw_y-loc->y,p->primarySeqId,p->primarySeqFrame,p->secondarySeqId,p->secondarySeqFrame,m->vertex_count,m->face_count,m->radius,m->max_y,m->min_y,nv,ni,hidden,alpha);
    rows++;if((rows&31)==0)fflush(f);if(rows==6000){fclose(f);f=NULL;}
#else
    (void)loc;(void)m;(void)cycle;(void)draw_y;(void)nv;(void)ni;
#endif
}
