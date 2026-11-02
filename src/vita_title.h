#pragma once
#include "client.h"
void vita_title_draw_panel(Client *c,bool loading,const char *message,int progress);

void vita_title_invalidate(void);
unsigned vita_title_generation(void);
