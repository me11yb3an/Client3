#pragma once
/* Tutorial acknowledgements belong to the shared game update, independent
 * of which renderer draws the sidebar. Preserve the original wire packet. */
static int r23_tutorial_tab_ack(Client*c){
 int tab=c->flashing_tab;
 if(!c->ingame||tab<0||tab>=14||c->selected_tab!=tab)return 0;
 c->flashing_tab=-1;c->redraw_sidebar=true;c->redraw_sideicons=true;
 p1isaac(c->out,175);p1(c->out,tab);return 1;
}
static int r23_tutorial_icon_visible(int flashing,int icon,int cycle){
 return flashing<0||flashing>=14||flashing!=icon||cycle%20<10;
}
static int r23_tutorial_visual_key(int flashing,int cycle){
 return flashing>=0&&flashing<14?flashing*2+(cycle%20>=10):-1;
}
