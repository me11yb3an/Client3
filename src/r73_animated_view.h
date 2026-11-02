#pragma once
/* A padded sphere encloses the complete source cylinder, including geometry
 * below its origin. Use the software focal length: Enhanced's larger focal
 * length makes its real camera cone narrower, never wider. Invalid/unknown
 * bounds retain the original draw. No model, animation or pick state changes. */
static int r73_animated_in_view(int radius,int above,int below,int x,int y,int z,
 int syaw,int cyaw,int spitch,int cpitch,int halfx,int halfy,int farclip){
 if(radius<0||above<0||below<0||radius>8192||above>8192||below>8192||halfx<64||halfy<64||halfx>2048||halfy>2048||farclip<=50)return 1;
 float height=(above+below)*.5f,mid=y+(below-above)*.5f;
 float r=radius+height+32.f;
 float horizontal=((float)z*cyaw-(float)x*syaw)/65536.f;
 float depth=(mid*spitch+horizontal*cpitch)/65536.f;
 if(depth+r<50.f||depth-r>farclip)return 0;
 float sx=((float)z*syaw+(float)x*cyaw)/65536.f;
 float vertical=(mid*cpitch-horizontal*spitch)/65536.f;
 float hx=halfx/512.f,hy=halfy/512.f;
 float mx=r*(1.f+hx),my=r*(1.f+hy);
 if(sx+hx*depth< -mx||-sx+hx*depth< -mx||vertical+hy*depth< -my||-vertical+hy*depth< -my)return 0;
 return 1;
}
