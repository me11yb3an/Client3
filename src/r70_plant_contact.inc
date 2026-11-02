/* Waterlily leaves are authored on the water plane. A confirmed contact
 * receives the same two-unit clearance already used for ground decoration.
 * Move the complete emitted flower/leaf assembly together, preserving its
 * geometry, alpha cutouts, UVs, picking model and every gameplay property. */
static unsigned r70_plant_contacts;
static void r70_plant_contact(Model*m,int bitset,int first){
 int id=((unsigned)bitset>>14)&32767;
 if(!baking||!trim_world||!m||(id!=1175&&id!=1176&&id!=1177)||trim_source_plane<0||trim_source_plane>3||first<0||first>=vertcount)return;
 int visible=0;for(int f=0;f<m->face_count;f++)if(!m->face_infos||m->face_infos[f]!=-1)visible++;
 if(vertcount-first!=visible*3)return;
 int v=first,contact=0;
 for(int f=0;f<m->face_count&&!contact;f++){
  if(m->face_infos&&m->face_infos[f]==-1)continue;Vertex*t=verts+v;v+=3;
  if((m->face_alphas&&m->face_alphas[f])||t[0].a!=255||t[1].a!=255||t[2].a!=255)continue;
  float xl=fminf(t[0].x,fminf(t[1].x,t[2].x)),xh=fmaxf(t[0].x,fmaxf(t[1].x,t[2].x)),zl=fminf(t[0].z,fminf(t[1].z,t[2].z)),zh=fmaxf(t[0].z,fmaxf(t[1].z,t[2].z));
  if(xh-xl>256||zh-zl>256)continue;
  for(int level=trim_world->minLevel;level<trim_world->maxLevel&&!contact;level++)
  for(int x=(int)floorf(xl/128)-1;x<=(int)floorf(xh/128)+1&&!contact;x++)for(int z=(int)floorf(zl/128)-1;z<=(int)floorf(zh/128)+1&&!contact;z++){
   if(x<0||z<0||x>=trim_world->maxTileX||z>=trim_world->maxTileZ)continue;
   Ground*g=trim_world->levelTiles[level][x][z];if(!g)continue;
   contact=r55_terrain_face(t,g,x,z)||r55_terrain_face(t,g->bridge,x,z);
  }
 }
 if(contact){for(int k=first;k<vertcount;k++)verts[k].y-=2.f;r70_plant_contacts++;}
}
