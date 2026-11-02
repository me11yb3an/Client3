/* RFC 2104 HMAC-SHA256 over bounded messages, using the existing Rust SHA256
 * primitive. Wire protocol v1: separate directional keys, strict sequence,
 * nonce-bound handshake, monotonic lease and absolute session deadline. */
#include "h8_protocol.h"
#include <string.h>
extern int rv_sha256(const uint8_t*,size_t,uint8_t*);
static int equal(const uint8_t*a,const uint8_t*b,size_t n){volatile unsigned d=0;for(size_t i=0;i<n;i++)d|=a[i]^b[i];return d==0;}
static void wipe(void*p,size_t n){volatile uint8_t*q=p;while(n--)*q++=0;}
int h8_mac(const uint8_t key[32],const void*data,size_t n,uint8_t out[32]){
 uint8_t inner[64+H8_MAX_PACKET],outer[96];
 if(n>H8_MAX_PACKET)return 0;
 for(unsigned i=0;i<64;i++){inner[i]=(i<32?key[i]:0)^0x36;outer[i]=(i<32?key[i]:0)^0x5c;}
 memcpy(inner+64,data,n);int ok=rv_sha256(inner,64+n,outer+64)==0&&rv_sha256(outer,96,out)==0;
 wipe(inner,sizeof(inner));wipe(outer,sizeof(outer));return ok;
}
void h8_disarm(H8Session*s){wipe(s,sizeof(*s));}
int h8_arm(H8Session*s,const uint8_t key[32],const uint8_t nonce[32],uint64_t now){h8_disarm(s);memcpy(s->provision,key,32);memcpy(s->nonce,nonce,32);s->armed=1;s->expires=now+H8_SESSION_US;s->lease=s->expires;return 1;}
int h8_alive(H8Session*s,uint64_t now){if(!s->armed||now>=s->expires||(s->authenticated&&now>=s->lease)){h8_disarm(s);return 0;}return 1;}
int h8_hello(H8Session*s,uint8_t out[68]){if(!s->armed||s->authenticated)return 0;memcpy(out,"R8H1",4);memcpy(out+4,s->nonce,32);return h8_mac(s->provision,out,36,out+36)?68:0;}
int h8_accept(H8Session*s,const uint8_t*p,size_t n,uint64_t now){
 uint8_t tag[32],master[32],seed[68];
 if(!h8_alive(s,now)||s->authenticated||n!=100||memcmp(p,"R8A1",4)||!equal(s->nonce,p+4,32))return 0;
 if(!h8_mac(s->provision,p,68,tag)||!equal(tag,p+68,32))return 0;
 memcpy(seed,"R8K1",4);memcpy(seed+4,p+4,64);
 if(!h8_mac(s->provision,seed,68,master)||!h8_mac(master,"pc-to-vita",10,s->rxkey)||!h8_mac(master,"vita-to-pc",10,s->txkey)||!h8_mac(master,"session-id",10,tag)){wipe(master,32);h8_disarm(s);return 0;}
 memcpy(s->sid,tag,16);wipe(master,32);wipe(s->provision,32);s->authenticated=1;s->lease=now+H8_LEASE_US;return 1;
}
static uint64_t get64(const uint8_t*p){uint64_t v=0;for(int i=0;i<8;i++)v=(v<<8)|p[i];return v;}
static void put64(uint8_t*p,uint64_t v){for(int i=7;i>=0;i--){p[i]=(uint8_t)v;v>>=8;}}
int h8_decode(H8Session*s,const uint8_t*p,size_t n,uint64_t now,uint8_t*type,uint8_t payload[1024],size_t*length){
 uint8_t tag[32];
 if(!h8_alive(s,now)||!s->authenticated||n<63||n>H8_MAX_PACKET||memcmp(p,"R8D1",4)||!equal(s->sid,p+4,16))return 0;
 size_t len=((size_t)p[29]<<8)|p[30];uint64_t seq=get64(p+20);
 if(len>1024||n!=63+len||seq!=s->rxseq+1||!h8_mac(s->rxkey,p,31+len,tag)||!equal(tag,p+31+len,32))return 0;
 *type=p[28];*length=len;memcpy(payload,p+31,len);s->rxseq=seq;s->lease=now+H8_LEASE_US;return 1;
}
int h8_encode(H8Session*s,uint8_t type,const void*data,size_t n,uint64_t now,uint8_t out[1087]){
 if(n>1024||!h8_alive(s,now)||!s->authenticated)return 0;
 memcpy(out,"R8D1",4);memcpy(out+4,s->sid,16);put64(out+20,++s->txseq);out[28]=type;out[29]=n>>8;out[30]=n;memcpy(out+31,data,n);
 return h8_mac(s->txkey,out,31+n,out+31+n)?(int)(63+n):0;
}
