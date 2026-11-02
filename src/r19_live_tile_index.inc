/* Cache coordinates only: current tile/model pointers are fetched at use.
 * Every scenery/resource/door/ground-stack mutation invalidates occupancy.
 * Actor-only movement does not require an entry: actors are picked/drawn
 * separately through temporaryLocs. Preserve the original level/x/z order. */
#define R19_TILE_CAP (4u*104u*104u)
static uint32_t live_tiles[R19_TILE_CAP];
static int live_tile_n;
static uint32_t live_pick_tiles[R19_TILE_CAP],live_dynamic_tiles[R19_TILE_CAP];
static int live_pick_n,live_dynamic_n;
static unsigned r84_fixture_class(int bitset){return (bitset>0?1u:0u)|(r16_live_location(bitset)?2u:0u);}
static unsigned r84_tile_class(const Ground*t,int x,int z){
 if(!t)return 0;unsigned flags=0;
 if(t->wall)flags|=r84_fixture_class(t->wall->bitset);
 if(t->decor)flags|=r84_fixture_class(t->decor->bitset);
 if(t->groundDecor)flags|=r84_fixture_class(t->groundDecor->bitset);
 if(t->groundObj)flags|=2u|(t->groundObj->bitset>0?1u:0u);
 for(int i=0;i<t->locCount;i++){
  Location*l=t->locs[i];if(l&&!l->r10_temporary&&l->minSceneTileX==x&&l->minSceneTileZ==z)flags|=r84_fixture_class(l->bitset);
 }
 return flags;
}
static struct {
    World3D *world; Ground ****tiles;
    int stamp, top, minlevel, maxlevel, x0, x1, z0, z1, valid;
    unsigned doors, skills, items;
} live_tile_key;
static int tile_has_fixture(const Ground *t) {
    return t && (t->wall || t->decor || t->groundDecor || t->groundObj || t->locCount);
}
static int live_tile_index(World3D *w,int top) {
    int x0=0,x1=w->maxTileX,z0=0,z1=w->maxTileZ;
    (void)top;
    if(w->minLevel<0||w->maxLevel>4||x0<0||z0<0||x1>w->maxTileX||z1>w->maxTileZ||x1>104||z1>104)return 0;
    if(live_tile_key.valid && live_tile_key.world==w && live_tile_key.tiles==w->levelTiles &&
       live_tile_key.stamp==stamp &&
       live_tile_key.minlevel==w->minLevel && live_tile_key.maxlevel==w->maxLevel &&
       live_tile_key.x0==x0 && live_tile_key.x1==x1 && live_tile_key.z0==z0 && live_tile_key.z1==z1 &&
       live_tile_key.doors==live_door_updates && live_tile_key.skills==live_skill_updates && live_tile_key.items==live_item_updates)return 1;
    live_tile_key.valid=0;live_tile_n=live_pick_n=live_dynamic_n=0;
    for(int level=w->minLevel;level<w->maxLevel;level++)
      for(int x=x0;x<x1;x++)for(int z=z0;z<z1;z++) {
        Ground*t=w->levelTiles[level][x][z];
        if(!t||(!tile_has_fixture(t)&&!tile_has_fixture(t->bridge)))continue;
        if((unsigned)live_tile_n>=R19_TILE_CAP)return 0;
        unsigned code=((unsigned)level<<16)|((unsigned)x<<8)|(unsigned)z;
        live_tiles[live_tile_n++]=code;
        unsigned flags=r84_tile_class(t,x,z)|r84_tile_class(t->bridge,x,z);
        if(flags&1)live_pick_tiles[live_pick_n++]=code;
        if(flags&2)live_dynamic_tiles[live_dynamic_n++]=code;
      }
    live_tile_key.world=w;live_tile_key.tiles=w->levelTiles;live_tile_key.stamp=stamp;live_tile_key.top=top;
    live_tile_key.minlevel=w->minLevel;live_tile_key.maxlevel=w->maxLevel;
    live_tile_key.x0=x0;live_tile_key.x1=x1;live_tile_key.z0=z0;live_tile_key.z1=z1;
    live_tile_key.doors=live_door_updates;live_tile_key.skills=live_skill_updates;live_tile_key.items=live_item_updates;
    live_tile_key.valid=1;return 1;
}
