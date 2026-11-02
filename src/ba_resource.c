#define _GNU_SOURCE 1
 #include "h8_remote.h"
#include "ba_resource.h"
#include "ba_timing.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <pthread.h>
extern FILE *__real_fopen(const char *,const char *);
extern int __real_stat(const char *,struct stat *);
typedef struct {char *name;uint32_t offset,size,crc;} Entry;
typedef struct {FILE *file;Entry *entry;char *names;uint32_t count,length;int status;pthread_mutex_t lock;} Pack;
static Pack packs[2]={{.lock=PTHREAD_MUTEX_INITIALIZER},{.lock=PTHREAD_MUTEX_INITIALIZER}};
static pthread_once_t once=PTHREAD_ONCE_INIT;
static _Atomic uint64_t open_count,read_count,read_bytes;static uint64_t index_bytes;
static uint32_t le32(const unsigned char *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint16_t le16(const unsigned char *p){return (uint16_t)p[0]|(uint16_t)p[1]<<8;}
static const char *canonical(const char *path,char out[1024]){
 if(!path){out[0]=0;return out;}if(!strncmp(path,"app0:/",6))path+=6;
 size_t used=0;while(*path){while(*path=='/')path++;const char *at=path;while(*path&&*path!='/')path++;size_t n=(size_t)(path-at);if(!n||(n==1&&at[0]=='.'))continue;
  if(n+used+1>=1024){out[0]=0;return out;}if(used)out[used++]='/';memcpy(out+used,at,n);used+=n;
 }out[used]=0;return out;
}
static int group(const char *p){if(!strncmp(p,"rom/cache/",10)||!strcmp(p,"rom/cache"))return 0;if(!strncmp(p,"serverdata/",11)||!strcmp(p,"serverdata"))return 1;return -1;}
static void load_pack(Pack *p,const char *name){
 char path[128];snprintf(path,sizeof(path),"app0:/resources/%s.pak",name);p->file=__real_fopen(path,"rb");
#ifndef __vita__
 if(!p->file){snprintf(path,sizeof(path),"resources/%s.pak",name);p->file=__real_fopen(path,"rb");}
#endif
 if(!p->file)return;
 p->status=-1;unsigned char h[24];unsigned char *index=NULL;
 if(fread(h,1,24,p->file)!=24||memcmp(h,"BARES001",8))goto fail;
 uint32_t count=le32(h+8),size=le32(h+12),start=le32(h+16),total=le32(h+20);
 if(count>8192||!count||size>2*1024*1024||start!=24+size||total<start||total>128*1024*1024)goto fail;
 if(fseek(p->file,0,SEEK_END)||ftell(p->file)!=(long)total||fseek(p->file,24,SEEK_SET))goto fail;
 index=malloc(size+count);p->entry=calloc(count,sizeof(Entry));if(!index||!p->entry)goto fail;
 if(fread(index,1,size,p->file)!=size)goto fail;
 /* Names copied out before validation; no packed byte is interpreted as a pointer. */
 p->names=malloc(size+count);if(!p->names)goto fail;uint32_t at=0,used=0;
 for(uint32_t i=0;i<count;i++){
  if(size-at<16)goto fail;uint16_t n=le16(index+at);uint16_t flags=le16(index+at+2);
  Entry *e=p->entry+i;e->offset=le32(index+at+4);e->size=le32(index+at+8);e->crc=le32(index+at+12);at+=16;
  if(!n||n>899||flags||n>size-at||e->offset<start||e->offset>total||e->size>total-e->offset)goto fail;
  e->name=p->names+used;memcpy(e->name,index+at,n);e->name[n]=0;used+=n+1;at+=n;
  if(strlen(e->name)!=n||e->name[0]=='/'||strchr(e->name,'\\')||strstr(e->name,"..")||group(e->name)!=(p-packs)||(i&&strcmp(p->entry[i-1].name,e->name)>=0))goto fail;
 }
 if(at!=size)goto fail;free(index);p->count=count;p->length=total;p->status=1;index_bytes+=sizeof(Entry)*count+size+count;return;
fail:
 free(index);free(p->entry);free(p->names);p->entry=NULL;p->names=NULL;if(p->file)fclose(p->file);p->file=NULL;
}
static void initialize(void){load_pack(packs,"client");load_pack(packs+1,"server");}
int ba_resource_init(void){pthread_once(&once,initialize);return (packs[0].status==1)+(packs[1].status==1);}
static Pack *get_pack(const char *path){int g=group(path);if(g<0)return NULL;ba_resource_init();return packs+g;}
static uint32_t lower_bound(Pack *p,const char *key){uint32_t a=0,b=p->count;while(a<b){uint32_t m=a+(b-a)/2;if(strcmp(p->entry[m].name,key)<0)a=m+1;else b=m;}return a;}
/* The native archive index already records exact CRC/length. Exposing this
 * immutable metadata avoids loading every map/song into the JS heap just to
 * calculate checksums at startup. Never intercept mutable player data. */
static _Atomic uint64_t metadata_calls;
int ba_resource_metadata(const char*path,uint32_t*crc,uint32_t*size){
 if(!path||!crc||!size)return -1;
 char normalized[1024];const char*key=canonical(path,normalized);
 Pack*p=get_pack(key);if(!p||p->status!=1)return -1;
 uint32_t i=lower_bound(p,key);if(i==p->count||strcmp(p->entry[i].name,key))return 0;
 *crc=p->entry[i].crc;*size=p->entry[i].size;metadata_calls++;return 1;
}
uint64_t ba_resource_metadata_calls(void){return metadata_calls;}
typedef struct {Pack *pack;Entry *entry;uint32_t pos;} Stream;
static ssize_t stream_read(void *ctx,char *buf,size_t n){
 Stream *s=ctx;if(n>s->entry->size-s->pos)n=s->entry->size-s->pos;if(!n)return 0;
 uint64_t began=ba_timing_now();pthread_mutex_lock(&s->pack->lock);
 int bad=fseek(s->pack->file,s->entry->offset+s->pos,SEEK_SET);size_t got=bad?0:fread(buf,1,n,s->pack->file);int err=bad||ferror(s->pack->file);
 pthread_mutex_unlock(&s->pack->lock);s->pos+=(uint32_t)got;read_count++;read_bytes+=got;ba_timing_record(BA_RESOURCE_READ,ba_timing_now()-began);
 if(err&&!got){errno=EIO;return -1;}return (ssize_t)got;
}
#ifdef __vita__
typedef off_t CookieOffset;
#else
typedef off64_t CookieOffset;
#endif
static int stream_seek(void *ctx,CookieOffset *where,int whence){Stream*s=ctx;int64_t base=whence==SEEK_SET?0:whence==SEEK_CUR?s->pos:whence==SEEK_END?s->entry->size:-1;if(base<0||*where>INT64_MAX-base||base+*where<0||base+*where>s->entry->size){errno=EINVAL;return -1;}s->pos=(uint32_t)(base+*where);*where=s->pos;return 0;}
static int stream_close(void *ctx){free(ctx);return 0;}
/* Disable legacy synchronous diagnostic files, never saves or preferences. */
static int legacy_diagnostic(const char *path){
 static const char *names[]={"input-events.csv","actor-animation-loops.csv","menu-actions.csv","minimap-input.log","npc-presence.csv","ui-content-frames.csv","ui-content-audit.txt","dialogue-layout.csv","minimap-state.csv","minimap-frame-only.txt","water-reference.txt","pixel-vector.csv","pixel-vector-selftest.log","draw-distance.csv","interface-packets.log","frame-intervals.csv","gpu-memory.csv"};
 const char *root="ux0:/data/RS2Vita/";if(strncmp(path,root,strlen(root)))return !strncmp(path,"/research/",10);
 const char*n=path+strlen(root);for(size_t i=0;i<sizeof(names)/sizeof(*names);i++)if(!strcmp(n,names[i]))return 1;return 0;
}
FILE *__wrap_fopen(const char *path,const char *mode){
 /* Classify the original logical path before diagnostic redirection. */
 if(legacy_diagnostic(path)){errno=ENOENT;return NULL;}
 char h8_path[256];path=h8_boot_path(path,h8_path,sizeof(h8_path));if(!path){errno=ENAMETOOLONG;return NULL;}
 char normalized[1024];const char *key=canonical(path,normalized);Pack*p=get_pack(key);if(!p||!p->status)return __real_fopen(path,mode);
 if(p->status<0){errno=EIO;return NULL;}if(mode[0]!='r'||strchr(mode,'+')){errno=EROFS;return NULL;}
 uint64_t began=ba_timing_now();uint32_t i=lower_bound(p,key);ba_timing_record(BA_RESOURCE_LOOKUP,ba_timing_now()-began);
 if(i==p->count||strcmp(p->entry[i].name,key)){errno=ENOENT;return NULL;}
 Stream*s=calloc(1,sizeof(*s));if(!s)return NULL;s->pack=p;s->entry=p->entry+i;
 cookie_io_functions_t io={.read=stream_read,.seek=stream_seek,.close=stream_close};FILE*f=fopencookie(s,"rb",io);if(!f){free(s);return NULL;}open_count++;return f;
}
int __wrap_stat(const char *path,struct stat *st){
 char normalized[1024];const char*key=canonical(path,normalized);Pack*p=get_pack(key);if(!p||!p->status)return __real_stat(path,st);if(p->status<0){errno=EIO;return -1;}
 uint32_t i=lower_bound(p,key);int file=i<p->count&&!strcmp(p->entry[i].name,key);char prefix[1024];snprintf(prefix,sizeof(prefix),"%s/",key);
 if(!file){i=lower_bound(p,prefix);if(i==p->count||strncmp(p->entry[i].name,prefix,strlen(prefix))){errno=ENOENT;return -1;}}
 memset(st,0,sizeof(*st));st->st_mode=(file?S_IFREG:S_IFDIR)|(file?0444:0555);st->st_nlink=1;if(file)st->st_size=p->entry[i].size;return 0;
}
int ba_resource_list(const char *path,void(*emit)(void*,const char*),void *ctx){
 char normalized[1024];const char*key=canonical(path,normalized);Pack*p=get_pack(key);if(!p||!p->status)return 0;if(p->status<0){errno=EIO;return -1;}
 char prefix[1024],last[1024]="";snprintf(prefix,sizeof(prefix),"%s/",key);size_t n=strlen(prefix);uint32_t i=lower_bound(p,prefix),found=0;
 for(;i<p->count&&!strncmp(p->entry[i].name,prefix,n);i++){
  const char *s=p->entry[i].name+n;size_t k=strcspn(s,"/");if(k>=sizeof(last))return -1;char child[1024];memcpy(child,s,k);child[k]=0;
  if(strcmp(child,last)){emit(ctx,child);strcpy(last,child);found++;}
 }
 if(!found){errno=ENOENT;return -1;}return 1;
}
void ba_resource_summary(uint64_t *o,uint64_t *r,uint64_t *b,uint64_t *i){*o=open_count;*r=read_count;*b=read_bytes;*i=index_bytes;}
