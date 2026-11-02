#pragma once
#include <stdint.h>
/* Exact packaged spawn/counter matches. A fallback only for a stationary
 * banker at its source spawn facing its verified counter row.
 * Explicit script/entity facing always wins; no client yaw is forced. */
typedef struct {uint32_t coord;uint16_t type;int8_t dx,dz;} R64BankCounter;
static const R64BankCounter r64_bank_counters[]={
{42339674u,494,1,0},
{42339675u,494,1,0},
{42339677u,495,1,0},
{42339678u,494,1,0},
{42831106u,494,0,1},
{42847251u,495,-1,0},
{42847252u,494,-1,0},
{42847254u,494,-1,0},
{42863874u,495,0,1},
{42913026u,495,0,1},
{43535568u,495,-1,0},
{43535571u,494,-1,0},
{43535574u,494,-1,0},
{44600743u,495,0,-1},
{44600744u,494,0,-1},
{44633511u,494,0,-1},
{44666280u,495,0,-1},
{44682663u,495,0,-1},
{44682664u,494,0,-1},
{44715431u,494,0,-1},
{44715432u,495,0,-1},
{45993331u,494,0,-1},
{46009716u,494,0,-1},
{46026099u,495,0,-1},
{46058868u,495,0,-1},
{48254246u,494,0,1},
{48270630u,494,0,1},
{48287014u,495,0,1},
{48303398u,494,0,1},
{48319782u,494,0,1},
{49335577u,494,0,1},
{49351961u,495,0,1},
{49368345u,494,0,1},
{49384729u,494,0,1},
{49401113u,495,0,1},
{50597034u,494,1,0},
{50629802u,495,1,0},
{50629803u,495,1,0},
{50629805u,494,1,0},
{50728353u,494,-1,0},
{50728355u,495,-1,0},
{50728356u,495,0,1},
{50761124u,494,0,1},
{51121205u,953,0,-1},
{51153973u,953,0,-1},
{52219244u,494,-1,0},
{52219246u,495,-1,0},
{52219248u,495,-1,0},
{52219250u,494,-1,0},
{52219252u,494,-1,0},
{52219254u,495,-1,0},
{53284186u,495,0,1},
{53300570u,494,0,1},
{53316953u,495,0,1},
{53316954u,494,0,1},
{53349722u,494,0,1},
{53529692u,496,1,0},
{53529694u,497,1,0},
{53529695u,496,1,0},
{53529696u,497,1,0},
{53529697u,496,1,0},
{308399519u,166,1,0},
{308415904u,166,1,0},
{308464991u,166,1,0},
{308464992u,166,1,0},
{308464994u,166,1,0},
{308546912u,166,-1,0},
{308546915u,166,-1,0},
{308546968u,166,0,1},
{308579736u,166,0,1},

};
static int r64_banker_type(int t){switch(t){case 166:case 494:case 495:case 496:case 497:case 498:case 499:case 953:return 1;default:return 0;}}
static int r64_counter_facing(int x,int plane,int z,int type,int entity,int moving,
 int face_x,int face_z,int*orientation_x,int*orientation_z){
 if(!r64_banker_type(type)||x<0||x>16383||z<0||z>16383||plane<0||plane>3||entity!=-1||moving||face_x!=-1||face_z!=-1)return 0;
 /* PathingEntity.unfocus() supplies one tile SOUTH for every idle NPC.
  * Native trace confirms that fallback at the east-facing Draynor counters.
  * Correct only that default (or absent/zero direction), never a different
  * authored orientation, explicit face-coordinate mask or active movement. */
 int absent=*orientation_x==-1&&*orientation_z==-1;
 int self=*orientation_x==x*2+1&&*orientation_z==z*2+1;
 int south=*orientation_x==x*2+1&&*orientation_z==z*2-1;
 if(!absent&&!self&&!south)return 0;
 uint32_t key=((uint32_t)plane<<28)|((uint32_t)x<<14)|(uint32_t)z;
 int lo=0,hi=(int)(sizeof(r64_bank_counters)/sizeof(r64_bank_counters[0]));
 while(lo<hi){int mid=lo+(hi-lo)/2;if(r64_bank_counters[mid].coord<key)lo=mid+1;else hi=mid;}
 for(int i=lo;i<(int)(sizeof(r64_bank_counters)/sizeof(r64_bank_counters[0]))&&r64_bank_counters[i].coord==key;i++){
  const R64BankCounter*b=&r64_bank_counters[i];if(b->type!=type)continue;
  int nx=(x+b->dx)*2+1,nz=(z+b->dz)*2+1;
  int changed=*orientation_x!=nx||*orientation_z!=nz;
  *orientation_x=nx;*orientation_z=nz;return changed;
 }
 return 0;
}
