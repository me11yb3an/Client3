#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "clientstream.h"
#include "platform.h"
#include "embedded_server.h"

bool clientstream_init(void) { return true; }
ClientStream *clientstream_new(void) {
    ClientStream *s=calloc(1,sizeof(*s));
    if(s) s->socket=-1;
    return s;
}
ClientStream *clientstream_opensocket(int port) {
    (void)port;
    static int started;
    if(!started) {
        if(ba_server_start("app0:/serverdata","ux0:/data/RS2Vita/players",
             "app0:/sp/sp-bootstrap.js","app0:/sp/server.bundle.npc-unload.js")) return NULL;
        started=1;
        uint64_t deadline=rs2_now()+120000;
        while(ba_server_ready()==0 && rs2_now()<deadline) {
            if(ba_server_pump()) return NULL;
            rs2_sleep(1);
        }
    }
    if(ba_server_ready()!=1) return NULL;
    ClientStream *s=clientstream_new();
    if(!s) return NULL;
    if(ba_server_connect()) { free(s); return NULL; }
    return s;
}
void clientstream_close(ClientStream *s) {
    if(s && !s->closed) { ba_server_disconnect(); s->closed=true; }
}
void clientstream_destroy(ClientStream *s) { if(s){clientstream_close(s);free(s);} }
int clientstream_available(ClientStream *s,int n) {
    if(!s||s->closed||n<0||n>(int)sizeof(s->buf)) return 0;
    if(s->bufLen>=n) return 1;
    if(s->bufPos && s->bufLen) memmove(s->buf,s->buf+s->bufPos,s->bufLen);
    s->bufPos=0;
    int r=ba_server_read(s->buf+s->bufLen,n-s->bufLen);
    if(r<0){s->closed=true;return 0;}
    s->bufLen+=r;
    return s->bufLen>=n;
}
int clientstream_read_bytes(ClientStream *s,int8_t *dst,int off,int len) {
    if(!s||s->closed||off<0||len<0) return -1;
    int copy=len<s->bufLen?len:s->bufLen;
    if(copy){memcpy(dst+off,s->buf+s->bufPos,copy);s->bufPos+=copy;s->bufLen-=copy;off+=copy;len-=copy;}
    if(!s->bufLen)s->bufPos=0;
    uint64_t deadline=rs2_now()+5000;
    while(len){
        int n=ba_server_read(dst+off,len);
        if(n<0||rs2_now()>=deadline){clientstream_close(s);return -1;}
        if(n){off+=n;len-=n;}else rs2_sleep(1);
    }
    return 0;
}
int clientstream_read_byte(ClientStream *s){int8_t b;return clientstream_read_bytes(s,&b,0,1)?-1:(unsigned char)b;}
int clientstream_write(ClientStream *s,const int8_t *src,int len,int off){
    if(!s||s->closed||off<0||len<0)return -1;
    return ba_server_write(src+off,len);
}
const char *dnslookup(const char *hostname){(void)hostname;return "offline";}
