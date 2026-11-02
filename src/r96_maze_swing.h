#ifndef R96_MAZE_SWING_H
#define R96_MAZE_SWING_H
#include <stdint.h>
#include <string.h>
/* Transient render poses only: original mesh, map identity, picking and
 * server collision remain authoritative. Settled poses use the exact cache.
 * Open visual aliases encode their original parent and quarter-turn. */
typedef struct { int valid,key,open,rotation;uint64_t began; } R96Door;
static R96Door r96_doors[256];
static int r96_epoch;
static unsigned r96_transitions;
static unsigned r96_animation_models;
static int r96_swing(int bitset,int floor,int epoch,uint64_t now,
                     int*yaw,int*x,int*z,const int*sin_table,const int*cos_table) {
    unsigned tag=(unsigned)bitset,id=(tag>>14)&32767u;
    if((tag&0x60000000u)!=0x40000000u)return 0;
    int base,open,rotation=-1;
    if(id>=3851&&id<=3882){base=(int)id-3851;open=0;}
    else if(id>=3907&&id<4035){base=((int)id-3907)/4;rotation=((int)id-3907)&3;open=1;}
    else return 0;
    if(r96_epoch!=epoch){memset(r96_doors,0,sizeof r96_doors);r96_epoch=epoch;}
    unsigned key=(tag&16383u)|((unsigned)base<<14)|((unsigned)(floor&3)<<19);
    unsigned hash=(key*2654435761u)>>24;
    R96Door*s=NULL;
    for(unsigned probe=0;probe<256;probe++){
        R96Door*q=r96_doors+((hash+probe)&255);
        if(!q->valid){q->valid=1;q->key=(int)key;q->open=open;q->rotation=rotation;q->began=0;return 0;}
        if(q->key==(int)key){s=q;break;}
    }
    if(!s)return 0;
    if(s->open!=open){s->open=open;if(rotation>=0)s->rotation=rotation;s->began=now;r96_transitions++;}
    if(!s->began||now<s->began||now-s->began>=400000u||s->rotation<0)return 0;
    int direction=base<16?-1:1;
    uint64_t remaining=400000u-(now-s->began);
    int angle=(int)(remaining*512u/400000u)*direction*(open?-1:1);
    int hx=-64,hz=-64*direction;
    for(int r=0;r<s->rotation;r++){int old=hx;hx=hz;hz=-old;}
    int sine=sin_table[angle&2047],cosine=cos_table[angle&2047];
    int rx=(int)(((int64_t)hx*cosine+(int64_t)hz*sine)>>16);
    int rz=(int)(((int64_t)hz*cosine-(int64_t)hx*sine)>>16);
    *yaw=(*yaw+angle)&2047;*x+=hx-rx;*z+=hz-rz;
    r96_animation_models++;
    return 1;
}
#endif
