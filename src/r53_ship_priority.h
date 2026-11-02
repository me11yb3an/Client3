#pragma once
/* Positive-area coplanar backing, evaluated near the emitted face so a
 * distant scene origin cannot turn merely touching edges into coverage. */
static int r53_backed(const Vertex*t,const float q[3][3]){
 double a[3],b[3],n[3];float p[3][3];
 const float v[3][3]={{t[0].x,t[0].y,t[0].z},{t[1].x,t[1].y,t[1].z},{t[2].x,t[2].y,t[2].z}};
 for(int k=0;k<3;k++){a[k]=(double)v[1][k]-v[0][k];b[k]=(double)v[2][k]-v[0][k];for(int i=0;i<3;i++)p[i][k]=q[i][k]-v[0][k];}
 n[0]=a[1]*b[2]-a[2]*b[1];n[1]=a[2]*b[0]-a[0]*b[2];n[2]=a[0]*b[1]-a[1]*b[0];
 double magnitude=fabs(n[0])+fabs(n[1])+fabs(n[2]);if(magnitude<.01)return 0;
 for(int i=0;i<3;i++)if(fabs(n[0]*p[i][0]+n[1]*p[i][1]+n[2]*p[i][2])>.0001*magnitude)return 0;
 int drop=0;for(int k=1;k<3;k++)if(fabs(n[k])>fabs(n[drop]))drop=k;int u=(drop+1)%3,w=(drop+2)%3;
 R16Point2 subject[3],back[3];for(int i=0;i<3;i++){subject[i]=(R16Point2){v[i][u]-v[0][u],v[i][w]-v[0][w]};back[i]=(R16Point2){p[i][u],p[i][w]};}
 return r16_triangle_overlap(subject,back)>.01f;
}
