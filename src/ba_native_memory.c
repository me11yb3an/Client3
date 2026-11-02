/* Native memblock reservations, separate from live allocations inside vitaGL pools.
 * No synthesized overlay counters. Unknown/dropped tracking is reported. */
#include "ba_native_memory.h"
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <string.h>
static struct {int id;unsigned bytes,category;} blocks[1024];
static H7NativeMemory totals;static volatile int held;
static void lock(void){while(__sync_lock_test_and_set(&held,1))sceKernelDelayThread(1);}
static void unlock(void){__sync_lock_release(&held);}
static unsigned category(unsigned t){
 if(t==SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW)return 0;
 if(t==SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_RW||t==SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW)return 1;
 return 2;
}
void h7_native_alloc(int id,unsigned type,unsigned requested){
 if(id<0){lock();totals.failed++;unlock();return;}
 void*base=NULL;SceKernelMemBlockInfo info={0};info.size=sizeof(info);
 int rc=sceKernelGetMemBlockBase(id,&base);if(!rc)rc=sceKernelGetMemBlockInfoByAddr(base,&info);
 unsigned bytes=rc<0?requested:info.mappedSize,c=category(type);
 lock();if(rc<0)totals.info_failures++;
 unsigned i;for(i=0;i<1024;i++)if(!blocks[i].id)break;
 if(i==1024){totals.untracked++;unlock();return;}
 blocks[i].id=id;blocks[i].bytes=bytes;blocks[i].category=c;totals.live[c]+=bytes;totals.allocs[c]++;
 if(totals.live[c]>totals.peak[c])totals.peak[c]=totals.live[c];unlock();
}
int __real_sceKernelFreeMemBlock(SceUID);
int __wrap_sceKernelFreeMemBlock(SceUID id){
 lock();int rc=__real_sceKernelFreeMemBlock(id);if(rc<0){unlock();return rc;}
 unsigned i;for(i=0;i<1024;i++)if(blocks[i].id==id)break;
 if(i<1024){unsigned c=blocks[i].category;totals.live[c]-=blocks[i].bytes;totals.frees[c]++;memset(blocks+i,0,sizeof(blocks[i]));}
 unlock();return rc;
}
void h7_native_snapshot(H7NativeMemory*out){lock();*out=totals;unlock();}
