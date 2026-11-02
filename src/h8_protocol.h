#pragma once
#include <stdint.h>
#include <stddef.h>
#define H8_MAX_PAYLOAD 1024
#define H8_MAX_PACKET 1087
#define H8_SESSION_US 600000000ULL
#define H8_LEASE_US 2000000ULL
typedef struct {
 uint8_t provision[32],nonce[32],rxkey[32],txkey[32],sid[16];
 uint64_t rxseq,txseq,expires,lease;
 int armed,authenticated;
} H8Session;
int h8_mac(const uint8_t key[32],const void*,size_t,uint8_t out[32]);
void h8_disarm(H8Session*);
int h8_arm(H8Session*,const uint8_t provision[32],const uint8_t nonce[32],uint64_t now);
int h8_hello(H8Session*,uint8_t out[68]);
int h8_accept(H8Session*,const uint8_t*,size_t,uint64_t now);
int h8_decode(H8Session*,const uint8_t*,size_t,uint64_t now,uint8_t *type,uint8_t payload[1024],size_t *length);
int h8_encode(H8Session*,uint8_t type,const void*,size_t,uint64_t now,uint8_t out[1087]);
int h8_alive(H8Session*,uint64_t now);
