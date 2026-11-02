/* Packed225 bank/shop/trade roots plus the existing informational skill and
 * quest-guide families. Mandatory creation/unknown interactive forms are
 * not inferred from their text. Close through the original server packet. */
static int r91_dismissible_root(int id){
 static const int roots[]={5292,3824,3323,3443,7386,7558,5993,6073,1136,3023,3702,5392,297,1689,2302,6987,6965,6994,5596};
 for(unsigned i=0;i<sizeof roots/sizeof roots[0];i++)if(id==roots[i])return 1;
 return 0;
}
static int client_dismiss_world_popup(Client*c){
 if(c->shell->mouse_click_button!=1||c->menu_visible||c->obj_drag_area||c->vita_interface_blocked||
    c->show_social_input||c->chatback_input_open||c->modal_message[0]||!r91_dismissible_root(c->viewport_interface_id))return 0;
 int id=c->viewport_interface_id;if(id<0||id>=_Component.count)return 0;
 Component*root=_Component.instances[id];if(!root||root->width<=0||root->height<=0)return 0;
 int px=c->shell->mouse_click_x,py=c->shell->mouse_click_y;
 if(!(ve_enabled()?ve_world_input(px,py):game_world_input(px,py))||lc_game_covers(px,py))return 0;
 int x=(ve_enabled()?224:8+GAME_MODAL_X)+client_modal_offset(c,id,ve_enabled());
 int y=(ve_enabled()?0:11)+client_modal_y(c,id,ve_enabled());
 if(px>=x&&py>=y&&px-x<root->width&&py-y<root->height)return 0;
 closeInterfaces(c);c->menu_size=0;c->menu_visible=false;c->viewportHoveredInterfaceIndex=0;
 /* Use the original ground-pick/pathfinding action once, after closing.
  * Consume the raw click so stale popup/NPC/object menu actions cannot run. */
 client_ui474_invoke(c,660,0,px,py);
 c->shell->mouse_click_button=0;return 1;
}
