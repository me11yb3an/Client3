/* Live locations never retain a scene-slot/model pointer. Resource stage
 * changes and respawns update their own model, not the retained landscape. */
#include "r24_door_states.h"
#include "r100_crypt_fixtures.h"
#define R16_LIVE_DOOR 1u
#define R16_LIVE_SKILL 2u
#define R16_LIVE_ANIM 4u
static unsigned live_door_updates,live_skill_updates,live_item_updates;
static unsigned char live_loc_type[32768];
static unsigned live_loc_revision[16384];
static int live_loc_log_ready;
extern void ba_cadence_diagnostic(const char*line);
static int door_word(const char*s,const char*word){
 if(!s)return 0;
 for(;*s;s++){
  const char*a=s,*b=word;
  while(*a&&*b){int c=(unsigned char)*a;if(c>='A'&&c<='Z')c+='a'-'A';if(c!=*b)break;a++;b++;}
  if(!*b)return 1;
 }
 return 0;
}
static int loc_action_is(const char*s,const char*word){
 if(!s)return 0;
 for(;;){while(*s&&(*s==' '||*s=='-'||*s=='_'))s++;
  int c=(unsigned char)*s;if(c>='A'&&c<='Z')c+='a'-'A';
  if(c!=(unsigned char)*word)return 0;if(!c)return 1;s++;word++;
 }
}
static unsigned live_loc_flags(int bitset){
#if defined(GL11) && defined(__vita__)
 if(((unsigned)bitset&0x60000000u)!=0x40000000u)return 0;
 int id=((unsigned)bitset>>14)&32767;
 if(id>=_LocType.count)return 0;
 if(!(live_loc_type[id]&128)){
  LocType*t=loctype_get(id);unsigned f=0;
  if(t){
   if(r100_barrows_sarcophagus(id))f|=R16_LIVE_SKILL;
   /* Packaged inviswall83 temporarily replaces tutorial doors/gates. It has
    * no display name/action, so name-based door detection misses both its
    * insertion and timed removal and rebuilds the entire landscape twice. */
   if(id==83||r24_unnamed_door_state(id))f|=R16_LIVE_DOOR;
   if(door_word(t->name,"door")||door_word(t->name,"gate"))f|=R16_LIVE_DOOR;
   /* Includes depleted states without actions, not just the harvestable mesh. */
   if(door_word(t->name,"tree")||door_word(t->name,"stump")||door_word(t->name,"stall")||door_word(t->name,"chest")||door_word(t->name,"spinning wheel")||door_word(t->name,"potter")||door_word(t->name,"furnace")||door_word(t->name,"anvil")||door_word(t->name,"altar")||door_word(t->name,"cooking")||loc_action_is(t->name,"loom")||loc_action_is(t->name,"range")||loc_action_is(t->name,"fire")||loc_action_is(t->name,"ashes")||loc_action_is(t->name,"rocks")||loc_action_is(t->name,"rock")||loc_action_is(t->name,"oak")||loc_action_is(t->name,"willow")||loc_action_is(t->name,"yew")||loc_action_is(t->name,"evergreen")||(!t->models&&t->shapes_and_models_count==0))f|=R16_LIVE_SKILL;
   /* Climb-over temporarily merges the fixture into the player model.
    * Keep both removal and restoration live so a landscape rebuild cannot
    * consume the entire forced-movement animation before it is presented. */
   static const char*ops[]={"mine","prospect","chopdown","chop","pick","pickup","stealfrom","picklock","searchfortraps","craftrune","craft","smelt","fish","bait","net","lure","harpoon","cage","swingon","swingacross","balance","climbover","climb","climbup","climbdown","climbon","climbto","climbout","cross","squeezethrough","walkacross","jumpfrom","jumpover","jump","climb=down"};
   if(t->op)for(int i=0;i<5;i++)for(unsigned k=0;k<sizeof(ops)/sizeof(*ops);k++)if(loc_action_is(t->op[i],ops[k]))f|=R16_LIVE_SKILL;
   if(t->anim!=-1)f|=R16_LIVE_ANIM;
  }
  live_loc_type[id]=(unsigned char)(128|f);
 }
 return live_loc_type[id]&7;
#else
 (void)bitset;return 0;
#endif
}
int r16_live_door(int bitset){return (live_loc_flags(bitset)&R16_LIVE_DOOR)!=0;}
int r16_live_location(int bitset){return live_loc_flags(bitset)!=0;}
int r16_location_changed(int bitset){
 unsigned f=live_loc_flags(bitset),tile=(unsigned)bitset&16383;
 if(live_loc_log_ready){char line[144];snprintf(line,sizeof line,"# loc_change %llu id %u tile %u %u class %u version %u",(unsigned long long)ba_timing_now(),((unsigned)bitset>>14)&32767,tile&127,tile>>7,f,live_loc_revision[tile]+1);ba_cadence_diagnostic(line);}
 if(!f)return 0;
 live_loc_revision[tile]++;
 if(f&R16_LIVE_DOOR)live_door_updates++;
 if(f&(R16_LIVE_SKILL|R16_LIVE_ANIM))live_skill_updates++;
 return 1;
}
int r16_door_changed(int bitset){if(!r16_live_door(bitset))return 0;return r16_location_changed(bitset);}
unsigned r16_door_updates(void){return live_door_updates;}
unsigned r16_skill_updates(void){return live_skill_updates;}

/* Ground stacks already update live; version their GPU entry as well so a
 * different item/stack model cannot reuse an older mesh at a recycled address. */
void r16_grounditem_changed(int x,int z){
 if(x<0||z<0||x>=128||z>=128)return;
 unsigned tile=(unsigned)x|((unsigned)z<<7);live_loc_revision[tile]++;live_item_updates++;
 if(live_loc_log_ready){char line[128];snprintf(line,sizeof line,"# item_stack_change %llu tile %d %d version %u",(unsigned long long)ba_timing_now(),x,z,live_loc_revision[tile]);ba_cadence_diagnostic(line);}
}
unsigned r16_item_updates(void){return live_item_updates;}
