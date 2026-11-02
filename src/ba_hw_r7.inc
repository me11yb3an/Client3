/* Included after H5 definitions. No sockets, allocation or file I/O per frame. */
#include "ba_hw_r7.h"
#include "vita-telemetry.h"
#include "ba_native_memory.h"
static int h7_worker,h7_transport;
static uint64_t h7_due,h7_sampling_us;
static unsigned h7_samples;
int h7_worker_enabled(void){return h7_worker;}
static uint64_t h7_read64(const volatile void*p){return __atomic_load_n((const uint64_t*)p,__ATOMIC_RELAXED);}
void h7_init(void){
 FILE*f=fopen("ux0:/data/RS2Vita/r7-server.txt","r");if(f){int n=0;if(fscanf(f,"%d",&n)==1&&n==1)h7_worker=1;fclose(f);}
 char run[64];snprintf(run,sizeof(run),"r7-%c-%llx",h5_selected,(unsigned long long)ba_timing_now());
 h7_transport=0; /* H8 uses its separate authenticated transport only. */
 FILE*status=fopen("ux0:/data/RS2Vita/r7-setup.csv","w");if(status){fprintf(status,"worker,transport_status\n%d,%d\n",h7_worker,h7_transport);fclose(status);}
}
void h7_live_poll(void){
 if(h7_transport!=1||!__atomic_load_n(&h5_active,__ATOMIC_ACQUIRE))return;
 uint64_t now=ba_timing_now();if(now<h7_due)return;h7_due=now+1000000;
 uint64_t began=now;TelemetrySample s={0};s.wall_us=now-h5_start;
 s.client_updates=h7_read64(&h5_metrics[BA_LOGIC].calls);s.render_calls=h7_read64(&h5_metrics[BA_RENDER].calls);s.present_calls=h7_read64(&h5_metrics[BA_PRESENT].calls);
 s.logic_us=h7_read64(&h5_metrics[BA_LOGIC].us);s.render_us=h7_read64(&h5_metrics[BA_RENDER].us);
 s.convert_us=h7_read64(&h5_metrics[BA_PIXEL_CONVERT].us);s.upload_us=h7_read64(&h5_metrics[BA_PIXEL_UPLOAD].us);
 s.server_ticks=h7_read64(&h5_metrics[BA_SERVER_TICK].calls);s.server_us=h7_read64(&h5_metrics[BA_SERVER_TICK].us);
 s.graphics_attempts=h7_read64(&h5_attempts[0]);s.gxm_attempts=h7_read64(&h5_attempts[1]);
 SceKernelThreadInfo info={0};info.size=sizeof(info);int rc=sceKernelGetThreadInfo(h3_main,&info),worker_valid=0;
 s.main_cpu_us=rc<0?0:info.runClocks;s.telemetry_cpu_us=telemetry_thread_cpu_us(&worker_valid);s.cpu_valid=rc==0&&worker_valid;
 if(!h5_no_init()){
  int valid=1;size_t total=vglMemTotal(VGL_MEM_RAM),free=vglMemFree(VGL_MEM_RAM);valid&=free<=total;s.ram_reserved=total;s.ram_used=free<=total?total-free:0;
  total=vglMemTotal(VGL_MEM_VRAM);free=vglMemFree(VGL_MEM_VRAM);valid&=free<=total;s.cdram_reserved=total;s.cdram_used=free<=total?total-free:0;
  total=vglMemTotal(VGL_MEM_PHYCONT);free=vglMemFree(VGL_MEM_PHYCONT);s.phy_reserved=total;s.phy_used=free<=total?total-free:0;s.memory_valid=valid&&(free<=total);
 }
 H7NativeMemory mem;h7_native_snapshot(&mem);s.native_phy_live=mem.live[1];s.native_cdram_live=mem.live[0];s.native_phy_peak=mem.peak[1];s.native_cdram_peak=mem.peak[0];s.native_valid=!mem.untracked&&!mem.info_failures;
 SceKernelFreeMemorySizeInfo os={0};os.size=sizeof(os);if(!sceKernelGetFreeMemorySize(&os)){s.os_valid=1;s.os_user_free=os.size_user;s.os_phy_free=os.size_phycont;}
 telemetry_push(&s);s.kind=2;telemetry_push(&s);
 TelemetrySample threads={0};threads.kind=1;threads.wall_us=s.wall_us;
 SceUID ids[35];unsigned count=0,n=hw_thread_count;if(n>32)n=32;ids[count++]=h3_main;
 for(unsigned i=0;i<n;i++){int duplicate=0;for(unsigned j=0;j<count;j++)if(ids[j]==hw_threads[i])duplicate=1;if(!duplicate)ids[count++]=hw_threads[i];}
 int more[2]={h7_owner_thread_id(),telemetry_thread_id()};for(unsigned i=0;i<2;i++){int duplicate=more[i]<=0;for(unsigned j=0;j<count;j++)if(ids[j]==more[i])duplicate=1;if(!duplicate&&count<35)ids[count++]=more[i];}
 static unsigned cursor;if(cursor>=count)cursor=0;threads.tracked_threads=count;threads.batch_start=cursor;
 for(unsigned i=0;i<4;i++){threads.tid[i]=-1;threads.thread_rc[i]=-1;if(cursor+i>=count)continue;threads.tid[i]=ids[cursor+i];SceKernelThreadInfo ti={0};ti.size=sizeof(ti);int tr=sceKernelGetThreadInfo(threads.tid[i],&ti);threads.thread_rc[i]=tr;if(!tr)threads.thread_clocks[i]=ti.runClocks;}
 cursor=(cursor+4)%count;telemetry_push(&threads);h7_sampling_us+=ba_timing_now()-began;h7_samples++;
}
void h7_live_finish(void){
 if(h7_transport==1){sceKernelDelayThread(250000);telemetry_stop();h7_transport=0;}
 H7NativeMemory mem;h7_native_snapshot(&mem);FILE*mf=fopen("ux0:/data/RS2Vita/r7-native-memory.csv","w");if(mf){fprintf(mf,"category_0_cdram_1_phy_2_other,live_mapped_bytes,peak_mapped_bytes,allocations,frees,failed_alloc_calls,untracked_allocs,info_failures\n");for(unsigned i=0;i<3;i++)fprintf(mf,"%u,%llu,%llu,%llu,%llu,%llu,%llu,%llu\n",i,(unsigned long long)mem.live[i],(unsigned long long)mem.peak[i],(unsigned long long)mem.allocs[i],(unsigned long long)mem.frees[i],(unsigned long long)mem.failed,(unsigned long long)mem.untracked,(unsigned long long)mem.info_failures);fclose(mf);}
 unsigned drops=0,errors=0;telemetry_stats(&drops,&errors);
 uint64_t calls=0,wait=0;unsigned peak=0;h7_owner_snapshot(&calls,&wait,&peak);
 FILE*f=fopen("ux0:/data/RS2Vita/r7-overhead.csv","w");if(f){fprintf(f,"samples,sampling_us,rpc_calls,rpc_wait_ns,request_queue_peak,server_worker_id,telemetry_drops,telemetry_send_errors\n%u,%llu,%llu,%llu,%u,%d,%u,%u\n",h7_samples,(unsigned long long)h7_sampling_us,(unsigned long long)calls,(unsigned long long)wait,peak,h7_owner_thread_id(),drops,errors);fclose(f);}
}
