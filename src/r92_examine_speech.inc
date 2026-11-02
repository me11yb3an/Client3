/* Examine retains its original chat-log message and becomes local player
 * overhead speech through the normal typed-chat lifetime/rendering path.
 * No public-chat packet, animation, server timing or interaction is sent. */
static void client_examine_message(Client*c,const char*text){
 if(!text)return;
 char message[CHAT_LENGTH+1];snprintf(message,sizeof message,"%s",text);
 client_add_message(c,0,message,"");
 if(!c->ingame||!c->local_player||!text[0])return;
 PathingEntity*p=&c->local_player->pathing_entity;
 memcpy(p->chat,message,strlen(message)+1);p->chatColor=0;p->chatStyle=0;p->chatTimer=150;
}
