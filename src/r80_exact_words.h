#pragma once
#include <stddef.h>
#include <stdint.h>
#if defined(__ARM_NEON)
#include <arm_neon.h>
#endif
/* Exact bounded comparison. Vector loads never extend past either array. */
static int r80_words_raw(const int*a,const int*b,size_t n){
 size_t i=0;uint32_t tail=0;
#if defined(__ARM_NEON)
 uint32x4_t diff=vdupq_n_u32(0);
 for(;i+16<=n;i+=16){diff=vorrq_u32(diff,veorq_u32(vld1q_u32((const uint32_t*)a+i),vld1q_u32((const uint32_t*)b+i)));diff=vorrq_u32(diff,veorq_u32(vld1q_u32((const uint32_t*)a+i+4),vld1q_u32((const uint32_t*)b+i+4)));diff=vorrq_u32(diff,veorq_u32(vld1q_u32((const uint32_t*)a+i+8),vld1q_u32((const uint32_t*)b+i+8)));diff=vorrq_u32(diff,veorq_u32(vld1q_u32((const uint32_t*)a+i+12),vld1q_u32((const uint32_t*)b+i+12)));}
 for(;i+4<=n;i+=4)diff=vorrq_u32(diff,veorq_u32(vld1q_u32((const uint32_t*)a+i),vld1q_u32((const uint32_t*)b+i)));
 uint32x2_t halves=vorr_u32(vget_low_u32(diff),vget_high_u32(diff));tail=vget_lane_u32(halves,0)|vget_lane_u32(halves,1);
#endif
 for(;i<n;i++)tail|=(uint32_t)a[i]^(uint32_t)b[i];return tail==0;
}
static int r80_tested=-1;
static int r80_words_selftest(void){
 int a[80],b[80];for(int i=0;i<80;i++)a[i]=b[i]=(int)((uint32_t)i*1234567u^0x8a13fc29u);
 for(int off=0;off<4;off++)for(int n=0;n<=65;n++){
  if(!r80_words_raw(a+off,b+off,(size_t)n))return 0;
  for(int j=0;j<n;j+=7){b[off+j]^=0x10001;if(r80_words_raw(a+off,b+off,(size_t)n))return 0;b[off+j]^=0x10001;}
 }return 1;
}
static int r80_words_equal(const int*a,const int*b,size_t n){
 if(!n)return 1;if(r80_tested<0)r80_tested=r80_words_selftest();
 if(r80_tested)return r80_words_raw(a,b,n);
 for(size_t i=0;i<n;i++)if(a[i]!=b[i])return 0;return 1;
}
