#pragma once
#include "client.h"
#include "gameshell.h"
bool vita_dialogue_input(Client *c, bool blocked, bool confirm, int direction, bool pointer_activity);
int vita_dialogue_hover(Client *c, int component);
void client_vita_dialogue_action(Client *c, int component, int action);

const char *vita_dialogue_prompt(Client *c, int component, const char *text);
int vita_interface_text_colour(Client *c, int component, int existing_colour);

void vita_dialogue_layout(bool enhanced);
