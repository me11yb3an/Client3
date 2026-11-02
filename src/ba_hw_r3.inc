#include <string.h>
#include <psp2/kernel/threadmgr.h>
/* Separate R3 logs: wall timing on every frame, thread CPU sampled every 30th frame.
 * Exclusive time subtracts directly nested scopes. Main thread only; no scheduling changes. */
#include "ba_hw_r3.h"
static SceUID h3_main;
static H3Scope *h3_parent;
static int h3_cpu_sample,h3_active;
static unsigned h3_frames;
static char h3_test='G';
static struct {uint64_t calls,wall,exclusive,cpu_calls,cpu,exclusive_cpu,max;} h3_metrics[H3_COUNT];
static const char*h3_names[]={"frame","input","logic_includes_server","outer_server","render","present","diagnostics","sleep","entity_prepare","world_cpu","world_overlays","chat","minimap","stone_ui","final_overlay","pixel_convert","pixel_upload","world_submit","swap","allocation_sampled","buffer_update","shader_compile"};
/* Physical R3 short self-query deltas were stale/zero despite97.7% interval CPU.
 * Use interval thread snapshots; do not label these short spans as CPU time. */
static uint64_t h3_cpu(void){return UINT64_MAX;}

H3Scope h3_enter(unsigned stage){
 H3Scope s={0};s.stage=stage;
 if(!h3_active||sceKernelGetThreadId()!=h3_main)return s;
 s.active=1;s.wall=ba_timing_now();s.cpu=h3_cpu_sample?h3_cpu():UINT64_MAX;s.cpu_ok=s.cpu!=UINT64_MAX;
 s.parent=h3_parent;
 /* Parent linkage is set by caller below: return-by-value cannot retain &s. */
 return s;
}
/* Explicit link is required after the returned scope has its stable caller address. */
void h3_link(H3Scope*s){if(s->active)h3_parent=s;}
void h3_leave(H3Scope*s){
 if(!s->active)return;
 uint64_t wall=ba_timing_now()-s->wall,cpu=h3_cpu_sample?h3_cpu():UINT64_MAX;
 unsigned valid=s->cpu_ok&&cpu!=UINT64_MAX&&cpu>=s->cpu;
 if(valid)cpu-=s->cpu;
 unsigned i=s->stage;h3_metrics[i].calls++;h3_metrics[i].wall+=wall;
 h3_metrics[i].exclusive+=wall>=s->child_wall?wall-s->child_wall:0;
 if(wall>h3_metrics[i].max)h3_metrics[i].max=wall;
 if(valid){h3_metrics[i].cpu_calls++;h3_metrics[i].cpu+=cpu;h3_metrics[i].exclusive_cpu+=cpu>=s->child_cpu?cpu-s->child_cpu:0;}
 if(s->parent){s->parent->child_wall+=wall;if(valid)s->parent->child_cpu+=cpu;}
 h8_scope(i,wall,wall>=s->child_wall?wall-s->child_wall:0);
 h5_scope(i,wall,wall>=s->child_wall?wall-s->child_wall:0);
 h4_scope(i,wall,wall>=s->child_wall?wall-s->child_wall:0,valid,valid?cpu:0,valid&&cpu>=s->child_cpu?cpu-s->child_cpu:0);
 h3_parent=s->parent;s->active=0;
}
static uint64_t h3_check;
void h3_frame_begin(void){
 h3_cpu_sample=(++h3_frames%30u)==0;h3_active=1;
 uint64_t now=ba_timing_now();if(now-h3_check>=1000000u){
  h3_test='G';h3_check=now; /* R4 matched pair ignores inherited R3 isolation settings. */
 }
}
int h3_mode(void){if(h5_mode()=='D')return 'C';if(h5_mode()=='W')return 'E';return h3_test;}
void h3_init(void){h3_main=sceKernelGetThreadId();h3_active=1;}
static uint64_t h3_ops[17];
/* Counters: binds, programs, framebuffer binds, array draws, indices, buffer calls,
 * buffer bytes, copy calls, shader calls, malloc, calloc, realloc, failures,
 * allocated requested bytes, vertex-patcher misses, fragment-patcher misses. */
void h3_summary(uint64_t now,unsigned serial){
 int active=h3_active;h3_active=0;
 FILE*f=fopen("ux0:/data/RS2Vita/hw-r3-frame.csv",serial?"a":"w");
 if(f){if(!serial)fprintf(f,"time_us,mode,stage,calls,inclusive_wall_us,exclusive_wall_us,cpu_sample_calls,inclusive_cpu_us,exclusive_cpu_us,max_wall_us\n");
  for(unsigned i=0;i<H3_COUNT;i++){fprintf(f,"%llu,%c,%s,%llu,%llu,%llu,%llu,%llu,%llu,%llu\n",(unsigned long long)now,h3_test,h3_names[i],(unsigned long long)h3_metrics[i].calls,(unsigned long long)h3_metrics[i].wall,(unsigned long long)h3_metrics[i].exclusive,(unsigned long long)h3_metrics[i].cpu_calls,(unsigned long long)h3_metrics[i].cpu,(unsigned long long)h3_metrics[i].exclusive_cpu,(unsigned long long)h3_metrics[i].max);memset(&h3_metrics[i],0,sizeof(h3_metrics[i]));}fclose(f);}
 f=fopen("ux0:/data/RS2Vita/hw-r3-operations.csv",serial?"a":"w");
 if(f){if(!serial)fprintf(f,"time_us,mode,texture_binds,program_changes,framebuffer_binds,array_draws,index_count,buffer_calls,buffer_bytes,framebuffer_copies,shader_compiles,malloc_calls,calloc_calls,realloc_calls,allocation_failures,requested_bytes,vertex_patcher_calls,fragment_patcher_calls,runtime_shader_compiles\n");
  fprintf(f,"%llu,%c",(unsigned long long)now,h3_test);for(unsigned i=0;i<17;i++){fprintf(f,",%llu",(unsigned long long)h3_ops[i]);h3_ops[i]=0;}fputc('\n',f);fclose(f);}
 h3_active=active;
}
#ifdef GL11
void __real_glBindTexture(GLenum,GLuint);
void __wrap_glBindTexture(GLenum t,GLuint n){h3_ops[0]++;__real_glBindTexture(t,n);}
void __real_glUseProgram(GLuint);
void __wrap_glUseProgram(GLuint n){h3_ops[1]++;__real_glUseProgram(n);}
void __real_glBindFramebuffer(GLenum,GLuint);
void __wrap_glBindFramebuffer(GLenum t,GLuint n){h3_ops[2]++;__real_glBindFramebuffer(t,n);}
void __real_glDrawArrays(GLenum,GLint,GLsizei);
void __wrap_glDrawArrays(GLenum t,GLint f,GLsizei n){h3_ops[3]++;if(!h5_graph(0))__real_glDrawArrays(t,f,n);}
void __real_glBufferData(GLenum,GLsizeiptr,const void*,GLenum);
void __wrap_glBufferData(GLenum t,GLsizeiptr n,const void*p,GLenum u){H3_SCOPE(H3_BUFFER);h3_ops[5]++;if(n>0){h3_ops[6]+=n;h4_graph(1,n);}if(!h5_graph(0))__real_glBufferData(t,n,p,u);}
void __real_glCompileShader(GLuint);
void __wrap_glCompileShader(GLuint n){H3_SCOPE(H3_SHADER);h3_ops[8]++;__real_glCompileShader(n);}
void __real_vglSwapBuffers(GLboolean);
void __wrap_vglSwapBuffers(GLboolean n){H3_SCOPE(H3_SWAP);if(!h5_graph(0))__real_vglSwapBuffers(n);}
void __real_glCopyTexSubImage2D(GLenum,GLint,GLint,GLint,GLint,GLint,GLsizei,GLsizei);
void __wrap_glCopyTexSubImage2D(GLenum t,GLint l,GLint xo,GLint yo,GLint x,GLint y,GLsizei w,GLsizei h){h3_ops[7]++;if(!h5_graph(0))__real_glCopyTexSubImage2D(t,l,xo,yo,x,y,w,h);}
int __real_sceGxmShaderPatcherCreateVertexProgram(SceGxmShaderPatcher*,SceGxmShaderPatcherId,const SceGxmVertexAttribute*,unsigned,const SceGxmVertexStream*,unsigned,SceGxmVertexProgram**);
int __wrap_sceGxmShaderPatcherCreateVertexProgram(SceGxmShaderPatcher*s,SceGxmShaderPatcherId p,const SceGxmVertexAttribute*a,unsigned n,const SceGxmVertexStream*v,unsigned m,SceGxmVertexProgram**o){H3_SCOPE(H3_SHADER);h3_ops[14]++;return __real_sceGxmShaderPatcherCreateVertexProgram(s,p,a,n,v,m,o);}
int __real_sceGxmShaderPatcherCreateFragmentProgram(SceGxmShaderPatcher*,SceGxmShaderPatcherId,SceGxmOutputRegisterFormat,SceGxmMultisampleMode,const SceGxmBlendInfo*,const SceGxmProgram*,SceGxmFragmentProgram**);
int __wrap_sceGxmShaderPatcherCreateFragmentProgram(SceGxmShaderPatcher*s,SceGxmShaderPatcherId p,SceGxmOutputRegisterFormat f,SceGxmMultisampleMode m,const SceGxmBlendInfo*b,const SceGxmProgram*v,SceGxmFragmentProgram**o){H3_SCOPE(H3_SHADER);h3_ops[15]++;h6_fragment_msaa(m);return __real_sceGxmShaderPatcherCreateFragmentProgram(s,p,f,m,b,v,o);}
#endif

/* Main-thread allocation counters. Timed only on CPU-sampled frames to bound overhead. */
void *__real_malloc(size_t);void *__real_calloc(size_t,size_t);void *__real_realloc(void*,size_t);
static int h3_alloc_main(void){return h3_main&&sceKernelGetThreadId()==h3_main;}
static void h3_alloc_record(unsigned i,size_t bytes,void*p,uint64_t t){
 h3_ops[i]++;h3_ops[13]+=bytes;if(!p&&bytes)h3_ops[12]++;
 if(t){uint64_t us=ba_timing_now()-t;h3_metrics[H3_ALLOC].calls++;h3_metrics[H3_ALLOC].wall+=us;h3_metrics[H3_ALLOC].exclusive+=us;if(us>h3_metrics[H3_ALLOC].max)h3_metrics[H3_ALLOC].max=us;}
}
void *__wrap_malloc(size_t n){int m=h3_alloc_main();uint64_t t=m&&h3_active&&h3_cpu_sample?ba_timing_now():0;void*p=__real_malloc(n);h5_allocation(0,n,!p&&n);if(m)h3_alloc_record(9,n,p,t);return p;}
void *__wrap_calloc(size_t n,size_t z){int m=h3_alloc_main();uint64_t t=m&&h3_active&&h3_cpu_sample?ba_timing_now():0;void*p=__real_calloc(n,z);h5_allocation(1,n&&z>SIZE_MAX/n?SIZE_MAX:n*z,!p&&n&&z);if(m)h3_alloc_record(10,n&&z>SIZE_MAX/n?SIZE_MAX:n*z,p,t);return p;}
void *__wrap_realloc(void*q,size_t n){int m=h3_alloc_main();uint64_t t=m&&h3_active&&h3_cpu_sample?ba_timing_now():0;void*p=__real_realloc(q,n);h5_allocation(2,n,!p&&n);if(m)h3_alloc_record(11,n,p,t);return p;}

#ifdef GL11
#include <vitashark.h>
SceGxmProgram *__real_shark_compile_shader_extended(const char*,uint32_t*,shark_type,shark_opt,int32_t,int32_t,int32_t);
SceGxmProgram *__wrap_shark_compile_shader_extended(const char*s,uint32_t*n,shark_type t,shark_opt o,int32_t a,int32_t b,int32_t c){H3_SCOPE(H3_SHADER);h3_ops[16]++;return __real_shark_compile_shader_extended(s,n,t,o,a,b,c);}
#endif

static void h3_config_snapshot(void){
 FILE*out=fopen("ux0:/data/RS2Vita/hw-r6-build.txt","w");
 if(out){fprintf(out,"Build=HW-R5-20261022 diagnostic; source=" HW_R5_SOURCE_ID "; package manifest and module fingerprint identify executable.\n");
 const char*names[]={"mode.txt","pool-experiment.txt","draw-radius.txt","render_fps.txt","hw-r2-ring-mib.txt","hw-r2-vbo.txt"};
 const char*defaults[]={"interactive C; no automatic benchmark","0 baseline","50","60","64","1"};
 for(unsigned i=0;i<6;i++){char path[160],buf[128]={0};snprintf(path,sizeof(path),"ux0:/data/RS2Vita/%s",names[i]);FILE*f=fopen(path,"r");if(f){fread(buf,1,sizeof(buf)-1,f);fclose(f);}else snprintf(buf,sizeof(buf),"<absent: %s>",defaults[i]);fprintf(out,"%s=%s\n",names[i],buf);}fclose(out);}

 FILE*in=fopen("app0:/sce_sys/param.sfo","rb");out=fopen("ux0:/data/RS2Vita/hw-r5-installed-param.sfo","wb");
 if(in&&out){char buf[512];size_t n;while((n=fread(buf,1,sizeof(buf),in)))fwrite(buf,1,n,out);}if(in)fclose(in);if(out)fclose(out);
}
