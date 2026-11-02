#ifndef R98_CAPE_FIT_H
#define R98_CAPE_FIT_H
#include <math.h>
/* Fit only the exterior rear of the fire cape to the actual worn torso/legs,
 * once when the assembled equipment cache entry is built. The collar's front
 * attachment, skin labels, texture mapping and other equipment remain intact. */
static int r98_rear_at(const Model*m,int px,int py,double*out) {
    if(!m||m->face_count<0||m->face_count>16384||!m->vertices_x||!m->vertices_y||!m->vertices_z)return 0;
    int found=0;double rear=-1e30;
    for(int f=0;f<m->face_count;f++){
        if((m->face_infos&&m->face_infos[f]==-1)||(m->face_alphas&&m->face_alphas[f]))continue;
        int a=m->face_indices_a[f],b=m->face_indices_b[f],c=m->face_indices_c[f];
        if(a<0||b<0||c<0||a>=m->vertex_count||b>=m->vertex_count||c>=m->vertex_count)continue;
        double ax=m->vertices_x[a],bx=m->vertices_x[b],cx=m->vertices_x[c];
        double ay=m->vertices_y[a],by=m->vertices_y[b],cy=m->vertices_y[c];
        double den=(by-cy)*(ax-cx)+(cx-bx)*(ay-cy);if(fabs(den)<1e-9)continue;
        double u=((by-cy)*(px-cx)+(cx-bx)*(py-cy))/den;
        double v=((cy-ay)*(px-cx)+(ax-cx)*(py-cy))/den,w=1-u-v;
        if(u< -1e-7||v< -1e-7||w< -1e-7)continue;
        double z=u*m->vertices_z[a]+v*m->vertices_z[b]+w*m->vertices_z[c];
        if(!found||z>rear)rear=z;found=1;
    }
    if(found)*out=rear;return found;
}
static int r98_fit_cape(Model*cape,const Model*torso,const Model*legs) {
    if(!cape||cape->vertex_count<0||cape->vertex_count>256||!cape->vertices_x||!cape->vertices_y||!cape->vertices_z)return 0;
    int changed=0;
    for(int i=0;i<cape->vertex_count;i++){
        if(cape->vertices_z[i]<0)continue;
        double rear=-1e30,z;int found=0;
        if(r98_rear_at(torso,cape->vertices_x[i],cape->vertices_y[i],&z)){rear=z;found=1;}
        if(r98_rear_at(legs,cape->vertices_x[i],cape->vertices_y[i],&z)){if(!found||z>rear)rear=z;found=1;}
        if(found){int target=(int)ceil(rear+2.0);if(target>cape->vertices_z[i]){cape->vertices_z[i]=target;changed++;}}
    }
    return changed;
}
#endif
