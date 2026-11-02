#pragma once
extern AnimFrameData _AnimFrame;
static bool r104_frame_changes_alpha(int id) {
    if(id<0 || id>=_AnimFrame.count || !_AnimFrame.instances[id])return false;
    const AnimFrame*f=_AnimFrame.instances[id];
    if(!f->base)return false;
    for(int i=0;i<f->length;i++) {
        int group=f->groups[i];
        if(group>=0 && group<f->base->length && f->base->types[group]==OP_ALPHA)return true;
    }
    return false;
}
