/* Isolated diagnostic capture only, explicitly enabled by a private file.
 * Readback affects timing; capture runs must not be performance measurements. */
#include "r13_capture.h"
#ifdef __vita__
#include <vitaGL.h>
#include <psp2/kernel/threadmgr.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "r21_capture_codec.h"
static int enabled=-1,shot,xyz[5],collect;static uint64_t start;static FILE*draws;
static const unsigned times[]={13,16,19,22,25,28,31,34,40,46};
void r13_scene(int x,int y,int z,int yaw,int pitch){
 if(enabled<0){enabled=0;FILE*f=fopen("ux0:/data/RS2Vita/r13-capture.txt","r");if(f){fscanf(f,"%d",&enabled);fclose(f);}}
 if(!enabled)return;uint64_t now=sceKernelGetSystemTimeWide();if(!start)start=now;
 xyz[0]=x;xyz[1]=y;xyz[2]=z;xyz[3]=yaw;xyz[4]=pitch;
 collect=shot<10&&now-start>=(uint64_t)times[shot]*1000000;
}
void r13_draw(unsigned low,unsigned high,unsigned count){
 if(!collect)return;if(!draws){draws=fopen("ux0:/data/RS2Vita/r13-draws.csv","w");if(draws)fputs("low,high,count,stride\n",draws);}
 if(draws)fprintf(draws,"%u,%u,%u,24\n",low,high,count);
}
static int map_mode=-1,map_done,map_token;static uint64_t map_first;
void r13_request_snapshot_token(int token){map_token=token;map_mode=1;map_done=0;map_first=sceKernelGetSystemTimeWide()-3000000ull;}
void r13_request_snapshot(void){r13_request_snapshot_token(0);}
void r13_note_ingame_frame(void){
 if(map_done||map_first)return;
 if(map_mode<0){map_mode=0;FILE*f=fopen("ux0:/data/RS2Vita/minimap-shot.txt","r");if(f){if(fscanf(f,"%d",&map_mode)!=1)map_mode=0;fclose(f);}}
 if(map_mode==1)map_first=sceKernelGetSystemTimeWide();else map_done=1;
}
static void r13_minimap_shot(void){
 unsigned char*bytes;FILE*f;GLenum error;size_t wrote=0;uint64_t now;
 if(map_done||!map_first)return;
 now=sceKernelGetSystemTimeWide();
 if(now-map_first<3000000ull)return;
 map_done=1;
 bytes=malloc(960*544*4);if(!bytes)return;
 glReadPixels(0,0,960,544,GL_RGBA,GL_UNSIGNED_BYTE,bytes);error=glGetError();
 f=error==GL_NO_ERROR?fopen("ux0:/data/RS2Vita/minimap-shot.rgba","wb"):NULL;
 if(f){wrote=fwrite(bytes,1,960*544*4,f);fclose(f);}
 size_t packed_bytes=0,capacity=960u*544u*4u+(960u*544u*4u)/128u+32u;
 unsigned char*packed=error==GL_NO_ERROR?malloc(capacity):NULL;
 if(packed){size_t n=r21_capture_pack(bytes,960u*544u*4u,packed,capacity);if(n){FILE*g=fopen("ux0:/data/RS2Vita/minimap-shot.r21","wb");if(g){packed_bytes=fwrite(packed,1,n,g);fclose(g);if(packed_bytes!=n)packed_bytes=0;}}free(packed);}
 free(bytes);
 f=fopen("ux0:/data/RS2Vita/minimap-shot.csv","w");
 if(f){fprintf(f,"elapsed_us,gl_error,bytes,absolute_us,token,packed_bytes\n%llu,%u,%u,%llu,%d,%u\n",(unsigned long long)(now-map_first),error,(unsigned)wrote,(unsigned long long)now,map_token,(unsigned)packed_bytes);fclose(f);}
}
void r13_capture_before_present(void){
 r13_minimap_shot();
 if(!enabled||!start||shot>=10||sceKernelGetSystemTimeWide()-start<(uint64_t)times[shot]*1000000)return;
 unsigned char*bytes=malloc(960*544*4);if(!bytes)return;
 glReadPixels(0,0,960,544,GL_RGBA,GL_UNSIGNED_BYTE,bytes);GLenum error=glGetError();
 char path[200];snprintf(path,sizeof(path),"ux0:/data/RS2Vita/capture-%02d.rgba",shot);
 FILE*f=error==GL_NO_ERROR?fopen(path,"wb"):NULL;size_t wrote=0;if(f){wrote=fwrite(bytes,1,960*544*4,f);fclose(f);}free(bytes);
 f=fopen("ux0:/data/RS2Vita/r13-captures.csv",shot?"a":"w");
 if(f){if(!shot)fputs("shot,elapsed_us,x,y,z,yaw,pitch,gl_error,bytes\n",f);fprintf(f,"%d,%llu,%d,%d,%d,%d,%d,%u,%u\n",shot,(unsigned long long)(sceKernelGetSystemTimeWide()-start),xyz[0],xyz[1],xyz[2],xyz[3],xyz[4],error,(unsigned)wrote);fclose(f);}
 if(draws){fclose(draws);draws=NULL;}collect=0;shot++;
}
static FILE*selected;
int r14_selected_begin(unsigned count){
 if(!collect)return 0;char path[200];snprintf(path,sizeof(path),"ux0:/data/RS2Vita/selected-%02d.bin",shot);
 selected=fopen(path,"wb");if(!selected)return 0;fwrite(&count,4,1,selected);return 1;
}
void r14_selected_vertex(const void*v){if(selected)fwrite(v,24,1,selected);}
void r14_selected_end(void){if(selected){fclose(selected);selected=NULL;}}

#else
void r13_request_snapshot(void){}
void r13_request_snapshot_token(int token){(void)token;}
int r14_selected_begin(unsigned n){(void)n;return 0;}
void r14_selected_vertex(const void*v){(void)v;}
void r14_selected_end(void){}
void r13_scene(int x,int y,int z,int yaw,int pitch){(void)x;(void)y;(void)z;(void)yaw;(void)pitch;}
void r13_draw(unsigned low,unsigned high,unsigned count){(void)low;(void)high;(void)count;}
void r13_capture_before_present(void){}
void r13_note_ingame_frame(void){}
#endif
