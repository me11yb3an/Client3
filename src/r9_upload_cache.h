/* Independent HW-R9 experiment. Exact byte comparison, no borrowed Bren code.
 * Single render-thread owner. A context/VBO change must invalidate the shadow.
 * This retains GPU data only; it does not cache CPU geometry construction. */
#ifndef R9_UPLOAD_CACHE_H
#define R9_UPLOAD_CACHE_H
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifndef R9_REALLOC
#define R9_REALLOC realloc
#endif
#ifndef R9_FREE
#define R9_FREE free
#endif
typedef struct {
 unsigned char *shadow;
 size_t length,capacity;
 uint64_t resource_epoch;
 unsigned resource;
 int valid;
 uint64_t checks,hits,misses,compared_bytes,uploaded_bytes,saved_bytes,failures;
} R9UploadCache;
static inline void r9_upload_invalidate(R9UploadCache *c) { c->valid=0; }
static inline void r9_upload_destroy(R9UploadCache *c) {
 R9_FREE(c->shadow); memset(c,0,sizeof(*c));
}
static inline int r9_upload_needed(R9UploadCache *c,unsigned resource,
 uint64_t epoch,const void *data,size_t length) {
 c->checks++;
 if(c->valid && c->resource==resource && c->resource_epoch==epoch &&
    c->length==length && (!length || data)) {
  c->compared_bytes+=length;
  if(!length || memcmp(c->shadow,data,length)==0) {
   c->hits++;c->saved_bytes+=length;return 0;
  }
 }
 c->misses++;
 /* A subsequent driver allocation may change the store even on failure. */
 c->valid=0;return 1;
}
static inline void r9_upload_commit(R9UploadCache *c,unsigned resource,
 uint64_t epoch,const void *data,size_t length,int success,size_t budget) {
 c->valid=0;
 if(!success) {c->failures++;return;}
 c->uploaded_bytes+=length;
 if(length>budget || (length && !data)) {c->failures++;return;}
 if(length>c->capacity) {
  void *p=R9_REALLOC(c->shadow,length);
  if(!p) {c->failures++;return;}
  c->shadow=p;c->capacity=length;
 }
 if(length)memcpy(c->shadow,data,length);
 c->length=length;c->resource=resource;c->resource_epoch=epoch;c->valid=1;
}
#endif
