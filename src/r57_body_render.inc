/* Pose-local replay indices reuse the actor's original vertex storage. The
 * uncached fallback is bounded and drawn after every opaque base mesh. */
#define R57_CPU_VERTS 8190
static uint32_t r57_pending[8192*3];
static int r57_pending_count,r57_pending_counts[10];
static Vertex r57_cpu[R57_CPU_VERTS];
static uint8_t r57_cpu_priority[R57_CPU_VERTS/3];
static uint16_t r57_cpu_indices[R57_CPU_VERTS];
static int r57_cpu_count;
static GLuint r57_vbo,r57_ibo;
static int r57_gpu_attempted;
static void r57_prepare_span_layout(Model*m,int v0,int e0,int indexed){
 static unsigned char selected[8192];
 r57_pending_count=0;memset(r57_pending_counts,0,sizeof r57_pending_counts);
 if(!m||vertcount<=v0||elementcount<=e0||!m->r57_body_layers||m->face_count>8192||!r57_select_body_layers(m,selected,8192))return;
 int visible=0,counts[10]={0};
 for(int f=0;f<m->face_count;f++)if(!m->face_infos||m->face_infos[f]!=-1){visible++;int layer=selected[f];if(layer>=1&&layer<=9)counts[layer]+=3;}
 if(elementcount-e0!=visible*3||vertcount<=v0||(indexed?vertcount-v0>visible*3:vertcount-v0!=visible*3))return;
 for(int i=0;i<visible*3;i++)if(indexed?(indices[e0+i]<(uint32_t)v0||indices[e0+i]>=(uint32_t)vertcount):indices[e0+i]!=(uint32_t)(v0+i))return;
 /* Stable counting scatter keeps the original per-layer face order while
  * avoiding nine complete walks of every actor's face list. */
 int cursor[10]={0},total=0;for(int layer=1;layer<=9;layer++){cursor[layer]=total;total+=counts[layer];}
 int at=0;for(int f=0;f<m->face_count;f++){
  if(m->face_infos&&m->face_infos[f]==-1)continue;
  int layer=selected[f];if(layer>=1&&layer<=9){int first=cursor[layer];for(int k=0;k<3;k++)r57_pending[first+k]=indexed?indices[e0+at+k]:(uint32_t)(v0+at+k);cursor[layer]+=3;}
  at+=3;
 }
 r57_pending_count=total;memcpy(r57_pending_counts,counts,sizeof counts);
}
static void r57_prepare_span(Model*m,int v0,int e0){r57_prepare_span_layout(m,v0,e0,0);}
static void r57_prepare_indexed_span(Model*m,int v0,int e0){r57_prepare_span_layout(m,v0,e0,1);}

static void r57_fallback_span(void){
 int at=0;for(int layer=1;layer<=9;layer++)for(int n=0;n<r57_pending_counts[layer];n+=3){
  if(r57_cpu_count+3>R57_CPU_VERTS){r57_pending_count=0;return;}
  for(int k=0;k<3;k++)r57_cpu[r57_cpu_count+k]=verts[r57_pending[at+k]];
  r57_cpu_priority[r57_cpu_count/3]=(uint8_t)layer;r57_cpu_count+=3;at+=3;
 }r57_pending_count=0;
}
static void r57_draw_fallback(void){
 if(!r57_cpu_count)return;
 if(!r57_gpu_attempted){
  r57_gpu_attempted=1;for(int i=0;i<8&&glGetError()!=GL_NO_ERROR;i++){}
  glGenBuffers(1,&r57_vbo);glGenBuffers(1,&r57_ibo);
  if(r57_vbo&&r57_ibo){glBindBuffer(GL_ARRAY_BUFFER,r57_vbo);glBufferData(GL_ARRAY_BUFFER,sizeof r57_cpu,NULL,GL_DYNAMIC_DRAW);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,r57_ibo);glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof r57_cpu_indices,NULL,GL_DYNAMIC_DRAW);}
  if(!r57_vbo||!r57_ibo||glGetError()!=GL_NO_ERROR){if(r57_vbo)glDeleteBuffers(1,&r57_vbo);if(r57_ibo)glDeleteBuffers(1,&r57_ibo);r57_vbo=r57_ibo=0;}
 }
 if(!r57_vbo||!r57_ibo)return;
 int first[10]={0},counts[10]={0},n=0;
 for(int layer=1;layer<=9;layer++){first[layer]=n;for(int t=0;t<r57_cpu_count/3;t++)if(r57_cpu_priority[t]==layer)for(int k=0;k<3;k++)r57_cpu_indices[n++]=(uint16_t)(t*3+k);counts[layer]=n-first[layer];}
 glBindBuffer(GL_ARRAY_BUFFER,r57_vbo);glBufferSubData(GL_ARRAY_BUFFER,0,r57_cpu_count*sizeof(Vertex),r57_cpu);
 glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,r57_ibo);glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,0,n*sizeof(uint16_t),r57_cpu_indices);
 glColorPointer(4,GL_UNSIGNED_BYTE,sizeof(Vertex),(const void*)offsetof(Vertex,r));glVertexPointer(3,GL_FLOAT,sizeof(Vertex),(const void*)offsetof(Vertex,x));glTexCoordPointer(2,GL_FLOAT,sizeof(Vertex),(const void*)offsetof(Vertex,u));
 for(int layer=1;layer<=9;layer++)if(counts[layer]){glPolygonOffset(-(float)layer,-(float)layer);glDrawElements(GL_TRIANGLES,counts[layer],GL_UNSIGNED_SHORT,(const void*)(size_t)(first[layer]*sizeof(uint16_t)));r57_drawn+=counts[layer]/3;}
}
