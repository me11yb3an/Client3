#include <stdio.h>
#include "ba_hw_r6.h"
#include "ba_hw_r5.h"
#include <stdint.h>
static int ui_policy=2,msaa=0,ring_mib=64,immediate_mib=8;
static struct {unsigned vertices,indices,vc,ic;uint64_t bytes,msaa[3];} geometry[2];
void h6_geometry(unsigned v,unsigned i,unsigned vc,unsigned ic){unsigned phase=h5_measuring()?1:0;if(v>geometry[phase].vertices)geometry[phase].vertices=v;if(i>geometry[phase].indices)geometry[phase].indices=i;uint64_t bytes=(uint64_t)v*24+(uint64_t)i*4;if(bytes>geometry[phase].bytes)geometry[phase].bytes=bytes;geometry[phase].vc=vc;geometry[phase].ic=ic;}
void h6_fragment_msaa(unsigned mode){if(mode<3)__sync_fetch_and_add(&geometry[h5_measuring()?1:0].msaa[mode],1);}
void h6_init(void){
 int benchmark=0;
 FILE*bench=fopen("ux0:/data/RS2Vita/hardware-benchmark.txt","r");
 if(bench){int enabled=0,logout=0;if(fscanf(bench,"%d %d",&enabled,&logout)>=1&&enabled)benchmark=1;fclose(bench);}
 /* A normal game ignores leftover experiment files. Those were forcing 4x
  * MSAA and a full-screen UI upload on the hardware that measured 8 fps. */
 if(!benchmark){ui_policy=2;msaa=0;return;}
 FILE*f=fopen("ux0:/data/RS2Vita/r6-experiment.txt","r");
 if(f){int u,m,r,i;if(fscanf(f,"%d %d %d %d",&u,&m,&r,&i)==4&&u>=0&&u<=2&&(m==0||m==4)&&(r==6||r==24||r==64)&&(i==1||i==8)){ui_policy=u;msaa=m;ring_mib=r;immediate_mib=i;}fclose(f);}
 FILE*launch=fopen("ux0:/data/RS2Vita/launch-preset.txt","r");if(launch){unsigned value;char extra;if(fscanf(launch,"%u %c",&value,&extra)==1&&value<=1)msaa=value?0:4;fclose(launch);}
}
int h6_ui_policy(void){return __atomic_load_n(&ui_policy,__ATOMIC_ACQUIRE);}
int h8_set_ui(unsigned value){if(value!=0&&value!=2)return 0;__atomic_store_n(&ui_policy,value,__ATOMIC_RELEASE);return 1;}
int h6_msaa(void){return msaa;}
int h6_ring_mib(void){return ring_mib;}
int h6_immediate_mib(void){return immediate_mib;}
void h6_finish(void){
 h6_pixmap_boundary(0);
 FILE*f=fopen("ux0:/data/RS2Vita/r6-configuration.csv","w");if(f){fprintf(f,"ui_policy,msaa,circular_mib,immediate_mib\n%d,%d,%d,%d\n",ui_policy,msaa,ring_mib,immediate_mib);fclose(f);}
 f=fopen("ux0:/data/RS2Vita/r6-graphics.csv","w");if(f){fprintf(f,"phase,peak_vertices,peak_indices,peak_source_geometry_bytes,vertex_capacity,index_capacity,fragment_patches_msaa_none,fragment_patches_msaa2,fragment_patches_msaa4\n");for(unsigned p=0;p<2;p++)fprintf(f,"%s,%u,%u,%llu,%u,%u,%llu,%llu,%llu\n",p?"measured":"startup_warmup",geometry[p].vertices,geometry[p].indices,(unsigned long long)geometry[p].bytes,geometry[p].vc,geometry[p].ic,(unsigned long long)geometry[p].msaa[0],(unsigned long long)geometry[p].msaa[1],(unsigned long long)geometry[p].msaa[2]);fclose(f);}
}
