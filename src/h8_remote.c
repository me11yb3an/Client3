#include "r10_scene.h"
/* Diagnostic-only controller. No kernel imports, global injection, remote paths,
 * shell, install, or arbitrary title commands. Network worker never owns Client. */
#include "h8_remote.h"
#include "h8_protocol.h"
#include "ba_timing.h"
#include "ba_hw_r3.h"
#ifdef H8_HOST_TEST
#include "h8_host_shim.h"
#else
#include <psp2/ctrl.h>
#include <psp2/kernel/rng.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/sysmodule.h>
#include <psp2/power.h>
#include <psp2/appmgr.h>
#include <vitaGL.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <malloc.h>
#include <time.h>
#ifndef H8_DATA_ROOT
#define H8_DATA_ROOT "ux0:/data/RS2Vita/"
#endif
#define ROOT H8_DATA_ROOT
#define LD(x) __atomic_load_n(&(x),__ATOMIC_ACQUIRE)
#define ST(x,v) __atomic_store_n(&(x),(v),__ATOMIC_RELEASE)
static int enabled,stop_worker,armed,running,pending,stop_run,snapshot_requested,snapshot_done;
static unsigned request_kind,request_seconds,request_preset,serial;
static SceUID worker=-1,main_id;
static uint64_t arm_at,run_at,measure_at,finish_at,last_present,pending_at,periods[8192];
static unsigned period_count,action_index,run_kind,run_seconds,preset;
static struct {uint64_t at;unsigned kind,step;int x,z,accepted;} actions[4];
static unsigned action_count;
static uint64_t calls[BA_TIMING_COUNT],elapsed[BA_TIMING_COUNT],scope_wall[H3_COUNT],scope_exclusive[H3_COUNT];
static unsigned scene_radius;static int scene_x,scene_z,scene_yaw,scene_pitch;
static char bootdir[160],rundir[192],result[1024]="{\"state\":\"idle\"}",host[32];static unsigned port;
static uint8_t provision[32];
static unsigned long long provision_expires;
static const char*files[]={"summary.json","presentations.csv","stages.csv","threads.csv","screenshot.rgba","scopes.csv","actions.csv"};
static SceUID thread_ids[40];static uint64_t thread_start[40];static unsigned thread_count;
static int initial_x,initial_z,initial_yaw,initial_pitch,initial_radius,initial_lowmem;
static int local_override;
static uint64_t cpu_start;static int cpu_start_valid;
static uint64_t authorized_until;
static uint64_t now_us(void){return sceKernelGetSystemTimeWide();}
int h8_is_running(void){return LD(running);}
int h8_is_armed(void){return LD(armed);}
unsigned h8_filter_buttons(unsigned physical){unsigned chord=SCE_CTRL_L1|SCE_CTRL_R1|SCE_CTRL_UP;int active=LD(armed);if(active&&(physical&SCE_CTRL_SELECT)){ST(local_override,1);ST(stop_run,1);ST(pending,0);ST(snapshot_requested,0);ST(armed,0);ST(authorized_until,0);return 0;}if(enabled&&((physical&chord)==chord||active))return 0;return physical;}
int h8_local_override(void){return LD(local_override);}
const char*h8_boot_path(const char*path,char*out,size_t cap){
 if(!bootdir[0]||strncmp(path,ROOT,strlen(ROOT)))return path;
 const char*n=path+strlen(ROOT);size_t len=strlen(n);
 /* Only old flat diagnostic output is redirected. Configuration and players
  * remain in the new private root. No caller supplies this mapping remotely. */
 if(strchr(n,'/')||(len>4&&!strcmp(n+len-4,".txt"))||(len>8&&(!strcmp(n+len-8,".txt.tmp")||!strcmp(n+len-8,".txt.bak"))))return path;
 if(snprintf(out,cap,"%s/%s",bootdir,n)>=(int)cap)return NULL;return out;
}
void h8_stage(unsigned stage,uint64_t us){if(LD(running)==2&&stage<BA_TIMING_COUNT){__atomic_fetch_add(&calls[stage],1,__ATOMIC_RELAXED);__atomic_fetch_add(&elapsed[stage],us,__ATOMIC_RELAXED);}}
void h8_scope(unsigned s,uint64_t w,uint64_t e){if(LD(running)==2&&s<H3_COUNT){scope_wall[s]+=w;scope_exclusive[s]+=e;}}
void h8_scene(int radius,int x,int z,int yaw,int pitch){scene_radius=radius;scene_x=x;scene_z=z;scene_yaw=yaw;scene_pitch=pitch;}
static int hex(int c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;return -1;}
static int config(void){
 FILE*f=fopen(ROOT"remote.txt","r");if(!f)return 0;char key[65],extra;int n=fscanf(f,"%31s %u %64s %llu %c",host,&port,key,&provision_expires,&extra);fclose(f);
 time_t utc=time(NULL);
 if(n!=4||port<1024||port>65535||strlen(key)!=64||utc<1700000000||provision_expires<=(uint64_t)utc||provision_expires>(uint64_t)utc+86400)return 0;
 unsigned a,b,c,d;char tail;if(sscanf(host,"%u.%u.%u.%u%c",&a,&b,&c,&d,&tail)!=4||a>255||b>255||c>255||d==0||d>=255)return 0;
 int private_ip=a==10||(a==172&&b>=16&&b<=31)||(a==192&&b==168);
#ifdef H8_HOST_TEST
 private_ip|=a==127;
#endif
 if(!private_ip)return 0;
 for(unsigned i=0;i<32;i++){int x=hex(key[i*2]),y=hex(key[i*2+1]);if(x<0||y<0){memset(key,0,sizeof(key));return 0;}provision[i]=x*16+y;}
 memset(key,0,sizeof(key));return 1;
}
static void file_path(unsigned id,char out[256]){snprintf(out,256,"%s/%s",rundir,files[id]);}
static int socket_id=-1;static SceNetSockaddrIn destination;
static uint8_t last_request[H8_MAX_PACKET],last_response[H8_MAX_PACKET];static int last_request_len,last_response_len;
static H8Session session;
static void reply(uint8_t type,const void*data,size_t size){
 int n=h8_encode(&session,type,data,size,now_us(),last_response);
 last_response_len=n;if(n)sceNetSendto(socket_id,last_response,n,SCE_NET_MSG_DONTWAIT,(SceNetSockaddr*)&destination,sizeof(destination));
}
static void text(const char*s){reply(128,s,strlen(s));}
static uint32_t get32(const uint8_t*p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static void put32(uint8_t*p,uint32_t v){p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v;}
static void dispatch(uint8_t type,const uint8_t*p,size_t n){
 if(type==1&&n==0){
  int state=LD(running);unsigned run=LD(serial);
  char status[1024];snprintf(status,sizeof(status),"{\"state\":\"%s\",\"run\":%u,\"snapshot\":%d,\"source\":\"%s\",\"title\":\"RS225D001\",\"ui\":%d,\"msaa\":%d,\"ring_mib\":%d,\"immediate_mib\":%d}",state==3?"finishing":state==2?"measuring":state==1?"warmup":LD(pending)?"pending":run?"done":"idle",run,LD(snapshot_done),HW_R5_SOURCE_ID,h6_ui_policy(),h6_msaa(),h6_ring_mib(),h6_immediate_mib());text(status);
 }else if(type==2&&n==0){text("{\"heartbeat\":true}");
 }else if(type==3&&n==4&&p[0]<=2&&p[1]>=10&&p[1]<=120&&(p[2]==0||p[2]==2)&&p[3]==0){
  if(LD(running)||LD(pending)||LD(snapshot_requested)){text("{\"error\":\"busy\"}");return;}
  request_kind=p[0];request_seconds=p[1];request_preset=p[2];pending_at=now_us();ST(stop_run,0);ST(pending,1);text("{\"queued\":true}");
 }else if(type==4&&n==0){ST(stop_run,1);ST(pending,0);text("{\"stopping\":true}");
 }else if(type==5&&n==0){
  if(LD(running)||LD(pending)||!serial||LD(snapshot_requested)||LD(snapshot_done)){text("{\"error\":\"busy_or_no_run\"}");return;}
  ST(snapshot_done,0);ST(snapshot_requested,1);text("{\"snapshot_queued\":true}");
 }else if(type==6&&n==5&&p[0]<sizeof(files)/sizeof(files[0])){
  if(LD(running)||LD(pending)||!serial||(p[0]==4&&!LD(snapshot_done))){text("{\"error\":\"not_ready\"}");return;}
  char path[256];file_path(p[0],path);FILE*f=fopen(path,"rb");if(!f){text("{\"error\":\"missing_log\"}");return;}
  if(fseek(f,0,SEEK_END)){fclose(f);text("{\"error\":\"seek\"}");return;}long total=ftell(f);uint32_t offset=get32(p+1);
  if(total<0||total>3*1024*1024||offset>(uint32_t)total||fseek(f,offset,SEEK_SET)){fclose(f);text("{\"error\":\"bounds\"}");return;}
  uint8_t data[1024];data[0]=p[0];put32(data+1,offset);put32(data+5,total);size_t got=fread(data+9,1,sizeof(data)-9,f);int bad=ferror(f);fclose(f);if(bad){text("{\"error\":\"read\"}");return;}reply(129,data,9+got);
 }else if(type==7&&n==1&&p[0]<=1){
  if(LD(running)||LD(pending)||LD(snapshot_requested)){text("{\"error\":\"busy\"}");return;}
  FILE*f=fopen(ROOT"launch-preset.txt","w");int good=0;if(f){good=fprintf(f,"%u\n",p[0])==2;if(fclose(f))good=0;}
  text(good?"{\"restart_required\":true,\"manual_relaunch\":true}":"{\"error\":\"config_write\"}");
 }else text("{\"error\":\"invalid_command\"}");
}
static int worker_main(SceSize argc,void*arg){
 (void)argc;(void)arg;uint8_t*net=memalign(64,1024*1024);int module_owned=0,net_owned=0,ctl_owned=0;
 uint64_t held_at=0,hello_at=0,last_poll=0;unsigned last_system_event=0,last_app_event=0;int chord_latched=0;ST(local_override,0);
 while(!LD(stop_worker)){
  uint64_t now=now_us();SceCtrlData pad={0};int ok=sceCtrlPeekBufferPositive(0,&pad,1)>0;
  SceAppMgrAppState app={0};int focus=_sceAppMgrGetAppState(&app,sizeof(app),3)==0&&!app.isSystemUiOverlaid;
  int event_changed=app.systemEventNum!=last_system_event||app.appEventNum!=last_app_event;last_system_event=app.systemEventNum;last_app_event=app.appEventNum;
  int stall=last_poll&&now-last_poll>500000;last_poll=now;
  if(LD(pending)==1&&now-pending_at>10000000)ST(pending,0);
  unsigned chord=SCE_CTRL_L1|SCE_CTRL_R1|SCE_CTRL_UP;
  int held=ok&&focus&&time(NULL)<(time_t)provision_expires&&(pad.buttons&chord)==chord;
  if(!held){held_at=0;chord_latched=0;}else if(!held_at)held_at=now;
  if(held&&now-held_at>=1000000&&!chord_latched&&!LD(armed)){
   chord_latched=1;uint8_t nonce[32];
   if(net&&sceKernelGetRandomNumber(nonce,32)==0){
    if(socket_id<0){
     int rc=0;if(sceSysmoduleIsLoaded(SCE_SYSMODULE_NET)!=0){rc=sceSysmoduleLoadModule(SCE_SYSMODULE_NET);if(rc>=0)module_owned=1;}
     if(rc>=0){SceNetInitParam init={net,1024*1024,0};rc=sceNetInit(&init);if(rc>=0)net_owned=1;}
     if(rc>=0){rc=sceNetCtlInit();if(rc>=0)ctl_owned=1;}
     if(rc>=0){socket_id=sceNetSocket("RS225D001-control",SCE_NET_AF_INET,SCE_NET_SOCK_DGRAM,0);if(socket_id>=0){int nb=1;if(sceNetSetsockopt(socket_id,SCE_NET_SOL_SOCKET,SCE_NET_SO_NBIO,&nb,sizeof(nb))<0){sceNetSocketClose(socket_id);socket_id=-1;}}}
     memset(&destination,0,sizeof(destination));destination.sin_len=sizeof(destination);destination.sin_family=SCE_NET_AF_INET;destination.sin_port=sceNetHtons(port);if(socket_id>=0&&sceNetInetPton(SCE_NET_AF_INET,host,&destination.sin_addr)!=1){sceNetSocketClose(socket_id);socket_id=-1;}
    }
    if(socket_id>=0){h8_arm(&session,provision,nonce,now);ST(local_override,0);ST(armed,1);arm_at=now;hello_at=0;last_request_len=last_response_len=0;}
   }
  }
  /* SELECT is read from the physical SDK pad, never a merged/injected pad.
   * Worker lease checking remains independent of the rendering thread. */
  if(LD(armed)&&(!ok||!focus||stall||event_changed||(pad.buttons&SCE_CTRL_SELECT)||!h8_alive(&session,now))){
   ST(stop_run,1);ST(pending,0);ST(snapshot_requested,0);ST(armed,0);ST(authorized_until,0);h8_disarm(&session);ST(local_override,1);
  }
  if(!LD(armed)&&session.armed){h8_disarm(&session);ST(stop_run,1);ST(pending,0);ST(snapshot_requested,0);ST(authorized_until,0);}
  if(LD(armed)&&socket_id>=0){
   uint8_t packet[H8_MAX_PACKET+1],payload[1024],type;size_t length;
   if(!session.authenticated&&now>=hello_at){uint8_t hello[68];int n=h8_hello(&session,hello);if(n)sceNetSendto(socket_id,hello,n,SCE_NET_MSG_DONTWAIT,(SceNetSockaddr*)&destination,sizeof(destination));hello_at=now+500000;}
   /* Bounded packet budget prevents network flooding from starving stop poll. */
   for(unsigned budget=0;budget<4;budget++){
    SceNetSockaddrIn from={0};unsigned size=sizeof(from);int n=sceNetRecvfrom(socket_id,packet,sizeof(packet),SCE_NET_MSG_DONTWAIT,(SceNetSockaddr*)&from,&size);if(n<=0)break;
    if(n>H8_MAX_PACKET||from.sin_addr.s_addr!=destination.sin_addr.s_addr||from.sin_port!=destination.sin_port)continue;
    if(h8_accept(&session,packet,n,now_us())){ST(authorized_until,session.lease<session.expires?session.lease:session.expires);continue;}
    if(h8_decode(&session,packet,n,now_us(),&type,payload,&length)){
     ST(authorized_until,session.lease<session.expires?session.lease:session.expires);
     memcpy(last_request,packet,n);last_request_len=n;dispatch(type,payload,length);
    }else if(n==last_request_len&&last_response_len&&memcmp(packet,last_request,n)==0){
     /* Exact retry returns cached bytes without executing or renewing lease. */
     sceNetSendto(socket_id,last_response,last_response_len,SCE_NET_MSG_DONTWAIT,(SceNetSockaddr*)&destination,sizeof(destination));
    }
   }
  }
  sceKernelDelayThread(20000);
 }
 ST(armed,0);ST(stop_run,1);ST(pending,0);ST(authorized_until,0);h8_disarm(&session);
 if(socket_id>=0)sceNetSocketClose(socket_id);socket_id=-1;
 if(ctl_owned)sceNetCtlTerm();if(net_owned)sceNetTerm();if(module_owned)sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);free(net);return 0;
}
void h8_init(void){
 main_id=sceKernelGetThreadId();
 /* Random boot namespace avoids overwriting old diagnostic history. */
 uint8_t bytes[8];if(sceKernelGetRandomNumber(bytes,8)!=0)exit(1);
 mkdir(ROOT,0777);snprintf(bootdir,sizeof(bootdir),ROOT"boot-%02x%02x%02x%02x%02x%02x%02x%02x",bytes[0],bytes[1],bytes[2],bytes[3],bytes[4],bytes[5],bytes[6],bytes[7]);if(mkdir(bootdir,0777)<0)exit(1); /* Fail before opening any old output. */
 enabled=config();if(!enabled)return;
 worker=sceKernelCreateThread("rs225-control",worker_main,0x10000100,64*1024,0,0,NULL);if(worker<0)return;
 if(sceKernelStartThread(worker,0,NULL)<0){sceKernelDeleteThread(worker);worker=-1;enabled=0;}
}
void h8_shutdown(void){ST(stop_worker,1);if(worker>=0){SceUInt timeout=3000000;sceKernelWaitThreadEnd(worker,NULL,&timeout);/* process exit owns final cleanup if driver hangs */}memset(provision,0,sizeof(provision));}
int h8_take_run(unsigned*kind,unsigned*seconds,unsigned*ui){
 if(!LD(armed)||now_us()>=LD(authorized_until)||!LD(pending)||LD(stop_run)||LD(running))return 0;
 *kind=request_kind;*seconds=request_seconds;*ui=request_preset;ST(pending,2);run_kind=*kind;run_seconds=*seconds;preset=*ui;return 1;
}
void h8_begin(int x,int z,int yaw,int pitch,int radius,int lowmem){
 initial_x=x;initial_z=z;initial_yaw=yaw;initial_pitch=pitch;initial_radius=radius;initial_lowmem=lowmem;
 ST(running,1);__atomic_fetch_add(&serial,1,__ATOMIC_RELEASE);snprintf(rundir,sizeof(rundir),"%s/run-%04u",bootdir,LD(serial));if(mkdir(rundir,0777)<0){ST(stop_run,1);ST(pending,0);ST(running,0);return;}
 action_index=action_count=0;period_count=0;last_present=0;cpu_start=0;cpu_start_valid=0;thread_count=0;memset(calls,0,sizeof(calls));memset(elapsed,0,sizeof(elapsed));memset(scope_wall,0,sizeof(scope_wall));memset(scope_exclusive,0,sizeof(scope_exclusive));ST(snapshot_done,0);run_at=now_us();measure_at=run_at+5000000;finish_at=measure_at+(uint64_t)run_seconds*1000000;ST(pending,0);
}
int h8_action(unsigned*kind,unsigned*step){
 if(!LD(armed)||now_us()>=LD(authorized_until)||LD(stop_run)||LD(local_override)||LD(running)!=2)return 0;
 uint64_t now=now_us();unsigned due=(unsigned)((now-measure_at)/5000000);
 if(due>3)return 0;
 if(due>action_index){ST(stop_run,1);return 0;}
 if(due!=action_index)return 0;
 *kind=run_kind;*step=action_index++;return 1;
}
void h8_action_result(unsigned kind,unsigned step,int x,int z,int accepted){if(action_count>=4)return;unsigned i=action_count++;actions[i].at=now_us()-measure_at;actions[i].kind=kind;actions[i].step=step;actions[i].x=x;actions[i].z=z;actions[i].accepted=accepted;if(!accepted)ST(stop_run,1);}
extern unsigned h8_list_threads(int*,unsigned);
static void thread_snapshot(int begin,FILE*f){
 SceUID ids[40];unsigned count=0;if(begin){thread_count=0;count=h8_list_threads(ids,40);int duplicate=0;for(unsigned i=0;i<count;i++)if(ids[i]==worker)duplicate=1;if(worker>=0&&count<40&&!duplicate)ids[count++]=worker;for(unsigned i=0;i<count&&thread_count<40;i++){
  SceKernelThreadInfo info={0};info.size=sizeof(info);if(sceKernelGetThreadInfo(ids[i],&info)==0){thread_ids[thread_count]=ids[i];thread_start[thread_count++]=info.runClocks;}}
 }else if(f){fprintf(f,"id,name,rc,begin_cpu_us,end_cpu_us,delta_cpu_us\n");for(unsigned i=0;i<thread_count;i++){SceKernelThreadInfo t={0};t.size=sizeof(t);int rc=sceKernelGetThreadInfo(thread_ids[i],&t);fprintf(f,"%d,%.31s,%d,%llu,%llu,%llu\n",thread_ids[i],rc?"unavailable":t.name,rc,(unsigned long long)thread_start[i],(unsigned long long)(rc?0:t.runClocks),(unsigned long long)(!rc&&t.runClocks>=thread_start[i]?t.runClocks-thread_start[i]:0));}}
}
static void finish(int aborted,int x,int z){
 uint64_t end=now_us();ST(running,3);char path[256];file_path(3,path);FILE*f=fopen(path,"wx");thread_snapshot(0,f);if(f)fclose(f);
 uint64_t duration=end>measure_at?end-measure_at:0;uint64_t presents=__atomic_load_n(&calls[BA_PRESENT],__ATOMIC_RELAXED);
 SceKernelThreadInfo t={0};t.size=sizeof(t);int rc=sceKernelGetThreadInfo(main_id,&t);if(!cpu_start_valid)rc=-1;
 size_t cr=vglMemTotal(VGL_MEM_VRAM),cf=vglMemFree(VGL_MEM_VRAM),pr=vglMemTotal(VGL_MEM_PHYCONT),pf=vglMemFree(VGL_MEM_PHYCONT);
 snprintf(result,sizeof(result),"{\"run\":%u,\"aborted\":%s,\"kind\":%u,\"source\":\"%s\",\"duration_us\":%llu,\"presentations\":%llu,\"client_updates\":%llu,\"main_cpu_delta_us\":%llu,\"cpu_valid\":%s,\"cd_reserved\":%u,\"cd_live_end\":%u,\"phy_reserved\":%u,\"phy_live_end\":%u,\"memory_valid\":%s,\"ui\":%u,\"msaa\":%d,\"arm_mhz\":%d,\"gpu_mhz\":%d,\"start\":[%d,%d,%d,%d,%d,%d],\"end\":[%d,%d],\"period_count\":%u}",serial,aborted?"true":"false",run_kind,HW_R5_SOURCE_ID,(unsigned long long)duration,(unsigned long long)presents,(unsigned long long)calls[BA_LOGIC],(unsigned long long)(!rc&&t.runClocks>=cpu_start?t.runClocks-cpu_start:0),!rc&&t.runClocks>=cpu_start?"true":"false",(unsigned)cr,(unsigned)(cf<=cr?cr-cf:0),(unsigned)pr,(unsigned)(pf<=pr?pr-pf:0),cf<=cr&&pf<=pr?"true":"false",preset,h6_msaa(),scePowerGetArmClockFrequency(),scePowerGetGpuClockFrequency(),initial_x,initial_z,initial_yaw,initial_pitch,initial_radius,initial_lowmem,x,z,period_count);
 file_path(0,path);f=fopen(path,"wx");if(f){fputs(result,f);fclose(f);}
 file_path(1,path);f=fopen(path,"wx");if(f){fputs("interval_us\n",f);for(unsigned i=0;i<period_count;i++)fprintf(f,"%llu\n",(unsigned long long)periods[i]);fclose(f);}
 file_path(2,path);f=fopen(path,"wx");if(f){fputs("stage,calls,total_elapsed_us\n",f);for(unsigned i=0;i<BA_TIMING_COUNT;i++)fprintf(f,"%u,%llu,%llu\n",i,(unsigned long long)calls[i],(unsigned long long)elapsed[i]);fclose(f);}
 file_path(5,path);f=fopen(path,"wx");if(f){fputs("scope,inclusive_elapsed_us,exclusive_elapsed_us\n",f);for(unsigned i=0;i<H3_COUNT;i++)fprintf(f,"%u,%llu,%llu\n",i,(unsigned long long)scope_wall[i],(unsigned long long)scope_exclusive[i]);fclose(f);}
 file_path(6,path);f=fopen(path,"wx");if(f){fputs("since_measure_us,kind,step,target_x_or_yaw,target_z_or_pitch,accepted\n",f);for(unsigned i=0;i<action_count;i++)fprintf(f,"%llu,%u,%u,%d,%d,%d\n",(unsigned long long)actions[i].at,actions[i].kind,actions[i].step,actions[i].x,actions[i].z,actions[i].accepted);fclose(f);}ST(running,0);r10_flush();r10_upload_flush();
}
void h8_end_tick(int x,int z,int yaw,int pitch,int radius,int ready){
 h8_scene(radius,x,z,yaw,pitch);uint64_t now=now_us();int state=LD(running);
 if(!state)return;
 if(!ready||!LD(armed)||now>=LD(authorized_until)||LD(stop_run)||LD(local_override)){finish(1,x,z);return;}
 if(state==1&&now>=measure_at){memset(calls,0,sizeof(calls));memset(elapsed,0,sizeof(elapsed));memset(scope_wall,0,sizeof(scope_wall));memset(scope_exclusive,0,sizeof(scope_exclusive));period_count=0;last_present=0;thread_snapshot(1,NULL);SceKernelThreadInfo t={0};t.size=sizeof(t);cpu_start_valid=sceKernelGetThreadInfo(main_id,&t)==0;cpu_start=cpu_start_valid?t.runClocks:0;measure_at=now;finish_at=now+(uint64_t)run_seconds*1000000;ST(running,2);}
 if(LD(running)==2&&now>=finish_at)finish(0,x,z);
}
void h8_present(void){
 uint64_t now=now_us();if(LD(running)==2){if(last_present&&period_count<8192)periods[period_count++]=now-last_present;last_present=now;}
 if(LD(snapshot_requested)&&!LD(running)&&LD(armed)&&now<LD(authorized_until)){
  ST(snapshot_requested,0);size_t size=960*544*4;uint8_t*image=malloc(size);if(!image){ST(snapshot_done,-1);return;}
  int clear=0;for(unsigned i=0;i<8;i++)if(glGetError()==GL_NO_ERROR){clear=1;break;}
  if(!clear){free(image);ST(snapshot_done,-1);return;}
  glFinish();glReadPixels(0,0,960,544,GL_RGBA,GL_UNSIGNED_BYTE,image);int good=glGetError()==GL_NO_ERROR;
  char path[256];file_path(4,path);FILE*f=good?fopen(path,"wx"):NULL;if(f){good=fwrite(image,1,size,f)==size;if(fclose(f))good=0;}else good=0;free(image);ST(snapshot_done,good?1:-1);
 }
}
