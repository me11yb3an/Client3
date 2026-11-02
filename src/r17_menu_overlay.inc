/* Transient context menu: original rasterizer and input coordinates, separate
 * bounded texture. Never put menu pixels into the retained world/UI texture. */
static PixMap *ve_menu_pm;
static int ve_menu_ready,ve_seen_menu_baked;
static uint64_t ve_menu_hash;
static int ve_menu_valid;
static void client_raster_menu(Client*c,int x,int y,int mouseX,int mouseY);

static int ve_menu_separate(Client*c){
 return ve_menu_ready&&ve_enabled()&&ve_native_markers()&&c->ingame&&c->scene_state==2;
}
static void ve_menu_prepare(Client*c){
 ve_menu_ready=0;
 if(!ve_enabled()||!ve_native_markers()||!c->ingame||c->scene_state!=2)return;
 if(!c->menu_visible){ve_menu_ready=1;ve_menu_valid=0;return;}
 if(c->menu_width<=0||c->menu_height<=0||c->menu_width>512||c->menu_height>512)return;
 int w=64,h=64;while(w<c->menu_width)w*=2;while(h<c->menu_height)h*=2;
 if(ve_menu_pm){if(w<ve_menu_pm->width)w=ve_menu_pm->width;if(h<ve_menu_pm->height)h=ve_menu_pm->height;}
 size_t old=ve_menu_pm?(size_t)ve_menu_pm->width*ve_menu_pm->height*12:0;
 size_t bytes=(size_t)w*h*12;
 /* Include the surface in the existing 4MiB transient-interface budget. */
 if(ve_effect_bytes-old+bytes>4u*1024u*1024u)return;
 if(!ve_menu_pm||ve_menu_pm->width!=w||ve_menu_pm->height!=h){
  if(ve_menu_pm){pixmap_free(ve_menu_pm);ve_effect_bytes-=old;ve_menu_pm=NULL;}
  ve_menu_valid=0;Pix2D saved=_Pix2D;ve_menu_pm=pixmap_new(w,h);_Pix2D=saved;
  if(!ve_menu_pm)return;ve_effect_bytes+=bytes;
 }
 ve_menu_ready=1;
}
static void ve_menu_origin(Client*c,int*x,int*y){
 *x=c->menu_x;*y=c->menu_y;
 if(c->menu_area==1){*x+=ui474_side_x();*y+=ui474_side_y();}
 else if(c->menu_area==2){*x+=22;*y+=ve_chat_text_y();}
}
static uint64_t ve_menu_key(Client*c,int mx,int my){
 uint64_t key=1469598103934665603ull;
 int hover=-1;
 for(int i=0;i<c->menu_size;i++){int row=(c->menu_size-1-i)*15+31;if(mx>0&&mx<c->menu_width&&my>row-13&&my<row+3)hover=i;}
 int values[]={c->menu_width,c->menu_height,c->menu_size,hover};
 for(unsigned j=0;j<sizeof(values)/sizeof(values[0]);j++){key^=(unsigned)values[j];key*=1099511628211ull;}
#ifdef __vita__
 key^=(unsigned)c->vita_menu_active;key*=1099511628211ull;
 key^=(unsigned)c->vita_menu_selected;key*=1099511628211ull;
#endif
 for(int i=0;i<c->menu_size;i++){
  const unsigned char*s=(const unsigned char*)c->menu_option[i];
  do{key^=*s;key*=1099511628211ull;}while(*s++);
 }
 return key;
}
static void ve_menu_draw(Client*c){
 if(!c->menu_visible||!ve_menu_separate(c)||!ve_menu_pm)return;
 int x,y;ve_menu_origin(c,&x,&y);
 int mx=c->shell->mouse_x-x,my=c->shell->mouse_y-y;
 uint64_t key=ve_menu_key(c,mx,my);Pix2D saved=_Pix2D;bool saved_gl=_Custom.use_opengl11;
 if(!ve_menu_valid||ve_menu_hash!=key){
  _Custom.use_opengl11=false;pixmap_bind(ve_menu_pm);
  memset(ve_menu_pm->pixels,0xff,(size_t)ve_menu_pm->width*ve_menu_pm->height*sizeof(int));
  client_raster_menu(c,0,0,mx,my);ve_menu_hash=key;ve_menu_valid=1;
 }
 pixmap_draw(ve_menu_pm,x,y);_Pix2D=saved;_Custom.use_opengl11=saved_gl;
}
