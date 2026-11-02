#include "r12_waits.h"
/* Local diagnostic: nested exclusive CPU wall times; no GPU completion claim. */
#include "r11_diag.h"
#if BA_R11_DIAG
#include "ba_timing.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
typedef struct {uint64_t at,child;} Stack;
static Stack stack[64];static unsigned depth;
static uint64_t excl[R11_STAGES],incl[R11_STAGES],calls[R11_STAGES];
static uint64_t start,previous,frame,period,runs,static_static,static_dynamic,dynamic_static,uploads,bytes,creates,deletes,vertices,ix,capacity,hits,misses,builds,cache_bytes,dynamic_bytes,cache_cpu,patterns;
static uint32_t lasttag;static int hasrun;static FILE*out;
extern int r16_diag_verbose(void);
R11Scope r11_enter(unsigned s){R11Scope v={s,0,0,0};if(start){assert(depth<64);v.at=ba_timing_now();v.depth=depth;stack[depth++]=(Stack){v.at,0};}return v;}
void r11_leave(R11Scope*v){if(!v->at)return;assert(depth==v->depth+1);uint64_t elapsed=ba_timing_now()-v->at;uint64_t child=stack[--depth].child;assert(child<=elapsed);incl[v->stage]+=elapsed;excl[v->stage]+=elapsed-child;calls[v->stage]++;if(depth)stack[depth-1].child+=elapsed;}
void r11_begin(void){r12_begin();assert(!depth);if(!r16_diag_verbose()){start=0;return;}uint64_t now=ba_timing_now();period=previous?now-previous:0;previous=now;start=now;frame++;memset(excl,0,sizeof(excl));memset(incl,0,sizeof(incl));memset(calls,0,sizeof(calls));runs=static_static=static_dynamic=dynamic_static=uploads=bytes=creates=deletes=vertices=ix=capacity=0;hasrun=0;}
void r11_geometry(unsigned v,unsigned i,uint64_t c){vertices=v;ix=i;capacity=c;}
void r11_cache(uint64_t h,uint64_t m,uint64_t b,uint64_t cb,uint64_t db,uint64_t cpu,uint64_t p){hits=h;misses=m;builds=b;cache_bytes=cb;dynamic_bytes=db;cache_cpu=cpu;patterns=p;}
void r11_run(uint32_t tag,int first,int count){(void)first;(void)count;int a=(lasttag&0x80000000u)!=0,b=(tag&0x80000000u)!=0;if(hasrun){if(a&&b)static_static++;else if(a)static_dynamic++;else if(b)dynamic_static++;}runs++;lasttag=tag;hasrun=1;}
void r11_buffer_data(GLenum target,GLsizeiptr n,const void*p,GLenum usage){uploads++;bytes+=(uint64_t)n;glBufferData(target,n,p,usage);}
void r11_gen_buffers(GLsizei n,GLuint*p){creates+=n;glGenBuffers(n,p);}
void r11_delete_buffers(GLsizei n,const GLuint*p){deletes+=n;glDeleteBuffers(n,p);}
void r11_end(void){if(!start)return;assert(!depth);uint64_t elapsed=ba_timing_now()-start;start=0;if(!out){out=fopen("ux0:/data/RS2Vita/r11-frames.csv","w");if(!out)return;fputs("frame,period_us,render_wall_us,traversal_excl_us,projection_excl_us,order_excl_us,emit_excl_us,submission_excl_us,present_excl_us,draw_runs,static_static,static_dynamic,dynamic_static,upload_calls,upload_bytes,buffer_creates,buffer_deletes,vertices,indices,geometry_capacity,cache_hits,cache_misses,cache_builds,cache_upload_bytes,dynamic_upload_bytes,cache_cpu_capacity,pattern_bytes,model_calls\n",out);setvbuf(out,NULL,_IOFBF,65536);}
fprintf(out,"%llu,%llu,%llu",(unsigned long long)frame,(unsigned long long)period,(unsigned long long)elapsed);for(unsigned s=0;s<R11_STAGES;s++)fprintf(out,",%llu",(unsigned long long)excl[s]);uint64_t v[]={runs,static_static,static_dynamic,dynamic_static,uploads,bytes,creates,deletes,vertices,ix,capacity,hits,misses,builds,cache_bytes,dynamic_bytes,cache_cpu,patterns,calls[R11_PROJECT]};for(unsigned k=0;k<sizeof(v)/sizeof(v[0]);k++)fprintf(out,",%llu",(unsigned long long)v[k]);fputc('\n',out);if(frame%30==0)fflush(out);
}
#endif
