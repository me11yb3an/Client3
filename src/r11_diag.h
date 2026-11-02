#pragma once
#include <stdint.h>
#include "gl11.h"
#ifndef BA_R11_DIAG
#define BA_R11_DIAG 0
#endif
enum {R11_TRAVERSE,R11_PROJECT,R11_ORDER,R11_EMIT,R11_SUBMIT,R11_PRESENT,R11_STAGES};
typedef struct {unsigned stage,depth;uint64_t at,child;} R11Scope;
#if BA_R11_DIAG
R11Scope r11_enter(unsigned);
void r11_leave(R11Scope*);
void r11_begin(void);void r11_end(void);
void r11_run(uint32_t,int,int);
void r11_geometry(unsigned,unsigned,uint64_t);
void r11_cache(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t);
void r11_buffer_data(GLenum,GLsizeiptr,const void*,GLenum);
void r11_gen_buffers(GLsizei,GLuint*);void r11_delete_buffers(GLsizei,const GLuint*);
#define R11_SCOPE(s) R11Scope r11_scope __attribute__((cleanup(r11_leave)))=r11_enter(s)
#else
#define R11_SCOPE(s) ((void)0)
#define r11_begin() ((void)0)
#define r11_end() ((void)0)
#define r11_run(a,b,c) ((void)0)
#define r11_geometry(a,b,c) ((void)0)
#define r11_cache(a,b,c,d,e,f,g) ((void)0)
#define r11_buffer_data glBufferData
#define r11_gen_buffers glGenBuffers
#define r11_delete_buffers glDeleteBuffers
#endif
