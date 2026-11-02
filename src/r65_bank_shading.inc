/* Scope the software texture brightness bands to bank furniture. Preserve
 * the existing texture, UVs, alpha, geometry and directional shade ordering.
 * This runs during a retained bake, never as a per-frame world scan. */
static void r65_bank_texture_lighting(Model*m,int bitset,int first){
 int id=((unsigned)bitset>>14)&32767;
 if(!baking||!r65_bank_layers_type(id)||first<0||!m->face_infos||!m->face_color_a)return;
 int visible=0;for(int f=0;f<m->face_count;f++)if(m->face_infos[f]!=-1)visible++;
 if(vertcount-first!=visible*3)return;
 int at=first;
 for(int f=0;f<m->face_count;f++){
  int info=m->face_infos[f];if(info==-1)continue;Vertex*t=verts+at;at+=3;
  if(!(info&2)||!r26_opaque_face(m,f))continue;
  if(!(info&1)&&(!m->face_color_b||!m->face_color_c))continue;
  int shade[3]={m->face_color_a[f],(info&1)?m->face_color_a[f]:m->face_color_b[f],(info&1)?m->face_color_a[f]:m->face_color_c[f]};
  int valid=1;
  for(int k=0;k<3;k++){
   if(r65_texture_brightness(shade[k])<0){valid=0;break;}
   int original=255-2*shade[k];
   if(t[k].a!=255||t[k].r!=original||t[k].g!=original||t[k].b!=original)valid=0;
  }
  if(!valid)continue;
  for(int k=0;k<3;k++)t[k].r=t[k].g=t[k].b=(unsigned char)r65_texture_brightness(shade[k]);
 }
}
