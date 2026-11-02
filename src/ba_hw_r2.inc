#include "r12_waits.h"
/* Main-thread summary only. Wrappers retain original arguments and return values. */
#include <psp2/kernel/threadmgr.h>
#include <string.h>
static SceUID hw_threads[32];
static volatile unsigned hw_thread_count;
SceUID __real_sceKernelCreateThread(const char*,SceKernelThreadEntry,int,SceSize,SceUInt,int,const SceKernelThreadOptParam*);
SceUID __wrap_sceKernelCreateThread(const char*n,SceKernelThreadEntry e,int p,SceSize s,SceUInt a,int c,const SceKernelThreadOptParam*o){
 SceUID id=__real_sceKernelCreateThread(n,e,p,s,a,c,o);
 if(id>=0){unsigned i=__sync_fetch_and_add(&hw_thread_count,1);if(i<32)hw_threads[i]=id;}
 return id;
}
static uint64_t hw_draw_calls,hw_upload_calls,hw_upload_bytes,hw_textures_created,hw_textures_deleted;
#ifdef GL11
void __real_glGenTextures(GLsizei,GLuint*);
void __wrap_glGenTextures(GLsizei n,GLuint*p){__real_glGenTextures(n,p);if(n>0)hw_textures_created+=n;}
void __real_glDeleteTextures(GLsizei,const GLuint*);
void __wrap_glDeleteTextures(GLsizei n,const GLuint*p){__real_glDeleteTextures(n,p);if(n>0)hw_textures_deleted+=n;}
void __real_glTexSubImage2D(GLenum,GLint,GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,const void*);
void __wrap_glTexSubImage2D(GLenum t,GLint l,GLint x,GLint y,GLsizei w,GLsizei h,GLenum f,GLenum ty,const void*p){
 hw_upload_calls++;if(w>0&&h>0)hw_upload_bytes+=(uint64_t)w*h*4;
 if(!h5_graph(0))__real_glTexSubImage2D(t,l,x,y,w,h,f,ty,p);
}
void __real_glDrawElements(GLenum,GLsizei,GLenum,const void*);
void __wrap_glDrawElements(GLenum m,GLsizei n,GLenum t,const void*p){hw_draw_calls++;if(n>0){h3_ops[4]+=n;h4_graph(0,n);}if(!h5_graph(0))__real_glDrawElements(m,n,t,p);}
extern uint8_t *circular_data_pool[];
extern uint32_t circular_data_pool_size;
extern uint8_t gxm_display_buffer_count;
extern int vgl_circular_idx;
extern uint8_t *circular_data_pool_limit[];
extern uint8_t *circular_data_pool_ptr[];
static uint64_t hw_ring_fallbacks,hw_ring_fallback_bytes;
static struct {uint64_t calls,max_request,peak_advance,capacity,fallbacks,fallback_bytes,nulls;} h6_ring[2];
void *__real_vgl_reserve_data_pool(uint32_t);
void *__wrap_vgl_reserve_data_pool(uint32_t n){
 void*p=__real_vgl_reserve_data_pool(n);int i=vgl_circular_idx;
 unsigned phase=h5_measuring()?1:0;h6_ring[phase].calls++;if(n>h6_ring[phase].max_request)h6_ring[phase].max_request=n;
 if(!p)h6_ring[phase].nulls++;
 if(i>=0&&i<gxm_display_buffer_count&&i<5){
  uintptr_t base=(uintptr_t)circular_data_pool[i],limit=(uintptr_t)circular_data_pool_limit[i],ptr=(uintptr_t)circular_data_pool_ptr[i],result=(uintptr_t)p;
  if(limit>=base)h6_ring[phase].capacity=limit-base;
  if(ptr>=base&&ptr-base>h6_ring[phase].peak_advance)h6_ring[phase].peak_advance=ptr-base;
  if(p&&(result<base||result>limit||n>limit-result)){hw_ring_fallbacks++;hw_ring_fallback_bytes+=n;h6_ring[phase].fallbacks++;h6_ring[phase].fallback_bytes+=n;}
 }
 return p;
}
static unsigned hw_gxm[4][4];
static void hw_gxm_measure(unsigned stage,uint64_t start,int rc){
 unsigned us=(unsigned)(ba_timing_now()-start),old;
 __sync_fetch_and_add(&hw_gxm[stage][0],1);__sync_fetch_and_add(&hw_gxm[stage][1],us);
 do{old=hw_gxm[stage][2];if(old>=us)break;}while(!__sync_bool_compare_and_swap(&hw_gxm[stage][2],old,us));
 if(rc<0)__sync_fetch_and_add(&hw_gxm[stage][3],1);
}
int __real_sceGxmBeginScene(SceGxmContext*,unsigned,const SceGxmRenderTarget*,const SceGxmValidRegion*,SceGxmSyncObject*,SceGxmSyncObject*,const SceGxmColorSurface*,const SceGxmDepthStencilSurface*);
int __wrap_sceGxmBeginScene(SceGxmContext*c,unsigned f,const SceGxmRenderTarget*r,const SceGxmValidRegion*v,SceGxmSyncObject*a,SceGxmSyncObject*b,const SceGxmColorSurface*s,const SceGxmDepthStencilSurface*d){uint64_t t=ba_timing_now();if(h5_graph(1))return -1;int rc=__real_sceGxmBeginScene(c,f,r,v,a,b,s,d);hw_gxm_measure(0,t,rc);return rc;}
int __real_sceGxmEndScene(SceGxmContext*,const SceGxmNotification*,const SceGxmNotification*);
int __wrap_sceGxmEndScene(SceGxmContext*c,const SceGxmNotification*a,const SceGxmNotification*b){uint64_t t=ba_timing_now();if(h5_graph(1))return -1;int rc=__real_sceGxmEndScene(c,a,b);r12_wait_record(3,ba_timing_now()-t);hw_gxm_measure(1,t,rc);return rc;}
int __real_sceGxmDraw(SceGxmContext*,SceGxmPrimitiveType,SceGxmIndexFormat,const void*,unsigned);
int __wrap_sceGxmDraw(SceGxmContext*c,SceGxmPrimitiveType p,SceGxmIndexFormat f,const void*i,unsigned n){uint64_t t=ba_timing_now();if(h5_graph(1))return -1;int rc=__real_sceGxmDraw(c,p,f,i,n);hw_gxm_measure(2,t,rc);return rc;}
void __real_sceGxmFinish(SceGxmContext*);
void __wrap_sceGxmFinish(SceGxmContext*c){uint64_t t=ba_timing_now();__real_sceGxmFinish(c);r12_wait_record(1,ba_timing_now()-t);hw_gxm_measure(3,t,0);}
#endif
static void hw_r2_summary(uint64_t now,unsigned serial){
 FILE*f=fopen("ux0:/data/RS2Vita/hw-r2-threads.csv",serial?"a":"w");
 if(f){
  if(!serial)fprintf(f,"time_us,id,name,rc,priority,affinity,current_cpu,last_cpu,stack_bytes,stack_free,run_clocks\n");
  unsigned n=hw_thread_count;if(n>32)n=32;
  for(unsigned i=0;i<=n;i++){
   SceUID id=i?hw_threads[i-1]:sceKernelGetThreadId();SceKernelThreadInfo t={0};t.size=sizeof(t);
   int rc=sceKernelGetThreadInfo(id,&t);
   fprintf(f,"%llu,%d,%.31s,%d,%d,%d,%d,%d,%d,%d,%llu\n",(unsigned long long)now,id,t.name,rc,t.currentPriority,t.currentCpuAffinityMask,t.currentCpuId,t.lastExecutedCpuId,t.stackSize,sceKernelGetThreadStackFreeSize(id),(unsigned long long)t.runClocks);
  }
  fclose(f);
 }
#ifdef GL11
 f=fopen("ux0:/data/RS2Vita/hw-r2-gxm.csv",serial?"a":"w");
 if(f){
  if(!serial)fprintf(f,"time_us,stage_0_begin_1_end_2_draw_3_finish,calls,total_us,max_us,failures,ring_fallbacks,ring_fallback_bytes\n");
  for(unsigned i=0;i<4;i++){
   unsigned v[4];for(unsigned j=0;j<4;j++)v[j]=__sync_lock_test_and_set(&hw_gxm[i][j],0);
   fprintf(f,"%llu,%u,%u,%u,%u,%u,%llu,%llu\n",(unsigned long long)now,i,v[0],v[1],v[2],v[3],(unsigned long long)hw_ring_fallbacks,(unsigned long long)hw_ring_fallback_bytes);
  }fclose(f);
 }
 if(h5_no_init())return;
 f=fopen("ux0:/data/RS2Vita/hw-r2-gpu.csv",serial?"a":"w");
 if(f){
  if(!serial)fprintf(f,"time_us,draw_elements,uploads,upload_bytes_rgba_estimate,textures_created,textures_deleted,circular_bytes,pool_type,pool_total,pool_free\n");
  for(unsigned type=0;type<4;type++){
   size_t total=vglMemTotal((vglMemType)type),free=vglMemFree((vglMemType)type);
   fprintf(f,"%llu,%llu,%llu,%llu,%llu,%llu,%u,%u,%u,%u\n",(unsigned long long)now,(unsigned long long)hw_draw_calls,(unsigned long long)hw_upload_calls,(unsigned long long)hw_upload_bytes,(unsigned long long)hw_textures_created,(unsigned long long)hw_textures_deleted,circular_data_pool_size,type,(unsigned)total,(unsigned)(free<=total?free:UINT32_MAX));
  }
  fclose(f);
 }
 if(!serial){
  f=fopen("ux0:/data/RS2Vita/hw-r2-ring-placement.csv","w");
  if(f){fprintf(f,"index,address,memblock_id,info_rc,mapped_bytes,type\n");
   for(int i=0;i<gxm_display_buffer_count&&i<5;i++){
    SceUID id=sceKernelFindMemBlockByAddr(circular_data_pool[i],1);SceKernelMemBlockInfo info={0};info.size=sizeof(info);
    int rc=id<0?id:sceKernelGetMemBlockInfoByAddr(circular_data_pool[i],&info);
    fprintf(f,"%d,%p,%d,%d,%u,%u\n",i,circular_data_pool[i],id,rc,(unsigned)info.mappedSize,(unsigned)info.type);
   }fclose(f);
  }
 }
#endif
}
