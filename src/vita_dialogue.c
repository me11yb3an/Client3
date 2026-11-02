#include "title_theme.h"
#include "ui474.h"

#include <string.h>
#include "vita_dialogue.h"
#include "component.h"
extern ComponentData _Component;
#define DIALOGUE_LIMIT 64
static Component *component(int id) {return id>=0 && id<_Component.count ? _Component.instances[id] : NULL;}
typedef struct {int choices[DIALOGUE_LIMIT],choiceY[DIALOGUE_LIMIT],choiceX[DIALOGUE_LIMIT],count,continuation,close,other;unsigned int signature;int skillHover;bool invalid,skillGuide,skillCategories;} Dialogue;
static void inspect(Component *p, Dialogue *d, int depth,int x,int y,int left,int top,int right,int bottom) {
    if(depth>16){d->invalid=true;return;}
    if(!p || p->hide || p->width<=0 || p->height<=0 || x>=right || y>=bottom || x+p->width<=left || y+p->height<=top) return;
    if(p->type==0) {
        if(!p->childId) return;
        int l=x>left?x:left,t=y>top?y:top,r=x+p->width<right?x+p->width:right,b=y+p->height<bottom?y+p->height:bottom;
        for(int i=0;i<p->childCount;i++){Component*a=component(p->childId[i]);if(a)inspect(a,d,depth+1,x+(p->childX?p->childX[i]:0)+a->x,y+(p->childY?p->childY[i]:0)+a->y-p->scrollPosition,l,t,r,b);}
        return;
    }
    // Only the verified Skill Guide policy permits its read-only item displays
    // and category links. Those links keep their original mouse/touch dispatch.
    if(d->skillGuide){
        if(p->type==TYPE_INV && !p->buttonType && !p->clientCode && !p->scriptCount && !p->comparatorCount && !p->interactable && !p->draggable && !p->usable)return;
        // Derive category actions from the live, clipped Skill Guide tree.
        // Do not infer siblings/positions from a category-ID list.
        if(p->type==TYPE_TEXT && p->buttonType==BUTTON_OK){
            if(d->skillCategories && !p->clientCode && !p->scriptCount && !p->comparatorCount && p->text[0] && p->overColour && p->option[0]){
                if(d->count>=DIALOGUE_LIMIT-1){d->invalid=true;return;}
                if(!d->skillHover)d->skillHover=p->overColour;
                if(d->skillHover!=p->overColour){d->invalid=true;return;}
                int at=d->count++;
                while(at>0 && (d->choiceY[at-1]>y || (d->choiceY[at-1]==y && d->choiceX[at-1]>x))){d->choices[at]=d->choices[at-1];d->choiceY[at]=d->choiceY[at-1];d->choiceX[at]=d->choiceX[at-1];at--;}
                d->choices[at]=p->id;d->choiceY[at]=y;d->choiceX[at]=x;
            }
            return;
        }
    }
    if(p->type==2 || (p->buttonType!=0 && p->buttonType!=BUTTON_CLOSE && p->buttonType!=BUTTON_CONTINUE && p->buttonType!=BUTTON_OK))d->other++;
    if(p->buttonType!=0 && p->type!=4)d->other++;
    if(p->type!=4 || p->width<=0 || p->height<=0 || !p->text[0]) return;
    d->signature=(d->signature^(unsigned)p->id)*16777619u;
    for(const unsigned char *t=(const unsigned char*)p->text;*t;t++)d->signature=(d->signature^*t)*16777619u;
    int action=p->buttonType;
    if(action==BUTTON_CLOSE){if(p->clientCode || p->scriptCount || p->comparatorCount || strcmp(p->text,"Close Window") || p->overColour!=0xffffff){d->other++;return;}if(d->close>=0)d->invalid=true;d->close=p->id;return;}
    if(action!=BUTTON_OK && action!=BUTTON_CONTINUE) return;
    // Conditional/client-code buttons are not ordinary dialogue responses.
    if(p->clientCode || p->scriptCount || p->comparatorCount){d->other++;return;}
    if(action==BUTTON_CONTINUE) {if(d->continuation>=0)d->invalid=true;d->continuation=p->id;}
    else if(p->overColour && p->option[0]) {
        if(d->count>=DIALOGUE_LIMIT){d->invalid=true;return;}
        int i=d->count++;while(i>0 && (d->choiceY[i-1]>y || (d->choiceY[i-1]==y && d->choiceX[i-1]>x))){d->choices[i]=d->choices[i-1];d->choiceY[i]=d->choiceY[i-1];d->choiceX[i]=d->choiceX[i-1];i--;}d->choices[i]=p->id;d->choiceY[i]=y;d->choiceX[i]=x;
    } else {d->other++;return;}
}
static int dialogue_root(Client*c){return c->chat_interface_id>=0?c->chat_interface_id:c->viewport_interface_id>=0?c->viewport_interface_id:c->sticky_chat_interface_id;}
static Dialogue resolve(Client *c) {
    Dialogue d={.continuation=-1,.close=-1,.signature=2166136261u};
    if(!c->ingame || c->show_social_input || c->chatback_input_open || c->modal_message[0]) return d;
    Component*root=component(dialogue_root(c));if(root)inspect(root,&d,0,0,0,0,0,root->width,root->height);
    // Mixed continue/response interfaces are unsupported, never guessed.
    if(d.invalid || (d.count && d.continuation>=0) || (c->chat_interface_id<0&&(d.count||d.other))){d.count=0;d.continuation=-1;}
    d.signature=(d.signature^(unsigned)d.count^(unsigned)(d.continuation+1))*16777619u;
    for(int i=0;i<d.count;i++)d.signature=(d.signature^(unsigned)d.choices[i])*16777619u;
    return d;
}
static Dialogue resolve_skill(Client*c){
    Dialogue d={.continuation=-1,.close=-1,.skillGuide=true,.skillCategories=true,.signature=2166136261u};
    if(!c->ingame || c->menu_visible || c->vita_interface_blocked || c->show_social_input || c->chatback_input_open || c->modal_message[0] || c->chat_interface_id>=0 || c->sidebar_interface_id>=0 || c->viewport_interface_id!=7386)return d;
    Component*root=component(7386);if(root)inspect(root,&d,0,0,0,0,0,root->width,root->height);
    // Close is the existing leaf, appended after the visible category actions.
    // A guide without categories uses the already-tested informational close policy.
    if(d.invalid || d.other || d.continuation>=0 || !d.count || d.close<0){d.count=0;return d;}
    d.choices[d.count]=d.close;d.choiceX[d.count]=d.choiceY[d.count]=0;d.count++;
    d.signature=2166136261u;Component*name=component(7387);
    if(name)for(const unsigned char*t=(unsigned char*)name->text;*t;t++)d.signature=(d.signature^*t)*16777619u;
    for(int i=0;i<d.count;i++){
        Component*p=component(d.choices[i]);
        d.signature=(d.signature^(unsigned)p->id^(unsigned)p->buttonType^(unsigned)p->width^(unsigned)p->height)*16777619u;
        d.signature=(d.signature^(unsigned)d.choiceX[i]^(unsigned)d.choiceY[i])*16777619u;
        for(const unsigned char*t=(unsigned char*)p->text;*t;t++)d.signature=(d.signature^*t)*16777619u;
    }
    return d;
}
static void clear_skill(Client*c){c->vita_skill_selected=-1;c->vita_skill_signature=0;c->vita_skill_controller=false;c->vita_skill_waiting=false;}
static bool skill_input(Client*c,Dialogue*d,bool confirm,int direction,bool pointer){
    bool changed=c->vita_skill_signature!=d->signature || c->vita_skill_seen_generation!=c->vita_skill_generation;
    bool moved=pointer || (!changed && (c->shell->mouse_x!=c->vita_skill_mouse_x || c->shell->mouse_y!=c->vita_skill_mouse_y));
    int index=-1;
    for(int i=0;i<d->count;i++)if(d->choices[i]==c->vita_skill_selected)index=i;
    if(changed || (c->vita_skill_controller && index<0)){
        c->vita_skill_signature=d->signature;c->vita_skill_seen_generation=c->vita_skill_generation;
        c->vita_skill_selected=d->choices[0];c->vita_skill_controller=true;c->vita_skill_waiting=false;
    }
    if(moved){c->vita_skill_controller=false;c->vita_skill_selected=-1;}
    c->vita_skill_mouse_x=c->shell->mouse_x;c->vita_skill_mouse_y=c->shell->mouse_y;
    if(!c->vita_skill_controller && !moved){
        c->vita_skill_selected=-1;
        for(int i=0;i<d->count;i++)if(c->viewportHoveredInterfaceIndex==d->choices[i])c->vita_skill_selected=d->choices[i];
    }
    if(direction){
        index=-1;for(int i=0;i<d->count;i++)if(d->choices[i]==c->vita_skill_selected)index=i;
        if(index<0)index=direction>0?-1:0;
        index=(index+(direction>0?1:-1)+d->count)%d->count;
        c->vita_skill_selected=d->choices[index];c->vita_skill_controller=true;
    }
    if(confirm){
        // Await a new page/snapshot before another category confirmation. Physical
        // edge polling still prevents a held Cross from acting on that new page.
        if(c->vita_skill_waiting)return true;
        if(!moved || direction)for(int i=0;i<d->count;i++)if(d->choices[i]==c->vita_skill_selected){
            int id=d->choices[i];
            if(id==d->close){client_vita_dialogue_action(c,id,947);clear_skill(c);}
            else {c->vita_skill_waiting=true;client_vita_dialogue_action(c,id,951);}
            break;
        }
        return true;
    }
    return false;
}
// Verified cached Build225 informational roots, cross-checked with interface.pack.
// Read-only main panels only. Unknown roots are not inferred from their text.
static int resolve_close(Client *c){
    if(!c->ingame || c->menu_visible || c->vita_interface_blocked || c->chat_interface_id>=0 || c->sidebar_interface_id>=0 || c->show_social_input || c->chatback_input_open || c->modal_message[0])return -1;
    if(resolve_skill(c).count)return -1;
    static const int informational[]={7386,7558,5993,6073,1136,3023,3702,5392,297,1689,2302,6987,6965,6994,5596};
    bool allowed=false;for(unsigned i=0;i<sizeof(informational)/sizeof(informational[0]);i++)if(c->viewport_interface_id==informational[i])allowed=true;
    if(!allowed)return -1;
    Dialogue d={.continuation=-1,.close=-1,.signature=2166136261u,.skillGuide=c->viewport_interface_id==7386};Component*root=component(c->viewport_interface_id);
    if(root)inspect(root,&d,0,0,0,0,0,root->width,root->height);
    return !d.invalid && !d.count && d.continuation<0 && !d.other?d.close:-1;
}
static void clear(Client *c){if(c->vita_dialogue_controller)c->redraw_chatback=true;c->vita_dialogue_root=-1;c->vita_dialogue_selected=-1;c->vita_dialogue_signature=0;c->vita_dialogue_controller=false;}
bool vita_dialogue_input(Client *c,bool blocked,bool confirm,int direction,bool pointer_activity) {
    if(c->vita_interface_enabled&&c->ingame&&!blocked&&!c->show_social_input&&!c->chatback_input_open&&c->modal_message[0]&&confirm){
        c->modal_message[0]=0;c->redraw_chatback=true;return true;
    }
    if(!c->ingame){clear(c);clear_skill(c);return title_theme_controller(c,blocked,confirm,direction,pointer_activity);}
    c->vita_interface_enabled=true;c->vita_interface_blocked=blocked;
    if(blocked){clear(c);clear_skill(c);return false;}
    Dialogue d=resolve(c);
    if(!d.count && d.continuation<0){clear(c);Dialogue skill=resolve_skill(c);if(skill.count)return skill_input(c,&skill,confirm,direction,pointer_activity);clear_skill(c);int close=resolve_close(c);if(confirm&&close>=0){client_vita_dialogue_action(c,close,947);return true;}return ui474_controller(c,confirm,direction,pointer_activity);}
    clear_skill(c);
    bool changed=c->vita_dialogue_root!=dialogue_root(c) || c->vita_dialogue_signature!=d.signature || c->vita_dialogue_seen_generation!=c->vita_dialogue_generation;
    bool moved=pointer_activity || (!changed && (c->vita_dialogue_mouse_x!=c->shell->mouse_x || c->vita_dialogue_mouse_y!=c->shell->mouse_y));
    if(changed){c->vita_dialogue_seen_generation=c->vita_dialogue_generation;c->vita_dialogue_root=dialogue_root(c);c->vita_dialogue_signature=d.signature;c->vita_dialogue_selected=d.count?d.choices[0]:d.continuation;c->vita_dialogue_controller=true;c->redraw_chatback=true;}
    if(moved){c->vita_dialogue_controller=false;c->vita_dialogue_selected=-1;c->redraw_chatback=true;}
    c->vita_dialogue_mouse_x=c->shell->mouse_x;c->vita_dialogue_mouse_y=c->shell->mouse_y;
    if(!c->vita_dialogue_controller && !moved){c->vita_dialogue_selected=-1;for(int i=0;i<d.count;i++)if(c->chatHoveredInterfaceIndex==d.choices[i])c->vita_dialogue_selected=d.choices[i];}
    if(direction && d.count){int index=-1;for(int i=0;i<d.count;i++)if(d.choices[i]==c->vita_dialogue_selected)index=i;if(index<0)index=direction>0?-1:0;index=(index+direction+d.count)%d.count;c->vita_dialogue_selected=d.choices[index];c->vita_dialogue_controller=true;c->redraw_chatback=true;}
    if(confirm){
        if(d.continuation>=0){if(!c->pressed_continue_option)client_vita_dialogue_action(c,d.continuation,44);}
        else if(c->vita_dialogue_selected>=0 && (!moved || direction)){for(int i=0;i<d.count;i++)if(d.choices[i]==c->vita_dialogue_selected){client_vita_dialogue_action(c,c->vita_dialogue_selected,951);break;}}
        // Consumed even if waiting or mouse just moved: no fallback virtual click.
        return true;
    }
    return false;
}
int vita_dialogue_hover(Client *c,int id){
    if(!c->vita_dialogue_controller || c->vita_dialogue_root!=dialogue_root(c) || c->menu_visible)return -1;
    Dialogue d=resolve(c);if(d.signature!=c->vita_dialogue_signature || c->vita_dialogue_seen_generation!=c->vita_dialogue_generation)return -1;
    for(int i=0;i<d.count;i++)if(d.choices[i]==id)return id==c->vita_dialogue_selected?1:0;
    return -1;
}

const char *vita_dialogue_prompt(Client*c,int id,const char*text){
    if(!c->vita_interface_enabled)return text;
    Dialogue d=resolve(c);if(d.continuation==id && !strcmp(text,"Click here to continue"))return "Press X to continue";
    return text;
}
int vita_interface_text_colour(Client*c,int id,int existing){
    if(!c->vita_interface_enabled || c->menu_visible || c->vita_interface_blocked)return existing;
    Dialogue d=resolve(c);Component*p=component(id);
    // One existing YELLOW definition for both controller dialogue interactions.
    // Pointer-owned hover and all component source colours remain unchanged.
    Dialogue skill=resolve_skill(c);
    if(c->vita_skill_controller && c->viewport_interface_id==7386 && p && p->layer==c->viewport_interface_id && p->type==TYPE_TEXT && (p->buttonType==BUTTON_OK || p->buttonType==BUTTON_CLOSE)){
        if(skill.count && c->vita_skill_signature==skill.signature && c->vita_skill_seen_generation==c->vita_skill_generation)
            for(int i=0;i<skill.count;i++)if(skill.choices[i]==id)return id==c->vita_skill_selected?(id==skill.close?skill.skillHover:p->overColour):p->colour;
        // Suppress stale pointer hover on disabled/removed actions or during a
        // page update. It must not compete with controller-owned selection.
        return p->colour;
    }
    if(p && vita_dialogue_hover(c,id)==1)return YELLOW;
    if(p && d.continuation==id && !c->pressed_continue_option && c->vita_dialogue_controller && !strcmp(p->text,"Click here to continue") && p->overColour)return YELLOW;
    if(resolve_close(c)==id && p)return p->overColour;
    return existing;
}

#include "dialogue_geometry.generated.h"
void vita_dialogue_layout(bool enhanced){
    static int previous=-1;if(!_Component.instances||previous==(int)enhanced)return;
    previous=(int)enhanced;
    for(unsigned i=0;i<sizeof(dialogue_widths)/sizeof(dialogue_widths[0]);i++){
        int id=dialogue_widths[i].id;Component *p=id>=0&&id<_Component.count?_Component.instances[id]:NULL;
        if(p&&p->type==dialogue_widths[i].type)p->width=enhanced?dialogue_widths[i].compact:dialogue_widths[i].wide;
    }
    for(unsigned i=0;i<sizeof(dialogue_positions)/sizeof(dialogue_positions[0]);i++){
        int id=dialogue_positions[i].root,j=dialogue_positions[i].index;Component *p=id>=0&&id<_Component.count?_Component.instances[id]:NULL;
        if(p&&p->childX&&j<p->childCount)p->childX[j]=enhanced?dialogue_positions[i].compact:dialogue_positions[i].wide;
    }
}
