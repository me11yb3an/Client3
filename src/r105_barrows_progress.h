#pragma once
/* Existing authoritative dhenc_dead bits; never derive progress from scenery
 * or include ordinary tunnel creatures in the six-brother total. */
#define R105_BARROWS_DEAD_VARP 306
static int r105_barrows_count(int x,int z,int level,unsigned mask){
 if((level!=0&&level!=3)||x<3520||x>=3584||z<9664||z>=9728)return -1;
 unsigned bits=mask&63u;int n=0;for(;bits;bits>>=1)n+=(int)(bits&1u);return n;
}
