/* CPU ownership and index-only roof visibility for retained buffers. The
 * caller supplies R32_HIDDEN(tag) and R32_HAS_SELECTION. Unknown/dynamic
 * buffers pass through unchanged. No render geometry is regenerated. */
#define R32_RECORDS 192
#define R32_GROUPS 12
#define R32_MAX_INDICES 65532
#ifndef R32_ALLOC
#define R32_ALLOC malloc
#endif
#ifndef R32_FREE
#define R32_FREE free
#endif
typedef struct {
 GLuint ibo;uint16_t*indices,*tags;int count,groups;
 int original[R32_GROUPS],active_count[R32_GROUPS],active_first[R32_GROUPS];
 unsigned revision;int had_hidden;
} R32IndexMask;
static R32IndexMask r32_masks[R32_RECORDS];
static int r32_used,r32_failed;
static unsigned r32_revision=1,r32_uploads;
static GLuint r32_bound_element;
static uint16_t r32_filter_indices[R32_MAX_INDICES];
static R32IndexMask*r32_find(GLuint id){
 if(!id)return NULL;for(int i=0;i<r32_used;i++)if(r32_masks[i].ibo==id)return &r32_masks[i];return NULL;
}
static void r32_forget(GLuint id){
 R32IndexMask*m=r32_find(id);if(!m)return;
 R32_FREE(m->indices);R32_FREE(m->tags);memset(m,0,sizeof(*m));
 while(r32_used&&!r32_masks[r32_used-1].ibo)r32_used--;
}
static int r32_register_indices(GLuint id,const uint16_t*indices,const uint16_t*tags,int count,int groups,const int*counts){
 if(!id||!indices||!tags||!counts||count<0||count>R32_MAX_INDICES||count%3||groups<1||groups>R32_GROUPS)return 0;
 int total=0;for(int i=0;i<groups;i++){if(counts[i]<0||counts[i]%3||counts[i]>count-total)return 0;total+=counts[i];}if(total!=count)return 0;
 r32_forget(id);if(!count)return 1;
 int slot=0;while(slot<r32_used&&r32_masks[slot].ibo)slot++;
 if(slot==R32_RECORDS)return 0;
 uint16_t*copy=R32_ALLOC((size_t)count*sizeof(*copy)),*tagcopy=R32_ALLOC((size_t)(count/3)*sizeof(*tagcopy));
 if(!copy||!tagcopy){R32_FREE(copy);R32_FREE(tagcopy);return 0;}
 memcpy(copy,indices,(size_t)count*sizeof(*copy));memcpy(tagcopy,tags,(size_t)(count/3)*sizeof(*tagcopy));
 R32IndexMask*m=r32_masks+slot;memset(m,0,sizeof(*m));m->ibo=id;m->indices=copy;m->tags=tagcopy;m->count=count;m->groups=groups;memcpy(m->original,counts,(size_t)groups*sizeof(*counts));
 if(slot==r32_used)r32_used++;return 1;
}
static void r32_change_visibility(void){if(!++r32_revision){r32_revision=1;for(int i=0;i<r32_used;i++)r32_masks[i].revision=0;}}
static int r32_apply_mask(R32IndexMask*m){
 if(r32_failed)return 0;if(m->revision==r32_revision)return 1;
 int any_hidden=0;
 if(R32_HAS_SELECTION)for(int i=0;i<m->count/3;i++)if(R32_HIDDEN(m->tags[i])){any_hidden=1;break;}
 if(!any_hidden){
  int first=0;for(int g=0;g<m->groups;g++){m->active_first[g]=first;m->active_count[g]=m->original[g];first+=m->original[g];}
  if(m->had_hidden){
   for(int i=0;i<8&&glGetError()!=GL_NO_ERROR;i++){}
   glBufferData(GL_ELEMENT_ARRAY_BUFFER,(GLsizei)((size_t)m->count*sizeof(uint16_t)),m->indices,GL_DYNAMIC_DRAW);
   if(glGetError()!=GL_NO_ERROR){r32_failed=1;return 0;}r32_uploads++;
  }
  m->had_hidden=0;m->revision=r32_revision;return 1;
 }
 int read=0,write=0,hidden=0;
 for(int group=0;group<m->groups;group++){
  m->active_first[group]=write;int end=read+m->original[group];
  int first=write;
  for(;read<end;read+=3){
   if(R32_HIDDEN(m->tags[read/3])){hidden=1;continue;}
   r32_filter_indices[write++]=m->indices[read];r32_filter_indices[write++]=m->indices[read+1];r32_filter_indices[write++]=m->indices[read+2];
  }
  m->active_count[group]=write-first;
 }
 if((hidden&&write)||(!hidden&&m->had_hidden)){
  for(int i=0;i<8&&glGetError()!=GL_NO_ERROR;i++){}
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,(GLsizei)((size_t)(hidden?write:m->count)*sizeof(uint16_t)),hidden?r32_filter_indices:m->indices,GL_DYNAMIC_DRAW);
  if(glGetError()!=GL_NO_ERROR){r32_failed=1;return 0;}r32_uploads++;
 }
 m->had_hidden=hidden;m->revision=r32_revision;return 1;
}
static void r32_bind_buffer(GLenum target,GLuint id){if(target==GL_ELEMENT_ARRAY_BUFFER)r32_bound_element=id;glBindBuffer(target,id);}
static void r32_delete_buffers(GLsizei n,const GLuint*ids){
 for(GLsizei i=0;i<n;i++){r32_forget(ids[i]);if(r32_bound_element==ids[i])r32_bound_element=0;}glDeleteBuffers(n,ids);
}
static void r32_draw_elements(GLenum mode,GLsizei count,GLenum type,const void*offset){
 R32IndexMask*m=r32_find(r32_bound_element);
 if(!m){glDrawElements(mode,count,type,offset);return;}
 if(mode!=GL_TRIANGLES||type!=GL_UNSIGNED_SHORT||((uintptr_t)offset&1)){r32_failed=1;return;}
 int first=0;for(int group=0;group<m->groups;group++){
  if((uintptr_t)offset==(uintptr_t)first*sizeof(uint16_t)&&count==m->original[group]){
   if(!r32_apply_mask(m))return;
   if(m->active_count[group])glDrawElements(mode,m->active_count[group],type,(const void*)(size_t)(m->active_first[group]*sizeof(uint16_t)));
   return;
  }
  first+=m->original[group];
 }
 r32_failed=1;
}
#define glBindBuffer r32_bind_buffer
#define glDeleteBuffers r32_delete_buffers
#define glDrawElements r32_draw_elements
