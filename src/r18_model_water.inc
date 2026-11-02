/* The atlas packs materials along U; scrolling is V only. Extract only
 * opaque 17/24 triangles into the existing retained scrolling-water pass. */
static int model_water_material(Vertex*a,Vertex*b,Vertex*c){
 if(a->a!=255||b->a!=255||c->a!=255)return 0;
 /* The overwhelming majority of scenery uses white/untextured or other
  * atlas slots. Reject those without three library floor operations. These
  * binary-exact bounds are precisely the original18/25 slot intervals. */
 if(a->u<18.f/64.f||a->u>=26.f/64.f||(a->u>=19.f/64.f&&a->u<25.f/64.f))return 0;
 int x=(int)floorf(a->u*64.f),y=(int)floorf(b->u*64.f),z=(int)floorf(c->u*64.f);
 return x==y&&x==z&&(x==18||x==25);
}
static int take_model_water(int first_vertex,int first_element){
 int end=elementcount,write=first_element;
 if(first_vertex<0||first_element<0||first_element>end||(end-first_element)%3)return 0;
 for(int i=first_element;i<end;i+=3){
  uint32_t a=indices[i],b=indices[i+1],c=indices[i+2];
  if(a<(unsigned)first_vertex||b<(unsigned)first_vertex||c<(unsigned)first_vertex||a>=(unsigned)vertcount||b>=(unsigned)vertcount||c>=(unsigned)vertcount)return 0;
  if(!model_water_material(&verts[a],&verts[b],&verts[c])){
   if(write!=i){indices[write]=a;indices[write+1]=b;indices[write+2]=c;}write+=3;continue;
  }
  if(water_vn+3>R16_CHUNK_VERTS||water_in+3>R16_CHUNK_VERTS)if(!water_flush())return 0;
  if(!water_cpu)water_cpu=malloc((size_t)R16_CHUNK_VERTS*sizeof(Vertex));
  if(!water_cpu_idx)water_cpu_idx=malloc((size_t)R16_CHUNK_VERTS*sizeof(uint16_t));
  if(!water_cpu||!water_cpu_idx)return 0;
  uint32_t ids[]={a,b,c};
  for(int j=0;j<3;j++){
   water_cpu[water_vn]=verts[ids[j]];
#ifdef R18_WATER_TAG_COPY
   R18_WATER_TAG_COPY(water_vn,ids[j]);
#endif
   water_cpu_idx[water_in++]=(uint16_t)water_vn++;
  }
 }
 elementcount=write;return 1;
}
