#pragma once
#include <stdbool.h>
#include <stdio.h>
static bool ui_water_preview(void){static int enabled=-1;if(enabled<0){FILE*f=fopen("ux0:/data/RS2Vita/thin-ui-preview.txt","r");enabled=1;if(f){fscanf(f,"%d",&enabled);fclose(f);}}return enabled==1;}

static bool ui_thin_preview(void){static int width=-1;if(width<0){width=1;FILE*f=fopen("ux0:/data/RS2Vita/outline-width-preview.txt","r");if(f){fscanf(f,"%d",&width);fclose(f);}}return width==1;}
#include <stdlib.h>
#include <string.h>
/* Preserve the silhouette and one outer black pixel. Replace only doubled
 * inner black edge pixels with an adjacent original stone/shading color. */
static void ui_one_pixel_border(unsigned*p,int stride,int x0,int y0,int w,int h){
 if(!ui_thin_preview())return;
 unsigned*old=malloc((size_t)w*h*sizeof(unsigned));if(!old)abort();
 for(int y=0;y<h;y++)memcpy(old+y*w,p+(y0+y)*stride+x0,(size_t)w*sizeof(unsigned));
 for(int y=0;y<h;y++)for(int x=0;x<w;x++){
  unsigned c=old[y*w+x];if(!(c>>24)||(c&0xffffffu)>0x010101u)continue;
  bool outer=x==0||y==0||x==w-1||y==h-1;
  if(!outer)outer=!(old[y*w+x-1]>>24)||!(old[y*w+x+1]>>24)||!(old[(y-1)*w+x]>>24)||!(old[(y+1)*w+x]>>24);
  if(outer)continue;
  bool near=x<2||y<2||x>=w-2||y>=h-2;
  if(!near)near=!(old[y*w+x-2]>>24)||!(old[y*w+x+2]>>24)||!(old[(y-2)*w+x]>>24)||!(old[(y+2)*w+x]>>24);
  if(!near)continue;
  int best=100;unsigned replacement=c;
  for(int dy=-3;dy<=3;dy++)for(int dx=-3;dx<=3;dx++){
   int xx=x+dx,yy=y+dy,d=abs(dx)+abs(dy);if(xx<0||xx>=w||yy<0||yy>=h||d>=best)continue;
   unsigned n=old[yy*w+xx];if((n>>24)&&(n&0xffffffu)>0x010101u){replacement=n;best=d;}
  }
  p[(y0+y)*stride+x0+x]=replacement;
 }free(old);
}
