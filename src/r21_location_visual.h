#pragma once
/* Keep the climbable fixture ID, action and collision; replace only its
 * misplaced iron appearance with the adjacent straight wooden railing. */
static int r21_visual_location_id(int id,int level,int x,int z,int shape,int rotation){
 return id==2068&&level==0&&x==3231&&z==3252&&shape==0&&rotation==0?980:id;
}
