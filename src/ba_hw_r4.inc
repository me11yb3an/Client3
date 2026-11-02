/* HW-R5: paired world-only bypass. All measurements are tagged with the actual
 * switch state; UI, client update, server execution and pool settings are retained. */
#include "ba_hw_r4.h"
static int h4_enabled; /* Diagnostic VPK defaults to world rendering OFF. */
static uint64_t h4_session,h4_interval,h4_last_poll,h4_last_loop;
static unsigned h4_last_loop_mode,h4_serial,h4_transition_serial;
static struct {
 uint64_t periods,period_us,max_period,draws,indices,buffer_calls,buffer_bytes,world_calls,grows;
 unsigned peak_vertices,peak_indices,vertex_capacity,index_capacity;
 struct {uint64_t calls,us,max;} stages[BA_TIMING_COUNT];
 struct {uint64_t calls,wall,exclusive,cpu_calls,cpu,exclusive_cpu,max;} scopes[H3_COUNT];
} h4_modes[2];
static unsigned h4_vcapacity,h4_icapacity;
static int h4_radius,h4_camera[5],h4_player[2],h4_ready;
static unsigned h4_live_fps,h4_live_logic;
static uint64_t h4_live_at,h4_last_tick;
static unsigned h4_live_frames,h4_live_ticks;
int h4_world_enabled(void){return h5_mode()!='N'&&h5_mode()!='U'&&h5_mode()!='M';}
void h4_record(unsigned i,uint64_t us){
 if(i>=BA_TIMING_COUNT)return;
 h4_modes[h4_enabled].stages[i].calls++;h4_modes[h4_enabled].stages[i].us+=us;
 if(us>h4_modes[h4_enabled].stages[i].max)h4_modes[h4_enabled].stages[i].max=us;
 if(i==BA_SERVER_TICK)h4_last_tick=us;
}
void h4_scope(unsigned i,uint64_t wall,uint64_t exclusive,unsigned valid,uint64_t cpu,uint64_t exclusive_cpu){
 if(i>=H3_COUNT)return;
 h4_modes[h4_enabled].scopes[i].calls++;h4_modes[h4_enabled].scopes[i].wall+=wall;h4_modes[h4_enabled].scopes[i].exclusive+=exclusive;
 if(wall>h4_modes[h4_enabled].scopes[i].max)h4_modes[h4_enabled].scopes[i].max=wall;
 if(valid){h4_modes[h4_enabled].scopes[i].cpu_calls++;h4_modes[h4_enabled].scopes[i].cpu+=cpu;h4_modes[h4_enabled].scopes[i].exclusive_cpu+=exclusive_cpu;}
}
void h4_graph(unsigned kind,uint64_t value){
 if(kind==0){h4_modes[h4_enabled].draws++;h4_modes[h4_enabled].indices+=value;}
 else {h4_modes[h4_enabled].buffer_calls++;h4_modes[h4_enabled].buffer_bytes+=value;}
}
void h4_world_enter(void){h4_modes[h4_enabled].world_calls++;}
void h4_geometry(unsigned vertices,unsigned indices,unsigned vc,unsigned ic,unsigned grew){
 h6_geometry(vertices,indices,vc,ic);
 if(vertices>h4_modes[h4_enabled].peak_vertices)h4_modes[h4_enabled].peak_vertices=vertices;
 if(indices>h4_modes[h4_enabled].peak_indices)h4_modes[h4_enabled].peak_indices=indices;
 h4_vcapacity=vc;h4_icapacity=ic;
 h4_modes[h4_enabled].vertex_capacity=vc;h4_modes[h4_enabled].index_capacity=ic;h4_modes[h4_enabled].grows+=grew;
}
void h4_context(int r,int x,int y,int z,int yaw,int pitch,int px,int pz,int ready){
 h4_radius=r;h4_camera[0]=x;h4_camera[1]=y;h4_camera[2]=z;h4_camera[3]=yaw;h4_camera[4]=pitch;h4_player[0]=px;h4_player[1]=pz;h4_ready=ready;
}
void h4_sample(unsigned frames,unsigned ticks){
 uint64_t now=ba_timing_now();if(!h4_live_at){h4_live_at=now;h4_live_frames=frames;h4_live_ticks=ticks;return;}
 if(now-h4_live_at>=1000000u){h4_live_fps=(uint64_t)(frames-h4_live_frames)*1000000u/(now-h4_live_at);h4_live_logic=(uint64_t)(ticks-h4_live_ticks)*1000000u/(now-h4_live_at);h4_live_at=now;h4_live_frames=frames;h4_live_ticks=ticks;}
}
void h4_hud_lines(char out[3][128]){
 snprintf(out[0],128,"HW-R6 mode %c | presentations %u/s | client %u Hz",h5_mode(),h4_live_fps,h4_live_logic);
 snprintf(out[1],128,"Last server tick %u ms | period 600 ms",(unsigned)(h4_last_tick/1000u));
 snprintf(out[2],128,"Benchmark logs: ux0:/data/RS2Vita/");
}

static void h4_threads(uint64_t now,const char*event){
 FILE*f=fopen("ux0:/data/RS2Vita/hw-r4-threads.csv",h4_serial++?"a":"w");if(!f)return;
 if(h4_serial==1)fprintf(f,"session,time_us,event,world3d,id,name,rc,priority,affinity,current_cpu,last_cpu,stack_bytes,run_clocks\n");
 unsigned n=hw_thread_count;if(n>32)n=32;
 for(unsigned i=0;i<=n;i++){
  SceUID id=i?hw_threads[i-1]:h3_main;SceKernelThreadInfo t={0};t.size=sizeof(t);int rc=sceKernelGetThreadInfo(id,&t);
  fprintf(f,"%llu,%llu,%s,%d,%d,%.31s,%d,%d,%d,%d,%d,%u,%llu\n",(unsigned long long)h4_session,(unsigned long long)now,event,h4_enabled,id,t.name,rc,t.currentPriority,t.currentCpuAffinityMask,t.currentCpuId,t.lastExecutedCpuId,(unsigned)t.stackSize,(unsigned long long)t.runClocks);
 }fclose(f);
}
static void h4_transition(uint64_t now,int previous,const char*event){
 FILE*f=fopen("ux0:/data/RS2Vita/hw-r4-transitions.csv",h4_transition_serial++?"a":"w");if(f){
  if(h4_transition_serial==1)fprintf(f,"session,time_us,event,previous_world3d,world3d\n");
  fprintf(f,"%llu,%llu,%s,%d,%d\n",(unsigned long long)h4_session,(unsigned long long)now,event,previous,h4_enabled);fclose(f);
 }h4_threads(now,event);
}
void h4_init(void){h4_session=ba_timing_now();h4_interval=h4_session;h4_transition(h4_session,-1,"initial_default");}
void h4_poll(void){
 uint64_t now=ba_timing_now();
 /* Entire outer-loop cadence includes work before/after the scoped frame,
  * including diagnostic polling and profiler overhead. Tag preceding period. */
 if(h4_last_loop){uint64_t us=now-h4_last_loop;h4_modes[h4_last_loop_mode].periods++;h4_modes[h4_last_loop_mode].period_us+=us;if(us>h4_modes[h4_last_loop_mode].max_period)h4_modes[h4_last_loop_mode].max_period=us;}
 h4_last_loop=now;
 extern int r16_diag_verbose(void);
 if(!h4_last_poll||(r16_diag_verbose()&&now-h4_last_poll>=1000000u)){
  FILE*f=fopen("ux0:/data/RS2Vita/hw-r4-world3d.txt","r");int requested=0;
  if(f){if(fscanf(f,"%d",&requested)!=1)requested=h4_enabled;fclose(f);}
  if(requested==0||requested==1){if(requested!=h4_enabled){int old=h4_enabled;h4_enabled=requested;h4_transition(now,old,"switch");}}
  h4_last_poll=now;
 }h4_last_loop_mode=h4_enabled;
}
void h4_summary(uint64_t now,unsigned serial){
 FILE*f=fopen("ux0:/data/RS2Vita/hw-r4-comparison.csv",serial?"a":"w");if(f){
  if(!serial)fprintf(f,"session,time_us,interval_us,world3d,loop_periods,loop_wall_us,max_loop_us,render_frames,logic_ticks,server_ticks,logic_us,server_tick_us,max_server_tick_us,render_us,world_cpu_us,present_us,pixel_convert_us,pixel_upload_us,frame_cpu_samples,frame_cpu_us,world_calls,indexed_draw_calls,index_count,buffer_calls,buffer_bytes,geometry_growths,peak_vertices,peak_indices,vertex_capacity,index_capacity,radius,camera_x,camera_y,camera_z,yaw,pitch,player_x,player_z,ready\n");
  for(unsigned m=0;m<2;m++){
   fprintf(f,"%llu,%llu,%llu,%u",(unsigned long long)h4_session,(unsigned long long)now,(unsigned long long)(now-h4_interval),m);
   uint64_t v[]={h4_modes[m].periods,h4_modes[m].period_us,h4_modes[m].max_period,h4_modes[m].stages[BA_RENDER].calls,h4_modes[m].stages[BA_LOGIC].calls,h4_modes[m].stages[BA_SERVER_TICK].calls,h4_modes[m].stages[BA_LOGIC].us,h4_modes[m].stages[BA_SERVER_TICK].us,h4_modes[m].stages[BA_SERVER_TICK].max,h4_modes[m].stages[BA_RENDER].us,h4_modes[m].stages[BA_WORLD_RASTER].us,h4_modes[m].stages[BA_PRESENT].us,h4_modes[m].stages[BA_PIXEL_CONVERT].us,h4_modes[m].stages[BA_PIXEL_UPLOAD].us,h4_modes[m].scopes[H3_FRAME].cpu_calls,h4_modes[m].scopes[H3_FRAME].cpu,h4_modes[m].world_calls,h4_modes[m].draws,h4_modes[m].indices,h4_modes[m].buffer_calls,h4_modes[m].buffer_bytes,h4_modes[m].grows,h4_modes[m].peak_vertices,h4_modes[m].peak_indices,h4_vcapacity,h4_icapacity};
   for(unsigned i=0;i<sizeof(v)/sizeof(*v);i++)fprintf(f,",%llu",(unsigned long long)v[i]);
   fprintf(f,",%d,%d,%d,%d,%d,%d,%d,%d,%d\n",h4_radius,h4_camera[0],h4_camera[1],h4_camera[2],h4_camera[3],h4_camera[4],h4_player[0],h4_player[1],h4_ready);
  }fclose(f);
 }
 f=fopen("ux0:/data/RS2Vita/hw-r4-scopes.csv",serial?"a":"w");if(f){
  if(!serial)fprintf(f,"session,time_us,world3d,scope,calls,inclusive_wall_us,exclusive_wall_us,cpu_sample_calls,inclusive_cpu_us,exclusive_cpu_us,max_wall_us\n");
  for(unsigned m=0;m<2;m++)for(unsigned i=0;i<H3_COUNT;i++)fprintf(f,"%llu,%llu,%u,%s,%llu,%llu,%llu,%llu,%llu,%llu,%llu\n",(unsigned long long)h4_session,(unsigned long long)now,m,h3_names[i],(unsigned long long)h4_modes[m].scopes[i].calls,(unsigned long long)h4_modes[m].scopes[i].wall,(unsigned long long)h4_modes[m].scopes[i].exclusive,(unsigned long long)h4_modes[m].scopes[i].cpu_calls,(unsigned long long)h4_modes[m].scopes[i].cpu,(unsigned long long)h4_modes[m].scopes[i].exclusive_cpu,(unsigned long long)h4_modes[m].scopes[i].max);
  fclose(f);
 }
 h4_threads(now,"summary");memset(h4_modes,0,sizeof(h4_modes));h4_interval=now;
}
