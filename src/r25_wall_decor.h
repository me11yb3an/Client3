#pragma once
/* Preserve the already accepted window fitting. Ordinary wall ornaments
 * retain the original client's no-offset / wall-width placement. */
/* Full packaged-world inventory also contains quest/temple window variants.
 * These have the same shape4 shallow panel convention as the base windows. */
static int r25_window_inset(int id) {
 return (id>=1821&&id<=1856)||id==2683||id==3362||id==3559||id==3560;
}
static int r25_straight_decor_inset(int id) { return r25_window_inset(id)?18:0; }
static int r25_offset_decor_extra(int id) { return r25_window_inset(id)?2:0; }

typedef struct { int yaw,x,z; } R25DecorPlacement;
/* The original world renderer's authored inset/outset corner transforms.
 * Shared by retained scenery, animated/live fixtures and pointer picking. */
static int r25_decor_placements(int type,int angle,int x,int z,R25DecorPlacement out[2]) {
 if(!(type&0x300)){out[0]=(R25DecorPlacement){angle,x,z};return 1;}
 int r=angle&3,n=0;
 static const int ix[]={53,-53,-53,53},iz[]={-53,-53,53,53};
 static const int ox[]={-45,45,45,-45},oz[]={45,45,-45,-45};
 if(type&0x100)out[n++]=(R25DecorPlacement){r*512+256,x+ix[r],z+iz[r]};
 if(type&0x200)out[n++]=(R25DecorPlacement){(r*512+1280)&2047,x+ox[r],z+oz[r]};
 return n;
}
