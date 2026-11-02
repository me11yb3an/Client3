#ifndef R103_SAVE_NATIVE_READ_H
#define R103_SAVE_NATIVE_READ_H
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
/* Save paths have already passed the host's fixed-root path validation.
 * Keep this read-only backend off newlib's unchecked _lseek_r descriptor
 * lookup. Native SceUID operations report failures as checked error codes. */
static int r103_save_native_read(const char*path,uint8_t**out,size_t*length){
    if(!path||!out||!length)return -1;
    *out=NULL;*length=0;
    SceUID fd=sceIoOpen(path,SCE_O_RDONLY,0);if(fd<0)return -1;
    SceOff size=sceIoLseek(fd,0,SCE_SEEK_END);
    if(size<0||size>64*1024*1024||sceIoLseek(fd,0,SCE_SEEK_SET)!=0){sceIoClose(fd);return -1;}
    size_t n=(size_t)size,used=0;uint8_t*buffer=malloc(n?n:1);
    if(!buffer){sceIoClose(fd);return -1;}
    while(used<n){
        SceSSize got=sceIoRead(fd,buffer+used,(SceSize)(n-used));
        if(got<=0||(size_t)got>n-used){free(buffer);sceIoClose(fd);return -1;}
        used+=(size_t)got;
    }
    if(sceIoClose(fd)<0){free(buffer);return -1;}
    *out=buffer;*length=n;return 0;
}
#endif
