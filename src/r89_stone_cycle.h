#pragma once
#include "vita_button_edges.h"
/* Display order: top0..6, then bottom8..13. Socket7 is Run, an independent
 * action rather than a sidebar panel, and is excluded even if enabled. */
static int r89_next_stone(const int*interfaces,int current,int direction){
 if(!interfaces||!direction)return current;
 int step=direction>0?1:-1;
 int start=current>=0&&current<14?current:(step>0?13:0);
 for(int n=1;n<=14;n++){
  int tab=(start+step*n+28)%14;
  if(tab!=7&&interfaces[tab]>=0)return tab;
 }
 return current;
}
static int r89_stone_edge(VitaButtonEdges*left,VitaButtonEdges*right,uint64_t stamp,unsigned buttons,unsigned left_mask,unsigned right_mask){
 int l=vita_button_edge(left,stamp,buttons,left_mask),r=vita_button_edge(right,stamp,buttons,right_mask);
 if((buttons&left_mask)&&(buttons&right_mask))return 0;
 return r-l;
}
