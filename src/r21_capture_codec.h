#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
/* Lossless diagnostic transport: literal packets or bounded backward copies.
 * No external codec dependency and no modification of captured RGBA bytes. */
static size_t r21_literal(uint8_t*out,size_t at,size_t cap,const uint8_t*src,size_t start,size_t n){
 if(!n)return at;if(n>128||at+1+n>cap)return 0;out[at++]=(uint8_t)(n-1);memcpy(out+at,src+start,n);return at+n;
}
static size_t r21_capture_pack(const uint8_t*src,size_t n,uint8_t*out,size_t cap){
 if(n>UINT32_MAX||cap<8)return 0;
 out[0]='R';out[1]='2';out[2]='1';out[3]='Z';for(int j=0;j<4;j++)out[4+j]=(uint8_t)(n>>(j*8));
 uint32_t history[4096];for(int j=0;j<4096;j++)history[j]=UINT32_MAX;
 size_t at=8,i=0,start=0,literals=0;
 while(i<n){
  size_t length=0,distance=0;
  if(n-i>=4){uint32_t word=(uint32_t)src[i]|((uint32_t)src[i+1]<<8)|((uint32_t)src[i+2]<<16)|((uint32_t)src[i+3]<<24);unsigned h=(word*2654435761u)>>20;uint32_t previous=history[h];history[h]=(uint32_t)i;
   if(previous!=UINT32_MAX&&previous<i&&i-previous<=65535){size_t max=n-i<131?n-i:131;while(length<max&&src[previous+length]==src[i+length])length++;if(length>=4)distance=i-previous;else length=0;}
  }
  if(length){if(literals){at=r21_literal(out,at,cap,src,start,literals);if(!at)return 0;literals=0;}if(at+3>cap)return 0;out[at++]=(uint8_t)(0x80u|(length-4));out[at++]=(uint8_t)distance;out[at++]=(uint8_t)(distance>>8);i+=length;start=i;}
  else{if(!literals)start=i;i++;literals++;if(literals==128){at=r21_literal(out,at,cap,src,start,literals);if(!at)return 0;literals=0;start=i;}}
 }
 if(literals)at=r21_literal(out,at,cap,src,start,literals);return at;
}
