#include <stdio.h>
#include <stdlib.h>

#include "allocator.h"
#include "animframe.h"
#include "r104_npc_alpha.h"
#include "datastruct/lrucache.h"
#include "jagfile.h"
#include "model.h"
#include "r57_body_layers.h"
#include "npctype.h"
#include "packet.h"
#include "platform.h"

NpcTypeData _NpcType = {0};

static void npctype_decode(NpcType *npc, Packet *dat);

static NpcType *npctype_new(void) {
    NpcType *npc = calloc(1, sizeof(NpcType));
    npc->index = -1L;
    npc->size = 1;
    npc->readyanim = -1;
    npc->walkanim = -1;
    npc->walkanim_b = -1;
    npc->walkanim_r = -1;
    npc->walkanim_l = -1;
    npc->animHasAlpha = false;
    npc->resizex = -1;
    npc->resizey = -1;
    npc->resizez = -1;
    npc->minimap = true;
    npc->vislevel = -1;
    npc->resizeh = 128;
    npc->resizev = 128;
    return npc;
}

void npctype_unpack(Jagfile *config) {
    _NpcType.dat = jagfile_to_packet(config, "npc.dat");
    Packet *idx = jagfile_to_packet(config, "npc.idx");

    _NpcType.count = g2(idx);
    _NpcType.offsets = calloc(_NpcType.count, sizeof(int));

    int offset = 2;
    for (int id = 0; id < _NpcType.count; id++) {
        _NpcType.offsets[id] = offset;
        offset += g2(idx);
    }

    _NpcType.cache = calloc(_NpcType.count, sizeof(NpcType *));
    _NpcType.modelCache = lrucache_new(30);

    packet_free(idx);
}

void npctype_free_global(void) {
    lrucache_free(_NpcType.modelCache);
    free(_NpcType.offsets);
    for (int i = 0; i < _NpcType.count; i++) {
        NpcType *npc = _NpcType.cache[i];
        if (npc) {
            free(npc->name);
            free(npc->desc);
            free(npc->models);
            free(npc->heads);
            free(npc->recol_s);
            free(npc->recol_d);
            if (npc->op) {
                for (int j = 0; j < 5; j++) {
                    free(npc->op[j]);
                }
                free(npc->op);
            }
            free(npc);
        }
    }
    free(_NpcType.cache);
    packet_free(_NpcType.dat);
}

NpcType *npctype_get(int id) {
    if (id < 0 || id >= _NpcType.count) {
        return NULL;
    }

    NpcType *npc = _NpcType.cache[id];
    if (!npc) {
        npc = _NpcType.cache[id] = npctype_new();
        _NpcType.dat->pos = _NpcType.offsets[id];
        npc->index = id;
        npctype_decode(npc, _NpcType.dat);
    }
    return npc;
}

static void npctype_decode(NpcType *npc, Packet *dat) {
    while (true) {
        int code = g1(dat);
        if (code == 0) {
            return;
        }

        if (code == 1) {
            int count = g1(dat);
            npc->models_count = count;
            npc->models = calloc(count, sizeof(int));

            for (int i = 0; i < count; i++) {
                npc->models[i] = g2(dat);
            }
        } else if (code == 2) {
            npc->name = gjstr(dat);
        } else if (code == 3) {
            npc->desc = gjstr(dat);
        } else if (code == 12) {
            npc->size = g1b(dat);
        } else if (code == 13) {
            npc->readyanim = g2(dat);
        } else if (code == 14) {
            npc->walkanim = g2(dat);
        } else if (code == 16) {
            npc->animHasAlpha = true;
        } else if (code == 17) {
            npc->walkanim = g2(dat);
            npc->walkanim_b = g2(dat);
            npc->walkanim_r = g2(dat);
            npc->walkanim_l = g2(dat);
        } else if (code >= 30 && code < 40) {
            if (!npc->op) {
                npc->op = calloc(5, sizeof(char *));
            }

            npc->op[code - 30] = gjstr(dat);
            if (platform_strcasecmp(npc->op[code - 30], "hidden") == 0) {
                free(npc->op[code - 30]);
                npc->op[code - 30] = NULL;
            }
        } else if (code == 40) {
            int count = g1(dat);
            npc->recol_count = count;
            npc->recol_s = calloc(count, sizeof(int));
            npc->recol_d = calloc(count, sizeof(int));

            for (int i = 0; i < count; i++) {
                npc->recol_s[i] = g2(dat);
                npc->recol_d[i] = g2(dat);
            }
        } else if (code == 60) {
            int count = g1(dat);
            npc->heads_count = count;
            npc->heads = calloc(count, sizeof(int));

            for (int i = 0; i < count; i++) {
                npc->heads[i] = g2(dat);
            }
        } else if (code == 90) {
            // unused
            npc->resizex = g2(dat);
        } else if (code == 91) {
            // unused
            npc->resizey = g2(dat);
        } else if (code == 92) {
            // unused
            npc->resizez = g2(dat);
        } else if (code == 93) {
            npc->minimap = false;
        } else if (code == 95) {
            npc->vislevel = g2(dat);
        } else if (code == 97) {
            npc->resizeh = g2(dat);
        } else if (code == 99) {
            npc->alwaysontop = true;
        } else if (code == 100) {
            npc->ambient = (int8_t)g1(dat);
        } else if (code == 101) {
            npc->contrast = (int8_t)g1(dat) * 5;
        } else if (code == 98) {
            npc->resizev = g2(dat);
        } else {
            rs2_error("Error unrecognised npc config code: %d\n", code);
        }
    }
}

Model *npctype_get_sequencedmodel(NpcType *npc, int primaryTransformId, int secondaryTransformId, int *seqMask) {
    Model *tmp = NULL;
    Model *model = (Model *)lrucache_get(_NpcType.modelCache, npc->index);

    if (!model) {
        Model **models = calloc(npc->models_count, sizeof(Model *));
        for (int i = 0; i < npc->models_count; i++) {
            models[i] = model_from_id(npc->models[i], npc->models_count == 1);
        }

        if (npc->models_count == 1) {
            model = models[0];
        } else {
            model = model_from_models(models, npc->models_count, true);
            for (int i = 0; i < npc->models_count; i++) {
                model_free(models[i]);
            }
        }
        free(models);

        if (npc->recol_s) {
            for (int i = 0; i < npc->recol_count; i++) {
                model_recolor(model, npc->recol_s[i], npc->recol_d[i]);
            }
        }

        model_create_label_references(model, true);
        model_calculate_normals(model, npc->ambient + 64, npc->contrast + 850, -30, -50, -30, true, true);
        r57_prepare_body_layers(model);
        lrucache_put(_NpcType.modelCache, npc->index, &model->link);
    }

    /* Imported animations can contain alpha transforms without opcode16.
     * Promote ownership before copying; entity_draw_free uses the same flag.
     * Once promoted, every pose starts from the immutable cached alpha. */
    if (r104_frame_changes_alpha(primaryTransformId) || r104_frame_changes_alpha(secondaryTransformId))
        npc->animHasAlpha = true;
    tmp = model_share_alpha(model, !npc->animHasAlpha);

    if (primaryTransformId != -1 && secondaryTransformId != -1) {
        model_apply_transforms(tmp, primaryTransformId, secondaryTransformId, seqMask);
    } else if (primaryTransformId != -1) {
        model_apply_transform(tmp, primaryTransformId);
    }

    if (npc->resizeh != 128 || npc->resizev != 128) {
        model_scale(tmp, npc->resizeh, npc->resizev, npc->resizeh);
    }

    model_calculate_bounds_cylinder(tmp);
    tmp->label_faces = NULL;
    tmp->label_vertices = NULL;

    if (npc->size == 1) {
        tmp->pickable = true;
    }

    return tmp;
}

Model *npctype_get_headmodel(NpcType *npc) {
    if (!npc->heads) {
        return NULL;
    }

    Model **models = calloc(npc->heads_count, sizeof(Model *));
    for (int i = 0; i < npc->heads_count; i++) {
        models[i] = model_from_id(npc->heads[i], false);
    }

    Model *model;
    if (npc->heads_count == 1) {
        model = models[0];
    } else {
        model = model_from_models(models, npc->heads_count, false);
    }
    free(models);

    if (npc->recol_s) {
        for (int i = 0; i < npc->recol_count; i++) {
            model_recolor(model, npc->recol_s[i], npc->recol_d[i]);
        }
    }

    return model;
}
