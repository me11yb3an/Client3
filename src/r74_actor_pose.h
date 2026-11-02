#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "r57_body_layers.h"
/* Exact copies of every input used by model emission and clothing replay.
 * No source pointer, allocator address or partial vertex hash proves a hit.
 * CPU copies share the existing six-MiB cache budget with their GPU buffers. */
typedef struct {
 size_t bytes;
 int vertices,faces,textures,priority;
 uint32_t present;
 unsigned pairs;
 unsigned char data[];
} R74Pose;
static void r74_arrays(const Model*m,const int**a,int*n){
 const int*p[]={m->vertices_x,m->vertices_y,m->vertices_z,m->face_indices_a,m->face_indices_b,m->face_indices_c,m->face_color_a,m->face_color_b,m->face_color_c,m->face_infos,m->face_priorities,m->face_alphas,m->face_colors,m->textured_p_coordinate,m->textured_m_coordinate,m->textured_n_coordinate};
 for(int i=0;i<16;i++){a[i]=p[i];n[i]=i<3?m->vertex_count:i<13?m->face_count:m->textured_face_count;}
}
static R74Pose*r74_pose_new(const Model*m){
 if(!m||m->vertex_count<0||m->vertex_count>20000||m->face_count<1||m->face_count>20000||m->textured_face_count<0||m->textured_face_count>8192)return NULL;
 const int*a[16];int n[16];r74_arrays(m,a,n);size_t bytes=sizeof(R74Pose);uint32_t present=0;
 for(int i=0;i<16;i++)if(a[i]){bytes+=(size_t)n[i]*sizeof(int);present|=1u<<i;}
 unsigned pairs=m->r57_body_layers?m->r57_body_layers->count:0;if(pairs>4096)return NULL;bytes+=(size_t)pairs*sizeof(R57Pair);
 if(bytes>256u*1024u)return NULL;
 R74Pose*k=malloc(bytes);if(!k)return NULL;
 k->bytes=bytes;k->vertices=m->vertex_count;k->faces=m->face_count;k->textures=m->textured_face_count;k->priority=m->model_priority;k->present=present;k->pairs=pairs;
 unsigned char*out=k->data;
 for(int i=0;i<16;i++)if(a[i]){size_t b=(size_t)n[i]*sizeof(int);memcpy(out,a[i],b);out+=b;}
 if(pairs)memcpy(out,m->r57_body_layers->pairs,(size_t)pairs*sizeof(R57Pair));return k;
}
static int r74_pose_equal(const R74Pose*k,const Model*m){
 if(!k||!m||k->vertices!=m->vertex_count||k->faces!=m->face_count||k->textures!=m->textured_face_count||k->priority!=m->model_priority)return 0;
 const int*a[16];int n[16];r74_arrays(m,a,n);uint32_t present=0;for(int i=0;i<16;i++)if(a[i])present|=1u<<i;
 unsigned pairs=m->r57_body_layers?m->r57_body_layers->count:0;if(k->present!=present||k->pairs!=pairs)return 0;
 const unsigned char*in=k->data;
 for(int i=0;i<16;i++)if(a[i]){size_t b=(size_t)n[i]*sizeof(int);if(memcmp(in,a[i],b))return 0;in+=b;}
 return !pairs||!memcmp(in,m->r57_body_layers->pairs,(size_t)pairs*sizeof(R57Pair));
}
static void r74_pose_cleanup(R74Pose**p){free(*p);}
