/* Cache only the static artwork of an explicitly cleared chat rectangle.
 * Text and animated portraits still come from the original live rasterizer. */
void ve_chat_draw_clean(Client*c){
    typedef struct{int*pixels;size_t capacity;int valid,key[15];const void*font;} Background;
    /* Separate shown/hidden entries keep dialogue open/close transitions warm. */
    static Background entries[2];
    Background*entry=entries+(ve_chat_hidden(c)?1:0);
    const LcRect*r=&lc_game_geometry(LC_CHAT)->rect;
    PixMap*dst=c->area_viewport;
    int tx=ve_chat_text_x(),ty=ve_chat_text_y();
    int border=ui_thin_preview()?1:2;
    /* The content must remain inside the cached border/buttons, so copying
     * it after the static layer has exactly the original composition order. */
    if(!dst||dst->width!=960||r->x<0||r->y<0||r->w<360||r->w>960||r->h<169||r->h>192||
       r->x+r->w>dst->width||r->y+r->h>dst->height||
       tx<r->x+border||tx+r->w-16>r->x+r->w-border||ty<r->y+border||
       ty+c->area_chatback->height>r->y+142-border){ve_chat_draw(c);return;}
    int key[]={r->x,r->y,r->w,r->h,tx,ty,ve_chat_hidden(c),chat_hidden_requested,
        c->public_chat_setting,c->private_chat_setting,c->trade_chat_setting,border,dst->width,dst->height,c->varps[323]};
    size_t bytes=(size_t)r->w*r->h*sizeof(int);
    if(bytes!=entry->capacity){
        int*next=malloc(bytes);
        if(!next){ve_chat_draw(c);return;}
        free(entry->pixels);entry->pixels=next;entry->capacity=bytes;entry->valid=0;
    }
    if(!entry->valid||entry->font!=c->font_plain11||memcmp(entry->key,key,sizeof key)){
        ve_chat_draw_layers(c,0);
        for(int row=0;row<r->h;row++)memcpy(entry->pixels+(size_t)row*r->w,dst->pixels+(size_t)(r->y+row)*dst->width+r->x,(size_t)r->w*sizeof(int));
        memcpy(entry->key,key,sizeof key);entry->font=c->font_plain11;entry->valid=1;
    }else{
        for(int row=0;row<r->h;row++)memcpy(dst->pixels+(size_t)(r->y+row)*dst->width+r->x,entry->pixels+(size_t)row*r->w,(size_t)r->w*sizeof(int));
        pixmap_bind(dst);
    }
    if(!ve_chat_hidden(c))ve_chat_content(c);
}
