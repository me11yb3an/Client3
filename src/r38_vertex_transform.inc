/* Preserve the original integer rotation/translation exactly while doing
 * it once per shared source vertex, including textured models. */
#define GL_SHARED_VERTEX_MAX 8192
static int flat_x[GL_SHARED_VERTEX_MAX],flat_y[GL_SHARED_VERTEX_MAX],flat_z[GL_SHARED_VERTEX_MAX];
static uint64_t shared_models,shared_vertices,shared_corners;
void gl_shared_transform_reset(void){shared_models=shared_vertices=shared_corners=0;}
void gl_shared_transform_stats(uint64_t*m,uint64_t*v,uint64_t*c){*m=shared_models;*v=shared_vertices;*c=shared_corners;}
static int gl_transform_shared(Model*model,int sceneX,int sceneY,int sceneZ,int yaw){
 int n=model->vertex_count;if(n<=0||n>GL_SHARED_VERTEX_MAX||!model->vertices_x||!model->vertices_y||!model->vertices_z)return 0;
 int si=_Pix3D.sin_table[(2048-yaw)&2047],co=_Pix3D.cos_table[(2048-yaw)&2047];
 int ox=sceneX+_World3D.eyeX,oy=sceneY+_World3D.eyeY,oz=sceneZ+_World3D.eyeZ;
 for(int i=0;i<n;i++){int x=model->vertices_x[i],z=model->vertices_z[i];flat_x[i]=((x*co-z*si)>>16)+ox;flat_y[i]=model->vertices_y[i]+oy;flat_z[i]=((x*si+z*co)>>16)+oz;}
 shared_models++;shared_vertices+=(unsigned)n;shared_corners+=(uint64_t)(unsigned)model->face_count*3u;return 1;
}
