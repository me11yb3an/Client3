#pragma once
#include "client.h"
int title_theme_profile(void);
unsigned title_theme_revision(void);
bool title_theme_set_profile(int next);
void title_theme_draw(Client*c);
bool title_theme_pointer(Client*c);
bool title_theme_controller(Client*c,bool blocked,bool confirm,int direction,bool pointer_activity);
void title_theme_review_draw(Client*c,int profile,bool unavailable);
