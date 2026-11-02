/* Existing panel blits. The approved baseline takes the original allocation-free path.
 * Diagnostic parent sizes crop panel buffers; no scaling, filtering, art or font edits. */
#include "layoutcore_draw.h"
#include "layoutcore.h"
#include "pix2d.h"
#include <string.h>
extern Pix2D _Pix2D;
void lc_draw(PixMap*p,int role,int x,int y){
 if(!lc_game_altered()){pixmap_draw(p,x,y);return;}
 const LcGeometry*g=lc_game_geometry(role);if(!g||!g->visible)return;
 int l=x>g->clip.x?x:g->clip.x,t=y>g->clip.y?y:g->clip.y;
 int r=x+p->width<g->clip.x+g->clip.w?x+p->width:g->clip.x+g->clip.w;
 int b=y+p->height<g->clip.y+g->clip.h?y+p->height:g->clip.y+g->clip.h;
 if(r<=l||b<=t)return;
 if(l==x&&t==y&&r==x+p->width&&b==y+p->height){pixmap_draw(p,x,y);return;}
 Pix2D saved=_Pix2D;PixMap*crop=pixmap_new(r-l,b-t);
 for(int yy=t;yy<b;yy++)memcpy(crop->pixels+(yy-t)*(r-l),p->pixels+(yy-y)*p->width+l-x,(size_t)(r-l)*sizeof(int));
 pixmap_draw(crop,l,t);pixmap_free(crop);
 _Pix2D=saved;
}
