#include <stdio.h>
#include <stdlib.h>

#include "loctype.h"
#include "model.h"
#include "r99_maze_caps.h"
#include "platform.h"
#include "r57_body_layers.h"
#include "r66_scenery_visuals.h"
#include "r87_vendor_layers.h"
#include "r90_sewing_table.h"

LocTypeData _LocType = {0};

static void loctype_decode(LocType *loc, Packet *dat);

static LocType *loctype_new(void) {
    LocType *loc = calloc(1, sizeof(LocType));
    loc->index = -1;
    return loc;
}

void loctype_unpack(Jagfile *config) {
    _LocType.modelCacheStatic = lrucache_new(500);
    // TzHaar compatibility: retain animated states rather than evicting into the non-reclaiming scene pool.
    // The old150-state cache exhausted32MiB;1024states stabilized the connected city at26,031,720bytes.
    _LocType.modelCacheDynamic = lrucache_new(1024); // TODO if dynamic cache size isn't big enough it'll leak in allocator

    _LocType.dat = jagfile_to_packet(config, "loc.dat");
    Packet *idx = jagfile_to_packet(config, "loc.idx");

    _LocType.count = g2(idx);
    _LocType.offsets = calloc(_LocType.count, sizeof(int));

    int offset = 2;
    for (int id = 0; id < _LocType.count; id++) {
        _LocType.offsets[id] = offset;
        offset += g2(idx);
    }

    _LocType.cache = calloc(10, sizeof(LocType *));
    for (int id = 0; id < 10; id++) {
        _LocType.cache[id] = loctype_new();
    }

    packet_free(idx);
}

void loctype_free_global(void) {
    lrucache_free(_LocType.modelCacheStatic);
    lrucache_free(_LocType.modelCacheDynamic);
    free(_LocType.offsets);
    for (int i = 0; i < 10; i++) {
        loctype_reset(_LocType.cache[i]);
        free(_LocType.cache[i]);
    }
    free(_LocType.cache);
    packet_free(_LocType.dat);
}

LocType *loctype_get(int id) {
    for (int i = 0; i < 10; i++) {
        if (_LocType.cache[i]->index == id) {
            return _LocType.cache[i];
        }
    }

    _LocType.cachePos = (_LocType.cachePos + 1) % 10;
    LocType *loc = _LocType.cache[_LocType.cachePos];
    _LocType.dat->pos = _LocType.offsets[id];
    loc->index = id;
    loctype_reset(loc);
    loctype_decode(loc, _LocType.dat);
    return loc;
}

void loctype_reset(LocType *loc) {
    free(loc->models);
    free(loc->shapes);
    free(loc->name);
    free(loc->desc);
    free(loc->recol_s);
    free(loc->recol_d);
    if (loc->op) {
        for (int i = 0; i < 5; i++) {
            free(loc->op[i]);
        }
        free(loc->op);
    }
    loc->models = NULL;
    loc->shapes = NULL;
    loc->name = NULL;
    loc->desc = NULL;
    loc->recol_s = NULL;
    loc->recol_d = NULL;
    loc->width = 1;
    loc->length = 1;
    loc->blockwalk = true;
    loc->blockrange = true;
    loc->active = false;
    loc->hillskew = false;
    loc->sharelight = false;
    loc->occlude = false;
    loc->anim = -1;
    loc->wallwidth = 16;
    loc->ambient = 0;
    loc->contrast = 0;
    loc->op = NULL;
    loc->animHasAlpha = false;
    loc->mapfunction = -1;
    loc->mapscene = -1;
    loc->mirror = false;
    loc->shadow = true;
    loc->resizex = 128;
    loc->resizey = 128;
    loc->resizez = 128;
    loc->forceapproach = 0;
    loc->offsetx = 0;
    loc->offsety = 0;
    loc->offsetz = 0;
    loc->forcedecor = false;
    loc->raiseobject = -1;
}

static void loctype_decode(LocType *loc, Packet *dat) {
    int active = -1;

    while (true) {
        int code = g1(dat);
        if (code == 0) {
            break;
        }

        if (code == 1) {
            int count = g1(dat);
            loc->shapes_and_models_count = count;
            loc->shapes = calloc(count, sizeof(int));
            loc->models = calloc(count, sizeof(int));

            for (int i = 0; i < count; i++) {
                loc->models[i] = g2(dat);
                loc->shapes[i] = g1(dat);
            }
        } else if (code == 2) {
            loc->name = gjstr(dat);
        } else if (code == 3) {
            loc->desc = gjstr(dat);
        } else if (code == 14) {
            loc->width = g1(dat);
        } else if (code == 15) {
            loc->length = g1(dat);
        } else if (code == 17) {
            loc->blockwalk = false;
        } else if (code == 18) {
            loc->blockrange = false;
        } else if (code == 19) {
            active = g1(dat);

            if (active == 1) {
                loc->active = true;
            }
        } else if (code == 21) {
            loc->hillskew = true;
        } else if (code == 22) {
            loc->sharelight = true;
        } else if (code == 23) {
            loc->occlude = true;
        } else if (code == 24) {
            loc->anim = g2(dat);
            if (loc->anim == 65535) {
                loc->anim = -1;
            }
        } else if (code == 25) {
            loc->animHasAlpha = true;
        } else if (code == 28) {
            loc->wallwidth = g1(dat);
        } else if (code == 29) {
            loc->ambient = g1b(dat);
        } else if (code == 39) {
            loc->contrast = g1b(dat);
        } else if (code >= 30 && code < 39) {
            if (!loc->op) {
                loc->op = calloc(5, sizeof(char *));
            }

            loc->op[code - 30] = gjstr(dat);
            if (platform_strcasecmp(loc->op[code - 30], "hidden") == 0) {
                free(loc->op[code - 30]);
                loc->op[code - 30] = NULL;
            }
        } else if (code == 40) {
            int count = g1(dat);
            loc->recol_count = count;
            loc->recol_s = calloc(count, sizeof(int));
            loc->recol_d = calloc(count, sizeof(int));

            for (int i = 0; i < count; i++) {
                loc->recol_s[i] = g2(dat);
                loc->recol_d[i] = g2(dat);
            }
        } else if (code == 60) {
            loc->mapfunction = g2(dat);
        } else if (code == 62) {
            loc->mirror = true;
        } else if (code == 64) {
            loc->shadow = false;
        } else if (code == 65) {
            loc->resizex = g2(dat);
        } else if (code == 66) {
            loc->resizey = g2(dat);
        } else if (code == 67) {
            loc->resizez = g2(dat);
        } else if (code == 68) {
            loc->mapscene = g2(dat);
        } else if (code == 69) {
            loc->forceapproach = g1(dat);
        } else if (code == 70) {
            loc->offsetx = g2b(dat);
        } else if (code == 71) {
            loc->offsety = g2b(dat);
        } else if (code == 72) {
            loc->offsetz = g2b(dat);
        } else if (code == 73) {
            loc->forcedecor = true;
        } else if (code == 75) {
            loc->raiseobject = g1(dat);
        } else {
            rs2_error("Error unrecognised loc config code: %d\n", code);
        }
    }

    if (!loc->shapes) {
        loc->shapes = calloc(1, sizeof(int)); // new int[0];
    }

    if (active == -1) {
        loc->active = loc->shapes_and_models_count > 0 && loc->shapes[0] == 10;

        if (loc->op) {
            loc->active = true;
        }
    }
}

#include "r21_location_visual.h"
static int visual_scene_x,visual_scene_z;
void loctype_set_scene_origin(int x,int z){visual_scene_x=x;visual_scene_z=z;}
bool loctype_visual_hidden_for_scene(LocType *loc,int level,int x,int z,int shape,int rotation){
#if defined(__vita__) && defined(BUILD_A_EMBEDDED)
    return r66_hidden_decoration(loc->index,level,visual_scene_x+x,visual_scene_z+z,shape,rotation)!=0;
#else
    (void)loc;(void)level;(void)x;(void)z;(void)shape;(void)rotation;return false;
#endif
}
LocType *loctype_visual_for_scene(LocType *loc,int level,int x,int z,int shape,int rotation){
#if defined(__vita__) && defined(BUILD_A_EMBEDDED)
    int id=r21_visual_location_id(loc->index,level,visual_scene_x+x,visual_scene_z+z,shape,rotation);
    if(id!=loc->index)return loctype_get(id);
#else
    (void)level;(void)x;(void)z;(void)shape;(void)rotation;
#endif
    return loc;
}

Model *loctype_get_model(LocType *loc, int shape, int rotation, int heightmapSW, int heightmapSE, int heightmapNE, int heightmapNW, int transformId) {
    int shapeIndex = -1;
    for (int i = 0; i < loc->shapes_and_models_count; i++) {
        if (loc->shapes[i] == shape) {
            shapeIndex = i;
            break;
        }
    }

    if (shapeIndex == -1) {
        return NULL;
    }

    int64_t bitset = ((int64_t)loc->index << 6) + ((int64_t)shapeIndex << 3) + rotation + ((int64_t)(transformId + 1) << 32);
    if (_LocType.reset) {
        bitset = 0L;
    }

    Model *cached = (Model *)lrucache_get(_LocType.modelCacheDynamic, bitset);
    if (cached) {
        /* Sequenced scenery is redrawn every frame. Keep it out of the static mesh. */
        if (loc->anim != -1 || transformId != -1) cached->r10_animated = true;
        if (_LocType.reset) {
            return cached;
        }

        if (loc->hillskew || loc->sharelight) {
            cached = model_copy_faces(cached, loc->hillskew, loc->sharelight, transformId == -1);
        }

        if (loc->hillskew) {
            int groundY = (heightmapSW + heightmapSE + heightmapNE + heightmapNW) / 4;

            for (int i = 0; i < cached->vertex_count; i++) {
                int x = cached->vertices_x[i];
                int z = cached->vertices_z[i];

                int heightS = heightmapSW + (heightmapSE - heightmapSW) * (x + 64) / 128;
                int heightN = heightmapNW + (heightmapNE - heightmapNW) * (x + 64) / 128;
                int y = heightS + (heightN - heightS) * (z + 64) / 128;

                cached->vertices_y[i] += y - groundY;
            }

            model_calculate_bounds_y(cached);
        }

        return cached;
    }

    if (shapeIndex >= loc->shapes_and_models_count) {
        return NULL;
    }

    int modelId = loc->models[shapeIndex];
    if (modelId == -1) return NULL;
    bool flipped = loc->mirror ^ (rotation > 3);
    int partCount = 0;
    for (int i = 0; i < loc->shapes_and_models_count; i++) {
        if (loc->shapes[i] == shape) partCount++;
    }
    // Duplicate shape entries are emitted only for recovered multipart locations.
    // Keep the original single-model cache key and path for all existing objects.
    int64_t staticKey = partCount > 1
        ? ((int64_t)1 << 32) | ((int64_t)loc->index << 1) | flipped
        : modelId + (flipped ? 65536 : 0);
    Model *model = (Model *)lrucache_get(_LocType.modelCacheStatic, staticKey);
    if (!model) {
        if (partCount == 1) {
            model = model_from_id(modelId, true);
            r99_close_door_trim(model,loc->index,modelId,shape);
            if (flipped) model_rotate_y180(model);
        } else {
            Model *parts[256];
            int n = 0;
            for (int i = 0; i < loc->shapes_and_models_count; i++) {
                if (loc->shapes[i] != shape) continue;
                int key = loc->models[i] + (flipped ? 65536 : 0);
                Model *part = (Model *)lrucache_get(_LocType.modelCacheStatic, key);
                if (!part) {
                    part = model_from_id(loc->models[i], true);
                    if (flipped) model_rotate_y180(part);
                    lrucache_put(_LocType.modelCacheStatic, key, &part->link);
                }
                parts[n++] = part;
            }
            model = model_from_models(parts, n, true);
        }
        /* Prepare bind-pose priorities once; pose copies retain the bounded metadata. */
        if(r66_pole_type(loc->index)&&modelId==3185&&shape==4)r57_prepare_body_layers(model);
        lrucache_put(_LocType.modelCacheStatic, staticKey, &model->link);
    }

    if((r87_vendor_type(loc->index)||r90_sewing_table_type(loc->index))&&shape==10)r57_prepare_fixture_layers(model,R87_VENDOR_BACKING_TEXTURES);
    bool scaled = loc->resizex != 128 || loc->resizey != 128 || loc->resizez != 128;
    bool translated = loc->offsetx != 0 || loc->offsety != 0 || loc->offsetz != 0;
    /* A centrepiece whose unrotated back sits on the outer wall plane is buried
     * in the 16-unit wall. 18 is that thickness plus 2 units of clearance.
     * The shift is local +X before rotation, so each wall side follows. */
    bool wall_backed = false;
    if (shape == CENTREPIECE_STRAIGHT && model->vertex_count > 0) {
        int min_x = model->vertices_x[0];
        int max_x = min_x;
        for (int v = 1; v < model->vertex_count; v++) {
            int x = model->vertices_x[v];
            if (x < min_x) min_x = x;
            if (x > max_x) max_x = x;
        }
        wall_backed = min_x <= -64 && max_x <= 16;
    }
    bool share_vertices = rotation == 0 && transformId == -1 && !scaled && !translated && !wall_backed;

    Model *modified = model_share_colored(model, !loc->recol_s, !loc->animHasAlpha, share_vertices, true);
    if (transformId != -1) {
        model_create_label_references(modified, false);
        model_apply_transform(modified, transformId);
        model_free_label_references(modified);
        modified->label_faces = NULL;
        modified->label_vertices = NULL;
    }

    if (wall_backed) {
        model_translate(modified, 0, 18, 0);
    }

    while (rotation-- > 0) {
        model_rotate_y90(modified);
    }

    if (loc->recol_s) {
        for (int i = 0; i < loc->recol_count; i++) {
            model_recolor(modified, loc->recol_s[i], loc->recol_d[i]);
        }
    }

    if (scaled) {
        model_scale(modified, loc->resizex, loc->resizey, loc->resizez);
    }

    if (translated) {
        model_translate(modified, loc->offsety, loc->offsetx, loc->offsetz);
    }

    model_calculate_normals(modified, loc->ambient + 64, loc->contrast * 5 + 768, -50, -10, -50, !loc->sharelight, true);
    if (loc->anim != -1 || transformId != -1) modified->r10_animated = true;

    if (loc->raiseobject == 1 || (loc->raiseobject == -1 && loc->blockwalk)) {
        modified->obj_raise = modified->max_y;
    }

    lrucache_put(_LocType.modelCacheDynamic, bitset, &modified->link);

    if (loc->hillskew || loc->sharelight) {
        modified = model_copy_faces(modified, loc->hillskew, loc->sharelight, transformId == -1);
    }

    if (loc->hillskew) {
        int groundY = (heightmapSW + heightmapSE + heightmapNE + heightmapNW) / 4;

        for (int i = 0; i < modified->vertex_count; i++) {
            int x = modified->vertices_x[i];
            int z = modified->vertices_z[i];

            int heightS = heightmapSW + (heightmapSE - heightmapSW) * (x + 64) / 128;
            int heightN = heightmapNW + (heightmapNE - heightmapNW) * (x + 64) / 128;
            int y = heightS + (heightN - heightS) * (z + 64) / 128;

            modified->vertices_y[i] += y - groundY;
        }

        model_calculate_bounds_y(modified);
    }

    if (loc->anim != -1 || transformId != -1) modified->r10_animated = true;
    return modified;
}
