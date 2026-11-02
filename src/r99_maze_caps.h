#ifndef R99_MAZE_CAPS_H
#define R99_MAZE_CAPS_H
#include <string.h>
#include "allocator.h"
static int r99_door_type(int id){return (id>=3851&&id<=3882)||(id>=3907&&id<4035);}
/* The recovered leaf has two six-edge holes on its upper trim. Close only
 * those measured boundaries before mirroring/lighting. New flat faces leave
 * the original vertex normals, all existing faces and texture data intact. */
static int*r99_extend(const int*old,int n,int fill){
    int*p=rs2_malloc(true,(n+8)*(int)sizeof(int));if(!p)return NULL;
    if(old)memcpy(p,old,n*sizeof(int));else memset(p,0,n*sizeof(int));
    for(int i=n;i<n+8;i++)p[i]=fill;return p;
}
static int r99_close_door_trim(Model*m,int id,int model_id,int shape){
    if(!m||!r99_door_type(id)||model_id!=4178||shape!=0||m->vertex_count!=358||m->face_count!=498||m->face_color_a||m->vertex_normal)return 0;
    static const int points[12][3]={
        {-62,-232,-47},{-62,-244,29},{-62,-245,54},{-58,-232,-47},{-58,-244,29},{-58,-245,54},
        {-44,-232,-47},{-44,-244,29},{-44,-245,54},{-40,-232,-47},{-40,-244,29},{-40,-245,54}};
    int v[12];for(int i=0;i<12;i++){v[i]=-1;for(int j=0;j<m->vertex_count;j++)if(m->vertices_x[j]==points[i][0]&&m->vertices_y[j]==points[i][1]&&m->vertices_z[j]==points[i][2]){v[i]=j;break;}if(v[i]<0)return 0;}
    static const int triangles[8][3]={{0,4,1},{0,3,4},{1,5,2},{1,4,5},{6,10,7},{6,9,10},{7,11,8},{7,10,11}};
    int n=m->face_count;
    int*a=r99_extend(m->face_indices_a,n,0),*b=r99_extend(m->face_indices_b,n,0),*c=r99_extend(m->face_indices_c,n,0),*colors=r99_extend(m->face_colors,n,10508),*infos=r99_extend(m->face_infos,n,1);
    int*priorities=m->face_priorities?r99_extend(m->face_priorities,n,0):NULL;
    int*alphas=m->face_alphas?r99_extend(m->face_alphas,n,0):NULL;
    int*labels=m->face_labels?r99_extend(m->face_labels,n,0):NULL;
    if(!a||!b||!c||!colors||!infos||(m->face_priorities&&!priorities)||(m->face_alphas&&!alphas)||(m->face_labels&&!labels))return 0;
    for(int i=0;i<8;i++){a[n+i]=v[triangles[i][0]];b[n+i]=v[triangles[i][1]];c[n+i]=v[triangles[i][2]];}
    m->face_indices_a=a;m->face_indices_b=b;m->face_indices_c=c;m->face_colors=colors;m->face_infos=infos;
    m->face_priorities=priorities;m->face_alphas=alphas;m->face_labels=labels;m->face_count=n+8;
    return 8;
}
#endif
