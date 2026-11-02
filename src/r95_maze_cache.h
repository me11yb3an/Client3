#ifndef R95_MAZE_CACHE_H
#define R95_MAZE_CACHE_H
/* Only appended, static Barrows leaf definitions enter the existing exact
 * live-location cache. Moving/animated doors retain their original path.
 * Cache keys still include the tile revision, scene epoch and resolved pose. */
static int r95_static_maze_door(int bitset) {
    unsigned tag = (unsigned)bitset;
    unsigned id = (tag >> 14) & 32767u;
    return (tag & 0x60000000u) == 0x40000000u && ((id >= 3851u && id <= 3882u) || (id >= 3907u && id < 4035u));
}
static int r95_cache_pose(int bitset, int door, int live_anim, int model_anim) {
    return !live_anim && !model_anim && (!door || r95_static_maze_door(bitset));
}
#endif
