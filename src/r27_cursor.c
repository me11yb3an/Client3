#include "r27_cursor.h"
#include "r27_cursor_images.h"
#include "offline_paths.h"
#include "pix2d.h"
#include <stdio.h>
extern Pix2D _Pix2D;
#ifndef R27_CURSOR_PREFERENCE
#define R27_CURSOR_PREFERENCE BA_PREFERENCE("cursor-style.txt")
#endif
static int choice=-1;
const R27Cursor*r27_cursor_at(int index){return &r27_cursor_images[index>=0&&index<5?index:0];}
int r27_cursor_choice(void){
 if(choice>=0)return choice;
 choice=0;FILE*f=fopen(R27_CURSOR_PREFERENCE,"r");
 if(f){int n;if(fscanf(f,"%d",&n)==1&&n>=0&&n<5)choice=n;fclose(f);}
 return choice;
}
int r27_cursor_cycle(void){
 choice=(r27_cursor_choice()+1)%5;
 FILE*f=fopen(R27_CURSOR_PREFERENCE,"w");if(f){fprintf(f,"%d\n",choice);fclose(f);}
 return choice;
}
/* PixMap conventions: A0 is opaque RGB, FFFFFFFF is transparent, other
 * alpha bytes retain their original straight-alpha coverage. */
static uint32_t r27_composite(uint32_t src,uint32_t dst){
 if(src==0xffffffffu)return dst;
 unsigned a=src>>24;if(!a)return src;
 unsigned da=dst==0xffffffffu?0:dst>>24;if(!da&&dst!=0xffffffffu)da=255;
 unsigned out=a+da*(255-a)/255,rgb=0;
 for(int shift=0;shift<=16;shift+=8){
  unsigned value=(((src>>shift&255)*a)+((dst>>shift&255)*da*(255-a)/255))/out;
  rgb|=value<<shift;
 }
 return rgb|(out<255?out<<24:0);
}
void r27_cursor_preview(int x,int y,int width,int height){
 int selected=r27_cursor_choice();const R27Cursor*s=r27_cursor_at(selected);
 int left=0,top=0,right=s->width,bottom=s->height;
 /* The dagger's CUR canvas has extra transparent padding. Center its
  * visible artwork in this button only; keep its actual pointer hotspot. */
 if(selected==4){
  left=s->width;top=s->height;right=bottom=0;
  for(int j=0;j<s->height;j++)for(int i=0;i<s->width;i++)if(s->pixels[j*s->width+i]!=0xffffffffu){
   if(i<left)left=i;if(j<top)top=j;if(i+1>right)right=i+1;if(j+1>bottom)bottom=j+1;
  }
 }
 x+=(width-(right-left))/2-left;y+=(height-(bottom-top))/2-top;
 for(int j=0;j<s->height;j++)for(int i=0;i<s->width;i++){
  int px=x+i,py=y+j;
  if(px<_Pix2D.left||px>=_Pix2D.right||py<_Pix2D.top||py>=_Pix2D.bottom||px<0||py<0||px>=_Pix2D.width||py>=_Pix2D.height)continue;
  uint32_t src=s->pixels[j*s->width+i];if(src==0xffffffffu)continue;
  int*p=_Pix2D.pixels+py*_Pix2D.width+px;*p=(int)r27_composite(src,(uint32_t)*p);
 }
}
