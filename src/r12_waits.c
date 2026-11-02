/* HW-R12 diagnostic only. Main-participant API wall waits, not GPU timestamps. */
#include "r12_waits.h"
#if BA_R12_DIAG
#include <psp2/gxm.h>
#include <psp2/kernel/threadmgr.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "gl11.h"
#ifndef BA_R12_PROBE
#define BA_R12_PROBE 0
#endif
static atomic_int owner;
extern int r16_diag_verbose(void);
static uint64_t values[8],frame;static FILE*out;
static uint64_t prefix_vertices,span_vertices,index_count;
static int measured(void){return atomic_load(&owner)==sceKernelGetThreadId();}
int __real_sceGxmDisplayQueueAddEntry(SceGxmSyncObject*,SceGxmSyncObject*,const void*);
void __real_sceGxmFinish(SceGxmContext*);
int __real_sceKernelWaitSema(SceUID,int,SceUInt*);
int __real_sceGxmEndScene(SceGxmContext*,const SceGxmNotification*,const SceGxmNotification*);
int __wrap_sceGxmDisplayQueueAddEntry(SceGxmSyncObject*a,SceGxmSyncObject*b,const void*c){if(!measured())return __real_sceGxmDisplayQueueAddEntry(a,b,c);uint64_t t=sceKernelGetSystemTimeWide();int rc=__real_sceGxmDisplayQueueAddEntry(a,b,c);values[0]+=sceKernelGetSystemTimeWide()-t;values[4]++;return rc;}
int __wrap_sceKernelWaitSema(SceUID id,int n,SceUInt*timeout){if(!measured())return __real_sceKernelWaitSema(id,n,timeout);uint64_t t=sceKernelGetSystemTimeWide();int rc=__real_sceKernelWaitSema(id,n,timeout);values[2]+=sceKernelGetSystemTimeWide()-t;values[6]++;return rc;}
void r12_wait_record(unsigned stage,unsigned long long us){if(stage<4&&measured()){values[stage]+=us;values[stage+4]++;}}
void r12_index_range(unsigned low,unsigned high,unsigned n){prefix_vertices+=(uint64_t)high+1;span_vertices+=(uint64_t)high-low+1;index_count+=n;}
void r12_begin(void){if(!r16_diag_verbose()){atomic_store(&owner,0);return;}atomic_store(&owner,sceKernelGetThreadId());memset(values,0,sizeof(values));prefix_vertices=span_vertices=index_count=0;frame++;}
static uint64_t idle_us;
void r12_before_present(void){idle_us=0;
#if BA_R12_PROBE == 1
 glFinish();
#elif BA_R12_PROBE == 2
 uint64_t t=sceKernelGetSystemTimeWide();sceKernelDelayThread(25000);idle_us=sceKernelGetSystemTimeWide()-t;
#endif
}
void r12_end(void){if(!measured())return;if(!out){out=fopen("ux0:/data/RS2Vita/r12-waits.csv","w");if(!out)return;fputs("frame,display_queue_us,finish_us,semaphore_us,end_scene_us,display_queue_calls,finish_calls,semaphore_calls,end_scene_calls,idle_probe_us,prefix_vertices,span_vertices,index_count\n",out);setvbuf(out,NULL,_IOFBF,32768);}fprintf(out,"%llu",(unsigned long long)frame);for(unsigned n=0;n<8;n++)fprintf(out,",%llu",(unsigned long long)values[n]);fprintf(out,",%llu,%llu,%llu,%llu\n",(unsigned long long)idle_us,(unsigned long long)prefix_vertices,(unsigned long long)span_vertices,(unsigned long long)index_count);if(frame%30==0)fflush(out);}
#endif
