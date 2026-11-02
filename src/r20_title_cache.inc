/* Exact title-panel cache. Password contents are never retained: the
 * original renderer displays only their length as asterisks. Oversized
 * messages fall back to the complete original compositor without truncation. */
typedef struct {
 const void *owner,*target,*scratch,*background,*box,*button,*font;
 int loading,progress,state,field,new_player,caret;
 size_t password_length;
 char username[sizeof(((Client*)0)->username)];
 char message0[256],message1[256],loading_message[256];
} R20TitleKey;
typedef struct {R20TitleKey key;int*pixels;int valid;} R20TitleEntry;
static R20TitleEntry title_cached[2];
static int title_cache_active=-1;
static unsigned title_asset_generation;
void vita_title_invalidate(void){
 for(int i=0;i<2;i++){free(title_cached[i].pixels);memset(&title_cached[i],0,sizeof title_cached[i]);}
 title_cache_active=-1;title_asset_generation++;
}
unsigned vita_title_generation(void){return title_asset_generation;}
static int title_key_text(char*dst,size_t capacity,const char*text){
 if(!text)text="";size_t n=strlen(text);if(n>=capacity)return 0;
 memcpy(dst,text,n+1);return 1;
}
static int title_make_key(Client*c,bool loading,const char*message,int progress,R20TitleKey*k){
 memset(k,0,sizeof(*k));
 k->owner=c;k->target=c->title_panel_scaled;k->scratch=c->image_title4;k->background=c->title_panel_background;
 k->box=c->image_titlebox;k->button=c->image_titlebutton;k->font=c->font_bold12;
 k->loading=loading;k->progress=loading?progress:0;k->state=c->title_screen_state;
 k->field=c->title_login_field;k->new_player=client_title_new_player();
 k->caret=!loading&&c->title_screen_state==2&&(c->title_login_field==0||c->title_login_field==1)&&_Client.loop_cycle%40<20;
 k->password_length=strlen(c->password);
 return title_key_text(k->username,sizeof k->username,c->username)&&
        title_key_text(k->message0,sizeof k->message0,c->login_message0)&&
        title_key_text(k->message1,sizeof k->message1,c->login_message1)&&
        title_key_text(k->loading_message,sizeof k->loading_message,loading?message:NULL);
}
