#pragma once
/* Opt-in diagnostic colours only. Eighteen bits fit in six high bits of
 * each RGB channel; the low two bits tolerate framebuffer quantisation.
 * Model: 15-bit location ID + two-bit physical plane. Terrain: bit17,
 * triangle parity at16, plane at14, local x/z at7/0. */
static void r48_source_colour(Vertex*stream,int first,int end,unsigned code,int enabled){
 if(!enabled)return;
 for(int i=first;i<end;i++)if(stream[i].a==255){
  stream[i].r=(unsigned char)(((code&63u)<<2)|2u);
  stream[i].g=(unsigned char)((((code>>6)&63u)<<2)|2u);
  stream[i].b=(unsigned char)((((code>>12)&63u)<<2)|2u);
  stream[i].u=stream[i].v=0.f;
 }
}
