#pragma once
#include "model.h"
#include <stdint.h>
/* Shared immutable bind-pose candidates. Pose copies retain this allocation;
 * each emitted pose checks its own geometry before replaying any face. */
typedef struct { uint16_t high, low; } R57Pair;
typedef struct R57Layers { unsigned refs, count; struct R57Layers *prev,*next; uint64_t backing_textures; R57Pair pairs[]; } R57Layers;
void r57_prepare_body_layers(Model *m);
void r57_prepare_fixture_layers(Model *m,uint64_t backing_textures);
void r57_merge_body_layers(Model *m, Model **parts, int count);
R57Layers *r57_retain_body_layers(R57Layers *layers);
void r57_release_body_layers(R57Layers *layers);
void r57_reset_body_layers(void);
int r57_select_body_layers(Model *m, unsigned char *selected, int capacity);

void r77_body_cache_status(unsigned out[3]);
