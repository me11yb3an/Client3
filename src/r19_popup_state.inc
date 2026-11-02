/* Exact bounded UI-state snapshots; no hash collisions and no timer-based
 * freezing. Unchanged animation frames reuse pixels, while scripts, item
 * counts, scroll positions, content, selection and actual poses invalidate. */
#define R19_POPUP_STATE_BYTES (512u*1024u)
static unsigned char popup_before[R19_POPUP_STATE_BYTES],popup_now[R19_POPUP_STATE_BYTES];
static size_t popup_before_n,popup_now_n;static int popup_state_valid,popup_nodes;
static size_t popup_trace_start[4096];static int popup_trace_id[4096],popup_trace_n;
static unsigned popup_reuses,popup_rasters;static int popup_reason,popup_diff_component=-1,popup_diff_relative=-1;
static int popup_state_add(const void*p,size_t n){
 if(n>R19_POPUP_STATE_BYTES-popup_now_n||(!p&&n))return 0;
 if(n)memcpy(popup_now+popup_now_n,p,n);popup_now_n+=n;return 1;
}
static int popup_component_state(Client*c,int id,int depth){
 if(depth>64||++popup_nodes>4096||id<0||id>=_Component.count)return 0;
 Component*p=_Component.instances[id];if(!p)return 0;
 if(p->clientCode>0)client_update_interface_content(c,p);
 if(p->type==0){if(p->scrollPosition>p->scroll-p->height)p->scrollPosition=p->scroll-p->height;if(p->scrollPosition<0)p->scrollPosition=0;}
 if(popup_trace_n<4096){popup_trace_start[popup_trace_n]=popup_now_n;popup_trace_id[popup_trace_n++]=id;}
 Component copy;memcpy(&copy,p,sizeof copy);copy.seqCycle=0;
 if(!popup_state_add(&copy,sizeof copy))return 0;
 if(p->activeText){size_t n=0;while(n<8192&&p->activeText[n])n++;if(n==8192||!popup_state_add(p->activeText,n+1))return 0;}
 if(p->scriptCount<0||p->scriptCount>128)return 0;
 for(int i=0;i<p->scriptCount;i++){int value=client_execute_clientscript1(c,p,i);if(!popup_state_add(&value,sizeof value))return 0;}
 if(p->type==2||p->type==7){
  if(p->width<0||p->height<0||(uint64_t)p->width*p->height>32768)return 0;
  size_t n=(size_t)p->width*p->height*sizeof(int);
  if(!popup_state_add(p->invSlotObjId,n)||!popup_state_add(p->invSlotObjCount,n))return 0;
 }
 if(p->type==0){
  if(p->hide&&c->viewportHoveredInterfaceIndex!=p->id&&c->sidebarHoveredInterfaceIndex!=p->id&&c->chatHoveredInterfaceIndex!=p->id)return 1;
  if(p->childCount<0||p->childCount>4096)return 0;
  size_t n=(size_t)p->childCount*sizeof(int);
  if(!popup_state_add(p->childId,n)||!popup_state_add(p->childX,n)||!popup_state_add(p->childY,n))return 0;
  for(int i=0;i<p->childCount;i++)if(!popup_component_state(c,p->childId[i],depth+1))return 0;
 }
 return 1;
}
static int popup_state_build(Client*c){
 popup_now_n=0;popup_nodes=0;popup_trace_n=0;
 int v[]={c->viewport_interface_id,c->chat_interface_id,c->sticky_chat_interface_id,c->sidebar_interface_id,c->selected_tab,
 c->viewportHoveredInterfaceIndex,c->sidebarHoveredInterfaceIndex,c->chatHoveredInterfaceIndex,
 c->obj_selected,c->objSelectedSlot,c->objSelectedInterface,c->selected_area,c->selectedItem,c->selectedInterface,
 c->pressed_continue_option,c->vita_dialogue_selected,c->vita_dialogue_controller,c->vita_skill_selected,c->vita_skill_controller,
 c->shell->mouse_button,c->shell->mouse_click_button,_Custom.item_outlines,c->chatback_input_open};
 if(!popup_state_add(v,sizeof v))return 0;
 if(c->chatback_input_open&&!popup_state_add(c->chatback_input,strlen(c->chatback_input)+1))return 0;
 int roots[]={c->viewport_interface_id,c->chat_interface_id,c->sticky_chat_interface_id,
 c->sidebar_interface_id>=0?c->sidebar_interface_id:(c->selected_tab>=0&&c->selected_tab<14?c->tab_interface_id[c->selected_tab]:-1)};
 for(unsigned i=0;i<sizeof roots/sizeof roots[0];i++)if(roots[i]>=0&&!popup_component_state(c,roots[i],0))return 0;
 return 1;
}
