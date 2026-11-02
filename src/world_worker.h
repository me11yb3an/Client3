#ifndef BA_WORLD_WORKER_H
#define BA_WORLD_WORKER_H
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
#include <errno.h>
/* One serialized JS owner. Render-side stream reads never acquire this lock. */
typedef struct {
 pthread_t thread;
 pthread_mutex_t runtime;
 atomic_int started, running, ready;
 void *ctx;
 int (*pump)(void*);
 int (*refresh)(void*);
 void (*enter)(void*);
} BaWorldWorker;
#define BA_WORLD_WORKER_INIT {.runtime=PTHREAD_MUTEX_INITIALIZER}
static void *ba_world_thread(void *arg){
 BaWorldWorker*w=arg;
 while(atomic_load_explicit(&w->running,memory_order_acquire)){
  pthread_mutex_lock(&w->runtime);
  if(!atomic_load_explicit(&w->running,memory_order_relaxed)){pthread_mutex_unlock(&w->runtime);break;}
  w->enter(w->ctx);int rc=w->pump(w->ctx);int ready=rc?-1:w->refresh(w->ctx);
  atomic_store_explicit(&w->ready,ready,memory_order_release);
  pthread_mutex_unlock(&w->runtime);
  if(ready<0)break;
  struct timespec nap={0,2000000};while(nanosleep(&nap,&nap)&&errno==EINTR){}
 }
 return NULL;
}
static int ba_world_start(BaWorldWorker*w,void*ctx,int(*pump)(void*),int(*refresh)(void*),void(*enter)(void*),int initial){
 if(atomic_load(&w->started))return -1;
 w->ctx=ctx;w->pump=pump;w->refresh=refresh;w->enter=enter;
 atomic_store(&w->ready,initial);atomic_store(&w->running,1);atomic_store(&w->started,1);
 pthread_attr_t attr;int rc=pthread_attr_init(&attr);if(rc){atomic_store(&w->started,0);atomic_store(&w->running,0);return rc;}
 rc=pthread_attr_setstacksize(&attr,2u*1024u*1024u);
 if(!rc)rc=pthread_create(&w->thread,&attr,ba_world_thread,w);
 pthread_attr_destroy(&attr);
 if(rc){atomic_store(&w->started,0);atomic_store(&w->running,0);}return rc;
}
static void ba_world_stop(BaWorldWorker*w){
 if(!atomic_load(&w->started))return;
 atomic_store(&w->running,0);pthread_join(w->thread,NULL);atomic_store(&w->started,0);
}
typedef struct {BaWorldWorker*w;} BaWorldGuard;
static BaWorldGuard ba_world_enter(BaWorldWorker*w){
 BaWorldGuard g={0};if(atomic_load(&w->started)){pthread_mutex_lock(&w->runtime);w->enter(w->ctx);g.w=w;}return g;
}
static void ba_world_leave(BaWorldGuard*g){if(g->w)pthread_mutex_unlock(&g->w->runtime);}
#endif
