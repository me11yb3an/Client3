#include <pthread.h>
/* Render-side producers copy bounded text under a short lock. Storage runs on
 * the world worker; neither file I/O nor file close holds the producer lock. */
static pthread_mutex_t ba_cadence_data_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t ba_cadence_io_lock=PTHREAD_MUTEX_INITIALIZER;
static char ba_cadence_mem[131072],ba_cadence_drain[131072];
static unsigned ba_cadence_len,ba_cadence_drop;
static char ba_guest_line[128];static int ba_guest_ready;
static void ba_cadence_note_drop(void){pthread_mutex_lock(&ba_cadence_data_lock);ba_cadence_drop++;pthread_mutex_unlock(&ba_cadence_data_lock);}
static void ba_cadence_line(const char*line){
 size_t n=strlen(line);pthread_mutex_lock(&ba_cadence_data_lock);
 if(ba_cadence_len+n+1>=sizeof ba_cadence_mem)ba_cadence_drop++;
 else {memcpy(ba_cadence_mem+ba_cadence_len,line,n);ba_cadence_len+=(unsigned)n;ba_cadence_mem[ba_cadence_len++]='\n';}
 pthread_mutex_unlock(&ba_cadence_data_lock);
}
static void ba_cadence_set_guest(const char*line){pthread_mutex_lock(&ba_cadence_data_lock);snprintf(ba_guest_line,sizeof ba_guest_line,"%s",line);ba_guest_ready=1;pthread_mutex_unlock(&ba_cadence_data_lock);}
static void ba_cadence_flush(void){
 pthread_mutex_lock(&ba_cadence_io_lock);pthread_mutex_lock(&ba_cadence_data_lock);
 unsigned n=ba_cadence_len,dropped=ba_cadence_drop;int guest=ba_guest_ready;char line[128];
 memcpy(ba_cadence_drain,ba_cadence_mem,n);memcpy(line,ba_guest_line,sizeof line);
 ba_cadence_len=ba_cadence_drop=0;ba_guest_ready=0;pthread_mutex_unlock(&ba_cadence_data_lock);
 if(n){FILE*f=fopen("ux0:/data/RS2Vita/frame-cadence.txt","a");if(f){fwrite(ba_cadence_drain,1,n,f);if(dropped)fprintf(f,"# cadence_dropped %u\n",dropped);fclose(f);}}
 if(guest){FILE*f=fopen("ux0:/data/RS2Vita/guest-fps.txt","w");if(f){fputs(line,f);fclose(f);}}
 pthread_mutex_unlock(&ba_cadence_io_lock);
}
void ba_cadence_background_flush(void){static uint64_t due;uint64_t now=ba_timing_now();if(now>=due){ba_cadence_flush();due=now+10000000ull;}}
