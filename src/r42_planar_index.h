#pragma once
#include <stdint.h>
#include <string.h>
/* Bake-local candidate index. Hash collisions still run every original
 * plane/material/coverage check. Reverse insertion keeps ascending face
 * order, including when several unrelated planes share a bucket. */
#define R42_FACES 4096
#define R42_BUCKETS 512
typedef struct{unsigned epoch;int head;}R42PlaneBucket;
static R42PlaneBucket r42_buckets[R42_BUCKETS];
static int r42_links[R42_FACES*3];
static unsigned r42_epoch;
static void r42_begin(void){if(!++r42_epoch){memset(r42_buckets,0,sizeof r42_buckets);r42_epoch=1;}}
static unsigned r42_bucket(int axis,int priority,float plane){
 uint32_t bits;memcpy(&bits,&plane,4);if(!(bits&0x7fffffffu))bits=0;
 uint32_t h=bits^((unsigned)(priority*3+axis)*0x9e3779b9u);h^=h>>16;h*=0x85ebca6bu;h^=h>>13;return h&(R42_BUCKETS-1);
}
static void r42_insert(int face,int axis,int priority,float plane){
 R42PlaneBucket*b=r42_buckets+r42_bucket(axis,priority,plane);int node=face*3+axis;
 r42_links[node]=b->epoch==r42_epoch?b->head:-1;b->head=node;b->epoch=r42_epoch;
}
static int r42_head(int axis,int priority,float plane){R42PlaneBucket*b=r42_buckets+r42_bucket(axis,priority,plane);return b->epoch==r42_epoch?b->head:-1;}
