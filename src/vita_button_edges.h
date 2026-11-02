#pragma once
#include <stdint.h>
typedef struct { uint64_t stamp; unsigned down; int ready; } VitaButtonEdges;
static inline int vita_button_edge(VitaButtonEdges *s,uint64_t stamp,unsigned buttons,unsigned mask) {
    if(s->ready&&stamp<=s->stamp)return 0;
    unsigned down=buttons&mask;
    int edge=s->ready&&down&&!s->down;
    s->ready=1;s->stamp=stamp;s->down=down;
    return edge;
}
