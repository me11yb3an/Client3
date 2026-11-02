/* HW-R5 bounded comparisons. File I/O occurs at boundaries, outside measurement. */
#ifdef HW_R5_GPROF
extern void moncontrol(int);
#endif
static int h5_pool_option;
int h5_pool_experiment(void){return h5_pool_option;}
static char h5_selected='C';
static int h5_requested;static _Atomic int h5_active,h5_done;
static unsigned h5_seconds=60;
static uint32_t h5_rng[2]={0x48575235u,0x52454e44u};static unsigned h5_rendering;
void h5_render_phase(int active){h5_rendering=active?1:0;}
double h5_random(void){uint32_t x=h5_rng[h5_rendering];x^=x<<13;x^=x>>17;x^=x<<5;h5_rng[h5_rendering]=x;return (double)x/4294967296.0;}

static uint64_t h5_boot,h5_ready_at,h5_start;
typedef struct {uint64_t calls,wall,exclusive;} H5ScopeMetric;
static H5ScopeMetric h5_scopes[H3_COUNT],h5_startup_scopes[H3_COUNT];
void h5_scope(unsigned i,uint64_t wall,uint64_t exclusive){if(i>=H3_COUNT||h5_done)return;H5ScopeMetric*m=h5_active?&h5_scopes[i]:&h5_startup_scopes[i];m->calls++;m->wall+=wall;m->exclusive+=exclusive;}
int h5_startup(void){return h5_requested&&!h5_active&&!h5_done;}
static uint64_t h5_alloc_calls[4],h5_alloc_bytes,h5_alloc_failures;
void h5_allocation(unsigned kind,uint64_t bytes,int failed){if(h5_active&&kind<4){__sync_fetch_and_add(&h5_alloc_calls[kind],1);__sync_fetch_and_add(&h5_alloc_bytes,bytes);if(failed)__sync_fetch_and_add(&h5_alloc_failures,1);}}
void __real_free(void*);
void __wrap_free(void*p){if(p)h5_allocation(3,0,0);__real_free(p);}
static uint64_t h5_copies,h5_copy_bytes;
static uint64_t h5_attempts[2],h5_blocked[2];
typedef struct {_Atomic uint64_t calls,us,max;} H5Metric;
static H5Metric h5_metrics[BA_TIMING_COUNT],h5_startup_metrics[BA_TIMING_COUNT];
int h5_requested_run(void){return h5_requested;}
const char*h5_username(void){return h5_selected=='R'?"vita_probe":"offline_test";}
int h5_mode(void){return h5_selected;}
int h5_no_init(void){return h5_selected=='A'||h5_selected=='S';}
int h5_silent(void){return h5_selected=='A'||h5_selected=='B'||h5_selected=='S';}
int h5_measuring(void){return h5_active;}
int h5_graph(unsigned kind){
 if(kind>1)kind=1;
 int deny=h5_no_init()||(h5_active&&h5_silent());
 if(h5_active){__sync_fetch_and_add(&h5_attempts[kind],1);if(deny)__sync_fetch_and_add(&h5_blocked[kind],1);}
 return deny;
}
void *__real_memcpy(void*,const void*,size_t);
void *__real_memmove(void*,const void*,size_t);
void *__wrap_memcpy(void*d,const void*s,size_t n){if(h5_active){__sync_fetch_and_add(&h5_copies,1);__sync_fetch_and_add(&h5_copy_bytes,n);}return __real_memcpy(d,s,n);}
void *__wrap_memmove(void*d,const void*s,size_t n){if(h5_active){__sync_fetch_and_add(&h5_copies,1);__sync_fetch_and_add(&h5_copy_bytes,n);}return __real_memmove(d,s,n);}
#ifndef HW_R5_HOST
void *__real_sceClibMemcpy(void*,const void*,SceSize);
void *__wrap_sceClibMemcpy(void*d,const void*s,SceSize n){if(h5_active){__sync_fetch_and_add(&h5_copies,1);__sync_fetch_and_add(&h5_copy_bytes,n);}return __real_sceClibMemcpy(d,s,n);}
#endif
void h5_record(unsigned i,uint64_t us){
 if(h5_done||i>=BA_TIMING_COUNT)return;
 H5Metric*m=h5_active?&h5_metrics[i]:&h5_startup_metrics[i];m->calls++;m->us+=us;uint64_t old=m->max;while(old<us&&!__atomic_compare_exchange_n(&m->max,&old,us,0,__ATOMIC_RELAXED,__ATOMIC_RELAXED)){}
}
void h5_mark(const char*label){
 static unsigned marks;static volatile int marking;
 while(__sync_lock_test_and_set(&marking,1)) {
#ifdef HW_R5_HOST
 sched_yield();
#else
 sceKernelDelayThread(1);
#endif
 }
 if(h5_active||marks>=128){__sync_lock_release(&marking);return;}
 uint64_t now=ba_timing_now();
 FILE*f=fopen("ux0:/data/RS2Vita/startup.csv",marks?"a":"w");
 if(f){if(!marks)fprintf(f,"since_init_us,event\n");fprintf(f,"%llu,%.160s\n",(unsigned long long)(now-h5_boot),label);fclose(f);}marks++;__sync_lock_release(&marking);
}
void h5_init(void){
#ifdef HW_R5_GPROF
 extern void moncontrol(int);moncontrol(0);
#endif
 h5_boot=ba_timing_now();
 h6_init();
 FILE*pool=fopen("ux0:/data/RS2Vita/pool-experiment.txt","r");
 if(pool){int n=0;if(fscanf(pool,"%d",&n)==1&&n>=0&&n<=2)h5_pool_option=n;fclose(pool);}

 FILE*f=fopen("ux0:/data/RS2Vita/mode.txt","r");
 if(f){char mode=0;unsigned sec=60;if(fscanf(f," %c %u",&mode,&sec)>=1&&strchr("ABCNTSUMWDRI",mode)){h5_selected=mode;h5_requested=1;h5_seconds=sec<10?10:sec>300?300:sec;}fclose(f);}
}
/* Long-window thread clocks, not the stale short-span self-query. */
static void h5_thread_boundary(const char*event,uint64_t now){
 FILE*f=fopen("ux0:/data/RS2Vita/threads.csv",event[0]=='b'?"w":"a");if(!f)return;
 if(event[0]=='b')fprintf(f,"event,time_us,mode,id,name,rc,priority,affinity,cpu,run_clocks\n");
#ifdef HW_R5_HOST
 struct timespec cpu;clock_gettime(CLOCK_THREAD_CPUTIME_ID,&cpu);
 fprintf(f,"%s,%llu,%c,%d,linux-main,0,0,0,0,%llu\n",event,(unsigned long long)now,h5_selected,(int)getpid(),(unsigned long long)cpu.tv_sec*1000000u+cpu.tv_nsec/1000u);
#else
 unsigned n=hw_thread_count;if(n>32)n=32;
 for(unsigned i=0;i<=n;i++){
  SceUID id=i?hw_threads[i-1]:h3_main;SceKernelThreadInfo t={0};t.size=sizeof(t);int rc=sceKernelGetThreadInfo(id,&t);
  fprintf(f,"%s,%llu,%c,%d,%.31s,%d,%d,%d,%d,%llu\n",event,(unsigned long long)now,h5_selected,id,t.name,rc,t.currentPriority,t.currentCpuAffinityMask,t.lastExecutedCpuId,(unsigned long long)t.runClocks);
 }
#endif
fclose(f);
}
#ifndef HW_R5_HOST
static struct {char name[32];uint32_t type,size,opt_size;int result;uint64_t us;} h5_blocks[256];
static volatile unsigned h5_block_count;
SceUID __real_sceKernelAllocMemBlock(const char*,SceKernelMemBlockType,SceSize,SceKernelAllocMemBlockOpt*);
SceUID __wrap_sceKernelAllocMemBlock(const char*n,SceKernelMemBlockType type,SceSize size,SceKernelAllocMemBlockOpt*opt){
 uint64_t start=ba_timing_now();SceUID id=__real_sceKernelAllocMemBlock(n,type,size,opt);h7_native_alloc(id,type,size);
 unsigned i=__sync_fetch_and_add(&h5_block_count,1);if(i<256){strncpy(h5_blocks[i].name,n?n:"",31);h5_blocks[i].type=type;h5_blocks[i].size=size;h5_blocks[i].opt_size=opt?opt->size:0;h5_blocks[i].result=id;h5_blocks[i].us=ba_timing_now()-start;}return id;
}
static struct {int valid,id,rc;char name[32];uint64_t time,cpu;} h5_exits[64];
static volatile unsigned h5_exit_count;
int __real_sceKernelExitDeleteThread(int);
int __wrap_sceKernelExitDeleteThread(int status){
 SceUID id=sceKernelGetThreadId();SceKernelThreadInfo t={0};t.size=sizeof(t);int rc=sceKernelGetThreadInfo(id,&t);unsigned i=__sync_fetch_and_add(&h5_exit_count,1);
 if(i<64){h5_exits[i].id=id;h5_exits[i].rc=rc;strncpy(h5_exits[i].name,t.name,31);h5_exits[i].time=ba_timing_now();h5_exits[i].cpu=t.runClocks;__atomic_store_n(&h5_exits[i].valid,1,__ATOMIC_RELEASE);}
 return __real_sceKernelExitDeleteThread(status);
}
static void h5_dump_blocks(void){
 FILE*f=fopen("ux0:/data/RS2Vita/memblocks.csv","w");if(!f)return;
 fprintf(f,"order,name,type_hex,requested_bytes,option_struct_bytes,uid,allocation_wall_us,total_calls\n");
 unsigned n=h5_block_count;if(n>256)n=256;
 for(unsigned i=0;i<n;i++)fprintf(f,"%u,%s,%08x,%u,%u,%d,%llu,%u\n",i,h5_blocks[i].name,h5_blocks[i].type,h5_blocks[i].size,h5_blocks[i].opt_size,h5_blocks[i].result,(unsigned long long)h5_blocks[i].us,h5_block_count);fclose(f);
 f=fopen("ux0:/data/RS2Vita/exited-threads.csv","w");if(f){fprintf(f,"id,name,rc,time_us,run_clocks,total_exits\n");unsigned count=h5_exit_count;if(count>64)count=64;for(unsigned i=0;i<count;i++)if(__atomic_load_n(&h5_exits[i].valid,__ATOMIC_ACQUIRE))fprintf(f,"%d,%.31s,%d,%llu,%llu,%u\n",h5_exits[i].id,h5_exits[i].name,h5_exits[i].rc,(unsigned long long)h5_exits[i].time,(unsigned long long)h5_exits[i].cpu,h5_exit_count);fclose(f);}

}
#else
static void h5_dump_blocks(void){}
#endif
static void h5_pools(const char*event){
 FILE*f=fopen("ux0:/data/RS2Vita/pools.csv",event[0]=='b'?"w":"a");if(!f)return;
 if(event[0]=='b')fprintf(f,"event,mode,initialized,pool,reserved_bytes,allocator_free_bytes,allocator_used_bytes,valid\n");
#ifdef HW_R5_HOST
 fprintf(f,"%s,%c,0,-1,0,0,0,1\n",event,h5_selected);
#else
 if(h5_no_init())fprintf(f,"%s,%c,0,-1,0,0,0,1\n",event,h5_selected);
 else for(unsigned i=0;i<4;i++){size_t total=vglMemTotal((vglMemType)i),free=vglMemFree((vglMemType)i);fprintf(f,"%s,%c,1,%u,%u,%u,%u,%d\n",event,h5_selected,i,(unsigned)total,(unsigned)(free<=total?free:UINT32_MAX),(unsigned)(free<=total?total-free:UINT32_MAX),free<=total);}
#endif
 fclose(f);
}
static void h5_startup_snapshot(const char*phase){
 static unsigned serial;FILE*f=fopen("ux0:/data/RS2Vita/startup-stages.csv",serial?"a":"w");
 if(f){if(!serial)fprintf(f,"phase,stage_id,calls,wall_us,max_us\n");for(unsigned i=0;i<BA_TIMING_COUNT;i++)fprintf(f,"%s,%u,%llu,%llu,%llu\n",phase,i,(unsigned long long)h5_startup_metrics[i].calls,(unsigned long long)h5_startup_metrics[i].us,(unsigned long long)h5_startup_metrics[i].max);fclose(f);}
 f=fopen("ux0:/data/RS2Vita/startup-scopes.csv",serial?"a":"w");if(f){if(!serial)fprintf(f,"phase,scope_id,calls,inclusive_wall_us,exclusive_wall_us\n");for(unsigned i=0;i<H3_COUNT;i++)fprintf(f,"%s,%u,%llu,%llu,%llu\n",phase,i,(unsigned long long)h5_startup_scopes[i].calls,(unsigned long long)h5_startup_scopes[i].wall,(unsigned long long)h5_startup_scopes[i].exclusive);fclose(f);}serial++;
}
int h5_step(int ready,int x,int z){
 if(!h5_requested)return 0;
 h7_live_poll();
 if(h5_done){
#ifdef HW_R5_HOST
 exit(0);
#endif
 return 1;}

 uint64_t now=ba_timing_now();
 if(h5_selected=='T')ready=1;
 if(!ready){if(now-h5_boot>300000000u){h5_mark("ABORT world_not_ready_300s");h5_done=1;return 1;}return 0;}
 if(!h5_ready_at){h5_ready_at=now;h5_mark("workload_ready");h5_startup_snapshot("ready");}
 if(!h5_active){
  if(now-h5_ready_at<5000000u)return 0;
  h5_mark("warmup_complete");h5_startup_snapshot("warmup_complete");h5_pools("begin");h5_thread_boundary("begin",ba_timing_now());
  memset(h5_metrics,0,sizeof(h5_metrics));memset(h5_attempts,0,sizeof(h5_attempts));memset(h5_blocked,0,sizeof(h5_blocked));
  h6_pixmap_boundary(1);h5_start=ba_timing_now();h5_active=1;
#ifdef HW_R5_GPROF
 moncontrol(1);
#endif
 return 0;
 }
 if(now-h5_start<(uint64_t)h5_seconds*1000000u)return 0;
 h5_active=0;h5_done=1;
#ifdef HW_R5_GPROF
 moncontrol(0);
#endif
 h5_thread_boundary("end",now);h5_pools("end");h5_dump_blocks();h6_finish();
#if defined(__vita__) && defined(GL11)
 FILE*ring_file=fopen("ux0:/data/RS2Vita/r6-ring.csv","w");
 if(ring_file){fprintf(ring_file,"phase,display_slots,total_ring_bytes,slot_capacity_bytes,wrapped_calls,max_request_bytes,observed_peak_advance_bytes,fallbacks,fallback_bytes,null_returns\n");
  for(unsigned p=0;p<2;p++)fprintf(ring_file,"%u,%u,%u,%llu,%llu,%llu,%llu,%llu,%llu,%llu\n",p,gxm_display_buffer_count,circular_data_pool_size,(unsigned long long)h6_ring[p].capacity,(unsigned long long)h6_ring[p].calls,(unsigned long long)h6_ring[p].max_request,(unsigned long long)h6_ring[p].peak_advance,(unsigned long long)h6_ring[p].fallbacks,(unsigned long long)h6_ring[p].fallback_bytes,(unsigned long long)h6_ring[p].nulls);fclose(ring_file);}
#endif
 FILE*f=fopen("ux0:/data/RS2Vita/benchmark.csv","w");
 if(f){fprintf(f,"mode,wall_us,player_x,player_z,stage,calls,processing_us,max_us,graphics_attempts,gxm_attempts,graphics_blocked,gxm_blocked,linked_copy_calls,linked_copy_bytes\n");
  for(unsigned i=0;i<BA_TIMING_COUNT;i++)fprintf(f,"%c,%llu,%d,%d,%u,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu\n",h5_selected,(unsigned long long)(now-h5_start),x,z,i,(unsigned long long)h5_metrics[i].calls,(unsigned long long)h5_metrics[i].us,(unsigned long long)h5_metrics[i].max,(unsigned long long)h5_attempts[0],(unsigned long long)h5_attempts[1],(unsigned long long)h5_blocked[0],(unsigned long long)h5_blocked[1],(unsigned long long)h5_copies,(unsigned long long)h5_copy_bytes);fclose(f);}
 f=fopen("ux0:/data/RS2Vita/scopes.csv","w");if(f){fprintf(f,"scope_id,calls,inclusive_wall_us,exclusive_wall_us\n");for(unsigned i=0;i<H3_COUNT;i++)fprintf(f,"%u,%llu,%llu,%llu\n",i,(unsigned long long)h5_scopes[i].calls,(unsigned long long)h5_scopes[i].wall,(unsigned long long)h5_scopes[i].exclusive);fclose(f);}
 f=fopen("ux0:/data/RS2Vita/allocations.csv","w");if(f){fprintf(f,"malloc_calls,calloc_calls,realloc_calls,free_nonnull_calls,requested_bytes,failures\n%llu,%llu,%llu,%llu,%llu,%llu\n",(unsigned long long)h5_alloc_calls[0],(unsigned long long)h5_alloc_calls[1],(unsigned long long)h5_alloc_calls[2],(unsigned long long)h5_alloc_calls[3],(unsigned long long)h5_alloc_bytes,(unsigned long long)h5_alloc_failures);fclose(f);}
 h5_mark("complete_paused_close_application");fflush(stdout);h7_live_finish();return 1;
}
