#ifdef BUILD_A_EMBEDDED
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <malloc.h>
#include <sys/time.h>
#include <psp2/rtc.h>
#include <psp2/power.h>
#include <psp2/kernel/threadmgr.h>
static SceUID h3_main;
#ifdef GL11
#include <vitaGL.h>
#endif
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/processmgr.h>
#include "embedded_server.h"
#include "ba_timing.h"
#include "h8_remote.h"
#include "ba_resource.h"
#include "allocator.h"
void bz_internal_error(int code) { fprintf(stderr,"BZIP_FATAL code=%d\n",code); abort(); }
/* User RAM budget. CDRAM framebuffers are allocated separately by platform/vita.c. */
unsigned int _newlib_heap_size_user = 224u * 1024u * 1024u;
const SceSize sceUserMainThreadStackSize = 2u * 1024u * 1024u;
int _newlib_heap_memblock_type = SCE_KERNEL_MEMBLOCK_TYPE_USER_RW;
static void seed_disposable_save(void) {

    char path[160];snprintf(path,sizeof(path),"ux0:/data/RS2Vita/players/%s.sav",h5_username());
    /* Each explicit benchmark run starts from the packaged disposable fixture. */
    /* H8 always resets only its disposable fixture on application launch. */
    FILE *in=fopen(h5_mode()=='R'?"ux0:/data/RS2Vita/fixture-field.sav":"app0:/sp/offline_test.sav","rb"); if(!in)return;
    FILE *out=fopen(path,"wb");
    if(out){char b[4096];size_t n;while((n=fread(b,1,sizeof(b),in)))if(fwrite(b,1,n,out)!=n)break;fclose(out);}
    fclose(in);
}
void h3_init(void);
#include "ba_hw_r4.h"
static void h3_config_snapshot(void);
void ba_vita_init(void) {
    h3_init();
    mkdir("ux0:/data",0777); mkdir("ux0:/data/RS2Vita",0777);
    mkdir("ux0:/data/RS2Vita/players",0777);
    h8_init();
    char out_path[256];
    freopen(h8_boot_path("ux0:/data/RS2Vita/runtime.log",out_path,sizeof(out_path)),"w",stdout);
    freopen(h8_boot_path("ux0:/data/RS2Vita/error.log",out_path,sizeof(out_path)),"w",stderr);
    static char output_buffer[8192];setvbuf(stdout,output_buffer,_IOFBF,sizeof(output_buffer));setvbuf(stderr,NULL,_IONBF,0);
    h5_init();h7_init();h5_mark("resource_index_begin");
    if(ba_resource_init()!=2){fprintf(stderr,"Resource pack missing or invalid\n");abort();}
    puts("BUILD_A_EMBEDDED transport=in-process-byte-queue heap=224MiB js_limit=96MiB");
    SceRtcTick rtc={0};struct timeval tv={0};
    int rtc_rc=sceRtcGetCurrentTick(&rtc),tv_rc=gettimeofday(&tv,NULL);
    FILE *clock_log=fopen("ux0:/data/RS2Vita/clock-audit.log","a");
    if(clock_log){fprintf(clock_log,"NATIVE_CLOCK rtc_rc=%d rtc_us=%llu timeval_rc=%d seconds=%lld micros=%lld monotonic_us=%llu\n",rtc_rc,(unsigned long long)rtc.tick,tv_rc,(long long)tv.tv_sec,(long long)tv.tv_usec,(unsigned long long)sceKernelGetSystemTimeWide());fclose(clock_log);}
    h5_mark("resource_index_ready");h4_init();h3_config_snapshot();
    seed_disposable_save();
}

static struct {uint64_t calls,total_us,max_us;} metrics[BA_TIMING_COUNT];
static unsigned rendered,logic_ticks,detail_frame,summary_count;
static int gl_maximum,gl_low_memory;
void ba_profile_gl_limits(int maximum,int low_memory){gl_maximum=maximum;gl_low_memory=low_memory;}
static int scene_radius,scene_x,scene_z,scene_yaw,scene_pitch,scene_vertices,scene_indices;
uint64_t ba_timing_now(void){return sceKernelGetSystemTimeWide();}
int ba_profile_detail(void){return h5_startup()?1:detail_frame;}
void ba_profile_render(void){static unsigned serial;detail_frame=(++serial%30)==0;}
void ba_profile_scene(int radius,int x,int z,int yaw,int pitch,int vertices,int indices){scene_radius=radius;scene_x=x;scene_z=z;scene_yaw=yaw;scene_pitch=pitch;scene_vertices=vertices;scene_indices=indices;}
void ba_timing_record(unsigned stage,uint64_t elapsed){h8_stage(stage,elapsed);if(stage>=BA_TIMING_COUNT)return;h5_record(stage,elapsed);if(sceKernelGetThreadId()!=h3_main)return;h4_record(stage,elapsed);metrics[stage].calls++;metrics[stage].total_us+=elapsed;if(elapsed>metrics[stage].max_us)metrics[stage].max_us=elapsed;}
void ba_vita_sample(unsigned frames,unsigned ticks){h4_sample(frames,ticks);rendered=frames;logic_ticks=ticks;}
extern size_t ba_server_heap_bytes(void);
#include "ba_hw_r3.inc"
#include "ba_hw_r2.inc"
#include "ba_hw_r4.inc"
#include "ba_hw_r5.inc"
#include "ba_hw_r7.inc"
void ba_timing_flush(void){
 if(h8_is_running())return; /* No bulk legacy log writes in an H8 measured run. */
 static uint64_t last;uint64_t now=ba_timing_now();if(!last){last=now;return;}if(now-last<30000000u||summary_count>=240)return;
 struct mallinfo heap=mallinfo();SceKernelFreeMemorySizeInfo mem={0};mem.size=sizeof(mem);int memrc=sceKernelGetFreeMemorySize(&mem);
 uint64_t opens,reads,bytes,index;ba_resource_summary(&opens,&reads,&bytes,&index);uint64_t began=ba_timing_now();
 FILE*f=fopen("ux0:/data/RS2Vita/hardware-profile.csv",summary_count?"a":"w");if(!f){last=now;return;}
 if(!summary_count)fprintf(f,"time_us,interval_us,stage,calls,total_us,max_us,frames,logic_ticks,heap_bytes,heap_free,js_malloc_bytes,resource_index_bytes,resource_opens,resource_reads,resource_bytes,user_free,cdram_free,memory_rc,radius,eye_x,eye_z,yaw,pitch,vertices,indices\n");
 for(unsigned i=0;i<BA_TIMING_COUNT;i++){
  fprintf(f,"%llu,%llu,%u,%llu,%llu,%llu,%u,%u,%d,%d,%u,%llu,%llu,%llu,%llu,%u,%u,%d,%d,%d,%d,%d,%d,%d,%d\n",(unsigned long long)now,(unsigned long long)(now-last),i,(unsigned long long)metrics[i].calls,(unsigned long long)metrics[i].total_us,(unsigned long long)metrics[i].max_us,rendered,logic_ticks,heap.uordblks,heap.fordblks,(unsigned)ba_server_heap_bytes(),(unsigned long long)index,(unsigned long long)opens,(unsigned long long)reads,(unsigned long long)bytes,mem.size_user,mem.size_cdram,memrc,scene_radius,scene_x,scene_z,scene_yaw,scene_pitch,scene_vertices,scene_indices);
  metrics[i].calls=metrics[i].total_us=metrics[i].max_us=0;
 }
 fclose(f);
#ifdef GL11
 if(!h5_no_init()){size_t ram_free=vglMemFree(VGL_MEM_RAM),ram_total=vglMemTotal(VGL_MEM_RAM),vram_free=vglMemFree(VGL_MEM_VRAM),vram_total=vglMemTotal(VGL_MEM_VRAM);
 FILE*gpu=fopen("ux0:/data/RS2Vita/hardware-memory.csv",summary_count?"a":"w");
 if(gpu){if(!summary_count)fprintf(gpu,"time_us,arm_mhz,gpu_mhz,vgl_ram_free,vgl_ram_total,vgl_vram_free,vgl_vram_total,gl_max_texture,low_memory,model_pool_used,model_pool_capacity\n");
 fprintf(gpu,"%llu,%d,%d,%u,%u,%u,%u,%d,%d,%d,%d\n",(unsigned long long)now,scePowerGetArmClockFrequency(),scePowerGetGpuClockFrequency(),(unsigned)(ram_free<=ram_total?ram_free:UINT32_MAX),(unsigned)ram_total,(unsigned)(vram_free<=vram_total?vram_free:UINT32_MAX),(unsigned)vram_total,gl_maximum,gl_low_memory,bump_allocator_used(),bump_allocator_capacity());fclose(gpu);}}
#endif
 h4_summary(now,summary_count);h3_summary(now,summary_count);hw_r2_summary(now,summary_count);fflush(stdout);last=now;summary_count++;ba_timing_record(BA_STORAGE_FLUSH,ba_timing_now()-began);
}
#endif

unsigned h8_list_threads(int *ids,unsigned cap){
 unsigned count=0;if(cap)ids[count++]=h3_main;
 unsigned n=hw_thread_count;if(n>32)n=32;
 for(unsigned i=0;i<n&&count<cap;i++){int duplicate=0;for(unsigned j=0;j<count;j++)if(ids[j]==hw_threads[i])duplicate=1;if(!duplicate)ids[count++]=hw_threads[i];}
 return count;
}
