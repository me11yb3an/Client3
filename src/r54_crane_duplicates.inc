/* Both authored crane meshes repeat four same-facing opaque triangles.
 * Keep the earliest face, and collapse only copies whose current emitted
 * positions/UVs still match it. Opposite-facing rope surfaces are retained. */
static void r54_crane_duplicates(Model*m,int bitset,int first){
 int id=((unsigned)bitset>>14)&32767,offset=id==2521?0:id==2520?80:-1;
 if(offset<0||m->face_count!=(id==2521?499:469)||first<0)return;
 int starts[499],v=first;
 for(int f=0;f<m->face_count;f++){starts[f]=-1;if(m->face_infos&&m->face_infos[f]==-1)continue;starts[f]=v;v+=3;}
 if(v!=vertcount)return;
 const int pairs[4][2]={{489,485},{490,486},{491,485},{497,486}};
 for(int i=0;i<4;i++){
  int f=pairs[i][0]-offset,g=pairs[i][1]-offset;if(starts[f]<0||starts[g]<0||!r26_opaque_face(m,f)||!r26_opaque_face(m,g)||r26_priority(m,f)!=r26_priority(m,g))continue;
  if(m->face_colors&&m->face_colors[f]!=m->face_colors[g])continue;
  if((m->face_infos?m->face_infos[f]:0)!=(m->face_infos?m->face_infos[g]:0))continue;
  Vertex*t=verts+starts[f],*base=verts+starts[g];int equal=0;
  for(int shift=0;shift<3;shift++){
   int match=1;for(int k=0;k<3;k++){Vertex*a=t+k,*b=base+(k+shift)%3;if(a->a!=255||b->a!=255||a->x!=b->x||a->y!=b->y||a->z!=b->z||a->u!=b->u||a->v!=b->v)match=0;}
   if(match){equal=1;break;}
  }
  if(!equal)continue;
  t[1].x=t[2].x=t[0].x;t[1].y=t[2].y=t[0].y;t[1].z=t[2].z=t[0].z;if(baking)r54_duplicates++;
 }
}
