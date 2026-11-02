/* Isolated linker-wrapper prototype: never linked into the HW-R6 renderer VPK.
 * All eight native server entry points execute on one 2 MiB worker. Request
 * pointers remain valid because each caller waits for completion. The original
 * generation-tagged 256 KiB byte queues remain exclusively worker-owned. */
#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <assert.h>
#ifdef __vita__
#include <psp2/kernel/threadmgr.h>
#include "ba_hw_r7.h"
static SceUID wake_sema=-1;static int owner_id;
#define H7_ON() h7_worker_enabled()
#else
#define H7_ON() 1
#endif
static uint64_t rpc_calls,rpc_wait_ns;static size_t cached_heap;
size_t __real_ba_server_heap_bytes(void);
enum {START,READY,PUMP,CONNECT,WRITE,READ,DISCONNECT,STOP};
typedef struct Request {int op,result,done;void*bytes;size_t size;pthread_cond_t finished;} Request;
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t lifecycle=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t available=PTHREAD_COND_INITIALIZER,space=PTHREAD_COND_INITIALIZER;
static pthread_t worker;
static Request*queue[8];static unsigned head,count,peak;static int running,closed;
static char paths[4][1024];
int __real_ba_server_start(const char*,const char*,const char*,const char*);
int __real_ba_server_ready(void),__real_ba_server_pump(void),__real_ba_server_connect(void),__real_ba_server_disconnect(void),__real_ba_server_stop(void);
int __real_ba_server_write(const void*,size_t),__real_ba_server_read(void*,size_t);
int __wrap_ba_server_stop(void);
static int stop_join(void);
static uint64_t mono(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return(uint64_t)t.tv_sec*1000000000u+t.tv_nsec;}
static void*owner(void*unused){
 (void)unused;
#ifdef __vita__
 owner_id=sceKernelGetThreadId();
#endif
 int started=0;uint64_t next_pump=mono()+5000000u;
 for(;;){
  pthread_mutex_lock(&lock);
  if(!count){
#ifdef __vita__
   pthread_mutex_unlock(&lock);SceUInt timeout=5000;sceKernelWaitSema(wake_sema,1,&timeout);pthread_mutex_lock(&lock);
#else
   struct timespec wake;clock_gettime(CLOCK_REALTIME,&wake);wake.tv_nsec+=5000000;if(wake.tv_nsec>=1000000000){wake.tv_sec++;wake.tv_nsec-=1000000000;}pthread_cond_timedwait(&available,&lock,&wake);
#endif
  }
  Request*r=NULL;if(count){r=queue[head];head=(head+1)%8;count--;pthread_cond_broadcast(&space);}
  pthread_mutex_unlock(&lock);
#ifdef __vita__
  if(r)sceKernelPollSema(wake_sema,1);
#endif
  int quit=0;
  if(r){
   switch(r->op){
    case START:r->result=__real_ba_server_start(paths[0],paths[1],paths[2],paths[3]);started=r->result==0;break;
    case READY:r->result=__real_ba_server_ready();break;
    case PUMP:r->result=__real_ba_server_pump();break;
    case CONNECT:r->result=__real_ba_server_connect();break;
    case WRITE:r->result=__real_ba_server_write(r->bytes,r->size);break;
    case READ:r->result=__real_ba_server_read(r->bytes,r->size);break;
    case DISCONNECT:r->result=__real_ba_server_disconnect();break;
    case STOP:r->result=__real_ba_server_stop();started=0;quit=1;break;
    default:r->result=-1;
   }
   __atomic_store_n(&cached_heap,__real_ba_server_heap_bytes(),__ATOMIC_RELEASE);
   pthread_mutex_lock(&lock);r->done=1;pthread_cond_signal(&r->finished);pthread_mutex_unlock(&lock);
  }
  if(quit)break;
  uint64_t now=mono();if(started&&now>=next_pump){__real_ba_server_pump();__atomic_store_n(&cached_heap,__real_ba_server_heap_bytes(),__ATOMIC_RELEASE);next_pump=mono()+5000000u;}
 }
 return NULL;
}
static int rpc(int op,void*bytes,size_t size){
 uint64_t began=mono();Request r={.op=op,.result=-1,.bytes=bytes,.size=size};if(pthread_cond_init(&r.finished,NULL))return-1;
 pthread_mutex_lock(&lock);
 while(running&&!closed&&count==8)pthread_cond_wait(&space,&lock);
 if(!running||closed){pthread_mutex_unlock(&lock);pthread_cond_destroy(&r.finished);return-1;}
 queue[(head+count)%8]=&r;if(++count>peak)peak=count;
 if(op==STOP){closed=1;pthread_cond_broadcast(&space);}
 pthread_cond_signal(&available);
#ifdef __vita__
 sceKernelSignalSema(wake_sema,1);
#endif
 while(!r.done)pthread_cond_wait(&r.finished,&lock);
 rpc_calls++;rpc_wait_ns+=mono()-began;pthread_mutex_unlock(&lock);pthread_cond_destroy(&r.finished);return r.result;
}
int __wrap_ba_server_start(const char*a,const char*b,const char*c,const char*d){
 if(!H7_ON())return __real_ba_server_start(a,b,c,d);
 const char*args[]={a,b,c,d};pthread_mutex_lock(&lifecycle);pthread_mutex_lock(&lock);
 if(running){pthread_mutex_unlock(&lock);pthread_mutex_unlock(&lifecycle);return-1;}
 for(unsigned i=0;i<4;i++){if(!args[i]||strlen(args[i])>=sizeof(paths[i])){pthread_mutex_unlock(&lock);pthread_mutex_unlock(&lifecycle);return-1;}strcpy(paths[i],args[i]);}
 head=count=peak=0;closed=0;rpc_calls=rpc_wait_ns=0;
#ifdef __vita__
 wake_sema=sceKernelCreateSema("h7-server-wake",0,0,8,NULL);if(wake_sema<0){pthread_mutex_unlock(&lock);pthread_mutex_unlock(&lifecycle);return -1;}
#endif
 pthread_attr_t attr;if(pthread_attr_init(&attr)){
#ifdef __vita__
 sceKernelDeleteSema(wake_sema);wake_sema=-1;
#endif
 pthread_mutex_unlock(&lock);pthread_mutex_unlock(&lifecycle);return-1;}
 int rc=pthread_attr_setstacksize(&attr,2u*1024u*1024u);if(!rc)rc=pthread_create(&worker,&attr,owner,NULL);pthread_attr_destroy(&attr);
 if(rc){
#ifdef __vita__
 sceKernelDeleteSema(wake_sema);wake_sema=-1;
#endif
 pthread_mutex_unlock(&lock);pthread_mutex_unlock(&lifecycle);return-1;}running=1;pthread_mutex_unlock(&lock);
 rc=rpc(START,NULL,0);if(rc)stop_join();pthread_mutex_unlock(&lifecycle);return rc;
}
int __wrap_ba_server_ready(void){if(!H7_ON())return __real_ba_server_ready();return rpc(READY,NULL,0);}int __wrap_ba_server_pump(void){if(!H7_ON())return __real_ba_server_pump();return rpc(PUMP,NULL,0);}
int __wrap_ba_server_connect(void){if(!H7_ON())return __real_ba_server_connect();return rpc(CONNECT,NULL,0);}int __wrap_ba_server_disconnect(void){if(!H7_ON())return __real_ba_server_disconnect();return rpc(DISCONNECT,NULL,0);}
int __wrap_ba_server_write(const void*b,size_t n){if(!H7_ON())return __real_ba_server_write(b,n);return rpc(WRITE,(void*)b,n);}int __wrap_ba_server_read(void*b,size_t n){if(!H7_ON())return __real_ba_server_read(b,n);return rpc(READ,b,n);}
static int stop_join(void){
 pthread_mutex_lock(&lock);int live=running;pthread_mutex_unlock(&lock);if(!live)return 0;
 int rc=rpc(STOP,NULL,0);
 pthread_join(worker,NULL);
#ifdef __vita__
 sceKernelDeleteSema(wake_sema);wake_sema=-1;owner_id=0;
#endif
 pthread_mutex_lock(&lock);running=0;pthread_cond_broadcast(&space);pthread_mutex_unlock(&lock);return rc;
}
int __wrap_ba_server_stop(void){if(!H7_ON())return __real_ba_server_stop();pthread_mutex_lock(&lifecycle);int rc=stop_join();pthread_mutex_unlock(&lifecycle);return rc;}
unsigned r6_owner_peak_requests(void){pthread_mutex_lock(&lock);unsigned n=peak;pthread_mutex_unlock(&lock);return n;}
#ifndef __vita__
uint64_t r6_owner_cpu_ns(void){clockid_t id;struct timespec t;pthread_mutex_lock(&lock);int rc=running?pthread_getcpuclockid(worker,&id):-1;pthread_mutex_unlock(&lock);if(rc||clock_gettime(id,&t))return 0;return(uint64_t)t.tv_sec*1000000000u+t.tv_nsec;}
#endif

size_t __wrap_ba_server_heap_bytes(void){return H7_ON()?__atomic_load_n(&cached_heap,__ATOMIC_ACQUIRE):__real_ba_server_heap_bytes();}
int h7_owner_thread_id(void){
#ifdef __vita__
 return __atomic_load_n(&owner_id,__ATOMIC_ACQUIRE);
#else
 return 0;
#endif
}
void h7_owner_snapshot(uint64_t*c,uint64_t*w,unsigned*p){pthread_mutex_lock(&lock);*c=rpc_calls;*w=rpc_wait_ns;*p=peak;pthread_mutex_unlock(&lock);}
