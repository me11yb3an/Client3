/* Diagnostic-only SPSC transport. Network work is never performed by push(). */
#include "vita-telemetry.h"
#include <psp2/kernel/threadmgr.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/sysmodule.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <ctype.h>
static unsigned char *net_memory;
static TelemetrySample queue[8];
static unsigned write_at,read_at,dropped,send_errors,stop_flag,acked;
static int socket_id=-1,net_owned,ctl_owned,module_owned;static SceUID worker=-1;
static SceNetSockaddrIn destination;static char token[33],run_id[65],build_id[65];static uint64_t seq;
static void cleanup(void){if(socket_id>=0){sceNetSocketClose(socket_id);socket_id=-1;}if(ctl_owned)sceNetCtlTerm();if(net_owned)sceNetTerm();if(module_owned)sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);ctl_owned=net_owned=module_owned=0;free(net_memory);net_memory=NULL;}
static int worker_main(SceSize size,void*arg){
 (void)size;(void)arg;uint64_t hello_at=0;char packet[1400];
 while(!__atomic_load_n(&stop_flag,__ATOMIC_ACQUIRE)||__atomic_load_n(&read_at,__ATOMIC_ACQUIRE)!=__atomic_load_n(&write_at,__ATOMIC_ACQUIRE)){
  uint64_t now=sceKernelGetSystemTimeWide();
  if(now>=hello_at&&!__atomic_load_n(&stop_flag,__ATOMIC_ACQUIRE)){
   __atomic_store_n(&acked,0,__ATOMIC_RELEASE);
   char nonce[32];snprintf(nonce,sizeof(nonce),"%llx",(unsigned long long)now);
   int n=snprintf(packet,sizeof(packet),"{\"v\":1,\"kind\":\"hello\",\"token\":\"%s\",\"run\":\"%s\",\"build\":\"%s\",\"seq\":%llu,\"nonce\":\"%s\"}",token,run_id,build_id,(unsigned long long)seq++,nonce);
   if(sceNetSendto(socket_id,packet,n,SCE_NET_MSG_DONTWAIT,(SceNetSockaddr*)&destination,sizeof(destination))!=n)__atomic_fetch_add(&send_errors,1,__ATOMIC_RELAXED);hello_at=now+5000000;
   /* ACK is informational only; no command or configuration messages accepted. */
   for(int retry=0;retry<5&&!__atomic_load_n(&stop_flag,__ATOMIC_ACQUIRE);retry++){
    SceNetSockaddrIn from={0};unsigned fromlen=sizeof(from);int got=sceNetRecvfrom(socket_id,packet,sizeof(packet)-1,SCE_NET_MSG_DONTWAIT,(SceNetSockaddr*)&from,&fromlen);
    if(got>0&&from.sin_addr.s_addr==destination.sin_addr.s_addr&&from.sin_port==destination.sin_port){
     packet[got]=0;char expected[180];snprintf(expected,sizeof(expected),"{\"v\":1,\"kind\":\"ack\",\"run\":\"%s\",\"nonce\":\"%s\"}",run_id,nonce);
     if(!strcmp(packet,expected))__atomic_store_n(&acked,1,__ATOMIC_RELEASE);
    }
    sceKernelDelayThread(20000);
   }
  }
  unsigned rd=__atomic_load_n(&read_at,__ATOMIC_RELAXED),wr=__atomic_load_n(&write_at,__ATOMIC_ACQUIRE);
  if(rd!=wr){
   TelemetrySample s=queue[rd%8];__atomic_store_n(&read_at,rd+1,__ATOMIC_RELEASE);
   int n;
   if(s.kind==1){
    n=snprintf(packet,sizeof(packet),"{\"v\":1,\"kind\":\"threads\",\"token\":\"%s\",\"run\":\"%s\",\"build\":\"%s\",\"seq\":%llu,\"wall_us\":%llu,\"tracked\":%d,\"batch_start\":%d,\"threads\":[[%d,%d,%llu],[%d,%d,%llu],[%d,%d,%llu],[%d,%d,%llu]]}",token,run_id,build_id,(unsigned long long)seq++,(unsigned long long)s.wall_us,s.tracked_threads,s.batch_start,s.tid[0],s.thread_rc[0],(unsigned long long)s.thread_clocks[0],s.tid[1],s.thread_rc[1],(unsigned long long)s.thread_clocks[1],s.tid[2],s.thread_rc[2],(unsigned long long)s.thread_clocks[2],s.tid[3],s.thread_rc[3],(unsigned long long)s.thread_clocks[3]);
   }else if(s.kind==2){
    n=snprintf(packet,sizeof(packet),"{\"v\":1,\"kind\":\"memory\",\"token\":\"%s\",\"run\":\"%s\",\"build\":\"%s\",\"seq\":%llu,\"wall_us\":%llu,\"native_phy_live\":%llu,\"native_cdram_live\":%llu,\"native_phy_peak\":%llu,\"native_cdram_peak\":%llu,\"native_valid\":%d,\"os_user_free\":%llu,\"os_phy_free\":%llu,\"os_valid\":%d}",token,run_id,build_id,(unsigned long long)seq++,(unsigned long long)s.wall_us,(unsigned long long)s.native_phy_live,(unsigned long long)s.native_cdram_live,(unsigned long long)s.native_phy_peak,(unsigned long long)s.native_cdram_peak,s.native_valid,(unsigned long long)s.os_user_free,(unsigned long long)s.os_phy_free,s.os_valid);
   }else{
n=snprintf(packet,sizeof(packet),"{\"v\":1,\"kind\":\"sample\",\"token\":\"%s\",\"run\":\"%s\",\"build\":\"%s\",\"seq\":%llu,\"wall_us\":%llu,\"client_updates\":%llu,\"render_calls\":%llu,\"present_calls\":%llu,\"logic_us\":%llu,\"render_us\":%llu,\"convert_us\":%llu,\"upload_us\":%llu,\"server_ticks\":%llu,\"server_us\":%llu,\"main_cpu_us\":%llu,\"telemetry_cpu_us\":%llu,\"cpu_valid\":%d,\"vgl_ram_capacity\":%llu,\"vgl_ram_used\":%llu,\"vgl_cdram_capacity\":%llu,\"vgl_cdram_used\":%llu,\"vgl_phy_capacity\":%llu,\"vgl_phy_used\":%llu,\"memory_valid\":%d,\"graphics_attempts\":%llu,\"gxm_attempts\":%llu,\"ack_observed\":%u,\"probe_only\":%d,\"dropped\":%u}",token,run_id,build_id,(unsigned long long)seq++,
    (unsigned long long)s.wall_us,(unsigned long long)s.client_updates,(unsigned long long)s.render_calls,(unsigned long long)s.present_calls,(unsigned long long)s.logic_us,(unsigned long long)s.render_us,(unsigned long long)s.convert_us,(unsigned long long)s.upload_us,(unsigned long long)s.server_ticks,(unsigned long long)s.server_us,(unsigned long long)s.main_cpu_us,(unsigned long long)s.telemetry_cpu_us,s.cpu_valid,(unsigned long long)s.ram_reserved,(unsigned long long)s.ram_used,(unsigned long long)s.cdram_reserved,(unsigned long long)s.cdram_used,(unsigned long long)s.phy_reserved,(unsigned long long)s.phy_used,s.memory_valid,(unsigned long long)s.graphics_attempts,(unsigned long long)s.gxm_attempts,__atomic_load_n(&acked,__ATOMIC_ACQUIRE),s.probe_only,__atomic_load_n(&dropped,__ATOMIC_RELAXED));
   

   }
   if(n>0&&n<(int)sizeof(packet)){if(sceNetSendto(socket_id,packet,n,SCE_NET_MSG_DONTWAIT,(SceNetSockaddr*)&destination,sizeof(destination))!=n)__atomic_fetch_add(&send_errors,1,__ATOMIC_RELAXED);}else __atomic_fetch_add(&dropped,1,__ATOMIC_RELAXED);
  }else sceKernelDelayThread(20000);
 }
 return 0;
}
int telemetry_start(const char*config,const char*run,const char*build){
 if(worker>=0)return -1;
 FILE*f=fopen(config,"r");if(!f)return 0;char host[32],extra;unsigned port;int count=fscanf(f," %31s %u %32s %c",host,&port,token,&extra);fclose(f);
 if(count!=3||!port||port>65535||strlen(token)!=32||strlen(run)>64||strlen(build)>64)return -2;
 for(unsigned i=0;i<32;i++)if(!isxdigit((unsigned char)token[i]))return -2;
 for(const char*p=run;*p;p++)if(!isalnum((unsigned char)*p)&&*p!='-'&&*p!='_')return -2;
 for(const char*p=build;*p;p++)if(!isalnum((unsigned char)*p)&&*p!='-'&&*p!='_')return -2;
 /* Pure-C validation before the Net module is loaded: no imported Net call yet. */
 unsigned ip[4];char tail;
 if(sscanf(host,"%u.%u.%u.%u%c",ip,ip+1,ip+2,ip+3,&tail)!=4||ip[0]>255||ip[1]>255||ip[2]>255||ip[3]>255)return -2;
 if(!(ip[0]==10||(ip[0]==172&&ip[1]>=16&&ip[1]<=31)||(ip[0]==192&&ip[1]==168))||ip[3]==255||ip[3]==0)return -2;
 strcpy(run_id,run);strcpy(build_id,build);seq=0;write_at=read_at=dropped=send_errors=stop_flag=acked=0;
 int rc=0;
 if(sceSysmoduleIsLoaded(SCE_SYSMODULE_NET)!=0){rc=sceSysmoduleLoadModule(SCE_SYSMODULE_NET);if(rc<0)return rc;module_owned=1;}
 net_memory=memalign(64,1024*1024);if(!net_memory){cleanup();return -3;}
 SceNetInitParam params={net_memory,1024*1024,0};rc=sceNetInit(&params);if(rc<0){cleanup();return rc;}net_owned=1;
 rc=sceNetCtlInit();if(rc<0){cleanup();return rc;}ctl_owned=1;
 memset(&destination,0,sizeof(destination));destination.sin_len=sizeof(destination);destination.sin_family=SCE_NET_AF_INET;destination.sin_port=sceNetHtons(port);
 if(sceNetInetPton(SCE_NET_AF_INET,host,&destination.sin_addr)!=1){cleanup();return -2;}
 socket_id=sceNetSocket("rs225-telemetry",SCE_NET_AF_INET,SCE_NET_SOCK_DGRAM,0);if(socket_id<0){rc=socket_id;cleanup();return rc;}
 int nonblocking=1;rc=sceNetSetsockopt(socket_id,SCE_NET_SOL_SOCKET,SCE_NET_SO_NBIO,&nonblocking,sizeof(nonblocking));if(rc<0){cleanup();return rc;}
 worker=sceKernelCreateThread("rs225-telemetry",worker_main,0x10000100,64*1024,0,0,NULL);if(worker<0){rc=worker;cleanup();return rc;}
 rc=sceKernelStartThread(worker,0,NULL);if(rc<0){sceKernelDeleteThread(worker);worker=-1;cleanup();return rc;}return 1;
}
void telemetry_push(const TelemetrySample*s){if(worker<0)return;unsigned wr=__atomic_load_n(&write_at,__ATOMIC_RELAXED),rd=__atomic_load_n(&read_at,__ATOMIC_ACQUIRE);if(wr-rd>=8){__atomic_fetch_add(&dropped,1,__ATOMIC_RELAXED);return;}queue[wr%8]=*s;__atomic_store_n(&write_at,wr+1,__ATOMIC_RELEASE);}
void telemetry_stop(void){if(worker>=0){__atomic_store_n(&stop_flag,1,__ATOMIC_RELEASE);sceKernelWaitThreadEnd(worker,NULL,NULL);sceKernelDeleteThread(worker);worker=-1;}cleanup();}
int telemetry_acknowledged(void){return __atomic_load_n(&acked,__ATOMIC_ACQUIRE);}

uint64_t telemetry_thread_cpu_us(int*valid){SceKernelThreadInfo info={0};info.size=sizeof(info);int rc=worker<0?-1:sceKernelGetThreadInfo(worker,&info);*valid=rc==0;return rc==0?info.runClocks:0;}

int telemetry_thread_id(void){return worker;}

void telemetry_stats(unsigned*d,unsigned*e){*d=__atomic_load_n(&dropped,__ATOMIC_RELAXED);*e=__atomic_load_n(&send_errors,__ATOMIC_RELAXED);}
