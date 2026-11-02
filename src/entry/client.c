#include "../r109_map_decode_cache.h"
static unsigned r108_sound_received[1000],r108_sound_dispatched[1000],r108_sound_blocked;
#include "../ba_resource.h"
#include "../r106_scenery_speed.h"
#include "../r105_barrows_progress.h"
#include "r10_scene.h"
#include "../r57_body_layers.h"
#include "../r94_audio_cost.h"
#ifdef BUILD_A_EMBEDDED
 #include "../h8_remote.h"
#include "embedded_server.h"
#endif
#include "../ba_timing.h"
#include "../r45_path_recovery.h"
#include "../draw_phases.h"
#include "../r16_fast.h"
#include "../r13_capture.h"
#include "../ba_hw_r3.h"
#include "../ui_preview.h"
#include "../ba_timing.h"
#include "../title_theme.h"
#include "../vita_enhanced.h"
#include "../layoutcore.h"
#include "../layoutcore_draw.h"
#include "../ui474.h"
#include "research_ids.h"
#include "../vita_title.h"
#include "../vita_title_layout.h"
#include "../title_preview_sampling.h"
#include "../vita_dialogue.h"
#include "../classic_ui.h"
#include "../vita_wide_layout.h"
#ifdef client
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_arch_dreamcast) || defined(__NDS__)
#include <malloc.h>
#endif

#include "../allocator.h"
#include "../animbase.h"
#include "../animframe.h"
#include "../client.h"
#include "../clientstream.h"
#include "../collisionmap.h"
#include "../component.h"
#include "../custom.h"
#include "../datastruct/jstring.h"
#include "../datastruct/linklist.h"
#include "../defines.h"
#include "../flotype.h"
#include "../gameshell.h"
#include "../idktype.h"
#include "../inputtracking.h"
#include "../jagfile.h"
#include "../locaddentity.h"
#include "../locentity.h"
#include "../locmergeentity.h"
#include "../loctype.h"
#include "../model.h"
#include "../npcentity.h"
#include "../npctype.h"
#include "../objstackentity.h"
#include "../objtype.h"
#include "../packet.h"
#include "../pix24.h"
#include "../pix3d.h"
#include "../pix8.h"
#include "../pixmap.h"
#include "../platform.h"
#include "../playerentity.h"
#include "../projectileentity.h"
#include "../protocol.h"
#include "../seqtype.h"
#include "../sound/wave.h"
#include "../spotanimentity.h"
#include "../spotanimtype.h"
#include "../thirdparty/bzip.h"
#include "../thirdparty/ini.h"
#include "../thirdparty/isaac.h"
#include "../varptype.h"
#include "../wordenc/wordfilter.h"
#include "../wordenc/wordpack.h"
#include "../world.h"
#include "../world3d.h"
#include "../gl11.h"

extern int DESIGN_BODY_COLOR_LENGTH[];
extern int *DESIGN_BODY_COLOR[];
extern int DESIGN_HAIR_COLOR[];

extern Pix2D _Pix2D;
extern Pix3D _Pix3D;
extern InputTracking _InputTracking;
extern ModelData _Model;
extern ObjTypeData _ObjType;
extern ComponentData _Component;

/* One geometry for draw, picking, drag and retained updates. Enhanced
 * popups aim at screen centre while staying clear of the stone sidebar.
 * Minimized chat frees vertical space, so the same assembly sits lower. */
static int client_modal_offset(Client*c,int id,int enhanced){
 if(!enhanced)return guide_modal_offset(id,0);
 if(id<0||id>=_Component.count||!_Component.instances[id])return 0;
 Component*r=_Component.instances[id];const LcRect*side=&lc_game_geometry(LC_SIDEBAR)->rect;
 int x=(960-r->width)/2,limit=side->x-r->width;
 if(x>limit)x=limit;x-=48;if(x<0)x=0;return x-224;
}
static int client_modal_y(Client*c,int id,int enhanced){
 if(!enhanced)return guide_modal_y(id,0);
 if(id<0||id>=_Component.count||!_Component.instances[id])return 0;
 int bottom=ve_chat_hidden(c)?520:lc_game_geometry(LC_CHAT)->rect.y;
 int y=(bottom-_Component.instances[id]->height)/2;return y>0?y:0;
}

extern IdkTypeData _IdkType;
extern VarpTypeData _VarpType;
extern SeqTypeData _SeqType;
extern LocTypeData _LocType;
extern SpotAnimTypeData _SpotAnimType;
extern NpcTypeData _NpcType;
extern PlayerEntityData _PlayerEntity;
extern WaveData _Wave;
extern WorldData _World;
extern SceneData _World3D;
extern Custom _Custom;

ClientData _Client = {
    .clientversion = 225,
    .members = true,
    .nodeid = 10,
    .socketip = "localhost",
#ifdef WITH_RSA_BIGINT
    // original rsa keys in dec, only used with js bigints: openssl with dec requires bigger result array and it doesn't work with rsa-tiny
    .rsa_exponent = "58778699976184461502525193738213253649000149147835990136706041084440742975821",
    .rsa_modulus = "7162900525229798032761816791230527296329313291232324290237849263501208207972894053929065636522363163621000728841182238772712427862772219676577293600221789",
#else
    // original rsa keys in hex
    .rsa_exponent = "81f390b2cf8ca7039ee507975951d5a0b15a87bf8b3f99c966834118c50fd94d", // pad exponent to an even number (prefix a 0 if needed)
    .rsa_modulus = "88c38748a58228f7261cdc340b5691d7d0975dee0ecdb717609e6bf971eb3fe723ef9d130e4686813739768ad9472eb46d8bfcc042c1a5fcb05e931f632eea5d",
#endif
};

const int CHAT_COLORS[6] = {YELLOW, RED, GREEN, CYAN, MAGENTA, WHITE};
const int LOC_SHAPE_TO_LAYER[23] = {0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3};

// TODO add more static funcs here
static void client_draw_interface(Client *c, Component *com, int x, int y, int scrollY);
static unsigned long long r106_ambient_input,r106_ambient_output,r106_action_input,r106_action_output;
static void draw2DEntityElements(Client *c);
static void drawTileHint(Client *c);
static void draw3DEntityElements(Client *c);
static void drawTooltip(Client *c);
static int ve_world_speech_active(Client*c) {
    if(c->local_player&&c->local_player->pathing_entity.chatTimer>0)return 1;
    for(int i=0;i<c->npc_count;i++){NpcEntity*n=c->npcs[c->npc_ids[i]];if(n&&n->pathing_entity.chatTimer>0)return 1;}
    for(int i=0;i<c->player_count;i++){PlayerEntity*p=c->players[c->player_ids[i]];if(p&&p->pathing_entity.chatTimer>0)return 1;}
    return 0;
}

static PixMap *ve_effect_pm[MAX_CHATS+12];
static size_t ve_effect_bytes;
static unsigned ve_effect_allocations,ve_effect_resizes;
static PixMap *ve_effect_surface(int slot,int w,int h){
    if(slot<0||slot>=MAX_CHATS+12)return NULL;
    if(w<16)w=16;if(w>1024)w=1024;if(h<16)h=16;if(h>96)h=96;
    PixMap*p=ve_effect_pm[slot];
    if(p&&(p->width!=w||p->height!=h)){ve_effect_resizes++;ve_effect_bytes-=(size_t)p->width*p->height*12;pixmap_free(p);ve_effect_pm[slot]=p=NULL;}
    if(!p){size_t bytes=(size_t)w*h*12;if(ve_effect_bytes+bytes>4u*1024u*1024u)return NULL;p=pixmap_new(w,h);if(!p)return NULL;ve_effect_pm[slot]=p;ve_effect_bytes+=bytes;ve_effect_allocations++;}
    pixmap_bind(p);memset(p->pixels,0xff,(size_t)w*h*4);return p;
}
static void ve_effect_finish(PixMap*p,Pix2D saved,int x,int y){pixmap_draw(p,x,y);_Pix2D=saved;}
static int ve_complex_overlay(Client*c){return _Custom.camera_editor||_Custom.show_debug;}

static void updateWorldLocationState(Client *c);
static void ve_draw_location_status(Client *c);
static void ve_draw_barrows_progress(Client *c);
static int barrows_progress_count(Client*c);
static int ve_ui_dirty(Client *c);
static int ve_script_dirty;
static int ve_anim_advanced;
static int ve_popup_patch_guard,ve_popup_frame;
static int ve_input_advanced;
static int ve_native_markers(void);
int ba_agility_probe_enabled;
static void ve_draw_world_markers(Client *c);
static void ve_ui_probe(Client *c);
static int ve_overlay_held_frame;
int ve_overlay_held(void) { return ve_overlay_held_frame; }
static void ve_note_component(Client *c, int com) {
    int id = com;
    for (int guard = 0; id >= 0 && guard < 24; guard++) {
        Component *p = _Component.instances[id];
        if (!p) return;
        if (id == c->viewport_interface_id || id == c->chat_interface_id || id == c->sticky_chat_interface_id) {
            ve_script_dirty = 1;
            return;
        }
        if (id == c->sidebar_interface_id || (c->selected_tab >= 0 && c->selected_tab < 15 && id == c->tab_interface_id[c->selected_tab])) {
            c->redraw_sidebar = true;
            return;
        }
        if (p->layer < 0 || p->layer == id) return;
        id = p->layer;
    }
}
static void client_scenemap_free(Client *c);
extern void ba_cadence_diagnostic(const char *line);
static void client_build_scene(Client *c);
static void client_clear_caches(void);
static void client_update_orbit_camera(Client *c);

void client_init_global(void) {
    int acc = 0;
    for (int i = 0; i < 99; i++) {
        int level = i + 1;
        int delta = (int)((double)level + pow(2.0, (double)level / 7.0) * 300.0);
        acc += delta;
        _Client.levelExperience[i] = acc / 4;
    }
}

void client_load(Client *c) {
    gl_load();

// TODO missing bits
// String vendor = System.getProperties().getProperty("java.vendor");
// if (vendor.toLowerCase().indexOf("sun") != -1 || vendor.toLowerCase().indexOf("apple") != -1) {
// 	signlink.sunjava = true;
// }
// if (signlink.sunjava) {
// 	super.mindel = 5;
// }

    if (!_Client.lowmem) {
        platform_set_midi("scape_main", 12345678, 40000);
    }

    if (_Client.started) {
        c->error_started = true;
        return;
    }

    _Client.started = true;

    // bool good = c->shell->window;
    // const char* host = this.getHost();
    // if (host.endsWith("2004scape.org")) {
    // 	// intended domain for players
    // 	good = true;
    // }
    // if (host.endsWith("localhost") || host.endsWith("127.0.0.1")) {
    // 	// allow localhost
    // 	good = true;
    // }
    // if (host.startsWith("192.168.") || host.startsWith("172.16.") || host.startsWith("10.")) {
    // 	// allow lan
    // 	good = true;
    // }
    // if (!good) {
    // 	this.errorHost = true;
    // 	return;
    // }

#ifdef __wasm
    int retry = 5;
#endif
    c->archive_checksum[8] = 0;
    while (c->archive_checksum[8] == 0) {
        client_draw_progress(c, "Connecting to fileserver", 10);
        char message[PATH_MAX];
        sprintf(message, "crc%d", (int)(jrand() * 9.9999999e7));
        int size = 0;
        int8_t *buffer = client_openurl(message, &size);
        if (!buffer) {
#ifdef __wasm
            for (int i = retry; i > 0; i--) {
                sprintf(message, "Error loading - Will retry in %d secs.", i);
                client_draw_progress(c, message, 10);
                rs2_sleep(1000);
            }
            retry *= 2;
            if (retry > 60) {
                retry = 60;
            }
#else
            // TODO: hardcoded for now add openurl
            c->archive_checksum[0] = 0;
            c->archive_checksum[1] = 784449929;
            c->archive_checksum[2] = -152376011;
            c->archive_checksum[3] = -1907447422;
            c->archive_checksum[4] = 1870069386;
            c->archive_checksum[5] = 385830284;
            c->archive_checksum[6] = -975987241;
            c->archive_checksum[7] = 1570981179;
            c->archive_checksum[8] = 606476196;
#endif
        } else {
            Packet *checksums = packet_new(buffer, size); // 36
            for (int i = 0; i < 9; i++) {
                c->archive_checksum[i] = g4(checksums);
            }
        }
    }

    c->archive_title = load_archive(c, "title", c->archive_checksum[1], "title screen", 10);
    if (!c->archive_title) {
        c->error_loading = true;
        return;
    } else {
        platform_free_font();
    }
    c->font_plain11 = pixfont_from_archive(c->archive_title, "p11");
    c->font_plain12 = pixfont_from_archive(c->archive_title, "p12");
    c->font_bold12 = pixfont_from_archive(c->archive_title, "b12");
    c->font_quill8 = pixfont_from_archive(c->archive_title, "q8");
    client_load_title_background(c);
    client_load_title_images(c);

    Jagfile *config = load_archive(c, "config", c->archive_checksum[2], "config", 15);
    Jagfile *inter = load_archive(c, "interface", c->archive_checksum[3], "interface", 20);
    Jagfile *media = load_archive(c, "media", c->archive_checksum[4], "2d graphics", 30);
    Jagfile *models = load_archive(c, "models", c->archive_checksum[5], "3d graphics", 40);
    Jagfile *textures = load_archive(c, "textures", c->archive_checksum[6], "textures", 60);
    Jagfile *wordenc = load_archive(c, "wordenc", c->archive_checksum[7], "chat system", 65);
    Jagfile *sounds = load_archive(c, "sounds", c->archive_checksum[8], "sound effects", 70);

    if (!config || !inter || !media || !models || !textures || !wordenc || !sounds) {
        c->error_loading = true;
        return;
    }

    c->levelTileFlags = calloc(4, sizeof(*c->levelTileFlags));
    c->levelHeightmap = calloc(4, sizeof(*c->levelHeightmap));
    c->scene = world3d_new(c->levelHeightmap, 104, 4, 104);
    for (int level = 0; level < 4; level++) {
        c->levelCollisionMap[level] = collisionmap_new(104, 104);
    }
    c->image_minimap = pix24_new(512, 512, false);
    client_draw_progress(c, "Unpacking media", 75);
    c->image_invback = pix8_from_archive(media, "invback", 0);
    c->image_chatback = pix8_from_archive(media, "chatback", 0);
    c->image_mapback = pix8_from_archive(media, "mapback", 0);
    c->image_backbase1 = pix8_from_archive(media, "backbase1", 0);
#ifdef VITA_NATIVE_WIDESCREEN
    classic_extend_pix8(c->image_chatback, CHAT_WIDTH, 96, 24, 0);
    classic_extend_chat_controls(c->image_backbase1);
#endif
    c->image_backbase2 = pix8_from_archive(media, "backbase2", 0);
#ifdef VITA_NATIVE_WIDESCREEN
    Pix8 *corner_stone_source = pix8_from_archive(media, "backvmid3", 0);
    classic_repair_lower_corner(c->image_backbase2, corner_stone_source);
    pix8_free(corner_stone_source);
#endif
    c->image_backhmid1 = pix8_from_archive(media, "backhmid1", 0);
#ifdef VITA_NATIVE_WIDESCREEN
    classic_extend_tab_backing(c->image_backhmid1, SIDE_TABS_HEIGHT);
#endif
    for (int i = 0; i < 13; i++) {
        c->image_sideicons[i] = pix8_from_archive(media, "sideicons", i);
    }
    c->image_compass = pix24_from_archive(media, "compass", 0);

    for (int i = 0; i < 65; i++) {
        if (_Custom.hide_debug_sprite) {
            if (i == 22) {
                // weird debug sprite along water
                continue;
            }
        }

        c->image_mapscene[i] = pix8_from_archive(media, "mapscene", i);
        if (!c->image_mapscene[i]) {
            break;
        }
    }
    for (int i = 0; i < 65; i++) {
        c->image_mapfunction[i] = pix24_from_archive(media, "mapfunction", i);
        if (!c->image_mapfunction[i]) {
            break;
        }
    }
    for (int i = 0; i < 20; i++) {
        c->image_hitmarks[i] = pix24_from_archive(media, "hitmarks", i);
        if (!c->image_hitmarks[i]) {
            break;
        }
    }
    for (int i = 0; i < 20; i++) {
        c->image_headicons[i] = pix24_from_archive(media, "headicons", i);
        if (!c->image_headicons[i]) {
            break;
        }
    }
    c->image_mapflag = pix24_from_archive(media, "mapflag", 0);
    for (int i = 0; i < 8; i++) {
        c->image_crosses[i] = pix24_from_archive(media, "cross", i);
    }
    c->image_mapdot0 = pix24_from_archive(media, "mapdots", 0);
    c->image_mapdot1 = pix24_from_archive(media, "mapdots", 1);
    c->image_mapdot2 = pix24_from_archive(media, "mapdots", 2);
    c->image_mapdot3 = pix24_from_archive(media, "mapdots", 3);
    c->image_scrollbar0 = pix8_from_archive(media, "scrollbar", 0);
    c->image_scrollbar1 = pix8_from_archive(media, "scrollbar", 1);
    c->image_redstone1 = pix8_from_archive(media, "redstone1", 0);
    c->image_redstone2 = pix8_from_archive(media, "redstone2", 0);
    c->image_redstone3 = pix8_from_archive(media, "redstone3", 0);
    c->image_redstone1h = pix8_from_archive(media, "redstone1", 0);
    pix8_flip_horizontally(c->image_redstone1h);
    c->image_redstone2h = pix8_from_archive(media, "redstone2", 0);
    pix8_flip_horizontally(c->image_redstone2h);
    c->image_redstone1v = pix8_from_archive(media, "redstone1", 0);
    pix8_flip_vertically(c->image_redstone1v);
    c->image_redstone2v = pix8_from_archive(media, "redstone2", 0);
    pix8_flip_vertically(c->image_redstone2v);
    c->image_redstone3v = pix8_from_archive(media, "redstone3", 0);
    pix8_flip_vertically(c->image_redstone3v);
    c->image_redstone1hv = pix8_from_archive(media, "redstone1", 0);
    pix8_flip_horizontally(c->image_redstone1hv);
    pix8_flip_vertically(c->image_redstone1hv);
    c->image_redstone2hv = pix8_from_archive(media, "redstone2", 0);
    pix8_flip_horizontally(c->image_redstone2hv);
    pix8_flip_vertically(c->image_redstone2hv);
    Pix24 *backleft1 = pix24_from_archive(media, "backleft1", 0);
#ifdef VITA_NATIVE_WIDESCREEN
    classic_extend_pix24(backleft1, 8, GAME_VIEW_HEIGHT, 0, 16);
#endif
    c->area_backleft1 = pixmap_new(backleft1->width, backleft1->height);
    pix24_blit_opaque(backleft1, 0, 0);
    Pix24 *backleft2 = pix24_from_archive(media, "backleft2", 0);
    c->area_backleft2 = pixmap_new(backleft2->width, backleft2->height);
    pix24_blit_opaque(backleft2, 0, 0);
    Pix24 *backright1 = pix24_from_archive(media, "backright1", 0);
#ifdef VITA_NATIVE_WIDESCREEN
    classic_extend_pix24(backright1, 60, 166, 0, 16);
#endif
    c->area_backright1 = pixmap_new(backright1->width, backright1->height);
    pix24_blit_opaque(backright1, 0, 0);
    Pix24 *backright2 = pix24_from_archive(media, "backright2", 0);
    c->area_backright2 = pixmap_new(backright2->width, backright2->height);
    pix24_blit_opaque(backright2, 0, 0);
    Pix24 *backtop1 = pix24_from_archive(media, "backtop1", 0);
#ifdef VITA_NATIVE_WIDESCREEN
    classic_extend_pix24(backtop1, UI_RIGHT(561), 11, 64, 0);
#endif
    c->area_backtop1 = pixmap_new(backtop1->width, backtop1->height);
    pix24_blit_opaque(backtop1, 0, 0);
    Pix24 *backtop2 = pix24_from_archive(media, "backtop2", 0);
    c->area_backtop2 = pixmap_new(backtop2->width, backtop2->height);
    pix24_blit_opaque(backtop2, 0, 0);
    Pix24 *backvmid1 = pix24_from_archive(media, "backvmid1", 0);
#ifdef VITA_NATIVE_WIDESCREEN
    classic_extend_pix24(backvmid1, 41, 154, 0, 16);
#endif
    c->area_backvmid1 = pixmap_new(backvmid1->width, backvmid1->height);
    pix24_blit_opaque(backvmid1, 0, 0);
    Pix24 *backvmid2 = pix24_from_archive(media, "backvmid2", 0);
    c->area_backvmid2 = pixmap_new(backvmid2->width, backvmid2->height);
    pix24_blit_opaque(backvmid2, 0, 0);
    Pix24 *backvmid3 = pix24_from_archive(media, "backvmid3", 0);
    c->area_backvmid3 = pixmap_new(backvmid3->width, backvmid3->height);
    pix24_blit_opaque(backvmid3, 0, 0);
    Pix24 *backhmid2 = pix24_from_archive(media, "backhmid2", 0);
#ifdef VITA_NATIVE_WIDESCREEN
    classic_extend_chat_border(backhmid2, UI_RIGHT(562));
#endif
    c->area_backhmid2 = pixmap_new(backhmid2->width, backhmid2->height);
    pix24_blit_opaque(backhmid2, 0, 0);

    int rand_r = (int)(jrand() * 21.0) - 10;
    int rand_g = (int)(jrand() * 21.0) - 10;
    int rand_b = (int)(jrand() * 21.0) - 10;
    int _rand = (int)(jrand() * 41.0) - 20;
    for (int i = 0; i < 65; i++) {
        if (c->image_mapfunction[i]) {
            pix24_translate(c->image_mapfunction[i], rand_r + _rand, rand_g + _rand, rand_b + _rand);
        }

        if (c->image_mapscene[i]) {
            pix8_translate(c->image_mapscene[i], rand_r + _rand, rand_g + _rand, rand_b + _rand);
        }
    }

    client_draw_progress(c, "Unpacking textures", 80);
    pix3d_unpack_textures(textures);
    pix3d_set_brightness(0.8);
    pix3d_init_pool(PIX3D_POOL_COUNT);

    client_draw_progress(c, "Unpacking models", 83);
    model_unpack(models);
    animbase_unpack(models);
    animframe_unpack(models);

    client_draw_progress(c, "Unpacking config", 86);
    seqtype_unpack(config);
    loctype_unpack(config);
    flotype_unpack(config);
    objtype_unpack(config);
    npctype_unpack(config);
    idktype_unpack(config);
    spotanimtype_unpack(config);
    varptype_unpack(config);

    _ObjType.membersWorld = _Client.members;
    if (!_Client.lowmem) {
        client_draw_progress(c, "Unpacking sounds", 90);
        Packet *sound_dat = jagfile_to_packet(sounds, "sounds.dat");
        wave_unpack(sound_dat);
        packet_free(sound_dat);
        if (jagfile_has_file(sounds, "ns_pcm.dat")) {
            Packet *original_pcm = jagfile_to_packet(sounds, "ns_pcm.dat");
            wave_unpack_pcm(original_pcm);
            packet_free(original_pcm);
        }
    }

    client_draw_progress(c, "Unpacking interfaces", 92);
    PixFont *fonts[] = {c->font_plain11, c->font_plain12, c->font_bold12, c->font_quill8};
    component_unpack(inter, media, fonts);

    client_draw_progress(c, "Preparing game engine", 97);
    for (int y = 0; y < 33; y++) {
        int left = 999;
        int right = 0;
        for (int x = 0; x < 35; x++) {
            if (c->image_mapback->pixels[x + y * c->image_mapback->width] == 0) {
                if (left == 999) {
                    left = x;
                }
            } else if (left != 999) {
                right = x;
                break;
            }
        }
        c->compass_mask_line_offsets[y] = left;
        c->compass_mask_line_lengths[y] = right - left;
    }

    for (int y = 9; y < 160; y++) {
        int left = 999;
        int right = 0;
        for (int x = 10; x < 168; x++) {
            if (c->image_mapback->pixels[x + y * c->image_mapback->width] == 0 && (x > 34 || y > 34)) {
                if (left == 999) {
                    left = x;
                }
            } else if (left != 999) {
                right = x;
                break;
            }
        }
        c->minimap_mask_line_offsets[y - 9] = left - 21;
        c->minimap_mask_line_lengths[y - 9] = right - left;
    }

    pix3d_init3d(CHAT_WIDTH, 96);
    c->area_chatback_offsets = _Pix3D.line_offset;
    pix3d_init3d(190, 261);
    c->area_sidebar_offsets = _Pix3D.line_offset;
    pix3d_init3d(GAME_VIEW_WIDTH, GAME_VIEW_HEIGHT);
    c->area_viewport_offsets = _Pix3D.line_offset;

    int *distance = malloc(9 * sizeof(int));
    for (int x = 0; x < 9; x++) {
        int angle = x * 32 + 128 + 15;
        int offset = angle * 3 + 600;
        int sin = _Pix3D.sin_table[angle];
        distance[x] = offset * sin >> 16;
    }

    world3d_init(GAME_VIEW_WIDTH, GAME_VIEW_HEIGHT, 500, 800, distance);
    free(distance);
    wordfilter_unpack(wordenc);

    pix24_free(backleft1);
    pix24_free(backleft2);
    pix24_free(backright1);
    pix24_free(backright2);
    pix24_free(backtop1);
    pix24_free(backtop2);
    pix24_free(backvmid1);
    pix24_free(backvmid2);
    pix24_free(backvmid3);
    pix24_free(backhmid2);

    jagfile_free(config);
    jagfile_free(inter);
    jagfile_free(media);
    jagfile_free(models);
    jagfile_free(textures);
    jagfile_free(wordenc);
    jagfile_free(sounds);
    // } catch (Exception ex) {
    // 	ex.printStackTrace();
    // 	this.errorLoading = true;
    // }

// NOTE: we can't grow it so it needs to fit the max usage, left value is shifted to MiB (arbitrary value)
#if defined(_arch_dreamcast) || defined(__NDS__)
    malloc_stats();
    if (!bump_allocator_init(8 << 20)) {
#else
    if (!(_Client.lowmem ? bump_allocator_init(16 << 20) : bump_allocator_init(32 << 20))) {
#endif
        c->error_loading = true;
    }

    // network init happens here after game loads instead of in platform_init because being connected disables fast-forward in emulators
    if (!clientstream_init()) {
        c->error_loading = true;
    }

// TODO temp: wait for wiiu and switch touch input fixes, melonds 32mb emulation
#if defined(__WIIU__) || defined(__SWITCH__) || defined(__NDS__)
    client_login(c, c->username, c->password, false);
#endif

#if defined(_arch_dreamcast) || defined(__NDS__)
    // it's fine for the consoles memory to be full here, it frees the login screen after this
    malloc_stats();
#endif
}

static void ba_interface_trace(const char *operation,int component,const char *text){
 FILE *log=fopen("ux0:/data/RS2Vita/interface-packets.log","a");
 if(log){fprintf(log,"%llu|%s|%d|%.120s\n",(unsigned long long)ba_timing_now(),operation,component,text?text:"");fclose(log);}
}
void client_load_title_background(Client *c) {
    vita_title_invalidate();
    int length;
    uint8_t *data = jagfile_to_bytes(c->archive_title, "title.dat", &length);
    Pix24 *title = pix24_from_jpeg(data, length);
    free(data);

#ifdef VITA_NATIVE_WIDESCREEN
    Pix24 *wide = classic_load_restored_title();
    c->title_restored=wide!=NULL;
    if(!wide)wide=classic_wide_title_background(title);
    PixMap *pieces[9] = {c->image_title0,c->image_title1,c->image_title2,
        c->image_title3,c->image_title4,c->image_title5,c->image_title6,
        c->image_title7,c->image_title8};
    const int origins[9][2] = {
        {TITLE_FLAME_LEFT_X,TITLE_Y(0)},{TITLE_FLAME_RIGHT_X,TITLE_Y(0)},
        {TITLE_TOP_X,TITLE_TOP_Y},{TITLE_BOTTOM_X,TITLE_BOTTOM_Y},
        {TITLE_X(214),TITLE_Y(186)},{TITLE_LEFT_BASE_X,TITLE_Y(265)},
        {TITLE_X(574),TITLE_Y(265)},{TITLE_LEFT_MID_X,TITLE_Y(186)},
        {TITLE_X(574),TITLE_Y(186)}
    };
    for (int i=0;i<9;i++) {
        pixmap_bind(pieces[i]);
        pix24_blit_opaque(wide,-origins[i][0],-origins[i][1]);
    }
    if(c->title_restored){c->title_restored_background=wide;c->title_panel_scaled=pixmap_new(360,200);c->title_panel_background=pix24_new(360,200,false);memcpy(c->title_panel_background->pixels,c->image_title4->pixels,360*200*sizeof(int));}else pix24_free(wide);
#else
    pixmap_bind(c->image_title0);
    pix24_blit_opaque(title, 0, 0);

    pixmap_bind(c->image_title1);
    pix24_blit_opaque(title, -661, 0);

    pixmap_bind(c->image_title2);
    pix24_blit_opaque(title, -128, 0);

    pixmap_bind(c->image_title3);
    pix24_blit_opaque(title, -214, -386);

    pixmap_bind(c->image_title4);
    pix24_blit_opaque(title, -214, -186);

    pixmap_bind(c->image_title5);
    pix24_blit_opaque(title, 0, -265);

    pixmap_bind(c->image_title6);
    pix24_blit_opaque(title, -574, -265);

    pixmap_bind(c->image_title7);
    pix24_blit_opaque(title, -128, -186);

    pixmap_bind(c->image_title8);
    pix24_blit_opaque(title, -574, -186);

    int *mirror = malloc(title->width * sizeof(int));
    for (int y = 0; y < title->height; y++) {
        for (int x = 0; x < title->width; x++) {
            mirror[x] = title->pixels[title->width + title->width * y - x - 1];
        }

        if (title->width >= 0) {
            memcpy(&title->pixels[title->width * y], mirror, title->width * sizeof(int));
        }
    }
    free(mirror);

    pixmap_bind(c->image_title0);
    pix24_blit_opaque(title, 394, 0);

    pixmap_bind(c->image_title1);
    pix24_blit_opaque(title, -267, 0);

    pixmap_bind(c->image_title2);
    pix24_blit_opaque(title, 266, 0);

    pixmap_bind(c->image_title3);
    pix24_blit_opaque(title, 180, -386);

    pixmap_bind(c->image_title4);
    pix24_blit_opaque(title, 180, -186);

    pixmap_bind(c->image_title5);
    pix24_blit_opaque(title, 394, -265);

    pixmap_bind(c->image_title6);
    pix24_blit_opaque(title, -180, -265);

    pixmap_bind(c->image_title7);
    pix24_blit_opaque(title, 212, -186);

    pixmap_bind(c->image_title8);
    pix24_blit_opaque(title, -180, -186);

#endif
    pix24_free(title);
    title = pix24_from_archive(c->archive_title, "logo", 0);
    pixmap_bind(c->image_title2);
    if(c->title_restored)classic_draw_title_logo_scaled(title,TITLE_Y(18));else classic_draw_title_logo(title, TITLE_Y(18));

    pix24_free(title);
    title = NULL;
}

void client_update_flame_buffer(Client *c, Pix8 *image) {
    int flame_height = 256;
    memset(c->flame_buffer0, 0, FLAME_BUFFER_SIZE * sizeof(int));

    for (int i = 0; i < 5000; i++) {
        int index = (int)(jrand() * 128.0 * (double)flame_height);
        c->flame_buffer0[index] = (int)(jrand() * 256.0);
    }

    for (int i = 0; i < 20; i++) {
        for (int y = 1; y < flame_height - 1; y++) {
            for (int x = 1; x < 127; x++) {
                int index = x + (y << 7);
                c->flame_buffer1[index] = (c->flame_buffer0[index - 1] + c->flame_buffer0[index + 1] + c->flame_buffer0[index - 128] + c->flame_buffer0[index + 128]) / 4;
            }
        }

        int *last = c->flame_buffer0;
        c->flame_buffer0 = c->flame_buffer1;
        c->flame_buffer1 = last;
    }

    if (image) {
        int off = 0;

        for (int y = 0; y < image->height; y++) {
            for (int x = 0; x < image->width; x++) {
                if (image->pixels[off++] != 0) {
                    int x0 = x + image->crop_x + 16;
                    int y0 = y + image->crop_y + 16;
                    int index = x0 + (y0 << 7);
                    c->flame_buffer0[index] = 0;
                }
            }
        }
    }
}

void client_load_title_images(Client *c) {
    c->image_titlebox = pix8_from_archive(c->archive_title, "titlebox", 0);
    c->image_titlebutton = pix8_from_archive(c->archive_title, "titlebutton", 0);
#ifdef DISABLE_FLAMES
    // TODO: redraw behind "flames" when there's any spare memory, gets freed after login
    // c->image_flames_left = pix24_new(128, 265, false);
    // c->image_flames_right = pix24_new(128, 265, false);
    // memcpy(c->image_flames_left->pixels, c->image_title0->pixels, 33920 * sizeof(int));
    // memcpy(c->image_flames_right->pixels, c->image_title1->pixels, 33920 * sizeof(int));
#else
    c->image_runes = calloc(12, sizeof(Pix8 *));
    for (int i = 0; i < 12; i++) {
        c->image_runes[i] = pix8_from_archive(c->archive_title, "runes", i);
    }
    c->image_flames_left = pix24_new(128, 265, false);
    c->image_flames_right = pix24_new(128, 265, false);
    memcpy(c->image_flames_left->pixels, c->image_title0->pixels, 33920 * sizeof(int));
    memcpy(c->image_flames_right->pixels, c->image_title1->pixels, 33920 * sizeof(int));
#ifdef VITA_NATIVE_WIDESCREEN
    if(c->title_restored){
        c->title_flame_left=pixmap_new(200,265);c->title_flame_right=pixmap_new(200,265);
        c->title_flame_clip_left=pixmap_new(200,217);c->title_flame_clip_right=pixmap_new(200,217);
        pix24_free(c->image_flames_left);pix24_free(c->image_flames_right);
        c->image_flames_left=pix24_new(200,265,false);c->image_flames_right=pix24_new(200,265,false);
        const int origin[2]={54,710};Pix24*bg[2]={c->image_flames_left,c->image_flames_right};
        for(int side=0;side<2;side++)for(int y=0;y<265;y++)for(int x=0;x<200;x++){
            int screenY=y-48;if(screenY<0)screenY=0;
            bg[side]->pixels[y*200+x]=c->title_restored_background->pixels[screenY*960+origin[side]+x];
        }
        pix24_free(c->title_restored_background);c->title_restored_background=NULL;
    }
#endif

    c->flame_gradient0 = calloc(256, sizeof(int));
    for (int i = 0; i < 64; i++) {
        c->flame_gradient0[i] = i * 262144;
    }
    for (int i = 0; i < 64; i++) {
        c->flame_gradient0[i + 64] = i * 1024 + RED;
    }
    for (int i = 0; i < 64; i++) {
        c->flame_gradient0[i + 128] = i * 4 + YELLOW;
    }
    for (int i = 0; i < 64; i++) {
        c->flame_gradient0[i + 192] = WHITE;
    }
    c->flame_gradient1 = calloc(256, sizeof(int));
    for (int i = 0; i < 64; i++) {
        c->flame_gradient1[i] = i * 1024;
    }
    for (int i = 0; i < 64; i++) {
        c->flame_gradient1[i + 64] = i * 4 + GREEN;
    }
    for (int i = 0; i < 64; i++) {
        c->flame_gradient1[i + 128] = i * 262144 + CYAN;
    }
    for (int i = 0; i < 64; i++) {
        c->flame_gradient1[i + 192] = WHITE;
    }
    c->flame_gradient2 = calloc(256, sizeof(int));
    for (int i = 0; i < 64; i++) {
        c->flame_gradient2[i] = i * 4;
    }
    for (int i = 0; i < 64; i++) {
        c->flame_gradient2[i + 64] = i * 262144 + BLUE;
    }
    for (int i = 0; i < 64; i++) {
        c->flame_gradient2[i + 128] = i * 1024 + MAGENTA;
    }
    for (int i = 0; i < 64; i++) {
        c->flame_gradient2[i + 192] = WHITE;
    }
    c->flame_gradient = calloc(256, sizeof(int));
    c->flame_buffer0 = calloc(FLAME_BUFFER_SIZE, sizeof(int));
    c->flame_buffer1 = calloc(FLAME_BUFFER_SIZE, sizeof(int));
    client_update_flame_buffer(c, NULL);
    c->flame_buffer3 = calloc(FLAME_BUFFER_SIZE, sizeof(int));
    c->flame_buffer2 = calloc(FLAME_BUFFER_SIZE, sizeof(int));
    client_draw_progress(c, "Connecting to fileserver", 10);
    if (!c->flame_active) {
        c->flame_active = true;
    }
#endif
}

static void client_update_flames(Client *c) {
    ba_timing_record(BA_FLAME_STEP,0);
    int height = 256;
    for (int x = 10; x < 117; x++) {
        int _rand = (int)(jrand() * 100.0);
        if (_rand < 50) {
            c->flame_buffer3[x + ((height - 2) << 7)] = 255;
        }
    }

    for (int l = 0; l < 100; l++) {
        int x = (int)(jrand() * 124.0) + 2;
        int y = (int)(jrand() * 128.0) + 128;
        int index = x + (y << 7);
        c->flame_buffer3[index] = 192;
    }

    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < 127; x++) {
            int index = x + (y << 7);
            c->flame_buffer2[index] = (c->flame_buffer3[index - 1] + c->flame_buffer3[index + 1] + c->flame_buffer3[index - 128] + c->flame_buffer3[index + 128]) / 4;
        }
    }

    c->flame_cycle0 += 128;
    if (c->flame_cycle0 > FLAME_BUFFER_SIZE) {
        c->flame_cycle0 -= FLAME_BUFFER_SIZE;
        int _rand = (int)(jrand() * 12.0);
        client_update_flame_buffer(c, c->image_runes[_rand]);
    }

    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < 127; x++) {
            int index = x + (y << 7);
            int intensity = c->flame_buffer2[index + 128] - c->flame_buffer0[index + c->flame_cycle0 & FLAME_BUFFER_SIZE - 1] / 5;
            if (intensity < 0) {
                intensity = 0;
            }
            c->flame_buffer3[index] = intensity;
        }
    }

    for (int y = 0; y < height - 1; y++) {
        c->flame_line_offset[y] = c->flame_line_offset[y + 1];
    }

    c->flame_line_offset[height - 1] = (int)(sin((double)_Client.loop_cycle / 14.0) * 16.0 + sin((double)_Client.loop_cycle / 15.0) * 14.0 + sin((double)_Client.loop_cycle / 16.0) * 12.0);

    if (c->flameGradientCycle0 > 0) {
        c->flameGradientCycle0 -= 4;
    }

    if (c->flameGradientCycle1 > 0) {
        c->flameGradientCycle1 -= 4;
    }

    if (c->flameGradientCycle0 == 0 && c->flameGradientCycle1 == 0) {
        int _rand = (int)(jrand() * 2000.0);

        if (_rand == 0) {
            c->flameGradientCycle0 = 1024;
        } else if (_rand == 1) {
            c->flameGradientCycle1 = 1024;
        }
    }
}

static int mix(int src, int alpha, int dst) {
    int invAlpha = 256 - alpha;
    return (((src & 0xff00ff) * invAlpha + (dst & 0xff00ff) * alpha & 0xff00ff00) + ((src & 0xff00) * invAlpha + (dst & 0xff00) * alpha & 0xff0000)) >> 8;
}

// Approved restoration: full original intensity field, no transparency mask.
// Widen only the reusable title-local tiles; simulation/palette/timing unchanged.
static void client_draw_restored_flames(Client*c){
 PixMap*targets[2]={c->title_flame_left,c->title_flame_right};Pix24*background[2]={c->image_flames_left,c->image_flames_right};
 for(int side=0;side<2;side++){
  memcpy(targets[side]->pixels,background[side]->pixels,200*265*sizeof(int));
  // Approved B: nearest-neighbor 90% transparent field, centered/bottom anchored.
  for(int dy=0;dy<195;dy++){
   int sy=title_flame_y[dy]+39;if(sy>=254)continue;
   int offset=c->flame_line_offset[sy+1]*(255-sy)/256;
   for(int dx=0;dx<180;dx++){
    int source_x=title_flame_x[dx],sx=side==0?source_x-36+offset:source_x-35-offset;
    if(sx<0||sx>=128||sy>=254)continue;
    int alpha=c->flame_buffer3[sy*128+sx];if(!alpha)continue;
    int colour=c->flame_gradient[alpha],inverse=256-alpha,at=(dy+70)*200+dx+10,old=targets[side]->pixels[at];
    targets[side]->pixels[at]=(((colour&0xff00ff)*alpha+(old&0xff00ff)*inverse&0xff00ff00)+((colour&0xff00)*alpha+(old&0xff00)*inverse&0xff0000))>>8;
   }
  }
 }
 memcpy(c->title_flame_clip_left->pixels,targets[0]->pixels+48*200,200*217*sizeof(int));memcpy(c->title_flame_clip_right->pixels,targets[1]->pixels+48*200,200*217*sizeof(int));
 c->title_flame_clip_left->dirty=true;c->title_flame_clip_right->dirty=true;
 pixmap_draw(c->title_flame_clip_left,54,0);pixmap_draw(c->title_flame_clip_right,710,0);
}

static void client_draw_flames(Client *c) {
    int height = 256;
    PixMap*left=c->title_restored?c->title_flame_left:c->image_title0;
    PixMap*right=c->title_restored?c->title_flame_right:c->image_title1;

    if (c->flameGradientCycle0 > 0) {
        for (int i = 0; i < 256; i++) {
            if (c->flameGradientCycle0 > 768) {
                c->flame_gradient[i] = mix(c->flame_gradient0[i], 1024 - c->flameGradientCycle0, c->flame_gradient1[i]);
            } else if (c->flameGradientCycle0 > 256) {
                c->flame_gradient[i] = c->flame_gradient1[i];
            } else {
                c->flame_gradient[i] = mix(c->flame_gradient1[i], 256 - c->flameGradientCycle0, c->flame_gradient0[i]);
            }
        }
    } else if (c->flameGradientCycle1 > 0) {
        for (int i = 0; i < 256; i++) {
            if (c->flameGradientCycle1 > 768) {
                c->flame_gradient[i] = mix(c->flame_gradient0[i], 1024 - c->flameGradientCycle1, c->flame_gradient2[i]);
            } else if (c->flameGradientCycle1 > 256) {
                c->flame_gradient[i] = c->flame_gradient2[i];
            } else {
                c->flame_gradient[i] = mix(c->flame_gradient2[i], 256 - c->flameGradientCycle1, c->flame_gradient0[i]);
            }
        }
    } else {
        memcpy(c->flame_gradient, c->flame_gradient0, 256 * sizeof(int));
    }
    if(c->title_restored){client_draw_restored_flames(c);return;}
    memcpy(left->pixels, c->image_flames_left->pixels, 33920 * sizeof(int));

    int srcOffset = 0;
    int dstOffset = 1152;

    for (int y = 1; y < height - 1; y++) {
        int offset = c->flame_line_offset[y] * (height - y) / height;
        int step = offset + 22;
        if (step < 0) {
            step = 0;
        }
        srcOffset += step;
        for (int x = step; x < 128; x++) {
            int value = c->flame_buffer3[srcOffset++];
            if (value == 0) {
                dstOffset++;
            } else {
                int alpha = value;
                int invAlpha = 256 - value;
                value = c->flame_gradient[value];
                int background = left->pixels[dstOffset];
                left->pixels[dstOffset++] = (((value & 0xff00ff) * alpha + (background & 0xff00ff) * invAlpha & 0xff00ff00) + ((value & 0xff00) * alpha + (background & 0xff00) * invAlpha & 0xff0000)) >> 8;
            }
        }
        dstOffset += step;
    }

    pixmap_draw(left,TITLE_FLAME_LEFT_X,TITLE_Y(0));

    memcpy(right->pixels, c->image_flames_right->pixels, 33920 * sizeof(int));

    srcOffset = 0;
    dstOffset = 1176;
    for (int y = 1; y < height - 1; y++) {
        int offset = c->flame_line_offset[y] * (height - y) / height;
        int step = 103 - offset;
        dstOffset += offset;
        for (int x = 0; x < step; x++) {
            int value = c->flame_buffer3[srcOffset++];
            if (value == 0) {
                dstOffset++;
            } else {
                int alpha = value;
                int invAlpha = 256 - value;
                value = c->flame_gradient[value];
                int background = right->pixels[dstOffset];
                right->pixels[dstOffset++] = (((value & 0xff00ff) * alpha + (background & 0xff00ff) * invAlpha & 0xff00ff00) + ((value & 0xff00) * alpha + (background & 0xff00) * invAlpha & 0xff0000)) >> 8;
            }
        }
        srcOffset += 128 - step;
        dstOffset += 128 - step - offset;
    }

    pixmap_draw(right,TITLE_FLAME_RIGHT_X,TITLE_Y(0));
}

void client_run_flames(Client *c) {
    /* The approved native 20 ms loop presents the original paired update every
       two frames (40 ms). Keep that measured cadence independently of rendering. */
    static uint64_t next = 0;
    static Client*flame_owner;static unsigned flame_generation;static bool flame_frame_ready;
    uint64_t now=rs2_now();
    if(!c->flame_active){next=0;flame_frame_ready=false;return;}
    if(!next || now>next+1000)next=now;
    bool changed=false;
    while(now>=next){
        client_update_flames(c);
        client_update_flames(c);
        next+=40;
        changed=true;
    }
    unsigned generation=vita_title_generation();
    if(changed||(_Custom.use_opengl11&&(!flame_frame_ready||flame_owner!=c||flame_generation!=generation))){
        client_draw_flames(c);flame_frame_ready=true;flame_owner=c;flame_generation=generation;
    }else if(_Custom.use_opengl11){
        /* GL still redraws the original flame quads on every presentation;
         * only identical CPU compositing/uploads between40ms updates are held. */
        if(c->title_restored){pixmap_draw(c->title_flame_clip_left,54,0);pixmap_draw(c->title_flame_clip_right,710,0);}
        else {pixmap_draw(c->image_title0,TITLE_FLAME_LEFT_X,TITLE_Y(0));pixmap_draw(c->image_title1,TITLE_FLAME_RIGHT_X,TITLE_Y(0));}
    }

    /* NOTE: original
    // try {
    uint64_t last = rs2_now();
    int cycle = 0;
    int interval = 20;
    while (c->flame_active) {
        client_update_flames(c);
        client_update_flames(c);
        client_draw_flames(c);

        cycle++;

        if (cycle > 10) {
            uint64_t now = rs2_now();
            int delay = (int)(now - last) / 10 - interval;

            interval = 40 - delay;
            if (interval < 5) {
                interval = 5;
            }

            cycle = 0;
            last = now;
        }

        // try {
        rs2_sleep(interval);
        // } catch (@Pc(52) Exception ignored) {
        // }
    }
    // } catch (@Pc(58) Exception ignored) {
    // }
    */
}

/* Opt-in native performance harness, absent from distributed VPK data.
 * Uses the ordinary login protocol and a copied disposable save; no UI input,
 * quest variables, skills or inventory are fabricated. */
static bool client_try_move(Client *c, int srcX, int srcZ, int dx, int dz, int type, int locWidth, int locLength, int locRotation, int locShape, int forceapproach, bool tryNearest);
static bool ba_bench_active;
static void pos_trace(Client *c, const char *why, int jump, int arg_x, int arg_z) {
    if(!r16_diag_verbose())return;
    static uint64_t next;
    uint64_t now = ba_timing_now();
    /* "tick" is throttled. Teleports, steps, and click-to-move are not. */
    int always = why && (strcmp(why, "teleport") == 0 || strcmp(why, "move") == 0 || strcmp(why, "step") == 0);
    if (!always && now < next) return;
    if (!always) next = now + 1000000ull;
    PathingEntity *entity = c->local_player ? &c->local_player->pathing_entity : NULL;
    int fine_x = entity ? entity->x : -1;
    int fine_z = entity ? entity->z : -1;
    int path_x = entity ? entity->pathTileX[0] : -1;
    int path_z = entity ? entity->pathTileZ[0] : -1;
    int path_len = entity ? entity->pathLength : -1;
    int world_x = entity ? c->sceneBaseTileX + fine_x / 128 : 0;
    int world_z = entity ? c->sceneBaseTileZ + fine_z / 128 : 0;
    int click = c->shell ? c->shell->mouse_click_button : 0;
    int click_x = c->shell ? c->shell->mouse_click_x : -1;
    int click_y = c->shell ? c->shell->mouse_click_y : -1;
    int mouse_x = c->shell ? c->shell->mouse_x : -1;
    int mouse_y = c->shell ? c->shell->mouse_y : -1;
    int queued = c->shell ? (c->shell->key_queue_write_pos - c->shell->key_queue_read_pos) & 0x7f : 0;
    FILE *trace = fopen("ux0:/data/RS2Vita/pos-trace.txt", "a");
    if (!trace) return;
    fprintf(trace, "t=%llu why=%s jump=%d arg=%d,%d base=%d,%d center=%d,%d world=%d,%d path=%d,%d len=%d fine=%d,%d click=%d at=%d,%d mouse=%d,%d keys=%d yaw=%d\n",
            (unsigned long long)now, why ? why : "", jump, arg_x, arg_z, c->sceneBaseTileX, c->sceneBaseTileZ, c->sceneCenterZoneX, c->sceneCenterZoneZ,
            world_x, world_z, path_x, path_z, path_len, fine_x, fine_z, click, click_x, click_y, mouse_x, mouse_y, queued, c->orbit_camera_yaw);
    fclose(trace);
}
static void ba_hardware_benchmark(Client*c){
#ifdef BUILD_A_EMBEDDED
 static int loaded,enabled,attempted,logout_seconds;
 if(!loaded){loaded=1;FILE*f=fopen("ux0:/data/RS2Vita/hardware-benchmark.txt","r");if(f){fscanf(f,"%d %d",&enabled,&logout_seconds);fclose(f);}}
 if(h5_mode()=='T')return;
 /* A normal launch waits on the Login button. The world load starts there. */
 if(enabled)ba_bench_active=true;
 static uint64_t check;static unsigned lines;uint64_t now=ba_timing_now();
 if(r16_diag_verbose()&&!h5_measuring()&&!h8_is_running()&&now>=check&&lines<24){check=now+5000000;FILE*f=fopen("ux0:/data/RS2Vita/benchmark-state.csv",lines?"a":"w");if(f){fprintf(f,"%llu,%d,%d,%d,%d,%d,%s|%s\n",(unsigned long long)now,c->ingame,c->scene_state,ba_server_ready(),c->local_player?c->local_player->pathing_entity.x/128+c->sceneBaseTileX:0,c->local_player?c->local_player->pathing_entity.z/128+c->sceneBaseTileZ:0,c->login_message0?c->login_message0:"",c->login_message1?c->login_message1:"");fclose(f);}lines++;}

 if(enabled&&!c->ingame&&!attempted&&_Client.loop_cycle>50){attempted=1;strcpy(c->username,h5_username());strcpy(c->password,"test");client_login(c,c->username,c->password,false);}
 if(c->ingame){
 pos_trace(c, "tick", -1, 0, 0);
 static unsigned h5_actions,h5_next_action;
 if(c->scene_state==2&&h5_measuring()&&h5_actions<4&&_Client.loop_cycle>=h5_next_action&&(h5_mode()=='R'||h5_mode()=='I')){
  h5_next_action=_Client.loop_cycle+400;h5_actions++;
  int sx=c->local_player->pathing_entity.pathTileX[0],sz=c->local_player->pathing_entity.pathTileZ[0];
  if(h5_mode()=='R')client_try_move(c,sx,sz,sx+40,sz,0,0,0,0,0,0,true);
  else if(h5_actions==1)client_try_move(c,sx,sz,sx+2,sz,0,0,0,0,0,0,true);
  else for(int i=0;i<c->npc_count;i++){NpcEntity*n=c->npcs[c->npc_ids[i]];if(n&&n->type&&n->type->op&&n->type->op[0]&&strstr(n->type->op[0],"Talk")){extern void client_ui474_invoke(Client*,int,int,int,int);client_ui474_invoke(c,728,c->npc_ids[i],0,0);break;}}
 }
 return; /* H8 owns only explicitly armed benchmark actions. */
 c->orbit_camera_yaw=0;c->orbit_camera_pitch=256;c->minimap_anticheat_angle=0;c->minimap_zoom=0;
 static uint64_t world_since;static int moved,closed;if(c->scene_state==2&&!world_since)world_since=now;
 if(world_since&&!moved&&logout_seconds>=330&&now-world_since>280000000u){moved=1;int sx=c->local_player->pathing_entity.pathTileX[0],sz=c->local_player->pathing_entity.pathTileZ[0];bool accepted=client_try_move(c,sx,sz,sx+3,sz,0,0,0,0,0,0,true);FILE*f=fopen("ux0:/data/RS2Vita/benchmark-lifecycle.log","a");if(f){fprintf(f,"MOVE normal_client_route accepted=%d\n",accepted);fclose(f);}}
 if(world_since&&!closed&&logout_seconds>0&&now-world_since>(uint64_t)logout_seconds*1000000u){closed=1;int x=c->sceneBaseTileX+c->local_player->pathing_entity.x/128,z=c->sceneBaseTileZ+c->local_player->pathing_entity.z/128;client_logout(c);FILE*f=fopen("ux0:/data/RS2Vita/benchmark-lifecycle.log","a");if(f){fprintf(f,"LOGOUT ordinary_client_logout ingame=%d x=%d z=%d\n",c->ingame,x,z);fclose(f);}}
}
#else
 (void)c;
#endif
}

void h8_game_step(void *context){
 Client*c=context;static unsigned previous_running;unsigned kind,seconds,ui;
 int ready=c->ingame&&c->scene_state==2&&c->local_player;
 int x=ready?c->sceneBaseTileX+c->local_player->pathing_entity.x/128:0;
 int z=ready?c->sceneBaseTileZ+c->local_player->pathing_entity.z/128:0;
 if(ready&&h8_take_run(&kind,&seconds,&ui)){
  extern int h8_set_ui(unsigned);h8_set_ui(ui);
  c->redraw_sidebar=c->redraw_chatback=true;
  c->orbit_camera_yaw=0;c->orbit_camera_pitch=256;
  h8_begin(x,z,0,256,world3d_draw_radius(),_Client.lowmem);
 }
 unsigned step;
 if(ready&&h8_action(&kind,&step)){
  if(kind==1){
   static const int dx[]={2,0,-2,0},dz[]={0,2,0,-2};
   int sx=c->local_player->pathing_entity.pathTileX[0],sz=c->local_player->pathing_entity.pathTileZ[0];
   bool accepted=client_try_move(c,sx,sz,sx+dx[step],sz+dz[step],0,0,0,0,0,0,true);
   h8_action_result(kind,step,sx+dx[step],sz+dz[step],accepted);
  }else if(kind==2){c->orbit_camera_yaw=(int)step*512;c->orbit_camera_pitch=256;h8_action_result(kind,step,c->orbit_camera_yaw,256,1);}
 }
 h8_end_tick(x,z,c->orbit_camera_yaw,c->orbit_camera_pitch,world3d_draw_radius(),ready);
 unsigned current=h8_is_running();
 if(previous_running&&!current&&ready){
  int sx=c->local_player->pathing_entity.x/128,sz=c->local_player->pathing_entity.z/128;
  client_try_move(c,sx,sz,sx,sz,0,0,0,0,0,0,true);
 }
 previous_running=current;
}
void client_update(Client *c) {
    ba_hardware_benchmark(c);
    if (c->error_started || c->error_loading || c->error_host) {
        return;
    }

    _Client.loop_cycle++;
    if (c->ingame) {
        client_update_game(c);
    } else {
        client_update_title(c);
    }
}

void handleChatMouseInput(Client *c, int mouseX, int mouseY) {
    (void)mouseX;
    int line = 0;
    for (int i = 0; i < 100; i++) {
        if (c->message_text[i][0] == '\0') {
            continue;
        }

        int rows = 1;
#ifdef VITA_NATIVE_WIDESCREEN
        rows = classic_chat_rows(c, i, _Custom.chat_era == 0 ? c->font_quill8 : c->font_plain12);
        if (!rows) continue;
#endif
        int type = c->message_type[i];
        int y = c->chat_scroll_offset + 70 + 4 - line * 14;
        if (y < -20) {
            break;
        }

        if (type == 0) {
            line += rows;
        }

        if ((type == 1 || type == 2) && (type == 1 || c->public_chat_setting == 0 || (c->public_chat_setting == 1 && client_is_friend(c, c->message_sender[i])))) {
            if (mouseY > y - 14 * rows && mouseY <= y && strcmp(c->message_sender[i], c->local_player->name) != 0) {
                if (c->rights) {
                    sprintf(c->menu_option[c->menu_size], "Report abuse @whi@%s", c->message_sender[i]);
                    c->menu_action[c->menu_size] = 34;
                    c->menu_size++;
                }

                sprintf(c->menu_option[c->menu_size], "Add ignore @whi@%s", c->message_sender[i]);
                c->menu_action[c->menu_size] = 436;
                c->menu_size++;
                sprintf(c->menu_option[c->menu_size], "Add friend @whi@%s", c->message_sender[i]);
                c->menu_action[c->menu_size] = 406;
                c->menu_size++;
            }

            line += rows;
        }

        if ((type == 3 || type == 7) && c->split_private_chat == 0 && (type == 7 || c->private_chat_setting == 0 || (c->private_chat_setting == 1 && client_is_friend(c, c->message_sender[i])))) {
            if (mouseY > y - 14 * rows && mouseY <= y) {
                if (c->rights) {
                    sprintf(c->menu_option[c->menu_size], "Report abuse @whi@%s", c->message_sender[i]);
                    c->menu_action[c->menu_size] = 34;
                    c->menu_size++;
                }

                sprintf(c->menu_option[c->menu_size], "Add ignore @whi@%s", c->message_sender[i]);
                c->menu_action[c->menu_size] = 436;
                c->menu_size++;
                sprintf(c->menu_option[c->menu_size], "Add friend @whi@%s", c->message_sender[i]);
                c->menu_action[c->menu_size] = 406;
                c->menu_size++;
            }

            line += rows;
        }

        if (type == 4 && (c->trade_chat_setting == 0 || (c->trade_chat_setting == 1 && client_is_friend(c, c->message_sender[i])))) {
            if (mouseY > y - 14 * rows && mouseY <= y) {
                sprintf(c->menu_option[c->menu_size], "Accept trade @whi@%s", c->message_sender[i]);
                c->menu_action[c->menu_size] = 903;
                c->menu_size++;
            }

            line += rows;
        }

        if ((type == 5 || type == 6) && c->split_private_chat == 0 && c->private_chat_setting < 2) {
            line += rows;
        }

        if (type == 8 && (c->trade_chat_setting == 0 || (c->trade_chat_setting == 1 && client_is_friend(c, c->message_sender[i])))) {
            if (mouseY > y - 14 * rows && mouseY <= y) {
                sprintf(c->menu_option[c->menu_size], "Accept duel @whi@%s", c->message_sender[i]);
                c->menu_action[c->menu_size] = 363;
                c->menu_size++;
            }

            line += rows;
        }
    }
}

void handleInterfaceInput(Client *c, Component *com, int mouseX, int mouseY, int x, int y, int scrollPosition) {
    if (com->type != 0 || !com->childId || com->hide || (mouseX < x || mouseY < y || mouseX > x + com->width || mouseY > y + com->height)) {
        return;
    }

    int children = com->childCount;
    for (int i = 0; i < children; i++) {
        int childX = com->childX[i] + x;
        int childY = com->childY[i] + y - scrollPosition;
        Component *child = _Component.instances[com->childId[i]];

        childX += child->x;
        childY += child->y;

        if ((child->overLayer >= 0 || child->overColour != 0) && mouseX >= childX && mouseY >= childY && mouseX < childX + child->width && mouseY < childY + child->height) {
            if (child->overLayer >= 0) {
                c->lastHoveredInterfaceId = child->overLayer;
            } else {
                c->lastHoveredInterfaceId = child->id;
            }
        }

        if (child->type == 0) {
            handleInterfaceInput(c, child, mouseX, mouseY, childX, childY, child->scrollPosition);

            if (child->scroll > child->height) {
                handleScrollInput(c, mouseX, mouseY, child->scroll, child->height, true, childX + child->width, childY, child);
            }
        } else if (child->type == 2) {
            int slot = 0;

            for (int row = 0; row < child->height; row++) {
                for (int col = 0; col < child->width; col++) {
                    int slotX = childX + col * (child->marginX + 32);
                    int slotY = childY + row * (child->marginY + 32);

                    if (slot < 20) {
                        slotX += child->invSlotOffsetX[slot];
                        slotY += child->invSlotOffsetY[slot];
                    }

                    if (mouseX < slotX || mouseY < slotY || mouseX >= slotX + 32 || mouseY >= slotY + 32) {
                        slot++;
                        continue;
                    }

                    c->hoveredSlot = slot;
                    c->hoveredSlotParentId = child->id;

                    if (child->invSlotObjId[slot] <= 0) {
                        slot++;
                        continue;
                    }

                    ObjType *obj = objtype_get(child->invSlotObjId[slot] - 1);

                    if (c->obj_selected == 1 && child->interactable) {
                        if (child->id != c->objSelectedInterface || slot != c->objSelectedSlot) {
                            sprintf(c->menu_option[c->menu_size], "Use %s with @lre@%s", c->objSelectedName, obj->name);
                            c->menu_action[c->menu_size] = 881;
                            c->menuParamA[c->menu_size] = obj->index;
                            c->menuParamB[c->menu_size] = slot;
                            c->menuParamC[c->menu_size] = child->id;
                            c->menu_size++;
                        }
                    } else if (c->spell_selected == 1 && child->interactable) {
                        if ((c->activeSpellFlags & 0x10) == 16) {
                            sprintf(c->menu_option[c->menu_size], "%s @lre@%s", c->spellCaption, obj->name);
                            c->menu_action[c->menu_size] = 391;
                            c->menuParamA[c->menu_size] = obj->index;
                            c->menuParamB[c->menu_size] = slot;
                            c->menuParamC[c->menu_size] = child->id;
                            c->menu_size++;
                        }
                    } else {
                        if (child->interactable) {
                            for (int op = 4; op >= 3; op--) {
                                if (obj->iop && obj->iop[op]) {
                                    sprintf(c->menu_option[c->menu_size], "%s @lre@%s", obj->iop[op], obj->name);
                                    if (op == 3) {
                                        c->menu_action[c->menu_size] = 478;
                                    } else if (op == 4) {
                                        c->menu_action[c->menu_size] = 347;
                                    }
                                    c->menuParamA[c->menu_size] = obj->index;
                                    c->menuParamB[c->menu_size] = slot;
                                    c->menuParamC[c->menu_size] = child->id;
                                    c->menu_size++;
                                } else if (op == 4) {
                                    sprintf(c->menu_option[c->menu_size], "Drop @lre@%s", obj->name);
                                    c->menu_action[c->menu_size] = 347;
                                    c->menuParamA[c->menu_size] = obj->index;
                                    c->menuParamB[c->menu_size] = slot;
                                    c->menuParamC[c->menu_size] = child->id;
                                    c->menu_size++;
                                }
                            }
                        }

                        if (child->usable) {
                            sprintf(c->menu_option[c->menu_size], "Use @lre@%s", obj->name);
                            c->menu_action[c->menu_size] = 188;
                            c->menuParamA[c->menu_size] = obj->index;
                            c->menuParamB[c->menu_size] = slot;
                            c->menuParamC[c->menu_size] = child->id;
                            c->menu_size++;
                        }

                        if (child->interactable && obj->iop) {
                            for (int op = 2; op >= 0; op--) {
                                if (obj->iop[op]) {
                                    sprintf(c->menu_option[c->menu_size], "%s @lre@%s", obj->iop[op], obj->name);
                                    if (op == 0) {
                                        c->menu_action[c->menu_size] = 405;
                                    } else if (op == 1) {
                                        c->menu_action[c->menu_size] = 38;
                                    } else if (op == 2) {
                                        c->menu_action[c->menu_size] = 422;
                                    }
                                    c->menuParamA[c->menu_size] = obj->index;
                                    c->menuParamB[c->menu_size] = slot;
                                    c->menuParamC[c->menu_size] = child->id;
                                    c->menu_size++;
                                }
                            }
                        }

                        if (child->iops) {
                            for (int op = 4; op >= 0; op--) {
                                if (child->iops[op]) {
                                    sprintf(c->menu_option[c->menu_size], "%s @lre@%s", child->iops[op], obj->name);
                                    if (op == 0) {
                                        c->menu_action[c->menu_size] = 602;
                                    } else if (op == 1) {
                                        c->menu_action[c->menu_size] = 596;
                                    } else if (op == 2) {
                                        c->menu_action[c->menu_size] = 22;
                                    } else if (op == 3) {
                                        c->menu_action[c->menu_size] = 892;
                                    } else if (op == 4) {
                                        c->menu_action[c->menu_size] = 415;
                                    }
                                    c->menuParamA[c->menu_size] = obj->index;
                                    c->menuParamB[c->menu_size] = slot;
                                    c->menuParamC[c->menu_size] = child->id;
                                    c->menu_size++;
                                }
                            }
                        }

                        sprintf(c->menu_option[c->menu_size], "Examine @lre@%s", obj->name);
                        // TODO
                        // if (c->show_debug) {
                        // 	c->menu_option[c->menu_size] += "@whi@ (" + (obj.index) + ")";
                        // }
                        c->menu_action[c->menu_size] = 1773;
                        c->menuParamA[c->menu_size] = obj->index;
                        c->menuParamC[c->menu_size] = child->invSlotObjCount[slot];
                        c->menu_size++;
                    }

                    slot++;
                }
            }
        } else if (mouseX >= childX && mouseY >= childY && mouseX < childX + child->width && mouseY < childY + child->height) {
            if (child->buttonType == BUTTON_OK) {
                bool override = false;
                if (child->clientCode != 0) {
                    override = handleSocialMenuOption(c, child);
                }

                if (!override) {
                    strcpy(c->menu_option[c->menu_size], child->option);
                    c->menu_action[c->menu_size] = 951;
                    c->menuParamC[c->menu_size] = child->id;
                    c->menu_size++;
                }
            } else if (child->buttonType == BUTTON_TARGET && c->spell_selected == 0) {
                char *prefix = child->actionVerb;
                bool _free = false;
                if (indexof_chr(prefix, ' ') != -1) {
                    prefix = substring(prefix, 0, indexof_chr(prefix, ' '));
                    _free = true;
                }

                sprintf(c->menu_option[c->menu_size], "%s @gre@%s", prefix, child->action);
                if (_free) {
                    free(prefix);
                }
                c->menu_action[c->menu_size] = 930;
                c->menuParamC[c->menu_size] = child->id;
                c->menu_size++;
            } else if (child->buttonType == BUTTON_CLOSE) {
                strcpy(c->menu_option[c->menu_size], "Close");
                c->menu_action[c->menu_size] = 947;
                c->menuParamC[c->menu_size] = child->id;
                c->menu_size++;
            } else if (child->buttonType == BUTTON_TOGGLE) {
                strcpy(c->menu_option[c->menu_size], child->option);
                c->menu_action[c->menu_size] = 465;
                c->menuParamC[c->menu_size] = child->id;
                c->menu_size++;
            } else if (child->buttonType == BUTTON_SELECT) {
                strcpy(c->menu_option[c->menu_size], child->option);
                c->menu_action[c->menu_size] = 960;
                c->menuParamC[c->menu_size] = child->id;
                c->menu_size++;
            } else if (child->buttonType == BUTTON_CONTINUE && !c->pressed_continue_option) {
                strcpy(c->menu_option[c->menu_size], child->option);
                c->menu_action[c->menu_size] = 44;
                c->menuParamC[c->menu_size] = child->id;
                c->menu_size++;
            }
        }
    }
}

bool handleSocialMenuOption(Client *c, Component *component) {
    int type = component->clientCode;
    if (type >= 1 && type <= 200) {
        if (type >= 101) {
            type -= 101;
        } else {
            type--;
        }

        sprintf(c->menu_option[c->menu_size], "Remove @whi@%s", c->friendName[type]);
        c->menu_action[c->menu_size] = 557;
        c->menu_size++;

        sprintf(c->menu_option[c->menu_size], "Message @whi@%s", c->friendName[type]);
        c->menu_action[c->menu_size] = 679;
        c->menu_size++;
        return true;
    } else if (type >= 401 && type <= 500) {
        sprintf(c->menu_option[c->menu_size], "Remove @whi@%s", component->text);
        c->menu_action[c->menu_size] = 556;
        c->menu_size++;
        return true;
    } else {
        return false;
    }
}

void handlePrivateChatInput(Client *c, int mouse_x, int mouse_y) {
    if (c->split_private_chat == 0) {
        return;
    }

    int lineOffset = 0;
    if (c->system_update_timer != 0) {
        lineOffset = 1;
    }

    for (int i = 0; i < 100; i++) {
        if (c->message_text[i][0]) {
            int type = c->message_type[i];
            if ((type == 3 || type == 7) && (type == 7 || c->private_chat_setting == 0 || (c->private_chat_setting == 1 && client_is_friend(c, c->message_sender[i])))) {
                int y = 329 - lineOffset * 13;
                // super.mouseX was used here for no reason when they are both passed to func
                if (mouse_x > 8 && mouse_x < 520 && mouse_y - 11 > y - 10 && mouse_y - 11 <= y + 3) {
                    if (c->rights) {
                        sprintf(c->menu_option[c->menu_size], "Report abuse @whi@%s", c->message_sender[i]);
                        c->menu_action[c->menu_size] = 2034;
                        c->menu_size++;
                    }
                    sprintf(c->menu_option[c->menu_size], "Add ignore @whi@%s", c->message_sender[i]);
                    c->menu_action[c->menu_size] = 2436;
                    c->menu_size++;
                    sprintf(c->menu_option[c->menu_size], "Add friend @whi@%s", c->message_sender[i]);
                    c->menu_action[c->menu_size] = 2406;
                    c->menu_size++;
                }

                lineOffset++;
                if (lineOffset >= 5) {
                    return;
                }
            }

            if ((type == 5 || type == 6) && c->private_chat_setting < 2) {
                lineOffset++;
                if (lineOffset >= 5) {
                    return;
                }
            }
        }
    }
}

static const char *getCombatLevelColorTag(int viewerLevel, int otherLevel) {
    int diff = viewerLevel - otherLevel;
    if (diff < -9) {
        return "@red@";
    } else if (diff < -6) {
        return "@or3@";
    } else if (diff < -3) {
        return "@or2@";
    } else if (diff < 0) {
        return "@or1@";
    } else if (diff > 9) {
        return "@gre@";
    } else if (diff > 6) {
        return "@gr3@";
    } else if (diff > 3) {
        return "@gr2@";
    } else if (diff > 0) {
        return "@gr1@";
    } else {
        return "@yel@";
    }
}

void addNpcOptions(Client *cl, NpcType *npc, int a, int b, int c) {
    if (cl->menu_size >= 400) {
        return;
    }

    char tooltip[MAX_STR];
    if (npc->vislevel != 0) {
        char tmp[HALF_STR];
        strcpy(tmp, npc->name);
        sprintf(tooltip, "%s%s (level-%d)", tmp, getCombatLevelColorTag(cl->local_player->combatLevel, npc->vislevel), npc->vislevel);
    } else {
        strcpy(tooltip, npc->name);
    }

    if (cl->obj_selected == 1) {
        sprintf(cl->menu_option[cl->menu_size], "Use %s with @yel@%s", cl->objSelectedName, tooltip);
        cl->menu_action[cl->menu_size] = 900;
        cl->menuParamA[cl->menu_size] = a;
        cl->menuParamB[cl->menu_size] = b;
        cl->menuParamC[cl->menu_size] = c;
        cl->menu_size++;
    } else if (cl->spell_selected != 1) {
        int type;
        if (npc->op) {
            for (type = 4; type >= 0; type--) {
                if (npc->op[type] && platform_strcasecmp(npc->op[type], "attack") != 0) {
                    sprintf(cl->menu_option[cl->menu_size], "%s @yel@%s", npc->op[type], tooltip);

                    if (type == 0) {
                        cl->menu_action[cl->menu_size] = 728;
                    } else if (type == 1) {
                        cl->menu_action[cl->menu_size] = 542;
                    } else if (type == 2) {
                        cl->menu_action[cl->menu_size] = 6;
                    } else if (type == 3) {
                        cl->menu_action[cl->menu_size] = 963;
                    } else if (type == 4) {
                        cl->menu_action[cl->menu_size] = 245;
                    }

                    cl->menuParamA[cl->menu_size] = a;
                    cl->menuParamB[cl->menu_size] = b;
                    cl->menuParamC[cl->menu_size] = c;
                    cl->menu_size++;
                }
            }
        }

        if (npc->op) {
            for (type = 4; type >= 0; type--) {
                if (npc->op[type] && platform_strcasecmp(npc->op[type], "attack") == 0) {
                    int action = 0;
                    if (npc->vislevel > cl->local_player->combatLevel) {
                        action = 2000;
                    }

                    sprintf(cl->menu_option[cl->menu_size], "%s @yel@%s", npc->op[type], tooltip);

                    if (type == 0) {
                        cl->menu_action[cl->menu_size] = action + 728;
                    } else if (type == 1) {
                        cl->menu_action[cl->menu_size] = action + 542;
                    } else if (type == 2) {
                        cl->menu_action[cl->menu_size] = action + 6;
                    } else if (type == 3) {
                        cl->menu_action[cl->menu_size] = action + 963;
                    } else if (type == 4) {
                        cl->menu_action[cl->menu_size] = action + 245;
                    }

                    cl->menuParamA[cl->menu_size] = a;
                    cl->menuParamB[cl->menu_size] = b;
                    cl->menuParamC[cl->menu_size] = c;
                    cl->menu_size++;
                }
            }
        }

        sprintf(cl->menu_option[cl->menu_size], "Examine @yel@%s", tooltip);
        // TODO:
        // if (cl->show_debug) {
        // 	cl->menu_option[cl->menu_size] += "@whi@ (" + (npc.index) + ")";
        // }
        cl->menu_action[cl->menu_size] = 1607;
        cl->menuParamA[cl->menu_size] = a;
        cl->menuParamB[cl->menu_size] = b;
        cl->menuParamC[cl->menu_size] = c;
        cl->menu_size++;
    } else if ((cl->activeSpellFlags & 0x2) == 2) {
        sprintf(cl->menu_option[cl->menu_size], "%s @yel@%s", cl->spellCaption, tooltip);
        cl->menu_action[cl->menu_size] = 265;
        cl->menuParamA[cl->menu_size] = a;
        cl->menuParamB[cl->menu_size] = b;
        cl->menuParamC[cl->menu_size] = c;
        cl->menu_size++;
    }
}

void addPlayerOptions(Client *cl, PlayerEntity *player, int a, int b, int c) {
    if (player == cl->local_player || cl->menu_size >= 400) {
        return;
    }

    char tooltip[MAX_STR];
    sprintf(tooltip, "%s%s (level-%d)", player->name, getCombatLevelColorTag(cl->local_player->combatLevel, player->combatLevel), player->combatLevel);
    if (cl->obj_selected == 1) {
        sprintf(cl->menu_option[cl->menu_size], "Use %s with @whi@%s", cl->objSelectedName, tooltip);
        cl->menu_action[cl->menu_size] = 367;
        cl->menuParamA[cl->menu_size] = a;
        cl->menuParamB[cl->menu_size] = b;
        cl->menuParamC[cl->menu_size] = c;
        cl->menu_size++;
    } else if (cl->spell_selected != 1) {
        sprintf(cl->menu_option[cl->menu_size], "Follow @whi@%s", tooltip);
        cl->menu_action[cl->menu_size] = 1544;
        cl->menuParamA[cl->menu_size] = a;
        cl->menuParamB[cl->menu_size] = b;
        cl->menuParamC[cl->menu_size] = c;
        cl->menu_size++;

        if (cl->overrideChat == 0) {
            sprintf(cl->menu_option[cl->menu_size], "Trade with @whi@%s", tooltip);
            cl->menu_action[cl->menu_size] = 1373;
            cl->menuParamA[cl->menu_size] = a;
            cl->menuParamB[cl->menu_size] = b;
            cl->menuParamC[cl->menu_size] = c;
            cl->menu_size++;
        }

        if (cl->wildernessLevel > 0) {
            sprintf(cl->menu_option[cl->menu_size], "Attack @whi@%s", tooltip);
            if (cl->local_player->combatLevel >= player->combatLevel) {
                cl->menu_action[cl->menu_size] = 151;
            } else {
                cl->menu_action[cl->menu_size] = 2151;
            }
            cl->menuParamA[cl->menu_size] = a;
            cl->menuParamB[cl->menu_size] = b;
            cl->menuParamC[cl->menu_size] = c;
            cl->menu_size++;
        }

        if (cl->worldLocationState == 1) {
            sprintf(cl->menu_option[cl->menu_size], "Fight @whi@%s", tooltip);
            cl->menu_action[cl->menu_size] = 151;
            cl->menuParamA[cl->menu_size] = a;
            cl->menuParamB[cl->menu_size] = b;
            cl->menuParamC[cl->menu_size] = c;
            cl->menu_size++;
        }

        if (cl->worldLocationState == 2) {
            sprintf(cl->menu_option[cl->menu_size], "Duel-with @whi@%s", tooltip);
            cl->menu_action[cl->menu_size] = 1101;
            cl->menuParamA[cl->menu_size] = a;
            cl->menuParamB[cl->menu_size] = b;
            cl->menuParamC[cl->menu_size] = c;
            cl->menu_size++;
        }
    } else if ((cl->activeSpellFlags & 0x8) == 8) {
        sprintf(cl->menu_option[cl->menu_size], "%s @whi@%s", cl->spellCaption, tooltip);
        cl->menu_action[cl->menu_size] = 651;
        cl->menuParamA[cl->menu_size] = a;
        cl->menuParamB[cl->menu_size] = b;
        cl->menuParamC[cl->menu_size] = c;
        cl->menu_size++;
    }

    for (int i = 0; i < cl->menu_size; i++) {
        if (cl->menu_action[i] == 660) {
            sprintf(cl->menu_option[i], "Walk here @whi@%s", tooltip);
            return;
        }
    }
}

void handleViewportOptions(Client *c) {
    if (c->obj_selected == 0 && c->spell_selected == 0) {
        strcpy(c->menu_option[c->menu_size], "Walk here");
        c->menu_action[c->menu_size] = 660;
        c->menuParamB[c->menu_size] = c->shell->mouse_x;
        c->menuParamC[c->menu_size] = c->shell->mouse_y;
        c->menu_size++;
    }

    int lastBitset = -1;
    for (int picked = 0; picked < _Model.picked_count; picked++) {
        int bitset = _Model.picked_bitsets[picked];
        int x = bitset & 0x7f;
        int z = bitset >> 7 & 0x7f;
        int entityType = bitset >> 29 & 0x3;
        int typeId = bitset >> 14 & 0x7fff;

        if (bitset == lastBitset) {
            continue;
        }

        lastBitset = bitset;

        if (entityType == 2 && world3d_get_info(c->scene, c->currentLevel, x, z, bitset) >= 0) {
            LocType *loc = loctype_get(typeId);
            if (c->obj_selected == 1) {
                sprintf(c->menu_option[c->menu_size], "Use %s with @cya@%s", c->objSelectedName, loc->name);
                c->menu_action[c->menu_size] = 450;
                c->menuParamA[c->menu_size] = bitset;
                c->menuParamB[c->menu_size] = x;
                c->menuParamC[c->menu_size] = z;
                c->menu_size++;
            } else if (c->spell_selected != 1) {
                if (loc->op) {
                    for (int op = 4; op >= 0; op--) {
                        if (loc->op[op]) {
                            sprintf(c->menu_option[c->menu_size], "%s @cya@%s", loc->op[op], loc->name);
                            if (op == 0) {
                                c->menu_action[c->menu_size] = 285;
                            }

                            if (op == 1) {
                                c->menu_action[c->menu_size] = 504;
                            }

                            if (op == 2) {
                                c->menu_action[c->menu_size] = 364;
                            }

                            if (op == 3) {
                                c->menu_action[c->menu_size] = 581;
                            }

                            if (op == 4) {
                                c->menu_action[c->menu_size] = 1501;
                            }

                            c->menuParamA[c->menu_size] = bitset;
                            c->menuParamB[c->menu_size] = x;
                            c->menuParamC[c->menu_size] = z;
                            c->menu_size++;
                        }
                    }
                }

                sprintf(c->menu_option[c->menu_size], "Examine @cya@%s", loc->name);
                // TODO
                // if (c->show_debug) {
                // 	c->menu_option[c->menu_size] += "@whi@ (" + (loc.index) + ")";
                // }
                c->menu_action[c->menu_size] = 1175;
                c->menuParamA[c->menu_size] = bitset;
                c->menuParamB[c->menu_size] = x;
                c->menuParamC[c->menu_size] = z;
                c->menu_size++;
            } else if ((c->activeSpellFlags & 0x4) == 4) {
                sprintf(c->menu_option[c->menu_size], "%s @cya@%s", c->spellCaption, loc->name);
                c->menu_action[c->menu_size] = 55;
                c->menuParamA[c->menu_size] = bitset;
                c->menuParamB[c->menu_size] = x;
                c->menuParamC[c->menu_size] = z;
                c->menu_size++;
            }
        }

        if (entityType == 1) {
            NpcEntity *npc = c->npcs[typeId];
            if (npc->type->size == 1 && (npc->pathing_entity.x & 0x7f) == 64 && (npc->pathing_entity.z & 0x7f) == 64) {
                for (int i = 0; i < c->npc_count; i++) {
                    NpcEntity *other = c->npcs[c->npc_ids[i]];

                    if (other && other != npc && other->type->size == 1 && other->pathing_entity.x == npc->pathing_entity.x && other->pathing_entity.z == npc->pathing_entity.z) {
                        addNpcOptions(c, other->type, c->npc_ids[i], x, z);
                    }
                }
            }

            addNpcOptions(c, npc->type, typeId, x, z);
        }

        if (entityType == 0) {
            PlayerEntity *player = c->players[typeId];
            if ((player->pathing_entity.x & 0x7f) == 64 && (player->pathing_entity.z & 0x7f) == 64) {
                for (int i = 0; i < c->npc_count; i++) {
                    NpcEntity *other = c->npcs[c->npc_ids[i]];

                    if (other && other->type->size == 1 && other->pathing_entity.x == player->pathing_entity.x && other->pathing_entity.z == player->pathing_entity.z) {
                        addNpcOptions(c, other->type, c->npc_ids[i], x, z);
                    }
                }

                for (int i = 0; i < c->player_count; i++) {
                    PlayerEntity *other = c->players[c->player_ids[i]];

                    if (other && other != player && other->pathing_entity.x == player->pathing_entity.x && other->pathing_entity.z == player->pathing_entity.z) {
                        addPlayerOptions(c, other, c->player_ids[i], x, z);
                    }
                }
            }

            addPlayerOptions(c, player, typeId, x, z);
        }

        if (entityType == 3) {
            LinkList *objs = c->level_obj_stacks[c->currentLevel][x][z];
            if (!objs) {
                continue;
            }

            for (ObjStackEntity *obj = (ObjStackEntity *)linklist_tail(objs); obj; obj = (ObjStackEntity *)linklist_prev(objs)) {
                ObjType *type = objtype_get(obj->index);
                if (c->obj_selected == 1) {
                    sprintf(c->menu_option[c->menu_size], "Use %s with @lre@%s", c->objSelectedName, type->name);
                    c->menu_action[c->menu_size] = 217;
                    c->menuParamA[c->menu_size] = obj->index;
                    c->menuParamB[c->menu_size] = x;
                    c->menuParamC[c->menu_size] = z;
                    c->menu_size++;
                } else if (c->spell_selected != 1) {
                    for (int op = 4; op >= 0; op--) {
                        if (type->op && type->op[op]) {
                            sprintf(c->menu_option[c->menu_size], "%s @lre@%s", type->op[op], type->name);
                            if (op == 0) {
                                c->menu_action[c->menu_size] = 224;
                            }

                            if (op == 1) {
                                c->menu_action[c->menu_size] = 993;
                            }

                            if (op == 2) {
                                c->menu_action[c->menu_size] = 99;
                            }

                            if (op == 3) {
                                c->menu_action[c->menu_size] = 746;
                            }

                            if (op == 4) {
                                c->menu_action[c->menu_size] = 877;
                            }

                            c->menuParamA[c->menu_size] = obj->index;
                            c->menuParamB[c->menu_size] = x;
                            c->menuParamC[c->menu_size] = z;
                            c->menu_size++;
                        } else if (op == 2) {
                            sprintf(c->menu_option[c->menu_size], "Take @lre@%s", type->name);
                            c->menu_action[c->menu_size] = 99;
                            c->menuParamA[c->menu_size] = obj->index;
                            c->menuParamB[c->menu_size] = x;
                            c->menuParamC[c->menu_size] = z;
                            c->menu_size++;
                        }
                    }

                    sprintf(c->menu_option[c->menu_size], "Examine @lre@%s", type->name);
                    // TODO
                    // if (c->show_debug) {
                    // 	c->menu_option[c->menu_size] += "@whi@ (" + (obj.index) + ")";
                    // }
                    c->menu_action[c->menu_size] = 1102;
                    c->menuParamA[c->menu_size] = obj->index;
                    c->menuParamB[c->menu_size] = x;
                    c->menuParamC[c->menu_size] = z;
                    c->menu_size++;
                } else if ((c->activeSpellFlags & 0x1) == 1) {
                    sprintf(c->menu_option[c->menu_size], "%s @lre@%s", c->spellCaption, type->name);
                    c->menu_action[c->menu_size] = 965;
                    c->menuParamA[c->menu_size] = obj->index;
                    c->menuParamB[c->menu_size] = x;
                    c->menuParamC[c->menu_size] = z;
                    c->menu_size++;
                }
            }
        }
    }
}

void client_handle_input(Client *c) {
    BA_PROFILE_SCOPE(BA_INPUT);
    if (c->obj_drag_area != 0) {
        return;
    }

    strcpy(c->menu_option[0], "Cancel");
    c->menu_action[0] = 1252;
    c->menu_size = 1;
    handlePrivateChatInput(c, c->shell->mouse_x, c->shell->mouse_y);
    c->lastHoveredInterfaceId = 0;

    if (((ve_enabled()?ve_world_input(c->shell->mouse_x,c->shell->mouse_y):game_world_input(c->shell->mouse_x,c->shell->mouse_y))&&!lc_game_covers(c->shell->mouse_x,c->shell->mouse_y))) {
        if (c->viewport_interface_id == -1) {
            handleViewportOptions(c);
        } else {
            handleInterfaceInput(c, _Component.instances[c->viewport_interface_id], c->shell->mouse_x, c->shell->mouse_y, (ve_enabled()?224:8+GAME_MODAL_X)+client_modal_offset(c,c->viewport_interface_id,ve_enabled()), (ve_enabled()?0:11)+client_modal_y(c,c->viewport_interface_id,ve_enabled()), 0);
        }
    }

    if (c->lastHoveredInterfaceId != c->viewportHoveredInterfaceIndex) {
        c->viewportHoveredInterfaceIndex = c->lastHoveredInterfaceId;
        ve_script_dirty = 1;
    }

    c->lastHoveredInterfaceId = 0;

    if (c->shell->mouse_x > ui474_side_x() && c->shell->mouse_y > ui474_side_y() && c->shell->mouse_x < (ui474_side_x()+190) && c->shell->mouse_y < (ui474_side_y()+261)) {
        if (c->sidebar_interface_id != -1) {
            handleInterfaceInput(c, _Component.instances[c->sidebar_interface_id], c->shell->mouse_x, c->shell->mouse_y, ui474_side_x(), ui474_side_y(), 0);
        } else if (c->tab_interface_id[c->selected_tab] != -1) {
            if (!ui474_menu(c,c->shell->mouse_x,c->shell->mouse_y)) handleInterfaceInput(c, _Component.instances[c->tab_interface_id[c->selected_tab]], c->shell->mouse_x, c->shell->mouse_y, ui474_side_x(), ui474_side_y(), 0);
        }
    }

    if (c->lastHoveredInterfaceId != c->sidebarHoveredInterfaceIndex) {
        c->redraw_sidebar = true;
        c->sidebarHoveredInterfaceIndex = c->lastHoveredInterfaceId;
    }

    c->lastHoveredInterfaceId = 0;

    if ((!ve_enabled()||!ve_chat_hidden(c)) && (!lc_game_altered()||lc_contains(lc_game_geometry(LC_CHAT),c->shell->mouse_x,c->shell->mouse_y,true)) && c->shell->mouse_x > 22 && c->shell->mouse_y > (ve_enabled()?ve_chat_text_y():lc_y(LC_CHAT,UI_BOTTOM(375))) && c->shell->mouse_x < 22 + lc_chat_text_right(CHAT_TEXT_RIGHT) && c->shell->mouse_y < lc_y(LC_CHAT,UI_BOTTOM(471))) {
        if (c->chat_interface_id == -1) {
            handleChatMouseInput(c, c->shell->mouse_x - (ve_enabled()?ve_chat_text_x():22), c->shell->mouse_y - (ve_enabled()?ve_chat_text_y():lc_y(LC_CHAT,UI_BOTTOM(375))));
        } else {
            handleInterfaceInput(c, _Component.instances[c->chat_interface_id], c->shell->mouse_x, c->shell->mouse_y, 22, (ve_enabled()?ve_chat_text_y():lc_y(LC_CHAT,UI_BOTTOM(375))), 0);
        }
    }

    if (c->chat_interface_id != -1 && c->lastHoveredInterfaceId != c->chatHoveredInterfaceIndex) {
        c->redraw_chatback = true;
        c->chatHoveredInterfaceIndex = c->lastHoveredInterfaceId;
    }

    bool done = false;
    while (!done) {
        done = true;

        for (int i = 0; i < c->menu_size - 1; i++) {
            if (c->menu_action[i] < 1000 && c->menu_action[i + 1] > 1000) {
                char tmp0[MAX_STR];
                strcpy(tmp0, c->menu_option[i]);
                strcpy(c->menu_option[i], c->menu_option[i + 1]);
                strcpy(c->menu_option[i + 1], tmp0);

                int tmp1 = c->menu_action[i];
                c->menu_action[i] = c->menu_action[i + 1];
                c->menu_action[i + 1] = tmp1;

                int tmp2 = c->menuParamB[i];
                c->menuParamB[i] = c->menuParamB[i + 1];
                c->menuParamB[i + 1] = tmp2;

                int tmp3 = c->menuParamC[i];
                c->menuParamC[i] = c->menuParamC[i + 1];
                c->menuParamC[i + 1] = tmp3;

                int tmp4 = c->menuParamA[i];
                c->menuParamA[i] = c->menuParamA[i + 1];
                c->menuParamA[i + 1] = tmp4;

                done = false;
            }
        }
    }
}

bool handleInterfaceAction(Client *c, Component *com) {
    int clientCode = com->clientCode;
    if (clientCode == 201) {
        c->redraw_chatback = true;
        c->chatback_input_open = false;
        c->show_social_input = true;
        c->social_input[0] = '\0';
        c->social_action = 1;
        strcpy(c->social_message, "Enter name of friend to add to list");
    }

    if (clientCode == 202) {
        c->redraw_chatback = true;
        c->chatback_input_open = false;
        c->show_social_input = true;
        c->social_input[0] = '\0';
        c->social_action = 2;
        strcpy(c->social_message, "Enter name of friend to delete from list");
    }

    if (clientCode == 205) {
        c->idle_timeout = 250;
        return true;
    }

    if (clientCode == 501) {
        c->redraw_chatback = true;
        c->chatback_input_open = false;
        c->show_social_input = true;
        c->social_input[0] = '\0';
        c->social_action = 4;
        strcpy(c->social_message, "Enter name of player to add to list");
    }

    if (clientCode == 502) {
        c->redraw_chatback = true;
        c->chatback_input_open = false;
        c->show_social_input = true;
        c->social_input[0] = '\0';
        c->social_action = 5;
        strcpy(c->social_message, "Enter name of player to delete from list");
    }

    if (clientCode >= 300 && clientCode <= 313) {
        int part = (clientCode - 300) / 2;
        int direction = clientCode & 0x1;
        int kit = c->designIdentikits[part];

        if (kit != -1) {
            while (true) {
                if (direction == 0) {
                    kit--;
                    if (kit < 0) {
                        kit = _IdkType.count - 1;
                    }
                }

                if (direction == 1) {
                    kit++;
                    if (kit >= _IdkType.count) {
                        kit = 0;
                    }
                }

                if (!_IdkType.instances[kit]->disable && _IdkType.instances[kit]->type == part + (c->design_gender_male ? 0 : 7)) {
                    c->designIdentikits[part] = kit;
                    c->update_design_model = true;
                    break;
                }
            }
        }
    }

    if (clientCode >= 314 && clientCode <= 323) {
        int part = (clientCode - 314) / 2;
        int direction = clientCode & 0x1;
        int color = c->design_colors[part];

        if (direction == 0) {
            color--;
            if (color < 0) {
                color = DESIGN_BODY_COLOR_LENGTH[part] - 1;
            }
        }

        if (direction == 1) {
            color++;
            if (color >= DESIGN_BODY_COLOR_LENGTH[part]) {
                color = 0;
            }
        }

        c->design_colors[part] = color;
        c->update_design_model = true;
    }

    if (clientCode == 324 && !c->design_gender_male) {
        c->design_gender_male = true;
        client_validate_character_design(c);
    }

    if (clientCode == 325 && c->design_gender_male) {
        c->design_gender_male = false;
        client_validate_character_design(c);
    }

    if (clientCode == 326) {
        // IF_PLAYERDESIGN
        p1isaac(c->out, 52);
        p1(c->out, c->design_gender_male ? 0 : 1);
        for (int i = 0; i < 7; i++) {
            p1(c->out, c->designIdentikits[i]);
        }
        for (int i = 0; i < 5; i++) {
            p1(c->out, c->design_colors[i]);
        }
        return true;
    }

    if (clientCode == 613) {
        c->reportAbuseMuteOption = !c->reportAbuseMuteOption;
    }

    if (clientCode >= 601 && clientCode <= 612) {
        closeInterfaces(c);

        if (strlen(c->reportAbuseInput) > 0) {
            // BUG_REPORT
            p1isaac(c->out, 190);
            p8(c->out, jstring_to_base37(c->reportAbuseInput));
            p1(c->out, clientCode - 601);
            p1(c->out, c->reportAbuseMuteOption ? 1 : 0);
        }
    }

    return false;
}

void handleScrollInput(Client *c, int mouseX, int mouseY, int scrollableHeight, int height, bool redraw, int left, int top, Component *component) {
    client_handle_scroll_input(c,mouseX,mouseY,scrollableHeight,height,redraw,left,top,component);
}

void showContextMenu(Client *c) {
    int width = stringWidth(c->font_bold12, "Choose Option");
    int maxWidth;
    for (int i = 0; i < c->menu_size; i++) {
        maxWidth = stringWidth(c->font_bold12, c->menu_option[i]);
        if (maxWidth > width) {
            width = maxWidth;
        }
    }
    width += 8;

    int height = c->menu_size * 15 + 22;

    int x;
    int y;
    if (ve_enabled()?ve_world_input(c->shell->mouse_click_x,c->shell->mouse_click_y):game_world_input(c->shell->mouse_click_x,c->shell->mouse_click_y)) {
        x = c->shell->mouse_click_x - width / 2 - (ve_enabled()?0:8);
        if (x + width > (ve_enabled()?SCREEN_WIDTH:GAME_MENU_WIDTH)) {
            x = (ve_enabled()?SCREEN_WIDTH:GAME_MENU_WIDTH) - width;
        } else if (x < 0) {
            x = 0;
        }

        y = c->shell->mouse_click_y - (ve_enabled()?0:11);
        if (y + height > (ve_enabled()?SCREEN_HEIGHT:GAME_MENU_HEIGHT)) {
            y = (ve_enabled()?SCREEN_HEIGHT:GAME_MENU_HEIGHT) - height;
        } else if (y < 0) {
            y = 0;
        }

        c->menu_visible = true;
        c->menu_area = 0;
        c->menu_x = x;
        c->menu_y = y;
        c->menu_width = width;
        c->menu_height = c->menu_size * 15 + 22;
    }
    if (c->shell->mouse_click_x > ui474_side_x() && c->shell->mouse_click_y > ui474_side_y() && c->shell->mouse_click_x < (ui474_side_x()+190) && c->shell->mouse_click_y < (ui474_side_y()+261)) {
        x = c->shell->mouse_click_x - width / 2 - ui474_side_x();
        if (x < 0) {
            x = 0;
        } else if (x + width > 190) {
            x = 190 - width;
        }

        y = c->shell->mouse_click_y - ui474_side_y();
        if (y < 0) {
            y = 0;
        } else if (y + height > 261) {
            y = 261 - height;
        }

        c->menu_visible = true;
        c->menu_area = 1;
        c->menu_x = x;
        c->menu_y = y;
        c->menu_width = width;
        c->menu_height = c->menu_size * 15 + 22;
    }
    if ((!lc_game_altered()||lc_contains(lc_game_geometry(LC_CHAT),c->shell->mouse_click_x,c->shell->mouse_click_y,true)) && c->shell->mouse_click_x > 22 && c->shell->mouse_click_y > (ve_enabled()?ve_chat_text_y():lc_y(LC_CHAT,UI_BOTTOM(375))) && c->shell->mouse_click_x < 22 + CHAT_WIDTH && c->shell->mouse_click_y < lc_y(LC_CHAT,UI_BOTTOM(471))) {
        x = c->shell->mouse_click_x - width / 2 - 22;
        if (x < 0) {
            x = 0;
        } else if (x + width > CHAT_WIDTH) {
            x = CHAT_WIDTH - width;
        }

        y = c->shell->mouse_click_y - (ve_enabled()?ve_chat_text_y():lc_y(LC_CHAT,UI_BOTTOM(375)));
        if (y < 0) {
            y = 0;
        } else if (y + height > 96) {
            y = 96 - height;
        }

        c->menu_visible = true;
        c->menu_area = 2;
        c->menu_x = x;
        c->menu_y = y;
        c->menu_width = width;
        c->menu_height = c->menu_size * 15 + 22;
    }
#ifdef __vita__
    if (c->menu_visible && c->menu_size > 0) {
        c->vita_menu_active = true;
        c->vita_menu_selected = c->menu_size - 1; // first displayed row
        c->vita_menu_confirm = c->vita_menu_cancel = false;
        int ox = c->menu_area == 0 ? (ve_enabled()?0:8) : c->menu_area == 1 ? ui474_side_x() : 22;
        int oy = c->menu_area == 0 ? (ve_enabled()?0:11) : c->menu_area == 1 ? ui474_side_y() : (ve_enabled()?ve_chat_text_y():lc_y(LC_CHAT,UI_BOTTOM(375)));
        int mx = c->shell->mouse_x - ox, my = c->shell->mouse_y - oy;
        for (int i = 0; i < c->menu_size; i++) {
            int row_y = c->menu_y + (c->menu_size - 1 - i) * 15 + 31;
            if (mx > c->menu_x && mx < c->menu_x + c->menu_width &&
                my > row_y - 13 && my < row_y + 3) c->vita_menu_selected = i;
        }
    }
#endif

}

bool isAddFriendOption(Client *c, int option) {
    if (option < 0) {
        return false;
    }

    int action = c->menu_action[option];
    if (action >= 2000) {
        action -= 2000;
    }
    return action == 406;
}

void updateMergeLocs(Client *c) {
    if (c->scene_state == 2) {
        for (LocMergeEntity *loc = (LocMergeEntity *)linklist_head(c->merged_locations); loc; loc = (LocMergeEntity *)linklist_next(c->merged_locations)) {
            if (_Client.loop_cycle >= loc->lastCycle) {
                addLoc(c, loc->plane, loc->x, loc->z, loc->locIndex, loc->angle, loc->shape, loc->layer);
                linkable_unlink(&loc->link);
                free(loc);
            }
        }

        _Client.cyclelogic5++;
        if (_Client.cyclelogic5 > 85) {
            _Client.cyclelogic5 = 0;
            // ANTICHEAT_CYCLELOGIC5
            p1isaac(c->out, 85);
        }
    }
}

void updateEntityChats(Client *c) {
    for (int i = -1; i < c->player_count; i++) {
        int index;
        if (i == -1) {
            index = LOCAL_PLAYER_INDEX;
        } else {
            index = c->player_ids[i];
        }

        PlayerEntity *player = c->players[index];
        if (player && player->pathing_entity.chatTimer > 0) {
            player->pathing_entity.chatTimer--;

            if (player->pathing_entity.chatTimer == 0) {
                player->pathing_entity.chat[0] = '\0';
            }
        }
    }

    for (int i = 0; i < c->npc_count; i++) {
        int index = c->npc_ids[i];
        NpcEntity *npc = c->npcs[index];

        if (npc && npc->pathing_entity.chatTimer > 0) {
            npc->pathing_entity.chatTimer--;

            if (npc->pathing_entity.chatTimer == 0) {
                npc->pathing_entity.chat[0] = '\0';
            }
        }
    }
}

static void updateForceMovement(PathingEntity *entity) {
    int delta = entity->forceMoveEndCycle - _Client.loop_cycle;
    int dstX = entity->forceMoveStartSceneTileX * 128 + entity->size * 64;
    int dstZ = entity->forceMoveStartSceneTileZ * 128 + entity->size * 64;

    entity->x += (dstX - entity->x) / delta;
    entity->z += (dstZ - entity->z) / delta;

    entity->seqTrigger = 0;

    if (entity->forceMoveFaceDirection == 0) {
        entity->dstYaw = 1024;
    }

    if (entity->forceMoveFaceDirection == 1) {
        entity->dstYaw = 1536;
    }

    if (entity->forceMoveFaceDirection == 2) {
        entity->dstYaw = 0;
    }

    if (entity->forceMoveFaceDirection == 3) {
        entity->dstYaw = 512;
    }
}

static void startForceMovement(PathingEntity *entity) {
    if (entity->forceMoveStartCycle == _Client.loop_cycle || entity->primarySeqId == -1 || entity->primarySeqDelay != 0 || entity->primarySeqCycle + 1 > _SeqType.instances[entity->primarySeqId]->delay[entity->primarySeqFrame]) {
        int duration = entity->forceMoveStartCycle - entity->forceMoveEndCycle;
        int delta = _Client.loop_cycle - entity->forceMoveEndCycle;
        int dx0 = entity->forceMoveStartSceneTileX * 128 + entity->size * 64;
        int dz0 = entity->forceMoveStartSceneTileZ * 128 + entity->size * 64;
        int dx1 = entity->forceMoveEndSceneTileX * 128 + entity->size * 64;
        int dz1 = entity->forceMoveEndSceneTileZ * 128 + entity->size * 64;
        entity->x = (dx0 * (duration - delta) + dx1 * delta) / duration;
        entity->z = (dz0 * (duration - delta) + dz1 * delta) / duration;
    }

    entity->seqTrigger = 0;

    if (entity->forceMoveFaceDirection == 0) {
        entity->dstYaw = 1024;
    }

    if (entity->forceMoveFaceDirection == 1) {
        entity->dstYaw = 1536;
    }

    if (entity->forceMoveFaceDirection == 2) {
        entity->dstYaw = 0;
    }

    if (entity->forceMoveFaceDirection == 3) {
        entity->dstYaw = 512;
    }

    entity->yaw = entity->dstYaw;
}

static void updateMovement(Client*c,PathingEntity *entity) {
    entity->secondarySeqId = entity->seqStandId;

    if (entity->pathLength == 0) {
        entity->seqTrigger = 0;
        return;
    }

    if (entity->primarySeqId != -1 && entity->primarySeqDelay == 0) {
        SeqType *seq = _SeqType.instances[entity->primarySeqId];
        if (!seq->walkmerge) {
            entity->seqTrigger++;
            return;
        }
    }

    int x = entity->x;
    int z = entity->z;
    int dstX = entity->pathTileX[entity->pathLength - 1] * 128 + entity->size * 64;
    int dstZ = entity->pathTileZ[entity->pathLength - 1] * 128 + entity->size * 64;

    int recover_loading_gap=0;
#if defined(BUILD_A_EMBEDDED) && defined(__vita__)
    extern uint64_t ba_scene_motion_recover_until;
    if(c->local_player&&entity==&c->local_player->pathing_entity&&entity->size==1&&c->currentLevel>=0&&c->currentLevel<4&&
       (dstX-x>256||dstX-x< -256||dstZ-z>256||dstZ-z< -256)&&ba_timing_now()<ba_scene_motion_recover_until){
        recover_loading_gap=r45_clear_recovery_segment(c->levelCollisionMap[c->currentLevel],x,z,dstX,dstZ);
        if(recover_loading_gap){static uint64_t next_note;uint64_t now=ba_timing_now();if(now>=next_note){char line[160];snprintf(line,sizeof line,"# travel_path_recovery %llu from %d %d target %d %d path %d",(unsigned long long)now,x,z,dstX,dstZ,entity->pathLength);ba_cadence_diagnostic(line);next_note=now+250000u;}}
    }
#endif
    if (recover_loading_gap || (dstX - x <= 256 && dstX - x >= -256 && dstZ - z <= 256 && dstZ - z >= -256)) {
        if (x < dstX) {
            if (z < dstZ) {
                entity->dstYaw = 1280;
            } else if (z > dstZ) {
                entity->dstYaw = 1792;
            } else {
                entity->dstYaw = 1536;
            }
        } else if (x > dstX) {
            if (z < dstZ) {
                entity->dstYaw = 768;
            } else if (z > dstZ) {
                entity->dstYaw = 256;
            } else {
                entity->dstYaw = 512;
            }
        } else if (z < dstZ) {
            entity->dstYaw = 1024;
        } else {
            entity->dstYaw = 0;
        }

        int deltaYaw = entity->dstYaw - entity->yaw & 0x7ff;
        if (deltaYaw > 1024) {
            deltaYaw -= 2048;
        }

        int seqId = entity->seqTurnAroundId;
        if (deltaYaw >= -256 && deltaYaw <= 256) {
            seqId = entity->seqWalkId;
        } else if (deltaYaw >= 256 && deltaYaw < 768) {
            seqId = entity->seqTurnRightId;
        } else if (deltaYaw >= -768 && deltaYaw <= -256) {
            seqId = entity->seqTurnLeftId;
        }

        if (seqId == -1) {
            seqId = entity->seqWalkId;
        }

        entity->secondarySeqId = seqId;
        int moveSpeed = 4;
        if (entity->yaw != entity->dstYaw && entity->targetId == -1) {
            moveSpeed = 2;
        }

        if (entity->pathLength > 2) {
            moveSpeed = 6;
        }

        if (entity->pathLength > 3) {
            moveSpeed = 8;
        }

        if (entity->seqTrigger > 0 && entity->pathLength > 1) {
            moveSpeed = 8;
            entity->seqTrigger--;
        }

        if (entity->pathRunning[entity->pathLength - 1]) {
            moveSpeed <<= 0x1;
        }

        if (moveSpeed >= 8 && entity->secondarySeqId == entity->seqWalkId && entity->seqRunId != -1) {
            entity->secondarySeqId = entity->seqRunId;
        }

        if (x < dstX) {
            entity->x += moveSpeed;
            if (entity->x > dstX) {
                entity->x = dstX;
            }
        } else if (x > dstX) {
            entity->x -= moveSpeed;
            if (entity->x < dstX) {
                entity->x = dstX;
            }
        }
        if (z < dstZ) {
            entity->z += moveSpeed;
            if (entity->z > dstZ) {
                entity->z = dstZ;
            }
        } else if (z > dstZ) {
            entity->z -= moveSpeed;
            if (entity->z < dstZ) {
                entity->z = dstZ;
            }
        }

        if (entity->x == dstX && entity->z == dstZ) {
            entity->pathLength--;
        }
    } else {
        entity->x = dstX;
        entity->z = dstZ;
    }
}

static void updateFacingDirection(Client *c, PathingEntity *e) {
    if (e->targetId != -1 && e->targetId < 32768) {
        NpcEntity *npc = c->npcs[e->targetId];
        if (npc) {
            int dstX = e->x - npc->pathing_entity.x;
            int dstZ = e->z - npc->pathing_entity.z;

            if (dstX != 0 || dstZ != 0) {
                e->dstYaw = (int)(atan2(dstX, dstZ) * 325.949) & 0x7ff;
            }
        }
    }

    if (e->targetId >= 32768) {
        int index = e->targetId - 32768;
        if (index == c->local_pid) {
            index = LOCAL_PLAYER_INDEX;
        }

        PlayerEntity *player = c->players[index];
        if (player) {
            int dstX = e->x - player->pathing_entity.x;
            int dstZ = e->z - player->pathing_entity.z;

            if (dstX != 0 || dstZ != 0) {
                e->dstYaw = (int)(atan2(dstX, dstZ) * 325.949) & 0x7ff;
            }
        }
    }

    if ((e->targetTileX != 0 || e->targetTileZ != 0) && (e->pathLength == 0 || e->seqTrigger > 0)) {
        int dstX = e->x - (e->targetTileX - c->sceneBaseTileX - c->sceneBaseTileX) * 64;
        int dstZ = e->z - (e->targetTileZ - c->sceneBaseTileZ - c->sceneBaseTileZ) * 64;

        if (dstX != 0 || dstZ != 0) {
            e->dstYaw = (int)(atan2(dstX, dstZ) * 325.949) & 0x7ff;
        }

        e->targetTileX = 0;
        e->targetTileZ = 0;
    }

    int remainingYaw = e->dstYaw - e->yaw & 0x7ff;

    if (remainingYaw != 0) {
        if (remainingYaw < 32 || remainingYaw > 2016) {
            e->yaw = e->dstYaw;
        } else if (remainingYaw > 1024) {
            e->yaw -= 32;
        } else {
            e->yaw += 32;
        }

        e->yaw &= 0x7ff;

        if (e->secondarySeqId == e->seqStandId && e->yaw != e->dstYaw) {
            if (e->seqTurnId != -1) {
                e->secondarySeqId = e->seqTurnId;
                return;
            }

            e->secondarySeqId = e->seqWalkId;
        }
    }
}

static void updateSequences(PathingEntity *e) {
    ba_timing_record(BA_ENTITY_STEP,0);
    e->seqStretches = false;

    SeqType *seq;
    if (e->secondarySeqId != -1) {
        seq = _SeqType.instances[e->secondarySeqId];
        e->secondarySeqCycle++;
        if (e->secondarySeqFrame < seq->frameCount && e->secondarySeqCycle > seq->delay[e->secondarySeqFrame]) {
            e->secondarySeqCycle = 0;
            e->secondarySeqFrame++;
        }
        if (e->secondarySeqFrame >= seq->frameCount) {
            e->secondarySeqCycle = 0;
            e->secondarySeqFrame = 0;
        }
    }

    if (e->primarySeqId != -1 && e->primarySeqDelay == 0) {
        seq = _SeqType.instances[e->primarySeqId];
        e->primarySeqCycle++;
        while (e->primarySeqFrame < seq->frameCount && e->primarySeqCycle > seq->delay[e->primarySeqFrame]) {
            e->primarySeqCycle -= seq->delay[e->primarySeqFrame];
            e->primarySeqFrame++;
        }

        if (e->primarySeqFrame >= seq->frameCount) {
            e->primarySeqFrame -= seq->replayoff;
            e->primarySeqLoop++;
            if (e->primarySeqLoop >= seq->replaycount) {
                e->primarySeqId = -1;
            }
            if (e->primarySeqFrame < 0 || e->primarySeqFrame >= seq->frameCount) {
                e->primarySeqId = -1;
            }
        }

        e->seqStretches = seq->stretches;
    }

    if (e->primarySeqDelay > 0) {
        e->primarySeqDelay--;
    }

    if (e->spotanimId != -1 && _Client.loop_cycle >= e->spotanimLastCycle) {
        if (e->spotanimFrame < 0) {
            e->spotanimFrame = 0;
        }

        seq = _SpotAnimType.instances[e->spotanimId]->seq;
        e->spotanimCycle++;
        while (e->spotanimFrame < seq->frameCount && e->spotanimCycle > seq->delay[e->spotanimFrame]) {
            e->spotanimCycle -= seq->delay[e->spotanimFrame];
            e->spotanimFrame++;
        }

        if (e->spotanimFrame >= seq->frameCount) {
            if (e->spotanimFrame < 0 || e->spotanimFrame >= seq->frameCount) {
                e->spotanimId = -1;
            }
        }
    }
}

static void agility_state_probe(Client*c,PathingEntity*e){
#if defined(__vita__) && defined(GL11)
    static int previous[12],valid,was_active,samples;
    if(!ba_agility_probe_enabled){valid=was_active=0;return;}
    if(e!=&c->local_player->pathing_entity||samples>=512)return;
    int active=e->primarySeqId>=0||e->forceMoveStartCycle>=_Client.loop_cycle||e->forceMoveEndCycle>_Client.loop_cycle;
    if(!active&&!was_active)return;
    int key[]={e->primarySeqId,e->primarySeqFrame,e->primarySeqDelay,e->secondarySeqId,e->secondarySeqFrame,e->forceMoveEndCycle,e->forceMoveStartCycle,e->x,e->z,e->pathLength,e->yaw,e->seqTrigger};
    if(valid&&!memcmp(previous,key,sizeof key))return;
    memcpy(previous,key,sizeof key);valid=1;was_active=active;samples++;
    char line[320];snprintf(line,sizeof line,"# agility_state %llu logic %d seq %d frame %d cycle %d delay %d secondary %d/%d force %d/%d pos %d/%d path %d yaw %d",(unsigned long long)ba_timing_now(),_Client.loop_cycle,e->primarySeqId,e->primarySeqFrame,e->primarySeqCycle,e->primarySeqDelay,e->secondarySeqId,e->secondarySeqFrame,e->forceMoveEndCycle-_Client.loop_cycle,e->forceMoveStartCycle-_Client.loop_cycle,e->x,e->z,e->pathLength,e->yaw);
    extern void ba_cadence_diagnostic(const char*);ba_cadence_diagnostic(line);
#else
    (void)c;(void)e;
#endif
}

static void updateEntity(Client *c, PathingEntity *entity, int size) {
    (void)size;
    if (entity->x < 128 || entity->z < 128 || entity->x >= 13184 || entity->z >= 13184) {
        entity->primarySeqId = -1;
        entity->spotanimId = -1;
        entity->forceMoveEndCycle = 0;
        entity->forceMoveStartCycle = 0;
        entity->x = entity->pathTileX[0] * 128 + entity->size * 64;
        entity->z = entity->pathTileZ[0] * 128 + entity->size * 64;
        entity->pathLength = 0;
    }

    if (entity == &c->local_player->pathing_entity && (entity->x < 1536 || entity->z < 1536 || entity->x >= 11776 || entity->z >= 11776)) {
        entity->primarySeqId = -1;
        entity->spotanimId = -1;
        entity->forceMoveEndCycle = 0;
        entity->forceMoveStartCycle = 0;
        entity->x = entity->pathTileX[0] * 128 + entity->size * 64;
        entity->z = entity->pathTileZ[0] * 128 + entity->size * 64;
        entity->pathLength = 0;
    }

    if (entity->forceMoveEndCycle > _Client.loop_cycle) {
        updateForceMovement(entity);
    } else if (entity->forceMoveStartCycle >= _Client.loop_cycle) {
        startForceMovement(entity);
    } else {
        updateMovement(c,entity);
    }

    updateFacingDirection(c, entity);
    updateSequences(entity);
    agility_state_probe(c,entity);
}

#ifdef BUILD_A_EMBEDDED
static void ba_sample_actor(unsigned kind,int id,PathingEntity *entity) {
    static struct {int id,seq,frame;unsigned cycle;uint64_t wrap;} previous[2];
    if(kind>=2)return;
    int seq_id=entity->secondarySeqId,frame=entity->secondarySeqFrame;
    if(seq_id<0||seq_id>=_SeqType.count)return;
    if(previous[kind].id!=id||previous[kind].seq!=seq_id){previous[kind].wrap=0;previous[kind].frame=-1;}
    if(frame<previous[kind].frame){
        SeqType *seq=_SeqType.instances[seq_id];unsigned expected=0;
        for(int i=0;i<seq->frameCount;i++)expected+=(unsigned)seq->delay[i]+1;
        uint64_t now=ba_timing_now();
        if(previous[kind].wrap){
            FILE *f=fopen("ux0:/data/RS2Vita/actor-animation-loops.csv","a");
            if(f){fprintf(f,"%llu,%u,%d,%d,%d,%u,%u,%llu,%d,%d\n",(unsigned long long)now,kind,id,seq_id,seq->frameCount,expected,_Client.loop_cycle-previous[kind].cycle,(unsigned long long)(now-previous[kind].wrap),entity->x,entity->z);fclose(f);}
        }
        previous[kind].wrap=now;previous[kind].cycle=_Client.loop_cycle;
    }
    previous[kind].id=id;previous[kind].seq=seq_id;previous[kind].frame=frame;
}
#endif

void updatePlayers(Client *c) {
    for (int i = -1; i < c->player_count; i++) {
        int index;
        if (i == -1) {
            index = LOCAL_PLAYER_INDEX;
        } else {
            index = c->player_ids[i];
        }

        PlayerEntity *player = c->players[index];
        if (player) {
            updateEntity(c, &player->pathing_entity, 1);
#ifdef BUILD_A_EMBEDDED
            if(i==-1)ba_sample_actor(0,index,&player->pathing_entity);
#endif
        }
    }

    _Client.cyclelogic6++;
    if (_Client.cyclelogic6 > 1406) {
        _Client.cyclelogic6 = 0;
        // ANTICHEAT_c->CYCLELOGIC6
        p1isaac(c->out, 219);
        p1(c->out, 0);
        int start = c->out->pos;
        p1(c->out, 162);
        p1(c->out, 22);
        if ((int)(jrand() * 2.0) == 0) {
            p1(c->out, 84);
        }
        p2(c->out, 31824);
        p2(c->out, 13490);
        if ((int)(jrand() * 2.0) == 0) {
            p1(c->out, 123);
        }
        if ((int)(jrand() * 2.0) == 0) {
            p1(c->out, 134);
        }
        p1(c->out, 100);
        p1(c->out, 94);
        p2(c->out, 35521);
        psize1(c->out, c->out->pos - start);
    }
}

static void updateNpcs(Client *c) {
    for (int i = 0; i < c->npc_count; i++) {
        int id = c->npc_ids[i];
        NpcEntity *npc = c->npcs[id];
        if (npc) {
            updateEntity(c, &npc->pathing_entity, npc->type->size);
#ifdef BUILD_A_EMBEDDED
            if(i==0)ba_sample_actor(1,id,&npc->pathing_entity);
#endif
        }
    }
}

static void client_update_orbit_camera(Client *c) {
    int orbitX = c->local_player->pathing_entity.x + c->camera_anticheat_offset_x;
    int orbitZ = c->local_player->pathing_entity.z + c->camera_anticheat_offset_z;
    if (c->orbitCameraX - orbitX < -500 || c->orbitCameraX - orbitX > 500 || c->orbitCameraZ - orbitZ < -500 || c->orbitCameraZ - orbitZ > 500) {
        c->orbitCameraX = orbitX;
        c->orbitCameraZ = orbitZ;
    }
    if (c->orbitCameraX != orbitX) {
        c->orbitCameraX += (orbitX - c->orbitCameraX) / 16;
    }
    if (c->orbitCameraZ != orbitZ) {
        c->orbitCameraZ += (orbitZ - c->orbitCameraZ) / 16;
    }
    if (c->shell->action_key[1] == 1) {
        c->orbitCameraYawVelocity += (-c->orbitCameraYawVelocity - 24) / 2;
    } else if (c->shell->action_key[2] == 1) {
        c->orbitCameraYawVelocity += (24 - c->orbitCameraYawVelocity) / 2;
    } else {
        c->orbitCameraYawVelocity /= 2;
    }
    if (c->shell->action_key[3] == 1) {
        c->orbitCameraPitchVelocity += (12 - c->orbitCameraPitchVelocity) / 2;
    } else if (c->shell->action_key[4] == 1) {
        c->orbitCameraPitchVelocity += (-c->orbitCameraPitchVelocity - 12) / 2;
    } else {
        c->orbitCameraPitchVelocity /= 2;
    }
    c->orbit_camera_yaw = c->orbit_camera_yaw + c->orbitCameraYawVelocity / 2 & 0x7ff;
    c->orbit_camera_pitch += c->orbitCameraPitchVelocity / 2;
    if (c->orbit_camera_pitch < 128) {
        c->orbit_camera_pitch = 128;
    }
    if (c->orbit_camera_pitch > 383) {
        c->orbit_camera_pitch = 383;
    }

    int orbitTileX = c->orbitCameraX >> 7;
    int orbitTileZ = c->orbitCameraZ >> 7;
    int orbitY = getHeightmapY(c, c->currentLevel, c->orbitCameraX, c->orbitCameraZ);
    int maxY = 0;

    if (orbitTileX > 3 && orbitTileZ > 3 && orbitTileX < 100 && orbitTileZ < 100) {
        for (int x = orbitTileX - 4; x <= orbitTileX + 4; x++) {
            for (int z = orbitTileZ - 4; z <= orbitTileZ + 4; z++) {
                int level = c->currentLevel;
                if (level < 3 && (c->levelTileFlags[1][x][z] & 0x2) == 2) {
                    level++;
                }

                int y = orbitY - c->levelHeightmap[level][x][z];
                if (y > maxY) {
                    maxY = y;
                }
            }
        }
    }

    int clamp = maxY * 192;
    if (clamp > 98048) {
        clamp = 98048;
    }

    if (clamp < 32768) {
        clamp = 32768;
    }

    if (clamp > c->cameraPitchClamp) {
        c->cameraPitchClamp += (clamp - c->cameraPitchClamp) / 24;
    } else if (clamp < c->cameraPitchClamp) {
        c->cameraPitchClamp += (clamp - c->cameraPitchClamp) / 80;
    }
}

static bool client_try_move(Client *c, int srcX, int srcZ, int dx, int dz, int type, int locWidth, int locLength, int locRotation, int locShape, int forceapproach, bool tryNearest) {
    pos_trace(c, "move", 0, dx, dz);
    int8_t sceneWidth = 104;
    int8_t sceneLength = 104;
    for (int x = 0; x < sceneWidth; x++) {
        for (int z = 0; z < sceneLength; z++) {
            c->bfsDirection[x][z] = 0;
            c->bfsCost[x][z] = 99999999;
        }
    }

    int x = srcX;
    int z = srcZ;

    c->bfsDirection[srcX][srcZ] = 99;
    c->bfsCost[srcX][srcZ] = 0;

    int steps = 0;
    int length = 0;

    c->bfsStepX[steps] = srcX;
    c->bfsStepZ[steps++] = srcZ;

    bool arrived = false;
    int bufferSize = BFS_STEP_SIZE;
    int **flags = c->levelCollisionMap[c->currentLevel]->flags;

    while (length != steps) {
        x = c->bfsStepX[length];
        z = c->bfsStepZ[length];
        length = (length + 1) % bufferSize;

        if (x == dx && z == dz) {
            arrived = true;
            break;
        }

        if (locShape != 0) {
            int shape = locShape - 1;

            if ((shape <= WALL_SQUARECORNER || shape == WALL_DIAGONAL) && collisionmap_test_wall(c->levelCollisionMap[c->currentLevel], x, z, dx, dz, shape, locRotation)) {
                arrived = true;
                break;
            }

            if (shape <= WALLDECOR_DIAGONAL_BOTH && collisionmap_test_wdecor(c->levelCollisionMap[c->currentLevel], x, z, dx, dz, shape, locRotation)) {
                arrived = true;
                break;
            }
        }

        if (locWidth != 0 && locLength != 0 && collisionmap_test_loc(c->levelCollisionMap[c->currentLevel], x, z, dx, dz, locWidth, locLength, forceapproach)) {
            arrived = true;
            break;
        }

        int nextCost = c->bfsCost[x][z] + 1;
        if (x > 0 && c->bfsDirection[x - 1][z] == 0 && (flags[x - 1][z] & 0x280108) == 0) {
            c->bfsStepX[steps] = x - 1;
            c->bfsStepZ[steps] = z;
            steps = (steps + 1) % bufferSize;
            c->bfsDirection[x - 1][z] = 2;
            c->bfsCost[x - 1][z] = nextCost;
        }

        if (x < sceneWidth - 1 && c->bfsDirection[x + 1][z] == 0 && (flags[x + 1][z] & 0x280180) == 0) {
            c->bfsStepX[steps] = x + 1;
            c->bfsStepZ[steps] = z;
            steps = (steps + 1) % bufferSize;
            c->bfsDirection[x + 1][z] = 8;
            c->bfsCost[x + 1][z] = nextCost;
        }

        if (z > 0 && c->bfsDirection[x][z - 1] == 0 && (flags[x][z - 1] & 0x280102) == 0) {
            c->bfsStepX[steps] = x;
            c->bfsStepZ[steps] = z - 1;
            steps = (steps + 1) % bufferSize;
            c->bfsDirection[x][z - 1] = 1;
            c->bfsCost[x][z - 1] = nextCost;
        }

        if (z < sceneLength - 1 && c->bfsDirection[x][z + 1] == 0 && (flags[x][z + 1] & 0x280120) == 0) {
            c->bfsStepX[steps] = x;
            c->bfsStepZ[steps] = z + 1;
            steps = (steps + 1) % bufferSize;
            c->bfsDirection[x][z + 1] = 4;
            c->bfsCost[x][z + 1] = nextCost;
        }

        if (x > 0 && z > 0 && c->bfsDirection[x - 1][z - 1] == 0 && (flags[x - 1][z - 1] & 0x28010E) == 0 && (flags[x - 1][z] & 0x280108) == 0 && (flags[x][z - 1] & 0x280102) == 0) {
            c->bfsStepX[steps] = x - 1;
            c->bfsStepZ[steps] = z - 1;
            steps = (steps + 1) % bufferSize;
            c->bfsDirection[x - 1][z - 1] = 3;
            c->bfsCost[x - 1][z - 1] = nextCost;
        }

        if (x < sceneWidth - 1 && z > 0 && c->bfsDirection[x + 1][z - 1] == 0 && (flags[x + 1][z - 1] & 0x280183) == 0 && (flags[x + 1][z] & 0x280180) == 0 && (flags[x][z - 1] & 0x280102) == 0) {
            c->bfsStepX[steps] = x + 1;
            c->bfsStepZ[steps] = z - 1;
            steps = (steps + 1) % bufferSize;
            c->bfsDirection[x + 1][z - 1] = 9;
            c->bfsCost[x + 1][z - 1] = nextCost;
        }

        if (x > 0 && z < sceneLength - 1 && c->bfsDirection[x - 1][z + 1] == 0 && (flags[x - 1][z + 1] & 0x280138) == 0 && (flags[x - 1][z] & 0x280108) == 0 && (flags[x][z + 1] & 0x280120) == 0) {
            c->bfsStepX[steps] = x - 1;
            c->bfsStepZ[steps] = z + 1;
            steps = (steps + 1) % bufferSize;
            c->bfsDirection[x - 1][z + 1] = 6;
            c->bfsCost[x - 1][z + 1] = nextCost;
        }

        if (x < sceneWidth - 1 && z < sceneLength - 1 && c->bfsDirection[x + 1][z + 1] == 0 && (flags[x + 1][z + 1] & 0x2801E0) == 0 && (flags[x + 1][z] & 0x280180) == 0 && (flags[x][z + 1] & 0x280120) == 0) {
            c->bfsStepX[steps] = x + 1;
            c->bfsStepZ[steps] = z + 1;
            steps = (steps + 1) % bufferSize;
            c->bfsDirection[x + 1][z + 1] = 12;
            c->bfsCost[x + 1][z + 1] = nextCost;
        }
    }

    c->tryMoveNearest = 0;

    if (!arrived) {
        if (tryNearest) {
            int min = 100;
            for (int padding = 1; padding < 2; padding++) {
                for (int px = dx - padding; px <= dx + padding; px++) {
                    for (int pz = dz - padding; pz <= dz + padding; pz++) {
                        if (px >= 0 && pz >= 0 && px < 104 && pz < 104 && c->bfsCost[px][pz] < min) {
                            min = c->bfsCost[px][pz];
                            x = px;
                            z = pz;
                            c->tryMoveNearest = 1;
                            arrived = true;
                        }
                    }
                }

                if (arrived) {
                    break;
                }
            }
        }

        if (!arrived) {
            return false;
        }
    }

    length = 0;
    c->bfsStepX[length] = x;
    c->bfsStepZ[length++] = z;

    int dir = c->bfsDirection[x][z];
    int next = dir;
    while (x != srcX || z != srcZ) {
        if (next != dir) {
            dir = next;
            c->bfsStepX[length] = x;
            c->bfsStepZ[length++] = z;
        }

        if ((next & 0x2) != 0) {
            x++;
        } else if ((next & 0x8) != 0) {
            x--;
        }

        if ((next & 0x1) != 0) {
            z++;
        } else if ((next & 0x4) != 0) {
            z--;
        }

        next = c->bfsDirection[x][z];
    }

    if (length > 0) {
        bufferSize = length;

        if (length > 25) {
            bufferSize = 25;
        }

        length--;

        int startX = c->bfsStepX[length];
        int startZ = c->bfsStepZ[length];

        // TODO showdebug
        // if (c->show_debug && super.actionKey[6] == 1 && super.actionKey[7] == 1) {
        // 	// check if tile is already added, if so remove it
        // 	for (int i = 0; i < c->userTileMarkers.length; i++) {
        // 		if (c->userTileMarkers[i] != null && c->userTileMarkers[i].x == World3D.clickTileX && c->userTileMarkers[i].z == World3D.clickTileZ) {
        // 			c->userTileMarkers[i] = null;
        // 			return false;
        // 		}
        // 	}

        // 	// add new
        // 	c->userTileMarkers[c->userTileMarkerIndex] = new Ground(c->currentLevel, World3D.clickTileX, World3D.clickTileZ);
        // 	c->userTileMarkerIndex = c->userTileMarkerIndex + 1 & (c->userTileMarkers.length - 1);
        // 	return false;
        // }

        if (type == 0) {
            // MOVE_GAMECLICK
            p1isaac(c->out, 181);
            p1(c->out, bufferSize + bufferSize + 3);
        } else if (type == 1) {
            // MOVE_MINIMAPCLICK
            p1isaac(c->out, 165);
            p1(c->out, bufferSize + bufferSize + 3 + 14);
        } else if (type == 2) {
            // MOVE_OPCLICK
            p1isaac(c->out, 93);
            p1(c->out, bufferSize + bufferSize + 3);
        }

        if (c->shell->action_key[5] == 1) {
            p1(c->out, 1);
        } else {
            p1(c->out, 0);
        }

        p2(c->out, startX + c->sceneBaseTileX);
        p2(c->out, startZ + c->sceneBaseTileZ);
        c->flagSceneTileX = c->bfsStepX[0];
        c->flagSceneTileZ = c->bfsStepZ[0];

        for (int i = 1; i < bufferSize; i++) {
            length--;
            p1(c->out, c->bfsStepX[length] - startX);
            p1(c->out, c->bfsStepZ[length] - startZ);
        }

        return true;
    }

    return type != 1;
}

static bool interactWithLoc(Client *c, int opcode, int x, int z, int bitset) {
    int locId = bitset >> 14 & 0x7fff;
    int info = world3d_get_info(c->scene, c->currentLevel, x, z, bitset);
    if (info == -1) {
        return false;
    }

    int type = info & 0x1f;
    int angle = info >> 6 & 0x3;
    if (type == 10 || type == 11 || type == 22) {
        LocType *loc = loctype_get(locId);
        int width;
        int height;

        if (angle == 0 || angle == 2) {
            width = loc->width;
            height = loc->length;
        } else {
            width = loc->length;
            height = loc->width;
        }

        int forceapproach = loc->forceapproach;
        if (angle != 0) {
            forceapproach = (forceapproach << angle & 0xf) + (forceapproach >> (4 - angle));
        }

        client_try_move(c, c->local_player->pathing_entity.pathTileX[0], c->local_player->pathing_entity.pathTileZ[0], x, z, 2, width, height, 0, 0, forceapproach, false);
    } else {
        client_try_move(c, c->local_player->pathing_entity.pathTileX[0], c->local_player->pathing_entity.pathTileZ[0], x, z, 2, 0, 0, angle, type + 1, 0, false);
    }

    c->crossX = c->shell->mouse_click_x;
    c->crossY = c->shell->mouse_click_y;
    c->cross_mode = 2;
    c->cross_cycle = 0;

    p1isaac(c->out, opcode);
    p2(c->out, x + c->sceneBaseTileX);
    p2(c->out, z + c->sceneBaseTileZ);
    p2(c->out, locId);
    return true;
}

static void addFriend(Client *c, int64_t username) {
    if (username == 0L) {
        return;
    }

    if (c->friend_count >= 100) {
        client_add_message(c, 0, "Your friends list is full. Max of 100 hit", "");
        return;
    }

    char *displayName = jstring_format_name(jstring_from_base37(username));
    for (int i = 0; i < c->friend_count; i++) {
        if (c->friendName37[i] == username) {
            char buf[MAX_STR];
            sprintf(buf, "%s is already on your friend list", displayName);
            client_add_message(c, 0, buf, "");
            return;
        }
    }

    for (int i = 0; i < c->ignoreCount; i++) {
        if (c->ignoreName37[i] == username) {
            char buf[MAX_STR];
            sprintf(buf, "Please remove %s from your ignore list first", displayName);
            client_add_message(c, 0, buf, "");
            return;
        }
    }

    if (strcmp(displayName, c->local_player->name) != 0) {
        c->friendName[c->friend_count] = displayName;
        c->friendName37[c->friend_count] = username;
        c->friendWorld[c->friend_count] = 0;
        c->friend_count++;
        c->redraw_sidebar = true;

        // FRIENDLIST_ADD
        p1isaac(c->out, 118);
        p8(c->out, username);
    }
}

static void addIgnore(Client *c, int64_t username) {
    if (username == 0L) {
        return;
    }

    if (c->ignoreCount >= 100) {
        client_add_message(c, 0, "Your ignore list is full. Max of 100 hit", "");
        return;
    }

    char *displayName = jstring_format_name(jstring_from_base37(username));
    for (int i = 0; i < c->ignoreCount; i++) {
        if (c->ignoreName37[i] == username) {
            char buf[MAX_STR];
            sprintf(buf, "%s is already on your ignore list", displayName);
            client_add_message(c, 0, buf, "");
            return;
        }
    }

    for (int i = 0; i < c->friend_count; i++) {
        if (c->friendName37[i] == username) {
            char buf[MAX_STR];
            sprintf(buf, "Please remove %s from your friend list first", displayName);
            client_add_message(c, 0, buf, "");
            return;
        }
    }

    c->ignoreName37[c->ignoreCount++] = username;
    c->redraw_sidebar = true;
    // IGNORELIST_ADD
    p1isaac(c->out, 79);
    p8(c->out, username);
}

static void removeFriend(Client *c, int64_t username) {
    if (username == 0L) {
        return;
    }

    for (int i = 0; i < c->friend_count; i++) {
        if (c->friendName37[i] == username) {
            c->friend_count--;
            c->redraw_sidebar = true;
            for (int j = i; j < c->friend_count; j++) {
                c->friendName[j] = c->friendName[j + 1];
                c->friendWorld[j] = c->friendWorld[j + 1];
                c->friendName37[j] = c->friendName37[j + 1];
            }
            // FRIENDLIST_DEL
            p1isaac(c->out, 11);
            p8(c->out, username);
            return;
        }
    }
}

static void removeIgnore(Client *c, int64_t username) {
    if (username == 0L) {
        return;
    }

    for (int i = 0; i < c->ignoreCount; i++) {
        if (c->ignoreName37[i] == username) {
            c->ignoreCount--;
            c->redraw_sidebar = true;
            for (int j = i; j < c->ignoreCount; j++) {
                c->ignoreName37[j] = c->ignoreName37[j + 1];
            }
            // IGNORELIST_DEL
            p1isaac(c->out, 171);
            p8(c->out, username);
            return;
        }
    }
}

#include "../r92_examine_speech.inc"

static void useMenuOption(Client *cl, int optionId);
void client_ui474_invoke(Client*c,int action,int a,int b,int component){int slot=399;int sa=c->menu_action[slot],pa=c->menuParamA[slot],pb=c->menuParamB[slot],pc=c->menuParamC[slot];c->menu_action[slot]=action;c->menuParamA[slot]=a;c->menuParamB[slot]=b;c->menuParamC[slot]=component;useMenuOption(c,slot);c->menu_action[slot]=sa;c->menuParamA[slot]=pa;c->menuParamB[slot]=pb;c->menuParamC[slot]=pc;}

void client_vita_dialogue_action(Client *c, int component, int action) {
    int slot=(int)(sizeof(c->menu_action)/sizeof(c->menu_action[0]))-1;
    int savedAction=c->menu_action[slot],savedA=c->menuParamA[slot],savedB=c->menuParamB[slot],savedC=c->menuParamC[slot];
    c->menu_action[slot]=action;c->menuParamA[slot]=c->menuParamB[slot]=0;c->menuParamC[slot]=component;
    useMenuOption(c,slot);
    c->menu_action[slot]=savedAction;c->menuParamA[slot]=savedA;c->menuParamB[slot]=savedB;c->menuParamC[slot]=savedC;
}

static void useMenuOption(Client *cl, int optionId) {
    if (optionId < 0) {
        return;
    }

    if (cl->chatback_input_open) {
        cl->chatback_input_open = false;
        cl->redraw_chatback = true;
    }

    int action = cl->menu_action[optionId];
    int a = cl->menuParamA[optionId];
    int b = cl->menuParamB[optionId];
    int c = cl->menuParamC[optionId];

#ifdef BUILD_A_EMBEDDED
    FILE *action_trace=h5_measuring()?NULL:fopen("ux0:/data/RS2Vita/menu-actions.csv","a");
    if(action_trace){fprintf(action_trace,"%llu,%d,%d,%d,%d,%d\n",(unsigned long long)ba_timing_now(),action,a,b,c,cl->obj_selected);fclose(action_trace);}
#endif

    if (action >= 2000) {
        action -= 2000;
    }

    if (action == 903 || action == 363) {
        char *option = cl->menu_option[optionId];
        int tag = indexof(option, "@whi@");

        if (tag != -1) {
            option = substring(option, tag + 5, strlen(option));
            strtrim(option);
            char *name = jstring_format_name(jstring_from_base37(jstring_to_base37(option)));
            bool found = false;

            for (int i = 0; i < cl->player_count; i++) {
                PlayerEntity *player = cl->players[cl->player_ids[i]];

                if (player && player->name[0] && platform_strcasecmp(player->name, name) == 0) {
                    client_try_move(cl, cl->local_player->pathing_entity.pathTileX[0], cl->local_player->pathing_entity.pathTileZ[0], player->pathing_entity.pathTileX[0], player->pathing_entity.pathTileZ[0], 2, 1, 1, 0, 0, 0, false);

                    if (action == 903) {
                        // OPPLAYER4
                        p1isaac(cl->out, 206);
                    } else if (action == 363) {
                        // OPPLAYER1
                        p1isaac(cl->out, 164);
                    }

                    p2(cl->out, cl->player_ids[i]);
                    found = true;
                    break;
                }
            }

            if (!found) {
                char buf[MAX_STR];
                sprintf(buf, "Unable to find %s", name);
                client_add_message(cl, 0, buf, "");
            }
        }
    } else if (action == 450) {
        // OPLOCU
        if (interactWithLoc(cl, 75, b, c, a)) {
            p2(cl->out, cl->objInterface);
            p2(cl->out, cl->objSelectedSlot);
            p2(cl->out, cl->objSelectedInterface);
        }
    } else if (action == 405 || action == 38 || action == 422 || action == 478 || action == 347) {
        if (action == 478) {
            if ((b & 0x3) == 0) {
                _Client.oplogic5++;
            }

            if (_Client.oplogic5 >= 90) {
                // ANTICHEAT_OPLOGIC5
                p1isaac(cl->out, 220);
            }

            // OPHELD4
            p1isaac(cl->out, 157);
        } else if (action == 347) {
            // OPHELD5
            p1isaac(cl->out, 211);
        } else if (action == 422) {
            // OPHELD3
            p1isaac(cl->out, 133);
        } else if (action == 405) {
            _Client.oplogic3 += a;
            if (_Client.oplogic3 >= 97) {
                // ANTICHEAT_OPLOGIC3
                p1isaac(cl->out, 30);
                p3(cl->out, 14953816);
            }

            // OPHELD1
            p1isaac(cl->out, 195);
        } else if (action == 38) {
            // OPHELD2
            p1isaac(cl->out, 71);
        }

        p2(cl->out, a);
        p2(cl->out, b);
        p2(cl->out, c);
        cl->selected_cycle = 0;
        cl->selectedInterface = c;
        cl->selectedItem = b;
        cl->selected_area = 2;

        if (_Component.instances[c]->layer == cl->viewport_interface_id) {
            cl->selected_area = 1;
        }

        if (_Component.instances[c]->layer == cl->chat_interface_id) {
            cl->selected_area = 3;
        }
    } else if (action == 728 || action == 542 || action == 6 || action == 963 || action == 245) {
        NpcEntity *npc = cl->npcs[a];
        if (npc) {
            client_try_move(cl, cl->local_player->pathing_entity.pathTileX[0], cl->local_player->pathing_entity.pathTileZ[0], npc->pathing_entity.pathTileX[0], npc->pathing_entity.pathTileZ[0], 2, 1, 1, 0, 0, 0, false);

            cl->crossX = cl->shell->mouse_click_x;
            cl->crossY = cl->shell->mouse_click_y;
            cl->cross_mode = 2;
            cl->cross_cycle = 0;

            if (action == 542) {
                // OPNPC2
                p1isaac(cl->out, 8);
            } else if (action == 6) {
                if ((a & 0x3) == 0) {
                    _Client.oplogic2++;
                }

                if (_Client.oplogic2 >= 124) {
                    // ANTICHEAT_OPLOGIC2
                    p1isaac(cl->out, 88);
                    p4(cl->out, 0);
                }

                // OPNPC3
                p1isaac(cl->out, 27);
            } else if (action == 963) {
                // OPNPC4
                p1isaac(cl->out, 113);
            } else if (action == 728) {
                // OPNPC1
                p1isaac(cl->out, 194);
            } else if (action == 245) {
                if ((a & 0x3) == 0) {
                    _Client.oplogic4++;
                }

                if (_Client.oplogic4 >= 85) {
                    // ANTICHEAT_OPLOGIC4
                    p1isaac(cl->out, 176);
                    p2(cl->out, 39596);
                }

                // OPNPC5
                p1isaac(cl->out, 100);
            }

            p2(cl->out, a);
        }
    } else if (action == 217) {
        bool success = client_try_move(cl, cl->local_player->pathing_entity.pathTileX[0], cl->local_player->pathing_entity.pathTileZ[0], b, c, 2, 0, 0, 0, 0, 0, false);
        if (!success) {
            client_try_move(cl, cl->local_player->pathing_entity.pathTileX[0], cl->local_player->pathing_entity.pathTileZ[0], b, c, 2, 1, 1, 0, 0, 0, false);
        }

        cl->crossX = cl->shell->mouse_click_x;
        cl->crossY = cl->shell->mouse_click_y;
        cl->cross_mode = 2;
        cl->cross_cycle = 0;

        // OPOBJU
        p1isaac(cl->out, 239);
        p2(cl->out, b + cl->sceneBaseTileX);
        p2(cl->out, c + cl->sceneBaseTileZ);
        p2(cl->out, a);
        p2(cl->out, cl->objInterface);
        p2(cl->out, cl->objSelectedSlot);
        p2(cl->out, cl->objSelectedInterface);
    } else if (action == 1175) {
        int locId = a >> 14 & 0x7fff;
        LocType *loc = loctype_get(locId);

        char examine[MAX_STR];
        if (!loc->desc) {
            snprintf(examine,sizeof examine, "It's a %s.", loc->name);
        } else {
            snprintf(examine,sizeof examine,"%s",loc->desc);
        }

        client_examine_message(cl,examine);
    } else if (action == 285) {
        // OPLOC1
        interactWithLoc(cl, 245, b, c, a);
    } else if (action == 881) {
        // OPHELDU
        p1isaac(cl->out, 130);
        p2(cl->out, a);
        p2(cl->out, b);
        p2(cl->out, c);
        p2(cl->out, cl->objInterface);
        p2(cl->out, cl->objSelectedSlot);
        p2(cl->out, cl->objSelectedInterface);

        cl->selected_cycle = 0;
        cl->selectedInterface = c;
        cl->selectedItem = b;
        cl->selected_area = 2;

        if (_Component.instances[c]->layer == cl->viewport_interface_id) {
            cl->selected_area = 1;
        }

        if (_Component.instances[c]->layer == cl->chat_interface_id) {
            cl->selected_area = 3;
        }
    } else if (action == 391) {
        // OPHELDT
        p1isaac(cl->out, 48);
        p2(cl->out, a);
        p2(cl->out, b);
        p2(cl->out, c);
        p2(cl->out, cl->activeSpellId);

        cl->selected_cycle = 0;
        cl->selectedInterface = c;
        cl->selectedItem = b;
        cl->selected_area = 2;

        if (_Component.instances[c]->layer == cl->viewport_interface_id) {
            cl->selected_area = 1;
        }

        if (_Component.instances[c]->layer == cl->chat_interface_id) {
            cl->selected_area = 3;
        }
    } else if (action == 660) {
        if (cl->menu_visible) {
            if ((ve_enabled()?ve_world_input(b,c):game_world_input(b,c))) world3d_click(ve_pointer(_Pix3D.center_x, b - (ve_enabled()?0:8)), ve_pointer(_Pix3D.center_y, c - (ve_enabled()?0:11)));
        } else {
            if (ve_enabled()?ve_world_input(cl->shell->mouse_click_x, cl->shell->mouse_click_y):game_world_input(cl->shell->mouse_click_x, cl->shell->mouse_click_y))
                world3d_click(ve_pointer(_Pix3D.center_x, cl->shell->mouse_click_x - (ve_enabled()?0:8)), ve_pointer(_Pix3D.center_y, cl->shell->mouse_click_y - (ve_enabled()?0:11)));
        }
    } else if (action == 188) {
        cl->obj_selected = 1;
        cl->objSelectedSlot = b;
        cl->objSelectedInterface = c;
        cl->objInterface = a;
        cl->objSelectedName = objtype_get(a)->name;
        cl->spell_selected = 0;
        if (_Custom.item_outlines) {
            cl->redraw_sidebar = true;
        }
        return;
    } else if (action == 44) {
        if (!cl->pressed_continue_option) {
            // RESUME_PAUSEBUTTON
            p1isaac(cl->out, 235);
            p2(cl->out, c);
            cl->pressed_continue_option = true;
        }
    } else if (action == 1773) {
        ObjType *obj = objtype_get(a);
        char examine[MAX_STR];

        if (c >= 100000) {
            snprintf(examine,sizeof examine, "%d x %s", c, obj->name);
        } else if (obj->desc[0] == '\0') {
            snprintf(examine,sizeof examine, "It's a %s.", obj->name);
        } else {
            snprintf(examine,sizeof examine,"%s",obj->desc);
        }

        client_examine_message(cl,examine);
    } else if (action == 900) {
        NpcEntity *npc = cl->npcs[a];

        if (npc) {
            client_try_move(cl, cl->local_player->pathing_entity.pathTileX[0], cl->local_player->pathing_entity.pathTileZ[0], npc->pathing_entity.pathTileX[0], npc->pathing_entity.pathTileZ[0], 2, 1, 1, 0, 0, 0, false);
            cl->crossX = cl->shell->mouse_click_x;
            cl->crossY = cl->shell->mouse_click_y;
            cl->cross_mode = 2;
            cl->cross_cycle = 0;
            // OPNPCU
            p1isaac(cl->out, 202);
            p2(cl->out, a);
            p2(cl->out, cl->objInterface);
            p2(cl->out, cl->objSelectedSlot);
            p2(cl->out, cl->objSelectedInterface);
        }
    } else if (action == 1373 || action == 1544 || action == 151 || action == 1101) {
        PlayerEntity *player = cl->players[a];
        if (player) {
            client_try_move(cl, cl->local_player->pathing_entity.pathTileX[0], cl->local_player->pathing_entity.pathTileZ[0], player->pathing_entity.pathTileX[0], player->pathing_entity.pathTileZ[0], 2, 1, 1, 0, 0, 0, false);

            cl->crossX = cl->shell->mouse_click_x;
            cl->crossY = cl->shell->mouse_click_y;
            cl->cross_mode = 2;
            cl->cross_cycle = 0;

            if (action == 1101) {
                // OPPLAYER1
                p1isaac(cl->out, 164);
            } else if (action == 151) {
                _Client.oplogic8++;
                if (_Client.oplogic8 >= 90) {
                    // ANTICHEAT_OPLOGIC8
                    p1isaac(cl->out, 2);
                    p2(cl->out, 31114);
                }

                // OPPLAYER2
                p1isaac(cl->out, 53);
            } else if (action == 1373) {
                // OPPLAYER4
                p1isaac(cl->out, 206);
            } else if (action == 1544) {
                // OPPLAYER3
                p1isaac(cl->out, 185);
            }

            p2(cl->out, a);
        }
    } else if (action == 265) {
        NpcEntity *npc = cl->npcs[a];
        if (npc) {
            client_try_move(cl, cl->local_player->pathing_entity.pathTileX[0], cl->local_player->pathing_entity.pathTileZ[0], npc->pathing_entity.pathTileX[0], npc->pathing_entity.pathTileZ[0], 2, 1, 1, 0, 0, 0, false);

            cl->crossX = cl->shell->mouse_click_x;
            cl->crossY = cl->shell->mouse_click_y;
            cl->cross_mode = 2;
            cl->cross_cycle = 0;

            // OPNPCT
            p1isaac(cl->out, 134);
            p2(cl->out, a);
            p2(cl->out, cl->activeSpellId);
        }
    } else if (action == 679) {
        const char *option = cl->menu_option[optionId];
        int tag = indexof(option, "@whi@");

        if (tag != -1) {
            char *name = substring(option, tag + 5, strlen(option));
            strtrim(name);
            int64_t name37 = jstring_to_base37(name);
            int friend = -1;
            for (int i = 0; i < cl->friend_count; i++) {
                if (cl->friendName37[i] == name37) {
                    friend = i;
                    break;
                }
            }

            if (friend != -1 && cl->friendWorld[friend] > 0) {
                cl->redraw_chatback = true;
                cl->chatback_input_open = false;
                cl->show_social_input = true;
                cl->social_input[0] = '\0';
                cl->social_action = 3;
                cl->social_name37 = cl->friendName37[friend];
                sprintf(cl->social_message, "Enter message to send to %s", cl->friendName[friend]);
            }
        }
    } else if (action == 55) {
        // OPLOCT
        if (interactWithLoc(cl, 9, b, c, a)) {
            p2(cl->out, cl->activeSpellId);
        }
    } else if (action == 224 || action == 993 || action == 99 || action == 746 || action == 877) {
        bool success = client_try_move(cl, cl->local_player->pathing_entity.pathTileX[0], cl->local_player->pathing_entity.pathTileZ[0], b, c, 2, 0, 0, 0, 0, 0, false);
        if (!success) {
            client_try_move(cl, cl->local_player->pathing_entity.pathTileX[0], cl->local_player->pathing_entity.pathTileZ[0], b, c, 2, 1, 1, 0, 0, 0, false);
        }

        cl->crossX = cl->shell->mouse_click_x;
        cl->crossY = cl->shell->mouse_click_y;
        cl->cross_mode = 2;
        cl->cross_cycle = 0;

        if (action == 224) {
            // OPOBJ1
            p1isaac(cl->out, 140);
        } else if (action == 746) {
            // OPOBJ4
            p1isaac(cl->out, 178);
        } else if (action == 877) {
            // OPOBJ5
            p1isaac(cl->out, 247);
        } else if (action == 99) {
            // OPOBJ3
            p1isaac(cl->out, 200);
        } else if (action == 993) {
            // OPOBJ2
            p1isaac(cl->out, 40);
        }

        p2(cl->out, b + cl->sceneBaseTileX);
        p2(cl->out, c + cl->sceneBaseTileZ);
        p2(cl->out, a);
    } else if (action == 1607) {
        NpcEntity *npc = cl->npcs[a];
        if (npc) {
            char examine[MAX_STR];

            if (!npc->type->desc) {
                snprintf(examine,sizeof examine, "It's a %s.", npc->type->name);
            } else {
                snprintf(examine,sizeof examine,"%s",npc->type->desc);
            }

            client_examine_message(cl,examine);
        }
    } else if (action == 504) {
        // OPLOC2
        interactWithLoc(cl, 172, b, c, a);
    } else if (action == 930) {
        Component *com = _Component.instances[c];
        cl->spell_selected = 1;
        cl->activeSpellId = c;
        cl->activeSpellFlags = com->actionTarget;
        cl->obj_selected = 0;
        if (_Custom.item_outlines) {
            cl->redraw_sidebar = true;
        }

        char *prefix = com->actionVerb;
        bool free_prefix = false;
        if (indexof_chr(prefix, ' ') != -1) {
            prefix = substring(prefix, 0, indexof_chr(prefix, ' '));
            free_prefix = true;
        }

        char *suffix = com->actionVerb;
        bool free_suffix = false;
        if (indexof_chr(suffix, ' ') != -1) {
            suffix = substring(suffix, indexof_chr(suffix, ' ') + 1, strlen(suffix));
            free_suffix = true;
        }

        sprintf(cl->spellCaption, "%s %s %s", prefix, com->action, suffix);
        if (free_prefix) {
            free(prefix);
        }
        if (free_suffix) {
            free(suffix);
        }

        if (cl->activeSpellFlags == 16) {
            cl->redraw_sidebar = true;
            cl->selected_tab = 3;
            cl->redraw_sideicons = true;
        }

        return;
    } else if (action == 951) {
        Component *com = _Component.instances[c];
        bool notify = true;

        if (com->clientCode > 0) {
            notify = handleInterfaceAction(cl, com);
        }

        if (notify) {
            // IF_BUTTON
            ba_interface_trace("IF_BUTTON",c,"");
            p1isaac(cl->out, 155);
            p2(cl->out, c);
        }
    } else if (action == 602 || action == 596 || action == 22 || action == 892 || action == 415) {
        if (action == 22) {
            // INV_BUTTON3
            p1isaac(cl->out, 212);
        } else if (action == 415) {
            if ((c & 0x3) == 0) {
                _Client.oplogic7++;
            }

            if (_Client.oplogic7 >= 55) {
                // ANTICHEAT_OPLOGIC7
                p1isaac(cl->out, 17);
                p4(cl->out, 0);
            }

            // INV_BUTTON5
            p1isaac(cl->out, 6);
        } else if (action == 602) {
            // INV_BUTTON1
            p1isaac(cl->out, 31);
        } else if (action == 892) {
            if ((b & 0x3) == 0) {
                _Client.oplogic9++;
            }

            if (_Client.oplogic9 >= 130) {
                // ANTICHEAT_OPLOGIC9
                p1isaac(cl->out, 238);
                p1(cl->out, 177);
            }

            // INV_BUTTON4
            p1isaac(cl->out, 38);
        } else if (action == 596) {
            // INV_BUTTON2
            p1isaac(cl->out, 59);
        }

        p2(cl->out, a);
        p2(cl->out, b);
        p2(cl->out, c);

        cl->selected_cycle = 0;
        cl->selectedInterface = c;
        cl->selectedItem = b;
        cl->selected_area = 2;

        if (_Component.instances[c]->layer == cl->viewport_interface_id) {
            cl->selected_area = 1;
        }

        if (_Component.instances[c]->layer == cl->chat_interface_id) {
            cl->selected_area = 3;
        }
    } else if (action == 581) {
        if ((a & 0x3) == 0) {
            _Client.oplogic1++;
        }

        if (_Client.oplogic1 >= 99) {
            // ANTICHEAT_OPLOGIC1
            p1isaac(cl->out, 7);
            p4(cl->out, 0);
        }

        // OPLOC4
        interactWithLoc(cl, 97, b, c, a);
    } else if (action == 965) {
        bool success = client_try_move(cl, cl->local_player->pathing_entity.pathTileX[0], cl->local_player->pathing_entity.pathTileZ[0], b, c, 2, 0, 0, 0, 0, 0, false);
        if (!success) {
            client_try_move(cl, cl->local_player->pathing_entity.pathTileX[0], cl->local_player->pathing_entity.pathTileZ[0], b, c, 2, 1, 1, 0, 0, 0, false);
        }

        cl->crossX = cl->shell->mouse_click_x;
        cl->crossY = cl->shell->mouse_click_y;
        cl->cross_mode = 2;
        cl->cross_cycle = 0;

        // OPOBJT
        p1isaac(cl->out, 138);
        p2(cl->out, b + cl->sceneBaseTileX);
        p2(cl->out, c + cl->sceneBaseTileZ);
        p2(cl->out, a);
        p2(cl->out, cl->activeSpellId);
    } else if (action == 1501) {
        _Client.oplogic6 += cl->sceneBaseTileZ;
        if (_Client.oplogic6 >= 92) {
            // ANTICHEAT_OPLOGIC6
            p1isaac(cl->out, 66);
            p4(cl->out, 0);
        }

        // OPLOC5
        interactWithLoc(cl, 116, b, c, a);
    } else if (action == 364) {
        // OPLOC3
        interactWithLoc(cl, 96, b, c, a);
    } else if (action == 1102) {
        ObjType *obj = objtype_get(a);
        char examine[MAX_STR];

        if (obj->desc[0] == '\0') {
            snprintf(examine,sizeof examine, "It's a %s.", obj->name);
        } else {
            snprintf(examine,sizeof examine,"%s",obj->desc);
        }
        client_examine_message(cl,examine);
    } else if (action == 960) {
        // IF_BUTTON
        ba_interface_trace("IF_BUTTON",c,"");
        p1isaac(cl->out, 155);
        p2(cl->out, c);

        Component *com = _Component.instances[c];
        if (com->scripts && com->scripts[0][0] == 5) {
            int varp = com->scripts[0][1];
            if (cl->varps[varp] != com->scriptOperand[0]) {
                cl->varps[varp] = com->scriptOperand[0];
                updateVarp(cl, varp);
                cl->redraw_sidebar = true;
            }
        }
    } else if (action == 34) {
        const char *option = cl->menu_option[optionId];
        int tag = indexof(option, "@whi@");

        if (tag != -1) {
            closeInterfaces(cl);

            strcpy(cl->reportAbuseInput, substring(option, tag + 5, strlen(option)));
            strtrim(cl->reportAbuseInput);
            cl->reportAbuseMuteOption = false;

            for (int i = 0; i < _Component.count; i++) {
                if (_Component.instances[i] && _Component.instances[i]->clientCode == 600) {
                    cl->reportAbuseInterfaceID = cl->viewport_interface_id = _Component.instances[i]->layer;
                    break;
                }
            }
        }
    } else if (action == 947) {
        closeInterfaces(cl);
    } else if (action == 367) {
        PlayerEntity *player = cl->players[a];
        if (player) {
            client_try_move(cl, cl->local_player->pathing_entity.pathTileX[0], cl->local_player->pathing_entity.pathTileZ[0], player->pathing_entity.pathTileX[0], player->pathing_entity.pathTileZ[0], 2, 1, 1, 0, 0, 0, false);

            cl->crossX = cl->shell->mouse_click_x;
            cl->crossY = cl->shell->mouse_click_y;
            cl->cross_mode = 2;
            cl->cross_cycle = 0;

            // OPPLAYERU
            p1isaac(cl->out, 248);
            p2(cl->out, a);
            p2(cl->out, cl->objInterface);
            p2(cl->out, cl->objSelectedSlot);
            p2(cl->out, cl->objSelectedInterface);
        }
    } else if (action == 465) {
        // IF_BUTTON
        ba_interface_trace("IF_BUTTON",c,"");
        p1isaac(cl->out, 155);
        p2(cl->out, c);

        Component *com = _Component.instances[c];
        if (com->scripts && com->scripts[0][0] == 5) {
            int varp = com->scripts[0][1];
            cl->varps[varp] = 1 - cl->varps[varp];
            updateVarp(cl, varp);
            cl->redraw_sidebar = true;
        }
    } else if (action == 406 || action == 436 || action == 557 || action == 556) {
        const char *option = cl->menu_option[optionId];
        int tag = indexof(option, "@whi@");

        if (tag != -1) {
            char *username = substring(option, tag + 5, strlen(option));
            strtrim(username);
            int64_t username37 = jstring_to_base37(username);
            if (action == 406) {
                addFriend(cl, username37);
            } else if (action == 436) {
                addIgnore(cl, username37);
            } else if (action == 557) {
                removeFriend(cl, username37);
            } else if (action == 556) {
                removeIgnore(cl, username37);
            }
        }
    } else if (action == 651) {
        PlayerEntity *player = cl->players[a];

        if (player) {
            client_try_move(cl, cl->local_player->pathing_entity.pathTileX[0], cl->local_player->pathing_entity.pathTileZ[0], player->pathing_entity.pathTileX[0], player->pathing_entity.pathTileZ[0], 2, 1, 1, 0, 0, 0, false);

            cl->crossX = cl->shell->mouse_click_x;
            cl->crossY = cl->shell->mouse_click_y;
            cl->cross_mode = 2;
            cl->cross_cycle = 0;

            // OPPLAYERT
            p1isaac(cl->out, 177);
            p2(cl->out, a);
            p2(cl->out, cl->activeSpellId);
        }
    }

    cl->obj_selected = 0;
    cl->spell_selected = 0;
    if (_Custom.item_outlines) {
        cl->redraw_sidebar = true;
    }
}

static void applyCutscene(Client *c) {
    int x = c->cutsceneSrcLocalTileX * 128 + 64;
    int z = c->cutsceneSrcLocalTileZ * 128 + 64;
    int y = getHeightmapY(c, c->currentLevel, c->cutsceneSrcLocalTileX, c->cutsceneSrcLocalTileZ) - c->cutsceneSrcHeight;

    if (c->cameraX < x) {
        c->cameraX += c->cutsceneMoveSpeed + (x - c->cameraX) * c->cutsceneMoveAcceleration / 1000;
        if (c->cameraX > x) {
            c->cameraX = x;
        }
    }

    if (c->cameraX > x) {
        c->cameraX -= c->cutsceneMoveSpeed + (c->cameraX - x) * c->cutsceneMoveAcceleration / 1000;
        if (c->cameraX < x) {
            c->cameraX = x;
        }
    }

    if (c->cameraY < y) {
        c->cameraY += c->cutsceneMoveSpeed + (y - c->cameraY) * c->cutsceneMoveAcceleration / 1000;
        if (c->cameraY > y) {
            c->cameraY = y;
        }
    }

    if (c->cameraY > y) {
        c->cameraY -= c->cutsceneMoveSpeed + (c->cameraY - y) * c->cutsceneMoveAcceleration / 1000;
        if (c->cameraY < y) {
            c->cameraY = y;
        }
    }

    if (c->cameraZ < z) {
        c->cameraZ += c->cutsceneMoveSpeed + (z - c->cameraZ) * c->cutsceneMoveAcceleration / 1000;
        if (c->cameraZ > z) {
            c->cameraZ = z;
        }
    }

    if (c->cameraZ > z) {
        c->cameraZ -= c->cutsceneMoveSpeed + (c->cameraZ - z) * c->cutsceneMoveAcceleration / 1000;
        if (c->cameraZ < z) {
            c->cameraZ = z;
        }
    }

    x = c->cutsceneDstLocalTileX * 128 + 64;
    z = c->cutsceneDstLocalTileZ * 128 + 64;
    y = getHeightmapY(c, c->currentLevel, c->cutsceneDstLocalTileX, c->cutsceneDstLocalTileZ) - c->cutsceneDstHeight;

    int deltaX = x - c->cameraX;
    int deltaY = y - c->cameraY;
    int deltaZ = z - c->cameraZ;

    int distance = (int)sqrt(deltaX * deltaX + deltaZ * deltaZ);
    int pitch = (int)(atan2(deltaY, distance) * 325.949) & 0x7ff;
    int yaw = (int)(atan2(deltaX, deltaZ) * -325.949) & 0x7ff;

    if (pitch < 128) {
        pitch = 128;
    }

    if (pitch > 383) {
        pitch = 383;
    }

    if (c->cameraPitch < pitch) {
        c->cameraPitch += c->cutsceneRotateSpeed + (pitch - c->cameraPitch) * c->cutsceneRotateAcceleration / 1000;
        if (c->cameraPitch > pitch) {
            c->cameraPitch = pitch;
        }
    }

    if (c->cameraPitch > pitch) {
        c->cameraPitch -= c->cutsceneRotateSpeed + (c->cameraPitch - pitch) * c->cutsceneRotateAcceleration / 1000;
        if (c->cameraPitch < pitch) {
            c->cameraPitch = pitch;
        }
    }

    int deltaYaw = yaw - c->cameraYaw;
    if (deltaYaw > 1024) {
        deltaYaw -= 2048;
    }

    if (deltaYaw < -1024) {
        deltaYaw += 2048;
    }

    if (deltaYaw > 0) {
        c->cameraYaw += c->cutsceneRotateSpeed + deltaYaw * c->cutsceneRotateAcceleration / 1000;
        c->cameraYaw &= 0x7ff;
    }

    if (deltaYaw < 0) {
        c->cameraYaw -= c->cutsceneRotateSpeed + -deltaYaw * c->cutsceneRotateAcceleration / 1000;
        c->cameraYaw &= 0x7ff;
    }

    int tmp = yaw - c->cameraYaw;
    if (tmp > 1024) {
        tmp -= 2048;
    }

    if (tmp < -1024) {
        tmp += 2048;
    }

    if ((tmp < 0 && deltaYaw > 0) || (tmp > 0 && deltaYaw < 0)) {
        c->cameraYaw = yaw;
    }
}

static void handleInputKey(Client *c) {
    while (true) {
        int key;
        do {
            while (true) {
                key = poll_key(c->shell);
                if (key == -1) {
                    return;
                }

                if (c->viewport_interface_id != -1 && c->viewport_interface_id == c->reportAbuseInterfaceID) {
                    size_t len = strlen(c->reportAbuseInput);
                    if (key == 8 && len > 0) {
                        c->reportAbuseInput[len - 1] = '\0';
                    }
                    break;
                }

                if (c->show_social_input) {
                    size_t len = strlen(c->social_input);
                    if (key >= 32 && key <= 122 && len < CHAT_LENGTH) {
                        c->social_input[len] = (char)key;
                        c->social_input[len + 1] = '\0';
                        c->redraw_chatback = true;
                    }

                    if (key == 8 && len > 0) {
                        c->social_input[len - 1] = '\0';
                        c->redraw_chatback = true;
                    }

                    if (key == 13 || key == 10) {
                        c->show_social_input = false;
                        c->redraw_chatback = true;

                        int64_t username;
                        if (c->social_action == 1) {
                            username = jstring_to_base37(c->social_input);
                            addFriend(c, username);
                        }

                        if (c->social_action == 2 && c->friend_count > 0) {
                            username = jstring_to_base37(c->social_input);
                            removeFriend(c, username);
                        }

                        if (c->social_action == 3 && len > 0) {
                            // MESSAGE_PRIVATE
                            p1isaac(c->out, 148);
                            p1(c->out, 0);
                            int start = c->out->pos;
                            p8(c->out, c->social_name37);
                            wordpack_pack(c->out, c->social_input);
                            psize1(c->out, c->out->pos - start);
                            jstring_to_sentence_case(c->social_input);
                            wordfilter_filter(c->social_input);
                            client_add_message(c, 6, c->social_input, jstring_format_name(jstring_from_base37(c->social_name37)));
                            if (c->private_chat_setting == 2) {
                                c->private_chat_setting = 1;
                                c->redraw_privacy_settings = true;
                                // CHAT_SETMODE
                                p1isaac(c->out, 244);
                                p1(c->out, c->public_chat_setting);
                                p1(c->out, c->private_chat_setting);
                                p1(c->out, c->trade_chat_setting);
                            }
                        }

                        if (c->social_action == 4 && c->ignoreCount < 100) {
                            username = jstring_to_base37(c->social_input);
                            addIgnore(c, username);
                        }

                        if (c->social_action == 5 && c->ignoreCount > 0) {
                            username = jstring_to_base37(c->social_input);
                            removeIgnore(c, username);
                        }
                    }
                } else if (c->chatback_input_open) {
                    size_t len = strlen(c->chatback_input);
                    if (key >= 48 && key <= 57 && len < CHATBACK_LENGTH) {
                        c->chatback_input[len] = (char)key;
                        c->chatback_input[len + 1] = '\0';
                        c->redraw_chatback = true;
                    }

                    if (key == 8 && len > 0) {
                        c->chatback_input[len - 1] = '\0';
                        c->redraw_chatback = true;
                    }

                    if (key == 13 || key == 10) {
                        if (len > 0) {
                            int value = 0;
                            // try {
                            value = atoi(c->chatback_input);
                            // } catch (Exception ignored) {
                            // }
                            // RESUME_P_COUNTDIALOG
                            p1isaac(c->out, 237);
                            p4(c->out, value);
                        }
                        c->chatback_input_open = false;
                        c->redraw_chatback = true;
                    }
                } else if (c->chat_interface_id == -1) {
                    size_t len = strlen(c->chat_typed);
                    if (_Custom.allow_debugprocs) {
                        if (key >= 32 && key <= 126 && len < CHAT_LENGTH) {
                            c->chat_typed[len] = (char)key;
                            c->chat_typed[len + 1] = '\0';
                            c->redraw_chatback = true;
                        }
                    } else {
                        if (key >= 32 && key <= 122 && len < CHAT_LENGTH) {
                            c->chat_typed[len] = (char)key;
                            c->chat_typed[len + 1] = '\0';
                            c->redraw_chatback = true;
                        }
                    }

                    if (key == 8 && len > 0) {
                        c->chat_typed[len - 1] = '\0';
                        c->redraw_chatback = true;
                    }

                    len = strlen(c->chat_typed);
                    if ((key == 13 || key == 10) && len > 0) {
                        // custom, originally only with frame or local servers
                        if (c->rights || _Custom.allow_commands) {
                            if (strcmp(c->chat_typed, "::clientdrop") == 0 /* && c->shell->window */) {
                                client_try_reconnect(c);
                            } else if (strcmp(c->chat_typed, "::noclip") == 0) {
                                for (int level = 0; level < 4; level++) {
                                    for (int x = 1; x < 104 - 1; x++) {
                                        for (int z = 1; z < 104 - 1; z++) {
                                            c->levelCollisionMap[level]->flags[x][z] = 0;
                                        }
                                    }
                                }
                            } else if (strcmp(c->chat_typed, "::debug") == 0) {
                                // _Custom.show_debug = !_Custom.show_debug;
                            } else if (strcmp(c->chat_typed, "::perf") == 0) {
                                _Custom.show_performance = !_Custom.show_performance;
                            } else if (strcmp(c->chat_typed, "::gl") == 0) {
                                _Custom.use_opengl11 = !_Custom.use_opengl11;

#ifdef GL11
                                char buf[MAX_STR];
                                sprintf(buf, "OpenGL renderer is now %s.", _Custom.use_opengl11 ? "enabled" : "disabled");
                                client_add_message(c, 0, buf, "");
#else
                                client_add_message(c, 0, "This client was not built with OpenGL support!", "");
#endif
                            } else if (strcmp(c->chat_typed, "::wf") == 0) {
#ifdef GL11
                                static bool wireframe;
                                if (!wireframe) {
                                    glPolygonMode(GL_FRONT, GL_LINE);
                                } else {
                                    glPolygonMode(GL_FRONT, GL_FILL);
                                }
                                wireframe = !wireframe;
#endif
                            } else if (strcmp(c->chat_typed, "::camera") == 0) {
                                // _Custom.camera_editor = !_Custom.camera_editor;
                                // c->cutscene = _Custom.camera_editor;
                                // c->cutsceneDstLocalTileX = 52;
                                // c->cutsceneDstLocalTileZ = 52;
                                // c->cutsceneSrcLocalTileX = 52;
                                // c->cutsceneSrcLocalTileZ = 52;
                                // c->cutsceneSrcHeight = 1000;
                                // c->cutsceneDstHeight = 1000;
                                // TODO
                                // } else if (c->chat_typed.startsWith("::camsrc ")) {
                                //     const char** args = c->chat_typed.split(" ");
                                //     if (args.length == 3) {
                                //         c->cutsceneSrcLocalTileX = atoi(args[1]);
                                //         c->cutsceneSrcLocalTileZ = atoi(args[2]);
                                //     } else if (args.length == 4) {
                                //         c->cutsceneSrcLocalTileX = atoi(args[1]);
                                //         c->cutsceneSrcLocalTileZ = atoi(args[2]);
                                //         c->cutsceneSrcHeight = atoi(args[3]);
                                //     }
                                // } else if (c->chat_typed.startsWith("::camdst ")) {
                                //     const char** args = c->chat_typed.split(" ");
                                //     if (args.length == 3) {
                                //         c->cutsceneDstLocalTileX = atoi(args[1]);
                                //         c->cutsceneDstLocalTileZ = atoi(args[2]);
                                //     } else if (args.length == 4) {
                                //         c->cutsceneDstLocalTileX = atoi(args[1]);
                                //         c->cutsceneDstLocalTileZ = atoi(args[2]);
                                //         c->cutsceneDstHeight = atoi(args[3]);
                                //     }
                                // }
                            }
                        }

                        if (strstartswith(c->chat_typed, "::")) {
                            // CLIENT_CHEAT
                            p1isaac(c->out, 4);
                            p1(c->out, len - 1);
                            char *sub = substring(c->chat_typed, 2, len);
                            pjstr(c->out, sub);
                            free(sub);
                        } else {
                            int8_t color = 0;
                            if (strstartswith(c->chat_typed, "yellow:")) {
                                color = 0;
                                strcpy(c->chat_typed, substring(c->chat_typed, 7, len));
                            } else if (strstartswith(c->chat_typed, "red:")) {
                                color = 1;
                                strcpy(c->chat_typed, substring(c->chat_typed, 4, len));
                            } else if (strstartswith(c->chat_typed, "green:")) {
                                color = 2;
                                strcpy(c->chat_typed, substring(c->chat_typed, 6, len));
                            } else if (strstartswith(c->chat_typed, "cyan:")) {
                                color = 3;
                                strcpy(c->chat_typed, substring(c->chat_typed, 5, len));
                            } else if (strstartswith(c->chat_typed, "purple:")) {
                                color = 4;
                                strcpy(c->chat_typed, substring(c->chat_typed, 7, len));
                            } else if (strstartswith(c->chat_typed, "white:")) {
                                color = 5;
                                strcpy(c->chat_typed, substring(c->chat_typed, 6, len));
                            } else if (strstartswith(c->chat_typed, "flash1:")) {
                                color = 6;
                                strcpy(c->chat_typed, substring(c->chat_typed, 7, len));
                            } else if (strstartswith(c->chat_typed, "flash2:")) {
                                color = 7;
                                strcpy(c->chat_typed, substring(c->chat_typed, 7, len));
                            } else if (strstartswith(c->chat_typed, "flash3:")) {
                                color = 8;
                                strcpy(c->chat_typed, substring(c->chat_typed, 7, len));
                            } else if (strstartswith(c->chat_typed, "glow1:")) {
                                color = 9;
                                strcpy(c->chat_typed, substring(c->chat_typed, 6, len));
                            } else if (strstartswith(c->chat_typed, "glow2:")) {
                                color = 10;
                                strcpy(c->chat_typed, substring(c->chat_typed, 6, len));
                            } else if (strstartswith(c->chat_typed, "glow3:")) {
                                color = 11;
                                strcpy(c->chat_typed, substring(c->chat_typed, 6, len));
                            }

                            int8_t effect = 0;
                            if (strstartswith(c->chat_typed, "wave:")) {
                                effect = 1;
                                strcpy(c->chat_typed, substring(c->chat_typed, 5, strlen(c->chat_typed)));
                            }
                            if (strstartswith(c->chat_typed, "scroll:")) {
                                effect = 2;
                                strcpy(c->chat_typed, substring(c->chat_typed, 7, strlen(c->chat_typed)));
                            }

                            // MESSAGE_PUBLIC
                            p1isaac(c->out, 158);
                            p1(c->out, 0);
                            int start = c->out->pos;
                            p1(c->out, color);
                            p1(c->out, effect);
                            wordpack_pack(c->out, c->chat_typed);
                            psize1(c->out, c->out->pos - start);

                            jstring_to_sentence_case(c->chat_typed);
                            wordfilter_filter(c->chat_typed);
                            strcpy(c->local_player->pathing_entity.chat, c->chat_typed);
                            c->local_player->pathing_entity.chatColor = color;
                            c->local_player->pathing_entity.chatStyle = effect;
                            c->local_player->pathing_entity.chatTimer = 150;
                            client_add_message(c, 2, c->local_player->pathing_entity.chat, c->local_player->name);

                            if (c->public_chat_setting == 2) {
                                c->public_chat_setting = 3;
                                c->redraw_privacy_settings = true;
                                // CHAT_SETMODE
                                p1isaac(c->out, 244);
                                p1(c->out, c->public_chat_setting);
                                p1(c->out, c->private_chat_setting);
                                p1(c->out, c->trade_chat_setting);
                            }
                        }

                        c->chat_typed[0] = '\0';
                        c->redraw_chatback = true;
                    }
                }
            }
        } while ((key < 97 || key > 122) && (key < 65 || key > 90) && (key < 48 || key > 57) && key != 32);

        size_t len = strlen(c->reportAbuseInput);
        if (len < REPORT_ABUSE_LENGTH) {
            c->reportAbuseInput[len] = (char)key;
            c->reportAbuseInput[len + 1] = '\0';
        }
    }
}

#include "../r91_popup_dismiss.inc"

static void handleMouseInput(Client *c) {
    if(client_dismiss_world_popup(c))return;
    if (ui474_pointer(c,c->shell->mouse_click_x,c->shell->mouse_click_y,c->shell->mouse_click_button)) return;
    if (c->obj_drag_area != 0) {
        return;
    }

#ifdef __vita__
    static int menu_mouse_x = -1, menu_mouse_y = -1;
    if (c->menu_visible && (c->shell->mouse_x != menu_mouse_x || c->shell->mouse_y != menu_mouse_y)) {
        int ox=c->menu_area==0?(ve_enabled()?0:8):c->menu_area==1?ui474_side_x():22;
        int oy=c->menu_area==0?(ve_enabled()?0:11):c->menu_area==1?ui474_side_y():(ve_enabled()?ve_chat_text_y():lc_y(LC_CHAT,UI_BOTTOM(375)));
        int mx=c->shell->mouse_x-ox,my=c->shell->mouse_y-oy;
        for(int i=0;i<c->menu_size;i++) {
            int row=c->menu_y+(c->menu_size-1-i)*15+31;
            if(mx>c->menu_x&&mx<c->menu_x+c->menu_width&&my>row-13&&my<row+3)c->vita_menu_selected=i;
        }
    }
    menu_mouse_x=c->shell->mouse_x;menu_mouse_y=c->shell->mouse_y;
    if (!c->menu_visible) {
        c->vita_menu_active = false;
        c->vita_menu_confirm = c->vita_menu_cancel = false;
    } else if (c->vita_menu_cancel || c->vita_menu_confirm) {
        bool confirm = c->vita_menu_confirm && !c->vita_menu_cancel;
        c->vita_menu_confirm = c->vita_menu_cancel = false;
        if (confirm && c->vita_menu_selected >= 0 && c->vita_menu_selected < c->menu_size) {
            useMenuOption(c, c->vita_menu_selected);
        }
        c->menu_visible = false;
        c->vita_menu_active = false;
        if (c->menu_area == 1) c->redraw_sidebar = true;
        if (c->menu_area == 2) c->redraw_chatback = true;
        return;
    }
#endif

    int button = c->shell->mouse_click_button;
    if (c->spell_selected == 1 && c->shell->mouse_click_x >= UI_RIGHT(520) && c->shell->mouse_click_y >= UI_BOTTOM(165) && c->shell->mouse_click_x <= UI_RIGHT(788) && c->shell->mouse_click_y <= UI_BOTTOM(230)) {
        button = 0;
    }

    if (c->menu_visible) {
#ifdef __vita__
        if (button != 1 && !c->vita_menu_active) {
#else
        if (button != 1) {
#endif
            int x = c->shell->mouse_x;
            int y = c->shell->mouse_y;

            if (c->menu_area == 0) {
                x -= ve_enabled()?0:8;
                y -= ve_enabled()?0:11;
            } else if (c->menu_area == 1) {
                x -= ui474_side_x();
                y -= ui474_side_y();
            } else if (c->menu_area == 2) {
                x -= 22;
                y -= (ve_enabled()?ve_chat_text_y():lc_y(LC_CHAT,UI_BOTTOM(375)));
            }

            if (x < c->menu_x - 10 || x > c->menu_x + c->menu_width + 10 || y < c->menu_y - 10 || y > c->menu_y + c->menu_height + 10) {
                c->menu_visible = false;
                if (c->menu_area == 1) {
                    c->redraw_sidebar = true;
                }
                if (c->menu_area == 2) {
                    c->redraw_chatback = true;
                }
            }
        }

        if (button == 1) {
            int menuX = c->menu_x;
            int menuY = c->menu_y;
            int menuWidth = c->menu_width;

            int clickX = c->shell->mouse_click_x;
            int clickY = c->shell->mouse_click_y;

            if (c->menu_area == 0) {
                clickX -= ve_enabled()?0:8;
                clickY -= ve_enabled()?0:11;
            } else if (c->menu_area == 1) {
                clickX -= ui474_side_x();
                clickY -= ui474_side_y();
            } else if (c->menu_area == 2) {
                clickX -= 22;
                clickY -= (ve_enabled()?ve_chat_text_y():lc_y(LC_CHAT,UI_BOTTOM(375)));
            }

            int option = -1;
            for (int i = 0; i < c->menu_size; i++) {
                int optionY = menuY + (c->menu_size - 1 - i) * 15 + 31;
                if (clickX > menuX && clickX < menuX + menuWidth && clickY > optionY - 13 && clickY < optionY + 3) {
                    option = i;
                }
            }

            if (option != -1) {
                useMenuOption(c, option);
            }

            c->menu_visible = false;
            if (c->menu_area == 1) {
                c->redraw_sidebar = true;
            } else if (c->menu_area == 2) {
                c->redraw_chatback = true;
            }
        }
    } else {
        if (button == 1 && c->menu_size > 0) {
            int action = c->menu_action[c->menu_size - 1];

            if (action == 602 || action == 596 || action == 22 || action == 892 || action == 415 || action == 405 || action == 38 || action == 422 || action == 478 || action == 347 || action == 188) {
                int slot = c->menuParamB[c->menu_size - 1];
                int comId = c->menuParamC[c->menu_size - 1];
                Component *com = _Component.instances[comId];

                if (com->draggable) {
                    c->objGrabThreshold = false;
                    c->obj_drag_cycles = 0;
                    c->objDragInterfaceId = comId;
                    c->objDragSlot = slot;
                    c->obj_drag_area = 2;
                    c->objGrabX = c->shell->mouse_click_x;
                    c->objGrabY = c->shell->mouse_click_y;

                    if (_Component.instances[comId]->layer == c->viewport_interface_id) {
                        c->obj_drag_area = 1;
                    }

                    if (_Component.instances[comId]->layer == c->chat_interface_id) {
                        c->obj_drag_area = 3;
                    }

                    return;
                }
            }
        }

        if (button == 1 && (c->mouseButtonsOption == 1 || isAddFriendOption(c, c->menu_size - 1)) && c->menu_size > 2) {
            button = 2;
        }

        if (button == 1 && c->menu_size > 0) {
            useMenuOption(c, c->menu_size - 1);
        }

        if (button != 2 || c->menu_size <= 0) {
            return;
        }

        showContextMenu(c);
    }
}

static void handleMinimapInput(Client *c) {
    if(c->shell->mouse_click_button==1){char msg[96];snprintf(msg,sizeof(msg),"x=%d y=%d",c->shell->mouse_click_x,c->shell->mouse_click_y);ba_interface_trace("MINIMAP_INPUT",0,msg);}
    if((ve_enabled()||lc_game_altered())&&!lc_contains(lc_game_geometry(LC_MINIMAP),c->shell->mouse_click_x,c->shell->mouse_click_y,true))return;
    if (c->shell->mouse_click_button == 1) {
        int x,y;
        if(ve_enabled()){
            if(!ve_map_input_position(c->shell->mouse_click_x,c->shell->mouse_click_y,&x,&y))return;
        }else{
            x=c->shell->mouse_click_x-(21+lc_x(LC_MINIMAP,UI_RIGHT(561)));
            y=c->shell->mouse_click_y-(9+lc_y(LC_MINIMAP,5));
        }
        if(ve_enabled()&&!ve_map_point(x,y))return;

        if (x >= 0 && y >= 0 && x < 146 && y < 151) {
            x -= 73;
            y -= 75;

            int yaw = c->orbit_camera_yaw + c->minimap_anticheat_angle & 0x7ff;
            int sinYaw = _Pix3D.sin_table[yaw];
            int cosYaw = _Pix3D.cos_table[yaw];

            sinYaw = (sinYaw * (c->minimap_zoom + 256)) >> 8;
            cosYaw = (cosYaw * (c->minimap_zoom + 256)) >> 8;

            int relX = (y * sinYaw + x * cosYaw) >> 11;
            int relY = (y * cosYaw - x * sinYaw) >> 11;

            int tileX = (c->local_player->pathing_entity.x + relX) >> 7;
            int tileZ = (c->local_player->pathing_entity.z - relY) >> 7;

            bool success = client_try_move(c, c->local_player->pathing_entity.pathTileX[0], c->local_player->pathing_entity.pathTileZ[0], tileX, tileZ, 1, 0, 0, 0, 0, 0, true);
            if(ve_enabled()){FILE*trace=fopen("ux0:/data/RS2Vita/minimap-input.log","a");if(trace){fprintf(trace,"screen=%d,%d start=%d,%d target=%d,%d accepted=%d\n",c->shell->mouse_click_x,c->shell->mouse_click_y,c->sceneBaseTileX+c->local_player->pathing_entity.x/128,c->sceneBaseTileZ+c->local_player->pathing_entity.z/128,c->sceneBaseTileX+tileX,c->sceneBaseTileZ+tileZ,success);fclose(trace);}}
            {char msg[128];snprintf(msg,sizeof(msg),"from=%d,%d dest=%d,%d success=%d",c->local_player->pathing_entity.pathTileX[0]+c->sceneBaseTileX,c->local_player->pathing_entity.pathTileZ[0]+c->sceneBaseTileZ,tileX+c->sceneBaseTileX,tileZ+c->sceneBaseTileZ,success);ba_interface_trace("MINIMAP_ROUTE",0,msg);}
            if (success) {
                // the additional 14-bytes in MOVE_MINIMAPCLICK
                p1(c->out, x);
                p1(c->out, y);
                p2(c->out, c->orbit_camera_yaw);
                p1(c->out, 57);
                p1(c->out, c->minimap_anticheat_angle);
                p1(c->out, c->minimap_zoom);
                p1(c->out, 89);
                p2(c->out, c->local_player->pathing_entity.x);
                p2(c->out, c->local_player->pathing_entity.z);
                p1(c->out, c->tryMoveNearest);
                p1(c->out, 63);
            }
        }
    }
}

#include "r23_tutorial.h"

static void handleTabInput(Client *c) {
    if (ui474_enabled()) return;
    int layout_click_x = c->shell->mouse_click_x-lc_dx(LC_SIDEBAR);
    int layout_click_y = c->shell->mouse_click_y-lc_dy(LC_SIDEBAR);
#ifdef VITA_NATIVE_WIDESCREEN
    layout_click_x -= 171;
    if (layout_click_y >= 177) layout_click_y -= 12;
#endif

    if (c->shell->mouse_click_button != 1) {
        return;
    }

    if (layout_click_x >= 549 && layout_click_x <= 583 && layout_click_y >= 195 && layout_click_y < 231 && c->tab_interface_id[0] != -1) {
        c->redraw_sidebar = true;
        c->selected_tab = 0;
        c->redraw_sideicons = true;
    } else if (layout_click_x >= 579 && layout_click_x <= 609 && layout_click_y >= 194 && layout_click_y < 231 && c->tab_interface_id[1] != -1) {
        c->redraw_sidebar = true;
        c->selected_tab = 1;
        c->redraw_sideicons = true;
    } else if (layout_click_x >= 607 && layout_click_x <= 637 && layout_click_y >= 194 && layout_click_y < 231 && c->tab_interface_id[2] != -1) {
        c->redraw_sidebar = true;
        c->selected_tab = 2;
        c->redraw_sideicons = true;
    } else if (layout_click_x >= 635 && layout_click_x <= 679 && layout_click_y >= 194 && layout_click_y < 229 && c->tab_interface_id[3] != -1) {
        c->redraw_sidebar = true;
        c->selected_tab = 3;
        c->redraw_sideicons = true;
    } else if (layout_click_x >= 676 && layout_click_x <= 706 && layout_click_y >= 194 && layout_click_y < 231 && c->tab_interface_id[4] != -1) {
        c->redraw_sidebar = true;
        c->selected_tab = 4;
        c->redraw_sideicons = true;
    } else if (layout_click_x >= 704 && layout_click_x <= 734 && layout_click_y >= 194 && layout_click_y < 231 && c->tab_interface_id[5] != -1) {
        c->redraw_sidebar = true;
        c->selected_tab = 5;
        c->redraw_sideicons = true;
    } else if (layout_click_x >= 732 && layout_click_x <= 766 && layout_click_y >= 195 && layout_click_y < 231 && c->tab_interface_id[6] != -1) {
        c->redraw_sidebar = true;
        c->selected_tab = 6;
        c->redraw_sideicons = true;
    } else if (layout_click_x >= 550 && layout_click_x <= 584 && layout_click_y >= 492 && layout_click_y < 528 && c->tab_interface_id[7] != -1) {
        c->redraw_sidebar = true;
        c->selected_tab = 7;
        c->redraw_sideicons = true;
    } else if (layout_click_x >= 582 && layout_click_x <= 612 && layout_click_y >= 492 && layout_click_y < 529 && c->tab_interface_id[8] != -1) {
        c->redraw_sidebar = true;
        c->selected_tab = 8;
        c->redraw_sideicons = true;
    } else if (layout_click_x >= 609 && layout_click_x <= 639 && layout_click_y >= 492 && layout_click_y < 529 && c->tab_interface_id[9] != -1) {
        c->redraw_sidebar = true;
        c->selected_tab = 9;
        c->redraw_sideicons = true;
    } else if (layout_click_x >= 637 && layout_click_x <= 681 && layout_click_y >= 493 && layout_click_y < 528 && c->tab_interface_id[10] != -1) {
        c->redraw_sidebar = true;
        c->selected_tab = 10;
        c->redraw_sideicons = true;
    } else if (layout_click_x >= 679 && layout_click_x <= 709 && layout_click_y >= 492 && layout_click_y < 529 && c->tab_interface_id[11] != -1) {
        c->redraw_sidebar = true;
        c->selected_tab = 11;
        c->redraw_sideicons = true;
    } else if (layout_click_x >= 706 && layout_click_x <= 736 && layout_click_y >= 492 && layout_click_y < 529 && c->tab_interface_id[12] != -1) {
        c->redraw_sidebar = true;
        c->selected_tab = 12;
        c->redraw_sideicons = true;
    } else if (layout_click_x >= 734 && layout_click_x <= 768 && layout_click_y >= 492 && layout_click_y < 528 && c->tab_interface_id[13] != -1) {
        c->redraw_sidebar = true;
        c->selected_tab = 13;
        c->redraw_sideicons = true;
    }

    _Client.cyclelogic1++;
    if (_Client.cyclelogic1 > 150) {
        _Client.cyclelogic1 = 0;
        // ANTICHEAT_CYCLELOGIC1
        p1isaac(c->out, 233);
        p1(c->out, 43);
    }
}

static void handleChatSettingsInput(Client *c) {
#ifdef BUILD_A_EMBEDDED
    if(c->shell->mouse_click_button){FILE*f=fopen("ux0:/data/RS2Vita/input-events.csv","a");if(f){fprintf(f,"%llu,CHAT_SETTINGS,%d,%d,%d,%d\n",(unsigned long long)ba_timing_now(),c->shell->mouse_click_x,c->shell->mouse_click_y,c->shell->mouse_click_button,ve_enabled()?ve_chat_button_at(c->shell->mouse_click_x,c->shell->mouse_click_y):-1);fclose(f);}}
#endif
    if(lc_game_altered()&&!lc_contains(lc_game_geometry(LC_CHAT),c->shell->mouse_click_x,c->shell->mouse_click_y,true))return;
    int layout_click_x = c->shell->mouse_click_x-lc_dx(LC_CHAT);
    int layout_click_y = c->shell->mouse_click_y-lc_dy(LC_CHAT);
#ifdef VITA_NATIVE_WIDESCREEN
    layout_click_y -= 12;
#endif

    if(ve_enabled()){if(c->shell->mouse_click_button!=1)return;int cell=ve_chat_button_at(c->shell->mouse_click_x,c->shell->mouse_click_y);if(cell<0)return;if(cell==4){ve_chat_toggle(c);return;}if(cell==5){p1isaac(c->out,155);p2(c->out,7746);return;}layout_click_x=84+168*cell;layout_click_y=506;}
    if (c->shell->mouse_click_button != 1) {
        return;
    }

    if (layout_click_x >= CHAT_BUTTON_LEFT(8, 0) && layout_click_x <= CHAT_BUTTON_RIGHT(108, 0) && layout_click_y >= 490 && layout_click_y <= 522) {
        c->public_chat_setting = (c->public_chat_setting + 1) % 4;
        ve_privacy_save(c);
        c->redraw_privacy_settings = true;
        c->redraw_chatback = true;

        // CHAT_SETMODE
        p1isaac(c->out, 244);
        p1(c->out, c->public_chat_setting);
        p1(c->out, c->private_chat_setting);
        p1(c->out, c->trade_chat_setting);
    } else if (layout_click_x >= CHAT_BUTTON_LEFT(137, 1) && layout_click_x <= CHAT_BUTTON_RIGHT(237, 1) && layout_click_y >= 490 && layout_click_y <= 522) {
        c->private_chat_setting = (c->private_chat_setting + 1) % 3;
        ve_privacy_save(c);
        c->redraw_privacy_settings = true;
        c->redraw_chatback = true;

        // CHAT_SETMODE
        p1isaac(c->out, 244);
        p1(c->out, c->public_chat_setting);
        p1(c->out, c->private_chat_setting);
        p1(c->out, c->trade_chat_setting);
    } else if (layout_click_x >= CHAT_BUTTON_LEFT(275, 2) && layout_click_x <= CHAT_BUTTON_RIGHT(375, 2) && layout_click_y >= 490 && layout_click_y <= 522) {
        c->trade_chat_setting = (c->trade_chat_setting + 1) % 3;
        ve_privacy_save(c);
        c->redraw_privacy_settings = true;
        c->redraw_chatback = true;

        // CHAT_SETMODE
        p1isaac(c->out, 244);
        p1(c->out, c->public_chat_setting);
        p1(c->out, c->private_chat_setting);
        p1(c->out, c->trade_chat_setting);
    } else if (layout_click_x >= CHAT_BUTTON_LEFT(416, 3) && layout_click_x <= MIN(CHAT_CONTROLS_WIDTH - 1, CHAT_BUTTON_RIGHT(516, 3)) && layout_click_y >= 490 && layout_click_y <= 522) {
        closeInterfaces(c);

        c->reportAbuseInput[0] = '\0';
        c->reportAbuseMuteOption = false;

        for (int i = 0; i < _Component.count; i++) {
            if (_Component.instances[i] && _Component.instances[i]->clientCode == 600) {
                c->reportAbuseInterfaceID = c->viewport_interface_id = _Component.instances[i]->layer;
                return;
            }
        }
    }
}

static void client_scenemap_free(Client *c) {
    for (int i = 0; i < c->sceneMapIndexLength; i++) {
        free(c->sceneMapLandData[i]);
        free(c->sceneMapLocData[i]);
    }
    free(c->sceneMapLandData);
    free(c->sceneMapLocData);
    free(c->sceneMapIndex);
    free(c->sceneMapLandDataIndexLength);
    free(c->sceneMapLocDataIndexLength);
}

void client_update_game(Client *c) {
    if (c->system_update_timer > 1) {
        c->system_update_timer--;
    }

    if (c->idle_timeout > 0) {
        c->idle_timeout--;
    }

    for (int i = 0; i < 5 && client_read(c); i++) {
    }

    if (c->ingame) {
        for (int wave = 0; wave < c->wave_count; wave++) {
            if (c->wave_delay[wave] <= 0) {
                // deprecated code unused to save wav for the browser to play it
                // bool failed = false;
                // try {
                // if (c->wave_ids[wave] != c->last_wave_id || c->wave_loops[wave] != c->last_wave_loops) {
                uint64_t sound_begin=rs2_now();
                Packet *buf = wave_generate(c->wave_ids[wave], c->wave_loops[wave]);
                r94_audio_record(0,rs2_now()-sound_begin);

                if (buf && rs2_now() + (uint64_t)(buf->pos / 22) > c->last_wave_start_time + (uint64_t)(c->last_wave_length / 22)) {
                    c->last_wave_length = buf->pos;
                    c->last_wave_start_time = rs2_now();
                    // if (c->saveWave(buf->data, buf->pos)) {
                    c->last_wave_id = c->wave_ids[wave];
                    c->last_wave_loops = c->wave_loops[wave];
#ifndef __vita__
                    if(getenv("NATURE_AUDIO_CAPTURE_DIR"))rs2_log("NATURE_AUDIO_EVENT id=%d loops=%d bytes=%d ticks=%llu\n",c->wave_ids[wave],c->wave_loops[wave],buf->pos,(unsigned long long)rs2_now());
#endif
                    sound_begin=rs2_now();
                    r108_sound_dispatched[c->wave_ids[wave]]++;
                    platform_play_wave(buf->data, buf->pos);
                    r94_audio_record(1,rs2_now()-sound_begin);
                    // } else {
                    // 	failed = true;
                    // }
                }
                // } else if (!c->replayWave()) {
                // 	failed = true;
                // }
                // } catch (Exception ignored) {
                // }

                // if (failed && c->wave_delay[wave] != -5) {
                // 	c->wave_delay[wave] = -5;
                // } else {
                c->wave_count--;
                for (int i = wave; i < c->wave_count; i++) {
                    c->wave_ids[i] = c->wave_ids[i + 1];
                    c->wave_loops[i] = c->wave_loops[i + 1];
                    c->wave_delay[i] = c->wave_delay[i + 1];
                }
                wave--;
                // }
            } else {
                c->wave_delay[wave]--;
            }
        }

        if (c->nextMusicDelay > 0) {
            c->nextMusicDelay -= 20;
            if (c->nextMusicDelay < 0) {
                c->nextMusicDelay = 0;
            }
            if (c->nextMusicDelay == 0 && c->midiActive && !_Client.lowmem) {
                platform_set_midi(c->currentMidi, c->midiCrc, c->midiSize);
            }
        }

        Packet *tracking = inputtracking_flush(&_InputTracking);
        if (tracking) {
            // EVENT_TRACKING
            p1isaac(c->out, 81);
            p2(c->out, tracking->pos);
            pdata(c->out, tracking->data, tracking->pos, 0);
            packet_release(tracking);
        }

        c->idle_net_cycles++;
        if (c->idle_net_cycles > 750) {
            client_try_reconnect(c);
        }

        updatePlayers(c);
        updateNpcs(c);
        updateEntityChats(c);
        updateMergeLocs(c);

        if ((c->shell->action_key[1] == 1 || c->shell->action_key[2] == 1 || c->shell->action_key[3] == 1 || c->shell->action_key[4] == 1) && c->camera_moved_write++ > 5) {
            c->camera_moved_write = 0;
            // EVENT_CAMERA_POSITION
            p1isaac(c->out, 189);
            p2(c->out, c->orbit_camera_pitch);
            p2(c->out, c->orbit_camera_yaw);
            p1(c->out, c->minimap_anticheat_angle);
            p1(c->out, c->minimap_zoom);
        }

        c->scene_delta++;
        ba_timing_record(BA_SCENE_DELTA,0);
        if (c->cross_mode != 0) {
            c->cross_cycle += 20;
            if (c->cross_cycle >= 400) {
                c->cross_mode = 0;
            }
        }

        if (c->selected_area != 0) {
            c->selected_cycle++;
            if (c->selected_cycle >= 15) {
                if (c->selected_area == 2) {
                    c->redraw_sidebar = true;
                }
                if (c->selected_area == 3) {
                    c->redraw_chatback = true;
                }
                c->selected_area = 0;
            }
        }

        if (c->obj_drag_area != 0) {
            c->obj_drag_cycles++;
            if (c->shell->mouse_x > c->objGrabX + 5 || c->shell->mouse_x < c->objGrabX - 5 || c->shell->mouse_y > c->objGrabY + 5 || c->shell->mouse_y < c->objGrabY - 5) {
                c->objGrabThreshold = true;
            }

            if (c->shell->mouse_button == 0) {
                if (c->obj_drag_area == 2) {
                    c->redraw_sidebar = true;
                }
                if (c->obj_drag_area == 3) {
                    c->redraw_chatback = true;
                }

                c->obj_drag_area = 0;
                if (c->objGrabThreshold && c->obj_drag_cycles >= 5) {
                    c->hoveredSlotParentId = -1;
                    client_handle_input(c);
                    if (c->hoveredSlotParentId == c->objDragInterfaceId && c->hoveredSlot != c->objDragSlot) {
                        Component *com = _Component.instances[c->objDragInterfaceId];
                        int obj = com->invSlotObjId[c->hoveredSlot];
                        com->invSlotObjId[c->hoveredSlot] = com->invSlotObjId[c->objDragSlot];
                        com->invSlotObjId[c->objDragSlot] = obj;

                        int count = com->invSlotObjCount[c->hoveredSlot];
                        com->invSlotObjCount[c->hoveredSlot] = com->invSlotObjCount[c->objDragSlot];
                        com->invSlotObjCount[c->objDragSlot] = count;

                        // INV_BUTTOND
                        p1isaac(c->out, 159);
                        p2(c->out, c->objDragInterfaceId);
                        p2(c->out, c->objDragSlot);
                        p2(c->out, c->hoveredSlot);
                    }
                } else if ((c->mouseButtonsOption == 1 || isAddFriendOption(c, c->menu_size - 1)) && c->menu_size > 2) {
                    showContextMenu(c);
                } else if (c->menu_size > 0) {
                    useMenuOption(c, c->menu_size - 1);
                }

                c->selected_cycle = 10;
                c->shell->mouse_click_button = 0;
            }
        }

        _Client.cyclelogic3++;
        if (_Client.cyclelogic3 > 127) {
            _Client.cyclelogic3 = 0;
            // ANTICHEAT_CYCLELOGIC3
            p1isaac(c->out, 215);
            p3(c->out, 4991788);
        }

        if (_World3D.clickTileX != -1) {
            int x = _World3D.clickTileX;
            int z = _World3D.clickTileZ;
            bool success = client_try_move(c, c->local_player->pathing_entity.pathTileX[0], c->local_player->pathing_entity.pathTileZ[0], x, z, 0, 0, 0, 0, 0, 0, true);
            _World3D.clickTileX = -1;
            {char msg[96];snprintf(msg,sizeof(msg),"dest=%d,%d success=%d",x+c->sceneBaseTileX,z+c->sceneBaseTileZ,success);ba_interface_trace("WORLD_ROUTE",0,msg);}

            if (success) {
                c->crossX = c->shell->mouse_click_x;
                c->crossY = c->shell->mouse_click_y;
                c->cross_mode = 1;
                c->cross_cycle = 0;
            }
        }

        if (c->shell->mouse_click_button == 1 && c->modal_message[0]) {
            c->modal_message[0] = '\0';
            c->redraw_chatback = true;
            c->shell->mouse_click_button = 0;
        }

        handleMouseInput(c);
        handleMinimapInput(c);
        handleTabInput(c);
        r23_tutorial_tab_ack(c);
        /* Only redraw the retained sidebar at flash edges. Hit regions and
         * the rest of the Enhanced HUD keep their existing retained path. */
        if(ui474_enabled()){
            static int tutorial_visual_key=-2;
            int key=r23_tutorial_visual_key(c->flashing_tab,_Client.loop_cycle);
            if(key!=tutorial_visual_key){tutorial_visual_key=key;c->redraw_sidebar=true;}
        }
        handleChatSettingsInput(c);

        if (c->shell->mouse_button == 1 || c->shell->mouse_click_button == 1) {
            c->drag_cycles++;
        }

        if (c->scene_state == 2) {
            // NOTE unused
            // if (_Custom.camera_editor) {
            //     update_camera_editor(c);
            // } else {
            client_update_orbit_camera(c);
            // }
        }
        if (c->scene_state == 2 && c->cutscene) {
            applyCutscene(c);
        }

        for (int i = 0; i < 5; i++) {
            c->cameraModifierCycle[i]++;
        }

        handleInputKey(c);
        c->shell->idle_cycles++;
        if (c->shell->idle_cycles > 4500) {
            c->idle_timeout = 250;
            c->shell->idle_cycles -= 500;
            // IDLE_TIMER
            p1isaac(c->out, 70);
        }

        c->cameraOffsetCycle++;
        if (c->cameraOffsetCycle > 500) {
            c->cameraOffsetCycle = 0;
            int _rand = (int)(jrand() * 8.0);
            if ((_rand & 0x1) == 1) {
                c->camera_anticheat_offset_x += c->cameraOffsetXModifier;
            }
            if ((_rand & 0x2) == 2) {
                c->camera_anticheat_offset_z += c->cameraOffsetZModifier;
            }
            if ((_rand & 0x4) == 4) {
                c->camera_anticheat_angle += c->cameraOffsetYawModifier;
            }
        }

        if (c->camera_anticheat_offset_x < -50) {
            c->cameraOffsetXModifier = 2;
        }
        if (c->camera_anticheat_offset_x > 50) {
            c->cameraOffsetXModifier = -2;
        }
        if (c->camera_anticheat_offset_z < -55) {
            c->cameraOffsetZModifier = 2;
        }
        if (c->camera_anticheat_offset_z > 55) {
            c->cameraOffsetZModifier = -2;
        }
        if (c->camera_anticheat_angle < -40) {
            c->cameraOffsetYawModifier = 1;
        }
        if (c->camera_anticheat_angle > 40) {
            c->cameraOffsetYawModifier = -1;
        }

        c->cameraOffsetCycle++;
        if (c->cameraOffsetCycle > 500) {
            c->cameraOffsetCycle = 0;
            int random = (int)(jrand() * 8.0);
            if ((random & 0x1) == 1) {
                c->minimap_anticheat_angle += c->minimapAngleModifier;
            }
            if ((random & 0x2) == 2) {
                c->minimap_zoom += c->minimapZoomModifier;
            }
        }

        if (c->minimap_anticheat_angle < -60) {
            c->minimapAngleModifier = 2;
        }
        if (c->minimap_anticheat_angle > 60) {
            c->minimapAngleModifier = -2;
        }

        if (c->minimap_zoom < -20) {
            c->minimapZoomModifier = 1;
        }
        if (c->minimap_zoom > 10) {
            c->minimapZoomModifier = -1;
        }

        _Client.cyclelogic4++;
        if (_Client.cyclelogic4 > 110) {
            _Client.cyclelogic4 = 0;
            // ANTICHEAT_CYCLELOGIC4
            p1isaac(c->out, 236);
            p4(c->out, 0);
        }

        c->heartbeatTimer++;
        if (c->heartbeatTimer > 50) {
            // NO_TIMEOUT
            p1isaac(c->out, 108);
        }

        // try {
        if (c->stream && c->out->pos > 0) {
            clientstream_write(c->stream, c->out->data, c->out->pos, 0);
            c->out->pos = 0;
            c->heartbeatTimer = 0;
        }
        // NOTE: no catch for logout or reconn
        // } catch (IOException ignored) {
        // client_try_reconnect(c);
        // } catch (Exception ignored) {
        // client_logout(c);
        // }
    }
}

bool client_read(Client *c) {
    if (!c->stream) {
        return false;
    }

    // try {
    if (!clientstream_available(c->stream, 1)) {
        return false;
    }

    if (c->packet_type == -1) {
        clientstream_read_bytes(c->stream, c->in->data, 0, 1);
        c->packet_type = c->in->data[0] & 0xff;
        // if (c->random_in) {
        c->packet_type = (c->packet_type - isaac_next(&c->random_in)) & 0xff;
        // }
        c->packet_size = _Protocol.SERVERPROT_SIZES[c->packet_type];
    }

    if (c->packet_size == -1) {
        if (!clientstream_available(c->stream, 1)) {
            return false;
        }

        clientstream_read_bytes(c->stream, c->in->data, 0, 1);
        c->packet_size = c->in->data[0] & 0xff;
    }

    if (c->packet_size == -2) {
        if (!clientstream_available(c->stream, 2)) {
            return false;
        }

        clientstream_read_bytes(c->stream, c->in->data, 0, 2);
        c->in->pos = 0;
        c->packet_size = g2(c->in);
    }

    if (!clientstream_available(c->stream, c->packet_size)) {
        return false;
    }
    c->in->pos = 0;
    clientstream_read_bytes(c->stream, c->in->data, 0, c->packet_size);
    c->idle_net_cycles = 0;
    c->last_packet_type2 = c->last_packet_type1;
    c->last_packet_type1 = c->last_packet_type0;
    c->last_packet_type0 = c->packet_type;

    if (c->packet_type == 150) {
        // VARP_SMALL
        int varp = g2(c->in);
        int8_t value = g1b(c->in);
        c->varCache[varp] = value;
        if (c->varps[varp] != value) {
            c->varps[varp] = value;
            updateVarp(c, varp);
            if(varp==323)c->redraw_chatback=c->redraw_privacy_settings=true;
            c->redraw_sidebar = true;
            if (c->sticky_chat_interface_id != -1) {
                c->redraw_chatback = true;
            }
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 152) {
        // UPDATE_FRIENDLIST
        int64_t username = g8(c->in);
        int world = g1(c->in);
        char *display_name = jstring_format_name(jstring_from_base37(username));
        for (int i = 0; i < c->friend_count; i++) {
            if (username == c->friendName37[i]) {
                if (c->friendWorld[i] != world) {
                    c->friendWorld[i] = world;
                    c->redraw_sidebar = true;
                    char buf[MAX_STR];
                    if (world > 0) {
                        sprintf(buf, "%s has logged in.", display_name);
                        client_add_message(c, 5, buf, "");
                    }
                    if (world == 0) {
                        sprintf(buf, "%s has logged out.", display_name);
                        client_add_message(c, 5, buf, "");
                    }
                }
                display_name = NULL;
                break;
            }
        }
        if (display_name && c->friend_count < 100) {
            c->friendName37[c->friend_count] = username;
            c->friendName[c->friend_count] = display_name;
            c->friendWorld[c->friend_count] = world;
            c->friend_count++;
            c->redraw_sidebar = true;
        }
        bool sorted = false;
        while (!sorted) {
            sorted = true;
            for (int i = 0; i < c->friend_count - 1; i++) {
                if ((c->friendWorld[i] != _Client.nodeid && c->friendWorld[i + 1] == _Client.nodeid) || (c->friendWorld[i] == 0 && c->friendWorld[i + 1] != 0)) {
                    int oldWorld = c->friendWorld[i];
                    c->friendWorld[i] = c->friendWorld[i + 1];
                    c->friendWorld[i + 1] = oldWorld;

                    char *oldName = c->friendName[i];
                    c->friendName[i] = c->friendName[i + 1];
                    c->friendName[i + 1] = oldName;

                    int64_t oldName37 = c->friendName37[i];
                    c->friendName37[i] = c->friendName37[i + 1];
                    c->friendName37[i + 1] = oldName37;
                    c->redraw_sidebar = true;
                    sorted = false;
                }
            }
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 43) {
        // UPDATE_REBOOT_TIMER
        c->system_update_timer = g2(c->in) * 30;
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 80) {
        // DATA_LAND_DONE
        int x = g1(c->in);
        int z = g1(c->in);
        int index = -1;
        for (int i = 0; i < c->sceneMapIndexLength; i++) {
            if (c->sceneMapIndex[i] == (x << 8) + z) {
                index = i;
            }
        }
        if (index != -1) {
#ifdef __EMSCRIPTEN__
            // TODO use indexeddb instead of emscripten memfs
            char filename[PATH_MAX];
            sprintf(filename, "m%d_%d", x, z);
            FILE *file = fopen(filename, "wb");
            fwrite(c->sceneMapLandData[index], 1, c->sceneMapLandDataIndexLength[index], file);
            fclose(file);
#endif
            // signlink.cachesave("m" + x + "_" + z, this.sceneMapLandData[index]);
            c->scene_state = 1;
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 1) {
        // NPC_INFO
        getNpcPos(c, c->in, c->packet_size);
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 237) {
        // REBUILD_NORMAL
        uint64_t region_packet_at=ba_timing_now();
        int zoneX = g2(c->in);
        int zoneZ = g2(c->in);
        if (c->sceneCenterZoneX == zoneX && c->sceneCenterZoneZ == zoneZ && c->scene_state != 0) {
            c->packet_type = -1;
            return true;
        }
        c->sceneCenterZoneX = zoneX;
        c->sceneCenterZoneZ = zoneZ;
        c->sceneBaseTileX = (c->sceneCenterZoneX - 6) * 8;
        c->sceneBaseTileZ = (c->sceneCenterZoneZ - 6) * 8;
        c->scene_state = 1;
        pixmap_bind(c->area_viewport);
        drawStringCenter(c->font_plain12, 257, 151, "Loading - please wait.", BLACK);
        drawStringCenter(c->font_plain12, 256, 150, "Loading - please wait.", WHITE);
        if(!ve_enabled())pixmap_draw(c->area_viewport, 8, 11);
        // signlink.looprate(5);
        int regions = (c->packet_size - 2) / 10;

        client_scenemap_free(c);
        c->sceneMapLandData = calloc(regions, sizeof(int8_t *));
        c->sceneMapLocData = calloc(regions, sizeof(int8_t *));
        c->sceneMapIndex = calloc(regions, sizeof(int));
        c->sceneMapLandDataIndexLength = calloc(regions, sizeof(int));
        c->sceneMapLocDataIndexLength = calloc(regions, sizeof(int));
        c->sceneMapIndexLength = regions;
        // REBUILD_GETMAPS
        p1isaac(c->out, 150);
        p1(c->out, 0);
        int mapCount = 0;
        for (int i = 0; i < regions; i++) {
            int mapsquareX = g1(c->in);
            int mapsquareZ = g1(c->in);
            int landCrc = g4(c->in);
            int locCrc = g4(c->in);
            c->sceneMapIndex[i] = (mapsquareX << 8) + mapsquareZ;
            int8_t *data = NULL;
            size_t size = 0;
            if (landCrc != 0) {
                // data = signlink.cacheload("m" + mapsquareX + "_" + mapsquareZ);
                // custom NOTE move these
                char filename[PATH_MAX];
#ifdef _arch_dreamcast
                snprintf(filename, sizeof(filename), "cache/client/maps/m%d_%d.", mapsquareX, mapsquareZ);
#elif defined(NXDK)
                snprintf(filename, sizeof(filename), "D:\\cache\\client\\maps\\m%d_%d", mapsquareX, mapsquareZ);
#elif defined(__EMSCRIPTEN__)
                snprintf(filename, sizeof(filename), "m%d_%d", mapsquareX, mapsquareZ);
#else
                snprintf(filename, sizeof(filename), "rom/cache/client/maps/m%d_%d", mapsquareX, mapsquareZ);
#endif

#if ANDROID
                SDL_RWops *file = SDL_RWFromFile(filename, "rb");
#else
                FILE *file = fopen(filename, "rb");
#endif
                if (!file) {
                    // rs2_error("%s: %s\n", filename, strerror(errno));
                } else {
#ifdef ANDROID
                    size_t size = SDL_RWseek(file, 0, RW_SEEK_END);
                    SDL_RWseek(file, 0, RW_SEEK_SET);
#else
                    fseek(file, 0, SEEK_END);
                    size = ftell(file);
                    fseek(file, 0, SEEK_SET);
#endif

                    data = malloc(size);
#ifdef ANDROID
                    if (SDL_RWread(file, data, 1, size) != size) {
#else
                    if (fread(data, 1, size, file) != size) {
#endif
                        rs2_error("Failed to read file: %s\n", strerror(errno));
                    }
#ifdef ANDROID
                    SDL_RWclose(file);
#else
                    fclose(file);
#endif
                }

                if (data) {
                    if (rs_crc32(data, size) != landCrc) {
                        // rs2_log("mapdata CRC check failed\n");
                        // free(data);
                        // data = NULL;
                    }
                }
                if (!data) {
                    c->scene_state = 0;
                    p1(c->out, 0);
                    p1(c->out, mapsquareX);
                    p1(c->out, mapsquareZ);
                    mapCount += 3;
                } else {
                    c->sceneMapLandDataIndexLength[i] = (int)size;
                    c->sceneMapLandData[i] = data;
                }
            }
            if (locCrc != 0) {
                // data = signlink.cacheload("l" + mapsquareX + "_" + mapsquareZ);
                // custom NOTE move this
                char filename[PATH_MAX];
#ifdef _arch_dreamcast
                snprintf(filename, sizeof(filename), "cache/client/maps/l%d_%d.", mapsquareX, mapsquareZ);
#elif defined(NXDK)
                snprintf(filename, sizeof(filename), "D:\\cache\\client\\maps\\l%d_%d", mapsquareX, mapsquareZ);
#elif defined(__EMSCRIPTEN__)
            snprintf(filename, sizeof(filename), "l%d_%d", mapsquareX, mapsquareZ);
#else
            snprintf(filename, sizeof(filename), "rom/cache/client/maps/l%d_%d", mapsquareX, mapsquareZ);
#endif

#if ANDROID
                SDL_RWops *file = SDL_RWFromFile(filename, "rb");
#else
                FILE *file = fopen(filename, "rb");
#endif
                if (!file) {
                    // rs2_error("%s: %s\n", filename, strerror(errno));
                } else {
#ifdef ANDROID
                    size_t size = SDL_RWseek(file, 0, RW_SEEK_END);
                    SDL_RWseek(file, 0, RW_SEEK_SET);
#else
                    fseek(file, 0, SEEK_END);
                    size = ftell(file);
                    fseek(file, 0, SEEK_SET);
#endif

                    data = malloc(size);
#ifdef ANDROID
                    if (SDL_RWread(file, data, 1, size) != size) {
#else
                    if (fread(data, 1, size, file) != size) {
#endif
                        rs2_error("Failed to read file: %s\n", strerror(errno));
                    }
#ifdef ANDROID
                    SDL_RWclose(file);
#else
                    fclose(file);
#endif
                }

                if (data) {
                    if (rs_crc32(data, size) != locCrc) {
                        // rs2_log("mapdata CRC check failed\n");
                        // free(data);
                        // data = NULL;
                    }
                }
                if (!data) {
                    c->scene_state = 0;
                    p1(c->out, 1);
                    p1(c->out, mapsquareX);
                    p1(c->out, mapsquareZ);
                    mapCount += 3;
                } else {
                    c->sceneMapLocDataIndexLength[i] = (int)size;
                    c->sceneMapLocData[i] = data;
                }
            }
        }
        psize1(c->out, mapCount);
        // signlink.looprate(50);
        pixmap_bind(c->area_viewport);
        if (c->scene_state == 0) {
            drawStringCenter(c->font_plain12, 257, 166, "Map area updated since last visit, so load will take longer this time only", BLACK);
            drawStringCenter(c->font_plain12, 256, 165, "Map area updated since last visit, so load will take longer this time only", WHITE);
        }
        if(!ve_enabled())pixmap_draw(c->area_viewport, 8, 11);
        int dx = c->sceneBaseTileX - c->mapLastBaseX;
        int dz = c->sceneBaseTileZ - c->mapLastBaseZ;
        c->mapLastBaseX = c->sceneBaseTileX;
        c->mapLastBaseZ = c->sceneBaseTileZ;
        for (int i = 0; i < MAX_NPC_COUNT; i++) {
            NpcEntity *npc = c->npcs[i];
            if (npc) {
                for (int j = 0; j < 10; j++) {
                    npc->pathing_entity.pathTileX[j] -= dx;
                    npc->pathing_entity.pathTileZ[j] -= dz;
                }
                npc->pathing_entity.x -= dx * 128;
                npc->pathing_entity.z -= dz * 128;
            }
        }
        for (int i = 0; i < MAX_PLAYER_COUNT; i++) {
            PlayerEntity *player = c->players[i];
            if (player) {
                for (int j = 0; j < 10; j++) {
                    player->pathing_entity.pathTileX[j] -= dx;
                    player->pathing_entity.pathTileZ[j] -= dz;
                }
                player->pathing_entity.x -= dx * 128;
                player->pathing_entity.z -= dz * 128;
            }
        }
        int8_t startTileX = 0;
        int8_t endTileX = 104;
        int8_t dirX = 1;
        if (dx < 0) {
            startTileX = 104 - 1;
            endTileX = -1;
            dirX = -1;
        }
        int8_t startTileZ = 0;
        int8_t endTileZ = 104;
        int8_t dirZ = 1;
        if (dz < 0) {
            startTileZ = 104 - 1;
            endTileZ = -1;
            dirZ = -1;
        }
        for (int x = startTileX; x != endTileX; x += dirX) {
            for (int z = startTileZ; z != endTileZ; z += dirZ) {
                int lastX = x + dx;
                int lastZ = z + dz;
                for (int level = 0; level < 4; level++) {
                    if (lastX >= 0 && lastZ >= 0 && lastX < 104 && lastZ < 104) {
                        c->level_obj_stacks[level][x][z] = c->level_obj_stacks[level][lastX][lastZ];
                    } else {
                        c->level_obj_stacks[level][x][z] = NULL;
                    }
                }
            }
        }
        for (LocAddEntity *loc = (LocAddEntity *)linklist_head(c->spawned_locations); loc; loc = (LocAddEntity *)linklist_next(c->spawned_locations)) {
            loc->x -= dx;
            loc->z -= dz;
            if (loc->x < 0 || loc->z < 0 || loc->x >= 104 || loc->z >= 104) {
                linkable_unlink(&loc->link);
                free(loc);
            }
        }
        if (c->flagSceneTileX != 0) {
            c->flagSceneTileX -= dx;
            c->flagSceneTileZ -= dz;
        }
        char region_line[192];
        snprintf(region_line,sizeof region_line,"# region_packet %llu base %d %d maps %d us %llu",(unsigned long long)ba_timing_now(),c->sceneBaseTileX,c->sceneBaseTileZ,regions,(unsigned long long)(ba_timing_now()-region_packet_at));
        ba_cadence_diagnostic(region_line);
        c->cutscene = false;
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 197) {
        // IF_SETPLAYERHEAD
        int com = g2(c->in);
        _Component.instances[com]->model = playerentity_get_headmodel(c->local_player);
        ve_note_component(c, com);
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 25) {
        // HINT_ARROW
        c->hint_type = g1(c->in);
        if (c->hint_type == 1) {
            c->hint_npc = g2(c->in);
        }
        if (c->hint_type >= 2 && c->hint_type <= 6) {
            if (c->hint_type == 2) {
                c->hint_offset_x = 64;
                c->hint_offset_z = 64;
            }
            if (c->hint_type == 3) {
                c->hint_offset_x = 0;
                c->hint_offset_z = 64;
            }
            if (c->hint_type == 4) {
                c->hint_offset_x = 128;
                c->hint_offset_z = 64;
            }
            if (c->hint_type == 5) {
                c->hint_offset_x = 64;
                c->hint_offset_z = 0;
            }
            if (c->hint_type == 6) {
                c->hint_offset_x = 64;
                c->hint_offset_z = 128;
            }
            c->hint_type = 2;
            c->hint_tile_x = g2(c->in);
            c->hint_tile_z = g2(c->in);
            c->hint_height = g1(c->in);
        }
        if (c->hint_type == 10) {
            c->hint_player = g2(c->in);
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 54) {
        // MIDI_SONG
        char *name = gjstr(c->in);
        int crc = g4(c->in);
        int length = g4(c->in);
        if (strcmp(name, c->currentMidi) != 0 && c->midiActive && !_Client.lowmem) {
            platform_set_midi(name, crc, length);
        }
        strcpy(c->currentMidi, name);
        free(name);
        c->midiCrc = crc;
        c->midiSize = length;
        c->nextMusicDelay = 0;
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 142) {
        // LOGOUT
        client_logout(c);
        c->packet_type = -1;
        return false;
    }
    if (c->packet_type == 20) {
        // DATA_LOC_DONE
        int x = g1(c->in);
        int z = g1(c->in);
        int index = -1;
        for (int i = 0; i < c->sceneMapIndexLength; i++) {
            if (c->sceneMapIndex[i] == (x << 8) + z) {
                index = i;
            }
        }
        if (index != -1) {
#if defined(__EMSCRIPTEN__)
            // TODO use indexeddb instead of emscripten memfs
            char filename[PATH_MAX];
            sprintf(filename, "l%d_%d", x, z);
            FILE *file = fopen(filename, "wb");
            fwrite(c->sceneMapLocData[index], 1, c->sceneMapLocDataIndexLength[index], file);
            fclose(file);
#endif
            // signlink.cachesave("l" + x + "_" + z, c->sceneMapLocData[index]);
            c->scene_state = 1;
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 19) {
        // UNSET_MAP_FLAG
        c->flagSceneTileX = 0;
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 139) {
        // UPDATE_UID192
        c->local_pid = g2(c->in);
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 151 || c->packet_type == 23 || c->packet_type == 50 || c->packet_type == 191 || c->packet_type == 69 || c->packet_type == 49 || c->packet_type == 223 || c->packet_type == 42 || c->packet_type == 76 || c->packet_type == 59) {
        // Zone Protocol
        readZonePacket(c, c->in, c->packet_type);
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 28) {
        // IF_OPENMAINSIDEMODAL
        int main = g2(c->in);
        int side = g2(c->in);
        if (c->chat_interface_id != -1) {
            c->chat_interface_id = -1;
            c->redraw_chatback = true;
        }
        if (c->chatback_input_open) {
            c->chatback_input_open = false;
            c->redraw_chatback = true;
        }
        c->viewport_interface_id = main;
        c->sidebar_interface_id = side;
        c->redraw_sidebar = true;
        c->redraw_sideicons = true;
        c->pressed_continue_option = false;
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 175) {
        // VARP_LARGE
        int varp = g2(c->in);
        int value = g4(c->in);
        c->varCache[varp] = value;
        if (c->varps[varp] != value) {
            c->varps[varp] = value;
            updateVarp(c, varp);
            if(varp==323)c->redraw_chatback=c->redraw_privacy_settings=true;
            c->redraw_sidebar = true;
            if (c->sticky_chat_interface_id != -1) {
                c->redraw_chatback = true;
            }
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 146) {
        // IF_SETANIM
        int com = g2(c->in);
        int seqId = g2(c->in);
        _Component.instances[com]->anim = seqId;
        ve_note_component(c, com);
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 167) {
        // IF_OPENSIDEOVERLAY
        int com = g2(c->in);
        int tab = g1(c->in);
        if (com == 65535) {
            com = -1;
        }
        c->tab_interface_id[tab] = com;
        c->redraw_sidebar = true;
        c->redraw_sideicons = true;
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 220) {
        // DATA_LOC
        int x = g1(c->in);
        int z = g1(c->in);
        int offset = g2(c->in);
        int length = g2(c->in);
        int index = -1;
        for (int i = 0; i < c->sceneMapIndexLength; i++) {
            if (c->sceneMapIndex[i] == (x << 8) + z) {
                index = i;
            }
        }
        if (index != -1) {
            if (!c->sceneMapLocData[index] || c->sceneMapLocDataIndexLength[index] != length) {
                c->sceneMapLocData[index] = calloc(length, sizeof(int8_t));
            }
            gdata(c->in, c->packet_size - 6, offset, c->sceneMapLocData[index]);
            c->sceneMapLocDataIndexLength[index] = length;
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 133) {
        // FINISH_TRACKING
        Packet *tracking = inputtracking_stop(&_InputTracking);
        if (tracking) {
            // EVENT_TRACKING
            p1isaac(c->out, 81);
            p2(c->out, tracking->pos);
            pdata(c->out, tracking->data, tracking->pos, 0);
            packet_release(tracking);
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 98) {
        // UPDATE_INV_FULL
        bool inv_changed=false;
        int com = g2(c->in);
        Component *inv = _Component.instances[com];
        int size = g1(c->in);
        for (int i = 0; i < size; i++) {
            int incoming_id = g2(c->in);
            int count = g1(c->in);
            if (count == 255) {
                count = g4(c->in);
            }
            if(inv->invSlotObjId[i]!=incoming_id||inv->invSlotObjCount[i]!=count)inv_changed=true;
            inv->invSlotObjId[i]=incoming_id;
            inv->invSlotObjCount[i] = count;
        }
        for (int i = size; i < inv->width * inv->height; i++) {
            if(inv->invSlotObjId[i]||inv->invSlotObjCount[i])inv_changed=true;
            inv->invSlotObjId[i] = 0;
            inv->invSlotObjCount[i] = 0;
        }
        if(inv_changed)c->redraw_sidebar=true;
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 226) {
        // ENABLE_TRACKING
        inputtracking_set_enabled(&_InputTracking);
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 243) {
        // P_COUNTDIALOG
        c->show_social_input = false;
        c->chatback_input_open = true;
        c->chatback_input[0] = '\0';
        c->redraw_chatback = true;
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 15) {
        // UPDATE_INV_STOP_TRANSMIT
        int com = g2(c->in);
        Component *inv = _Component.instances[com];
        for (int i = 0; i < inv->width * inv->height; i++) {
            inv->invSlotObjId[i] = -1;
            inv->invSlotObjId[i] = 0;
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 140) {
        // LAST_LOGIN_INFO
        c->lastAddress = g4(c->in);
        c->daysSinceLastLogin = g2(c->in);
        c->daysSinceRecoveriesChanged = g1(c->in);
        c->unreadMessages = g2(c->in);
        if (c->lastAddress != 0 && c->viewport_interface_id == -1) {
            if (_Custom.hide_dns) {
                _Client.dns = "unknown";
            } else {
                _Client.dns = dnslookup(jstring_format_ipv4(c->lastAddress));
            }
            closeInterfaces(c);
            int clientCode = 650;
            if (c->daysSinceRecoveriesChanged != 201) {
                clientCode = 655;
            }
            c->reportAbuseInput[0] = '\0';
            c->reportAbuseMuteOption = false;
            for (int i = 0; i < _Component.count; i++) {
                if (_Component.instances[i] && _Component.instances[i]->clientCode == clientCode) {
                    c->viewport_interface_id = _Component.instances[i]->layer;
                    break;
                }
            }
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 126) {
        // TUTORIAL_FLASHSIDE
        c->flashing_tab = g1(c->in);
        if (c->flashing_tab == c->selected_tab) {
            if (c->flashing_tab == 3) {
                c->selected_tab = 1;
            } else {
                c->selected_tab = 3;
            }
            c->redraw_sidebar = true;
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 212) {
        // MIDI_JINGLE
        if (c->midiActive && !_Client.lowmem) {
            int delay = g2(c->in);
            int length = g4(c->in);
            int remaining = c->packet_size - 6;
            int8_t *src = calloc(length, sizeof(int8_t));
            bzip_decompress(src, c->in->data, remaining, c->in->pos);
            platform_set_jingle(src, length);
            c->nextMusicDelay = delay;
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 254) {
        // SET_MULTIWAY
        c->in_multizone = g1(c->in);
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 12) {
        // SYNTH_SOUND
        int id = g2(c->in);
        int loop = g1(c->in);
        int delay = g2(c->in);
        if(id>=0&&id<1000)r108_sound_received[id]++;
        if (c->wave_enabled && !_Client.lowmem && c->wave_count < 50 && wave_available(id)) {
            c->wave_ids[c->wave_count] = id;
            c->wave_loops[c->wave_count] = loop;
            c->wave_delay[c->wave_count] = delay + _Wave.delays[id];
            c->wave_count++;
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 204) {
        // IF_SETNPCHEAD
        int com = g2(c->in);
        int npcId = g2(c->in);
        NpcType *npc = npctype_get(npcId);
        _Component.instances[com]->model = npctype_get_headmodel(npc);
        ve_note_component(c, com);
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 7) {
        // UPDATE_ZONE_PARTIAL_FOLLOWS
        c->baseX = g1(c->in);
        c->baseZ = g1(c->in);
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 103) {
        // IF_SETRECOL
        int com = g2(c->in);
        int src = g2(c->in);
        int dst = g2(c->in);
        Component *inter = _Component.instances[com];
        Model *model = inter->model;
        if (model) {
            model_recolor(model, src, dst);
        }
        ve_note_component(c, com);
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 32) {
        // CHAT_FILTER_SETTINGS
        c->public_chat_setting = g1(c->in);
        c->private_chat_setting = g1(c->in);
        c->trade_chat_setting = g1(c->in);
        ve_privacy_restore(c);
        c->redraw_privacy_settings = true;
        c->redraw_chatback = true;
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 195) {
        // IF_OPENSIDEMODAL
        int com = g2(c->in);
        reset_interface_animation(com);
        if (c->chat_interface_id != -1) {
            c->chat_interface_id = -1;
            c->redraw_chatback = true;
        }
        if (c->chatback_input_open) {
            c->chatback_input_open = false;
            c->redraw_chatback = true;
        }
        c->sidebar_interface_id = com;
        c->redraw_sidebar = true;
        c->redraw_sideicons = true;
        c->viewport_interface_id = -1;
        c->pressed_continue_option = false;
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 14) {
        // IF_OPENCHATMODAL
        c->vita_dialogue_generation++;
        int com = g2(c->in);
        reset_interface_animation(com);
        if (c->sidebar_interface_id != -1) {
            c->sidebar_interface_id = -1;
            c->redraw_sidebar = true;
            c->redraw_sideicons = true;
        }
        c->chat_interface_id = com;
        c->redraw_chatback = true;
        c->viewport_interface_id = -1;
        c->pressed_continue_option = false;
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 209) {
        // IF_SETPOSITION
        int com = g2(c->in);
        int x = g2b(c->in);
        int z = g2b(c->in);
        Component *inter = _Component.instances[com];
        inter->x = x;
        inter->y = z;
        ve_note_component(c, com);
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 3) {
        // CAM_MOVETO
        c->cutscene = true;
        c->cutsceneSrcLocalTileX = g1(c->in);
        c->cutsceneSrcLocalTileZ = g1(c->in);
        c->cutsceneSrcHeight = g2(c->in);
        c->cutsceneMoveSpeed = g1(c->in);
        c->cutsceneMoveAcceleration = g1(c->in);
        if (c->cutsceneMoveAcceleration >= 100) {
            c->cameraX = c->cutsceneSrcLocalTileX * 128 + 64;
            c->cameraZ = c->cutsceneSrcLocalTileZ * 128 + 64;
            c->cameraY = getHeightmapY(c, c->currentLevel, c->cutsceneSrcLocalTileX, c->cutsceneSrcLocalTileZ) - c->cutsceneSrcHeight;
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 135) {
        // UPDATE_ZONE_FULL_FOLLOWS
        c->baseX = g1(c->in);
        c->baseZ = g1(c->in);
        for (int x = c->baseX; x < c->baseX + 8; x++) {
            for (int z = c->baseZ; z < c->baseZ + 8; z++) {
                if (c->level_obj_stacks[c->currentLevel][x][z]) {
                    linklist_free(c->level_obj_stacks[c->currentLevel][x][z]);
                    c->level_obj_stacks[c->currentLevel][x][z] = NULL;
                }
                sortObjStacks(c, x, z);
            }
        }
        for (LocAddEntity *loc = (LocAddEntity *)linklist_head(c->spawned_locations); loc; loc = (LocAddEntity *)linklist_next(c->spawned_locations)) {
            if (loc->x >= c->baseX && loc->x < c->baseX + 8 && loc->z >= c->baseZ && loc->z < c->baseZ + 8 && loc->plane == c->currentLevel) {
                addLoc(c, loc->plane, loc->x, loc->z, loc->lastLocIndex, loc->lastAngle, loc->lastShape, loc->layer);
                linkable_unlink(&loc->link);
                free(loc);
            }
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 132) {
        // DATA_LAND
        int x = g1(c->in);
        int z = g1(c->in);
        int offset = g2(c->in);
        int length = g2(c->in);
        int index = -1;
        for (int i = 0; i < c->sceneMapIndexLength; i++) {
            if (c->sceneMapIndex[i] == (x << 8) + z) {
                index = i;
            }
        }
        if (index != -1) {
            if (!c->sceneMapLandData[index] || c->sceneMapLandDataIndexLength[index] != length) {
                c->sceneMapLandData[index] = calloc(length, sizeof(int8_t));
            }
            gdata(c->in, c->packet_size - 6, offset, c->sceneMapLandData[index]);
            c->sceneMapLandDataIndexLength[index] = length;
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 41) {
        // MESSAGE_PRIVATE
        int64_t from = g8(c->in);
        int messageId = g4(c->in);
        int staffModLevel = g1(c->in);
        bool ignored = false;
        for (int i = 0; i < 100; i++) {
            if (c->messageIds[i] == messageId) {
                ignored = true;
                break;
            }
        }
        if (staffModLevel <= 1) {
            for (int i = 0; i < c->ignoreCount; i++) {
                if (c->ignoreName37[i] == from) {
                    ignored = true;
                    break;
                }
            }
        }
        if (!ignored && c->overrideChat == 0) {
            // try {
            c->messageIds[c->privateMessageCount] = messageId;
            c->privateMessageCount = (c->privateMessageCount + 1) % 100;
            char *uncompressed = wordpack_unpack(c->in, c->packet_size - 13);
            wordfilter_filter(uncompressed);
            if (staffModLevel > 1) {
                client_add_message(c, 7, uncompressed, jstring_format_name(jstring_from_base37(from)));
            } else {
                client_add_message(c, 3, uncompressed, jstring_format_name(jstring_from_base37(from)));
            }
            // } catch (@Pc(2752) Exception ex) {
            // 	signlink.reporterror("cde1");
            // }
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 193) {
        // RESET_CLIENT_VARCACHE
        for (int i = 0; i < VARPS_COUNT; i++) {
            if (c->varps[i] != c->varCache[i]) {
                c->varps[i] = c->varCache[i];
                updateVarp(c, i);
                c->redraw_sidebar = true;
            }
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 87) {
        // IF_SETMODEL
        int com = g2(c->in);
        int model = g2(c->in);
        _Component.instances[com]->model = model_from_id(model, false);
        ve_note_component(c, com);
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 185) {
        // TUTORIAL_OPENCHAT
        int com = g2b(c->in);
        c->sticky_chat_interface_id = com;
        c->redraw_chatback = true;
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 68) {
        // UPDATE_RUNENERGY
        if (c->selected_tab == 12) {
            c->redraw_sidebar = true;
        }
        c->energy = g1(c->in);
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 74) {
        // CAM_LOOKAT
        c->cutscene = true;
        c->cutsceneDstLocalTileX = g1(c->in);
        c->cutsceneDstLocalTileZ = g1(c->in);
        c->cutsceneDstHeight = g2(c->in);
        c->cutsceneRotateSpeed = g1(c->in);
        c->cutsceneRotateAcceleration = g1(c->in);
        if (c->cutsceneRotateAcceleration >= 100) {
            int sceneX = c->cutsceneDstLocalTileX * 128 + 64;
            int sceneZ = c->cutsceneDstLocalTileZ * 128 + 64;
            int sceneY = getHeightmapY(c, c->currentLevel, c->cutsceneDstLocalTileX, c->cutsceneDstLocalTileZ) - c->cutsceneDstHeight;
            int deltaX = sceneX - c->cameraX;
            int deltaY = sceneY - c->cameraY;
            int deltaZ = sceneZ - c->cameraZ;
            int distance = (int)sqrt(deltaX * deltaX + deltaZ * deltaZ);
            c->cameraPitch = (int)(atan2(deltaY, distance) * 325.949) & 0x7ff;
            c->cameraYaw = (int)(atan2(deltaX, deltaZ) * -325.949) & 0x7ff;
            if (c->cameraPitch < 128) {
                c->cameraPitch = 128;
            }
            if (c->cameraPitch > 383) {
                c->cameraPitch = 383;
            }
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 84) {
        // IF_SHOWSIDE
        c->selected_tab = g1(c->in);
        c->redraw_sidebar = true;
        c->redraw_sideicons = true;
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 4) {
        // MESSAGE_GAME
        char *message = gjstr(c->in);
        int64_t username;
        if (strendswith(message, ":tradereq:")) {
            char *player = substring(message, 0, indexof_chr(message, ':'));
            username = jstring_to_base37(player);
            bool ignored = false;
            for (int i = 0; i < c->ignoreCount; i++) {
                if (c->ignoreName37[i] == username) {
                    ignored = true;
                    break;
                }
            }
            if (!ignored && c->overrideChat == 0) {
                client_add_message(c, 4, "wishes to trade with you.", player);
            }
        } else if (strendswith(message, ":duelreq:")) {
            char *player = substring(message, 0, indexof_chr(message, ':'));
            username = jstring_to_base37(player);
            bool ignored = false;
            for (int i = 0; i < c->ignoreCount; i++) {
                if (c->ignoreName37[i] == username) {
                    ignored = true;
                    break;
                }
            }
            if (!ignored && c->overrideChat == 0) {
                client_add_message(c, 8, "wishes to duel with you.", player);
            }
        } else {
            client_add_message(c, 0, message, "");
        }
        free(message);
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 46) {
        // IF_SETOBJECT
        int com = g2(c->in);
        int objId = g2(c->in);
        int zoom = g2(c->in);
        ObjType *obj = objtype_get(objId);
        _Component.instances[com]->model = objtype_get_interfacemodel(obj, 50, false);
        _Component.instances[com]->xan = obj->xan2d;
        _Component.instances[com]->yan = obj->yan2d;
        _Component.instances[com]->zoom = obj->zoom2d * 100 / zoom;
        ve_note_component(c, com);
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 168) {
        // IF_OPENMAINMODAL
        c->vita_skill_generation++;
        int com = g2(c->in);
        ba_interface_trace("IF_OPENMAIN",com,"");
        reset_interface_animation(com);
        if (c->sidebar_interface_id != -1) {
            c->sidebar_interface_id = -1;
            c->redraw_sidebar = true;
            c->redraw_sideicons = true;
        }
        if (c->chat_interface_id != -1) {
            c->chat_interface_id = -1;
            c->redraw_chatback = true;
        }
        if (c->chatback_input_open) {
            c->chatback_input_open = false;
            c->redraw_chatback = true;
        }
        c->viewport_interface_id = com;
        c->pressed_continue_option = false;
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 2) {
        // IF_SETCOLOUR
        int com = g2(c->in);
        int color = g2(c->in);
        int r = color >> 10 & 0x1f;
        int g = color >> 5 & 0x1f;
        int b = color & 0x1f;
        _Component.instances[com]->colour = (r << 19) + (g << 11) + (b << 3);
        ve_note_component(c, com);
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 136) {
        // RESET_ANIMS
        for (int i = 0; i < MAX_PLAYER_COUNT; i++) {
            if (c->players[i]) {
                c->players[i]->pathing_entity.primarySeqId = -1;
            }
        }
        for (int i = 0; i < MAX_NPC_COUNT; i++) {
            if (c->npcs[i]) {
                c->npcs[i]->pathing_entity.primarySeqId = -1;
            }
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 26) {
        // IF_SETHIDE
        int com = g2(c->in);
        bool hide = g1(c->in) == 1;
        _Component.instances[com]->hide = hide;
        ve_note_component(c, com);
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 21) {
        // UPDATE_IGNORELIST
        c->ignoreCount = c->packet_size / 8;
        for (int i = 0; i < c->ignoreCount; i++) {
            c->ignoreName37[i] = g8(c->in);
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 239) {
        // CAM_RESET
        c->cutscene = false;
        for (int i = 0; i < 5; i++) {
            c->cameraModifierEnabled[i] = false;
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 129) {
        // IF_CLOSE
        if (c->sidebar_interface_id != -1) {
            c->sidebar_interface_id = -1;
            c->redraw_sidebar = true;
            c->redraw_sideicons = true;
        }
        if (c->chat_interface_id != -1) {
            c->chat_interface_id = -1;
            c->redraw_chatback = true;
        }
        if (c->chatback_input_open) {
            c->chatback_input_open = false;
            c->redraw_chatback = true;
        }
        c->viewport_interface_id = -1;
        c->pressed_continue_option = false;
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 201) {
        // IF_SETTEXT
        int com = g2(c->in);
        char *text = gjstr(c->in);
        ba_interface_trace("IF_SETTEXT",com,text);
        strcpy(_Component.instances[com]->text, text);
        free(text);
        ve_note_component(c, com);
        if (c->selected_tab >= 0 && c->selected_tab < 15 && _Component.instances[com]->layer == c->tab_interface_id[c->selected_tab]) {
            c->redraw_sidebar = true;
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 44) {
        // UPDATE_STAT
        int stat = g1(c->in);
        int xp = g4(c->in);
        int level = g1(c->in);
        if(c->skillExperience[stat]!=xp||c->skillLevel[stat]!=level)c->redraw_sidebar=true;
        c->skillExperience[stat] = xp;
        c->skillLevel[stat] = level;
        c->skillBaseLevel[stat] = 1;
        for (int i = 0; i < 98; i++) {
            if (xp >= _Client.levelExperience[i]) {
                c->skillBaseLevel[stat] = i + 2;
            }
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 162) {
        // UPDATE_ZONE_PARTIAL_ENCLOSED
        c->baseX = g1(c->in);
        c->baseZ = g1(c->in);
        while (c->in->pos < c->packet_size) {
            int opcode = g1(c->in);
            readZonePacket(c, c->in, opcode);
        }
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 22) {
        // UPDATE_RUNWEIGHT
        if (c->selected_tab == 12) {
            c->redraw_sidebar = true;
        }
        c->weightCarried = g2b(c->in);
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 13) {
        // CAM_SHAKE
        int type = g1(c->in);
        int jitter = g1(c->in);
        int wobbleScale = g1(c->in);
        int wobbleSpeed = g1(c->in);
        c->cameraModifierEnabled[type] = true;
        c->cameraModifierJitter[type] = jitter;
        c->cameraModifierWobbleScale[type] = wobbleScale;
        c->cameraModifierWobbleSpeed[type] = wobbleSpeed;
        c->cameraModifierCycle[type] = 0;
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 213) {
        // UPDATE_INV_PARTIAL
        bool inv_changed=false;
        int com = g2(c->in);
        Component *inv = _Component.instances[com];
        while (c->in->pos < c->packet_size) {
            int slot = g1(c->in);
            int id = g2(c->in);
            int count = g1(c->in);
            if (count == 255) {
                count = g4(c->in);
            }
            if (slot >= 0 && slot < inv->width * inv->height) {
                if(inv->invSlotObjId[slot]!=id||inv->invSlotObjCount[slot]!=count)inv_changed=true;
                inv->invSlotObjId[slot] = id;
                inv->invSlotObjCount[slot] = count;
            }
        }
        if(inv_changed)c->redraw_sidebar=true;
        c->packet_type = -1;
        return true;
    }
    if (c->packet_type == 184) {
        // PLAYER_INFO
        getPlayer(c, c->in, c->packet_size);
        if (c->scene_state == 1) {
            c->scene_state = 2;
            _World.levelBuilt = c->currentLevel;
            client_build_scene(c);
        }
        if (_Client.lowmem && c->scene_state == 2 && _World.levelBuilt != c->currentLevel) {
            pixmap_bind(c->area_viewport);
            drawStringCenter(c->font_plain12, 257, 151, "Loading - please wait.", BLACK);
            drawStringCenter(c->font_plain12, 256, 150, "Loading - please wait.", WHITE);
            if(!ve_enabled())pixmap_draw(c->area_viewport, 8, 11);
            _World.levelBuilt = c->currentLevel;
            client_build_scene(c);
        }
        if (c->currentLevel != c->minimap_level && c->scene_state == 2) {
            c->minimap_level = c->currentLevel;
            createMinimap(c, c->currentLevel);
        }
        c->packet_type = -1;
        return true;
    }
    rs2_error("T1 - %i,%i - %i,%i\n", c->packet_type, c->packet_size, c->last_packet_type1, c->last_packet_type2);
    // signlink.reporterror("T1 - " + c->packet_type + "," + c->packetSize + " - " + c->lastPacketType1 + "," + c->lastPacketType2);
    client_logout(c);
    // } catch (@Pc(3862) IOException ex) {
    // client_try_reconnect(c);
    // } catch (@Pc(3867) Exception ex) {
    // ex.printStackTrace();
    // const char* error = "T2 - " + c->packet_type + "," + c->lastPacketType1 + "," + c->lastPacketType2 + " - " + c->packetSize + "," + (c->sceneBaseTileX + c->localPlayer.pathTileX[0]) + "," + (c->sceneBaseTileZ + c->localPlayer.pathTileZ[0]) + " - ";
    // for (int i = 0; i < c->packet_size && i < 50; i++) {
    // 	error = error + c->in->data[i] + ",";
    // }
    // signlink.reporterror(error);
    // client_logout(c);
    // }
    return true;
}

void getPlayerLocal(Client *c, Packet *buf, int size) {
    (void)size;
    access_bits(buf);

    int info = gbit(buf, 1);
    if (info != 0) {
        int op = gbit(buf, 2);

        if (op == 0) {
            c->entityUpdateIds[c->entityUpdateCount++] = LOCAL_PLAYER_INDEX;
        } else if (op == 1) {
            int walkDir = gbit(buf, 3);
            pathingentity_movealongroute(&c->local_player->pathing_entity, false, walkDir);
            pos_trace(c, "step", 0, walkDir, 0);

            int extendedInfo = gbit(buf, 1);
            if (extendedInfo == 1) {
                c->entityUpdateIds[c->entityUpdateCount++] = LOCAL_PLAYER_INDEX;
            }
        } else if (op == 2) {
            int walkDir = gbit(buf, 3);
            pathingentity_movealongroute(&c->local_player->pathing_entity, true, walkDir);
            int runDir = gbit(buf, 3);
            pathingentity_movealongroute(&c->local_player->pathing_entity, true, runDir);
            pos_trace(c, "step", 1, walkDir, runDir);

            int extendedInfo = gbit(buf, 1);
            if (extendedInfo == 1) {
                c->entityUpdateIds[c->entityUpdateCount++] = LOCAL_PLAYER_INDEX;
            }
        } else if (op == 3) {
            c->currentLevel = gbit(buf, 2);
            // TODO:
            // if (c->show_debug) {
            // 	c->userTileMarkers = ground_new(4);
            // 	c->userTileMarkerIndex = 0;
            // }
            int localX = gbit(buf, 7);
            int localZ = gbit(buf, 7);
            int jump = gbit(buf, 1);
            pathingentity_teleport(&c->local_player->pathing_entity, jump == 1, localX, localZ);
            pos_trace(c, "teleport", jump, localX, localZ);

            int extendedInfo = gbit(buf, 1);
            if (extendedInfo == 1) {
                c->entityUpdateIds[c->entityUpdateCount++] = LOCAL_PLAYER_INDEX;
            }
        }
    }
}

void getPlayerOldVis(Client *c, Packet *buf, int size) {
    (void)size;
    int count = gbit(buf, 8);

    if (count < c->player_count) {
        for (int i = count; i < c->player_count; i++) {
            c->entityRemovalIds[c->entityRemovalCount++] = c->player_ids[i];
        }
    }

    if (count > c->player_count) {
        rs2_error("%s Too many players\n", c->username);
        // signlink.reporterror(c->username + " Too many players");
        // throw new RuntimeException("eek");
    }

    c->player_count = 0;
    for (int i = 0; i < count; i++) {
        int index = c->player_ids[i];
        PlayerEntity *player = c->players[index];

        int info = gbit(buf, 1);
        if (info == 0) {
            c->player_ids[c->player_count++] = index;
            player->pathing_entity.cycle = _Client.loop_cycle;
        } else {
            int op = gbit(buf, 2);

            if (op == 0) {
                c->player_ids[c->player_count++] = index;
                player->pathing_entity.cycle = _Client.loop_cycle;
                c->entityUpdateIds[c->entityUpdateCount++] = index;
            } else if (op == 1) {
                c->player_ids[c->player_count++] = index;
                player->pathing_entity.cycle = _Client.loop_cycle;

                int walkDir = gbit(buf, 3);
                pathingentity_movealongroute(&player->pathing_entity, false, walkDir);

                int extendedInfo = gbit(buf, 1);
                if (extendedInfo == 1) {
                    c->entityUpdateIds[c->entityUpdateCount++] = index;
                }
            } else if (op == 2) {
                c->player_ids[c->player_count++] = index;
                player->pathing_entity.cycle = _Client.loop_cycle;

                int walkDir = gbit(buf, 3);
                pathingentity_movealongroute(&player->pathing_entity, true, walkDir);
                int runDir = gbit(buf, 3);
                pathingentity_movealongroute(&player->pathing_entity, true, runDir);

                int extendedInfo = gbit(buf, 1);
                if (extendedInfo == 1) {
                    c->entityUpdateIds[c->entityUpdateCount++] = index;
                }
            } else if (op == 3) {
                c->entityRemovalIds[c->entityRemovalCount++] = index;
            }
        }
    }
}

void getPlayerNewVis(Client *c, int size, Packet *buf) {
    int index;
    while (buf->bit_pos + 10 < size * 8) {
        index = gbit(buf, 11);
        if (index == 2047) {
            break;
        }

        if (!c->players[index]) {
            c->players[index] = playerentity_new();
            if (c->player_appearance_buffer[index]) {
                playerentity_read(c->players[index], c->player_appearance_buffer[index]);
            }
        }

        c->player_ids[c->player_count++] = index;
        PlayerEntity *player = c->players[index];
        player->pathing_entity.cycle = _Client.loop_cycle;
        int dx = gbit(buf, 5);
        if (dx > 15) {
            dx -= 32;
        }
        int dz = gbit(buf, 5);
        if (dz > 15) {
            dz -= 32;
        }
        int jump = gbit(buf, 1);
        pathingentity_teleport(&player->pathing_entity, jump == 1, c->local_player->pathing_entity.pathTileX[0] + dx, c->local_player->pathing_entity.pathTileZ[0] + dz);

        int extendedInfo = gbit(buf, 1);
        if (extendedInfo == 1) {
            c->entityUpdateIds[c->entityUpdateCount++] = index;
        }
    }

    access_bytes(buf);
}

void getPlayerExtended(Client *c, Packet *buf, int size) {
    (void)size;
    for (int i = 0; i < c->entityUpdateCount; i++) {
        int index = c->entityUpdateIds[i];
        PlayerEntity *player = c->players[index];
        int mask = g1(buf);
        if ((mask & 0x80) == 128) {
            mask += g1(buf) << 8;
        }
        getPlayerExtended2(c, player, index, mask, buf);
    }
}

void getPlayerExtended2(Client *c, PlayerEntity *player, int index, int mask, Packet *buf) {
    player->pathing_entity.lastMask = mask;
    player->pathing_entity.lastMaskCycle = _Client.loop_cycle;

    if ((mask & 0x1) == 1) {
        int length = g1(buf);
        int8_t *data = calloc(length, sizeof(int8_t));
        Packet *appearance = packet_new(data, length);
        gdata(buf, length, 0, data);

        if (c->player_appearance_buffer[index]) {
            packet_free(c->player_appearance_buffer[index]);
        }
        c->player_appearance_buffer[index] = appearance;
        playerentity_read(player, appearance);
    }
    if ((mask & 0x2) == 2) {
        int rawSeqId=g2(buf);
        int seqId = ui474_player_sequence(rawSeqId);
        if (seqId == 65535) {
            seqId = -1;
        }
        if (seqId == player->pathing_entity.primarySeqId) {
            player->pathing_entity.primarySeqLoop = 0;
        }
        int delay = g1(buf);
        if(ba_agility_probe_enabled&&player==c->local_player){char line[256];int frames=seqId>=0&&seqId<_SeqType.count&&_SeqType.instances[seqId]?_SeqType.instances[seqId]->frameCount:-1;snprintf(line,sizeof line,"# agility_packet %llu logic %d rawseq %d seq %d delay %d frames %d mask %d",(unsigned long long)ba_timing_now(),_Client.loop_cycle,rawSeqId,seqId,delay,frames,mask);ba_cadence_diagnostic(line);}

        if (seqId == -1 || player->pathing_entity.primarySeqId == -1 || _SeqType.instances[seqId]->priority > _SeqType.instances[player->pathing_entity.primarySeqId]->priority || _SeqType.instances[player->pathing_entity.primarySeqId]->priority == 0) {
            player->pathing_entity.primarySeqId = seqId;
            player->pathing_entity.primarySeqFrame = 0;
            player->pathing_entity.primarySeqCycle = 0;
            player->pathing_entity.primarySeqDelay = delay;
            player->pathing_entity.primarySeqLoop = 0;
        }
    }
    if ((mask & 0x4) == 4) {
        player->pathing_entity.targetId = g2(buf);
        if (player->pathing_entity.targetId == 65535) {
            player->pathing_entity.targetId = -1;
        }
    }
    if ((mask & 0x8) == 8) {
        strcpy(player->pathing_entity.chat, gjstr(buf));
        player->pathing_entity.chatColor = 0;
        player->pathing_entity.chatStyle = 0;
        player->pathing_entity.chatTimer = 150;
        client_add_message(c, 2, player->pathing_entity.chat, player->name);
    }
    if ((mask & 0x10) == 16) {
        player->pathing_entity.damage = g1(buf);
        player->pathing_entity.damageType = g1(buf);
        player->pathing_entity.combatCycle = _Client.loop_cycle + 400;
        player->pathing_entity.health = g1(buf);
        player->pathing_entity.totalHealth = g1(buf);
    }
    if ((mask & 0x20) == 32) {
        player->pathing_entity.targetTileX = g2(buf);
        player->pathing_entity.targetTileZ = g2(buf);
        player->pathing_entity.lastFaceX = player->pathing_entity.targetTileX;
        player->pathing_entity.lastFaceZ = player->pathing_entity.targetTileZ;
    }
    if ((mask & 0x40) == 64) {
        int colorEffect = g2(buf);
        int type = g1(buf);
        int length = g1(buf);
        int start = buf->pos;
        if (player->name[0]) {
            int64_t username = jstring_to_base37(player->name);
            bool ignored = false;
            if (type <= 1) {
                for (int i = 0; i < c->ignoreCount; i++) {
                    if (c->ignoreName37[i] == username) {
                        ignored = true;
                        break;
                    }
                }
            }
            if (!ignored && c->overrideChat == 0) {
                // try {
                char *uncompressed = wordpack_unpack(buf, length);
                wordfilter_filter(uncompressed);
                strcpy(player->pathing_entity.chat, uncompressed);
                player->pathing_entity.chatColor = colorEffect >> 8;
                player->pathing_entity.chatStyle = colorEffect & 0xff;
                player->pathing_entity.chatTimer = 150;
                if (type > 1) {
                    client_add_message(c, 1, uncompressed, player->name);
                } else {
                    client_add_message(c, 2, uncompressed, player->name);
                }
                // } catch (Exception ex) {
                // 	signlink.reporterror("cde2");
                // }
            }
        }
        buf->pos = start + length;
    }
    if ((mask & 0x100) == 256) {
        player->pathing_entity.spotanimId = g2(buf);
        int heightDelay = g4(buf);
        player->pathing_entity.spotanimOffset = heightDelay >> 16;
        player->pathing_entity.spotanimLastCycle = _Client.loop_cycle + (heightDelay & 0xffff);
        player->pathing_entity.spotanimFrame = 0;
        player->pathing_entity.spotanimCycle = 0;
        if (player->pathing_entity.spotanimLastCycle > _Client.loop_cycle) {
            player->pathing_entity.spotanimFrame = -1;
        }
        if (player->pathing_entity.spotanimId == 65535) {
            player->pathing_entity.spotanimId = -1;
        }
    }
    if ((mask & 0x200) == 512) {
        player->pathing_entity.forceMoveStartSceneTileX = g1(buf);
        player->pathing_entity.forceMoveStartSceneTileZ = g1(buf);
        player->pathing_entity.forceMoveEndSceneTileX = g1(buf);
        player->pathing_entity.forceMoveEndSceneTileZ = g1(buf);
        player->pathing_entity.forceMoveEndCycle = g2(buf) + _Client.loop_cycle;
        player->pathing_entity.forceMoveStartCycle = g2(buf) + _Client.loop_cycle;
        player->pathing_entity.forceMoveFaceDirection = g1(buf);
        if(ba_agility_probe_enabled&&player==c->local_player){PathingEntity*e=&player->pathing_entity;char line[256];snprintf(line,sizeof line,"# agility_exact %llu logic %d start %d/%d end %d/%d offsets %d/%d face %d",(unsigned long long)ba_timing_now(),_Client.loop_cycle,e->forceMoveStartSceneTileX,e->forceMoveStartSceneTileZ,e->forceMoveEndSceneTileX,e->forceMoveEndSceneTileZ,e->forceMoveEndCycle-_Client.loop_cycle,e->forceMoveStartCycle-_Client.loop_cycle,e->forceMoveFaceDirection);ba_cadence_diagnostic(line);}

        player->pathing_entity.pathLength = 0;
        player->pathing_entity.pathTileX[0] = player->pathing_entity.forceMoveEndSceneTileX;
        player->pathing_entity.pathTileZ[0] = player->pathing_entity.forceMoveEndSceneTileZ;
    }
}

void getPlayer(Client *c, Packet *buf, int size) {
    c->entityRemovalCount = 0;
    c->entityUpdateCount = 0;

    getPlayerLocal(c, buf, size);
    getPlayerOldVis(c, buf, size);
    getPlayerNewVis(c, size, buf);
    getPlayerExtended(c, buf, size);

    for (int i = 0; i < c->entityRemovalCount; i++) {
        int index = c->entityRemovalIds[i];
        if (c->players[index]->pathing_entity.cycle != _Client.loop_cycle) {
            free(c->players[i]);
            c->players[index] = NULL;
        }
    }

    if (buf->pos != size) {
        rs2_error("Error packet size mismatch in getplayer pos:%d psize:%d\n", buf->pos, size);
        // signlink.reporterror("Error packet size mismatch in getplayer pos:" + buf.pos + " psize:" + size);
        // throw new RuntimeException("eek");
    }

    for (int index = 0; index < c->player_count; index++) {
        if (!c->players[c->player_ids[index]]) {
            rs2_error("%s null entry in pl list - pos:%d size:%d\n", c->username, index, c->player_count);
            // signlink.reporterror(c->username + " null entry in pl list - pos:" + index + " size:" + c->playerCount);
            // throw new RuntimeException("eek");
        }
    }
}

static void client_clear_caches(void) {
    lrucache_clear(_LocType.modelCacheStatic);
    lrucache_clear(_LocType.modelCacheDynamic);
    lrucache_clear(_NpcType.modelCache);
    lrucache_clear(_ObjType.modelCache);
    lrucache_clear(_ObjType.iconCache);
    lrucache_clear(_PlayerEntity.modelCache);
    lrucache_clear(_SpotAnimType.modelCache);
    r57_reset_body_layers();
    bump_allocator_reset();
}

static void client_build_scene(Client *c) {
    uint64_t region_at=ba_timing_now(),phase_at=region_at;
    uint64_t reset_us=0,land_unpack_us=0,land_decode_us=0,loc_unpack_us=0,loc_place_us=0,build_us=0;
    h5_mark("region_scene_build_begin");
    BA_PROFILE_ALWAYS(BA_SCENE_BUILD);
    // try {
    c->minimap_level = -1;
    linklist_clear(c->merged_locations);
    linklist_clear(c->locList);
    linklist_clear(c->spotanims);
    linklist_clear(c->projectiles);
    pix3d_clear_texels();
    client_clear_caches();
    world3d_reset(c->scene);
    for (int level = 0; level < 4; level++) {
        collisionmap_reset(c->levelCollisionMap[level]);
    }

    World *world = world_new(104, 104, c->levelHeightmap, c->levelTileFlags);
    loctype_set_scene_origin(c->sceneBaseTileX,c->sceneBaseTileZ);
    _World.lowMemory = _World3D.lowMemory;

    reset_us=ba_timing_now()-phase_at;
    int maps = c->sceneMapIndexLength;

    for (int index = 0; index < maps; index++) {
        int mapsquareX = c->sceneMapIndex[index] >> 8;
        int mapsquareZ = c->sceneMapIndex[index] & 0xff;

        // underground pass check
        if (mapsquareX == 33 && mapsquareZ >= 71 && mapsquareZ <= 73) {
            _World.lowMemory = false;
            break;
        }
    }

    if (_World.lowMemory) {
        world3d_set_minlevel(c->scene, c->currentLevel);
    } else {
        world3d_set_minlevel(c->scene, 0);
    }

    int8_t *data = calloc(100000, sizeof(int8_t));

    // NO_TIMEOUT
    p1isaac(c->out, 108);
    for (int i = 0; i < maps; i++) {
        int x = (c->sceneMapIndex[i] >> 8) * 64 - c->sceneBaseTileX;
        int z = (c->sceneMapIndex[i] & 0xff) * 64 - c->sceneBaseTileZ;
        int8_t *src = c->sceneMapLandData[i];

        if (src) {
            Packet *buf = packet_new(src, c->sceneMapLandDataIndexLength[i]);
            int length = g4(buf);
            phase_at=ba_timing_now();
#if defined(__vita__) && defined(BUILD_A_EMBEDDED)
            uint32_t map_key=(uint32_t)c->sceneMapIndex[i];size_t source_n=(size_t)c->sceneMapLandDataIndexLength[i];
            int cached=length>0&&length<=100000&&r109_get(map_key,src,source_n,data,(size_t)length);
            if(!cached){bzip_decompress(data,src,c->sceneMapLandDataIndexLength[i]-4,4);if(length>0&&length<=100000)r109_put(map_key,src,source_n,data,(size_t)length);}
#else
            bzip_decompress(data, src, c->sceneMapLandDataIndexLength[i] - 4, 4);
#endif
            land_unpack_us+=ba_timing_now()-phase_at;
            free(buf);
            phase_at=ba_timing_now();
            world_load_ground(world, (c->sceneCenterZoneX - 6) * 8, (c->sceneCenterZoneZ - 6) * 8, x, z, data, length);
            land_decode_us+=ba_timing_now()-phase_at;
        } else if (c->sceneCenterZoneZ < 800) {
            clearLandscape(world, z, x, 64, 64);
        }
    }

    // NO_TIMEOUT
    p1isaac(c->out, 108);
    for (int i = 0; i < maps; i++) {
        int8_t *src = c->sceneMapLocData[i];
        if (src) {
            Packet *buf = packet_new(src, c->sceneMapLocDataIndexLength[i]);
            int length = g4(buf);
            phase_at=ba_timing_now();
#if defined(__vita__) && defined(BUILD_A_EMBEDDED)
            uint32_t map_key=0x10000u|(uint32_t)c->sceneMapIndex[i];size_t source_n=(size_t)c->sceneMapLocDataIndexLength[i];
            int cached=length>0&&length<=100000&&r109_get(map_key,src,source_n,data,(size_t)length);
            if(!cached){bzip_decompress(data,src,c->sceneMapLocDataIndexLength[i]-4,4);if(length>0&&length<=100000)r109_put(map_key,src,source_n,data,(size_t)length);}
#else
            bzip_decompress(data, src, c->sceneMapLocDataIndexLength[i] - 4, 4);
#endif
            loc_unpack_us+=ba_timing_now()-phase_at;
            free(buf);
            int x = (c->sceneMapIndex[i] >> 8) * 64 - c->sceneBaseTileX;
            int z = (c->sceneMapIndex[i] & 0xff) * 64 - c->sceneBaseTileZ;
            phase_at=ba_timing_now();
            world_load_locations(world, c->scene, c->locList, c->levelCollisionMap, data, length, x, z);
            loc_place_us+=ba_timing_now()-phase_at;
        }
    }

    free(data);

    // NO_TIMEOUT
    p1isaac(c->out, 108);
    phase_at=ba_timing_now();
    world_build(world, c->scene, c->levelCollisionMap);
    build_us=ba_timing_now()-phase_at;
    phase_at=ba_timing_now();
    pixmap_bind(c->area_viewport);

    // NO_TIMEOUT
    p1isaac(c->out, 108);
    for (LocEntity *loc = (LocEntity *)linklist_head(c->locList); loc; loc = (LocEntity *)linklist_next(c->locList)) {
        if ((c->levelTileFlags[1][loc->x][loc->z] & 0x2) == 2) {
            loc->level--;
            if (loc->level < 0) {
                linkable_unlink(&loc->link);
                free(loc);
            }
        }
    }

    for (int x = 0; x < 104; x++) {
        for (int z = 0; z < 104; z++) {
            sortObjStacks(c, x, z);
        }
    }

    for (LocAddEntity *loc = (LocAddEntity *)linklist_head(c->spawned_locations); loc; loc = (LocAddEntity *)linklist_next(c->spawned_locations)) {
        addLoc(c, loc->plane, loc->x, loc->z, loc->locIndex, loc->angle, loc->shape, loc->layer);
    }
    // } catch (Exception ignored) {
    // }

    lrucache_clear(_LocType.modelCacheStatic);
    pix3d_init_pool(PIX3D_POOL_COUNT);
    world_free(world);
    char region_line[384];
    snprintf(region_line,sizeof region_line,"# region_build %llu base %d %d maps %d reset %llu land_unpack %llu land_decode %llu loc_unpack %llu loc_place %llu build %llu finish %llu total %llu",(unsigned long long)ba_timing_now(),c->sceneBaseTileX,c->sceneBaseTileZ,maps,(unsigned long long)reset_us,(unsigned long long)land_unpack_us,(unsigned long long)land_decode_us,(unsigned long long)loc_unpack_us,(unsigned long long)loc_place_us,(unsigned long long)build_us,(unsigned long long)(ba_timing_now()-phase_at),(unsigned long long)(ba_timing_now()-region_at));
    ba_cadence_diagnostic(region_line);
}

void drawMinimapLoc(Client *c, int tileX, int tileZ, int level, int wallRgb, int doorRgb) {
    int bitset = world3d_get_wallbitset(c->scene, level, tileX, tileZ);
    if (bitset != 0) {
        int info = world3d_get_info(c->scene, level, tileX, tileZ, bitset);
        int angle = info >> 6 & 0x3;
        int shape = info & 0x1f;
        int rgb = wallRgb;
        if (bitset > 0) {
            rgb = doorRgb;
        }

        int *dst = c->image_minimap->pixels;
        int offset = tileX * 4 + (104 - 1 - tileZ) * 512 * 4 + 24624;
        int locId = bitset >> 14 & 0x7fff;

        LocType *loc = loctype_get(locId);
        if (loc->mapscene == -1) {
            if (shape == WALL_STRAIGHT || shape == WALL_L) {
                if (angle == 0) {
                    dst[offset] = rgb;
                    dst[offset + 512] = rgb;
                    dst[offset + 1024] = rgb;
                    dst[offset + 1536] = rgb;
                } else if (angle == 1) {
                    dst[offset] = rgb;
                    dst[offset + 1] = rgb;
                    dst[offset + 2] = rgb;
                    dst[offset + 3] = rgb;
                } else if (angle == 2) {
                    dst[offset + 3] = rgb;
                    dst[offset + 3 + 512] = rgb;
                    dst[offset + 3 + 1024] = rgb;
                    dst[offset + 3 + 1536] = rgb;
                } else if (angle == 3) {
                    dst[offset + 1536] = rgb;
                    dst[offset + 1536 + 1] = rgb;
                    dst[offset + 1536 + 2] = rgb;
                    dst[offset + 1536 + 3] = rgb;
                }
            }

            if (shape == WALL_SQUARECORNER) {
                if (angle == 0) {
                    dst[offset] = rgb;
                } else if (angle == 1) {
                    dst[offset + 3] = rgb;
                } else if (angle == 2) {
                    dst[offset + 3 + 1536] = rgb;
                } else if (angle == 3) {
                    dst[offset + 1536] = rgb;
                }
            }

            if (shape == WALL_L) {
                if (angle == 3) {
                    dst[offset] = rgb;
                    dst[offset + 512] = rgb;
                    dst[offset + 1024] = rgb;
                    dst[offset + 1536] = rgb;
                } else if (angle == 0) {
                    dst[offset] = rgb;
                    dst[offset + 1] = rgb;
                    dst[offset + 2] = rgb;
                    dst[offset + 3] = rgb;
                } else if (angle == 1) {
                    dst[offset + 3] = rgb;
                    dst[offset + 3 + 512] = rgb;
                    dst[offset + 3 + 1024] = rgb;
                    dst[offset + 3 + 1536] = rgb;
                } else if (angle == 2) {
                    dst[offset + 1536] = rgb;
                    dst[offset + 1536 + 1] = rgb;
                    dst[offset + 1536 + 2] = rgb;
                    dst[offset + 1536 + 3] = rgb;
                }
            }
        } else {
            Pix8 *scene = (ui_water_preview()&&locId==85&&loc->mapscene==22)?NULL:c->image_mapscene[loc->mapscene];
            if (scene) {
                int offsetX = (loc->width * 4 - scene->width) / 2;
                int offsetY = (loc->length * 4 - scene->height) / 2;
                pix8_draw(scene, tileX * 4 + 48 + offsetX, (104 - tileZ - loc->length) * 4 + offsetY + 48);
            }
        }
    }

    bitset = world3d_get_locbitset(c->scene, level, tileX, tileZ);
    if (bitset != 0) {
        int info = world3d_get_info(c->scene, level, tileX, tileZ, bitset);
        int angle = info >> 6 & 0x3;
        int shape = info & 0x1f;
        int locId = bitset >> 14 & 0x7fff;
        LocType *loc = loctype_get(locId);

        if (loc->mapscene != -1) {
            Pix8 *scene = (ui_water_preview()&&locId==85&&loc->mapscene==22)?NULL:c->image_mapscene[loc->mapscene];
            if (scene) {
                int offsetX = (loc->width * 4 - scene->width) / 2;
                int offsetY = (loc->length * 4 - scene->height) / 2;
                pix8_draw(scene, tileX * 4 + 48 + offsetX, (104 - tileZ - loc->length) * 4 + offsetY + 48);
            }
        } else if (shape == WALL_DIAGONAL) {
            int rgb = 0xeeeeee;
            if (bitset > 0) {
                rgb = 0xee0000;
            }

            int *dst = c->image_minimap->pixels;
            int offset = tileX * 4 + (104 - 1 - tileZ) * 512 * 4 + 24624;

            if (angle == 0 || angle == 2) {
                dst[offset + 1536] = rgb;
                dst[offset + 1024 + 1] = rgb;
                dst[offset + 512 + 2] = rgb;
                dst[offset + 3] = rgb;
            } else {
                dst[offset] = rgb;
                dst[offset + 512 + 1] = rgb;
                dst[offset + 1024 + 2] = rgb;
                dst[offset + 1536 + 3] = rgb;
            }
        }
    }

    bitset = world3d_get_grounddecorationbitset(c->scene, level, tileX, tileZ);
    if (bitset != 0) {
        int locId = bitset >> 14 & 0x7fff;
        LocType *loc = loctype_get(locId);
        if (loc->mapscene != -1) {
            Pix8 *scene = (ui_water_preview()&&locId==85&&loc->mapscene==22)?NULL:c->image_mapscene[loc->mapscene];
            if (scene) {
                int offsetX = (loc->width * 4 - scene->width) / 2;
                int offsetY = (loc->length * 4 - scene->height) / 2;
                pix8_draw(scene, tileX * 4 + 48 + offsetX, (104 - tileZ - loc->length) * 4 + offsetY + 48);
            }
        }
    }
}

void createMinimap(Client *c, int level) {
    int *pixels = c->image_minimap->pixels;
    int length = c->image_minimap->width * c->image_minimap->height;
    for (int i = 0; i < length; i++) {
        pixels[i] = 0;
    }

    for (int z = 1; z < 104 - 1; z++) {
        int offset = (104 - 1 - z) * 512 * 4 + 24628;

        for (int x = 1; x < 104 - 1; x++) {
            if ((c->levelTileFlags[level][x][z] & 0x18) == 0) {
                world3d_draw_minimaptile(c->scene, level, x, z, pixels, offset, 512);
            }

            if (level < 3 && (c->levelTileFlags[level + 1][x][z] & 0x8) != 0) {
                world3d_draw_minimaptile(c->scene, level + 1, x, z, pixels, offset, 512);
            }

            offset += 4;
        }
    }

    int wallRgb = (((int)(jrand() * 20.0) + 238 - 10) << 16) + (((int)(jrand() * 20.0) + 238 - 10) << 8) + (int)(jrand() * 20.0) + 238 - 10;
    int doorRgb = ((int)(jrand() * 20.0) + 238 - 10) << 16;

    pix24_bind(c->image_minimap);

    for (int z = 1; z < 104 - 1; z++) {
        for (int x = 1; x < 104 - 1; x++) {
            if ((c->levelTileFlags[level][x][z] & 0x18) == 0) {
                drawMinimapLoc(c, x, z, level, wallRgb, doorRgb);
            }

            if (level < 3 && (c->levelTileFlags[level + 1][x][z] & 0x8) != 0) {
                drawMinimapLoc(c, x, z, level + 1, wallRgb, doorRgb);
            }
        }
    }

    pixmap_bind(c->area_viewport);
    c->activeMapFunctionCount = 0;

    for (int x = 0; x < 104; x++) {
        for (int z = 0; z < 104; z++) {
            int bitset = world3d_get_grounddecorationbitset(c->scene, c->currentLevel, x, z);
            if (bitset == 0) {
                continue;
            }

            bitset = bitset >> 14 & 0x7fff;

            int func = loctype_get(bitset)->mapfunction;
            if (func < 0) {
                continue;
            }

            int stx = x;
            int stz = z;

            if (func != 22 && func != 29 && func != 34 && func != 36 && func != 46 && func != 47 && func != 48) {
                int8_t maxX = 104;
                int8_t maxZ = 104;
                int **flags = c->levelCollisionMap[c->currentLevel]->flags;

                for (int i = 0; i < 10; i++) {
                    int random = (int)(jrand() * 4.0);
                    if (random == 0 && stx > 0 && stx > x - 3 && (flags[stx - 1][stz] & 0x280108) == 0) {
                        stx--;
                    }

                    if (random == 1 && stx < maxX - 1 && stx < x + 3 && (flags[stx + 1][stz] & 0x280180) == 0) {
                        stx++;
                    }

                    if (random == 2 && stz > 0 && stz > z - 3 && (flags[stx][stz - 1] & 0x280102) == 0) {
                        stz--;
                    }

                    if (random == 3 && stz < maxZ - 1 && stz < z + 3 && (flags[stx][stz + 1] & 0x280120) == 0) {
                        stz++;
                    }
                }
            }

            c->activeMapFunctions[c->activeMapFunctionCount] = c->image_mapfunction[func];
            c->activeMapFunctionX[c->activeMapFunctionCount] = stx;
            c->activeMapFunctionZ[c->activeMapFunctionCount] = stz;
            c->activeMapFunctionCount++;
        }
    }
    ve_map_gl_invalidate();
}

void closeInterfaces(Client *c) {
    ba_interface_trace("CLOSE_MODAL",c->viewport_interface_id,"");
    // CLOSE_MODAL
    p1isaac(c->out, 231);

    if (c->sidebar_interface_id != -1) {
        c->sidebar_interface_id = -1;
        c->redraw_sidebar = true;
        c->pressed_continue_option = false;
        c->redraw_sideicons = true;
    }

    if (c->chat_interface_id != -1) {
        c->chat_interface_id = -1;
        c->redraw_chatback = true;
        c->pressed_continue_option = false;
    }

    c->viewport_interface_id = -1;
}

void addLoc(Client *c, int level, int x, int z, int id, int angle, int shape, int layer) {
    if (x < 1 || z < 1 || x > 102 || z > 102) {
        return;
    }

    if (_Client.lowmem && level != c->currentLevel) {
        return;
    }

    int bitset = 0;

    if (layer == 0) {
        bitset = world3d_get_wallbitset(c->scene, level, x, z);
    }

    if (layer == 1) {
        bitset = world3d_get_walldecorationbitset(c->scene, level, z, x);
    }

    if (layer == 2) {
        bitset = world3d_get_locbitset(c->scene, level, x, z);
    }

    if (layer == 3) {
        bitset = world3d_get_grounddecorationbitset(c->scene, level, x, z);
    }

    if (bitset != 0) {
        int otherInfo = world3d_get_info(c->scene, level, x, z, bitset);
        int otherId = bitset >> 14 & 0x7fff;
        int otherShape = otherInfo & 0x1f;
        int otherRotation = otherInfo >> 6;

        if (layer == 0) {
            world3d_remove_wall(c->scene, level, x, z, 1);
            LocType *type = loctype_get(otherId);

            if (type->blockwalk) {
                collisionmap_del_wall(c->levelCollisionMap[level], x, z, otherShape, otherRotation, type->blockrange);
            }
        }

        if (layer == 1) {
            world3d_remove_walldecoration(c->scene, level, x, z);
        }

        if (layer == 2) {
            world3d_remove_loc(c->scene, level, x, z);
            LocType *type = loctype_get(otherId);

            if (x + type->width > 104 - 1 || z + type->width > 104 - 1 || x + type->length > 104 - 1 || z + type->length > 104 - 1) {
                return;
            }

            if (type->blockwalk) {
                collisionmap_del_loc(c->levelCollisionMap[level], x, z, type->width, type->length, otherRotation, type->blockrange);
            }
        }

        if (layer == 3) {
            world3d_remove_grounddecoration(c->scene, level, x, z);
            LocType *type = loctype_get(otherId);

            if (type->blockwalk && type->active) {
                collisionmap_remove_blocked(c->levelCollisionMap[level], x, z);
            }
        }
    }

    if (id >= 0) {
        int tileLevel = level;

        if (level < 3 && (c->levelTileFlags[1][x][z] & 0x2) == 2) {
            tileLevel = level + 1;
        }

        world_add_loc(level, x, z, c->scene, c->levelHeightmap, c->locList, c->levelCollisionMap[level], id, shape, angle, tileLevel);
    }
}

void sortObjStacks(Client *c, int x, int z) {
    LinkList *objStacks = c->level_obj_stacks[c->currentLevel][x][z];
    if (!objStacks) {
        world3d_remove_objstack(c->scene, c->currentLevel, x, z);
        return;
    }

    int topCost = -99999999;
    ObjStackEntity *topObj = NULL;

    for (ObjStackEntity *obj = (ObjStackEntity *)linklist_head(objStacks); obj != NULL; obj = (ObjStackEntity *)linklist_next(objStacks)) {
        ObjType *type = objtype_get(obj->index);
        int cost = type->cost;

        if (type->stackable) {
            cost *= obj->count + 1;
        }

        if (cost > topCost) {
            topCost = cost;
            topObj = obj;
        }
    }

    linklist_add_head(objStacks, &topObj->link);

    int bottomObjId = -1;
    int middleObjId = -1;
    int bottomObjCount = 0;
    int middleObjCount = 0;
    for (ObjStackEntity *obj = (ObjStackEntity *)linklist_head(objStacks); obj; obj = (ObjStackEntity *)linklist_next(objStacks)) {
        if (obj->index != topObj->index && bottomObjId == -1) {
            bottomObjId = obj->index;
            bottomObjCount = obj->count;
        }

        if (obj->index != topObj->index && obj->index != bottomObjId && middleObjId == -1) {
            middleObjId = obj->index;
            middleObjCount = obj->count;
        }
    }

    Model *bottomObj = NULL;
    if (bottomObjId != -1) {
        bottomObj = objtype_get_interfacemodel(objtype_get(bottomObjId), bottomObjCount, true);
    }

    Model *middleObj = NULL;
    if (middleObjId != -1) {
        middleObj = objtype_get_interfacemodel(objtype_get(middleObjId), middleObjCount, true);
    }

    int bitset = x + (z << 7) + 1610612736;
    ObjType *type = objtype_get(topObj->index);
    world3d_add_objstack(c->scene, x, z, getHeightmapY(c, c->currentLevel, x * 128 + 64, z * 128 + 64), c->currentLevel, bitset, objtype_get_interfacemodel(type, topObj->count, true), middleObj, bottomObj);
}

void readZonePacket(Client *c, Packet *buf, int opcode) {
    int pos = g1(buf);
    int x = c->baseX + (pos >> 4 & 0x7);
    int z = c->baseZ + (pos & 0x7);

    if (opcode == 59 || opcode == 76) {
        // LOC_ADD_CHANGE || LOC_DEL
        int info = g1(buf);
        int shape = info >> 2;
        int angle = info & 0x3;
        int layer = LOC_SHAPE_TO_LAYER[shape];
        int id;
        if (opcode == 76) {
            id = -1;
        } else {
            id = g2(buf);
        }
        if (x >= 0 && z >= 0 && x < 104 && z < 104) {
            LocAddEntity *loc = NULL;
            for (LocAddEntity *next = (LocAddEntity *)linklist_head(c->spawned_locations); next != NULL; next = (LocAddEntity *)linklist_next(c->spawned_locations)) {
                if (next->plane == c->currentLevel && next->x == x && next->z == z && next->layer == layer) {
                    loc = next;
                    break;
                }
            }
            if (!loc) {
                int bitset = 0;
                int otherId = -1;
                int otherShape = 0;
                int otherAngle = 0;
                if (layer == 0) {
                    bitset = world3d_get_wallbitset(c->scene, c->currentLevel, x, z);
                }
                if (layer == 1) {
                    bitset = world3d_get_walldecorationbitset(c->scene, c->currentLevel, z, x);
                }
                if (layer == 2) {
                    bitset = world3d_get_locbitset(c->scene, c->currentLevel, x, z);
                }
                if (layer == 3) {
                    bitset = world3d_get_grounddecorationbitset(c->scene, c->currentLevel, x, z);
                }
                if (bitset != 0) {
                    int otherInfo = world3d_get_info(c->scene, c->currentLevel, x, z, bitset);
                    otherId = bitset >> 14 & 0x7fff;
                    otherShape = otherInfo & 0x1f;
                    otherAngle = otherInfo >> 6;
                }
                loc = calloc(1, sizeof(LocAddEntity));
                loc->plane = c->currentLevel;
                loc->layer = layer;
                loc->x = x;
                loc->z = z;
                loc->lastLocIndex = otherId;
                loc->lastShape = otherShape;
                loc->lastAngle = otherAngle;
                linklist_add_tail(c->spawned_locations, &loc->link);
            }
            loc->locIndex = id;
            loc->shape = shape;
            loc->angle = angle;
            addLoc(c, c->currentLevel, x, z, id, angle, shape, layer);
        }
    } else if (opcode == 42) {
        // LOC_ANIM
        int info = g1(buf);
        int shape = info >> 2;
        int layer = LOC_SHAPE_TO_LAYER[shape];
        int id = g2(buf);
        if (x >= 0 && z >= 0 && x < 104 && z < 104) {
            int bitset = 0;
            if (layer == 0) {
                bitset = world3d_get_wallbitset(c->scene, c->currentLevel, x, z);
            }
            if (layer == 1) {
                bitset = world3d_get_walldecorationbitset(c->scene, c->currentLevel, z, x);
            }
            if (layer == 2) {
                bitset = world3d_get_locbitset(c->scene, c->currentLevel, x, z);
            }
            if (layer == 3) {
                bitset = world3d_get_grounddecorationbitset(c->scene, c->currentLevel, x, z);
            }
            if (bitset != 0) {
                LocEntity *loc = locentity_new(bitset >> 14 & 0x7fff, c->currentLevel, layer, x, z, _SeqType.instances[id], false);
                linklist_add_tail(c->locList, &loc->link);
            }
        }
    } else if (opcode == 223) {
        // OBJ_ADD
        int id = g2(buf);
        int count = g2(buf);
        if (x >= 0 && z >= 0 && x < 104 && z < 104) {
            ObjStackEntity *obj = objstackentity_new();
            obj->index = id;
            obj->count = count;
            if (!c->level_obj_stacks[c->currentLevel][x][z]) {
                c->level_obj_stacks[c->currentLevel][x][z] = linklist_new();
            }
            linklist_add_tail(c->level_obj_stacks[c->currentLevel][x][z], &obj->link);
            sortObjStacks(c, x, z);
        }
    } else if (opcode == 49) {
        // OBJ_DEL
        int id = g2(buf);
        if (x >= 0 && z >= 0 && x < 104 && z < 104) {
            LinkList *list = c->level_obj_stacks[c->currentLevel][x][z];
            if (list) {
                for (ObjStackEntity *next = (ObjStackEntity *)linklist_head(list); next; next = (ObjStackEntity *)linklist_next(list)) {
                    if (next->index == (id & 0x7fff)) {
                        linkable_unlink(&next->link);
                        free(next);
                        break;
                    }
                }
                if (!linklist_head(list)) {
                    linklist_free(c->level_obj_stacks[c->currentLevel][x][z]);
                    c->level_obj_stacks[c->currentLevel][x][z] = NULL;
                }
                sortObjStacks(c, x, z);
            }
        }
    } else if (opcode == 69) {
        // MAP_PROJANIM
        int dx = x + g1b(buf);
        int dz = z + g1b(buf);
        int target = g2b(buf);
        int spotanim = g2(buf);
        int srcHeight = g1(buf);
        int dstHeight = g1(buf);
        int startDelay = g2(buf);
        int endDelay = g2(buf);
        int peak = g1(buf);
        int arc = g1(buf);
        if (x >= 0 && z >= 0 && x < 104 && z < 104 && dx >= 0 && dz >= 0 && dx < 104 && dz < 104) {
            x = x * 128 + 64;
            z = z * 128 + 64;
            dx = dx * 128 + 64;
            dz = dz * 128 + 64;
            ProjectileEntity *proj = projectileentity_new(spotanim, c->currentLevel, x, getHeightmapY(c, c->currentLevel, x, z) - srcHeight, z, startDelay + _Client.loop_cycle, endDelay + _Client.loop_cycle, peak, arc, target, dstHeight);
            projectileentity_update_velocity(proj, dx, getHeightmapY(c, c->currentLevel, dx, dz) - dstHeight, dz, startDelay + _Client.loop_cycle);
            linklist_add_tail(c->projectiles, &proj->entity.link);
        }
    } else if (opcode == 191) {
        // MAP_ANIM
        int id = g2(buf);
        int height = g1(buf);
        int delay = g2(buf);
        if (x >= 0 && z >= 0 && x < 104 && z < 104) {
            x = x * 128 + 64;
            z = z * 128 + 64;
            SpotAnimEntity *spotanim = spotanimentity_new(id, c->currentLevel, x, z, getHeightmapY(c, c->currentLevel, x, z) - height, _Client.loop_cycle, delay);
            linklist_add_tail(c->spotanims, &spotanim->entity.link);
        }
    } else if (opcode == 50) {
        // OBJ_REVEAL
        int id = g2(buf);
        int count = g2(buf);
        int receiver = g2(buf);
        if (x >= 0 && z >= 0 && x < 104 && z < 104 && receiver != c->local_pid) {
            ObjStackEntity *obj = objstackentity_new();
            obj->index = id;
            obj->count = count;
            if (!c->level_obj_stacks[c->currentLevel][x][z]) {
                c->level_obj_stacks[c->currentLevel][x][z] = linklist_new();
            }
            linklist_add_tail(c->level_obj_stacks[c->currentLevel][x][z], &obj->link);
            sortObjStacks(c, x, z);
        }
    } else if (opcode == 23) {
        // LOC_MERGE
        int info = g1(buf);
        int shape = info >> 2;
        int angle = info & 0x3;
        int layer = LOC_SHAPE_TO_LAYER[shape];
        int id = g2(buf);
        int start = g2(buf);
        int end = g2(buf);
        int pid = g2(buf);
        int8_t east = g1b(buf);
        int8_t south = g1b(buf);
        int8_t west = g1b(buf);
        int8_t north = g1b(buf);

        PlayerEntity *player;
        if (pid == c->local_pid) {
            player = c->local_player;
        } else {
            player = c->players[pid];
        }

        if (player) {
            LocMergeEntity *loc1 = locmergeentity_new(c->currentLevel, layer, x, z, -1, angle, shape, start + _Client.loop_cycle);
            linklist_add_tail(c->merged_locations, &loc1->link);

            LocMergeEntity *loc2 = locmergeentity_new(c->currentLevel, layer, x, z, id, angle, shape, end + _Client.loop_cycle);
            linklist_add_tail(c->merged_locations, &loc2->link);

            int y0 = c->levelHeightmap[c->currentLevel][x][z];
            int y1 = c->levelHeightmap[c->currentLevel][x + 1][z];
            int y2 = c->levelHeightmap[c->currentLevel][x + 1][z + 1];
            int y3 = c->levelHeightmap[c->currentLevel][x][z + 1];
            LocType *loc = loctype_get(id);

            player->locStartCycle = start + _Client.loop_cycle;
            player->locStopCycle = end + _Client.loop_cycle;
            player->locModel = loctype_get_model(loctype_visual_for_scene(loc,c->currentLevel,x,z,shape,angle), shape, angle, y0, y1, y2, y3, -1);

            int width = loc->width;
            int height = loc->length;
            if (angle == 1 || angle == 3) {
                width = loc->length;
                height = loc->width;
            }

            player->locOffsetX = x * 128 + width * 64;
            player->locOffsetZ = z * 128 + height * 64;
            player->locOffsetY = getHeightmapY(c, c->currentLevel, player->locOffsetX, player->locOffsetZ);

            int8_t tmp;
            if (east > west) {
                tmp = east;
                east = west;
                west = tmp;
            }

            if (south > north) {
                tmp = south;
                south = north;
                north = tmp;
            }

            player->minTileX = x + east;
            player->maxTileX = x + west;
            player->minTileZ = z + south;
            player->maxTileZ = z + north;
        }
    } else if (opcode == 151) {
        // OBJ_COUNT
        int id = g2(buf);
        int oldCount = g2(buf);
        int newCount = g2(buf);
        if (x >= 0 && z >= 0 && x < 104 && z < 104) {
            LinkList *list = c->level_obj_stacks[c->currentLevel][x][z];
            if (list) {
                for (ObjStackEntity *next = (ObjStackEntity *)linklist_head(list); next; next = (ObjStackEntity *)linklist_next(list)) {
                    if (next->index == (id & 0x7fff) && next->count == oldCount) {
                        next->count = newCount;
                        break;
                    }
                }
                sortObjStacks(c, x, z);
            }
        }
    }
}

static void recovery_npc_trace(Client*c,NpcEntity*n,int id,const char*stage,int reason){
#ifdef BUILD_A_EMBEDDED
 if(!n||!n->type||!n->type->name||id<0||id>=8192)return;
 if(strcmp(n->type->name,"Hans")&&strcmp(n->type->name,"Cook"))return;
 static unsigned last[8192];static int previous[8192];
 if(!strcmp(stage,"SCENE")){
  if(previous[id]==reason&&_Client.loop_cycle-last[id]<50)return;
  previous[id]=reason;last[id]=_Client.loop_cycle;
 }
 FILE*f=fopen("ux0:/data/RS2Vita/npc-presence.csv","a");
 if(f){fprintf(f,"%llu,%s,%d,%s,%lld,%d,%d,%d,%d,%d\n",
  (unsigned long long)ba_timing_now(),stage,id,n->type->name,(long long)n->type->index,
  n->pathing_entity.x/128+c->sceneBaseTileX,n->pathing_entity.z/128+c->sceneBaseTileZ,
  c->local_player->pathing_entity.x/128+c->sceneBaseTileX,
  c->local_player->pathing_entity.z/128+c->sceneBaseTileZ,reason);fclose(f);}
#else
 (void)c;(void)n;(void)id;(void)stage;(void)reason;
#endif
}
void getNpcPos(Client *c, Packet *buf, int size) {
    c->entityRemovalCount = 0;
    c->entityUpdateCount = 0;

    getNpcPosOldVis(c, buf, size);
    getNpcPosNewVis(c, buf, size);
    getNpcPosExtended(c, buf, size);

    for (int i = 0; i < c->entityRemovalCount; i++) {
        int index = c->entityRemovalIds[i];
        if (c->npcs[index]->pathing_entity.cycle != _Client.loop_cycle) {
            recovery_npc_trace(c,c->npcs[index],index,"PACKET_REMOVE",0);
            c->npcs[index]->type = NULL;
            free(c->npcs[index]);
            c->npcs[index] = NULL;
        }
    }

    if (buf->pos != size) {
        rs2_error("%s size mismatch in getnpcpos - pos:%d psize:%d\n", c->username, buf->pos, size);
        // signlink.reporterror(c->username + " size mismatch in getnpcpos - pos:" + buf.pos + " psize:" + size);
        // throw new RuntimeException("eek");
    }

    for (int i = 0; i < c->npc_count; i++) {
        if (c->npcs[c->npc_ids[i]] == NULL) {
            rs2_error("%s null entry in npc lit - pos:%d size:%d\n", c->username, i, c->npc_count);
            // signlink.reporterror(c->username + " null entry in npc list - pos:" + i + " size:" + c->npcCount);
            // throw new RuntimeException("eek");
        }
    }
}

void getNpcPosExtended(Client *c, Packet *buf, int size) {
    (void)size;
    for (int i = 0; i < c->entityUpdateCount; i++) {
        int id = c->entityUpdateIds[i];
        NpcEntity *npc = c->npcs[id];
        int mask = g1(buf);

        npc->pathing_entity.lastMask = mask;
        npc->pathing_entity.lastMaskCycle = _Client.loop_cycle;

        if ((mask & 0x2) == 2) {
            int seqId = g2(buf);
            if (seqId == 65535) {
                seqId = -1;
            }
            if (seqId == npc->pathing_entity.primarySeqId) {
                npc->pathing_entity.primarySeqLoop = 0;
            }
            int delay = g1(buf);
            if (seqId == -1 || npc->pathing_entity.primarySeqId == -1 || _SeqType.instances[seqId]->priority > _SeqType.instances[npc->pathing_entity.primarySeqId]->priority || _SeqType.instances[npc->pathing_entity.primarySeqId]->priority == 0) {
                npc->pathing_entity.primarySeqId = seqId;
                npc->pathing_entity.primarySeqFrame = 0;
                npc->pathing_entity.primarySeqCycle = 0;
                npc->pathing_entity.primarySeqDelay = delay;
                npc->pathing_entity.primarySeqLoop = 0;
            }
        }
        if ((mask & 0x4) == 4) {
            npc->pathing_entity.targetId = g2(buf);
            if (npc->pathing_entity.targetId == 65535) {
                npc->pathing_entity.targetId = -1;
            }
        }
        if ((mask & 0x8) == 8) {
            strcpy(npc->pathing_entity.chat, gjstr(buf));
            npc->pathing_entity.chatTimer = 100;
        }
        if ((mask & 0x10) == 16) {
            npc->pathing_entity.damage = g1(buf);
            npc->pathing_entity.damageType = g1(buf);
            npc->pathing_entity.combatCycle = _Client.loop_cycle + 400;
            npc->pathing_entity.health = g1(buf);
            npc->pathing_entity.totalHealth = g1(buf);
        }
        if ((mask & 0x20) == 32) {
            npc->type = npctype_get(g2(buf));
            npc->pathing_entity.seqWalkId = npc->type->walkanim;
            npc->pathing_entity.seqTurnAroundId = npc->type->walkanim_b;
            npc->pathing_entity.seqTurnLeftId = npc->type->walkanim_r;
            npc->pathing_entity.seqTurnRightId = npc->type->walkanim_l;
            npc->pathing_entity.seqStandId = npc->type->readyanim;
        }
        if ((mask & 0x40) == 64) {
            npc->pathing_entity.spotanimId = g2(buf);
            int info = g4(buf);
            npc->pathing_entity.spotanimOffset = info >> 16;
            npc->pathing_entity.spotanimLastCycle = _Client.loop_cycle + (info & 0xffff);
            npc->pathing_entity.spotanimFrame = 0;
            npc->pathing_entity.spotanimCycle = 0;
            if (npc->pathing_entity.spotanimLastCycle > _Client.loop_cycle) {
                npc->pathing_entity.spotanimFrame = -1;
            }
            if (npc->pathing_entity.spotanimId == 65535) {
                npc->pathing_entity.spotanimId = -1;
            }
        }
        if ((mask & 0x80) == 128) {
            npc->pathing_entity.targetTileX = g2(buf);
            npc->pathing_entity.targetTileZ = g2(buf);
            npc->pathing_entity.lastFaceX = npc->pathing_entity.targetTileX;
            npc->pathing_entity.lastFaceZ = npc->pathing_entity.targetTileZ;
        }
    }
}

void getNpcPosNewVis(Client *c, Packet *buf, int size) {
    while (buf->bit_pos + 21 < size * 8) {
        int index = gbit(buf, 13);
        if (index == 8191) {
            break;
        }
        if (!c->npcs[index]) {
            c->npcs[index] = npcentity_new();
        }
        NpcEntity *npc = c->npcs[index];
        c->npc_ids[c->npc_count++] = index;
        npc->pathing_entity.cycle = _Client.loop_cycle;
        npc->type = npctype_get(gbit(buf, 11));
        npc->pathing_entity.size = npc->type->size;
        npc->pathing_entity.seqWalkId = npc->type->walkanim;
        npc->pathing_entity.seqTurnAroundId = npc->type->walkanim_b;
        npc->pathing_entity.seqTurnLeftId = npc->type->walkanim_r;
        npc->pathing_entity.seqTurnRightId = npc->type->walkanim_l;
        npc->pathing_entity.seqStandId = npc->type->readyanim;
        int dx = gbit(buf, 5);
        if (dx > 15) {
            dx -= 32;
        }
        int dz = gbit(buf, 5);
        if (dz > 15) {
            dz -= 32;
        }
        pathingentity_teleport(&npc->pathing_entity, false, c->local_player->pathing_entity.pathTileX[0] + dx, c->local_player->pathing_entity.pathTileZ[0] + dz);
        recovery_npc_trace(c,npc,index,"PACKET_ADD",0);
        int extendedInfo = gbit(buf, 1);
        if (extendedInfo == 1) {
            c->entityUpdateIds[c->entityUpdateCount++] = index;
        }
    }
    access_bytes(buf);
}

void getNpcPosOldVis(Client *c, Packet *buf, int size) {
    (void)size;
    access_bits(buf);

    int count = gbit(buf, 8);
    if (count < c->npc_count) {
        for (int i = count; i < c->npc_count; i++) {
            c->entityRemovalIds[c->entityRemovalCount++] = c->npc_ids[i];
        }
    }

    if (count > c->npc_count) {
        rs2_error("%s Too many npcs\n", c->username);
        // signlink.reporterror(c->username + " Too many npcs");
        // throw new RuntimeException("eek");
    }

    c->npc_count = 0;
    for (int i = 0; i < count; i++) {
        int index = c->npc_ids[i];
        NpcEntity *npc = c->npcs[index];

        int info = gbit(buf, 1);
        if (info == 0) {
            c->npc_ids[c->npc_count++] = index;
            npc->pathing_entity.cycle = _Client.loop_cycle;
        } else {
            int op = gbit(buf, 2);

            if (op == 0) {
                c->npc_ids[c->npc_count++] = index;
                npc->pathing_entity.cycle = _Client.loop_cycle;
                c->entityUpdateIds[c->entityUpdateCount++] = index;
            } else if (op == 1) {
                c->npc_ids[c->npc_count++] = index;
                npc->pathing_entity.cycle = _Client.loop_cycle;

                int walkDir = gbit(buf, 3);
                pathingentity_movealongroute(&npc->pathing_entity, false, walkDir);

                int extendedInfo = gbit(buf, 1);
                if (extendedInfo == 1) {
                    c->entityUpdateIds[c->entityUpdateCount++] = index;
                }
            } else if (op == 2) {
                c->npc_ids[c->npc_count++] = index;
                npc->pathing_entity.cycle = _Client.loop_cycle;

                int walkDir = gbit(buf, 3);
                pathingentity_movealongroute(&npc->pathing_entity, true, walkDir);
                int runDir = gbit(buf, 3);
                pathingentity_movealongroute(&npc->pathing_entity, true, runDir);

                int extendedInfo = gbit(buf, 1);
                if (extendedInfo == 1) {
                    c->entityUpdateIds[c->entityUpdateCount++] = index;
                }
            } else if (op == 3) {
                c->entityRemovalIds[c->entityRemovalCount++] = index;
            }
        }
    }
}

void client_add_message(Client *c, int type, const char *text, const char *sender) {
    if (type == 0 && c->sticky_chat_interface_id != -1) {
        strcpy(c->modal_message, text);
        c->shell->mouse_click_button = 0;
    }

    if (c->chat_interface_id == -1) {
        c->redraw_chatback = true;
    }

    for (int i = 99; i > 0; i--) {
        c->message_type[i] = c->message_type[i - 1];
        strcpy(c->message_sender[i], c->message_sender[i - 1]);
        strcpy(c->message_text[i], c->message_text[i - 1]);
    }

    // TODO: debug
    // if (c->show_debug && type == 0) {
    // 	text = "[" + (loopCycle / 30) + "]: " + text;
    // }

    c->message_type[0] = type;
    strcpy(c->message_sender[0], sender);
    strcpy(c->message_text[0], text);
}

void updateVarp(Client *c, int id) {
    int clientcode = _VarpType.instances[id]->clientcode;
    if (clientcode == 0) {
        return;
    }

    int value = c->varps[id];
    if (clientcode == 1) {
        if (value == 1) {
            pix3d_set_brightness(0.9);
        } else if (value == 2) {
            pix3d_set_brightness(0.8);
        } else if (value == 3) {
            pix3d_set_brightness(0.7);
        } else if (value == 4) {
            pix3d_set_brightness(0.6);
        }

        lrucache_clear(_ObjType.iconCache);
        c->redraw_background = true;
    // NOTE different volume values as bgsound element isn't used
    } else if (clientcode == 3) {
        bool lastMidiActive = c->midiActive;
        if (value == 0) {
            platform_set_midi_volume(1.0); // 0
            c->midiActive = true;
        } else if (value == 1) {
            platform_set_midi_volume(0.75); // -400
            c->midiActive = true;
        } else if (value == 2) {
            platform_set_midi_volume(0.5); // -800
            c->midiActive = true;
        } else if (value == 3) {
            platform_set_midi_volume(0.25); // -1200
            c->midiActive = true;
        } else if (value == 4) {
            c->midiActive = false;
        }

        if (c->midiActive != lastMidiActive) {
            if (c->midiActive) {
                platform_set_midi(c->currentMidi, c->midiCrc, c->midiSize);
            } else {
                platform_stop_midi();
            }

            c->nextMusicDelay = 0;
        }
    } else if (clientcode == 4) {
        if (value == 0) {
            c->wave_enabled = true;
            platform_set_wave_volume(127);
        } else if (value == 1) {
            c->wave_enabled = true;
            platform_set_wave_volume(96);
        } else if (value == 2) {
            c->wave_enabled = true;
            platform_set_wave_volume(64);
        } else if (value == 3) {
            c->wave_enabled = true;
            platform_set_wave_volume(32);
        } else if (value == 4) {
            c->wave_enabled = false;
        }
    } else if (clientcode == 5) {
        c->mouseButtonsOption = value;
    } else if (clientcode == 6) {
        c->chatEffects = value;
    } else if (clientcode == 8) {
        c->split_private_chat = value;
        c->redraw_chatback = true;
    }
}

void reset_interface_animation(int id) {
    Component *parent = _Component.instances[id];
    for (int i = 0; i < parent->childCount && parent->childId[i] != -1; i++) {
        Component *child = _Component.instances[parent->childId[i]];
        if (child->type == 1) {
            reset_interface_animation(child->id);
        }
        child->seqFrame = 0;
        child->seqCycle = 0;
    }
}

void client_try_reconnect(Client *c) {
    if (c->idle_timeout > 0) {
        client_logout(c);
    } else {
        pixmap_bind(c->area_viewport);
        drawStringCenter(c->font_plain12, 257, 144, "Connection lost", BLACK);
        drawStringCenter(c->font_plain12, 256, 143, "Connection lost", WHITE);
        drawStringCenter(c->font_plain12, 257, 159, "Please wait - attempting to reestablish", BLACK);
        drawStringCenter(c->font_plain12, 256, 158, "Please wait - attempting to reestablish", WHITE);
        if(!ve_enabled())pixmap_draw(c->area_viewport, 8, 11);
        c->flag_scene_tile_x = 0;
        ClientStream *stream = c->stream;
        c->ingame = false;

        client_login(c, c->username, c->password, true);
        if (!c->ingame) {
            client_logout(c);
        }

        // try {
        clientstream_destroy(stream);
        // } catch (@Pc(80) Exception ex) {
        // }
    }
}

void client_logout(Client *c) {
    // try {
    if (c->stream) {
        clientstream_destroy(c->stream);
    }
    // } catch (@Pc(9) Exception ignored) {
    // }

    c->stream = NULL;
    c->ingame = false;
    c->title_screen_state = 0;
    if (!_Custom.remember_username) {
        c->username[0] = '\0';
    }
    if (!_Custom.remember_password) {
        c->password[0] = '\0';
    }

    inputtracking_set_disabled(&_InputTracking);
    client_clear_caches();
    world3d_reset(c->scene);

    for (int level = 0; level < 4; level++) {
        collisionmap_reset(c->levelCollisionMap[level]);
    }

    platform_stop_midi();

    c->currentMidi[0] = '\0';
    c->nextMusicDelay = 0;
}

static int title_new_player;
int client_title_new_player(void){return title_new_player;}

void client_update_title(Client *c) {
 if(title_theme_pointer(c))return;
 int click_x=c->shell->mouse_click_x,click_y=c->shell->mouse_click_y;
 if(c->title_restored){
        const ApprovedTitleControl *boxes=c->title_screen_state==0?approved_welcome_controls:approved_login_controls;
        int count=c->title_screen_state==3?1:2;
        if(c->title_screen_state==3)boxes=&approved_create_control;
        bool button=false;
        for(int i=0;i<count;i++)if(approved_title_contains(&boxes[i],click_x,click_y)){
            click_x=TITLE_X(214)+boxes[i].logical_x;
            click_y=TITLE_Y(186)+boxes[i].logical_y;
            button=true;break;
        }
        if(!button){
            int field=c->title_screen_state==2?approved_title_field_at(click_x,click_y):-1;
            if(field>=0){click_x=TITLE_X(350);click_y=TITLE_Y(field?284:270);}
            else {click_x=-1000;click_y=-1000;}
        }
    }

    if (c->title_screen_state == 0) {
        int x = TITLE_X(394) - 80;
        int y = TITLE_Y(266) + 20;

        y += 20;
        if (c->shell->mouse_click_button == 1 && title_button_contains(click_x, click_y, x, y)) {
            c->title_screen_state = 3;
            c->title_login_field = 0;
#ifdef BUILD_A_EMBEDDED
            title_new_player=1;c->title_screen_state=2;
            c->username[0]=c->password[0]=0;
            c->login_message0="";c->login_message1="Choose a new offline username & password.";
#endif
        }

        x = TITLE_X(394) + 80;
        if (c->shell->mouse_click_button == 1 && title_button_contains(click_x, click_y, x, y)) {
            c->login_message0 = "";
            c->login_message1 = "Enter your username & password.";
            title_new_player=0;
            c->title_screen_state = 2;
            c->title_login_field = 0;
        }
    } else if (c->title_screen_state == 2) {
        int y = TITLE_Y(266) - 40;
        y += 30;
        y += 25;

#ifdef VITA_NATIVE_WIDESCREEN
        int field = title_login_field_at(click_x, click_y);
        if (c->shell->mouse_click_button == 1 && field >= 0) c->title_login_field = field;
#else
        if (c->shell->mouse_click_button == 1 && click_y >= y - 15 && click_y < y) {
            c->title_login_field = 0;
        }
#endif
        y += 15;

#ifndef VITA_NATIVE_WIDESCREEN
        if (c->shell->mouse_click_button == 1 && click_y >= y - 15 && click_y < y) {
            c->title_login_field = 1;
        }
#endif
        y += 15;

        int buttonX = TITLE_X(394) - 80;
        int buttonY = TITLE_Y(266) + 50;
        buttonY += 20;

        if (c->shell->mouse_click_button == 1 && title_button_contains(click_x, click_y, buttonX, buttonY)) {
#ifdef BUILD_A_EMBEDDED
            if(title_new_player){
                int64_t key=jstring_to_base37(c->username);
                if(key<=0){c->login_message0="";c->login_message1="Please choose a valid player name.";return;}
                char*name=jstring_from_base37(key);char path[180];
                snprintf(path,sizeof(path),"ux0:/data/RS2Vita/players/%s.sav",name);
                FILE*f=fopen(path,"rb");int exists=f!=NULL;if(f)fclose(f);
                snprintf(path,sizeof(path),"ux0:/data/RS2Vita/players/%s.sav.bak",name);free(name);
                f=fopen(path,"rb");if(f){exists=1;fclose(f);}
                if(exists){c->login_message0="That local player already exists.";c->login_message1="Use Existing User to load that player.";return;}
            }
#endif
            client_login(c, c->username, c->password, false);
        }

        buttonX = TITLE_X(394) + 80;
        if (c->shell->mouse_click_button == 1 && title_button_contains(click_x, click_y, buttonX, buttonY)) {
            c->title_screen_state = 0;
            if (!_Custom.remember_username) {
                c->username[0] = '\0';
            }
            if (!_Custom.remember_password) {
                c->password[0] = '\0';
            }
        }

        while (true) {
            while (true) {
                int key = poll_key(c->shell);
                if (key == -1) {
                    return;
                }

                bool valid = false;
                size_t len = strlen(CHARSET);
                for (size_t i = 0; i < len; i++) {
                    if (key == CHARSET[i]) {
                        valid = true;
                        break;
                    }
                }

                if (c->title_login_field == 0) {
                    len = strlen(c->username);
                    if (key == 8 && len > 0) {
                        c->username[len - 1] = '\0';
                    }

                    if (key == 9 || key == 10 || key == 13) {
                        c->title_login_field = 1;
                    }

                    if (valid) {
                        len = strlen(c->username);
                        if (len < USERNAME_LENGTH) {
                            c->username[len] = (char)key;
                            c->username[len + 1] = '\0';
                        }
                    }
                } else if (c->title_login_field == 1) {
                    len = strlen(c->password);
                    if (key == 8 && len > 0) {
                        c->password[len - 1] = '\0';
                    }

                    if (key == 9 || key == 10 || key == 13) {
                        c->title_login_field = 0;
                    }

                    if (valid) {
                        len = strlen(c->password);
                        if (len < PASSWORD_LENGTH) {
                            c->password[len] = (char)key;
                            c->password[len + 1] = '\0';
                        }
                    }
                }
            }
        }
    } else if (c->title_screen_state == 3) {
        int x = TITLE_X(394);
        int y = TITLE_Y(266) + 50;
        y += 20;

        if (c->shell->mouse_click_button == 1 && title_button_contains(click_x, click_y, x, y)) {
            c->title_screen_state = 0;
        }
    }
}

#ifdef BUILD_A_EMBEDDED
static void login_world_progress(void *ctx, int percent, const char *text) {
    Client *c = ctx;
    if (!c) return;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    static int last_percent = -1;
    static uint64_t last_draw;
    uint64_t now = rs2_now();
    if (percent != 100 && percent == last_percent) return;
    if (percent != 100 && last_draw && now - last_draw < 80) return;
    last_percent = percent;
    last_draw = now;
    char line[96];
    snprintf(line, sizeof(line), "%s - %d%%", text && text[0] ? text : "Building world", percent);
    client_draw_progress(c, line, percent);
    FILE *f = fopen("ux0:/data/RS2Vita/world-load.txt", "a");
    if (f) { fprintf(f, "%d %s\n", percent, line); fclose(f); }
}
#endif

void client_login(Client *c, const char *username, const char *password, bool reconnect) {
    // signlink.errorname = username;
    // try {
    if (!reconnect) {
        c->login_message0 = "";
        c->login_message1 = "Connecting to server...";
        client_draw_title_screen(c);
    }
    platform_update_surface();
#ifdef BUILD_A_EMBEDDED
    if (!reconnect) {
        client_draw_progress(c, "Building world - 0%", 0);
        ba_progress_set(login_world_progress, c);
    }
#endif

#ifdef __wasm
    c->stream = clientstream_opensocket(_Custom.http_port);
#else
    c->stream = clientstream_opensocket(_Client.portoff + 43594);
#endif
#ifdef BUILD_A_EMBEDDED
    ba_progress_set(NULL, NULL);
#endif
    if (!c->stream) {
        goto login_fail;
    }
    clientstream_read_bytes(c->stream, c->in->data, 0, 8);
    c->in->pos = 0;

    c->server_seed = g8(c->in);
    int seed[] = {(int)(jrand() * 9.9999999E7), (int)(jrand() * 9.9999999E7), (int)(c->server_seed >> 32), (int)c->server_seed};

    c->out->pos = 0;
    p1(c->out, 10);
    p4(c->out, seed[0]);
    p4(c->out, seed[1]);
    p4(c->out, seed[2]);
    p4(c->out, seed[3]);
    p4(c->out, _Client.uid);
    pjstr(c->out, username);
    pjstr(c->out, password);
    rsaenc(c->out, _Client.rsa_modulus, _Client.rsa_exponent);

    c->login->pos = 0;
    if (reconnect) {
        p1(c->login, 18);
    } else {
        p1(c->login, 16);
    }

    p1(c->login, c->out->pos + 36 + 1 + 1);
    p1(c->login, _Client.clientversion);
    p1(c->login, _Client.lowmem ? 1 : 0);

    for (int i = 0; i < 9; i++) {
        p4(c->login, c->archive_checksum[i]);
    }
    pdata(c->login, c->out->data, c->out->pos, 0);

    memset(c->out->random.randrsl, 0,
           sizeof(c->out->random.randrsl));

    memset(c->random_in.randrsl, 0,
           sizeof(c->random_in.randrsl));

    memcpy(c->out->random.randrsl, seed, sizeof(seed));
    isaac_init(&c->out->random, 1);
    for (int i = 0; i < 4; i++) {
        seed[i] += 50;
    }
    memcpy(c->random_in.randrsl, seed, sizeof(seed));
    isaac_init(&c->random_in, 1);
    clientstream_write(c->stream, c->login->data, c->login->pos, 0);

    int reply = clientstream_read_byte(c->stream);
    if(getenv("CLASSIC_MIGRATION_TEST"))rs2_log("RESEARCH login reply=%d\n",reply);
    if (reply == 1) {
        rs2_sleep(2000);
        client_login(c, username, password, reconnect);
    } else if (reply == 2 || reply == 18) {
        c->rights = reply == 18;
        inputtracking_set_disabled(&_InputTracking);

        /* End the title soundtrack before accepting the new world's music. */
        platform_stop_midi();
        c->currentMidi[0] = '\0';
        c->nextMusicDelay = 0;
        c->ingame = true;
        c->out->pos = 0;
        c->in->pos = 0;
        c->packet_type = -1;
        c->last_packet_type0 = -1;
        c->last_packet_type1 = -1;
        c->last_packet_type2 = -1;
        c->packet_size = 0;
        c->idle_net_cycles = 0;
        c->system_update_timer = 0;
        c->idle_timeout = 0;
        c->hint_type = 0;
        c->menu_size = 0;
        c->menu_visible = false;
        c->shell->idle_cycles = 0;

        for (int i = 0; i < 100; i++) {
            c->message_text[i][0] = '\0';
        }

        c->obj_selected = 0;
        c->spell_selected = 0;
        c->scene_state = 0;
        c->wave_count = 0;

        c->camera_anticheat_offset_x = (int)(jrand() * 100.0) - 50;
        c->camera_anticheat_offset_z = (int)(jrand() * 110.0) - 55;
        c->camera_anticheat_angle = (int)(jrand() * 80.0) - 40;
        c->minimap_anticheat_angle = (int)(jrand() * 120.0) - 60;
        c->minimap_zoom = (int)(jrand() * 30.0) - 20;
        c->orbit_camera_yaw = (int)(jrand() * 20.0) - 10 & 0x7ff;

        c->minimap_level = -1;
        c->flag_scene_tile_x = 0;
        c->flag_scene_tile_z = 0;

        c->player_count = 0;
        c->npc_count = 0;

        for (int i = 0; i < MAX_PLAYER_COUNT; i++) {
            free(c->players[i]);
            c->players[i] = NULL;
            if (c->player_appearance_buffer[i]) {
                packet_free(c->player_appearance_buffer[i]);
                c->player_appearance_buffer[i] = NULL;
            }
        }

        for (int i = 0; i < MAX_NPC_COUNT; i++) {
            if (c->npcs[i]) {
                free(c->npcs[i]);
                c->npcs[i] = NULL;
            }
        }

        c->local_player = c->players[LOCAL_PLAYER_INDEX] = playerentity_new();
        linklist_clear(c->projectiles);
        linklist_clear(c->spotanims);
        linklist_clear(c->merged_locations);
        for (int level = 0; level < 4; level++) {
            for (int x = 0; x < 104; x++) {
                for (int z = 0; z < 104; z++) {
                    if (c->level_obj_stacks[level][x][z]) {
                        linklist_free(c->level_obj_stacks[level][x][z]);
                        c->level_obj_stacks[level][x][z] = NULL;
                    }
                }
            }
        }

        // TODO: why not linklist_clear originally?
        linklist_free(c->spawned_locations);
        c->spawned_locations = linklist_new();
        c->friend_count = 0;
        c->sticky_chat_interface_id = -1;
        c->chat_interface_id = -1;
        c->viewport_interface_id = -1;
        c->sidebar_interface_id = -1;
        c->pressed_continue_option = false;
        c->selected_tab = 3;
        c->chatback_input_open = false;
        c->menu_visible = false;
        c->show_social_input = false;
        c->modal_message[0] = '\0';
        c->in_multizone = 0;
        c->flashing_tab = -1;
        c->design_gender_male = true;

        client_validate_character_design(c);
        for (int i = 0; i < 5; i++) {
            c->design_colors[i] = 0;
        }

        _Client.oplogic1 = 0;
        _Client.oplogic2 = 0;
        _Client.oplogic3 = 0;
        _Client.oplogic4 = 0;
        _Client.oplogic5 = 0;
        _Client.oplogic6 = 0;
        _Client.oplogic7 = 0;
        _Client.oplogic8 = 0;
        _Client.oplogic9 = 0;

        client_prepare_game_screen(c);
    } else if (reply == 3) {
        c->login_message0 = "";
        c->login_message1 = "Invalid username or password.";
    } else if (reply == 4) {
        c->login_message0 = "Your account has been disabled.";
        c->login_message1 = "Please check your message-centre for details.";
    } else if (reply == 5) {
        c->login_message0 = "Your account is already logged in.";
        c->login_message1 = "Try again in 60 secs...";
    } else if (reply == 6) {
        c->login_message0 = "RuneScape has been updated!";
        c->login_message1 = "Please reload this page.";
    } else if (reply == 7) {
        c->login_message0 = "This world is full.";
        c->login_message1 = "Please use a different world.";
    } else if (reply == 8) {
        c->login_message0 = "Unable to connect.";
        c->login_message1 = "Login server offline.";
    } else if (reply == 9) {
        c->login_message0 = "Login limit exceeded.";
        c->login_message1 = "Too many connections from your address.";
    } else if (reply == 10) {
        c->login_message0 = "Unable to connect.";
        c->login_message1 = "Bad session id.";
    } else if (reply == 11) {
        c->login_message0 = "Login server rejected session.";
        c->login_message1 = "Please try again.";
    } else if (reply == 12) {
        c->login_message0 = "You need a members account to login to this world.";
        c->login_message1 = "Please subscribe, or use a different world.";
    } else if (reply == 13) {
        c->login_message0 = "Could not complete login.";
        c->login_message1 = "Please try using a different world.";
    } else if (reply == 14) {
        c->login_message0 = "The server is being updated.";
        c->login_message1 = "Please wait 1 minute and try again.";
    } else if (reply == 15) {
        c->ingame = true;
        c->out->pos = 0;
        c->in->pos = 0;
        c->packet_type = -1;
        c->last_packet_type0 = -1;
        c->last_packet_type1 = -1;
        c->last_packet_type2 = -1;
        c->packet_size = 0;
        c->idle_net_cycles = 0;
        c->system_update_timer = 0;
        c->menu_size = 0;
        c->menu_visible = false;
    } else if (reply == 16) {
        c->login_message0 = "Login attempts exceeded.";
        c->login_message1 = "Please wait 1 minute and try again.";
    } else if (reply == 17) {
        c->login_message0 = "You are standing in a members-only area.";
        c->login_message1 = "To play on this world move to a free area first";
    }
    return;
login_fail:
    c->login_message0 = "";
    c->login_message1 = "Error connecting to server.";
}

void client_prepare_game_screen(Client *c) {
    ve_release(c);
    if (c->area_chatback) {
        return;
    }

    client_unload_title(c);

    if (c->shell->draw_area) {
        pixmap_free(c->shell->draw_area);
        c->shell->draw_area = NULL;
    }
    pixmap_free(c->image_title0);
    pixmap_free(c->image_title1);
    pixmap_free(c->image_title2);
    pixmap_free(c->image_title3);
    pixmap_free(c->image_title4);
    pixmap_free(c->image_title5);
    pixmap_free(c->image_title6);
    pixmap_free(c->image_title7);
    pixmap_free(c->image_title8);
    c->image_title2 = NULL;
    c->image_title3 = NULL;
    c->image_title4 = NULL;
    c->image_title0 = NULL;
    c->image_title1 = NULL;
    c->image_title5 = NULL;
    c->image_title6 = NULL;
    c->image_title7 = NULL;
    c->image_title8 = NULL;
    c->area_chatback = pixmap_new(CHAT_WIDTH, 96);
    c->area_mapback = pixmap_new(168, 160);
    pix2d_clear();
    pix8_draw(c->image_mapback, 0, 0);
    c->area_sidebar = pixmap_new(190, 261);
    c->area_viewport = pixmap_new(GAME_VIEW_WIDTH, GAME_VIEW_HEIGHT);
    pix2d_clear();
    c->area_backbase1 = pixmap_new(CHAT_CONTROLS_WIDTH, 61);
    c->area_backbase2 = pixmap_new(288, 40);
    c->area_backhmid1 = pixmap_new(269, SIDE_TABS_HEIGHT);
    c->redraw_background = true;
}

void client_unload_title(Client *c) {
    vita_title_invalidate();
    c->flame_active = false;
    pix8_free(c->image_titlebox);
    pix8_free(c->image_titlebutton);
#ifndef DISABLE_FLAMES
    for (int i = 0; i < 12; i++) {
        pix8_free(c->image_runes[i]);
    }
    free(c->image_runes);
    free(c->flame_gradient0);
    free(c->flame_gradient1);
    free(c->flame_gradient2);
    free(c->flame_gradient);
    free(c->flame_buffer0);
    free(c->flame_buffer1);
    free(c->flame_buffer3);
    free(c->flame_buffer2);
    pix24_free(c->image_flames_left);
    pix24_free(c->image_flames_right);
#endif

    if(c->title_restored){pixmap_free(c->title_panel_scaled);pix24_free(c->title_panel_background);c->title_panel_scaled=NULL;c->title_panel_background=NULL;pixmap_free(c->title_flame_left);pixmap_free(c->title_flame_right);pixmap_free(c->title_flame_clip_left);pixmap_free(c->title_flame_clip_right);c->title_restored=false;}
    c->image_titlebox = NULL;
    c->image_titlebutton = NULL;
    for (int i = 0; i < 12; i++) {
        c->image_runes = NULL;
    }
    c->image_runes = NULL;
    c->flame_gradient = NULL;
    c->flame_gradient0 = NULL;
    c->flame_gradient1 = NULL;
    c->flame_gradient2 = NULL;
    c->flame_buffer0 = NULL;
    c->flame_buffer1 = NULL;
    c->flame_buffer3 = NULL;
    c->flame_buffer2 = NULL;
    c->image_flames_left = NULL;
    c->image_flames_right = NULL;
}

void client_validate_character_design(Client *c) {
    c->update_design_model = true;

    for (int i = 0; i < 7; i++) {
        c->designIdentikits[i] = -1;

        for (int j = 0; j < _IdkType.count; j++) {
            if (!_IdkType.instances[j]->disable && _IdkType.instances[j]->type == i + (c->design_gender_male ? 0 : 7)) {
                c->designIdentikits[i] = j;
                break;
            }
        }
    }
}

static int integration_inventory_slot(int com,int obj){if(com<0||com>=_Component.count)return -1;Component*t=_Component.instances[com];if(!t||!t->invSlotObjId)return -1;for(int i=0;i<t->width*t->height;i++)if(t->invSlotObjId[i]==obj+1)return i;return -1;}
static void migration_research_driver(Client *c) {
    if (!getenv("CLASSIC_MIGRATION_TEST")) return;
    static int login_attempted=0,game_frames=0,previous_sequence=-999,previous_primary=-999;
    if (!c->ingame && !login_attempted && _Client.loop_cycle>50) {login_attempted=1;client_login(c,c->username,c->password,false);rs2_log("RESEARCH login requested ingame=%d message=%s\n",c->ingame,c->login_message1);}
    if (!c->ingame || c->scene_state!=2 || !c->local_player) return;
    game_frames++;
    if(previous_primary!=c->local_player->pathing_entity.primarySeqId){previous_primary=c->local_player->pathing_entity.primarySeqId;rs2_log("RESEARCH primary sequence transition=%d game_frame=%d\n",previous_primary,game_frames);}
    if(previous_sequence!=c->local_player->pathing_entity.secondarySeqId){previous_sequence=c->local_player->pathing_entity.secondarySeqId;rs2_log("RESEARCH sequence transition=%d game_frame=%d\n",previous_sequence,game_frames);}
    if(getenv("TEMPLE_CONTROL_TEST")){
if(getenv("PIP_JOURNAL_INPUT_TEST")){
 static uint64_t jb=0;static int js=0;uint64_t jnow=rs2_now();if(!jb)jb=jnow;
 int target=7630,mode=getenv("PIP_JOURNAL_INPUT_MODE")?atoi(getenv("PIP_JOURNAL_INPUT_MODE")):0;
 Component*root=_Component.instances[638],*parent=_Component.instances[639],*entry=_Component.instances[target];
 if(js==0&&jnow-jb>1800){int ri=-1,ti=-1;for(int i=0;i<root->childCount;i++)if(root->childId[i]==639)ri=i;for(int i=0;i<parent->childCount;i++)if(parent->childId[i]==target)ti=i;if(ri<0||ti<0||!entry||entry->buttonType!=BUTTON_OK)abort();
 for(int i=0;i<14;i++)if(c->tab_interface_id[i]==638){c->selected_tab=i;c->redraw_sideicons=true;}
 int left=ui474_side_x()+root->childX[ri]+parent->x,top=ui474_side_y()+root->childY[ri]+parent->y,max=parent->scroll-parent->height;parent->scrollPosition=0;c->drag_cycles=1;
 for(int i=0;i<1000&&parent->scrollPosition<max;i++)handleScrollInput(c,left+parent->width+8,top+parent->height-8,parent->scroll,parent->height,true,left+parent->width,top,parent);
 if(parent->scrollPosition>max)parent->scrollPosition=max;if(parent->scrollPosition!=max)abort();int before=parent->scrollPosition;handleScrollInput(c,left+parent->width+8,top+8,parent->scroll,parent->height,true,left+parent->width,top,parent);if(parent->scrollPosition!=before-4)abort();handleScrollInput(c,left+parent->width+8,top+parent->height-8,parent->scroll,parent->height,true,left+parent->width,top,parent);if(parent->scrollPosition!=before)abort();c->drag_cycles=0;c->redraw_sidebar=true;rs2_log("PIP_JOURNAL_SCROLL mode=%d max=%d original_bounds=%d,%d,%d,%d entry=%d,%d,%d,%d\n",mode,max,left,top,parent->width,parent->height,parent->childX[ti],parent->childY[ti],entry->width,entry->height);js=1;}
 if(js==1&&jnow-jb>5500){int ri=-1,ti=-1;for(int i=0;i<root->childCount;i++)if(root->childId[i]==639)ri=i;for(int i=0;i<parent->childCount;i++)if(parent->childId[i]==target)ti=i;int x=ui474_side_x()+root->childX[ri]+parent->x+parent->childX[ti]+entry->x+entry->width/2,y=ui474_side_y()+root->childY[ri]+parent->y+parent->childY[ti]+entry->y-parent->scrollPosition+entry->height/2;
 c->menu_size=0;c->menu_visible=false;handleInterfaceInput(c,root,x,y,ui474_side_x(),ui474_side_y(),0);int found=-1;for(int i=0;i<c->menu_size;i++)if(c->menuParamC[i]==target&&c->menu_action[i]==951)found=i;if(found<0)abort();if(found!=c->menu_size-1)abort();int oldbutton=c->shell->mouse_click_button,oldx=c->shell->mouse_click_x,oldy=c->shell->mouse_click_y;c->shell->mouse_click_button=1;c->shell->mouse_click_x=x;c->shell->mouse_click_y=y;handleMouseInput(c);c->shell->mouse_click_button=oldbutton;c->shell->mouse_click_x=oldx;c->shell->mouse_click_y=oldy;rs2_log("PIP_JOURNAL_CLICK component=%d original_action=951 mode=%d path=shared_pointer_dispatch\n",target,mode);js=2;}
 if(js==2&&jnow-jb>9500){if(c->viewport_interface_id<0)abort();rs2_log("PIP_JOURNAL_OPEN actual_root=%d mode=%d\n",c->viewport_interface_id,mode);js=3;}
 if(js==3&&jnow-jb>13500){vita_dialogue_input(c,false,false,0,false);vita_dialogue_input(c,false,true,0,false);vita_dialogue_input(c,false,true,0,false);rs2_log("PIP_JOURNAL_CLOSE via=production_vita_dialogue_input held_probe=1 mode=%d\n",mode);js=4;}
 if(js==4&&jnow-jb>16500){if(c->viewport_interface_id>=0)abort();rs2_log("PIP_JOURNAL_INPUT_PASS mode=%d physical_device_test=0\n",mode);js=5;}return;}

 static uint64_t action_at=0,page_at=0,status_at=0;static int executed=-1;static unsigned long lastpage=0;uint64_t now=rs2_now();
 int sequence=0,kind=0,a=0,b=0,zarg=0,option=0,item=-1,component=3214;char line[1024]={0},wanted[512]={0};
 FILE*f=fopen("/research/scripts/TEMPLE_CONTROL.txt","r");if(f){fgets(line,sizeof(line),f);fclose(f);sscanf(line,"%d %d %d %d %d %d %d %d",&sequence,&kind,&a,&b,&zarg,&option,&item,&component);char*q=strchr(line,'|');if(q){snprintf(wanted,sizeof(wanted),"%s",q+1);wanted[strcspn(wanted,"\r\n")]=0;}}
 int px=c->local_player->pathing_entity.x/128+c->sceneBaseTileX,pz=c->local_player->pathing_entity.z/128+c->sceneBaseTileZ;
 static int frame_yaw=512;c->orbit_camera_yaw=frame_yaw;c->orbit_camera_pitch=256;vita_dialogue_input(c,false,false,0,false);
 if(now-status_at>=300){status_at=now;f=fopen("/research/scripts/TEMPLE_CLIENT_STATE.txt","w");if(f){fprintf(f,"%d %d %d %d %d %d %d\n",executed,px,pz,c->currentLevel,c->chat_interface_id,c->viewport_interface_id,bump_allocator_used());fclose(f);}}
 static uint64_t ui474_status=0;if(now-ui474_status>250){ui474_status=now;FILE*uf=fopen("/research/scripts/UI474_CLIENT_STATE.json","w");if(uf){fprintf(uf,"{\"profile474\":%d,\"tab\":%d,\"primary\":%d,\"brightness\":%d,\"music\":%d,\"sounds\":%d,\"mouse\":%d,\"energy\":%d,\"inventory\":[",ui474_enabled(),c->selected_tab,c->local_player->pathing_entity.primarySeqId,c->varps[166],c->varps[168],c->varps[169],c->varps[170],c->energy);for(int i=0;i<28;i++){Component*q=_Component.instances[3214];fprintf(uf,"%s[%d,%d]",i?",":"",q->invSlotObjId[i]-1,q->invSlotObjCount[i]);}fprintf(uf,"],\"equipment\":[");for(int i=0;i<15;i++){Component*q=_Component.instances[1688];fprintf(uf,"%s[%d,%d]",i?",":"",q->invSlotObjId[i]-1,q->invSlotObjCount[i]);}fprintf(uf,"],\"base_skills\":[");for(int i=0;i<21;i++)fprintf(uf,"%s%d",i?",":"",c->skillBaseLevel[i]);fprintf(uf,"],\"client_varps\":[");for(int i=0;i<VARPS_COUNT;i++)fprintf(uf,"%s%d",i?",":"",c->varps[i]);fprintf(uf,"],\"focus_target\":%d,\"focus_kind\":%d,\"emote_scroll\":%d}",ui474_focus_target(),ui474_focus_kind(),ui474_scroll_position());fclose(uf);}}
 static int coffin_seq=-999,coffin_frame=-999,coffin_secondary=-999;PathingEntity*ce=&c->local_player->pathing_entity;if(ce->primarySeqId!=coffin_seq||ce->primarySeqFrame!=coffin_frame||ce->secondarySeqId!=coffin_secondary){coffin_seq=ce->primarySeqId;coffin_frame=ce->primarySeqFrame;coffin_secondary=ce->secondarySeqId;FILE*cf=fopen("/research/scripts/COFFIN_PLAYER_TRACE.jsonl","a");if(cf){fprintf(cf,"{\"ms\":%llu,\"primary\":%d,\"frame\":%d,\"secondary\":%d,\"secondary_frame\":%d,\"delay\":%d,\"gender\":%d,\"x\":%d,\"z\":%d,\"plane\":%d}\n",(unsigned long long)now,ce->primarySeqId,ce->primarySeqFrame,ce->secondarySeqId,ce->secondarySeqFrame,ce->primarySeqDelay,c->local_player->gender,px,pz,c->currentLevel);fclose(cf);}}

 static int ns_type[8192], ns_primary[8192], ns_frame[8192], ns_secondary[8192], ns_secondary_frame[8192];
 for(int ni=0;ni<c->npc_count;ni++){int id=c->npc_ids[ni];NpcEntity*n=c->npcs[id];if(id<0||id>=8192||!n||!n->type)continue;int type=(int)n->type->index;if(type<1031&&type!=1028)continue;PathingEntity*a=&n->pathing_entity;if(ns_type[id]!=type||ns_primary[id]!=a->primarySeqId||ns_frame[id]!=a->primarySeqFrame||ns_secondary[id]!=a->secondarySeqId||ns_secondary_frame[id]!=a->secondarySeqFrame){ns_type[id]=type;ns_primary[id]=a->primarySeqId;ns_frame[id]=a->primarySeqFrame;ns_secondary[id]=a->secondarySeqId;ns_secondary_frame[id]=a->secondarySeqFrame;FILE*f=fopen("/research/scripts/NATURE_NPC_FRAME_TRACE.jsonl","a");if(f){fprintf(f,"{\"ms\":%llu,\"slot\":%d,\"type\":%d,\"primary\":%d,\"frame\":%d,\"secondary\":%d,\"secondary_frame\":%d,\"x\":%d,\"z\":%d,\"plane\":%d}\n",(unsigned long long)now,id,type,a->primarySeqId,a->primarySeqFrame,a->secondarySeqId,a->secondarySeqFrame,a->x/128+c->sceneBaseTileX,a->z/128+c->sceneBaseTileZ,c->currentLevel);fclose(f);}}}
 {FILE*lf=fopen("/research/scripts/LAYOUTCORE_STATE.json","w");if(lf){fprintf(lf,"{\"generation\":%u,\"chat_scroll\":%d,\"chat_height\":%d,\"roles\":[",lc_game_generation(),c->chat_scroll_offset,c->chat_scroll_height);for(int i=0;i<LC_ROLES;i++){const LcGeometry*g=lc_game_geometry(i);fprintf(lf,"%s{\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d,\"clip\":[%d,%d,%d,%d]}",i?",":"",g->rect.x,g->rect.y,g->rect.w,g->rect.h,g->clip.x,g->clip.y,g->clip.w,g->clip.h);}fprintf(lf,"]}");fclose(lf);}}
 Component*r=c->chat_interface_id>=0?_Component.instances[c->chat_interface_id]:NULL;
 if(r){unsigned long hash=5381;hash=hash*33+c->vita_dialogue_generation;for(int j=0;j<r->childCount;j++){Component*t=_Component.instances[r->childId[j]];if(t&&t->text)for(const unsigned char*q=(const unsigned char*)t->text;*q;q++)hash=hash*33+*q;}
  if(hash!=lastpage){lastpage=hash;page_at=now;rs2_log("TEMPLE_PAGE root=%d\n",r->id);for(int j=0;j<r->childCount;j++){Component*t=_Component.instances[r->childId[j]];if(t&&t->text&&t->text[0])rs2_log("TEMPLE_TEXT id=%d button=%d text=%s\n",t->id,t->buttonType,t->text);}}
  if(now-page_at>=1300&&strcmp(wanted,"!hold")){int chosen=-1;bool choices=false;for(int j=0;j<r->childCount;j++){Component*t=_Component.instances[r->childId[j]];if(t&&t->buttonType==BUTTON_OK&&t->text&&t->text[0]){choices=true;if((!wanted[0]&&chosen<0)||!strcmp(wanted,"*first")||!strcmp(wanted,t->text)){chosen=t->id;break;}}}
   if(choices&&chosen>=0){vita_dialogue_input(c,false,false,0,false);for(int n=0;n<12&&c->vita_dialogue_selected!=chosen;n++)vita_dialogue_input(c,false,false,1,false);if(c->vita_dialogue_selected!=chosen)abort();vita_dialogue_input(c,false,true,0,false);vita_dialogue_input(c,false,true,0,false);page_at=now;rs2_log("TEMPLE_CHOICE component=%d via=Dpad_Cross seq=%d\n",chosen,sequence);}
   if(!choices){for(int j=0;j<r->childCount;j++){Component*t=_Component.instances[r->childId[j]];if(t&&t->buttonType==BUTTON_CONTINUE){vita_dialogue_input(c,false,true,0,false);vita_dialogue_input(c,false,true,0,false);page_at=now;rs2_log("TEMPLE_CONTINUE seq=%d duplicate-held-Cross-probe=1\n",sequence);break;}}}
  }
 }else lastpage=0;
 if(kind&&sequence!=executed&&now-action_at>=1500&&(!r||kind==7||kind==12)){bool sent=false;
  if(kind==1){int x=a-c->sceneBaseTileX,z=b-c->sceneBaseTileZ;if(x>=0&&x<104&&z>=0&&z<104){c->shell->action_key[5]=option?1:0;sent=client_try_move(c,c->local_player->pathing_entity.pathTileX[0],c->local_player->pathing_entity.pathTileZ[0],x,z,0,0,0,0,0,0,false);c->shell->action_key[5]=0;}}
  else if(kind==2||kind==5){int best=-1,distance=0x7fffffff;for(int i=0;i<c->npc_count;i++){int id=c->npc_ids[i];NpcEntity*n=c->npcs[id];if(n&&n->type&&n->type->index==a){int dx=n->pathing_entity.pathTileX[0]-c->local_player->pathing_entity.pathTileX[0],dz=n->pathing_entity.pathTileZ[0]-c->local_player->pathing_entity.pathTileZ[0];int d=dx*dx+dz*dz;if(d<distance){best=id;distance=d;}}}
   if(best>=0){if(kind==5){int slot=integration_inventory_slot(component,item);if(slot>=0){c->menu_action[0]=188;c->menuParamA[0]=item;c->menuParamB[0]=slot;c->menuParamC[0]=component;useMenuOption(c,0);c->menu_action[0]=900;c->menuParamA[0]=best;useMenuOption(c,0);sent=true;}}
    else {int actions[]={728,542,6,963,245};if(option>=0&&option<5&&c->npcs[best]->type->op&&c->npcs[best]->type->op[option]){c->menu_action[0]=actions[option];c->menuParamA[0]=best;useMenuOption(c,0);sent=true;}}}}
  else if(kind==3||kind==4){int x=b-c->sceneBaseTileX,z=zarg-c->sceneBaseTileZ;if(x>=0&&x<104&&z>=0&&z<104){int bits[]={world3d_get_locbitset(c->scene,c->currentLevel,x,z),world3d_get_wallbitset(c->scene,c->currentLevel,x,z),world3d_get_walldecorationbitset(c->scene,c->currentLevel,x,z),world3d_get_grounddecorationbitset(c->scene,c->currentLevel,x,z)};int bit=0;for(int i=0;i<4;i++)if(bits[i]&&((bits[i]>>14)&0x7fff)==a){bit=bits[i];break;}
   if(bit){if(kind==4){int slot=integration_inventory_slot(component,item);if(slot>=0){c->menu_action[0]=188;c->menuParamA[0]=item;c->menuParamB[0]=slot;c->menuParamC[0]=component;useMenuOption(c,0);c->menu_action[0]=450;sent=true;}}
    else{int actions[]={285,504,364,581,1501};LocType*l=loctype_get(a);if(option>=0&&option<5&&l&&l->op&&l->op[option]){c->menu_action[0]=actions[option];sent=true;}}
    if(sent){c->menuParamA[0]=bit;c->menuParamB[0]=x;c->menuParamC[0]=z;useMenuOption(c,0);}}}}
  else if(kind==6){int x=b-c->sceneBaseTileX,z=zarg-c->sceneBaseTileZ;if(x>=0&&x<104&&z>=0&&z<104){c->menu_action[0]=99;c->menuParamA[0]=a;c->menuParamB[0]=x;c->menuParamC[0]=z;useMenuOption(c,0);sent=true;}}
  else if(kind==7){c->menu_action[0]=947;useMenuOption(c,0);sent=true;}
  else if(kind==8){c->menu_action[0]=465;c->menuParamC[0]=a;useMenuOption(c,0);sent=true;}
  else if(kind==16){sent=lc_game_parent(a,b);if(sent){c->redraw_background=c->redraw_sidebar=c->redraw_sideicons=c->redraw_chatback=c->redraw_privacy_settings=true;rs2_log("LAYOUTCORE_PARENT seq=%d width=%d height=%d generation=%u\n",sequence,a,b,lc_game_generation());}}
  else if(kind==18){frame_yaw=a&2047;c->orbit_camera_yaw=frame_yaw;sent=true;rs2_log("FRAME_CAMERA seq=%d yaw=%d\n",sequence,c->orbit_camera_yaw);}
  else if(kind==17){c->shell->mouse_x=a;c->shell->mouse_y=b;c->drag_cycles=zarg>0?zarg:1;int old=c->chat_scroll_offset;c->chat_interface->scrollPosition=c->chat_scroll_height-old-77;client_handle_scroll_input(c,a-(ve_enabled()?ve_chat_text_x():lc_x(LC_CHAT,22)),b-(ve_enabled()?ve_chat_text_y():lc_y(LC_CHAT,UI_BOTTOM(375))),c->chat_scroll_height,77,false,lc_chat_text_right(CHAT_TEXT_RIGHT),0,c->chat_interface);int next=c->chat_scroll_height-c->chat_interface->scrollPosition-77;if(next<0)next=0;if(next>c->chat_scroll_height-77)next=c->chat_scroll_height-77;c->chat_scroll_offset=next;c->redraw_chatback=true;c->drag_cycles=0;sent=true;rs2_log("LAYOUTCORE_CHAT_SCROLL seq=%d old=%d new=%d height=%d x=%d y=%d\n",sequence,old,next,c->chat_scroll_height,a,b);}
  else if(kind==14){c->shell->mouse_x=a;c->shell->mouse_y=b;c->shell->mouse_click_x=a;c->shell->mouse_click_y=b;c->shell->mouse_click_button=option?option:1;client_handle_input(c);handleMouseInput(c);handleTabInput(c);handleMinimapInput(c);handleChatSettingsInput(c);c->shell->mouse_click_button=0;sent=true;rs2_log("UI474_POINTER x=%d y=%d button=%d tab=%d profile474=%d\n",a,b,option?option:1,c->selected_tab,ui474_enabled());}
  else if(kind==15){int startpos=c->out->pos;vita_dialogue_input(c,false,false,0,false);bool first=false,held=false;for(int n=0;n<(zarg>0?zarg:1);n++)first=vita_dialogue_input(c,false,b!=0,a,false);if(b>1)held=vita_dialogue_input(c,false,true,0,false);sent=true;rs2_log("UI474_CONTROLLER seq=%d direction=%d confirms=%d repeat=%d tab=%d profile474=%d bytes=%d first_consumed=%d held_consumed=%d focus_target=%d focus_kind=%d\n",sequence,a,b,zarg>0?zarg:1,c->selected_tab,ui474_enabled(),c->out->pos-startpos,first,held,ui474_focus_target(),ui474_focus_kind());}
  else if(kind==12){vita_dialogue_input(c,false,false,0,false);vita_dialogue_input(c,false,true,0,false);sent=true;}
  else if(kind==11){int slot=integration_inventory_slot(component,a);int actions[]={405,38,422,478,347};ObjType*o=objtype_get(a);if(slot>=0&&option>=0&&option<5&&o&&o->iop&&o->iop[option]){c->menu_action[0]=actions[option];c->menuParamA[0]=a;c->menuParamB[0]=slot;c->menuParamC[0]=component;useMenuOption(c,0);sent=true;}}
  else if(kind==9||kind==10){int slot=integration_inventory_slot(component,a);if(slot>=0&&option>=0&&option<5){int bank_actions[]={602,596,22,892,415};if(kind==10&&(!_Component.instances[component]->iops||!_Component.instances[component]->iops[option]))abort();c->menu_action[0]=kind==9?38:bank_actions[option];c->menuParamA[0]=a;c->menuParamB[0]=slot;c->menuParamC[0]=component;useMenuOption(c,0);sent=true;}}
  action_at=now;if(sent){executed=sequence;rs2_log("TEMPLE_EXEC seq=%d kind=%d target=%d args=%d,%d option=%d item=%d component=%d pos=%d,%d,%d\n",sequence,kind,a,b,zarg,option,item,component,px,pz,c->currentLevel);}else rs2_log("TEMPLE_WAIT seq=%d kind=%d target=%d pos=%d,%d,%d\n",sequence,kind,a,px,pz,c->currentLevel);
 }
 return;}
if(getenv("TZHAAR_RECONNECT")){static uint64_t last=0;uint64_t now=rs2_now();if(now-last>2000){last=now;int px=c->local_player->pathing_entity.x/128+c->sceneBaseTileX,pz=c->local_player->pathing_entity.z/128+c->sceneBaseTileZ;rs2_log("TZHAAR_RECONNECT pos=%d,%d,%d pool=%d/%d\n",px,pz,c->currentLevel,bump_allocator_used(),bump_allocator_capacity());}return;}
if(getenv("TZHAAR_TEST")){static int phase=0;static uint64_t last=0;uint64_t now=rs2_now();int px=c->local_player->pathing_entity.x/128+c->sceneBaseTileX,pz=c->local_player->pathing_entity.z/128+c->sceneBaseTileZ;c->orbit_camera_yaw=512;c->orbit_camera_pitch=256;
if(now-last<2200)return;last=now;rs2_log("TZHAAR_STATE phase=%d pos=%d,%d,%d base=%d,%d pool=%d/%d\n",phase,px,pz,c->currentLevel,c->sceneBaseTileX,c->sceneBaseTileZ,bump_allocator_used(),bump_allocator_capacity());
int ox=-1,oz=-1,expect=-1;
if(phase==0){ox=2856;oz=3168;expect=492;}
else if(phase==1&&pz>9000){ox=2863;oz=9571;expect=3405;}
else if(phase==2&&pz<6000){bool wall=client_try_move(c,c->local_player->pathing_entity.pathTileX[0],c->local_player->pathing_entity.pathTileZ[0],2430-c->sceneBaseTileX,5165-c->sceneBaseTileZ,0,0,0,0,0,0,false);rs2_log("TZHAAR_BLOCKED_TILE rejected=%d\n",!wall);if(wall)abort();phase++;}
else if(phase==3||phase==4||phase==5){const int route[3][2]={{2428,5174},{2460,5158},{2480,5175}};int tx=route[phase-3][0],tz=route[phase-3][1];if(px==tx&&pz==tz){rs2_log("TZHAAR_WAYPOINT_REACHED phase=%d x=%d z=%d\n",phase,px,pz);phase++;}else{bool ok=client_try_move(c,c->local_player->pathing_entity.pathTileX[0],c->local_player->pathing_entity.pathTileZ[0],tx-c->sceneBaseTileX,tz-c->sceneBaseTileZ,0,0,0,0,0,0,false);rs2_log("TZHAAR_WALK phase=%d target=%d,%d success=%d\n",phase,tx,tz,ok);}}
else if(phase==6){ox=2479;oz=5176;expect=3406;}
else if(phase==7&&pz>9000){ox=2856;oz=9569;expect=1764;}
else if(phase==8&&pz<4000){rs2_log("TZHAAR_LOOP_COMPLETE pos=%d,%d,%d\n",px,pz,c->currentLevel);phase++;}
if(expect>=0){int x=ox-c->sceneBaseTileX,z=oz-c->sceneBaseTileZ;int bit=world3d_get_locbitset(c->scene,c->currentLevel,x,z);if(!bit)bit=world3d_get_wallbitset(c->scene,c->currentLevel,x,z);int found=(bit>>14)&0x7fff;rs2_log("TZHAAR_OBJECT expected=%d actual=%d at=%d,%d\n",expect,found,ox,oz);if(found==expect){c->menu_action[0]=285;c->menuParamA[0]=bit;c->menuParamB[0]=x;c->menuParamC[0]=z;useMenuOption(c,0);phase++;}}
return;}
if(getenv("BANK_NETWORK_TEST")){static int phase=0;static uint64_t last=0;uint64_t now=rs2_now();if(now-last<1400)return;last=now;int cape=2920,hood=2922,slot=-1;
#define BANK_ACTION(act,ob,sl,co) do{c->menu_action[0]=(act);c->menuParamA[0]=(ob);c->menuParamB[0]=(sl);c->menuParamC[0]=(co);useMenuOption(c,0);}while(0)
if(phase==0&&(slot=integration_inventory_slot(1688,cape))>=0){c->selected_tab=4;c->redraw_sidebar=true;BANK_ACTION(602,cape,slot,1688);phase++;rs2_log("BANK_NETWORK removed cape via original equipment action\n");}
else if(phase==1&&(slot=integration_inventory_slot(1688,hood))>=0){BANK_ACTION(602,hood,slot,1688);phase++;rs2_log("BANK_NETWORK removed hood\n");}
else if(phase==2&&integration_inventory_slot(3214,cape)>=0&&integration_inventory_slot(3214,hood)>=0){int x=atoi(getenv("BANK_X"))-c->sceneBaseTileX,z=atoi(getenv("BANK_Z"))-c->sceneBaseTileZ;int bit=world3d_get_locbitset(c->scene,c->currentLevel,x,z);if(((bit>>14)&0x7fff)!=2213)abort();BANK_ACTION(504,bit,x,z);phase++;rs2_log("BANK_NETWORK original OPLOC2 bankbooth=2213\n");}
else if(phase==3&&c->viewport_interface_id==5292&&(slot=integration_inventory_slot(2006,cape))>=0){BANK_ACTION(602,cape,slot,2006);phase++;rs2_log("BANK_NETWORK deposit cape\n");}
else if(phase==4&&integration_inventory_slot(5382,cape)>=0&&(slot=integration_inventory_slot(2006,hood))>=0){BANK_ACTION(602,hood,slot,2006);phase++;rs2_log("BANK_NETWORK deposit hood\n");}
else if(phase==5&&integration_inventory_slot(5382,hood)>=0&&(slot=integration_inventory_slot(5382,cape))>=0){BANK_ACTION(602,cape,slot,5382);phase++;rs2_log("BANK_NETWORK withdraw cape\n");}
else if(phase==6&&integration_inventory_slot(2006,cape)>=0&&integration_inventory_slot(5382,cape)<0&&(slot=integration_inventory_slot(5382,hood))>=0){BANK_ACTION(602,hood,slot,5382);phase++;rs2_log("BANK_NETWORK withdraw hood\n");}
else if(phase==7&&integration_inventory_slot(2006,hood)>=0&&integration_inventory_slot(5382,hood)<0){BANK_ACTION(947,0,0,0);phase++;rs2_log("BANK_NETWORK close through original action\n");}
else if(phase==8&&c->viewport_interface_id<0&&(slot=integration_inventory_slot(3214,cape))>=0){BANK_ACTION(38,cape,slot,3214);phase++;}
else if(phase==9&&c->local_player->appearances[1]==3432&&(slot=integration_inventory_slot(3214,hood))>=0){BANK_ACTION(38,hood,slot,3214);phase++;}
else if(phase==10&&c->local_player->appearances[0]==3434){phase++;rs2_log("BANK_NETWORK_COMPLETE cape=%d hood=%d\n",c->local_player->appearances[1],c->local_player->appearances[0]);}
#undef BANK_ACTION
return;}
if(getenv("WIDTH_REVIEW")){static uint64_t changed=0;static unsigned long prev=0;static int seq=0,acted=0,logged=0;uint64_t now=rs2_now();unsigned long hash=5381;Component*r=c->chat_interface_id>=0?_Component.instances[c->chat_interface_id]:NULL;if(r){hash=hash*33+c->chat_interface_id;for(int j=0;j<r->childCount;j++){Component*t=_Component.instances[r->childId[j]];for(const unsigned char*q=(const unsigned char*)t->text;*q;q++)hash=hash*33+*q;}}else hash=0;
if(r)hash=hash*33+c->vita_dialogue_generation;
if(hash&&hash!=prev){prev=hash;changed=now;acted=0;logged=0;}if(r&&!logged&&now-changed>700){logged=1;seq++;vita_dialogue_input(c,false,false,0,false);rs2_log("WIDTH_PAGE seq=%d root=%d width=%d\n",seq,c->chat_interface_id,r->width);for(int j=0;j<r->childCount;j++){Component*t=_Component.instances[r->childId[j]];rs2_log("WIDTH_COMPONENT seq=%d id=%d type=%d button=%d x=%d y=%d width=%d height=%d text=%s\n",seq,t->id,t->type,t->buttonType,r->childX[j],r->childY[j],t->width,t->height,t->text);}}
if(r&&!acted&&now-changed>2600){int count=0,first=-1;for(int j=0;j<r->childCount;j++){Component*t=_Component.instances[r->childId[j]];if(t->type==4&&t->buttonType==1){if(first<0)first=t->id;count++;}}if(count){for(int j=0;j<count;j++)vita_dialogue_input(c,false,false,1,false);if(c->vita_dialogue_selected!=first)abort();for(int j=0;j<count;j++)vita_dialogue_input(c,false,false,-1,false);if(c->vita_dialogue_selected!=first)abort();rs2_log("WIDTH_NAV seq=%d count=%d wrapped_both_directions=1\n",seq,count);}vita_dialogue_input(c,false,true,0,false);vita_dialogue_input(c,false,true,0,false);rs2_log("WIDTH_CONFIRM seq=%d duplicate_continue_guard=%d\n",seq,c->pressed_continue_option);acted=1;}if(!r)prev=0;c->orbit_camera_yaw=512;c->orbit_camera_pitch=256;return;}
if(getenv("FISH_PLACEMENT_ONLY")){c->orbit_camera_yaw=512;c->orbit_camera_pitch=256;return;}
if(getenv("FISH_GUILD_TEST")){static uint64_t began=0,last=0;static int clicked=0;uint64_t now=rs2_now();if(!began)began=now;int sec=(now-began)/1000;c->orbit_camera_yaw=512;c->orbit_camera_pitch=256;if(!clicked&&sec>=3){int x=2611-c->sceneBaseTileX,z=3394-c->sceneBaseTileZ;int bit=world3d_get_wallbitset(c->scene,c->currentLevel,x,z);if(!bit)bit=world3d_get_locbitset(c->scene,c->currentLevel,x,z);if(((bit>>14)&0x7fff)!=2025)abort();c->menu_action[0]=285;c->menuParamA[0]=bit;c->menuParamB[0]=x;c->menuParamC[0]=z;useMenuOption(c,0);clicked=1;rs2_log("FISH_GUILD original OPLOC1 type=%d at=%d,%d\n",2025,2611,3394);}if(c->chat_interface_id>=0&&now-last>1600){last=now;Component*root=_Component.instances[c->chat_interface_id];for(int j=0;root&&j<root->childCount;j++){Component*t=_Component.instances[root->childId[j]];if(t&&t->text[0])rs2_log("FISH_GUILD TEXT %s\n",t->text);if(t&&t->buttonType==BUTTON_CONTINUE){vita_dialogue_input(c,false,true,0,false);break;}}}if(game_frames%100==0)rs2_log("FISH_GUILD sec=%d pos=%d,%d\n",sec,c->local_player->pathing_entity.x/128+c->sceneBaseTileX,c->local_player->pathing_entity.z/128+c->sceneBaseTileZ);return;}
if(getenv("HERBLORE_TEST")){if(getenv("RELOGIN_ONLY")){if(game_frames%100==0){int coins=0;Component*v=_Component.instances[3214];for(int i=0;i<v->width*v->height;i++)if(v->invSlotObjId[i]==996)coins+=v->invSlotObjCount[i];rs2_log("HERB_RELOGIN cape=%d hood=%d coins=%d\n",c->local_player->appearances[1],c->local_player->appearances[0],coins);}return;}
 static uint64_t began=0,last=0;static int phase=0,done=0;uint64_t now=rs2_now();if(!began)began=now;int sec=(now-began)/1000;
 c->orbit_camera_yaw=512;c->orbit_camera_pitch=256;
 if(!phase&&sec>=2){for(int i=0;i<c->npc_count;i++){int id=c->npc_ids[i];NpcEntity*n=c->npcs[id];if(n&&n->type&&n->type->index==455){c->menu_action[0]=728;c->menuParamA[0]=id;useMenuOption(c,0);phase=1;rs2_log("HERB normal Talk-to target=455 sec=%d\n",sec);break;}}}
 if(phase&&c->chat_interface_id>=0&&now-last>=1600){last=now;Component*root=_Component.instances[c->chat_interface_id];const char*wanted=phase==1?(getenv("CRAFT_ORIGINAL")?"Talk to Kaqemeex.":"I'd like to buy a Herblore cape."):(getenv("RANGE_DECLINE")?"No thank you.":"Buy the cape and hood for 99,000 coins.");if(getenv("CRAFT_ORIGINAL")&&phase>=2)wanted="Very well, thank you.";bool acted=false;if(getenv("HERB_PREVIEW")){for(int j=0;root&&j<root->childCount;j++){Component*t=_Component.instances[root->childId[j]];if(t&&((phase==1&&!strcmp(t->text,"I'd like to buy a Herblore cape.")&&sec<8)||(phase>=2&&!strcmp(t->text,"Buy the cape and hood for 99,000 coins.")))){rs2_log("HERB_PREVIEW_READY stage=%s\n",phase==1?"initial":"choices");return;}}}
 for(int j=0;root&&j<root->childCount;j++){Component*t=_Component.instances[root->childId[j]];if(t&&t->text[0]&&!strcmp(t->text,wanted)){vita_dialogue_input(c,false,false,0,false);for(int n=0;n<5&&c->vita_dialogue_selected!=t->id;n++)vita_dialogue_input(c,false,false,1,false);if(c->vita_dialogue_selected!=t->id)abort();vita_dialogue_input(c,false,true,0,false);if(getenv("HERBLORE_REPLAY_PROBE")&&strstr(wanted,"Buy the cape")){for(int k=0;k<5;k++)client_vita_dialogue_action(c,t->id,951);rs2_log("HERB_NETWORK_REPLAY sent5 stale purchase buttons\n");}rs2_log("HERB D-pad Cross phase=%d text=%s colour=%06x\n",phase,wanted,vita_interface_text_colour(c,t->id,t->colour));phase++;acted=true;break;}}
 if(!acted&&getenv("HERB_PREVIEW")){for(int j=0;root&&j<root->childCount;j++){Component*t=_Component.instances[root->childId[j]];if(!t)continue;const char*stage=NULL;if(strstr(t->text,"You have mastered Herblore")&&sec<18)stage="eligible";else if(strstr(t->text,"mastered another skill")&&sec<28)stage="trim";else if(strstr(t->text,"When you reach level 99 Herblore"))stage="lowlevel";if(stage){rs2_log("HERB_PREVIEW_READY stage=%s\n",stage);return;}}}
if(!acted)for(int j=0;root&&j<root->childCount;j++){Component*t=_Component.instances[root->childId[j]];if(t&&t->text[0])rs2_log("HERB TEXT phase=%d %s\n",phase,t->text);if(t&&t->buttonType==BUTTON_CONTINUE){vita_dialogue_input(c,false,true,0,false);rs2_log("HERB Cross continue sec=%d\n",sec);break;}}
 }
 if(phase>=2&&c->chat_interface_id<0&&!done&&sec>16){Component*inv=_Component.instances[3214];int cape=getenv("MASTER_TRIM")?2918:2917;for(int slot=0;slot<inv->width*inv->height;slot++)if(inv->invSlotObjId[slot]==cape+1){c->menu_action[0]=38;c->menuParamA[0]=cape;c->menuParamB[0]=slot;c->menuParamC[0]=3214;useMenuOption(c,0);rs2_log("HERB normal Wear cape=%d\n",cape);break;}done=1;}
 if(done==1&&sec>20){Component*inv=_Component.instances[3214];for(int slot=0;slot<inv->width*inv->height;slot++)if(inv->invSlotObjId[slot]==2920){c->menu_action[0]=38;c->menuParamA[0]=2919;c->menuParamB[0]=slot;c->menuParamC[0]=3214;useMenuOption(c,0);rs2_log("HERB normal Wear hood=2919\n");break;}done=2;}
 if(game_frames%100==0)rs2_log("HERB sec=%d phase=%d cape=%d head=%d latest=%s\n",sec,phase,c->local_player->appearances[1],c->local_player->appearances[0],c->message_text[0]);return;
}
if(getenv("RUNECRAFTING_TEST")){if(getenv("RELOGIN_ONLY")){if(game_frames%100==0){int coins=0;Component*v=_Component.instances[3214];for(int i=0;i<v->width*v->height;i++)if(v->invSlotObjId[i]==996)coins+=v->invSlotObjCount[i];rs2_log("RUNE_RELOGIN cape=%d hood=%d coins=%d\n",c->local_player->appearances[1],c->local_player->appearances[0],coins);}return;}
 if(getenv("CRAFT_ORIGINAL")&&c->viewport_interface_id==3824){static int shoplogged=0;if(!shoplogged){rs2_log("RUNE_ORIGINAL_SHOP_OPEN id=3824\n");shoplogged=1;}}
if(getenv("RUNE_ESSENCE")){static int entered=0,portalclicked=0;int px=c->local_player->pathing_entity.x/128+c->sceneBaseTileX,pz=c->local_player->pathing_entity.z/128+c->sceneBaseTileZ;if(pz>4500){entered=1;if(!portalclicked)for(int x=0;x<104;x++)for(int z=0;z<104;z++){int bit=world3d_get_locbitset(c->scene,c->currentLevel,x,z);if(bit&&!portalclicked&&((bit>>14)&0x7fff)==2492){c->menu_action[0]=285;c->menuParamA[0]=bit;c->menuParamB[0]=x;c->menuParamC[0]=z;useMenuOption(c,0);rs2_log("RUNE_ESSENCE normal portal action type=2492 at=%d,%d\n",x+c->sceneBaseTileX,z+c->sceneBaseTileZ);portalclicked=1;}}return;}if(entered&&pz<4000){rs2_log("RUNE_ESSENCE_LOOP_COMPLETE pos=%d,%d,%d\n",px,pz,c->currentLevel);return;}}
static uint64_t began=0,last=0;static int phase=0,done=0;uint64_t now=rs2_now();if(!began)began=now;int sec=(now-began)/1000;
 c->orbit_camera_yaw=512;c->orbit_camera_pitch=256;
 if(!phase&&sec>=2){for(int i=0;i<c->npc_count;i++){int id=c->npc_ids[i];NpcEntity*n=c->npcs[id];if(n&&n->type&&n->type->index==553){c->menu_action[0]=728;c->menuParamA[0]=id;useMenuOption(c,0);phase=1;rs2_log("RUNE normal Talk-to target=553 sec=%d\n",sec);break;}}}
 if(phase&&c->chat_interface_id>=0&&now-last>=1600){last=now;Component*root=_Component.instances[c->chat_interface_id];const char*wanted=phase==1?(getenv("CRAFT_ORIGINAL")?"Talk to Aubury.":"I'd like to buy a Runecrafting cape."):(getenv("RANGE_DECLINE")?"No thank you.":"Buy the cape and hood for 99,000 coins.");if(getenv("CRAFT_ORIGINAL")&&phase>=2)wanted=getenv("RUNE_ESSENCE")?"Can you teleport me to the Rune Essence?":"Yes please!";bool acted=false;if(getenv("RUNE_PREVIEW")){for(int j=0;root&&j<root->childCount;j++){Component*t=_Component.instances[root->childId[j]];if(t&&((phase==1&&!strcmp(t->text,"I'd like to buy a Runecrafting cape.")&&sec<8)||(phase>=2&&!strcmp(t->text,"Buy the cape and hood for 99,000 coins.")))){rs2_log("RUNE_PREVIEW_READY stage=%s\n",phase==1?"initial":"choices");return;}}}
 for(int j=0;root&&j<root->childCount;j++){Component*t=_Component.instances[root->childId[j]];if(t&&t->buttonType==BUTTON_OK&&t->text[0]&&!strcmp(t->text,wanted)){vita_dialogue_input(c,false,false,0,false);for(int n=0;n<5&&c->vita_dialogue_selected!=t->id;n++)vita_dialogue_input(c,false,false,1,false);if(c->vita_dialogue_selected!=t->id)abort();vita_dialogue_input(c,false,true,0,false);if(getenv("RUNECRAFTING_REPLAY_PROBE")&&strstr(wanted,"Buy the cape")){for(int k=0;k<5;k++)client_vita_dialogue_action(c,t->id,951);rs2_log("RUNE_NETWORK_REPLAY sent5 stale purchase buttons\n");}rs2_log("RUNE D-pad Cross phase=%d text=%s colour=%06x\n",phase,wanted,vita_interface_text_colour(c,t->id,t->colour));phase++;acted=true;break;}}
 if(!acted&&getenv("RUNE_PREVIEW")){for(int j=0;root&&j<root->childCount;j++){Component*t=_Component.instances[root->childId[j]];if(!t)continue;const char*stage=NULL;if(strstr(t->text,"You have mastered Runecrafting")&&sec<18)stage="eligible";else if(strstr(t->text,"mastered another skill")&&sec<28)stage="trim";else if(strstr(t->text,"When you reach level 99 Runecrafting"))stage="lowlevel";if(stage){rs2_log("RUNE_PREVIEW_READY stage=%s\n",stage);return;}}}
if(!acted)for(int j=0;root&&j<root->childCount;j++){Component*t=_Component.instances[root->childId[j]];if(t&&t->text[0])rs2_log("RUNE TEXT phase=%d %s\n",phase,t->text);if(t&&t->buttonType==BUTTON_CONTINUE){vita_dialogue_input(c,false,true,0,false);rs2_log("RUNE Cross continue sec=%d\n",sec);break;}}
 }
 if(phase>=2&&c->chat_interface_id<0&&!done&&sec>16){Component*inv=_Component.instances[3214];int cape=getenv("MASTER_TRIM")?2921:2920;for(int slot=0;slot<inv->width*inv->height;slot++)if(inv->invSlotObjId[slot]==cape+1){c->menu_action[0]=38;c->menuParamA[0]=cape;c->menuParamB[0]=slot;c->menuParamC[0]=3214;useMenuOption(c,0);rs2_log("RUNE normal Wear cape=%d\n",cape);break;}done=1;}
 if(done==1&&sec>20){Component*inv=_Component.instances[3214];for(int slot=0;slot<inv->width*inv->height;slot++)if(inv->invSlotObjId[slot]==2923){c->menu_action[0]=38;c->menuParamA[0]=2922;c->menuParamB[0]=slot;c->menuParamC[0]=3214;useMenuOption(c,0);rs2_log("RUNE normal Wear hood=2922\n");break;}done=2;}
 if(game_frames%100==0)rs2_log("RUNE sec=%d phase=%d cape=%d head=%d latest=%s\n",sec,phase,c->local_player->appearances[1],c->local_player->appearances[0],c->message_text[0]);return;
}
if(getenv("HITPOINTS_TEST")){if(getenv("HP_DIRECT_HEAL")){static uint64_t start=0;static int sent=0;uint64_t now=rs2_now();if(!start)start=now;if(!sent&&now-start>2000){for(int i=0;i<c->npc_count;i++){int id=c->npc_ids[i];NpcEntity*n=c->npcs[id];if(n&&n->type&&n->type->index==961){c->menu_size=0;addNpcOptions(c,n->type,id,n->pathing_entity.pathTileX[0],n->pathing_entity.pathTileZ[0]);for(int j=0;j<c->menu_size;j++)if(!strncmp(c->menu_option[j],"Heal ",5)){rs2_log("HP_DIRECT_HEAL normal context action=%d\n",c->menu_action[j]);useMenuOption(c,j);sent=1;break;}break;}}}return;}
if(getenv("RELOGIN_ONLY")){if(game_frames%100==0){int coins=0;Component*v=_Component.instances[3214];for(int i=0;i<v->width*v->height;i++)if(v->invSlotObjId[i]==996)coins+=v->invSlotObjCount[i];rs2_log("RUNE_RELOGIN cape=%d hood=%d coins=%d\n",c->local_player->appearances[1],c->local_player->appearances[0],coins);}return;}
 static uint64_t began=0,last=0;static int phase=0,done=0;uint64_t now=rs2_now();if(!began)began=now;int sec=(now-began)/1000;
 c->orbit_camera_yaw=512;c->orbit_camera_pitch=256;
 if(!phase&&sec>=2){for(int i=0;i<c->npc_count;i++){int id=c->npc_ids[i];NpcEntity*n=c->npcs[id];if(n&&n->type&&n->type->index==961){c->menu_action[0]=728;c->menuParamA[0]=id;useMenuOption(c,0);phase=1;rs2_log("RUNE normal Talk-to target=961 sec=%d\n",sec);break;}}}
 if(phase&&c->chat_interface_id>=0&&now-last>=1600){last=now;Component*root=_Component.instances[c->chat_interface_id];const char*wanted=phase==1?(getenv("CRAFT_ORIGINAL")?"Talk to Tafani.":"I'd like to buy a Hitpoints cape."):(getenv("RANGE_DECLINE")?"No thank you.":"Buy the cape and hood for 99,000 coins.");if(getenv("CRAFT_ORIGINAL")&&phase>=2)wanted="Can you heal me?";bool acted=false;if(getenv("RUNE_PREVIEW")){for(int j=0;root&&j<root->childCount;j++){Component*t=_Component.instances[root->childId[j]];if(t&&((phase==1&&!strcmp(t->text,"I'd like to buy a Hitpoints cape.")&&sec<8)||(phase>=2&&!strcmp(t->text,"Buy the cape and hood for 99,000 coins.")))){rs2_log("RUNE_PREVIEW_READY stage=%s\n",phase==1?"initial":"choices");return;}}}
 for(int j=0;root&&j<root->childCount;j++){Component*t=_Component.instances[root->childId[j]];if(t&&t->buttonType==BUTTON_OK&&t->text[0]&&!strcmp(t->text,wanted)){vita_dialogue_input(c,false,false,0,false);for(int n=0;n<5&&c->vita_dialogue_selected!=t->id;n++)vita_dialogue_input(c,false,false,1,false);if(c->vita_dialogue_selected!=t->id)abort();vita_dialogue_input(c,false,true,0,false);if(getenv("HITPOINTS_REPLAY_PROBE")&&strstr(wanted,"Buy the cape")){for(int k=0;k<5;k++)client_vita_dialogue_action(c,t->id,951);rs2_log("RUNE_NETWORK_REPLAY sent5 stale purchase buttons\n");}rs2_log("RUNE D-pad Cross phase=%d text=%s colour=%06x\n",phase,wanted,vita_interface_text_colour(c,t->id,t->colour));phase++;acted=true;break;}}
 if(!acted&&getenv("RUNE_PREVIEW")){for(int j=0;root&&j<root->childCount;j++){Component*t=_Component.instances[root->childId[j]];if(!t)continue;const char*stage=NULL;if(strstr(t->text,"You have mastered Hitpoints")&&sec<18)stage="eligible";else if(strstr(t->text,"mastered another skill")&&sec<28)stage="trim";else if(strstr(t->text,"When you reach level 99 Hitpoints"))stage="lowlevel";if(stage){rs2_log("RUNE_PREVIEW_READY stage=%s\n",stage);return;}}}
if(!acted)for(int j=0;root&&j<root->childCount;j++){Component*t=_Component.instances[root->childId[j]];if(t&&t->text[0])rs2_log("RUNE TEXT phase=%d %s\n",phase,t->text);if(t&&t->buttonType==BUTTON_CONTINUE){vita_dialogue_input(c,false,true,0,false);rs2_log("RUNE Cross continue sec=%d\n",sec);break;}}
 }
 if(phase>=2&&c->chat_interface_id<0&&!done&&sec>16){Component*inv=_Component.instances[3214];int cape=getenv("MASTER_TRIM")?2924:2923;for(int slot=0;slot<inv->width*inv->height;slot++)if(inv->invSlotObjId[slot]==cape+1){c->menu_action[0]=38;c->menuParamA[0]=cape;c->menuParamB[0]=slot;c->menuParamC[0]=3214;useMenuOption(c,0);rs2_log("RUNE normal Wear cape=%d\n",cape);break;}done=1;}
 if(done==1&&sec>20){Component*inv=_Component.instances[3214];for(int slot=0;slot<inv->width*inv->height;slot++)if(inv->invSlotObjId[slot]==2926){c->menu_action[0]=38;c->menuParamA[0]=2925;c->menuParamB[0]=slot;c->menuParamC[0]=3214;useMenuOption(c,0);rs2_log("RUNE normal Wear hood=2925\n");break;}done=2;}
 if(game_frames%100==0)rs2_log("RUNE sec=%d phase=%d cape=%d head=%d latest=%s\n",sec,phase,c->local_player->appearances[1],c->local_player->appearances[0],c->message_text[0]);return;
}
if(getenv("PRIESTPERIL_TEST")){static uint64_t began=0,last=0;static int phase=0,pages=0,done=0;uint64_t now=rs2_now();if(!began)began=now;c->orbit_camera_yaw=512;c->orbit_camera_pitch=256;vita_dialogue_input(c,false,false,0,false);
if(!phase&&now-began>1800){for(int i=0;i<c->npc_count;i++){int id=c->npc_ids[i];NpcEntity*n=c->npcs[id];if(n&&n->type&&n->type->index==648){c->menu_action[0]=728;c->menuParamA[0]=id;useMenuOption(c,0);phase=1;rs2_log("PIP_CONNECTED normal Talk-to existing King Roald\n");break;}}}
if(phase&&c->chat_interface_id>=0&&now-last>650){last=now;Component*r=_Component.instances[c->chat_interface_id];if(getenv("PIP_DIALOGUE_PREVIEW")){for(int j=0;r&&j<r->childCount;j++){Component*t=_Component.instances[r->childId[j]];if(t&&t->text&&strstr(t->text,"A quest you say?")){rs2_log("PIP_DIALOGUE_PREVIEW original Roald page held for capture\n");return;}}}bool acted=false;for(int j=0;r&&j<r->childCount;j++){Component*t=_Component.instances[r->childId[j]];if(!t||!t->text)continue;rs2_log("PIP_TEXT %s\n",t->text);if(t->buttonType==BUTTON_OK&&!strcmp(t->text,"Sure.")){vita_dialogue_input(c,false,false,0,false);int first=c->vita_dialogue_selected;vita_dialogue_input(c,false,false,1,false);vita_dialogue_input(c,false,false,-1,false);if(c->vita_dialogue_selected!=first)abort();for(int k=0;k<5&&c->vita_dialogue_selected!=t->id;k++)vita_dialogue_input(c,false,false,1,false);if(c->vita_dialogue_selected!=t->id)abort();vita_dialogue_input(c,false,true,0,false);vita_dialogue_input(c,false,true,0,false);phase=2;acted=true;rs2_log("PIP_CONNECTED D-pad and Cross accepted original Sure choice\n");break;}}
if(!acted)for(int j=0;r&&j<r->childCount;j++){Component*t=_Component.instances[r->childId[j]];if(t&&t->buttonType==BUTTON_CONTINUE){vita_dialogue_input(c,false,true,0,false);pages++;rs2_log("PIP_CONNECTED continue page=%d\n",pages);break;}}}
if(phase==2&&c->chat_interface_id<0&&!done&&now-began>18000){client_vita_dialogue_action(c,7630,951);done=1;rs2_log("PIP_CONNECTED original IF_BUTTON requested new Journal entry\n");}return;}
if(getenv("PRIESTPERIL_JOURNAL_REVIEW")){static uint64_t began=0;static int sent=0;uint64_t now=rs2_now();if(!began)began=now;if(now-began>2000){for(int tab=0;tab<14;tab++)if(c->tab_interface_id[tab]==638){c->selected_tab=tab;c->redraw_sideicons=true;c->redraw_sidebar=true;break;}Component*p=_Component.instances[639];p->scrollPosition=p->scroll-p->height;c->redraw_sidebar=true;}if(now-began>9000&&!sent){client_vita_dialogue_action(c,7630,951);sent=1;rs2_log("PIP_JOURNAL original button requested\n");}return;}
if(getenv("MINING_TEST")){if(getenv("RELOGIN_ONLY")){if(game_frames%100==0){int coins=0;Component*v=_Component.instances[3214];for(int i=0;i<v->width*v->height;i++)if(v->invSlotObjId[i]==996)coins+=v->invSlotObjCount[i];rs2_log("RUNE_RELOGIN cape=%d hood=%d coins=%d\n",c->local_player->appearances[1],c->local_player->appearances[0],coins);}return;}
 static uint64_t began=0,last=0;static int phase=0,done=0;uint64_t now=rs2_now();if(!began)began=now;int sec=(now-began)/1000;
 c->orbit_camera_yaw=512;c->orbit_camera_pitch=256;
 if(!phase&&sec>=2&&getenv("GADRIN_NAME_PROBE")){
 if(strcmp(npctype_get(1020)->name,"Gadrin")||strcmp(npctype_get(382)->name,"Dwarf"))abort();
 for(int i=0;i<c->npc_count;i++){int id=c->npc_ids[i];NpcEntity*n=c->npcs[id];if(n&&n->type&&n->type->index==1020&&sec<8){
 c->menu_size=1;strcpy(c->menu_option[0],"Cancel");c->menu_action[0]=1252;addNpcOptions(c,n->type,id,0,0);
 bool talk=false,examine=false;for(int j=1;j<c->menu_size;j++){rs2_log("GADRIN_MENU %s\n",c->menu_option[j]);if(strstr(c->menu_option[j],"Talk-to")&&strstr(c->menu_option[j],"Gadrin"))talk=true;if(strstr(c->menu_option[j],"Examine")&&strstr(c->menu_option[j],"Gadrin"))examine=true;}if(!talk||!examine)abort();
 c->menu_visible=true;c->menu_area=0;c->menu_x=310;c->menu_y=170;c->menu_width=230;c->menu_height=c->menu_size*15+22;rs2_log("RUNE_PREVIEW_READY stage=menu\n");return;}}
 c->menu_visible=false;
}
if(!phase&&sec>=2){for(int i=0;i<c->npc_count;i++){int id=c->npc_ids[i];NpcEntity*n=c->npcs[id];if(n&&n->type&&n->type->index==1020){c->menu_action[0]=728;c->menuParamA[0]=id;useMenuOption(c,0);phase=1;rs2_log("RUNE normal Talk-to target=1020 sec=%d\n",sec);break;}}}
 if(phase&&c->chat_interface_id>=0&&now-last>=1600){last=now;Component*root=_Component.instances[c->chat_interface_id];const char*wanted=phase==1?(getenv("CRAFT_ORIGINAL")?"Talk about the Mining Guild.":"I'd like to buy a Mining cape."):(getenv("RANGE_DECLINE")?"No thank you.":"Buy the cape and hood for 99,000 coins.");if(getenv("CRAFT_ORIGINAL")&&phase>=2)wanted="No thanks, I'm fine.";bool acted=false;if(getenv("RUNE_PREVIEW")){for(int j=0;root&&j<root->childCount;j++){Component*t=_Component.instances[root->childId[j]];if(t&&((phase==1&&!strcmp(t->text,"I'd like to buy a Mining cape.")&&sec<14)||(phase>=2&&!strcmp(t->text,"Buy the cape and hood for 99,000 coins.")))){rs2_log("RUNE_PREVIEW_READY stage=%s\n",phase==1?"initial":"choices");return;}}}
 for(int j=0;root&&j<root->childCount;j++){Component*t=_Component.instances[root->childId[j]];if(t&&t->buttonType==BUTTON_OK&&t->text[0]&&!strcmp(t->text,wanted)){vita_dialogue_input(c,false,false,0,false);for(int n=0;n<5&&c->vita_dialogue_selected!=t->id;n++)vita_dialogue_input(c,false,false,1,false);if(c->vita_dialogue_selected!=t->id)abort();vita_dialogue_input(c,false,true,0,false);if(getenv("MINING_REPLAY_PROBE")&&strstr(wanted,"Buy the cape")){for(int k=0;k<5;k++)client_vita_dialogue_action(c,t->id,951);rs2_log("RUNE_NETWORK_REPLAY sent5 stale purchase buttons\n");}rs2_log("RUNE D-pad Cross phase=%d text=%s colour=%06x\n",phase,wanted,vita_interface_text_colour(c,t->id,t->colour));phase++;acted=true;break;}}
 if(!acted&&getenv("RUNE_PREVIEW")){for(int j=0;root&&j<root->childCount;j++){Component*t=_Component.instances[root->childId[j]];if(!t)continue;const char*stage=NULL;if(strstr(t->text,"You have mastered Mining")&&sec<24)stage="eligible";else if(strstr(t->text,"mastered another skill")&&sec<34)stage="trim";else if(strstr(t->text,"When you reach level 99 Mining"))stage="lowlevel";if(stage){rs2_log("RUNE_PREVIEW_READY stage=%s\n",stage);return;}}}
if(!acted&&getenv("GADRIN_ORIGINAL")){for(int j=0;root&&j<root->childCount;j++){Component*t=_Component.instances[root->childId[j]];if(t&&strstr(t->text,"Welcome to the Mining Guild")&&sec<24){rs2_log("RUNE_PREVIEW_READY stage=original\n");return;}}}
if(!acted)for(int j=0;root&&j<root->childCount;j++){Component*t=_Component.instances[root->childId[j]];if(t&&t->text[0])rs2_log("RUNE TEXT phase=%d %s\n",phase,t->text);if(t&&t->buttonType==BUTTON_CONTINUE){vita_dialogue_input(c,false,true,0,false);rs2_log("RUNE Cross continue sec=%d\n",sec);break;}}
 }
 if(phase>=2&&c->chat_interface_id<0&&!done&&sec>16){Component*inv=_Component.instances[3214];int cape=getenv("MASTER_TRIM")?2927:2926;for(int slot=0;slot<inv->width*inv->height;slot++)if(inv->invSlotObjId[slot]==cape+1){c->menu_action[0]=38;c->menuParamA[0]=cape;c->menuParamB[0]=slot;c->menuParamC[0]=3214;useMenuOption(c,0);rs2_log("RUNE normal Wear cape=%d\n",cape);break;}done=1;}
 if(done==1&&sec>20){Component*inv=_Component.instances[3214];for(int slot=0;slot<inv->width*inv->height;slot++)if(inv->invSlotObjId[slot]==2929){c->menu_action[0]=38;c->menuParamA[0]=2928;c->menuParamB[0]=slot;c->menuParamC[0]=3214;useMenuOption(c,0);rs2_log("RUNE normal Wear hood=2928\n");break;}done=2;}
 if(game_frames%100==0)rs2_log("RUNE sec=%d phase=%d cape=%d head=%d latest=%s\n",sec,phase,c->local_player->appearances[1],c->local_player->appearances[0],c->message_text[0]);return;
}
if(getenv("FISHING_TEST")){if(getenv("RELOGIN_ONLY")){if(game_frames%100==0){int coins=0;Component*v=_Component.instances[3214];for(int i=0;i<v->width*v->height;i++)if(v->invSlotObjId[i]==996)coins+=v->invSlotObjCount[i];rs2_log("FISH_RELOGIN cape=%d hood=%d coins=%d\n",c->local_player->appearances[1],c->local_player->appearances[0],coins);}return;}
 static uint64_t began=0,last=0;static int phase=0,done=0;uint64_t now=rs2_now();if(!began)began=now;int sec=(now-began)/1000;
 c->orbit_camera_yaw=512;c->orbit_camera_pitch=256;
 if(!phase&&sec>=2){for(int i=0;i<c->npc_count;i++){int id=c->npc_ids[i];NpcEntity*n=c->npcs[id];if(n&&n->type&&n->type->index==308){c->menu_action[0]=728;c->menuParamA[0]=id;useMenuOption(c,0);phase=1;rs2_log("FISH normal Talk-to target=308 sec=%d\n",sec);break;}}}
 if(phase&&c->chat_interface_id>=0&&now-last>=1600){last=now;Component*root=_Component.instances[c->chat_interface_id];const char*wanted=phase==1?(getenv("CRAFT_ORIGINAL")?"Talk about the Fishing Guild.":"I'd like to buy a Fishing cape."):(getenv("RANGE_DECLINE")?"No thank you.":"Buy the cape and hood for 99,000 coins.");bool acted=false;if(getenv("FISH_PREVIEW")){for(int j=0;root&&j<root->childCount;j++){Component*t=_Component.instances[root->childId[j]];if(t&&((phase==1&&!strcmp(t->text,"I'd like to buy a Fishing cape.")&&sec<8)||(phase>=2&&!strcmp(t->text,"Buy the cape and hood for 99,000 coins.")))){rs2_log("FISH_PREVIEW_READY stage=%s\n",phase==1?"initial":"choices");return;}}}
 for(int j=0;root&&j<root->childCount;j++){Component*t=_Component.instances[root->childId[j]];if(t&&t->text[0]&&!strcmp(t->text,wanted)){vita_dialogue_input(c,false,false,0,false);for(int n=0;n<5&&c->vita_dialogue_selected!=t->id;n++)vita_dialogue_input(c,false,false,1,false);if(c->vita_dialogue_selected!=t->id)abort();vita_dialogue_input(c,false,true,0,false);if(getenv("FISHING_REPLAY_PROBE")&&strstr(wanted,"Buy the cape")){for(int k=0;k<5;k++)client_vita_dialogue_action(c,t->id,951);rs2_log("FISH_NETWORK_REPLAY sent5 stale purchase buttons\n");}rs2_log("FISH D-pad Cross phase=%d text=%s colour=%06x\n",phase,wanted,vita_interface_text_colour(c,t->id,t->colour));phase++;acted=true;break;}}
 if(!acted&&getenv("FISH_PREVIEW")){for(int j=0;root&&j<root->childCount;j++){Component*t=_Component.instances[root->childId[j]];if(!t)continue;const char*stage=NULL;if(strstr(t->text,"You have mastered Fishing")&&sec<18)stage="eligible";else if(strstr(t->text,"mastered another skill")&&sec<28)stage="trim";else if(strstr(t->text,"When you reach level 99 Fishing"))stage="lowlevel";if(stage){rs2_log("FISH_PREVIEW_READY stage=%s\n",stage);return;}}}
if(!acted)for(int j=0;root&&j<root->childCount;j++){Component*t=_Component.instances[root->childId[j]];if(t&&t->text[0])rs2_log("FISH TEXT phase=%d %s\n",phase,t->text);if(t&&t->buttonType==BUTTON_CONTINUE){vita_dialogue_input(c,false,true,0,false);rs2_log("FISH Cross continue sec=%d\n",sec);break;}}
 }
 if(phase>=2&&c->chat_interface_id<0&&!done&&sec>16){Component*inv=_Component.instances[3214];int cape=getenv("MASTER_TRIM")?2915:2914;for(int slot=0;slot<inv->width*inv->height;slot++)if(inv->invSlotObjId[slot]==cape+1){c->menu_action[0]=38;c->menuParamA[0]=cape;c->menuParamB[0]=slot;c->menuParamC[0]=3214;useMenuOption(c,0);rs2_log("FISH normal Wear cape=%d\n",cape);break;}done=1;}
 if(done==1&&sec>20){Component*inv=_Component.instances[3214];for(int slot=0;slot<inv->width*inv->height;slot++)if(inv->invSlotObjId[slot]==2917){c->menu_action[0]=38;c->menuParamA[0]=2916;c->menuParamB[0]=slot;c->menuParamC[0]=3214;useMenuOption(c,0);rs2_log("FISH normal Wear hood=2916\n");break;}done=2;}
 if(game_frames%100==0)rs2_log("FISH sec=%d phase=%d cape=%d head=%d latest=%s\n",sec,phase,c->local_player->appearances[1],c->local_player->appearances[0],c->message_text[0]);return;
}
if(getenv("SMITHING_TEST")){if(getenv("RELOGIN_ONLY")){if(game_frames%100==0){int coins=0;Component*v=_Component.instances[3214];for(int i=0;i<v->width*v->height;i++)if(v->invSlotObjId[i]==996)coins+=v->invSlotObjCount[i];rs2_log("SMITH_RELOGIN cape=%d hood=%d coins=%d\n",c->local_player->appearances[1],c->local_player->appearances[0],coins);}return;}
 static uint64_t began=0,last=0;static int phase=0,done=0;uint64_t now=rs2_now();if(!began)began=now;int sec=(now-began)/1000;
 c->orbit_camera_yaw=512;c->orbit_camera_pitch=256;
 if(!phase&&sec>=2){for(int i=0;i<c->npc_count;i++){int id=c->npc_ids[i];NpcEntity*n=c->npcs[id];if(n&&n->type&&n->type->index==604){c->menu_action[0]=728;c->menuParamA[0]=id;useMenuOption(c,0);phase=1;rs2_log("SMITH normal Talk-to target=604 sec=%d\n",sec);break;}}}
 if(phase&&c->chat_interface_id>=0&&now-last>=1600){last=now;Component*root=_Component.instances[c->chat_interface_id];const char*wanted=phase==1?(getenv("CRAFT_ORIGINAL")?"Talk to Thurgo.":"I'd like to buy a Smithing cape."):(getenv("RANGE_DECLINE")?"No thank you.":"Buy the cape and hood for 99,000 coins.");bool acted=false;
 for(int j=0;root&&j<root->childCount;j++){Component*t=_Component.instances[root->childId[j]];if(t&&t->text[0]&&!strcmp(t->text,wanted)){vita_dialogue_input(c,false,false,0,false);for(int n=0;n<5&&c->vita_dialogue_selected!=t->id;n++)vita_dialogue_input(c,false,false,1,false);if(c->vita_dialogue_selected!=t->id)abort();vita_dialogue_input(c,false,true,0,false);rs2_log("SMITH D-pad Cross phase=%d text=%s colour=%06x\n",phase,wanted,vita_interface_text_colour(c,t->id,t->colour));phase++;acted=true;break;}}
 if(!acted&&getenv("SMITH_PREVIEW")){for(int j=0;root&&j<root->childCount;j++){Component*t=_Component.instances[root->childId[j]];if(t&&((strstr(t->text,"You have mastered Smithing")&&sec<12)||strstr(t->text,"mastered another skill")||strstr(t->text,"When you reach level 99 Smithing")))return;}}
if(!acted)for(int j=0;root&&j<root->childCount;j++){Component*t=_Component.instances[root->childId[j]];if(t&&t->text[0])rs2_log("SMITH TEXT phase=%d %s\n",phase,t->text);if(t&&t->buttonType==BUTTON_CONTINUE){vita_dialogue_input(c,false,true,0,false);rs2_log("SMITH Cross continue sec=%d\n",sec);break;}}
 }
 if(phase>=2&&c->chat_interface_id<0&&!done&&sec>16){Component*inv=_Component.instances[3214];int cape=getenv("MASTER_TRIM")?2912:2911;for(int slot=0;slot<inv->width*inv->height;slot++)if(inv->invSlotObjId[slot]==cape+1){c->menu_action[0]=38;c->menuParamA[0]=cape;c->menuParamB[0]=slot;c->menuParamC[0]=3214;useMenuOption(c,0);rs2_log("SMITH normal Wear cape=%d\n",cape);break;}done=1;}
 if(done==1&&sec>20){Component*inv=_Component.instances[3214];for(int slot=0;slot<inv->width*inv->height;slot++)if(inv->invSlotObjId[slot]==2914){c->menu_action[0]=38;c->menuParamA[0]=2913;c->menuParamB[0]=slot;c->menuParamC[0]=3214;useMenuOption(c,0);rs2_log("SMITH normal Wear hood=2913\n");break;}done=2;}
 if(game_frames%100==0)rs2_log("SMITH sec=%d phase=%d cape=%d head=%d latest=%s\n",sec,phase,c->local_player->appearances[1],c->local_player->appearances[0],c->message_text[0]);return;
}
if(getenv("CRAFTING_TEST")){if(getenv("RELOGIN_ONLY")){if(game_frames%100==0){int coins=0;Component*v=_Component.instances[3214];for(int i=0;i<v->width*v->height;i++)if(v->invSlotObjId[i]==996)coins+=v->invSlotObjCount[i];rs2_log("CRAFT_RELOGIN cape=%d hood=%d coins=%d\n",c->local_player->appearances[1],c->local_player->appearances[0],coins);}return;}
 static uint64_t began=0,last=0;static int phase=0,done=0;uint64_t now=rs2_now();if(!began)began=now;int sec=(now-began)/1000;
 c->orbit_camera_yaw=512;c->orbit_camera_pitch=256;
 if(!phase&&sec>=2){for(int i=0;i<c->npc_count;i++){int id=c->npc_ids[i];NpcEntity*n=c->npcs[id];if(n&&n->type&&n->type->index==805){c->menu_action[0]=728;c->menuParamA[0]=id;useMenuOption(c,0);phase=1;rs2_log("CRAFT normal Talk-to target=805 sec=%d\n",sec);break;}}}
 if(phase&&c->chat_interface_id>=0&&now-last>=1600){last=now;Component*root=_Component.instances[c->chat_interface_id];const char*wanted=phase==1?(getenv("CRAFT_ORIGINAL")?"Hello.":"I'd like to buy a Crafting cape."):(getenv("RANGE_DECLINE")?"No thank you.":"Buy the cape and hood for 99,000 coins.");bool acted=false;
 for(int j=0;root&&j<root->childCount;j++){Component*t=_Component.instances[root->childId[j]];if(t&&t->text[0]&&!strcmp(t->text,wanted)){vita_dialogue_input(c,false,false,0,false);for(int n=0;n<5&&c->vita_dialogue_selected!=t->id;n++)vita_dialogue_input(c,false,false,1,false);if(c->vita_dialogue_selected!=t->id)abort();vita_dialogue_input(c,false,true,0,false);rs2_log("CRAFT D-pad Cross phase=%d text=%s colour=%06x\n",phase,wanted,vita_interface_text_colour(c,t->id,t->colour));phase++;acted=true;break;}}
 if(!acted&&getenv("CRAFT_PREVIEW")){for(int j=0;root&&j<root->childCount;j++){Component*t=_Component.instances[root->childId[j]];if(t&&((strstr(t->text,"You have mastered Crafting")&&sec<12)||strstr(t->text,"mastered another skill")||strstr(t->text,"When you reach level 99 Crafting")))return;}}
if(!acted)for(int j=0;root&&j<root->childCount;j++){Component*t=_Component.instances[root->childId[j]];if(t&&t->text[0])rs2_log("CRAFT TEXT phase=%d %s\n",phase,t->text);if(t&&t->buttonType==BUTTON_CONTINUE){vita_dialogue_input(c,false,true,0,false);rs2_log("CRAFT Cross continue sec=%d\n",sec);break;}}
 }
 if(phase>=2&&c->chat_interface_id<0&&!done&&sec>16){Component*inv=_Component.instances[3214];int cape=getenv("MASTER_TRIM")?2909:2908;for(int slot=0;slot<inv->width*inv->height;slot++)if(inv->invSlotObjId[slot]==cape+1){c->menu_action[0]=38;c->menuParamA[0]=cape;c->menuParamB[0]=slot;c->menuParamC[0]=3214;useMenuOption(c,0);rs2_log("CRAFT normal Wear cape=%d\n",cape);break;}done=1;}
 if(done==1&&sec>20){Component*inv=_Component.instances[3214];for(int slot=0;slot<inv->width*inv->height;slot++)if(inv->invSlotObjId[slot]==2911){c->menu_action[0]=38;c->menuParamA[0]=2910;c->menuParamB[0]=slot;c->menuParamC[0]=3214;useMenuOption(c,0);rs2_log("CRAFT normal Wear hood=2910\n");break;}done=2;}
 if(game_frames%100==0)rs2_log("CRAFT sec=%d phase=%d cape=%d head=%d latest=%s\n",sec,phase,c->local_player->appearances[1],c->local_player->appearances[0],c->message_text[0]);return;
}
if(getenv("TUTOR_PLACEMENT_PREVIEW")){
 static uint64_t began=0,lastAction=0,opened=0;static int round=0,active=0,entry=0;uint64_t now=rs2_now();if(!began)began=now;int seconds=(now-began)/1000;
 c->orbit_camera_yaw=512;c->orbit_camera_pitch=256;
 int types[]={1017,1018,1019,1017,1018,1019};int starts[]={12,70,130,190,250,310};
 if(round<6&&!active&&seconds>=starts[round]){int found=-1;for(int i=0;i<c->npc_count;i++){int id=c->npc_ids[i];NpcEntity*n=c->npcs[id];if(n&&n->type&&n->type->index==types[round]){found=id;break;}}if(found>=0){c->menu_action[0]=728;c->menuParamA[0]=found;useMenuOption(c,0);rs2_log("OBS_TALK_REQUEST round=%d type=%d sec=%d\n",round,types[round],seconds);active=1;entry=0;opened=0;lastAction=now;}}
 if(active&&c->chat_interface_id>=0){if(!opened){opened=now;rs2_log("OBS_DIALOGUE_OPEN round=%d root=%d sec=%d\n",round,c->chat_interface_id,seconds);}if(now-opened>=8000&&now-lastAction>=1000){lastAction=now;Component*root=_Component.instances[c->chat_interface_id];bool acted=false;const char*wanted=round==3?(entry?"Buy the cape and hood for 99,000 coins.":"I'd like to buy a Defence cape."):"Goodbye.";for(int j=0;root&&j<root->childCount;j++){Component*t=_Component.instances[root->childId[j]];if(t&&t->text[0]&&!strcmp(t->text,wanted)){vita_dialogue_input(c,false,false,0,false);for(int n=0;n<10&&c->vita_dialogue_selected!=t->id;n++)vita_dialogue_input(c,false,false,1,false);if(c->vita_dialogue_selected!=t->id)abort();vita_dialogue_input(c,false,true,0,false);rs2_log("OBS_CONTROLLER_CHOICE round=%d text=%s\n",round,wanted);entry++;acted=true;break;}}
 if(!acted){for(int j=0;root&&j<root->childCount;j++){Component*t=_Component.instances[root->childId[j]];if(t&&t->buttonType==BUTTON_CONTINUE){vita_dialogue_input(c,false,true,0,false);rs2_log("OBS_CONTROLLER_CONTINUE round=%d sec=%d\n",round,seconds);break;}}}}}
 if(active&&opened&&c->chat_interface_id<0&&now-opened>8500){int sx=c->local_player->pathing_entity.pathTileX[0],sz=c->local_player->pathing_entity.pathTileZ[0];bool ok=client_try_move(c,sx,sz,3223-c->sceneBaseTileX,3236-c->sceneBaseTileZ,0,0,0,0,0,0,true);rs2_log("OBS_DIALOGUE_COMPLETE round=%d depart_walk=%d sec=%d\n",round,ok,seconds);active=0;round++;}
 if(game_frames%50==0){for(int i=0;i<c->npc_count;i++){NpcEntity*n=c->npcs[c->npc_ids[i]];if(n&&n->type&&n->type->index>=1017&&n->type->index<=1019)rs2_log("OBS_CLIENT_NPC sec=%d type=%lld seq=%d frame=%d\n",seconds,(long long)n->type->index,n->pathing_entity.secondarySeqId,n->pathing_entity.secondarySeqFrame);}}
 return;
}
    if(getenv("RELOGIN_ONLY")){if(game_frames%100==0)rs2_log("RELOGIN frame=%d cape=%d head=%d player=%s\n",game_frames,c->local_player->appearances[1],c->local_player->appearances[0],c->local_player->name);return;}
    if(getenv("MULTI_FIRE")){
        if(game_frames%200==100){Component*inv=_Component.instances[3214];for(int slot=0;slot<inv->width*inv->height;slot++)if(inv->invSlotObjId[slot]==2890){c->menu_action[0]=38;c->menuParamA[0]=2889;c->menuParamB[0]=slot;c->menuParamC[0]=3214;useMenuOption(c,0);break;}}
        if(game_frames%200==0 && game_frames>0 && game_frames<2000){Component*w=_Component.instances[1688];if(w->invSlotObjId[1]==2890){c->menu_action[0]=602;c->menuParamA[0]=2889;c->menuParamB[0]=1;c->menuParamC[0]=1688;useMenuOption(c,0);}}
        c->shell->action_key[2]=(game_frames>=400&&game_frames<800)?1:0;
        if(game_frames%100==0){int fire=c->local_player->appearances[1]==3401?1:0;for(int i=0;i<c->player_count;i++){PlayerEntity*p=c->players[c->player_ids[i]];if(p&&p->appearances[1]==3401)fire++;}rs2_log("MULTI frame=%d visible_fire=%d own_cape=%d players=%d\n",game_frames,fire,c->local_player->appearances[1],c->player_count);}
        return;
    }
    if(getenv("MASTER_TEST")) {
        if(game_frames==100){int found=-1;for(int i=0;i<c->npc_count;i++){int id=c->npc_ids[i];NpcEntity*n=c->npcs[id];if(n&&n->type&&n->type->name&&strcmp(n->type->name,"Melee combat tutor")==0){found=id;break;}}if(found>=0){c->menu_action[0]=728;c->menuParamA[0]=found;useMenuOption(c,0);rs2_log("MASTER normal NPC Talk-to slot=%d type=%lld\n",found,(long long)c->npcs[found]->type->index);}else rs2_log("MASTER NPC unavailable\n");}
        static int master_accepted=0,master_entry=0;static int logged_root=-2;
        if(getenv("HARLAN_REPEAT") && game_frames==1300){master_accepted=0;master_entry=0;for(int i=0;i<c->npc_count;i++){int id=c->npc_ids[i];NpcEntity*n=c->npcs[id];if(n&&n->type&&n->type->index==1017){c->menu_action[0]=728;c->menuParamA[0]=id;useMenuOption(c,0);rs2_log("HARLAN fresh paid repeat normal Talk-to\n");break;}}}
        if(getenv("HARLAN_REPEAT") && (game_frames==2300||game_frames==2500)){int sx=c->local_player->pathing_entity.pathTileX[0],sz=c->local_player->pathing_entity.pathTileZ[0];bool run=game_frames==2500;c->shell->action_key[5]=run?1:0;bool moved=client_try_move(c,sx,sz,sx+3,sz,0,0,0,0,0,0,true);c->shell->action_key[5]=0;rs2_log("HARLAN normal movement run=%d accepted=%d\n",run,moved);}
        if(getenv("HARLAN_REPEAT"))c->shell->action_key[2]=(game_frames>=2400&&game_frames<2480)?1:0;

 if(c->chat_interface_id!=logged_root){logged_root=c->chat_interface_id;if(logged_root>=0){Component*r=_Component.instances[logged_root];for(int j=0;r&&j<r->childCount;j++){Component*t=_Component.instances[r->childId[j]];if(t&&t->text&&t->text[0])rs2_log("LIVE_DIALOGUE root=%d id=%d button=%d text=%s\n",logged_root,t->id,t->buttonType,t->text);}}}

        if(game_frames>140 && game_frames%70==0 && c->chat_interface_id>=0){Component*root=_Component.instances[c->chat_interface_id];if(root&&root->childId){for(int i=0;i<root->childCount;i++){int id=root->childId[i];if(id<0||id>=_Component.count)continue;Component*com=_Component.instances[id];if(!com||com->hide)continue;if(id==2484&&!master_entry){
 int option=getenv("RANGE_SHOP")?0:getenv("RANGE_FAREWELL")?1:2;
 vita_dialogue_input(c,false,false,0,false);int first=c->vita_dialogue_selected;
 for(int step=0;step<4;step++)vita_dialogue_input(c,false,false,1,false);
 if(c->vita_dialogue_selected!=first){rs2_error("TEST: four-choice wrap failed\n");abort();}
 for(int step=0;step<option;step++)vita_dialogue_input(c,false,false,1,false);
 int selected=c->vita_dialogue_selected;
 if(selected!=2482+option){rs2_error("TEST: original choice order changed\n");abort();}
 rs2_log("HARLAN production D-pad option=%d component=%d colour=%06x\n",option+1,selected,vita_interface_text_colour(c,selected,_Component.instances[selected]->colour));
 vita_dialogue_input(c,false,true,0,false);master_entry=1;break;}
 if(getenv("RANGE_ADVICE")&&id==2472&&master_entry==1){
 vita_dialogue_input(c,false,false,0,false);
 if(getenv("RANGE_ACCESSORIES"))vita_dialogue_input(c,false,false,1,false);
 rs2_log("HARLAN original advice production selection=%d\n",c->vita_dialogue_selected);
 vita_dialogue_input(c,false,true,0,false);master_entry=2;break;}
if(id==RESEARCH_MASTER_YES&&!master_accepted){c->menu_action[0]=951;c->menuParamC[0]=getenv("RANGE_DECLINE")?2462:id;useMenuOption(c,0);if(!getenv("RANGE_DECLINE"))useMenuOption(c,0);master_accepted=1;rs2_log("MASTER accept normal IF_BUTTON component=%d twice replay test frame=%d\n",id,game_frames);break;}if(com->buttonType==BUTTON_CONTINUE){
if(getenv("APPROVED_DIALOGUE_PREVIEW")) {
 bool approved_page=false;
 for(int j=0;j<root->childCount;j++){int cid=root->childId[j];if(cid>=0&&cid<_Component.count){Component *t=_Component.instances[cid];if(t&&t->text&&strstr(t->text,"mastered another"))approved_page=true;}}
 if(approved_page)break;
}
c->menu_action[0]=44;c->menuParamC[0]=id;useMenuOption(c,0);rs2_log("MASTER normal continuation component=%d frame=%d\n",id,game_frames);break;}}}}
        if(game_frames==1000){Component*inv=_Component.instances[3214];for(int slot=0;slot<inv->width*inv->height;slot++)if(inv->invSlotObjId[slot]==(getenv("MASTER_TRIM")?2898:2897)){c->menu_action[0]=38;c->menuParamA[0]=getenv("MASTER_TRIM")?2897:2896;c->menuParamB[0]=slot;c->menuParamC[0]=3214;useMenuOption(c,0);rs2_log("MASTER purchased cape normal Wear slot=%d\n",slot);break;}}
        if(game_frames==1150){Component*inv=_Component.instances[3214];for(int slot=0;slot<inv->width*inv->height;slot++)if(inv->invSlotObjId[slot]==2899){c->menu_action[0]=38;c->menuParamA[0]=2898;c->menuParamB[0]=slot;c->menuParamC[0]=3214;useMenuOption(c,0);rs2_log("MASTER purchased hood normal Wear slot=%d\n",slot);break;}}
        if(game_frames%100==0)rs2_log("HARLAN_WINDOW main=%d\n",c->viewport_interface_id);
        if(game_frames%100==0)rs2_log("MASTER frame=%d chat=%d cape=%d head=%d latest_message=%s\n",game_frames,c->chat_interface_id,c->local_player->appearances[1],c->local_player->appearances[0],c->message_text[0]);
        return;
    }
    if (game_frames==100 || (game_frames==500&&!getenv("COMBAT_ONLY"))) {
        Component *inv=_Component.instances[3214];int wanted=game_frames==100?(getenv("EXPAND_TEST")?(getenv("EXPAND_TRIM")?2894:2893):(getenv("COOKING_TEST")?2890:2886)):(getenv("EXPAND_TEST")?(getenv("EXPAND_TRIM")?2897:2896):2889);
        for(int slot=0;slot<inv->width*inv->height;slot++) if(inv->invSlotObjId[slot]==wanted+1){
            c->menu_action[0]=38;c->menuParamA[0]=wanted;c->menuParamB[0]=slot;c->menuParamC[0]=3214;useMenuOption(c,0);rs2_log("RESEARCH normal OPHELD2 item=%d slot=%d\n",wanted,slot);break;}
    }
    if(!getenv("COMBAT_ONLY") && (game_frames==250 || game_frames==700)){int sx=c->local_player->pathing_entity.pathTileX[0],sz=c->local_player->pathing_entity.pathTileZ[0];bool run=game_frames==700;c->shell->action_key[5]=run?1:0;bool moved=client_try_move(c,sx,sz,sx+3,sz,0,0,0,0,0,0,true);c->shell->action_key[5]=0;rs2_log("RESEARCH normal movement requested run=%d accepted=%d start=%d,%d target=%d,%d\n",run,moved,sx,sz,sx+3,sz);}
    if(game_frames>=300 && game_frames<380)c->shell->action_key[2]=1;else c->shell->action_key[2]=0;
    if(game_frames==1000){Component*worn=_Component.instances[1688];if(worn->invSlotObjId[1]>0){c->menu_action[0]=602;c->menuParamA[0]=worn->invSlotObjId[1]-1;c->menuParamB[0]=1;c->menuParamC[0]=1688;useMenuOption(c,0);rs2_log("RESEARCH normal worn-interface unequip requested\n");}}
    if(getenv("COMBAT_ONLY") && game_frames==600){int best=-1,bestdist=100000,bestop=-1;for(int i=0;i<c->npc_count;i++){int id=c->npc_ids[i];NpcEntity*n=c->npcs[id];if(!n||!n->type||!n->type->name||n->type->vislevel<=0||n->type->vislevel>10||!n->type->op)continue;int op=-1;for(int j=0;j<5;j++)if(n->type->op[j]&&strcmp(n->type->op[j],"Attack")==0)op=j;if(op<0)continue;int dx=n->pathing_entity.pathTileX[0]-c->local_player->pathing_entity.pathTileX[0],dz=n->pathing_entity.pathTileZ[0]-c->local_player->pathing_entity.pathTileZ[0];int distance=dx*dx+dz*dz;if(distance<bestdist){best=id;bestdist=distance;bestop=op;}}if(best>=0){int actions[]={728,542,6,963,245};c->menu_action[0]=actions[bestop];c->menuParamA[0]=best;useMenuOption(c,0);rs2_log("RESEARCH normal NPC Attack name=%s definition=%lld slot=%d distance_sq=%d\n",c->npcs[best]->type->name,(long long)c->npcs[best]->type->index,best,bestdist);}else rs2_log("RESEARCH combat target unavailable\n");}
    if(getenv("REGRESSION_ONLY")){
        int component=game_frames==200?RESEARCH_SKILL_ATTACK:game_frames==450?RESEARCH_QUEST_COOK:(game_frames==1150||game_frames==1350)?(getenv("LUMBRIDGE_ONLY")?RESEARCH_LUMBRIDGE:RESEARCH_HOME):-1;
        if(component>=0){c->menu_action[0]=951;c->menuParamC[0]=component;useMenuOption(c,0);rs2_log("RESEARCH existing IF_BUTTON component=%d frame=%d\n",component,game_frames);}
        if(game_frames==350||game_frames==600){c->menu_action[0]=947;useMenuOption(c,0);rs2_log("RESEARCH existing close-modal requested\n");}
        if(game_frames%50==0)rs2_log("RESEARCH viewport_interface=%d chat_interface=%d spot=%d frame=%d\n",c->viewport_interface_id,c->chat_interface_id,c->local_player->pathing_entity.spotanimId,game_frames);
    }
    if(getenv("REGRESSION_ONLY") && game_frames>=1300 && game_frames%50==0)rs2_log("RESEARCH latest message=%s\n",c->message_text[0]);
    if(game_frames%100==0)rs2_log("RESEARCH game frames=%d cape appearance=%d tile=%d,%d sequence=%d player=%s\n",game_frames,c->local_player->appearances[1],c->local_player->pathing_entity.pathTileX[0],c->local_player->pathing_entity.pathTileZ[0],c->local_player->pathing_entity.secondarySeqId,c->local_player->name);
}

static const char *dp_name[DP_N] = {
    "setup", "clear", "prescene", "entity", "orbit", "world",
    "glprep", "glsubmit", "gltail", "post",
    "chat", "ovclear", "ov2d", "ovhint", "ov3d", "minimap", "coldui", "hud",
    "holdio",
    "pixother", "pixcmp", "pixcvt", "pixup", "pixquad",
    "tail"
};
extern void ba_cadence_diagnostic(const char*line);
static uint64_t dp_frame_us[DP_N];
static uint64_t dp_us[DP_N], dp_max[DP_N], dp_wall, dp_wall_max, dp_t0, dp_frame_sum, dp_flush_at, dp_prev_log;
static int64_t dp_gap;
static unsigned dp_n, dp_overlay, dp_map, dp_uploads, dp_saw_upload;
static int dp_active, dp_sp, dp_pending, dp_header_done;
static int dp_saw_overlay, dp_saw_map, dp_pitch, dp_yaw, dp_cam_pitch, dp_cam_yaw, dp_eye_x, dp_eye_z, dp_actors;
static struct { int bucket; uint64_t at; } dp_stack[8];

static void dp_add(int bucket, uint64_t us) {
    if (!us || bucket < 0 || bucket >= DP_N) return;
    dp_us[bucket] += us;dp_frame_us[bucket]+=us;
    dp_frame_sum += us;
    if (us > dp_max[bucket]) dp_max[bucket] = us;
}

void dp_switch(int bucket) {
    if (!dp_active || bucket < 0 || bucket >= DP_N) return;
    uint64_t now = ba_timing_now();
    if (dp_sp > 0) {
        dp_add(dp_stack[dp_sp - 1].bucket, now - dp_stack[dp_sp - 1].at);
        dp_stack[dp_sp - 1].bucket = bucket;
        dp_stack[dp_sp - 1].at = now;
    } else {
        dp_stack[0].bucket = bucket;
        dp_stack[0].at = now;
        dp_sp = 1;
    }
}

void dp_begin(int bucket) {
    if (!dp_active || dp_sp >= 8 || bucket < 0 || bucket >= DP_N) return;
    uint64_t now = ba_timing_now();
    if (dp_sp > 0) dp_add(dp_stack[dp_sp - 1].bucket, now - dp_stack[dp_sp - 1].at);
    dp_stack[dp_sp].bucket = bucket;
    dp_stack[dp_sp].at = now;
    dp_sp++;
}

void dp_end(void) {
    if (!dp_active || dp_sp <= 0) return;
    uint64_t now = ba_timing_now();
    dp_sp--;
    dp_add(dp_stack[dp_sp].bucket, now - dp_stack[dp_sp].at);
    if (dp_sp > 0) dp_stack[dp_sp - 1].at = now;
}

void dp_mark_overlay(void) { if (dp_active) dp_saw_overlay = 1; }
void dp_mark_map(void) { if (dp_active) dp_saw_map = 1; }
void dp_mark_upload(void) { if (dp_active) dp_saw_upload++; }

static void dp_write(uint64_t now) {
    if (r16_diag_verbose()) {
        uint64_t io0 = ba_timing_now();
        FILE *f = fopen("ux0:/data/RS2Vita/draw-phases.txt", "a");
        if (!f) {
            r16_io_add(5, ba_timing_now() - io0);
            return;
        }
        if (!dp_header_done) {
            fputs("exclusive client_draw microseconds. totals/max. gap is wall minus the sum of buckets. nested pixel and gl spans are not also counted in their parents. log is the previous flush and is outside wall.\n", f);
            dp_header_done = 1;
        }
        extern int vita_last_rx, vita_last_ry;
        fprintf(f, "t=%llu n=%u pitch=%d yaw=%d cam_pitch=%d cam_yaw=%d eye=%d,%d actors=%d stick=%d,%d overlay=%u map=%u uploads=%u wall=%llu/%llu gap=%lld prev_log=%llu",
            (unsigned long long)now, dp_n, dp_pitch, dp_yaw, dp_cam_pitch, dp_cam_yaw, dp_eye_x, dp_eye_z, dp_actors,
            vita_last_rx, vita_last_ry,
            dp_overlay, dp_map, dp_uploads, (unsigned long long)dp_wall, (unsigned long long)dp_wall_max,
            (long long)dp_gap, (unsigned long long)dp_prev_log);
        for (int i = 0; i < DP_N; i++) fprintf(f, " %s=%llu/%llu", dp_name[i], (unsigned long long)dp_us[i], (unsigned long long)dp_max[i]);
        fputc('\n', f);
        fclose(f);
        r16_io_add(5, ba_timing_now() - io0);
    }
    memset(dp_us, 0, sizeof dp_us);
    memset(dp_max, 0, sizeof dp_max);
    dp_n = dp_overlay = dp_map = dp_uploads = 0;
    dp_wall = dp_wall_max = 0;
    dp_gap = 0;
    dp_flush_at = now;
}

static void dp_frame_begin(void) {
    if (dp_pending) {
        uint64_t began = ba_timing_now();
        dp_write(began);
        dp_prev_log = ba_timing_now() - began;
        dp_pending = 0;
    }
    r16_dyn_flush();
    dp_active = 1;
    dp_sp = 0;
    dp_frame_sum = 0;memset(dp_frame_us,0,sizeof dp_frame_us);
    dp_saw_overlay = dp_saw_map = 0;
    dp_saw_upload = 0;
    dp_t0 = ba_timing_now();
    if (!dp_flush_at) dp_flush_at = dp_t0;
    dp_switch(DP_SETUP);
}

static void dp_frame_end(Client *c) {
    uint64_t now = ba_timing_now();
    while (dp_sp > 0) {
        dp_sp--;
        dp_add(dp_stack[dp_sp].bucket, now - dp_stack[dp_sp].at);
    }
    uint64_t wall = now - dp_t0;
    if(wall>50000u&&!r16_diag_verbose()){
        char line[384];int used=snprintf(line,sizeof line,"# render_hitch %llu wall %llu",(unsigned long long)now,(unsigned long long)wall);
        for(int i=0;i<DP_N&&used>0&&used<(int)sizeof line-32;i++)if(dp_frame_us[i]>=1000u)used+=snprintf(line+used,sizeof line-used," %s %llu",dp_name[i],(unsigned long long)dp_frame_us[i]);
        ba_cadence_diagnostic(line);
    }
    dp_wall += wall;
    if (wall > dp_wall_max) dp_wall_max = wall;
    dp_gap += (int64_t)wall - (int64_t)dp_frame_sum;
    dp_n++;
    dp_overlay += (unsigned)dp_saw_overlay;
    dp_map += (unsigned)dp_saw_map;
    dp_uploads += dp_saw_upload;
    dp_pitch = c->orbit_camera_pitch;
    dp_yaw = c->orbit_camera_yaw & 2047;
    dp_cam_pitch = c->cameraPitch;
    dp_cam_yaw = c->cameraYaw & 2047;
    dp_eye_x = c->cameraX >> 7;
    dp_eye_z = c->cameraZ >> 7;
    dp_actors = c->player_count + c->npc_count;
    dp_frame_sum = 0;
    dp_active = 0;
    r13_note_ingame_frame();
    if (now - dp_flush_at >= 1000000ull) dp_pending = 1;
}

void client_draw(Client *c) {
    ve_overlay_held_frame = 0;
    if(h5_silent())return;
    if(c->ingame) dp_frame_begin();
    h5_render_phase(1);
    if(ba_bench_active&&c->ingame){c->camera_anticheat_offset_x=c->camera_anticheat_offset_z=c->camera_anticheat_angle=0;c->minimap_anticheat_angle=0;c->minimap_zoom=0;}

    ui474_begin(c);
    migration_research_driver(c);
    if(c->ingame) dp_switch(DP_CLEAR);
    gl_start_frame();
    if(c->ingame) dp_switch(DP_SETUP);

    if (c->error_started || c->error_loading || c->error_host) {
        client_draw_error(c);
    } else {
        if (c->ingame) {
            /* Both theme paths consume the logic time accumulated since the last draw. */
            dp_switch(DP_PRESCENE);
            client_draw_game(c);
            /* Opt-in screenshots/observations also cover the Classic Brown path. */
            if (!ve_enabled()) ve_ui_probe(c);
            c->scene_delta = 0;
        } else {
            client_draw_title_screen(c);
        }

        c->drag_cycles = 0;
    }

    if(c->ingame) dp_switch(DP_TAIL);
    gl_end_frame();h5_render_phase(0);
    if(c->ingame) dp_frame_end(c);
}

static int ve_ui_warm, ve_warm_energy, ve_warm_tab, ve_warm_run, ve_warm_scroll;
static int ve_map_ready, ve_map_yaw, ve_map_x, ve_map_z;
static int ve_overlay_ready, ve_overlay_yaw, ve_overlay_pitch, ve_overlay_x, ve_overlay_z;
static int ve_seen_view = -2, ve_seen_chat = -2, ve_seen_sticky = -2;
static int ve_seen_modal_x,ve_seen_modal_y;
static int ve_seen_social, ve_seen_input_open, ve_seen_menu, ve_seen_performance, ve_seen_speech;
static char ve_seen_modal[MAX_STR];
static char ve_seen_input[CHATBACK_LENGTH + 1];

#include "r17_menu_overlay.inc"

static int ve_ui_dirty(Client *c) {
    updateWorldLocationState(c);
    if (!ve_ui_warm || c->scene_state != 2) return 1;
    int speech=ve_world_speech_active(c);
    /* Native speech is transient GPU content, independent of retained UI. */
    if(!ve_native_markers()&&(speech||speech!=ve_seen_speech))return 1;
    /* Live modal previews and their hit feedback must not become a held HUD. */
    if (c->viewport_interface_id >= 0 && !ve_popup_patch_guard) return 1;
    if (c->redraw_sidebar || c->redraw_chatback || c->redraw_sideicons || c->redraw_privacy_settings) return 1;
    if ((ve_enabled()&&ve_native_markers()) ? ((c->menu_visible&&!ve_menu_separate(c))||ve_seen_menu_baked) : (c->menu_visible||c->menu_visible!=ve_seen_menu)) return 1;
    if (_Custom.show_performance != ve_seen_performance) return 1;
    if (c->chat_interface_id != ve_seen_chat || c->sticky_chat_interface_id != ve_seen_sticky || c->viewport_interface_id != ve_seen_view) return 1;
    if (ve_script_dirty && !ve_popup_patch_guard) return 1;
    if (c->show_social_input != ve_seen_social || c->chatback_input_open != ve_seen_input_open) return 1;
    if (!ve_popup_patch_guard && c->chatback_input_open && strcmp(c->chatback_input, ve_seen_input) != 0) return 1;
    if (strcmp(c->modal_message, ve_seen_modal) != 0) return 1;
    if ((c->energy != ve_warm_energy && !(ve_native_markers()&&ui474_energy_patch_ready(c))) || c->selected_tab != ve_warm_tab || c->chat_scroll_offset != ve_warm_scroll || (c->varps && c->varps[173] != ve_warm_run)) return 1;
    return 0;
}

/* Routine skill rewards update inventory/stats and chat, not the world or
 * interface layout. Rasterize the original panels and update only those
 * texture rectangles; menus, dialogs and layout changes keep the full path. */
static int ve_patch_skill_panels(Client*c){
 if(!ve_enabled()||!ui474_enabled()||!ve_native_markers()||!ve_ui_warm||c->scene_state!=2||!c->area_viewport||!c->area_viewport->retained_valid)return 0;
 int side=c->redraw_sidebar,chat=c->redraw_chatback;
 if(!side&&!chat)return 0;
 if(c->viewport_interface_id>=0||c->sidebar_interface_id>=0||c->chat_interface_id>=0||c->sticky_chat_interface_id>=0||c->menu_visible||c->show_social_input||c->chatback_input_open||c->modal_message[0])return 0;
 const LcRect*sr=&lc_game_geometry(LC_SIDEBAR)->rect,*cr=&lc_game_geometry(LC_CHAT)->rect;
 const LcRect*rs[]={sr,cr};
 for(int i=0;i<2;i++){const LcRect*r=rs[i];if(r->x<0||r->y<0||r->w<=0||r->h<=0||r->x+r->w>c->area_viewport->width||r->y+r->h>c->area_viewport->height)return 0;}
 c->redraw_sidebar=c->redraw_chatback=false;
 if(ve_ui_dirty(c)){c->redraw_sidebar=side;c->redraw_chatback=chat;return 0;}
 uint64_t began=ba_timing_now();Pix2D saved=_Pix2D;
 /* This patch precedes client_draw_scene and its normal GL-to-software
  * handoff. Icon generation must rasterize into the 32x32 CPU sprite,
  * never submit its triangles to the world or cache an empty sprite. */
 bool saved_gl=_Custom.use_opengl11;_Custom.use_opengl11=false;
 for(int i=0;i<2;i++){
  if((i==0&&!side)||(i==1&&!chat))continue;const LcRect*r=rs[i];
  for(int y=r->y;y<r->y+r->h;y++)memset(c->area_viewport->pixels+y*c->area_viewport->width+r->x,0xff,(size_t)r->w*sizeof(int));
  if(i==0){ui474_draw_sidebar(c);ve_warm_energy=c->energy;}else{client_draw_chatback(c);ve_chat_draw_clean(c);}
  pixmap_flush_rect(c->area_viewport,r->x,r->y,r->w,r->h);
 }
 c->redraw_sidebar=c->redraw_chatback=false;c->area_viewport->dirty=false;_Pix2D=saved;_Custom.use_opengl11=saved_gl;
 char line[128];snprintf(line,sizeof line,"# panel_patch %llu sidebar %d chat %d us %llu",(unsigned long long)ba_timing_now(),side,chat,(unsigned long long)(ba_timing_now()-began));ba_cadence_diagnostic(line);
 return 1;
}

#include "../r19_popup_state.inc"
static uint64_t popup_chat_us,popup_compose_us,popup_clear_us,popup_flush_us,popup_state_us;

/* Stable popup layouts update their own original pixel rectangles. Input
 * and animation still run normally; exact pixel comparison suppresses only
 * unchanged uploads. Opening/closing/layout changes use the full UI path. */
static int ve_patch_popup_regions(Client*c,int paint){
 if(!ve_enabled()||!ve_native_markers()||!ui474_enabled()||!ve_ui_warm||c->scene_state!=2||
    !c->area_viewport||!c->area_viewport->retained_valid||ve_complex_overlay(c))return 0;
 int view=c->viewport_interface_id;
 if(view<0&&c->chat_interface_id<0&&c->sticky_chat_interface_id<0&&!c->chatback_input_open)return 0;
 if(view>=0&&(client_modal_offset(c,view,1)!=ve_seen_modal_x||client_modal_y(c,view,1)!=ve_seen_modal_y))return 0;
 if(view!=ve_seen_view||c->chat_interface_id!=ve_seen_chat||c->sticky_chat_interface_id!=ve_seen_sticky)return 0;
 if(c->obj_drag_area||c->show_social_input||c->modal_message[0])return 0;
 int side=c->redraw_sidebar,chat=c->redraw_chatback;
 c->redraw_sidebar=c->redraw_chatback=false;ve_popup_patch_guard=1;
 int other=ve_ui_dirty(c);ve_popup_patch_guard=0;
 c->redraw_sidebar=side;c->redraw_chatback=chat;if(other)return 0;
 const LcRect*sr=&lc_game_geometry(LC_SIDEBAR)->rect,*cr=&lc_game_geometry(LC_CHAT)->rect;
 LcRect modal={0};Component*root=NULL;
 if(view>=0){
  if(view>=_Component.count||!(root=_Component.instances[view]))return 0;
  modal=(LcRect){224+client_modal_offset(c,view,1),client_modal_y(c,view,1),root->width,root->height};
 }
 int drawchat=chat||c->chat_interface_id>=0||c->sticky_chat_interface_id>=0||c->chatback_input_open;
 const LcRect*rs[]={&modal,sr,cr};int active[]={view>=0,side,drawchat};
 /* Recompose overlapping retained panels, then paint the popup on top of
  * the minimized chat's transparent background. */
 if(view>=0)for(int i=1;i<3;i++){const LcRect*r=rs[i];
  if(modal.x<r->x+r->w&&modal.x+modal.w>r->x&&modal.y<r->y+r->h&&modal.y+modal.h>r->y)active[i]=1;
 }
 const LcRect*map=&lc_game_geometry(LC_MINIMAP)->rect;
 int map_overlap=view>=0&&modal.x<map->x+map->w&&modal.x+modal.w>map->x&&modal.y<map->y+map->h&&modal.y+modal.h>map->y;
 for(int i=0;i<3;i++)if(active[i]){const LcRect*r=rs[i];
  if(r->x<0||r->y<0||r->w<=0||r->h<=0||r->w>c->area_viewport->width-r->x||r->h>c->area_viewport->height-r->y)return 0;
 }
 if(!paint)return 1;
 uint64_t state_at=ba_timing_now();int snapshot_ok=popup_state_build(c);popup_state_us+=ba_timing_now()-state_at;
 int same=snapshot_ok&&popup_state_valid&&popup_before_n==popup_now_n&&!memcmp(popup_before,popup_now,popup_now_n);
 if(same&&!side&&!chat&&!ve_script_dirty){popup_reuses++;return 1;}
 popup_rasters++;popup_reason=side|(chat<<1)|(ve_script_dirty<<2)|((!snapshot_ok)<<3)|((!same)<<4);
 popup_diff_component=popup_diff_relative=-1;
 if(snapshot_ok&&popup_state_valid&&!same){size_t diff=0,n=popup_now_n<popup_before_n?popup_now_n:popup_before_n;
  while(diff<n&&popup_now[diff]==popup_before[diff])diff++;
  for(int i=0;i<popup_trace_n;i++)if(popup_trace_start[i]<=diff){popup_diff_component=popup_trace_id[i];popup_diff_relative=(int)(diff-popup_trace_start[i]);}
 }
 Pix2D saved=_Pix2D;bool saved_gl=_Custom.use_opengl11;int*saved_lines=_Pix3D.line_offset;
 _Custom.use_opengl11=false;pixmap_bind(c->area_viewport);_Pix3D.line_offset=c->area_viewport_offsets;
 for(int pass=0;pass<3;pass++){int i=2-pass;if(active[i]){const LcRect*r=rs[i];
  uint64_t at=ba_timing_now();
  for(int y=r->y;y<r->y+r->h;y++)memset(c->area_viewport->pixels+y*c->area_viewport->width+r->x,0xff,(size_t)r->w*sizeof(int));
  popup_clear_us+=ba_timing_now()-at;at=ba_timing_now();
  if(i==0)client_draw_interface(c,root,r->x,r->y,0);
  else if(i==1)ui474_draw_sidebar(c);
  else{client_draw_chatback(c);popup_chat_us+=ba_timing_now()-at;at=ba_timing_now();ve_chat_draw_clean(c);}
  popup_compose_us+=ba_timing_now()-at;
 }}
 if(map_overlap){ve_map_gl_hole_reset();ve_map_gl_prepare_cold(c);}
 uint64_t flush_at=ba_timing_now();for(int i=0;i<3;i++)if(active[i]){const LcRect*r=rs[i];pixmap_flush_changed_rect(c->area_viewport,r->x,r->y,r->w,r->h);}popup_flush_us+=ba_timing_now()-flush_at;
 c->area_viewport->dirty=false;_Pix2D=saved;_Pix3D.line_offset=saved_lines;_Custom.use_opengl11=saved_gl;
 c->redraw_sidebar=c->redraw_chatback=false;ve_script_dirty=0;
 state_at=ba_timing_now();popup_state_valid=popup_state_build(c);popup_state_us+=ba_timing_now()-state_at;if(popup_state_valid){memcpy(popup_before,popup_now,popup_now_n);popup_before_n=popup_now_n;}
 ve_warm_energy=c->energy;ve_warm_tab=c->selected_tab;ve_warm_scroll=c->chat_scroll_offset;ve_warm_run=c->varps?c->varps[173]:0;
 memcpy(ve_seen_input,c->chatback_input,CHATBACK_LENGTH+1);ve_seen_input[CHATBACK_LENGTH]=0;
 return 1;
}

static void ve_ui_commit(Client *c) {
    popup_state_valid=0;
    ve_ui_warm = c->scene_state == 2;
    ve_warm_energy = c->energy;
    ve_warm_tab = c->selected_tab;
    ve_warm_scroll = c->chat_scroll_offset;
    ve_warm_run = c->varps ? c->varps[173] : 0;
    ve_seen_view = c->viewport_interface_id;
    ve_seen_modal_x=client_modal_offset(c,ve_seen_view,1);ve_seen_modal_y=client_modal_y(c,ve_seen_view,1);
    ve_seen_chat = c->chat_interface_id;
    ve_seen_sticky = c->sticky_chat_interface_id;
    ve_seen_speech=ve_world_speech_active(c);
    ve_seen_menu = c->menu_visible;
    ve_seen_menu_baked = c->menu_visible&&!ve_menu_separate(c);
    ve_seen_performance = _Custom.show_performance;
    ve_seen_social = c->show_social_input;
    ve_seen_input_open = c->chatback_input_open;
    memcpy(ve_seen_modal, c->modal_message, MAX_STR);
    ve_seen_modal[MAX_STR - 1] = 0;
    memcpy(ve_seen_input, c->chatback_input, CHATBACK_LENGTH + 1);
    ve_seen_input[CHATBACK_LENGTH] = 0;
    ve_script_dirty = 0;
    c->redraw_sidebar = c->redraw_chatback = c->redraw_sideicons = c->redraw_privacy_settings = false;
    ve_map_ready = 0;
    ve_overlay_ready = 0;
}

void client_draw_game(Client *c) {
#ifdef GL11
    extern int recovery_water_reference;
    extern int recovery_reference_yaw;
    extern int recovery_reference_pitch;
    extern int r10_camera_replay(Client*);
    int r10_replay=!h8_is_running()&&r10_camera_replay(c);
    if(recovery_water_reference>=0&&!h8_is_running()&&!r10_replay){
        // Reference captures only. No player, NPC, quest or server timing changes.
        c->orbit_camera_yaw=recovery_reference_yaw;c->orbit_camera_pitch=recovery_reference_pitch;
        c->camera_anticheat_offset_x=c->camera_anticheat_offset_z=c->camera_anticheat_angle=0;
        c->minimap_anticheat_angle=c->minimap_zoom=0;
    }
#endif
    ui474_begin(c);
    ve_prepare(c);
    // Both themes consume the same elapsed logic steps exactly once per draw.
    // Enhanced returns below, so animation advancement must precede that branch.
    if (c->sidebar_interface_id != -1 &&
        client_update_interface_animation(c, c->sidebar_interface_id, c->scene_delta)) {
        c->redraw_sidebar = true;
    }
    if (c->chat_interface_id != -1 &&
        client_update_interface_animation(c, c->chat_interface_id, c->scene_delta)) {
        c->redraw_chatback = true;
    }
    ve_anim_advanced = 0;
    ve_input_advanced = 0;
    if (c->viewport_interface_id != -1) {
        ve_anim_advanced = 1;
        client_update_interface_animation(c, c->viewport_interface_id, c->scene_delta);
    }
    if(ve_enabled()){
      uint64_t begin=rs2_now();
      ve_menu_prepare(c);
      ve_patch_skill_panels(c);
      ve_popup_frame=ve_patch_popup_regions(c,0);
      int ve_hold=c->scene_state==2 && !ve_ui_dirty(c);
      if(h5_mode()=='U'||h5_mode()=='M'){pixmap_bind(c->area_viewport);pix2d_clear();_Custom.use_opengl11=false;ve_hold=0;}
      else if(c->scene_state==2)client_draw_scene(c);else pixmap_draw(c->area_viewport,0,0);
      uint64_t world_done=rs2_now();
      if(!ve_hold||ve_popup_frame){
        if(ve_patch_popup_regions(c,1))ve_hold=1;
        else if(ve_popup_frame){pixmap_bind(c->area_viewport);pix2d_clear();drawTileHint(c);draw3DEntityElements(c);}
      }
      dp_switch(DP_CHAT);
      if(c->chat_interface_id<0&&!ve_chat_hidden(c)){c->chat_interface->scrollPosition=c->chat_scroll_height-c->chat_scroll_offset-77;
       if(c->shell->mouse_x>=ve_chat_text_x()+lc_chat_text_right(CHAT_TEXT_RIGHT)&&c->shell->mouse_x<ve_chat_text_x()+650&&c->shell->mouse_y>=ve_chat_text_y()&&c->shell->mouse_y<ve_chat_text_y()+77)client_handle_scroll_input(c,c->shell->mouse_x-ve_chat_text_x(),c->shell->mouse_y-ve_chat_text_y(),c->chat_scroll_height,77,false,lc_chat_text_right(CHAT_TEXT_RIGHT),0,c->chat_interface);
       int offset=c->chat_scroll_height-c->chat_interface->scrollPosition-77;if(offset<0)offset=0;if(offset>c->chat_scroll_height-77)offset=c->chat_scroll_height-77;c->chat_scroll_offset=offset;}
      int h3_full=(h3_mode()=='G'||h3_mode()=='H'||h3_mode()=='A')&&h5_mode()!='M';
      int pixels_changed=!ve_hold;
      if(ve_hold&&c->energy!=ve_warm_energy&&ui474_energy_patch(c))ve_warm_energy=c->energy;
      if(!ve_hold&&!r16_diag_verbose()){
        char line[224];snprintf(line,sizeof line,"# cold_ui %llu energy %d/%d run %d/%d sidebar %d chat %d icons %d privacy %d script %d menu %d/%d view %d",(unsigned long long)ba_timing_now(),c->energy,ve_warm_energy,c->varps?c->varps[173]:0,ve_warm_run,c->redraw_sidebar,c->redraw_chatback,c->redraw_sideicons,c->redraw_privacy_settings,ve_script_dirty,c->menu_visible,ve_seen_menu,c->viewport_interface_id);ba_cadence_diagnostic(line);
      }
      if(ve_hold){
        int ax=c->local_player?c->local_player->pathing_entity.x/32:0;
        int az=c->local_player?c->local_player->pathing_entity.z/32:0;
        int iface=c->viewport_interface_id>=0||c->chat_interface_id>=0||c->sticky_chat_interface_id>=0;
        int look=!ve_overlay_ready||ve_overlay_yaw!=c->orbit_camera_yaw||ve_overlay_pitch!=c->orbit_camera_pitch||ve_overlay_x!=ax||ve_overlay_z!=az;
        int overlay_drew=0;
        if(look&&!iface&&(!ve_native_markers()||ve_complex_overlay(c))){
          dp_switch(DP_OVCLEAR);
          ve_clear_world(c);
          dp_switch(DP_OV2D);
          if(!ve_native_markers())draw2DEntityElements(c);
          dp_switch(DP_OVHINT);
          drawTileHint(c);
          dp_switch(DP_OV3D);
          draw3DEntityElements(c);
          ve_overlay_yaw=c->orbit_camera_yaw;ve_overlay_pitch=c->orbit_camera_pitch;ve_overlay_x=ax;ve_overlay_z=az;ve_overlay_ready=1;
          pixels_changed=1;
          overlay_drew=1;
          dp_mark_overlay();
        }
        /* The rotating map is drawn in GL after the viewport quad. Punch a
         * transparent hole once so the world shows around the fan. */
        if((h3_full||h3_mode()=='F'||h5_mode()=='M')&&h5_mode()!='U'){
          dp_switch(DP_MINIMAP);
          ve_map_gl_prepare(c);
        }
        ve_map_yaw=c->orbit_camera_yaw;ve_map_x=ax;ve_map_z=az;ve_map_ready=1;
      }else{
      ve_map_gl_hole_reset();
      ve_hud_gl_reset();
      dp_switch(DP_COLDUI);
      if(h3_full){H3_SCOPE(H3_CHAT);client_draw_chatback(c);}
      if(!ve_native_markers()&&(h3_full||h3_mode()=='F'||h5_mode()=='M')&&h5_mode()!='U'&&c->scene_state==2){H3_SCOPE(H3_MINIMAP);client_draw_minimap(c);}
      if(h3_full){H3_SCOPE(H3_UI);
        const LcRect*r=&lc_game_geometry(LC_CHAT)->rect;
        if(ve_native_markers()&&c->viewport_interface_id<0&&!ve_complex_overlay(c)&&r->x>=0&&r->y>=0&&r->w>0&&r->h>0&&r->w<=c->area_viewport->width-r->x&&r->h<=c->area_viewport->height-r->y){
          ui474_draw_sidebar(c);
          for(int y=r->y;y<r->y+r->h;y++)memset(c->area_viewport->pixels+(size_t)y*c->area_viewport->width+r->x,0xff,(size_t)r->w*sizeof(int));
          ve_chat_draw_clean(c);
        }else{
          ui474_draw(c);
          /* With chat minimized the popup can occupy its old rectangle.
           * Compose it after the hidden-chat background, just as the
           * retained-region path does, so no transparent cutout remains. */
          if(ve_chat_hidden(c)&&c->viewport_interface_id>=0&&c->viewport_interface_id<_Component.count){
            Component*root=_Component.instances[c->viewport_interface_id];
            if(root){pixmap_bind(c->area_viewport);_Pix3D.line_offset=c->area_viewport_offsets;
              client_draw_interface(c,root,224+client_modal_offset(c,root->id,1),client_modal_y(c,root->id,1),0);}
          }
        }
      }
      if((h3_full||h3_mode()=='F'||h5_mode()=='M')&&h5_mode()!='U'){H3_SCOPE(H3_MINIMAP);if(ve_native_markers())ve_map_gl_prepare_cold(c);else ve_map_draw(c);}
      ve_ui_commit(c);
      }
      dp_switch(DP_HUD);
      ve_draw_location_status(c);
      ve_draw_barrows_progress(c);
      {char h4_lines[3][128];h4_hud_lines(h4_lines);
       /* Compose the diagnostic text under interface panels. No destructive
        * transparent rectangle is needed in the retained UI texture. */
       if (_Custom.show_performance) {
           ve_hud_gl_update(c,h4_lines);ve_hud_gl_draw();
           Pix2D saved=_Pix2D;PixMap*p=ve_effect_surface(MAX_CHATS+9,256,96);
           if(p){draw_info_overlay_local(c,252,-21);ve_effect_finish(p,saved,PERF_RIGHT(507)-252,PERF_BASELINE(52)-18);}
           else _Pix2D=saved;
       }
       if(h8_is_armed()&&!h8_is_running()){pixmap_bind(c->area_viewport);drawString(c->font_plain11,12,100,"Remote armed - SELECT stops",YELLOW);pixels_changed=1;}}
      if(!pixels_changed&&c->area_viewport)c->area_viewport->dirty=false;
      {H3_SCOPE(H3_OVERLAY);pixmap_draw(c->area_viewport,0,0);pixmap_bind(c->area_viewport);_Pix3D.line_offset=c->area_viewport_offsets;}
      if((ve_hold||ve_native_markers())&&(h3_full||h3_mode()=='F'||h5_mode()=='M')&&h5_mode()!='U'){dp_switch(DP_MINIMAP);ve_map_gl_draw(c);}
      ve_menu_draw(c);
      dp_switch(DP_HOLDIO);
      {static unsigned hold_n,cold_n;static uint64_t logged;if(ve_hold)hold_n++;else cold_n++;if(r16_diag_verbose()){uint64_t stamp=rs2_now();if(stamp-logged>=2000ull){logged=stamp;uint64_t io0=ba_timing_now();FILE*f=fopen("ux0:/data/RS2Vita/ui-hold.txt","w");if(f){fprintf(f,"hold=%u cold=%u view=%d chat=%d sticky=%d\n",hold_n,cold_n,c->viewport_interface_id,c->chat_interface_id,c->sticky_chat_interface_id);fclose(f);}r16_io_add(0,ba_timing_now()-io0);}}}
      static unsigned frames=0;static unsigned long long world_ms=0,ui_ms=0;world_ms+=world_done-begin;ui_ms+=rs2_now()-world_done;if(++frames%100==0&&getenv("CLASSIC_MIGRATION_TEST"))rs2_log("VITA_ENHANCED_TIMING frames=%u world_ms=%llu ui_ms=%llu viewport=960x544\n",frames,world_ms,ui_ms);
      return;
    }

    if (c->redraw_background) {
        c->redraw_background = false;
        pixmap_draw(c->area_backleft1, 0, 11);
        lc_draw(c->area_backleft2,LC_CHAT, lc_x(LC_CHAT,0), (ve_enabled()?ve_chat_text_y():lc_y(LC_CHAT,UI_BOTTOM(375))));
        lc_draw(c->area_backright1,LC_MINIMAP, lc_x(LC_MINIMAP,UI_RIGHT(729)), lc_y(LC_MINIMAP,5));
        lc_draw(c->area_backright2,LC_SIDEBAR, lc_x(LC_SIDEBAR,UI_RIGHT(752)), lc_y(LC_SIDEBAR,UI_BOTTOM(231)));
        pixmap_draw(c->area_backtop1, 0, 0);
        lc_draw(c->area_backtop2,LC_MINIMAP, lc_x(LC_MINIMAP,UI_RIGHT(561)), lc_y(LC_MINIMAP,0));
        pixmap_draw(c->area_backvmid1, UI_RIGHT(520), 11);
        lc_draw(c->area_backvmid2,LC_SIDEBAR, lc_x(LC_SIDEBAR,UI_RIGHT(520)), lc_y(LC_SIDEBAR,UI_BOTTOM(231)));
        lc_draw(c->area_backvmid3,LC_SIDEBAR, lc_x(LC_SIDEBAR,UI_RIGHT(501)), lc_y(LC_SIDEBAR,UI_BOTTOM(375)));
        lc_draw(c->area_backhmid2,LC_CHAT, lc_x(LC_CHAT,0), lc_y(LC_CHAT,UI_BOTTOM(345)));
        c->redraw_sidebar = true;
        c->redraw_chatback = true;
        c->redraw_sideicons = true;
        c->redraw_privacy_settings = true;
        if (c->scene_state != 2) {
            if(!ve_enabled())pixmap_draw(c->area_viewport, 8, 11);
            lc_draw(c->area_mapback,LC_MINIMAP, lc_x(LC_MINIMAP,UI_RIGHT(561)), lc_y(LC_MINIMAP,5));
        }
    }
#ifdef GL11
    else {
        pixmap_draw(c->area_backleft1, 0, 11);
        lc_draw(c->area_backleft2,LC_CHAT, lc_x(LC_CHAT,0), (ve_enabled()?ve_chat_text_y():lc_y(LC_CHAT,UI_BOTTOM(375))));
        lc_draw(c->area_backright1,LC_MINIMAP, lc_x(LC_MINIMAP,UI_RIGHT(729)), lc_y(LC_MINIMAP,5));
        lc_draw(c->area_backright2,LC_SIDEBAR, lc_x(LC_SIDEBAR,UI_RIGHT(752)), lc_y(LC_SIDEBAR,UI_BOTTOM(231)));
        pixmap_draw(c->area_backtop1, 0, 0);
        lc_draw(c->area_backtop2,LC_MINIMAP, lc_x(LC_MINIMAP,UI_RIGHT(561)), lc_y(LC_MINIMAP,0));
        pixmap_draw(c->area_backvmid1, UI_RIGHT(520), 11);
        lc_draw(c->area_backvmid2,LC_SIDEBAR, lc_x(LC_SIDEBAR,UI_RIGHT(520)), lc_y(LC_SIDEBAR,UI_BOTTOM(231)));
        lc_draw(c->area_backvmid3,LC_SIDEBAR, lc_x(LC_SIDEBAR,UI_RIGHT(501)), lc_y(LC_SIDEBAR,UI_BOTTOM(375)));
        lc_draw(c->area_backhmid2,LC_CHAT, lc_x(LC_CHAT,0), lc_y(LC_CHAT,UI_BOTTOM(345)));
        if (c->scene_state != 2) {
            if(!ve_enabled())pixmap_draw(c->area_viewport, 8, 11);
            lc_draw(c->area_mapback,LC_MINIMAP, lc_x(LC_MINIMAP,UI_RIGHT(561)), lc_y(LC_MINIMAP,5));
        }
    }
#endif

    if (c->scene_state == 2) {
        client_draw_scene(c);
    }
    if(lc_dx(LC_SIDEBAR)||lc_dy(LC_SIDEBAR)){
      c->redraw_sidebar=c->redraw_sideicons=c->redraw_chatback=c->redraw_privacy_settings=true;
      lc_draw(c->area_backvmid2,LC_SIDEBAR,lc_x(LC_SIDEBAR,UI_RIGHT(520)),lc_y(LC_SIDEBAR,UI_BOTTOM(231)));
      lc_draw(c->area_backvmid3,LC_SIDEBAR,lc_x(LC_SIDEBAR,UI_RIGHT(501)),lc_y(LC_SIDEBAR,UI_BOTTOM(375)));
      lc_draw(c->area_backright2,LC_SIDEBAR,lc_x(LC_SIDEBAR,UI_RIGHT(752)),lc_y(LC_SIDEBAR,UI_BOTTOM(231)));
      lc_draw(c->area_backright1,LC_MINIMAP,lc_x(LC_MINIMAP,UI_RIGHT(729)),lc_y(LC_MINIMAP,5));
      lc_draw(c->area_backtop2,LC_MINIMAP,lc_x(LC_MINIMAP,UI_RIGHT(561)),lc_y(LC_MINIMAP,0));
      lc_draw(c->area_backleft2,LC_CHAT,lc_x(LC_CHAT,0),(ve_enabled()?ve_chat_text_y():lc_y(LC_CHAT,UI_BOTTOM(375))));
      lc_draw(c->area_backhmid2,LC_CHAT,lc_x(LC_CHAT,0),lc_y(LC_CHAT,UI_BOTTOM(345)));
    }




    if (c->menu_visible && c->menu_area == 1) {
        c->redraw_sidebar = true;
    }

    if (c->selected_area == 2) {
        c->redraw_sidebar = true;
    }

    if (c->obj_drag_area == 2) {
        c->redraw_sidebar = true;
    }

    if (c->redraw_sidebar) {
        client_draw_sidebar(c);
        c->redraw_sidebar = false;
    }
#ifdef GL11
    else {
        lc_draw(c->area_sidebar,LC_SIDEBAR, ui474_side_x(), ui474_side_y());
    }
#endif

    if (c->chat_interface_id == -1) {
        c->chat_interface->scrollPosition = c->chat_scroll_height - c->chat_scroll_offset - 77;
        if (c->shell->mouse_x >= 22 + lc_chat_text_right(CHAT_TEXT_RIGHT) && c->shell->mouse_x < 22 + lc_chat_text_right(CHAT_TEXT_RIGHT) + 16 && c->shell->mouse_y >= (ve_enabled()?ve_chat_text_y():lc_y(LC_CHAT,UI_BOTTOM(375))) && c->shell->mouse_y < lc_y(LC_CHAT,UI_BOTTOM(471))) {
            client_handle_scroll_input(c, c->shell->mouse_x - (ve_enabled()?ve_chat_text_x():22), c->shell->mouse_y - (ve_enabled()?ve_chat_text_y():lc_y(LC_CHAT,UI_BOTTOM(375))), c->chat_scroll_height, 77, false, lc_chat_text_right(CHAT_TEXT_RIGHT), 0, c->chat_interface);
        }

        int offset = c->chat_scroll_height - c->chat_interface->scrollPosition - 77;
        if (offset < 0) {
            offset = 0;
        }

        if (offset > c->chat_scroll_height - 77) {
            offset = c->chat_scroll_height - 77;
        }

        if (c->chat_scroll_offset != offset) {
            c->chat_scroll_offset = offset;
            c->redraw_chatback = true;
        }
    }

    if (c->selected_area == 3) {
        c->redraw_chatback = true;
    }

    if (c->obj_drag_area == 3) {
        c->redraw_chatback = true;
    }

    if (c->modal_message[0]) {
        c->redraw_chatback = true;
    }

    if (c->menu_visible && c->menu_area == 2) {
        c->redraw_chatback = true;
    }

    if (c->redraw_chatback) {
        client_draw_chatback(c);
        c->redraw_chatback = false;
    }
#ifdef GL11
    else {
        if(!ve_enabled())lc_draw(c->area_chatback,LC_CHAT, (ve_enabled()?ve_chat_text_x():lc_x(LC_CHAT,22)), (ve_enabled()?ve_chat_text_y():lc_y(LC_CHAT,UI_BOTTOM(375))));
    }
#endif

    if (c->scene_state == 2) {
        client_draw_minimap(c);
        lc_draw(c->area_mapback,LC_MINIMAP, lc_x(LC_MINIMAP,UI_RIGHT(561)), lc_y(LC_MINIMAP,5));
    }

    if (c->flashing_tab != -1) {
        c->redraw_sideicons = true;
    }

    if (c->redraw_sideicons) {

        c->redraw_sideicons = false;
        pixmap_bind(c->area_backhmid1);
        pix8_draw(c->image_backhmid1, 0, 0);

        if (c->sidebar_interface_id == -1) {
            if (c->tab_interface_id[c->selected_tab] != -1) {
                if (c->selected_tab == 0) {
                    pix8_draw(c->image_redstone1, 29, UI_BOTTOM(30));
                } else if (c->selected_tab == 1) {
                    pix8_draw(c->image_redstone2, 59, UI_BOTTOM(29));
                } else if (c->selected_tab == 2) {
                    pix8_draw(c->image_redstone2, 87, UI_BOTTOM(29));
                } else if (c->selected_tab == 3) {
                    pix8_draw(c->image_redstone3, 115, UI_BOTTOM(29));
                } else if (c->selected_tab == 4) {
                    pix8_draw(c->image_redstone2h, 156, UI_BOTTOM(29));
                } else if (c->selected_tab == 5) {
                    pix8_draw(c->image_redstone2h, 184, UI_BOTTOM(29));
                } else if (c->selected_tab == 6) {
                    pix8_draw(c->image_redstone1h, 212, UI_BOTTOM(30));
                }
            }

            if (c->tab_interface_id[0] != -1 && (c->flashing_tab != 0 || _Client.loop_cycle % 20 < 10)) {
                pix8_draw(c->image_sideicons[0], 35, UI_BOTTOM(34));
            }

            if (c->tab_interface_id[1] != -1 && (c->flashing_tab != 1 || _Client.loop_cycle % 20 < 10)) {
                pix8_draw(c->image_sideicons[1], 59, UI_BOTTOM(32));
            }

            if (c->tab_interface_id[2] != -1 && (c->flashing_tab != 2 || _Client.loop_cycle % 20 < 10)) {
                pix8_draw(c->image_sideicons[2], 86, UI_BOTTOM(32));
            }

            if (c->tab_interface_id[3] != -1 && (c->flashing_tab != 3 || _Client.loop_cycle % 20 < 10)) {
                pix8_draw(c->image_sideicons[3], 121, UI_BOTTOM(33));
            }

            if (c->tab_interface_id[4] != -1 && (c->flashing_tab != 4 || _Client.loop_cycle % 20 < 10)) {
                pix8_draw(c->image_sideicons[4], 157, UI_BOTTOM(34));
            }

            if (c->tab_interface_id[5] != -1 && (c->flashing_tab != 5 || _Client.loop_cycle % 20 < 10)) {
                pix8_draw(c->image_sideicons[5], 185, UI_BOTTOM(32));
            }

            if (c->tab_interface_id[6] != -1 && (c->flashing_tab != 6 || _Client.loop_cycle % 20 < 10)) {
                pix8_draw(c->image_sideicons[6], 212, UI_BOTTOM(34));
            }
        }

        lc_draw(c->area_backhmid1,LC_SIDEBAR, lc_x(LC_SIDEBAR,UI_RIGHT(520)), lc_y(LC_SIDEBAR,165));
        pixmap_bind(c->area_backbase2);
        pix8_draw(c->image_backbase2, 0, 0);

        if (c->sidebar_interface_id == -1) {
            if (c->tab_interface_id[c->selected_tab] != -1) {
                if (c->selected_tab == 7) {
                    pix8_draw(c->image_redstone1v, 49, 0);
                } else if (c->selected_tab == 8) {
                    pix8_draw(c->image_redstone2v, 81, 0);
                } else if (c->selected_tab == 9) {
                    pix8_draw(c->image_redstone2v, 108, 0);
                } else if (c->selected_tab == 10) {
                    pix8_draw(c->image_redstone3v, 136, 1);
                } else if (c->selected_tab == 11) {
                    pix8_draw(c->image_redstone2hv, 178, 0);
                } else if (c->selected_tab == 12) {
                    pix8_draw(c->image_redstone2hv, 205, 0);
                } else if (c->selected_tab == 13) {
                    pix8_draw(c->image_redstone1hv, 233, 0);
                }
            }

            if (c->tab_interface_id[8] != -1 && (c->flashing_tab != 8 || _Client.loop_cycle % 20 < 10)) {
                pix8_draw(c->image_sideicons[7], 80, 2);
            }

            if (c->tab_interface_id[9] != -1 && (c->flashing_tab != 9 || _Client.loop_cycle % 20 < 10)) {
                pix8_draw(c->image_sideicons[8], 107, 3);
            }

            if (c->tab_interface_id[10] != -1 && (c->flashing_tab != 10 || _Client.loop_cycle % 20 < 10)) {
                pix8_draw(c->image_sideicons[9], 142, 4);
            }

            if (c->tab_interface_id[11] != -1 && (c->flashing_tab != 11 || _Client.loop_cycle % 20 < 10)) {
                pix8_draw(c->image_sideicons[10], 179, 2);
            }

            if (c->tab_interface_id[12] != -1 && (c->flashing_tab != 12 || _Client.loop_cycle % 20 < 10)) {
                pix8_draw(c->image_sideicons[11], 206, 2);
            }

            if (c->tab_interface_id[13] != -1 && (c->flashing_tab != 13 || _Client.loop_cycle % 20 < 10)) {
                pix8_draw(c->image_sideicons[12], 230, 2);
            }
        }
        lc_draw(c->area_backbase2,LC_SIDEBAR, lc_x(LC_SIDEBAR,UI_RIGHT(501)), lc_y(LC_SIDEBAR,UI_BOTTOM(492)));
        pixmap_bind(c->area_viewport);
    }
#ifdef GL11
    else {
        lc_draw(c->area_backhmid1,LC_SIDEBAR, lc_x(LC_SIDEBAR,UI_RIGHT(520)), lc_y(LC_SIDEBAR,165));
        lc_draw(c->area_backbase2,LC_SIDEBAR, lc_x(LC_SIDEBAR,UI_RIGHT(501)), lc_y(LC_SIDEBAR,UI_BOTTOM(492)));
    }
#endif


    if (c->redraw_privacy_settings) {
        c->redraw_privacy_settings = false;
        pixmap_bind(c->area_backbase1);
        pix8_draw(c->image_backbase1, 0, 0);

        drawStringTaggableCenter(c->font_plain12, "Public chat", CHAT_BUTTON_X(57, 0), 33, WHITE, true);
        if (c->public_chat_setting == 0) {
            drawStringTaggableCenter(c->font_plain12, "On", CHAT_BUTTON_X(57, 0), 46, GREEN, true);
        }
        if (c->public_chat_setting == 1) {
            drawStringTaggableCenter(c->font_plain12, "Friends", CHAT_BUTTON_X(57, 0), 46, YELLOW, true);
        }
        if (c->public_chat_setting == 2) {
            drawStringTaggableCenter(c->font_plain12, "Off", CHAT_BUTTON_X(57, 0), 46, RED, true);
        }
        if (c->public_chat_setting == 3) {
            drawStringTaggableCenter(c->font_plain12, "Hide", CHAT_BUTTON_X(57, 0), 46, CYAN, true);
        }

        drawStringTaggableCenter(c->font_plain12, "Private chat", CHAT_BUTTON_X(186, 1), 33, WHITE, true);
        if (c->private_chat_setting == 0) {
            drawStringTaggableCenter(c->font_plain12, "On", CHAT_BUTTON_X(186, 1), 46, GREEN, true);
        }
        if (c->private_chat_setting == 1) {
            drawStringTaggableCenter(c->font_plain12, "Friends", CHAT_BUTTON_X(186, 1), 46, YELLOW, true);
        }
        if (c->private_chat_setting == 2) {
            drawStringTaggableCenter(c->font_plain12, "Off", CHAT_BUTTON_X(186, 1), 46, RED, true);
        }

        drawStringTaggableCenter(c->font_plain12, "Trade/duel", CHAT_BUTTON_X(326, 2), 33, WHITE, true);
        if (c->trade_chat_setting == 0) {
            drawStringTaggableCenter(c->font_plain12, "On", CHAT_BUTTON_X(326, 2), 46, GREEN, true);
        }
        if (c->trade_chat_setting == 1) {
            drawStringTaggableCenter(c->font_plain12, "Friends", CHAT_BUTTON_X(326, 2), 46, YELLOW, true);
        }
        if (c->trade_chat_setting == 2) {
            drawStringTaggableCenter(c->font_plain12, "Off", CHAT_BUTTON_X(326, 2), 46, RED, true);
        }

        drawStringTaggableCenter(c->font_plain12, "Report abuse", CHAT_BUTTON_X(462, 3), 38, WHITE, true);
        lc_draw(c->area_backbase1,LC_CHAT, lc_x(LC_CHAT,0), lc_y(LC_CHAT,UI_BOTTOM(471)));
        pixmap_bind(c->area_viewport);
    }
#ifdef GL11
    else {
        lc_draw(c->area_backbase1,LC_CHAT, lc_x(LC_CHAT,0), lc_y(LC_CHAT,UI_BOTTOM(471)));
    }
#endif

    ui474_draw(c);
}

void client_handle_scroll_input(Client *c, int mouseX, int mouseY, int scrollableHeight, int height, bool redraw, int left, int top, Component *component) {
    int previous_scroll=component->scrollPosition;
    if (c->scrollGrabbed) {
        c->scrollInputPadding = 32;
    } else {
        c->scrollInputPadding = 0;
    }

    c->scrollGrabbed = false;

    if (mouseX >= left && mouseX < left + 16 && mouseY >= top && mouseY < top + 16) {
        component->scrollPosition -= c->drag_cycles * 4;
    } else if (mouseX >= left && mouseX < left + 16 && mouseY >= top + height - 16 && mouseY < top + height) {
        component->scrollPosition += c->drag_cycles * 4;
    } else if (mouseX >= left - c->scrollInputPadding && mouseX < left + c->scrollInputPadding + 16 && mouseY >= top + 16 && mouseY < top + height - 16 && c->drag_cycles > 0) {
        int gripSize = (height - 32) * height / scrollableHeight;
        if (gripSize < 8) {
            gripSize = 8;
        }
        int gripY = mouseY - top - gripSize / 2 - 16;
        int maxY = height - gripSize - 32;
        component->scrollPosition = (scrollableHeight - height) * gripY / maxY;
        c->scrollGrabbed = true;
    }
    int max_scroll=scrollableHeight-height;if(max_scroll<0)max_scroll=0;
    if(component->scrollPosition<0)component->scrollPosition=0;
    if(component->scrollPosition>max_scroll)component->scrollPosition=max_scroll;
    if(redraw&&component->scrollPosition!=previous_scroll)c->redraw_sidebar=true;

}

bool client_update_interface_animation(Client *c, int id, int delta) {
    bool updated = false;
    Component *parent = _Component.instances[id];
    for (int i = 0; i < parent->childCount && parent->childId[i] != -1; i++) {
        Component *child = _Component.instances[parent->childId[i]];
        if (child->type == 1) {
            updated |= client_update_interface_animation(c, child->id, delta);
        }
        if (child->type == 6 && (child->anim != -1 || child->activeAnim != -1)) {
            bool active = client_execute_interface_script(c, child);
            int seqId;
            if (active) {
                seqId = child->activeAnim;
            } else {
                seqId = child->anim;
            }
            if (seqId != -1) {
                SeqType *type = _SeqType.instances[seqId];
                child->seqCycle += delta;
                while (child->seqCycle > type->delay[child->seqFrame]) {
                    child->seqCycle -= type->delay[child->seqFrame] + 1;
                    child->seqFrame++;
                    if (child->seqFrame >= type->frameCount) {
                        child->seqFrame -= type->replayoff;
                        if (child->seqFrame < 0 || child->seqFrame >= type->frameCount) {
                            child->seqFrame = 0;
                        }
                    }
                    updated = true;
                }
            }
        }
    }
    return updated;
}

bool client_execute_interface_script(Client *c, Component *com) {
    if (!com->scriptComparator) {
        return false;
    }

    for (int i = 0; i < com->comparatorCount; i++) {
        int value = client_execute_clientscript1(c, com, i);
        int operand = com->scriptOperand[i];

        if (com->scriptComparator[i] == 2) {
            if (value >= operand) {
                return false;
            }
        } else if (com->scriptComparator[i] == 3) {
            if (value <= operand) {
                return false;
            }
        } else if (com->scriptComparator[i] == 4) {
            if (value == operand) {
                return false;
            }
        } else if (value != operand) {
            return false;
        }
    }

    return true;
}

int client_execute_clientscript1(Client *c, Component *component, int scriptId) {
    if (!component->scripts || scriptId >= component->scriptCount) {
        return -2;
    }

    // try {
    int *script = component->scripts[scriptId];
    int _register = 0;
    int pc = 0;

    while (true) {
        int opcode = script[pc++];
        if (opcode == 0) {
            return _register;
        }

        if (opcode == 1) { // load_skill_level {skill}
            _register += c->skillLevel[script[pc++]];
        } else if (opcode == 2) { // load_skill_base_level {skill}
            _register += c->skillBaseLevel[script[pc++]];
        } else if (opcode == 3) { // load_skill_exp {skill}
            _register += c->skillExperience[script[pc++]];
        } else if (opcode == 4) { // load_inv_count {interface id} {obj id}
            Component *com = _Component.instances[script[pc++]];
            int obj = script[pc++] + 1;

            for (int i = 0; i < com->width * com->height; i++) {
                if (com->invSlotObjId[i] == obj) {
                    _register += com->invSlotObjCount[i];
                }
            }
        } else if (opcode == 5) { // load_var {id}
            _register += c->varps[script[pc++]];
        } else if (opcode == 6) { // load_next_level_xp {skill}
            _register += _Client.levelExperience[c->skillBaseLevel[script[pc++]] - 1];
        } else if (opcode == 7) {
            _register += c->varps[script[pc++]] * 100 / 46875;
        } else if (opcode == 8) { // load_combat_level
            _register += c->local_player->combatLevel;
        } else if (opcode == 9) { // load_total_level
            for (int i = 0; i < 19; i++) {
                if (i == 18) {
                    // runecrafting
                    i = 20;
                }

                _register += c->skillBaseLevel[i];
            }
        } else if (opcode == 10) { // load_inv_contains {interface id} {obj id}
            Component *com = _Component.instances[script[pc++]];
            int obj = script[pc++] + 1;

            for (int i = 0; i < com->width * com->height; i++) {
                if (com->invSlotObjId[i] == obj) {
                    _register += 999999999;
                    break;
                }
            }
        } else if (opcode == 11) { // load_energy
            _register += c->energy;
        } else if (opcode == 12) { // load_weight
            _register += c->weightCarried;
        } else if (opcode == 13) { // load_bool {varp} {bit: 0..31}
            int varp = c->varps[script[pc++]];
            int lsb = script[pc++];

            _register += (varp & 0x1 << lsb) == 0 ? 0 : 1;
        }
    }
    // } catch (@Pc(282) Exception ex) {
    // 	return -1;
    // }
}

void projectFromGround(Client *c, PathingEntity *entity, int height) {
    projectFromGround2(c, entity->x, height-r16_player_visual_offset(&entity->entity), entity->z);
}

void projectFromGround2(Client *c, int x, int height, int z) {
    if (x < 128 || z < 128 || x > 13056 || z > 13056) {
        c->projectX = -1;
        c->projectY = -1;
        return;
    }

    int y = getHeightmapY(c, c->currentLevel, x, z) - height;
    project(c, x, y, z);
}

void project(Client *c, int x, int y, int z) {
    int dx = x - c->cameraX;
    int dy = y - c->cameraY;
    int dz = z - c->cameraZ;

    int sinPitch = _Pix3D.sin_table[c->cameraPitch];
    int cosPitch = _Pix3D.cos_table[c->cameraPitch];
    int sinYaw = _Pix3D.sin_table[c->cameraYaw];
    int cosYaw = _Pix3D.cos_table[c->cameraYaw];

    int tmp = (dz * sinYaw + dx * cosYaw) >> 16;
    dz = (dz * cosYaw - dx * sinYaw) >> 16;
    dx = tmp;

    tmp = (dy * cosPitch - dz * sinPitch) >> 16;
    dz = (dy * sinPitch + dz * cosPitch) >> 16;
    dy = tmp;

    if (dz >= 50) {
        c->projectX = _Pix3D.center_x + ve_screen((dx << 9) / dz);
        c->projectY = _Pix3D.center_y + ve_screen((dy << 9) / dz);
    } else {
        c->projectX = -1;
        c->projectY = -1;
    }
}

static int ve_native_markers(void) {
#if defined(__vita__) && defined(GL11) && defined(VITA_NATIVE_WIDESCREEN)
    return ve_enabled() && h4_world_enabled() && !h5_no_init();
#else
    return 0;
#endif
}

/* Opt-in private diagnostic; removing the flag disables it for this run.
 * Fixed output names only, no remote command interpreter or save edits. */
static void ve_ui_probe(Client *c) {
#if defined(__vita__) && defined(GL11)
    static int enabled = -1, last_shot, last_flush;
    static uint64_t last;
    if (enabled < 0) {
        enabled = 0; FILE *f = fopen("ux0:/data/RS2Vita/ui-probe.txt","r");
        if (f) { fscanf(f,"%d",&enabled); fclose(f); }
    }
    if (!enabled) {ba_agility_probe_enabled=0;return;}
    uint64_t now = rs2_now(); if (now-last < 1000) return; last = now;
    FILE *f = fopen("ux0:/data/RS2Vita/ui-probe.txt","r");
    int active = 0, shot = 0, fixture_probe=0, fields=0; if (f) { fields=fscanf(f,"%d %d %d",&active,&shot,&fixture_probe); fclose(f); }
    /* FTP refresh may briefly expose an empty request; only an explicit zero disables this opt-in probe. */
    if(fields<1){ba_agility_probe_enabled=0;return;}
    if (!active) { enabled = 0;ba_agility_probe_enabled=0;return;}
    ba_agility_probe_enabled=fixture_probe==5;
#ifdef BUILD_A_EMBEDDED
    if(shot<0&&shot!=last_flush){
        extern void ba_cadence_probe_flush(void);
        ba_cadence_probe_flush();last_flush=shot;
        /* One explicit, opt-in geometry recapture request; no normal-play polling. */
        if(fixture_probe==4)r16_dirty();
    }
#endif
    int bx = -1, by = -1, hx = -1, hy = -1;
    if (c->hint_type == 1 && c->hint_npc >= 0 && c->hint_npc < MAX_NPC_COUNT && c->npcs[c->hint_npc]) {
        PathingEntity *npc = &c->npcs[c->hint_npc]->pathing_entity;
        projectFromGround(c,npc,npc->height/2); bx = c->projectX; by = c->projectY;
        projectFromGround(c,npc,npc->height+15); hx = c->projectX; hy = c->projectY;
    }
    if (shot > 0 && shot != last_shot) { last_shot = shot; r13_request_snapshot_token(shot); }
    f = fopen("ux0:/data/RS2Vita/ui-observation.txt","w");
    if (f) {
        fprintf(f,"ingame=%d hint_type=%d hint_npc=%d body_x=%d body_y=%d head_x=%d head_y=%d mouse_x=%d mouse_y=%d picked=%d menu_size=%d view=%d chat=%d sticky=%d cross_mode=%d cross_cycle=%d shot=%d\n",
            c->ingame,c->hint_type,c->hint_npc,bx,by,hx,hy,c->shell->mouse_x,c->shell->mouse_y,_Model.picked_count,c->menu_size,c->viewport_interface_id,c->chat_interface_id,c->sticky_chat_interface_id,c->cross_mode,c->cross_cycle,last_shot);
        fprintf(f,"title_state=%d profile=%d field=%d username_length=%u new_player=%d\n",c->title_screen_state,title_theme_profile(),c->title_login_field,(unsigned)strlen(c->username),title_new_player);
        {extern const char*platform_midi_current_track(void);fprintf(f,"music requested=%s loaded=%s enabled=%d delay=%d\n",c->currentMidi,platform_midi_current_track(),c->midiActive,c->nextMusicDelay);}
        {extern void platform_midi_loop_status(unsigned out[3]);unsigned a[3];platform_midi_loop_status(a);fprintf(f,"music_loop repeats=%u enabled=%u duration_ms=%u\n",a[0],a[1],a[2]);}
        {extern void r101_bloodworm_status(unsigned out[4]);unsigned a[4];r101_bloodworm_status(a);fprintf(f,"bloodworm_ground samples=%u adjusted=%u max_lift=%u penetrations=%u\n",a[0],a[1],a[2],a[3]);}
        fprintf(f,"probe_projection=%d,%d,%d|%d,%d|%d,%d\n",c->cameraX,c->cameraY,c->cameraZ,c->cameraYaw,c->cameraPitch,_Pix3D.center_x,_Pix3D.center_y);
        {extern void r16_door_probe_status(int out[3]);int status[3];r16_door_probe_status(status);fprintf(f,"door_probe=%d,%d,%d\n",status[0],status[1],status[2]);}
        fprintf(f,"probe_camera=%d,%d,%d yaw=%d pitch=%d\n",_World3D.eyeX,_World3D.eyeY,_World3D.eyeZ,c->cameraYaw,c->cameraPitch);
        fprintf(f,"tutorial flashing=%d selected=%d tick=%d\n",c->flashing_tab,c->selected_tab,_Client.loop_cycle);
        char snapshot_path[256];const char*meta=h8_boot_path("ux0:/data/RS2Vita/minimap-shot.csv",snapshot_path,sizeof snapshot_path);
        if(meta)fprintf(f,"snapshot_meta=%s\n",meta);
        if(ba_agility_probe_enabled&&_SeqType.count>839&&_SeqType.instances[839]){SeqType*q=_SeqType.instances[839];int total=0;for(int i=0;i<q->frameCount;i++)total+=q->delay[i];fprintf(f,"agility_reference seq=839 frames=%d delays=%d first=%d last=%d\n",q->frameCount,total,q->frames[0],q->frames[q->frameCount-1]);}

        {extern int platform_keyboard_is_open(void);fprintf(f,"keyboard_open=%d\n",platform_keyboard_is_open());}
        if(c->local_player){PathingEntity*p=&c->local_player->pathing_entity;fprintf(f,"skill_animation seq=%d frame=%d cycle=%d delay=%d\n",p->primarySeqId,p->primarySeqFrame,p->primarySeqCycle,p->primarySeqDelay);}
        fprintf(f,"xp_rate=%d\n",c->varps[323]);
        {extern void r96_door_status(unsigned out[2]);unsigned counts[2];r96_door_status(counts);fprintf(f,"door_pose_transitions=%u door_animation_models=%u\n",counts[0],counts[1]);}
        fprintf(f,"zone_status wilderness=%d arena=%d multi=%d\n",c->wildernessLevel,c->worldLocationState,c->in_multizone);
        fprintf(f,"barrows_progress count=%d mask=%u\n",barrows_progress_count(c),c->varps?(unsigned)c->varps[R105_BARROWS_DEAD_VARP]&63u:0u);
        fprintf(f,"scenery_speed rate=0.8 ambient_in=%llu ambient_out=%llu action_in=%llu action_out=%llu water_phase=%.6f\n",r106_ambient_input,r106_ambient_output,r106_action_input,r106_action_output,r106_water_offset(rs2_now()));
        fprintf(f,"probe_time_us=%llu cadence_flush=%d\n",(unsigned long long)ba_timing_now(),last_flush);
        fprintf(f,"world_bakes=%u roof_cache_swaps=%u\n",r16_bake_count(),r16_roof_swaps());
        fprintf(f,"hull_layers replayed_triangles=%u\n",r16_hull_layers());
        fprintf(f,"body_layers replayed_triangles=%u\n",r16_body_layer_faces());
        fprintf(f,"audio_cost synth_calls=%llu synth_ms=%llu synth_max_ms=%llu play_calls=%llu play_ms=%llu play_max_ms=%llu\n",(unsigned long long)r94_audio_cost[0].calls,(unsigned long long)r94_audio_cost[0].total_ms,(unsigned long long)r94_audio_cost[0].max_ms,(unsigned long long)r94_audio_cost[1].calls,(unsigned long long)r94_audio_cost[1].total_ms,(unsigned long long)r94_audio_cost[1].max_ms);
        {extern void platform_wave_status(unsigned out[6]);unsigned a[6];platform_wave_status(a);fprintf(f,"wave_output attempts=%u started=%u busy=%u failed=%u completed=%u nonzero_samples=%u metadata_calls=%llu\n",a[0],a[1],a[2],a[3],a[4],a[5],(unsigned long long)ba_resource_metadata_calls());}
        for(int i=0;i<1000;i++)if(r108_sound_received[i])fprintf(f,"sound_event id=%d received=%u dispatched=%u\n",i,r108_sound_received[i],r108_sound_dispatched[i]);
        fprintf(f,"map_decode_cache hits=%llu misses=%llu evictions=%llu bytes=%lu peak=%lu budget=%u\n",(unsigned long long)r109_hits,(unsigned long long)r109_misses,(unsigned long long)r109_evictions,(unsigned long)r109_bytes,(unsigned long)r109_peak,R109_BUDGET);
        int crop_status[4];r16_crop_status(crop_status);fprintf(f,"crop_cache meshes=%d instances=%d source_vertices=%d gpu_bytes=%d\n",crop_status[0],crop_status[1],crop_status[2],crop_status[3]);
        fprintf(f,"fixture_contacts replayed_triangles=%u\n",r16_fixture_contacts());
        fprintf(f,"crane_mesh removed_duplicates=%u\n",r16_crane_duplicates());
        fprintf(f,"ship_rail replayed_triangles=%u\n",r16_ship_rail_faces());
        fprintf(f,"underground_seams clipped_models=%u\n",r16_underground_cuts());
        int opening_status[3];r16_opening_status(opening_status);fprintf(f,"floor_openings triangles=%d clipped_tiles=%d overflow=%d\n",opening_status[0],opening_status[1],opening_status[2]);
        int roof_status[8];r16_roof_status(roof_status);
        fprintf(f,"roof_focus selected=%d level=%d components=%d buffers=%d uploads=%d failed=%d revision=%d hidden_indices=%d\n",roof_status[0],roof_status[1],roof_status[2],roof_status[3],roof_status[4],roof_status[5],roof_status[6],roof_status[7]);
        if(!ve_enabled()){
            fprintf(f,"classic_surface=%d,%d raster=%d,%d\n",c->area_viewport?c->area_viewport->width:0,c->area_viewport?c->area_viewport->height:0,_Pix2D.width,_Pix2D.height);
            fputs("classic_cost",f);for(int i=0;i<DP_N;i++)fprintf(f," %s=%llu",dp_name[i],(unsigned long long)dp_frame_us[i]);fputc('\n',f);
        }
        fprintf(f,"context_visible=%d context_area=%d context_rect=%d,%d,%d,%d context_selected=%d context_overlay=%d\n",c->menu_visible,c->menu_area,c->menu_x,c->menu_y,c->menu_width,c->menu_height,c->vita_menu_selected,ve_menu_separate(c));
        fprintf(f,"popup_reuses=%u popup_rasters=%u popup_reason=%d popup_bytes=%u popup_diff=%d,%d\n",popup_reuses,popup_rasters,popup_reason,(unsigned)popup_now_n,popup_diff_component,popup_diff_relative);
        fprintf(f,"popup_cpu_us chat=%llu compose=%llu clear=%llu flush=%llu state=%llu\n",(unsigned long long)popup_chat_us,(unsigned long long)popup_compose_us,(unsigned long long)popup_clear_us,(unsigned long long)popup_flush_us,(unsigned long long)popup_state_us);
        fprintf(f,"effect_allocations=%u effect_resizes=%u effect_cpu_bytes=%u\n",ve_effect_allocations,ve_effect_resizes,(unsigned)ve_effect_bytes);
        for (int i = 0; i < c->menu_size && i < 20; i++) fprintf(f,"menu[%d]=%d|%s\n",i,c->menu_action[i],c->menu_option[i]);
        if(fixture_probe&&c->ingame&&c->scene_state==2&&c->local_player&&c->scene){
         int ax=c->local_player->pathing_entity.x/128,az=c->local_player->pathing_entity.z/128;
         fprintf(f,"scene_base=%d,%d player=%d,%d yaw=%d pitch=%d run=%d energy=%d map_angle=%d map_zoom=%d\n",c->sceneBaseTileX,c->sceneBaseTileZ,c->sceneBaseTileX+ax,c->sceneBaseTileZ+az,c->orbit_camera_yaw,c->orbit_camera_pitch,c->varps?c->varps[173]:0,c->energy,c->minimap_anticheat_angle,c->minimap_zoom);
         for(int x=ax-8;x<=ax+8;x++)for(int z=az-8;z<=az+8;z++){
          if(x<0||z<0||x>=c->scene->maxTileX||z>=c->scene->maxTileZ)continue;Ground*t=c->scene->levelTiles[c->currentLevel][x][z];if(!t)continue;
          if(t->wall&&r16_live_door(t->wall->bitset)){
           Wall*w=t->wall;int id=((unsigned)w->bitset>>14)&32767;LocType*type=loctype_get(id);Model*model=w->modelA?w->modelA:w->modelB;
           projectFromGround2(c,w->x,model?model->max_y/2:0,w->z);
           fprintf(f,"door=%d|%s|%d,%d|%d,%d\n",id,type->name?type->name:"",c->sceneBaseTileX+x,c->sceneBaseTileZ+z,c->projectX,c->projectY);
           fprintf(f,"door_geometry=%d|%d,%d|%d,%d,%d|%d,%d,%d\n",id,c->sceneBaseTileX+x,c->sceneBaseTileZ+z,w->x,w->y,w->z,model?model->face_count:0,model?model->max_y:0,model?model->min_y:0);
           if(model&&((id>=3851&&id<=3882)||(id>=3907&&id<4035))&&model->face_count>=498){
            for(int face=63;face<=64;face++){
             int a=model->face_indices_a[face],b=model->face_indices_b[face],d=model->face_indices_c[face];
             fprintf(f,"door_top=%d|%d,%d|%d|%d,%d,%d,%d|%d,%d,%d|%d,%d,%d|%d,%d,%d\n",id,c->sceneBaseTileX+x,c->sceneBaseTileZ+z,face,model->face_infos?model->face_infos[face]:0,model->face_color_a?model->face_color_a[face]:-1,model->face_color_b?model->face_color_b[face]:-1,model->face_color_c?model->face_color_c[face]:-1,model->vertices_x[a],model->vertices_y[a],model->vertices_z[a],model->vertices_x[b],model->vertices_y[b],model->vertices_z[b],model->vertices_x[d],model->vertices_y[d],model->vertices_z[d]);
            }
           }
          }
          for(int j=0;j<t->locCount;j++){
           Location*l=t->locs[j];if(!l||l->r10_temporary||!l->model||l->minSceneTileX!=x||l->minSceneTileZ!=z||!r16_live_location(l->bitset))continue;
           int id=((unsigned)l->bitset>>14)&32767;LocType*type=loctype_get(id);projectFromGround2(c,l->x,l->model->max_y/2,l->z);
           fprintf(f,"fixture=%d|%s|%d,%d|%d,%d\n",id,type->name?type->name:"",c->sceneBaseTileX+x,c->sceneBaseTileZ+z,c->projectX,c->projectY);
          }
         }
         for(int i=0;i<c->npc_count;i++){
          int slot=c->npc_ids[i];NpcEntity*n=c->npcs[slot];if(!n||!n->type)continue;
          projectFromGround(c,&n->pathing_entity,n->pathing_entity.height/2);
          fprintf(f,"npc=%d|%lld|%s|%d,%d|%d,%d\n",slot,(long long)n->type->index,n->type->name?n->type->name:"",c->sceneBaseTileX+n->pathing_entity.x/128,c->sceneBaseTileZ+n->pathing_entity.z/128,c->projectX,c->projectY);
          fprintf(f,"npc_facing=%d|%lld|%d,%d|%d,%d|%d,%d|%d\n",slot,(long long)n->type->index,n->pathing_entity.yaw,n->pathing_entity.dstYaw,n->pathing_entity.lastFaceX,n->pathing_entity.lastFaceZ,n->pathing_entity.targetTileX,n->pathing_entity.targetTileZ,n->pathing_entity.targetId);
         }
         for(int i=0;i<5;i++)fprintf(f,"message=%d|%s\n",i,c->message_text[i]);
         Component*inv=_Component.instances[3214];if(inv)for(int i=0;i<28;i++)fprintf(f,"inventory=%d|%d|%d\n",i,inv->invSlotObjId[i]-1,inv->invSlotObjCount[i]);
        }
        fclose(f);
    }
#else
    (void)c;
#endif
}

static void ve_draw_world_markers(Client *c) {
    if (!ve_native_markers()) return;
    if (c->cross_mode == 1 || c->cross_mode == 2) {
        int frame = c->cross_cycle/100 + (c->cross_mode == 2 ? 4 : 0);
        if (frame >= 0 && frame < 8) ve_world_sprite(c->image_crosses[frame],c->crossX-8,c->crossY-8);
    }
    if (_Client.loop_cycle % 20 >= 10) return;
    if (c->hint_type == 1 && c->hint_npc >= 0 && c->hint_npc < MAX_NPC_COUNT) {
        NpcEntity *npc = c->npcs[c->hint_npc];
        if (npc && pathingentity_is_visible(&npc->pathing_entity)) {
            projectFromGround(c,&npc->pathing_entity,npc->pathing_entity.height+15);
            if (c->projectX >= 0) ve_world_sprite(c->image_headicons[2],c->projectX-12,c->projectY-28);
        }
    } else if (c->hint_type == 2) {
        projectFromGround2(c,((c->hint_tile_x-c->sceneBaseTileX)<<7)+c->hint_offset_x,c->hint_height*2,
            ((c->hint_tile_z-c->sceneBaseTileZ)<<7)+c->hint_offset_z);
        if (c->projectX >= 0) ve_world_sprite(c->image_headicons[2],c->projectX-12,c->projectY-28);
    }
}

static void draw2DEntityElements(Client *c) {
    c->chatCount = 0;

    for (int index = -1; index < c->player_count + c->npc_count; index++) {
        PathingEntity *entity;
        if (index == -1) {
            entity = &c->local_player->pathing_entity;
        } else if (index < c->player_count) {
            entity = &c->players[c->player_ids[index]]->pathing_entity;
        } else {
            entity = &c->npcs[c->npc_ids[index - c->player_count]]->pathing_entity;
        }

        if (!entity || !pathingentity_is_visible(entity)) {
            continue;
        }

        // TODO
        // if (c->show_debug) {
        // 	// true tile overlay
        // 	if (entity.pathLength > 0 || entity.forceMoveEndCycle >= loopCycle || entity.forceMoveStartCycle > loopCycle) {
        // 		int halfUnit = 64 * entity.size;
        // 		c->drawTileOverlay(entity.pathTileX[0] * 128 + halfUnit, entity.pathTileZ[0] * 128 + halfUnit, c->currentLevel, entity.size, 0x00FFFF, false);
        // 	}

        // 	// local tile overlay
        // 	c->drawTileOverlay(entity.x, entity.z, c->currentLevel, entity.size, 0x666666, false);

        // 	int offsetY = 0;
        // 	c->projectFromGround(entity, entity.height + 30);

        // 	if (index < c->playerCount) {
        // 		// player debug
        // 		PlayerEntity player = (PlayerEntity) entity;

        // 		c->fontPlain11.drawStringCenter(c->projectX, c->projectY + offsetY, player.name, 0xffffff);
        // 		offsetY -= 15;

        // 		if (player.lastMask != -1 && loopCycle - player.lastMaskCycle < 30) {
        // 			if ((player.lastMask & 0x1) == 0x1) {
        // 				c->fontPlain11.drawStringCenter(c->projectX, c->projectY + offsetY, "Appearance Update", 0xffffff);
        // 				offsetY -= 15;
        // 			}

        // 			if ((player.lastMask & 0x2) == 0x2) {
        // 				c->fontPlain11.drawStringCenter(c->projectX, c->projectY + offsetY, "Play Seq: " + player.primarySeqId, 0xffffff);
        // 				offsetY -= 15;
        // 			}

        // 			if ((player.lastMask & 0x4) == 0x4) {
        // 				int target = player.targetId;
        // 				if (target > 32767) {
        // 					target -= 32768;
        // 				}
        // 				c->fontPlain11.drawStringCenter(c->projectX, c->projectY + offsetY, "Face Entity: " + target, 0xffffff);
        // 				offsetY -= 15;
        // 			}

        // 			if ((player.lastMask & 0x8) == 0x8) {
        // 				c->fontPlain11.drawStringCenter(c->projectX, c->projectY + offsetY, "Say", 0xffffff);
        // 				offsetY -= 15;
        // 			}

        // 			if ((player.lastMask & 0x10) == 0x10) {
        // 				c->fontPlain11.drawStringCenter(c->projectX, c->projectY + offsetY, "Hit: Type " + player.damageType + " Amount " + player.damage + " HP " + player.health + "/" + player.totalHealth, 0xffffff);
        // 				offsetY -= 15;
        // 			}

        // 			if ((player.lastMask & 0x20) == 0x20) {
        // 				c->fontPlain11.drawStringCenter(c->projectX, c->projectY + offsetY, "Face Coord: " + (player.lastFaceX / 2) + " " + (player.lastFaceZ / 2), 0xffffff);
        // 				offsetY -= 15;
        // 			}

        // 			if ((player.lastMask & 0x40) == 0x40) {
        // 				c->fontPlain11.drawStringCenter(c->projectX, c->projectY + offsetY, "Chat", 0xffffff);
        // 				offsetY -= 15;
        // 			}

        // 			if ((player.lastMask & 0x100) == 0x100) {
        // 				c->fontPlain11.drawStringCenter(c->projectX, c->projectY + offsetY, "Play Spotanim: " + player.spotanimId, 0xffffff);
        // 				offsetY -= 15;
        // 			}

        // 			if ((player.lastMask & 0x200) == 0x200) {
        // 				c->fontPlain11.drawStringCenter(c->projectX, c->projectY + offsetY, "Exact Move", 0xffffff);
        // 				offsetY -= 15;
        // 			}
        // 		}
        // 	} else {
        // 		// npc debug
        // 		NpcEntity npc = (NpcEntity) entity;

        // 		c->fontPlain11.drawStringCenter(c->projectX, c->projectY + offsetY, npc.type.name, 0xffffff);
        // 		offsetY -= 15;

        // 		if (npc.lastMask != -1 && loopCycle - npc.lastMaskCycle < 30) {
        // 			if ((npc.lastMask & 0x2) == 0x2) {
        // 				c->fontPlain11.drawStringCenter(c->projectX, c->projectY + offsetY, "Play Seq: " + npc.primarySeqId, 0xffffff);
        // 				offsetY -= 15;
        // 			}

        // 			if ((npc.lastMask & 0x4) == 0x4) {
        // 				int target = npc.targetId;
        // 				if (target > 32767) {
        // 					target -= 32768;
        // 				}
        // 				c->fontPlain11.drawStringCenter(c->projectX, c->projectY + offsetY, "Face Entity: " + target, 0xffffff);
        // 				offsetY -= 15;
        // 			}

        // 			if ((npc.lastMask & 0x8) == 0x8) {
        // 				c->fontPlain11.drawStringCenter(c->projectX, c->projectY + offsetY, "Say", 0xffffff);
        // 				offsetY -= 15;
        // 			}

        // 			if ((npc.lastMask & 0x10) == 0x10) {
        // 				c->fontPlain11.drawStringCenter(c->projectX, c->projectY + offsetY, "Hit: Type " + npc.damageType + " Amount " + npc.damage + " HP " + npc.health + "/" + npc.totalHealth, 0xffffff);
        // 				offsetY -= 15;
        // 			}

        // 			if ((npc.lastMask & 0x20) == 0x20) {
        // 				c->fontPlain11.drawStringCenter(c->projectX, c->projectY + offsetY, "Change Type: " + npc.type.index, 0xffffff);
        // 				offsetY -= 15;
        // 			}

        // 			if ((npc.lastMask & 0x40) == 0x40) {
        // 				c->fontPlain11.drawStringCenter(c->projectX, c->projectY + offsetY, "Play Spotanim: " + npc.spotanimId, 0xffffff);
        // 				offsetY -= 15;
        // 			}

        // 			if ((npc.lastMask & 0x80) == 0x80) {
        // 				c->fontPlain11.drawStringCenter(c->projectX, c->projectY + offsetY, "Face Coord: " + (npc.lastFaceX / 2) + " " + (npc.lastFaceZ / 2), 0xffffff);
        // 				offsetY -= 15;
        // 			}
        // 		}
        // 	}
        // }

        if (index < c->player_count) {
            int y = 30;

            PlayerEntity *player = (PlayerEntity *)entity;
            if (player->headicons != 0) {
                projectFromGround(c, entity, entity->height + 15);

                if (c->projectX > -1) {
                    for (int icon = 0; icon < 8; icon++) {
                        if ((player->headicons & 0x1 << icon) != 0) {
                            if(ve_native_markers())ve_world_sprite(c->image_headicons[icon], c->projectX - 12, c->projectY - y);else pix24_draw(c->image_headicons[icon], c->projectX - 12, c->projectY - y);
                            y -= 25;
                        }
                    }
                }
            }

            if (index >= 0 && c->hint_type == 10 && c->hint_player == c->player_ids[index]) {
                projectFromGround(c, entity, entity->height + 15);
                if (c->projectX > -1) {
                    if(ve_native_markers())ve_world_sprite(c->image_headicons[7], c->projectX - 12, c->projectY - y);else pix24_draw(c->image_headicons[7], c->projectX - 12, c->projectY - y);
                }
            }
        } else if (!ve_native_markers() && c->hint_type == 1 && c->hint_npc == c->npc_ids[index - c->player_count] && _Client.loop_cycle % 20 < 10) {
            projectFromGround(c, entity, entity->height + 15);
            if (c->projectX > -1) {
                pix24_draw(c->image_headicons[2], c->projectX - 12, c->projectY - 28);
            }
        }

        if (entity->chat[0] && (index >= c->player_count || c->public_chat_setting == 0 || c->public_chat_setting == 3 || (c->public_chat_setting == 1 && client_is_friend(c, ((PlayerEntity *)entity)->name)))) {
            projectFromGround(c, entity, entity->height);

            if (c->projectX > -1 && c->chatCount < MAX_CHATS) {
                c->chatWidth[c->chatCount] = stringWidth(c->font_bold12, entity->chat) / 2;
                c->chatHeight[c->chatCount] = c->font_bold12->height;
                c->chatX[c->chatCount] = c->projectX;
                c->chatY[c->chatCount] = c->projectY;

                c->chatColors[c->chatCount] = entity->chatColor;
                c->chatStyles[c->chatCount] = entity->chatStyle;
                c->chatTimers[c->chatCount] = entity->chatTimer;
                strcpy(c->chats[c->chatCount++], entity->chat);

                if (c->chatEffects == 0 && entity->chatStyle == 1) {
                    c->chatHeight[c->chatCount] += 10;
                    c->chatY[c->chatCount] += 5;
                }

                if (c->chatEffects == 0 && entity->chatStyle == 2) {
                    c->chatWidth[c->chatCount] = 60;
                }
            }
        }

        if (entity->combatCycle > _Client.loop_cycle + 100) {
            projectFromGround(c, entity, entity->height + 15);

            if (c->projectX > -1) {
                int w = entity->health * 30 / entity->totalHealth;
                if (w > 30) {
                    w = 30;
                }
                if(ve_native_markers()){ve_world_rect(c->projectX-15,c->projectY-3,w,5,GREEN);ve_world_rect(c->projectX-15+w,c->projectY-3,30-w,5,RED);}
                else {pix2d_fill_rect(c->projectX - 15, c->projectY - 3, GREEN, w, 5);pix2d_fill_rect(c->projectX - 15 + w, c->projectY - 3, RED, 30 - w, 5);}
            }
        }

        if (entity->combatCycle > _Client.loop_cycle + 330) {
            projectFromGround(c, entity, entity->height / 2);

            if (c->projectX > -1) {
                if(ve_native_markers())ve_world_sprite(c->image_hitmarks[entity->damageType], c->projectX - 12, c->projectY - 12);else pix24_draw(c->image_hitmarks[entity->damageType], c->projectX - 12, c->projectY - 12);
                int hit_x=c->projectX,hit_y=c->projectY;Pix2D hit_saved=_Pix2D;
                PixMap*hit_pm=ve_native_markers()?ve_effect_surface(MAX_CHATS+1+((index+1)%8),32,24):NULL;
                if(hit_pm){c->projectX=16;c->projectY=12;}
                char *damage = valueof(entity->damage);
                drawStringCenter(c->font_plain11, c->projectX, c->projectY + 4, damage, BLACK);
                drawStringCenter(c->font_plain11, c->projectX - 1, c->projectY + 3, damage, WHITE);
                free(damage);
                if(hit_pm){ve_effect_finish(hit_pm,hit_saved,hit_x-16,hit_y-12);c->projectX=hit_x;c->projectY=hit_y;}
            }
        }
    }

    // TODO
    // if (c->show_debug) {
    // 	for (int i = 0; i < c->userTileMarkers.length; i++) {
    // 		if (c->userTileMarkers[i] == null || c->userTileMarkers[i].level != c->currentLevel || c->userTileMarkers[i].x < 0 || c->userTileMarkers[i].z < 0 || c->userTileMarkers[i].x >= 104 || c->userTileMarkers[i].z >= 104) {
    // 			continue;
    // 		}

    // 		c->drawTileOverlay(c->userTileMarkers[i].x * 128 + 64, c->userTileMarkers[i].z * 128 + 64, c->userTileMarkers[i].level, 1, 0xffff00, false);
    // 	}
    // }

    for (int i = 0; i < c->chatCount; i++) {
        int x = c->chatX[i];
        int y = c->chatY[i];
        int padding = c->chatWidth[i];
        int height = c->chatHeight[i];
        bool sorting = true;
        while (sorting) {
            sorting = false;
            for (int j = 0; j < i; j++) {
                if (y + 2 > c->chatY[j] - c->chatHeight[j] && y - height < c->chatY[j] + 2 && x - padding < c->chatX[j] + c->chatWidth[j] && x + padding > c->chatX[j] - c->chatWidth[j] && c->chatY[j] - c->chatHeight[j] < y) {
                    y = c->chatY[j] - c->chatHeight[j];
                    sorting = true;
                }
            }
        }
        c->projectX = c->chatX[i];
        c->projectY = c->chatY[i] = y;
        const char *message = c->chats[i];
        int speech_x=c->projectX,speech_y=c->projectY;Pix2D speech_saved=_Pix2D;
        int speech_w=1024,speech_h=48;
        PixMap*speech_pm=ve_native_markers()?ve_effect_surface(i,speech_w,speech_h):NULL;
        if(speech_pm){c->projectX=speech_pm->width/2;c->projectY=height+12;}
        if (c->chatEffects == 0) {
            int color = YELLOW;
            if (c->chatColors[i] < 6) {
                color = CHAT_COLORS[c->chatColors[i]];
            }
            if (c->chatColors[i] == 6) {
                color = _Client.loop_cycle % 20 < 10 ? RED : YELLOW;
            }
            if (c->chatColors[i] == 7) {
                color = _Client.loop_cycle % 20 < 10 ? BLUE : CYAN;
            }
            if (c->chatColors[i] == 8) {
                color = _Client.loop_cycle % 20 < 10 ? 0xb000 : 0x80ff80;
            }
            if (c->chatColors[i] == 9) {
                int delta = 150 - c->chatTimers[i];
                if (delta < 50) {
                    color = delta * 1280 + RED;
                } else if (delta < 100) {
                    color = YELLOW - (delta - 50) * 327680;
                } else if (delta < 150) {
                    color = (delta - 100) * 5 + GREEN;
                }
            }
            if (c->chatColors[i] == 10) {
                int delta = 150 - c->chatTimers[i];
                if (delta < 50) {
                    color = delta * 5 + RED;
                } else if (delta < 100) {
                    color = MAGENTA - (delta - 50) * 327680;
                } else if (delta < 150) {
                    color = (delta - 100) * 327680 + BLUE - (delta - 100) * 5;
                }
            }
            if (c->chatColors[i] == 11) {
                int delta = 150 - c->chatTimers[i];
                if (delta < 50) {
                    color = WHITE - delta * 327685;
                } else if (delta < 100) {
                    color = (delta - 50) * 327685 + GREEN;
                } else if (delta < 150) {
                    color = WHITE - (delta - 100) * 327680;
                }
            }
            if (c->chatStyles[i] == 0) {
                drawStringCenter(c->font_bold12, c->projectX, c->projectY + 1, message, BLACK);
                drawStringCenter(c->font_bold12, c->projectX, c->projectY, message, color);
            }
            if (c->chatStyles[i] == 1) {
                drawCenteredWave(c->font_bold12, c->projectX, c->projectY + 1, message, BLACK, _Client.loop_cycle);
                drawCenteredWave(c->font_bold12, c->projectX, c->projectY, message, color, _Client.loop_cycle);
            }
            if (c->chatStyles[i] == 2) {
                int w = stringWidth(c->font_bold12, message);
                int offsetX = (150 - c->chatTimers[i]) * (w + 100) / 150;
                pix2d_set_clipping(GAME_VIEW_HEIGHT, c->projectX + 50, 0, c->projectX - 50);
                drawString(c->font_bold12, c->projectX + 50 - offsetX, c->projectY + 1, message, BLACK);
                drawString(c->font_bold12, c->projectX + 50 - offsetX, c->projectY, message, color);
                pix2d_reset_clipping();
            }
        } else {
            drawStringCenter(c->font_bold12, c->projectX, c->projectY + 1, message, BLACK);
            drawStringCenter(c->font_bold12, c->projectX, c->projectY, message, YELLOW);
        }
        if(speech_pm){ve_effect_finish(speech_pm,speech_saved,speech_x-speech_pm->width/2,speech_y-height-12);c->projectX=speech_x;c->projectY=speech_y;}
        else if(ve_native_markers())c->redraw_sidebar=true;
    }
    if(ve_native_markers())for(int i=c->chatCount;i<MAX_CHATS;i++)if(ve_effect_pm[i]){PixMap*p=ve_effect_pm[i];ve_effect_bytes-=(size_t)p->width*p->height*12;pixmap_free(p);ve_effect_pm[i]=NULL;}
}

static void drawTileHint(Client *c) {
    if (ve_native_markers()) return;
    if (c->hint_type != 2) {
        return;
    }

    projectFromGround2(c, ((c->hint_tile_x - c->sceneBaseTileX) << 7) + c->hint_offset_x, c->hint_height * 2, ((c->hint_tile_z - c->sceneBaseTileZ) << 7) + c->hint_offset_z);

    if (c->projectX > -1 && _Client.loop_cycle % 20 < 10) {
        pix24_draw(c->image_headicons[2], c->projectX - 12, c->projectY - 28);
    }
}

static void updateTextures(Client *c, int cycle) {
    ba_timing_record(BA_TEXTURE,0);
    if (!_Client.lowmem) {
        if (_Pix3D.textureCycle[17] >= cycle) {
            Pix8 *texture = _Pix3D.textures[17];
            int bottom = texture->width * texture->height - 1;
            int adjustment = texture->width * c->scene_delta * 2;

            int8_t *src = texture->pixels;
            int8_t *dst = c->textureBuffer;
            for (int i = 0; i <= bottom; i++) {
                dst[i] = src[i - adjustment & bottom];
            }

            texture->pixels = dst;
            c->textureBuffer = src;
            pix3d_push_texture(17);
        }

        if (_Pix3D.textureCycle[24] >= cycle) {
            Pix8 *texture = _Pix3D.textures[24];
            int bottom = texture->width * texture->height - 1;
            int adjustment = texture->width * c->scene_delta * 2;

            int8_t *src = texture->pixels;
            int8_t *dst = c->textureBuffer;
            for (int i = 0; i <= bottom; i++) {
                dst[i] = src[i - adjustment & bottom];
            }

            texture->pixels = dst;
            c->textureBuffer = src;
            pix3d_push_texture(24);
        }
        if (_Pix3D.textureCycle[50] >= cycle) {
            Pix8 *texture = _Pix3D.textures[50];
            int bottom = texture->width * texture->height - 1;
            int adjustment = texture->width * c->scene_delta * 2;

            int8_t *src = texture->pixels;
            int8_t *dst = c->textureBuffer;
            for (int i = 0; i <= bottom; i++) {
                dst[i] = src[i - adjustment & bottom];
            }

            texture->pixels = dst;
            c->textureBuffer = src;
            pix3d_push_texture(50);
        }

    }
}

static void drawPrivateMessages(Client *c) {
    if (c->split_private_chat == 0) {
        return;
    }

    PixFont *font = c->font_plain12;
    int lineOffset = 0;
    if (c->system_update_timer != 0) {
        lineOffset = 1;
    }

    for (int i = 0; i < 100; i++) {
        if (c->message_text[i][0] == '\0') {
            continue;
        }

        int type = c->message_type[i];
        int y;
        char buf[USERNAME_LENGTH + CHAT_LENGTH + 8];
        if ((type == 3 || type == 7) && (type == 7 || c->private_chat_setting == 0 || (c->private_chat_setting == 1 && client_is_friend(c, c->message_sender[i])))) {
            y = 329 - lineOffset * 13;
            sprintf(buf, "From %s: %s", c->message_sender[i], c->message_text[i]);
            drawString(font, 4, y, buf, BLACK);
            sprintf(buf, "From %s: %s", c->message_sender[i], c->message_text[i]);
            drawString(font, 4, y - 1, buf, CYAN);

            lineOffset++;
            if (lineOffset >= 5) {
                return;
            }
        }

        if (type == 5 && c->private_chat_setting < 2) {
            y = 329 - lineOffset * 13;
            drawString(font, 4, y, c->message_text[i], BLACK);
            drawString(font, 4, y - 1, c->message_text[i], CYAN);

            lineOffset++;
            if (lineOffset >= 5) {
                return;
            }
        }

        if (type == 6 && c->private_chat_setting < 2) {
            y = 329 - lineOffset * 13;
            sprintf(buf, "To %s: %s", c->message_sender[i], c->message_text[i]);
            drawString(font, 4, y, buf, BLACK);
            sprintf(buf, "To %s: %s", c->message_sender[i], c->message_text[i]);
            drawString(font, 4, y - 1, buf, CYAN);

            lineOffset++;
            if (lineOffset >= 5) {
                return;
            }
        }
    }
}

static void updateWorldLocationState(Client *c) {
    if(!c->local_player){c->wildernessLevel=c->worldLocationState=c->overrideChat=0;return;}
    int x = (c->local_player->pathing_entity.x >> 7) + c->sceneBaseTileX;
    int z = (c->local_player->pathing_entity.z >> 7) + c->sceneBaseTileZ;

    if (x >= 2944 && x < 3392 && z >= 3520 && z < 6400) {
        c->wildernessLevel = (z - 3520) / 8 + 1;
    } else if (x >= 2944 && x < 3392 && z >= 9920 && z < 12800) {
        c->wildernessLevel = (z - 9920) / 8 + 1;
    } else {
        c->wildernessLevel = 0;
    }

    c->worldLocationState = 0;
    if (x >= 3328 && x < 3392 && z >= 3200 && z < 3264) {
        int localX = x & 63;
        int localZ = z & 63;

        if (localX >= 4 && localX <= 29 && localZ >= 44 && localZ <= 58) {
            c->worldLocationState = 1;
        } else if (localX >= 36 && localX <= 61 && localZ >= 44 && localZ <= 58) {
            c->worldLocationState = 1;
        } else if (localX >= 4 && localX <= 29 && localZ >= 25 && localZ <= 39) {
            c->worldLocationState = 1;
        } else if (localX >= 36 && localX <= 61 && localZ >= 25 && localZ <= 39) {
            c->worldLocationState = 1;
        } else if (localX >= 4 && localX <= 29 && localZ >= 6 && localZ <= 20) {
            c->worldLocationState = 1;
        } else if (localX >= 36 && localX <= 61 && localZ >= 6 && localZ <= 20) {
            c->worldLocationState = 1;
        }
    }

    if (c->worldLocationState == 0 && x >= 3328 && x <= 3393 && z >= 3203 && z <= 3325) {
        c->worldLocationState = 2;
    }

    c->overrideChat = 0;
    if (x >= 3053 && x <= 3156 && z >= 3056 && z <= 3136) {
        c->overrideChat = 1;
    } else if (x >= 3072 && x <= 3118 && z >= 9492 && z <= 9535) {
        c->overrideChat = 1;
    }

    if (c->overrideChat == 1 && x >= 3139 && x <= 3199 && z >= 3008 && z <= 3062) {
        c->overrideChat = 0;
    }
}

/* Region indicators are transient GPU content, like speech and click marks.
 * They cannot be retained in the UI texture after leaving their region. */
static int barrows_progress_count(Client*c){
 if(!c->ingame||c->scene_state!=2||!c->local_player||!c->varps)return -1;
 return r105_barrows_count(c->sceneBaseTileX+c->local_player->pathing_entity.x/128,c->sceneBaseTileZ+c->local_player->pathing_entity.z/128,c->currentLevel,(unsigned)c->varps[R105_BARROWS_DEAD_VARP]);
}
static void ve_draw_barrows_progress(Client*c){
 if(!ve_native_markers())return;
 int count=barrows_progress_count(c);if(count<0)return;
 static int last=-1;PixMap*p=ve_effect_pm[MAX_CHATS+11];Pix2D saved=_Pix2D;
 if(!p||last!=count){p=ve_effect_surface(MAX_CHATS+11,176,18);if(!p){_Pix2D=saved;return;}
  char text[48];snprintf(text,sizeof text,"Brothers slain: %d/6",count);
  drawStringCenter(c->font_plain12,88,14,text,YELLOW);p->dirty=true;last=count;
 }
 ve_effect_finish(p,saved,744,184);
}

static void ve_draw_location_status(Client*c){
 if(!ve_native_markers()||!c->ingame||c->scene_state!=2||!c->local_player)return;
 updateWorldLocationState(c);
 if(c->wildernessLevel<=0&&c->worldLocationState!=1&&c->in_multizone!=1)return;
 Pix2D saved=_Pix2D;PixMap*p=ve_effect_surface(MAX_CHATS+10,64,96);if(!p){_Pix2D=saved;return;}
 if(c->in_multizone==1)pix24_draw(c->image_headicons[1],20,(c->wildernessLevel>0||c->worldLocationState==1)?8:46);
 char text[HALF_STR];
 if(c->wildernessLevel>0){pix24_draw(c->image_headicons[0],20,46);sprintf(text,"Level: %d",c->wildernessLevel);drawStringCenter(c->font_plain12,32,79,text,YELLOW);}
 if(c->worldLocationState==1){pix24_draw(c->image_headicons[6],20,46);drawStringCenter(c->font_plain12,32,79,"Arena",YELLOW);}
 ve_effect_finish(p,saved,452,250);
}

static void drawTooltip(Client *c) {
    if (c->menu_size < 2 && c->obj_selected == 0 && c->spell_selected == 0) {
        return;
    }

    char tooltip[DOUBLE_STR];
    if (c->obj_selected == 1 && c->menu_size < 2) {
        sprintf(tooltip, "Use %s with...", c->objSelectedName);
    } else if (c->spell_selected == 1 && c->menu_size < 2) {
        sprintf(tooltip, "%s...", c->spellCaption);
    } else {
        strcpy(tooltip, c->menu_option[c->menu_size - 1]);
    }

    if (c->menu_size > 2) {
        char tmp[MAX_STR];
        strcpy(tmp, tooltip);
        sprintf(tooltip, "%s@whi@ / %d more options", tmp, (c->menu_size - 2));
    }

    if(ve_native_markers()){
        Pix2D saved=_Pix2D;PixMap*p=ve_effect_surface(MAX_CHATS,960,32);
        if(p){drawStringTooltip(c->font_bold12,4,15,tooltip,WHITE,true,_Client.loop_cycle/1000);ve_effect_finish(p,saved,0,0);return;}
        _Pix2D=saved;
    }
    drawStringTooltip(c->font_bold12, 4, 15, tooltip, WHITE, true, _Client.loop_cycle / 1000);
}

static void draw3DEntityElements(Client *c) {
    drawPrivateMessages(c);
    if (!ve_native_markers() && c->cross_mode == 1) {
        pix24_draw(c->image_crosses[c->cross_cycle / 100], c->crossX - 8 - 8, c->crossY - 8 - 11);
    }

    if (!ve_native_markers() && c->cross_mode == 2) {
        pix24_draw(c->image_crosses[c->cross_cycle / 100 + 4], c->crossX - 8 - 8, c->crossY - 8 - 11);
    }

    if (c->viewport_interface_id != -1) {
        if (!ve_anim_advanced) client_update_interface_animation(c, c->viewport_interface_id, c->scene_delta);
        client_draw_interface(c, _Component.instances[c->viewport_interface_id], (ve_enabled()?224:GAME_MODAL_X)+client_modal_offset(c,c->viewport_interface_id,ve_enabled()), client_modal_y(c,c->viewport_interface_id,ve_enabled()), 0);
    }

    updateWorldLocationState(c);

    if (!c->menu_visible) {
        if (!ve_input_advanced) client_handle_input(c);
        if(!ve_native_markers())drawTooltip(c);
    } else if (c->menu_area == 0) {
        client_draw_menu(c);
    }

    char buf[HALF_STR];
    if(!ve_native_markers()){
    {int count=barrows_progress_count(c);if(count>=0){char text[48];snprintf(text,sizeof text,"Brothers slain: %d/6",count);drawStringRight(c->font_plain12,507,160,text,YELLOW,true);}}
    if (c->in_multizone == 1) {
        if (c->wildernessLevel > 0 || c->worldLocationState == 1) {
            pix24_draw(c->image_headicons[1], 472, 258);
        } else {
            pix24_draw(c->image_headicons[1], 472, 296);
        }
    }

    if (c->wildernessLevel > 0) {
        pix24_draw(c->image_headicons[0], 472, 296);
        sprintf(buf, "Level: %d", c->wildernessLevel);
        drawStringCenter(c->font_plain12, 484, 329, buf, YELLOW);
    }

    if (c->worldLocationState == 1) {
        pix24_draw(c->image_headicons[6], 472, 296);
        drawStringCenter(c->font_plain12, 484, 329, "Arena", YELLOW);
    }

    }

    if (c->system_update_timer != 0) {
        int seconds = c->system_update_timer / 50;
        int minutes = seconds / 60;
        seconds %= 60;

        if (seconds < 10) {
            sprintf(buf, "System update in: %d:0%d", minutes, seconds);
            drawString(c->font_plain12, 4, 329, buf, YELLOW);
        } else {
            sprintf(buf, "System update in: %d:%d", minutes, seconds);
            drawString(c->font_plain12, 4, 329, buf, YELLOW);
        }
    }

    if(!ve_native_markers()||_Custom.camera_editor||_Custom.show_debug)draw_info_overlay(c);
}

static void pushNpcsPriority(Client *c, bool priority);

void client_draw_scene(Client *c) {
    updateWorldLocationState(c);
    dp_switch(DP_ENTITY);
    H3Scope h3_entity=h3_enter(H3_ENTITY);h3_link(&h3_entity);
    uint64_t entity_started=ba_timing_now();
    ba_timing_record(BA_SCENE_CONSUMED,(uint64_t)c->scene_delta);
    c->scene_cycle++;
    pushNpcsPriority(c, true);
    pushPlayers(c);
    pushNpcs(c);
    pushProjectiles(c);
    pushSpotanims(c);
    // TODO see if defines are needed
#if !defined(_arch_dreamcast) && !defined(__NDS__)
    pushLocs(c);
#endif

    h3_leave(&h3_entity);
    ba_timing_record(BA_ENTITY_PREP,ba_timing_now()-entity_started);
    dp_switch(DP_ORBIT);
    if (!c->cutscene) {
        int pitch = c->orbit_camera_pitch;

        if (c->cameraPitchClamp / 256 > pitch) {
            pitch = c->cameraPitchClamp / 256;
        }

        if (c->cameraModifierEnabled[4] && c->cameraModifierWobbleScale[4] + 128 > pitch) {
            pitch = c->cameraModifierWobbleScale[4] + 128;
        }

        int yaw = c->orbit_camera_yaw + c->camera_anticheat_angle & 0x7ff;
        orbitCamera(c, c->orbitCameraX, getHeightmapY(c, c->currentLevel, c->local_player->pathing_entity.x, c->local_player->pathing_entity.z) - 50, c->orbitCameraZ, yaw, pitch, pitch * 3 + 600);

        _Client.cyclelogic2++;
        if (_Client.cyclelogic2 > 1802) {
            _Client.cyclelogic2 = 0;
            // ANTICHEAT_CYCLELOGIC2
            p1isaac(c->out, 146);
            p1(c->out, 0);
            int start = c->out->pos;
            p2(c->out, 29711);
            p1(c->out, 70);
            p1(c->out, (int)(jrand() * 256.0));
            p1(c->out, 242);
            p1(c->out, 186);
            p1(c->out, 39);
            p1(c->out, 61);
            if ((int)(jrand() * 2.0) == 0) {
                p1(c->out, 13);
            }
            if ((int)(jrand() * 2.0) == 0) {
                p2(c->out, 57856);
            }
            p2(c->out, (int)(jrand() * 65536.0));
            psize1(c->out, c->out->pos - start);
        }
    }

    if (getenv("BUILD_A_MAP_TRACE")) {
        static bool traced_map_state = false;
        if (!traced_map_state) {
            traced_map_state = true;
            int player_x = c->local_player ? c->local_player->pathing_entity.x : -1;
            int player_z = c->local_player ? c->local_player->pathing_entity.z : -1;
            rs2_log("BUILD_A_MAP_TRACE camera=%d,%d,%d player=%d,%d level=%d base=%d,%d center=%d,%d scene=%d\n",
                    c->cameraX, c->cameraY, c->cameraZ, player_x, player_z,
                    c->currentLevel, c->sceneBaseTileX, c->sceneBaseTileZ,
                    c->sceneCenterZoneX, c->sceneCenterZoneZ, c->scene_state);
        }
    }

    int level;
    if (c->cutscene) {
        level = getTopLevelCutscene(c);
    } else {
        level = getTopLevel(c);
    }

    int cameraX = c->cameraX;
    int cameraY = c->cameraY;
    int cameraZ = c->cameraZ;
    int cameraPitch = c->cameraPitch;
    int cameraYaw = c->cameraYaw;
    int jitter;
    for (int type = 0; type < 5; type++) {
        if (c->cameraModifierEnabled[type]) {
            jitter = (int)(jrand() * (double)(c->cameraModifierJitter[type] * 2 + 1) - (double)c->cameraModifierJitter[type] + sin((double)c->cameraModifierCycle[type] * ((double)c->cameraModifierWobbleSpeed[type] / 100.0)) * (double)c->cameraModifierWobbleScale[type]);
            if (type == 0) {
                c->cameraX += jitter;
            }
            if (type == 1) {
                c->cameraY += jitter;
            }
            if (type == 2) {
                c->cameraZ += jitter;
            }
            if (type == 3) {
                c->cameraYaw = c->cameraYaw + jitter & 0x7ff;
            }
            if (type == 4) {
                c->cameraPitch += jitter;
                if (c->cameraPitch < 128) {
                    c->cameraPitch = 128;
                }
                if (c->cameraPitch > 383) {
                    c->cameraPitch = 383;
                }
            }
        }
    }
    jitter = _Pix3D.cycle;
    _Model.check_hover = ((ve_enabled()?ve_world_input(c->shell->mouse_x,c->shell->mouse_y):game_world_input(c->shell->mouse_x,c->shell->mouse_y))&&!lc_game_covers(c->shell->mouse_x,c->shell->mouse_y)) && c->viewport_interface_id == -1;
    _Model.picked_count = 0;
    _Model.mouse_x = ve_pointer(_Pix3D.center_x, c->shell->mouse_x - (ve_enabled()?0:8));
    _Model.mouse_y = ve_pointer(_Pix3D.center_y, c->shell->mouse_y - (ve_enabled()?0:11));
    int ve_hold_overlay=ve_enabled() && (ve_popup_frame || !ve_ui_dirty(c));
    ve_overlay_held_frame=ve_hold_overlay;
    if(!ve_hold_overlay)pix2d_clear();

    gl_start_drawscene();

    // NOTE rm these
    char buf[MAX_STR];
    uint64_t last = rs2_now();
    dp_switch(DP_WORLD);
    uint64_t raster_started=ba_timing_now();
    void r16_set_anchor(int tile_x, int tile_z);
    r16_set_scene_region(c->sceneBaseTileZ);
    if (c->local_player) {
        r16_set_anchor(c->local_player->pathing_entity.x / 128, c->local_player->pathing_entity.z / 128);
        r16_set_roof_focus(c->levelTileFlags,c->currentLevel,c->local_player->pathing_entity.x/128,c->local_player->pathing_entity.z/128);
    }
    if(h4_world_enabled()){H3_SCOPE(H3_WORLD);h4_world_enter();world3d_draw(c->scene, c->cameraX, c->cameraY, c->cameraZ, level, c->cameraYaw, c->cameraPitch, _Client.loop_cycle);}
    else {gl_discard_geometry();ba_profile_scene(world3d_draw_radius(),c->cameraX/128,c->cameraZ/128,c->cameraYaw,c->cameraPitch,0,0);}
    h4_context(world3d_draw_radius(),c->cameraX,c->cameraY,c->cameraZ,c->cameraYaw,c->cameraPitch,c->sceneBaseTileX+c->local_player->pathing_entity.x/128,c->sceneBaseTileZ+c->local_player->pathing_entity.z/128,c->scene_state==2);
    ba_timing_record(BA_WORLD_RASTER,ba_timing_now()-raster_started);
    dp_switch(DP_POST);
    if(ve_enabled())ve_hud_scene_metrics((int)(rs2_now()-last),-1);
    if (!ve_enabled() && _Custom.show_performance && !ve_hold_overlay) {
        sprintf(buf, "World3D: %lu ms", rs2_now() - last);
        drawStringRight(c->font_plain11, PERF_RIGHT(507), PERF_BASELINE(200), classic_perf_display(2,buf), YELLOW, true);
    }

    if(h4_world_enabled()){H3_SCOPE(H3_SUBMIT);gl_end_drawscene(c);}
    if (ve_enabled()) {
        /* Consume this frame's pick results even while the UI is retained. */
        if (!c->menu_visible) client_handle_input(c);
        ve_input_advanced = 1;
        ve_draw_world_markers(c);
        draw2DEntityElements(c);
        if(!c->menu_visible)drawTooltip(c);
        ve_ui_probe(c);
    }


    H3Scope h3_post=h3_enter(H3_POST_WORLD);h3_link(&h3_post);
    world3d_clear_temporarylocs(c->scene);
    updateTextures(c, jitter);
    if(!ve_hold_overlay){
    dp_switch(DP_OV2D);
    if(!ve_native_markers())draw2DEntityElements(c);
    dp_switch(DP_OVHINT);
    drawTileHint(c);
    dp_switch(DP_OV3D);
    draw3DEntityElements(c);
    dp_switch(DP_POST);
    }

    static uint64_t pixmap_now;
    static uint64_t pixmap_last;
    if(ve_enabled())ve_hud_scene_metrics(-1,(int)(pixmap_now-pixmap_last));
    if (!ve_enabled() && _Custom.show_performance && !ve_hold_overlay) {
        sprintf(buf, "Viewport pixmap: %lu ms", pixmap_now - pixmap_last);
        drawStringRight(c->font_plain11, PERF_RIGHT(507), PERF_BASELINE(213), classic_perf_display(3,buf), YELLOW, true);
    }
    pixmap_last = rs2_now();
    if(!ve_enabled())pixmap_draw(c->area_viewport, 8, 11);
    pixmap_now = rs2_now();

    h3_leave(&h3_post);
    c->cameraX = cameraX;
    c->cameraY = cameraY;
    c->cameraZ = cameraZ;
    c->cameraPitch = cameraPitch;
    c->cameraYaw = cameraYaw;
}

int getTopLevel(Client *c) {
    int top = 3;
    if (c->cameraPitch < 310) {
        int cameraLocalTileX = c->cameraX >> 7;
        int cameraLocalTileZ = c->cameraZ >> 7;
        int playerLocalTileX = c->local_player->pathing_entity.x >> 7;
        int playerLocalTileZ = c->local_player->pathing_entity.z >> 7;
        if ((c->levelTileFlags[c->currentLevel][cameraLocalTileX][cameraLocalTileZ] & 0x4) != 0) {
            top = c->currentLevel;
        }
        int tileDeltaX;
        if (playerLocalTileX > cameraLocalTileX) {
            tileDeltaX = playerLocalTileX - cameraLocalTileX;
        } else {
            tileDeltaX = cameraLocalTileX - playerLocalTileX;
        }
        int tileDeltaZ;
        if (playerLocalTileZ > cameraLocalTileZ) {
            tileDeltaZ = playerLocalTileZ - cameraLocalTileZ;
        } else {
            tileDeltaZ = cameraLocalTileZ - playerLocalTileZ;
        }
        int delta;
        int accumulator;
        if (tileDeltaX > tileDeltaZ) {
            delta = tileDeltaZ * 65536 / tileDeltaX;
            accumulator = 32768;
            while (cameraLocalTileX != playerLocalTileX) {
                if (cameraLocalTileX < playerLocalTileX) {
                    cameraLocalTileX++;
                } else if (cameraLocalTileX > playerLocalTileX) {
                    cameraLocalTileX--;
                }
                if ((c->levelTileFlags[c->currentLevel][cameraLocalTileX][cameraLocalTileZ] & 0x4) != 0) {
                    top = c->currentLevel;
                }
                accumulator += delta;
                if (accumulator >= 65536) {
                    accumulator -= 65536;
                    if (cameraLocalTileZ < playerLocalTileZ) {
                        cameraLocalTileZ++;
                    } else if (cameraLocalTileZ > playerLocalTileZ) {
                        cameraLocalTileZ--;
                    }
                    if ((c->levelTileFlags[c->currentLevel][cameraLocalTileX][cameraLocalTileZ] & 0x4) != 0) {
                        top = c->currentLevel;
                    }
                }
            }
        } else {
            delta = tileDeltaX * 65536 / tileDeltaZ;
            accumulator = 32768;
            while (cameraLocalTileZ != playerLocalTileZ) {
                if (cameraLocalTileZ < playerLocalTileZ) {
                    cameraLocalTileZ++;
                } else if (cameraLocalTileZ > playerLocalTileZ) {
                    cameraLocalTileZ--;
                }
                if ((c->levelTileFlags[c->currentLevel][cameraLocalTileX][cameraLocalTileZ] & 0x4) != 0) {
                    top = c->currentLevel;
                }
                accumulator += delta;
                if (accumulator >= 65536) {
                    accumulator -= 65536;
                    if (cameraLocalTileX < playerLocalTileX) {
                        cameraLocalTileX++;
                    } else if (cameraLocalTileX > playerLocalTileX) {
                        cameraLocalTileX--;
                    }
                    if ((c->levelTileFlags[c->currentLevel][cameraLocalTileX][cameraLocalTileZ] & 0x4) != 0) {
                        top = c->currentLevel;
                    }
                }
            }
        }
    }
    if ((c->levelTileFlags[c->currentLevel][c->local_player->pathing_entity.x >> 7][c->local_player->pathing_entity.z >> 7] & 0x4) != 0) {
        top = c->currentLevel;
    }
    return top;
}

int getTopLevelCutscene(Client *c) {
    int y = getHeightmapY(c, c->currentLevel, c->cameraX, c->cameraZ);
    return y - c->cameraY >= 800 || (c->levelTileFlags[c->currentLevel][c->cameraX >> 7][c->cameraZ >> 7] & 0x4) == 0 ? 3 : c->currentLevel;
}

int getHeightmapY(Client *c, int level, int sceneX, int sceneZ) {
    int tileX = MIN(sceneX >> 7, 104 - 1);
    int tileZ = MIN(sceneZ >> 7, 104 - 1);
    int realLevel = level;
    if (level < 3 && (c->levelTileFlags[1][tileX][tileZ] & 0x2) == 2) {
        realLevel = level + 1;
    }

    int tileLocalX = sceneX & 0x7f;
    int tileLocalZ = sceneZ & 0x7f;
    int y00 = (c->levelHeightmap[realLevel][tileX][tileZ] * (128 - tileLocalX) + c->levelHeightmap[realLevel][tileX + 1][tileZ] * tileLocalX) >> 7;
    int y11 = (c->levelHeightmap[realLevel][tileX][tileZ + 1] * (128 - tileLocalX) + c->levelHeightmap[realLevel][tileX + 1][tileZ + 1] * tileLocalX) >> 7;
    return (y00 * (128 - tileLocalZ) + y11 * tileLocalZ) >> 7;
}

void orbitCamera(Client *c, int targetX, int targetY, int targetZ, int yaw, int pitch, int distance) {
    int invPitch = 2048 - pitch & 0x7ff;
    int invYaw = 2048 - yaw & 0x7ff;
    int x = 0;
    int z = 0;
    int y = distance;
    int sin;
    int cos;
    int tmp;

    if (invPitch != 0) {
        sin = _Pix3D.sin_table[invPitch];
        cos = _Pix3D.cos_table[invPitch];
        tmp = (z * cos - distance * sin) >> 16;
        y = (z * sin + distance * cos) >> 16;
        z = tmp;
    }

    if (invYaw != 0) {
        sin = _Pix3D.sin_table[invYaw];
        cos = _Pix3D.cos_table[invYaw];
        tmp = (y * sin + x * cos) >> 16;
        y = (y * cos - x * sin) >> 16;
        x = tmp;
    }

    c->cameraX = targetX - x;
    c->cameraY = targetY - z;
    c->cameraZ = targetZ - y;
    c->cameraPitch = pitch;
    c->cameraYaw = yaw;
}

void pushLocs(Client *c) {
    r10_animation_update(1);
    for (LocEntity *loc = (LocEntity *)linklist_head(c->locList); loc; loc = (LocEntity *)linklist_next(c->locList)) {
        bool append = false;
        int visual_delta=r106_scenery_delta(c->scene_delta,loc->ambient_half,&loc->ambient_remainder);
        if(c->scene_delta>0){
            if(loc->ambient_half){r106_ambient_input+=(unsigned)c->scene_delta;r106_ambient_output+=(unsigned)visual_delta;}
            else{r106_action_input+=(unsigned)c->scene_delta;r106_action_output+=(unsigned)visual_delta;}
        }
        loc->seqCycle += visual_delta;
        if (loc->seqFrame == -1) {
            loc->seqFrame = 0;
            append = true;
        }

        while (loc->seqCycle > loc->seq->delay[loc->seqFrame]) {
            loc->seqCycle -= loc->seq->delay[loc->seqFrame] + 1;
            loc->seqFrame++;

            append = true;

            if (loc->seqFrame >= loc->seq->frameCount) {
                loc->seqFrame -= loc->seq->replayoff;

                if (loc->seqFrame < 0 || loc->seqFrame >= loc->seq->frameCount) {
                    linkable_unlink(&loc->link);
                    free(loc);
                    append = false;
                    break;
                }
            }
        }

        if (append) {
            int level = loc->level;
            int x = loc->x;
            int z = loc->z;

            int bitset = 0;
            if (loc->type == 0) {
                bitset = world3d_get_wallbitset(c->scene, level, x, z);
            }

            if (loc->type == 1) {
                bitset = world3d_get_walldecorationbitset(c->scene, level, z, x);
            }

            if (loc->type == 2) {
                bitset = world3d_get_locbitset(c->scene, level, x, z);
            }

            if (loc->type == 3) {
                bitset = world3d_get_grounddecorationbitset(c->scene, level, x, z);
            }

            if (bitset != 0 && (bitset >> 14 & 0x7fff) == loc->index) {
                int heightmapSW = c->levelHeightmap[level][x][z];
                int heightmapSE = c->levelHeightmap[level][x + 1][z];
                int heightmapNE = c->levelHeightmap[level][x + 1][z + 1];
                int heightmapNW = c->levelHeightmap[level][x][z + 1];

                LocType *type = loctype_get(loc->index);
                int seqId = -1;
                if (loc->seqFrame != -1) {
                    seqId = loc->seq->frames[loc->seqFrame];
                }

                if (loc->type == 2) {
                    int info = world3d_get_info(c->scene, level, x, z, bitset);
                    int shape = info & 0x1f;
                    int rotation = info >> 6;

                    if (shape == CENTREPIECE_DIAGONAL) {
                        shape = CENTREPIECE_STRAIGHT;
                    }

                    Model *model = loctype_get_model(type, shape, rotation, heightmapSW, heightmapSE, heightmapNE, heightmapNW, seqId);
                    world3d_set_locmodel(c->scene, level, x, z, model);
                } else if (loc->type == 1) {
                    Model *model = loctype_get_model(type, WALLDECOR_STRAIGHT_NOOFFSET, 0, heightmapSW, heightmapSE, heightmapNE, heightmapNW, seqId);
                    world3d_set_walldecorationmodel(c->scene, level, x, z, model);
                } else if (loc->type == 0) {
                    int info = world3d_get_info(c->scene, level, x, z, bitset);
                    int shape = info & 0x1f;
                    int rotation = info >> 6;

                    if (shape == WALL_L) {
                        int nextRotation = rotation + 1 & 0x3;
                        Model *model1 = loctype_get_model(type, WALL_L, rotation + 4, heightmapSW, heightmapSE, heightmapNE, heightmapNW, seqId);
                        Model *model2 = loctype_get_model(type, WALL_L, nextRotation, heightmapSW, heightmapSE, heightmapNE, heightmapNW, seqId);
                        world3d_set_wallmodels(c->scene, x, z, level, model1, model2);
                    } else {
                        Model *model = loctype_get_model(type, shape, rotation, heightmapSW, heightmapSE, heightmapNE, heightmapNW, seqId);
                        world3d_set_wallmodel(c->scene, level, x, z, model);
                    }
                } else if (loc->type == 3) {
                    int info = world3d_get_info(c->scene, level, x, z, bitset);
                    int rotation = info >> 6;
                    Model *model = loctype_get_model(type, GROUNDDECOR, rotation, heightmapSW, heightmapSE, heightmapNE, heightmapNW, seqId);
                    world3d_set_grounddecorationmodel(c->scene, level, x, z, model);
                }
            } else {
                linkable_unlink(&loc->link);
                free(loc);
            }
        }
    }
    r10_animation_update(0);
}

void pushSpotanims(Client *c) {
    for (SpotAnimEntity *entity = (SpotAnimEntity *)linklist_head(c->spotanims); entity; entity = (SpotAnimEntity *)linklist_next(c->spotanims)) {
        if (entity->level != c->currentLevel || entity->seqComplete) {
            linkable_unlink(&entity->entity.link);
            free(entity);
        } else if (_Client.loop_cycle >= entity->startCycle) {
            spotanimentity_update(entity, c->scene_delta);
            if (entity->seqComplete) {
                linkable_unlink(&entity->entity.link);
                free(entity);
            } else {
                world3d_add_temporary(c->scene, entity->level, entity->x, entity->y, entity->z, NULL, &entity->entity, -1, 0, 60, false);
            }
        }
    }
}

void pushProjectiles(Client *c) {
    for (ProjectileEntity *proj = (ProjectileEntity *)linklist_head(c->projectiles); proj; proj = (ProjectileEntity *)linklist_next(c->projectiles)) {
        if (proj->level != c->currentLevel || _Client.loop_cycle > proj->lastCycle) {
            linkable_unlink(&proj->entity.link);
            free(proj);
        } else if (_Client.loop_cycle >= proj->startCycle) {
            if (proj->target > 0) {
                NpcEntity *npc = c->npcs[proj->target - 1];
                if (npc) {
                    projectileentity_update_velocity(proj, npc->pathing_entity.x, getHeightmapY(c, proj->level, npc->pathing_entity.x, npc->pathing_entity.z) - proj->offsetY, npc->pathing_entity.z, _Client.loop_cycle);
                }
            }

            if (proj->target < 0) {
                int index = -proj->target - 1;
                PlayerEntity *player;
                if (index == c->local_pid) {
                    player = c->local_player;
                } else {
                    player = c->players[index];
                }
                if (player) {
                    projectileentity_update_velocity(proj, player->pathing_entity.x, getHeightmapY(c, proj->level, player->pathing_entity.x, player->pathing_entity.z) - proj->offsetY, player->pathing_entity.z, _Client.loop_cycle);
                }
            }

            projectileentity_update(proj, c->scene_delta);
            world3d_add_temporary(c->scene, c->currentLevel, (int)proj->x, (int)proj->y, (int)proj->z, NULL, &proj->entity, -1, proj->yaw, 60, false);
        }
    }
}

void pushNpcs(Client *c) { pushNpcsPriority(c, false); }

static void pushNpcsPriority(Client *c, bool priority) {
    extern void r101_bloodworm_scene_probe(const NpcEntity*,int,int,int,int);
    for (int i = 0; i < c->npc_count; i++) {
        NpcEntity *npc = c->npcs[c->npc_ids[i]];
        int bitset = (c->npc_ids[i] << 14) + 0x20000000;

        if (!npc || !npcentity_is_visible(npc) || npc->type->alwaysontop != priority) {
            if(npc&&npc->type&&npc->type->index==1049&&npc->type->alwaysontop==priority)r101_bloodworm_scene_probe(npc,c->npc_ids[i],c->scene_cycle,_Client.loop_cycle,1);
            if(npc&&npc->type&&npc->type->alwaysontop==priority)recovery_npc_trace(c,npc,c->npc_ids[i],"SCENE",1);
            continue;
        }

        int x = npc->pathing_entity.x >> 7;
        int z = npc->pathing_entity.z >> 7;

        if (x < 0 || x >= 104 || z < 0 || z >= 104) {
            if(npc->type->index==1049)r101_bloodworm_scene_probe(npc,c->npc_ids[i],c->scene_cycle,_Client.loop_cycle,3);
            continue;
        }

        if (npc->pathing_entity.size == 1 && (npc->pathing_entity.x & 0x7f) == 64 && (npc->pathing_entity.z & 0x7f) == 64) {
            if (c->tileLastOccupiedCycle[x][z] == c->scene_cycle) {
                if(npc->type->index==1049)r101_bloodworm_scene_probe(npc,c->npc_ids[i],c->scene_cycle,_Client.loop_cycle,2);
                recovery_npc_trace(c,npc,c->npc_ids[i],"SCENE",2);
                continue;
            }

            c->tileLastOccupiedCycle[x][z] = c->scene_cycle;
        }

        recovery_npc_trace(c,npc,c->npc_ids[i],"SCENE",0);
        if(npc->type->index==1049)r101_bloodworm_scene_probe(npc,c->npc_ids[i],c->scene_cycle,_Client.loop_cycle,0);
        world3d_add_temporary(c->scene, c->currentLevel, npc->pathing_entity.x, getHeightmapY(c, c->currentLevel, npc->pathing_entity.x, npc->pathing_entity.z), npc->pathing_entity.z, NULL, &npc->pathing_entity.entity, bitset, npc->pathing_entity.yaw, (npc->pathing_entity.size - 1) * 64 + 60, npc->pathing_entity.seqStretches);
    }
}

void pushPlayers(Client *c) {
    if (c->local_player->pathing_entity.x >> 7 == c->flagSceneTileX && c->local_player->pathing_entity.z >> 7 == c->flagSceneTileZ) {
        c->flagSceneTileX = 0;
    }

    for (int i = -1; i < c->player_count; i++) {
        PlayerEntity *player;
        int id;
        if (i == -1) {
            player = c->local_player;
            id = LOCAL_PLAYER_INDEX << 14;
        } else {
            player = c->players[c->player_ids[i]];
            id = c->player_ids[i] << 14;
        }

        if (!player || !playerentity_is_visible(player)) {
            continue;
        }

        player->lowmem = ((_Client.lowmem && c->player_count > 50) || c->player_count > 200) && i != -1 && player->pathing_entity.secondarySeqId == player->pathing_entity.seqStandId;
        int stx = player->pathing_entity.x >> 7;
        int stz = player->pathing_entity.z >> 7;

        if (stx < 0 || stx >= 104 || stz < 0 || stz >= 104) {
            continue;
        }

        if (!player->locModel || _Client.loop_cycle < player->locStartCycle || _Client.loop_cycle >= player->locStopCycle) {
            if ((player->pathing_entity.x & 0x7f) == 64 && (player->pathing_entity.z & 0x7f) == 64) {
                if (c->tileLastOccupiedCycle[stx][stz] == c->scene_cycle) {
                    continue;
                }

                c->tileLastOccupiedCycle[stx][stz] = c->scene_cycle;
            }

            player->y = getHeightmapY(c, c->currentLevel, player->pathing_entity.x, player->pathing_entity.z);
            world3d_add_temporary(c->scene, c->currentLevel, player->pathing_entity.x, player->y, player->pathing_entity.z, NULL, &player->pathing_entity.entity, id, player->pathing_entity.yaw, 60, player->pathing_entity.seqStretches);
        } else {
            player->lowmem = false;
            player->y = getHeightmapY(c, c->currentLevel, player->pathing_entity.x, player->pathing_entity.z);
            world3d_add_temporary2(c->scene, c->currentLevel, player->pathing_entity.x, player->y, player->pathing_entity.z, player->minTileX, player->minTileZ, player->maxTileX, player->maxTileZ, NULL, &player->pathing_entity.entity, id, player->pathing_entity.yaw);
        }
    }
}

void client_draw_minimap(Client *c) {
    pixmap_bind(c->area_mapback);
    /* Enhanced fills the full map source. Restore Classic artwork before its circular draw. */
    if (!ve_enabled()) pix8_draw(c->image_mapback, 0, 0);
    int angle = c->orbit_camera_yaw + c->minimap_anticheat_angle & 0x7ff;
    int anchorX = c->local_player->pathing_entity.x / 32 + 48;
    int anchorY = 464 - c->local_player->pathing_entity.z / 32;

    int full_offsets[151]={0},full_lengths[151];for(int row=0;row<151;row++)full_lengths[row]=146;
    pix24_draw_rotated_masked(c->image_minimap, 21, 9, 146, 151, ve_enabled()?full_offsets:c->minimap_mask_line_offsets, ve_enabled()?full_lengths:c->minimap_mask_line_lengths, anchorX, anchorY, angle, c->minimap_zoom + 256);
    pix24_draw_rotated_masked(c->image_compass, 0, 0, 33, 33, c->compass_mask_line_offsets, c->compass_mask_line_lengths, 25, 25, c->orbit_camera_yaw, 256);
    for (int i = 0; i < c->activeMapFunctionCount; i++) {
        anchorX = c->activeMapFunctionX[i] * 4 + 2 - c->local_player->pathing_entity.x / 32;
        anchorY = c->activeMapFunctionZ[i] * 4 + 2 - c->local_player->pathing_entity.z / 32;
        client_draw_on_minimap(c, anchorY, c->activeMapFunctions[i], anchorX);
    }

    for (int ltx = 0; ltx < 104; ltx++) {
        for (int ltz = 0; ltz < 104; ltz++) {
            LinkList *stack = c->level_obj_stacks[c->currentLevel][ltx][ltz];
            if (stack) {
                anchorX = ltx * 4 + 2 - c->local_player->pathing_entity.x / 32;
                anchorY = ltz * 4 + 2 - c->local_player->pathing_entity.z / 32;
                client_draw_on_minimap(c, anchorY, c->image_mapdot0, anchorX);
            }
        }
    }

    for (int i = 0; i < c->npc_count; i++) {
        NpcEntity *npc = c->npcs[c->npc_ids[i]];
        if (npc && npcentity_is_visible(npc) && npc->type->minimap) {
            anchorX = npc->pathing_entity.x / 32 - c->local_player->pathing_entity.x / 32;
            anchorY = npc->pathing_entity.z / 32 - c->local_player->pathing_entity.z / 32;
            client_draw_on_minimap(c, anchorY, c->image_mapdot1, anchorX);
        }
    }

    for (int i = 0; i < c->player_count; i++) {
        PlayerEntity *player = c->players[c->player_ids[i]];
        if (player && playerentity_is_visible(player)) {
            anchorX = player->pathing_entity.x / 32 - c->local_player->pathing_entity.x / 32;
            anchorY = player->pathing_entity.z / 32 - c->local_player->pathing_entity.z / 32;

            bool friend = false;
            int64_t name37 = jstring_to_base37(player->name);
            for (int j = 0; j < c->friend_count; j++) {
                if (name37 == c->friendName37[j] && c->friendWorld[j] != 0) {
                    friend = true;
                    break;
                }
            }

            if (friend) {
                client_draw_on_minimap(c, anchorY, c->image_mapdot3, anchorX);
            } else {
                client_draw_on_minimap(c, anchorY, c->image_mapdot2, anchorX);
            }
        }
    }

    if (c->flagSceneTileX != 0) {
        anchorX = c->flagSceneTileX * 4 + 2 - c->local_player->pathing_entity.x / 32;
        anchorY = c->flagSceneTileZ * 4 + 2 - c->local_player->pathing_entity.z / 32;
        client_draw_on_minimap(c, anchorY, c->image_mapflag, anchorX);
    }

    pix2d_fill_rect(93, 82, WHITE, 3, 3);
    pixmap_bind(c->area_viewport);
}

void client_draw_on_minimap(Client *c, int dy, Pix24 *image, int dx) {
    int angle = c->orbit_camera_yaw + c->minimap_anticheat_angle & 0x7ff;
    int distance = dx * dx + dy * dy;
    if (distance > 6400) {
        return;
    }

    int sinAngle = _Pix3D.sin_table[angle];
    int cosAngle = _Pix3D.cos_table[angle];

    sinAngle = sinAngle * 256 / (c->minimap_zoom + 256);
    cosAngle = cosAngle * 256 / (c->minimap_zoom + 256);

    int x = (dy * sinAngle + dx * cosAngle) >> 16;
    int y = (dy * cosAngle - dx * sinAngle) >> 16;

    if (distance > 2500 && !ve_enabled()) {
        pix24_draw_masked(image, x + 94 - image->crop_w / 2, 83 - y - image->crop_h / 2, c->image_mapback);
    } else {
        pix24_draw(image, x + 94 - image->crop_w / 2, 83 - y - image->crop_h / 2);
    }
}

void client_draw_chatback(Client *c) {
    pixmap_bind(c->area_chatback);
    _Pix3D.line_offset = c->area_chatback_offsets;
    ui474_parchment_chat(c);
    if (c->show_social_input) {
        char buf[CHAT_LENGTH + 2];
        sprintf(buf, "%s*", c->social_input);
        drawStringCenter(c->font_bold12, (ve_enabled()?lc_chat_text_right(CHAT_TEXT_RIGHT):CHAT_WIDTH) / 2, 40, c->social_message, BLACK);
        drawStringCenter(c->font_bold12, (ve_enabled()?lc_chat_text_right(CHAT_TEXT_RIGHT):CHAT_WIDTH) / 2, 60, buf, DARKBLUE);
    } else if (c->chatback_input_open) {
        char buf[CHATBACK_LENGTH + 2];
        sprintf(buf, "%s*", c->chatback_input);
        drawStringCenter(c->font_bold12, (ve_enabled()?lc_chat_text_right(CHAT_TEXT_RIGHT):CHAT_WIDTH) / 2, 40, "Enter amount:", BLACK);
        drawStringCenter(c->font_bold12, (ve_enabled()?lc_chat_text_right(CHAT_TEXT_RIGHT):CHAT_WIDTH) / 2, 60, buf, DARKBLUE);
    } else if (c->modal_message[0]) {
        drawStringCenter(c->font_bold12, (ve_enabled()?lc_chat_text_right(CHAT_TEXT_RIGHT):CHAT_WIDTH) / 2, 40, c->modal_message, BLACK);
        drawStringCenter(c->font_bold12, (ve_enabled()?lc_chat_text_right(CHAT_TEXT_RIGHT):CHAT_WIDTH) / 2, 60, c->vita_interface_enabled?"Press X to continue":"Click to continue", DARKBLUE);
    } else if (c->chat_interface_id != -1) {
        client_draw_interface(c, _Component.instances[c->chat_interface_id], 0, 0, 0);
    } else if (c->sticky_chat_interface_id == -1) {
        PixFont *font = c->font_plain12;
        if (_Custom.chat_era == 0) {
            font = c->font_quill8;
        }
        int line = 0;
        pix2d_set_clipping(77, lc_chat_text_right(CHAT_TEXT_RIGHT), 0, 0);
#ifdef VITA_NATIVE_WIDESCREEN
        if(ve_enabled())ve_draw_chat(c,font);else classic_draw_chat(c,font);
        line = (c->chat_scroll_height - 7) / 14;
#else
        for (int i = 0; i < 100; i++) {
            if (c->message_text[i][0]) {
                int type = c->message_type[i];
                int offset = c->chat_scroll_offset + 70 - line * 14;
                if (type == 0) {
                    if (offset > 0 && offset < 110) {
                        drawString(font, 4, offset, c->message_text[i], BLACK);
                    }
                    line++;
                }
                if (type == 1) {
                    if (offset > 0 && offset < 110) {
                        char buf[USERNAME_LENGTH + 2];
                        sprintf(buf, "%s:", c->message_sender[i]);
                        drawString(font, 4, offset, buf, WHITE);
                        drawString(font, stringWidth(font, c->message_sender[i]) + 12, offset, c->message_text[i], BLUE);
                    }
                    line++;
                }
                if (type == 2 && (c->public_chat_setting == 0 || (c->public_chat_setting == 1 && client_is_friend(c, c->message_sender[i])))) {
                    if (offset > 0 && offset < 110) {
                        char buf[USERNAME_LENGTH + 2];
                        sprintf(buf, "%s:", c->message_sender[i]);
                        drawString(font, 4, offset, buf, BLACK);
                        drawString(font, stringWidth(font, c->message_sender[i]) + 12, offset, c->message_text[i], BLUE);
                    }
                    line++;
                }
                if ((type == 3 || type == 7) && c->split_private_chat == 0 && (type == 7 || c->private_chat_setting == 0 || (c->private_chat_setting == 1 && client_is_friend(c, c->message_sender[i])))) {
                    if (offset > 0 && offset < 110) {
                        char buf[USERNAME_LENGTH + 7];
                        sprintf(buf, "From %s:", c->message_sender[i]);
                        drawString(font, 4, offset, buf, BLACK);
                        sprintf(buf, "From %s", c->message_sender[i]);
                        drawString(font, stringWidth(font, buf) + 12, offset, c->message_text[i], DARKRED);
                    }
                    line++;
                }
                if (type == 4 && (c->trade_chat_setting == 0 || (c->trade_chat_setting == 1 && client_is_friend(c, c->message_sender[i])))) {
                    if (offset > 0 && offset < 110) {
                        char buf[USERNAME_LENGTH + CHAT_LENGTH + 2];
                        sprintf(buf, "%s %s", c->message_sender[i], c->message_text[i]);
                        drawString(font, 4, offset, buf, TRADE_MESSAGE);
                    }
                    line++;
                }
                if (type == 5 && c->split_private_chat == 0 && c->private_chat_setting < 2) {
                    if (offset > 0 && offset < 110) {
                        drawString(font, 4, offset, c->message_text[i], DARKRED);
                    }
                    line++;
                }
                if (type == 6 && c->split_private_chat == 0 && c->private_chat_setting < 2) {
                    if (offset > 0 && offset < 110) {
                        char buf[USERNAME_LENGTH + 6];
                        sprintf(buf, "To %s:", c->message_sender[i]);
                        drawString(font, 4, offset, buf, BLACK);
                        sprintf(buf, "To %s", c->message_sender[i]);
                        drawString(font, stringWidth(font, buf) + 12, offset, c->message_text[i], DARKRED);
                    }
                    line++;
                }
                if (type == 8 && (c->trade_chat_setting == 0 || (c->trade_chat_setting == 1 && client_is_friend(c, c->message_sender[i])))) {
                    if (offset > 0 && offset < 110) {
                        char buf[USERNAME_LENGTH + CHAT_LENGTH + 2];
                        sprintf(buf, "%s %s", c->message_sender[i], c->message_text[i]);
                        drawString(font, 4, offset, buf, DUEL_MESSAGE);
                    }
                    line++;
                }
            }
        }
#endif
        pix2d_reset_clipping();
        c->chat_scroll_height = line * 14 + 7;
        if (c->chat_scroll_height < 78) {
            c->chat_scroll_height = 78;
        }
        client_draw_scrollbar(c, lc_chat_text_right(CHAT_TEXT_RIGHT), 0, c->chat_scroll_height - c->chat_scroll_offset - 77, c->chat_scroll_height, 77);

        if (_Custom.chat_era == 0) {
            // 186-194?
            char buf2[CHAT_LENGTH + 2];
            sprintf(buf2, "%s*", c->chat_typed);
            (ve_enabled()?ve_draw_chat_input:classic_draw_chat_input)(font, 3, ve_enabled()?93:90, buf2, BLACK);
        } else if (_Custom.chat_era == 1) {
            // <204
            char buf2[CHAT_LENGTH + 2];
            sprintf(buf2, "%s*", c->chat_typed);
            (ve_enabled()?ve_draw_chat_input:classic_draw_chat_input)(font, 3, ve_enabled()?93:90, buf2, BLUE);
        } else if (_Custom.chat_era == 2) {
            // 204+
            char buf[USERNAME_LENGTH + 3];
            sprintf(buf, "%s:", jstring_format_name(c->username));
            drawString(font, 4, ve_enabled()?93:90, buf, BLACK);
            sprintf(buf, "%s: ", c->username);

            char buf2[CHAT_LENGTH + 2];
            sprintf(buf2, "%s*", c->chat_typed);
            (ve_enabled()?ve_draw_chat_input:classic_draw_chat_input)(font, stringWidth(font, buf) + 6, ve_enabled()?93:90, buf2, BLUE);
        }

        pix2d_hline(0, ve_enabled()?80:77, BLACK, ve_enabled()?lc_chat_text_right(CHAT_TEXT_RIGHT):CHAT_WIDTH);
    } else {
        client_draw_interface(c, _Component.instances[c->sticky_chat_interface_id], 0, 0, 0);
    }
    if (c->menu_visible && c->menu_area == 2) {
        client_draw_menu(c);
    }
    if(!ve_enabled())lc_draw(c->area_chatback,LC_CHAT, (ve_enabled()?ve_chat_text_x():lc_x(LC_CHAT,22)), (ve_enabled()?ve_chat_text_y():lc_y(LC_CHAT,UI_BOTTOM(375))));
    pixmap_bind(c->area_viewport);
    _Pix3D.line_offset = c->area_viewport_offsets;
}

bool client_is_friend(Client *c, const char *username) {
    if (!username) {
        return false;
    }

    for (int i = 0; i < c->friend_count; i++) {
        if (platform_strcasecmp(username, c->friendName[i]) == 0) {
            return true;
        }
    }

    return (platform_strcasecmp(username, c->local_player->name) == 0);
}

void client_draw_scrollbar(Client *c, int x, int y, int scrollY, int scrollHeight, int height) {
    pix8_draw(c->image_scrollbar0, x, y);
    pix8_draw(c->image_scrollbar1, x, y + height - 16);
    pix2d_fill_rect(x, y + 16, SCROLLBAR_TRACK, 16, height - 32);

    int gripSize = (height - 32) * height / scrollHeight;
    if (gripSize < 8) {
        gripSize = 8;
    }

    int gripY = (height - gripSize - 32) * scrollY / (scrollHeight - height);
    pix2d_fill_rect(x, y + gripY + 16, SCROLLBAR_GRIP_FOREGROUND, 16, gripSize);

    pix2d_vline(x, y + gripY + 16, SCROLLBAR_GRIP_HIGHLIGHT, gripSize);
    pix2d_vline(x + 1, y + gripY + 16, SCROLLBAR_GRIP_HIGHLIGHT, gripSize);

    pix2d_hline(x, y + gripY + 16, SCROLLBAR_GRIP_HIGHLIGHT, 16);
    pix2d_hline(x, y + gripY + 17, SCROLLBAR_GRIP_HIGHLIGHT, 16);

    pix2d_vline(x + 15, y + gripY + 16, SCROLLBAR_GRIP_LOWLIGHT, gripSize);
    pix2d_vline(x + 14, y + gripY + 17, SCROLLBAR_GRIP_LOWLIGHT, gripSize - 1);

    pix2d_hline(x, y + gripY + gripSize + 15, SCROLLBAR_GRIP_LOWLIGHT, 16);
    pix2d_hline(x + 1, y + gripY + gripSize + 14, SCROLLBAR_GRIP_LOWLIGHT, 15);
}

void client_draw_sidebar(Client *c) {
    pixmap_bind(c->area_sidebar);
    _Pix3D.line_offset = c->area_sidebar_offsets;
    pix8_draw(c->image_invback, 0, 0);
    if (c->sidebar_interface_id != -1) {
        client_draw_interface(c, _Component.instances[c->sidebar_interface_id], 0, 0, 0);
    } else if (c->tab_interface_id[c->selected_tab] != -1) {
        client_draw_interface(c, _Component.instances[c->tab_interface_id[c->selected_tab]], 0, 0, 0);
    }
    if (c->menu_visible && c->menu_area == 1) {
        client_draw_menu(c);
    }
    lc_draw(c->area_sidebar,LC_SIDEBAR, ui474_side_x(), ui474_side_y());
    pixmap_bind(c->area_viewport);
    _Pix3D.line_offset = c->area_viewport_offsets;
}

void client_update_interface_content(Client *c, Component *component) {
    int clientCode = component->clientCode;

    if (clientCode >= 1 && clientCode <= 100) {
        clientCode--;
        if (clientCode >= c->friend_count) {
            strcpy(component->text, "");
            component->buttonType = 0;
        } else {
            strcpy(component->text, c->friendName[clientCode]);
            component->buttonType = 1;
        }
    } else if (clientCode >= 101 && clientCode <= 200) {
        clientCode -= 101;
        if (clientCode >= c->friend_count) {
            strcpy(component->text, "");
            component->buttonType = 0;
        } else {
            if (c->friendWorld[clientCode] == 0) {
                strcpy(component->text, "@red@Offline");
            } else if (c->friendWorld[clientCode] == _Client.nodeid) {
                sprintf(component->text, "@gre@World-%d", c->friendWorld[clientCode] - 9);
            } else {
                sprintf(component->text, "@yel@World-%d", c->friendWorld[clientCode] - 9);
            }
            component->buttonType = 1;
        }
    } else if (clientCode == 203) {
        component->scroll = c->friend_count * 15 + 20;
        if (component->scroll <= component->height) {
            component->scroll = component->height + 1;
        }
    } else if (clientCode >= 401 && clientCode <= 500) {
        clientCode -= 401;
        if (clientCode >= c->ignoreCount) {
            strcpy(component->text, "");
            component->buttonType = 0;
        } else {
            strcpy(component->text, jstring_format_name(jstring_from_base37(c->ignoreName37[clientCode])));
            component->buttonType = 1;
        }
    } else if (clientCode == 503) {
        component->scroll = c->ignoreCount * 15 + 20;
        if (component->scroll <= component->height) {
            component->scroll = component->height + 1;
        }
    } else if (clientCode == 327) {
        component->xan = 150;
        component->yan = (int)(sin((double)_Client.loop_cycle / 40.0) * 256.0) & 0x7ff;
        if (c->update_design_model) {
            c->update_design_model = false;

            Model **models = calloc(7, sizeof(Model *));
            int modelCount = 0;
            for (int part = 0; part < 7; part++) {
                int kit = c->designIdentikits[part];
                if (kit >= 0) {
                    models[modelCount++] = idktype_get_model(_IdkType.instances[kit]);
                }
            }

            Model *model = model_from_models(models, modelCount, false);
            for (int part = 0; part < 5; part++) {
                if (c->design_colors[part] != 0) {
                    model_recolor(model, DESIGN_BODY_COLOR[part][0], DESIGN_BODY_COLOR[part][c->design_colors[part]]);
                    if (part == 1) {
                        model_recolor(model, DESIGN_HAIR_COLOR[0], DESIGN_HAIR_COLOR[c->design_colors[part]]);
                    }
                }
            }

            model_create_label_references(model, false);
            model_apply_transform(model, _SeqType.instances[c->local_player->pathing_entity.seqStandId]->frames[0]);
            model_calculate_normals(model, 64, 850, -30, -50, -30, true, false);
            component->model = model;
        }
    } else if (clientCode == 324) {
        if (!c->genderButtonImage0) {
            c->genderButtonImage0 = component->graphic;
            c->genderButtonImage1 = component->activeGraphic;
        }
        if (c->design_gender_male) {
            component->graphic = c->genderButtonImage1;
        } else {
            component->graphic = c->genderButtonImage0;
        }
    } else if (clientCode == 325) {
        if (!c->genderButtonImage0) {
            c->genderButtonImage0 = component->graphic;
            c->genderButtonImage1 = component->activeGraphic;
        }
        if (c->design_gender_male) {
            component->graphic = c->genderButtonImage0;
        } else {
            component->graphic = c->genderButtonImage1;
        }
    } else if (clientCode == 600) {
        strcpy(component->text, c->reportAbuseInput);
        if (_Client.loop_cycle % 20 < 10) {
            strcat(component->text, "|");
        } else {
            strcat(component->text, " ");
        }
    } else if (clientCode == 613) {
        if (!c->rights) {
            strcpy(component->text, "");
        } else if (c->reportAbuseMuteOption) {
            component->colour = RED;
            strcpy(component->text, "Moderator option: Mute player for 48 hours: <ON>");
        } else {
            component->colour = WHITE;
            strcpy(component->text, "Moderator option: Mute player for 48 hours: <OFF>");
        }
    } else if (clientCode == 650 || clientCode == 655) {
        if (c->lastAddress == 0) {
            strcpy(component->text, "");
        } else {
            char text[HALF_STR];
            if (c->daysSinceLastLogin == 0) {
                strcpy(text, "earlier today");
            } else if (c->daysSinceLastLogin == 1) {
                strcpy(text, "yesterday");
            } else {
                sprintf(text, "%d days ago", c->daysSinceLastLogin);
            }
            sprintf(component->text, "You last logged in %s from: %s", text, _Client.dns);
        }
    } else if (clientCode == 651) {
        if (c->unreadMessages == 0) {
            strcpy(component->text, "0 unread messages");
            component->colour = YELLOW;
        }
        if (c->unreadMessages == 1) {
            strcpy(component->text, "1 unread message");
            component->colour = GREEN;
        }
        if (c->unreadMessages > 1) {
            sprintf(component->text, "%d unread messages", c->unreadMessages);
            component->colour = GREEN;
        }
    } else if (clientCode == 652) {
        if (c->daysSinceRecoveriesChanged == 201) {
            strcpy(component->text, "");
        } else if (c->daysSinceRecoveriesChanged == 200) {
            strcpy(component->text, "You have not yet set any password recovery questions.");
        } else {
            char text[HALF_STR];
            if (c->daysSinceRecoveriesChanged == 0) {
                strcpy(text, "Earlier today");
            } else if (c->daysSinceRecoveriesChanged == 1) {
                strcpy(text, "Yesterday");
            } else {
                sprintf(text, "%d days ago", c->daysSinceRecoveriesChanged);
            }
            sprintf(component->text, "%s you changed your recovery questions", text);
        }
    } else if (clientCode == 653) {
        if (c->daysSinceRecoveriesChanged == 201) {
            strcpy(component->text, "");
        } else if (c->daysSinceRecoveriesChanged == 200) {
            strcpy(component->text, "We strongly recommend you do so now to secure your account.");
        } else {
            strcpy(component->text, "If you do not remember making this change then cancel it immediately");
        }
    } else if (clientCode == 654) {
        if (c->daysSinceRecoveriesChanged == 201) {
            strcpy(component->text, "");
        } else if (c->daysSinceRecoveriesChanged == 200) {
            strcpy(component->text, "Do this from the 'account management' area on our front webpage");
        } else {
            strcpy(component->text, "Do this from the 'account management' area on our front webpage");
        }
    }
}

void client_set_lowmem(void) {
    _World3D.lowMemory = true;
    _Pix3D.lowMemory = true;
    _Client.lowmem = true;
    _World.lowMemory = true;
}

void client_set_highmem(void) {
    _World3D.lowMemory = false;
    _Pix3D.lowMemory = false;
    _Client.lowmem = false;
    _World.lowMemory = false;
}

static const char *formatObjCountTagged(int amount) {
    static char s[SIXTY_STR];

    char tmp[14];
    sprintf(tmp, "%d", amount);

    int len = (int)strlen(tmp);
    for (int i = len - 3; i > 0; i -= 3) {
        for (int k = len; k >= i; k--) {
            tmp[k + 1] = tmp[k];
        }
        tmp[i] = ',';
        len++;
    }

    if (len > 8) {
        sprintf(s, " @gre@%.*s million @whi@(%s)", len - 8, tmp, tmp);
    } else if (len > 4) {
        sprintf(s, " @cya@%.*sK @whi@(%s)", len - 4, tmp, tmp);
    } else {
        sprintf(s, " %s", tmp);
    }

    return s;
}

static const char *formatObjCount(int amount) {
    static char s[12];
    if (amount < 100000) {
        sprintf(s, "%d", amount);
    } else if (amount < 10000000) {
        sprintf(s, "%dK", amount / 1000);
    } else {
        sprintf(s, "%dM", amount / 1000000);
    }
    return s;
}

static char *getIntString(int value) {
    return value < 999999999 ? valueof(value) : platform_strndup("*", 1);
}

static void client_draw_interface(Client *c, Component *com, int x, int y, int scrollY) {
    if (com->type != 0 || !com->childId || (com->hide && c->viewportHoveredInterfaceIndex != com->id && c->sidebarHoveredInterfaceIndex != com->id && c->chatHoveredInterfaceIndex != com->id)) {
        return;
    }

    int left = _Pix2D.left;
    int top = _Pix2D.top;
    int right = _Pix2D.right;
    int bottom = _Pix2D.bottom;

    pix2d_set_clipping(y + com->height, x + com->width, y, x);
    int children = com->childCount;

    for (int i = 0; i < children; i++) {
        int childX = com->childX[i] + x;
        int childY = com->childY[i] + y - scrollY;

        Component *child = _Component.instances[com->childId[i]];
        childX += child->x;
        childY += child->y;

        if (child->clientCode > 0) {
            client_update_interface_content(c, child);
        }

        if (child->type == 0) {
            if (child->scrollPosition > child->scroll - child->height) {
                child->scrollPosition = child->scroll - child->height;
            }

            if (child->scrollPosition < 0) {
                child->scrollPosition = 0;
            }

            client_draw_interface(c, child, childX, childY, child->scrollPosition);
            if (child->scroll > child->height) {
                client_draw_scrollbar(c, childX + child->width, childY, child->scrollPosition, child->scroll, child->height);
            }
        } else if (child->type == 2) {
            int slot = 0;

            for (int row = 0; row < child->height; row++) {
                for (int col = 0; col < child->width; col++) {
                    int slotX = childX + col * (child->marginX + 32);
                    int slotY = childY + row * (child->marginY + 32);

                    if (slot < 20) {
                        slotX += child->invSlotOffsetX[slot];
                        slotY += child->invSlotOffsetY[slot];
                    }

                    if (child->invSlotObjId[slot] > 0) {
                        int dx = 0;
                        int dy = 0;
                        int id = child->invSlotObjId[slot] - 1;

                        if ((slotX >= -32 && slotX <= _Pix2D.width && slotY >= -32 && slotY <= _Pix2D.height) || (c->obj_drag_area != 0 && c->objDragSlot == slot)) {
                            Pix24 *icon = NULL;
                            bool selected_use=c->obj_selected==1&&c->objSelectedSlot==slot&&c->objSelectedInterface==child->id;
                            /* Item-use feedback is required in both layouts,
                             * independent of the ordinary outline preference. */
                            if (_Custom.item_outlines || selected_use) {
                                icon = objtype_get_icon_outline(id, child->invSlotObjCount[slot], selected_use?WHITE:0);
                            } else {
                                icon = objtype_get_icon(id, child->invSlotObjCount[slot]);
                            }
                            if (c->obj_drag_area != 0 && c->objDragSlot == slot && c->objDragInterfaceId == child->id) {
                                dx = c->shell->mouse_x - c->objGrabX;
                                dy = c->shell->mouse_y - c->objGrabY;

                                if (dx < 5 && dx > -5) {
                                    dx = 0;
                                }

                                if (dy < 5 && dy > -5) {
                                    dy = 0;
                                }

                                if (c->obj_drag_cycles < 5) {
                                    dx = 0;
                                    dy = 0;
                                }

                                pix24_draw_alpha(icon, 128, slotX + dx, slotY + dy);
                            } else if (c->selected_area != 0 && c->selectedItem == slot && c->selectedInterface == child->id) {
                                pix24_draw_alpha(icon, 128, slotX, slotY);
                            } else {
                                pix24_draw(icon, slotX, slotY);
                            }

                            if (icon->crop_w == 33 || child->invSlotObjCount[slot] != 1) {
                                int count = child->invSlotObjCount[slot];
                                drawString(c->font_plain11, slotX + dx + 1, slotY + 10 + dy, formatObjCount(count), BLACK);
                                drawString(c->font_plain11, slotX + dx, slotY + 9 + dy, formatObjCount(count), YELLOW);
                            }

                            if (selected_use) pix24_free(icon);
                        }
                    } else if (child->invSlotSprite && slot < 20) {
                        Pix24 *image = child->invSlotSprite[slot];

                        if (image) {
                            pix24_draw(image, slotX, slotY);
                        }
                    }

                    slot++;
                }
            }
        } else if (child->type == 3) {
            if (child->fill) {
                pix2d_fill_rect(childX, childY, child->colour, child->width, child->height);
            } else {
                pix2d_draw_rect(childX, childY, child->colour, child->width, child->height);
            }
        } else if (child->type == 4) {
            PixFont *font = child->font;
            int color = child->colour;
            char text[DOUBLE_STR];
            strcpy(text, child->text);

            int dialogueHover=vita_dialogue_hover(c,child->id);
            if ((dialogueHover==1 || (dialogueHover<0 && (c->chatHoveredInterfaceIndex == child->id || c->sidebarHoveredInterfaceIndex == child->id || c->viewportHoveredInterfaceIndex == child->id))) && child->overColour != 0) {
                color = child->overColour;
            }

            if (client_execute_interface_script(c, child)) {
                color = child->activeColour;

                if (strlen(child->activeText) > 0) {
                    strcpy(text, child->activeText);
                }
            }

            if (child->buttonType == BUTTON_CONTINUE && c->pressed_continue_option) {
                strcpy(text, "Please wait...");
                color = child->colour;
            }

            const char *prompt=vita_dialogue_prompt(c,child->id,text);
            if(prompt!=text)strcpy(text,prompt);
            color=vita_interface_text_colour(c,child->id,color);

            for (int lineY = childY + font->height; strlen(text) > 0; lineY += font->height) {
                if (indexof_chr(text, '%') != -1) {
                    do {
                        int index = indexof(text, "%1");
                        if (index == -1) {
                            break;
                        }

                        char *sub = substring(text, 0, index);
                        char *value = getIntString(client_execute_clientscript1(c, child, 0));
                        char *sub1 = substring(text, index + 2, strlen(text));
                        sprintf(text, "%s%s%s", sub, value, sub1);
                        free(sub);
                        free(value);
                        free(sub1);
                    } while (true);

                    do {
                        int index = indexof(text, "%2");
                        if (index == -1) {
                            break;
                        }

                        char *sub = substring(text, 0, index);
                        char *value = getIntString(client_execute_clientscript1(c, child, 1));
                        char *sub1 = substring(text, index + 2, strlen(text));
                        sprintf(text, "%s%s%s", sub, value, sub1);
                        free(sub);
                        free(value);
                        free(sub1);
                    } while (true);

                    do {
                        int index = indexof(text, "%3");
                        if (index == -1) {
                            break;
                        }

                        char *sub = substring(text, 0, index);
                        char *value = getIntString(client_execute_clientscript1(c, child, 2));
                        char *sub1 = substring(text, index + 2, strlen(text));
                        sprintf(text, "%s%s%s", sub, value, sub1);
                        free(sub);
                        free(value);
                        free(sub1);
                    } while (true);

                    do {
                        int index = indexof(text, "%4");
                        if (index == -1) {
                            break;
                        }

                        char *sub = substring(text, 0, index);
                        char *value = getIntString(client_execute_clientscript1(c, child, 3));
                        char *sub1 = substring(text, index + 2, strlen(text));
                        sprintf(text, "%s%s%s", sub, value, sub1);
                        free(sub);
                        free(value);
                        free(sub1);
                    } while (true);

                    do {
                        int index = indexof(text, "%5");
                        if (index == -1) {
                            break;
                        }

                        char *sub = substring(text, 0, index);
                        char *value = getIntString(client_execute_clientscript1(c, child, 4));
                        char *sub1 = substring(text, index + 2, strlen(text));
                        sprintf(text, "%s%s%s", sub, value, sub1);
                        free(sub);
                        free(value);
                        free(sub1);
                    } while (true);
                }

                int newline = indexof(text, "\\n");
                char split[DOUBLE_STR];
                if (newline != -1) {
                    char *sub = substring(text, 0, newline);
                    strcpy(split, sub);
                    free(sub);
                    char *sub1 = substring(text, newline + 2, strlen(text));
                    strcpy(text, sub1);
                    free(sub1);
                } else {
                    strcpy(split, text);
                    strcpy(text, "");
                }

                if (child->center) {
                    drawStringTaggableCenter(font, split, childX + child->width / 2, lineY, color, child->shadowed);
                } else {
                    drawStringTaggable(font, childX, lineY, split, color, child->shadowed);
                }
            }
        } else if (child->type == 5) {
            Pix24 *image;
            if (client_execute_interface_script(c, child)) {
                image = child->activeGraphic;
            } else {
                image = child->graphic;
            }

            if (image) {
                pix24_draw(image, childX, childY);
            }
        } else if (child->type == 6) {
            int tmpX = _Pix3D.center_x;
            int tmpY = _Pix3D.center_y;

            _Pix3D.center_x = childX + child->width / 2;
            _Pix3D.center_y = childY + child->height / 2;

            int eyeY = _Pix3D.sin_table[child->xan] * child->zoom >> 16;
            int eyeZ = _Pix3D.cos_table[child->xan] * child->zoom >> 16;

            bool active = client_execute_interface_script(c, child);
            int seqId;
            if (active) {
                seqId = child->activeAnim;
            } else {
                seqId = child->anim;
            }

            Model *model;
            bool _free = false;
            if (seqId == -1) {
                model = component_get_model2(child, -1, -1, active, &_free);
            } else {
                SeqType *seq = _SeqType.instances[seqId];
                model = component_get_model2(child, seq->frames[child->seqFrame], seq->iframes[child->seqFrame], active, &_free);
            }

            if (model) {
                model_draw_simple(model, 0, child->yan, 0, child->xan, 0, eyeY, eyeZ);
                if (_free) {
                    model_free_label_references(model);
                    model_free_calculate_normals(model);
                    model_free_share_colored(model, true, true, false);
                }
            }

            _Pix3D.center_x = tmpX;
            _Pix3D.center_y = tmpY;
        } else if (child->type == 7) {
            PixFont *font = child->font;
            int slot = 0;
            for (int row = 0; row < child->height; row++) {
                for (int col = 0; col < child->width; col++) {
                    if (child->invSlotObjId[slot] > 0) {
                        ObjType *obj = objtype_get(child->invSlotObjId[slot] - 1);
                        char text[MAX_STR];
                        strcpy(text, obj->name);
                        if (obj->stackable || child->invSlotObjCount[slot] != 1) {
                            char tmp[HALF_STR];
                            strcpy(tmp, text);
                            sprintf(text, "%s x%s", tmp, formatObjCountTagged(child->invSlotObjCount[slot]));
                        }

                        int textX = childX + col * (child->marginX + 115);
                        int textY = childY + row * (child->marginY + 12);

                        if (child->center) {
                            drawStringTaggableCenter(font, text, textX + child->width / 2, textY, child->colour, child->shadowed);
                        } else {
                            drawStringTaggable(font, textX, textY, text, child->colour, child->shadowed);
                        }
                    }

                    slot++;
                }
            }
        }
    }

    pix2d_set_clipping(bottom, right, top, left);
}

void client_draw_menu(Client *c) {
    if(ve_menu_separate(c))return;
    int x = c->menu_x;
    int y = c->menu_y;
    int mouseX=c->shell->mouse_x,mouseY=c->shell->mouse_y;
    if(c->menu_area==0){mouseX-=ve_enabled()?0:8;mouseY-=ve_enabled()?0:11;}
    if(c->menu_area==1){mouseX-=ui474_side_x();mouseY-=ui474_side_y();}
    if(c->menu_area==2){mouseX-=22;mouseY-=(ve_enabled()?ve_chat_text_y():lc_y(LC_CHAT,UI_BOTTOM(375)));}
    client_raster_menu(c,x,y,mouseX,mouseY);
}

static void client_raster_menu(Client*c,int x,int y,int mouseX,int mouseY){
    int w = c->menu_width;
    int h = c->menu_height;
    int background = OPTIONS_MENU;
    pix2d_fill_rect(x, y, background, w, h);
    pix2d_fill_rect(x + 1, y + 1, BLACK, w - 2, 16);
    pix2d_draw_rect(x + 1, y + 18, BLACK, w - 2, h - 19);

    drawString(c->font_bold12, x + 3, y + 14, "Choose Option", background);

    for (int i = 0; i < c->menu_size; i++) {
        int optionY = y + (c->menu_size - 1 - i) * 15 + 31;
        int rgb = WHITE;
        if (mouseX > x && mouseX < x + w && mouseY > optionY - 13 && mouseY < optionY + 3) {
            rgb = YELLOW;
        }
#ifdef __vita__
        if (c->vita_menu_active) {
            rgb = i == c->vita_menu_selected ? YELLOW : WHITE;
            if (i == c->vita_menu_selected) {
                pix2d_fill_rect(x + 2, optionY - 12, 0x302B23, w - 4, 15);
            }
        }
#endif
        drawStringTaggable(c->font_bold12, x + 3, optionY, c->menu_option[i], rgb, true);
    }
}

void client_draw_error(Client *c) {
    platform_set_color(BLACK);
    platform_fill_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    gameshell_set_framerate(c->shell, 1);

    if (c->error_loading) {
        c->flame_active = false;
        int y = 35;

        platform_set_font("Helvetica", true, 16);
        platform_set_color(YELLOW);
        platform_draw_string("Sorry, an error has occured whilst loading RuneScape", 30, y);
        y += 50;

        platform_set_color(WHITE);
        platform_draw_string("To fix this try the following (in order):", 30, y);
        y += 50;

        platform_set_color(WHITE);
        platform_set_font("Helvetica", true, 12);
        platform_draw_string("1: Try closing ALL open web-browser windows, and reloading", 30, y);
        y += 30;

        platform_draw_string("2: Try clearing your web-browsers cache from tools->internet options", 30, y);
        y += 30;

        platform_draw_string("3: Try using a different game-world", 30, y);
        y += 30;

        platform_draw_string("4: Try rebooting your computer", 30, y);
        y += 30;

        platform_draw_string("5: Try selecting a different version of Java from the play-game menu", 30, y);
    }

    if (c->error_host) {
        c->flame_active = false;
        platform_set_font("Helvetica", true, 20);
        platform_set_color(WHITE);
        platform_draw_string("Error - unable to load game!", 50, 50);
        platform_draw_string("To play RuneScape make sure you play from", 50, 100);
        platform_draw_string("http://www.runescape.com", 50, 150);
    }

    if (c->error_started) {
        c->flame_active = false;
        int y = 35;

        platform_set_color(YELLOW);
        platform_draw_string("Error a copy of RuneScape already appears to be loaded", 30, y);
        y += 50;

        platform_set_color(WHITE);
        platform_draw_string("To fix this try the following (in order):", 30, y);
        y += 50;

        platform_set_color(WHITE);
        platform_set_font("Helvetica", true, 12);
        platform_draw_string("1: Try closing ALL open web-browser windows, and reloading", 30, y);
        y += 30;

        platform_draw_string("2: Try rebooting your computer, and reloading", 30, y);
        y += 30;
    }
}

static void client_draw_title_panel(Client*c,bool loading){
 if(!c->title_restored){pixmap_draw(c->image_title4,TITLE_X(214),TITLE_Y(186));return;}
 memcpy(c->title_panel_scaled->pixels,c->title_panel_background->pixels,360*200*sizeof(int));
 int width=loading?325:342,height=loading?181:190;
 int ox=loading?19:10,oy=loading?10:5;
 for(int dy=0;dy<height;dy++)for(int dx=0;dx<width;dx++){
  int sx=title_panel_x[loading?title_loading_x[dx]:dx],sy=title_panel_y[loading?title_loading_y[dy]:dy],color=c->image_title4->pixels[sy*360+sx];
  int bx=sx-c->image_titlebox->crop_x,by=sy-c->image_titlebox->crop_y;
  bool opaque=bx>=0&&bx<c->image_titlebox->width&&by>=0&&by<c->image_titlebox->height&&c->image_titlebox->pixels[by*c->image_titlebox->width+bx]!=0;
  if(color||opaque)c->title_panel_scaled->pixels[(dy+oy)*360+dx+ox]=color;
 }
 pixmap_draw(c->title_panel_scaled,TITLE_X(214),TITLE_Y(186));
}

void client_draw_title_screen(Client *c) {
    client_load_title(c);
    if(c->title_restored){vita_title_draw_panel(c,false,NULL,0);goto title_background;}
    pixmap_bind(c->image_title4);
        if(c->title_restored){memset(c->image_title4->pixels,0,360*200*sizeof(int));pix8_draw(c->image_titlebox,0,0);}
    pix8_draw(c->image_titlebox, 0, 0);

    int w = 360;
    int h = 200;

    if (c->title_screen_state == 0) {
        int y = h / 2 - 20;
        drawStringTaggableCenter(c->font_bold12, "Welcome to RuneScape", w / 2, y, YELLOW, true);

        int x = w / 2 - 80;
        y = h / 2 + 20;

        pix8_draw(c->image_titlebutton, x - 73, y - 20);
        drawStringTaggableCenter(c->font_bold12, "New user", x, y + 5, WHITE, true);

        x = w / 2 + 80;
        pix8_draw(c->image_titlebutton, x - 73, y - 20);
        drawStringTaggableCenter(c->font_bold12, "Existing User", x, y + 5, WHITE, true);
    } else if (c->title_screen_state == 2) {
        int y = h / 2 - 40;
        if (strlen(c->login_message0) > 0) {
            drawStringTaggableCenter(c->font_bold12, c->login_message0, w / 2, y - 15, YELLOW, true);
            drawStringTaggableCenter(c->font_bold12, c->login_message1, w / 2, y, YELLOW, true);
            y += 30;
        } else {
            drawStringTaggableCenter(c->font_bold12, c->login_message1, w / 2, y - 7, YELLOW, true);
            y += 30;
        }

        char username[MAX_STR];
        sprintf(username, "Username: %s%s", c->username, c->title_login_field == 0 && _Client.loop_cycle % 40 < 20 ? "@yel@|" : "");
        drawStringTaggable(c->font_bold12, w / 2 - 90, y, username, WHITE, true);
        y += 15;

        char password[MAX_STR];
        char *censored = jstring_to_asterisk(c->password);
        sprintf(password, "Password: %s%s", censored, c->title_login_field == 1 && _Client.loop_cycle % 40 < 20 ? "@yel@|" : "");
        free(censored);
        drawStringTaggable(c->font_bold12, w / 2 - 88, y, password, WHITE, true);
        y += 15;

        int x = w / 2 - 80;
        y = h / 2 + 50;
        pix8_draw(c->image_titlebutton, x - 73, y - 20);
        drawStringTaggableCenter(c->font_bold12,title_new_player?"Create Player":"Login",x,y+5,WHITE,true);

        x = w / 2 + 80;
        pix8_draw(c->image_titlebutton, x - 73, y - 20);
        drawStringTaggableCenter(c->font_bold12, "Cancel", x, y + 5, WHITE, true);
    } else if (c->title_screen_state == 3) {
        drawStringTaggableCenter(c->font_bold12, "Create a free account", w / 2, h / 2 - 60, YELLOW, true);

        int y = h / 2 - 35;
        drawStringTaggableCenter(c->font_bold12, "To create a new account you need to", w / 2, y, WHITE, true);
        y += 15;

        drawStringTaggableCenter(c->font_bold12, "go back to the main RuneScape webpage", w / 2, y, WHITE, true);
        y += 15;

        drawStringTaggableCenter(c->font_bold12, "and choose the red 'create account'", w / 2, y, WHITE, true);
        y += 15;

        drawStringTaggableCenter(c->font_bold12, "button at the top right of that page.", w / 2, y, WHITE, true);
        y += 15;

        int x = w / 2;
        y = h / 2 + 50;
        pix8_draw(c->image_titlebutton, x - 73, y - 20);
        drawStringTaggableCenter(c->font_bold12, "Cancel", x, y + 5, WHITE, true);
    }

    client_draw_title_panel(c,false);
title_background:
#ifdef GL11
    c->redraw_background = true;

    /* Restored-title flame animation uses its own clipped textures. These
     * two images are immutable background artwork in that mode. */
    if(!c->title_restored){c->image_title0->dirty=true;c->image_title1->dirty=true;}
    pixmap_draw(c->image_title0, TITLE_FLAME_LEFT_X, TITLE_Y(0));
    pixmap_draw(c->image_title1, TITLE_FLAME_RIGHT_X, TITLE_Y(0));
#endif
    if (c->redraw_background) {
        c->redraw_background = false;
        pixmap_draw(c->image_title0, TITLE_FLAME_LEFT_X, TITLE_Y(0));
        pixmap_draw(c->image_title1, TITLE_FLAME_RIGHT_X, TITLE_Y(0));
        pixmap_draw(c->image_title2, TITLE_TOP_X, TITLE_TOP_Y);
        pixmap_draw(c->image_title3, TITLE_BOTTOM_X, TITLE_BOTTOM_Y);
        pixmap_draw(c->image_title5, TITLE_LEFT_BASE_X, TITLE_Y(265));
        pixmap_draw(c->image_title6, TITLE_X(574), TITLE_Y(265));
        pixmap_draw(c->image_title7, TITLE_LEFT_MID_X, TITLE_Y(186));
        pixmap_draw(c->image_title8, TITLE_X(574), TITLE_Y(186));
    }

    client_run_flames(c); // NOTE: random placement of run_flames
    title_theme_draw(c);
    ve_ui_probe(c);
}

void client_unload(Client *c) {
    r109_clear();
    vita_title_invalidate();
    bump_allocator_free();

    model_free_global();
    animbase_free_global();
    animframe_free_global();
    component_free_global();
    pix3d_free_global();
    tone_free_global();
    wave_free_global();
    objtype_free_global();
    loctype_free_global();
    npctype_free_global();
    seqtype_free_global();
    varptype_free_global();
    playerentity_free_global();
    spotanimtype_free_global();
    idktype_free_global();
    flotype_free_global();
    packet_free_global();
    world3d_free_global();
    wordfilter_free_global();

    client_free(c);
}

#ifdef __EMSCRIPTEN__
#include "emscripten.h"

EM_JS(bool, get_host_js, (char *socketip, size_t len, int *http_port), {
    const url = new URL(window.location.href);
    stringToUTF8(url.hostname, socketip, len);
    if (url.port && url.hostname != 'localhost' && url.hostname != '127.0.0.1') {
        HEAP32[http_port >> 2] = parseInt(url.port, 10);
    }
    const secured = url.protocol == 'https';
    const protocol = secured ? 'wss' : 'ws';
    // TODO: check https://github.com/emscripten-core/emscripten/issues/22969
    SOCKFS.websocketArgs = {'url' : protocol + '://'};
    return secured;
})

static bool secured = false;
#endif

int main(int argc, char **argv) {
#ifdef BUILD_A_EMBEDDED
    extern void ba_vita_init(void);
    ba_vita_init();
#endif
    // init screen before logging is required for some platforms
    if (!platform_init()) {
        rs2_error("Failed to init platform!\n");
        rs2_sleep(5000);
        return 1;
    }
    // to print argv on emscripten you need to print index to flush instead of just \n?
    rs2_log("RS2 user client - release #%d\n", _Client.clientversion);

#ifdef __EMSCRIPTEN__
    // we fetch instead of preload config.ini to avoid leaking account details
    emscripten_wget("config.ini", "config.ini");

    if (argc != 5) {
        if (load_ini_args()) {
            _Client.lowmem ? client_set_lowmem() : client_set_highmem();
            goto init;
        }
        rs2_error("Usage: node-id, port-offset, [lowmem/highmem], [free/members]\n");
        return 0;
    }

    secured = get_host_js(_Client.socketip, MAX_STR - 1, &_Custom.http_port);
    _Client.nodeid = atoi(argv[1]);
    _Client.portoff = atoi(argv[2]);

    const char *lowmem = argv[3];
    if (lowmem && strcmp(lowmem, "1") == 0) {
        client_set_lowmem();
    } else {
        client_set_highmem();
    }

    const char *_free = argv[4];
    _Client.members = !_free || strcmp(_free, "1") != 0;
#else
#ifdef __TINYC__ // tcc -run passes many args
    if (load_ini_args()) {
#else
    // some console sdks (nxdk) have argc set to 0 with empty argv
    if (argc <= 1 && load_ini_args()) {
#endif
        _Client.lowmem ? client_set_lowmem() : client_set_highmem();
        goto init;
    }

    if (argc != 5) {
        rs2_error("Usage: node-id, port-offset, [lowmem/highmem], [free/members]\n");
        return 0;
    } else {
        _Client.nodeid = atoi(argv[1]);
        _Client.portoff = atoi(argv[2]);
        if (strcmp(argv[3], "lowmem") == 0) {
            client_set_lowmem();
        } else {
            if (strcmp(argv[3], "highmem") != 0) {
                rs2_error("Usage: node-id, port-offset, [lowmem/highmem], [free/members]\n");
                return 0;
            }
            client_set_highmem();
        }
        if (strcmp(argv[4], "free") == 0) {
            _Client.members = false;
        } else {
            if (strcmp(argv[4], "members") != 0) {
                rs2_error("Usage: node-id, port-offset, [lowmem/highmem], [free/members]\n");
                return 0;
            }
            _Client.members = true;
        }
    }
#endif

init:
    srand(0);
    client_init_global();
    model_init_global();
    packet_init_global();
    pix3d_init_global();
    pixfont_init_global();
    playerentity_init_global();
    world_init_global();
    world3d_init_global();

    Client *c = client_new();
    load_ini_config(c);
    gameshell_init_application(c, SCREEN_WIDTH, SCREEN_HEIGHT);
    return 0;
}

void client_free(Client *c) {
    free(c->stream);
    client_scenemap_free(c);
    gameshell_free(c->shell);
    pixfont_free(c->font_plain11);
    pixfont_free(c->font_plain12);
    pixfont_free(c->font_bold12);
    pixfont_free(c->font_quill8);
    jagfile_free(c->archive_title);

    ve_release(c);
    if (c->area_chatback) {
        // other images are allocated at same time
        pixmap_free(c->area_chatback);
        pixmap_free(c->area_mapback);
        pixmap_free(c->area_sidebar);
        pixmap_free(c->area_viewport);
        pixmap_free(c->area_backbase1);
        pixmap_free(c->area_backbase2);
        pixmap_free(c->area_backhmid1);
    }

    if (c->image_titlebox) {
        // other images are allocated at same time
        pix8_free(c->image_titlebox);
        pix8_free(c->image_titlebutton);
#ifndef DISABLE_FLAMES
        for (int i = 0; i < 12; i++) {
            pix8_free(c->image_runes[i]);
        }
        free(c->image_runes);
        free(c->flame_gradient0);
        free(c->flame_gradient1);
        free(c->flame_gradient2);
        free(c->flame_gradient);
        free(c->flame_buffer0);
        free(c->flame_buffer1);
        free(c->flame_buffer3);
        free(c->flame_buffer2);
        pix24_free(c->image_flames_left);
        pix24_free(c->image_flames_right);
#endif
        if(c->title_restored){pixmap_free(c->title_panel_scaled);pix24_free(c->title_panel_background);c->title_panel_scaled=NULL;c->title_panel_background=NULL;pixmap_free(c->title_flame_left);pixmap_free(c->title_flame_right);pixmap_free(c->title_flame_clip_left);pixmap_free(c->title_flame_clip_right);c->title_restored=false;}
        pixmap_free(c->image_title0);
        pixmap_free(c->image_title1);
        pixmap_free(c->image_title2);
        pixmap_free(c->image_title3);
        pixmap_free(c->image_title4);
        pixmap_free(c->image_title5);
        pixmap_free(c->image_title6);
        pixmap_free(c->image_title7);
        pixmap_free(c->image_title8);
    }

    pix24_free(c->image_minimap);
    pix8_free(c->image_invback);
    pix8_free(c->image_chatback);
    pix8_free(c->image_mapback);
    pix8_free(c->image_backbase1);
    pix8_free(c->image_backbase2);
    pix8_free(c->image_backhmid1);
    for (int i = 0; i < 13; i++) {
        pix8_free(c->image_sideicons[i]);
    }
    free(c->image_sideicons);
    pix24_free(c->image_compass);
    for (int i = 0; i < 65; i++) {
        if (c->image_mapscene[i]) {
            pix8_free(c->image_mapscene[i]);
        }
    }
    free(c->image_mapscene);
    for (int i = 0; i < 65; i++) {
        if (c->image_mapfunction[i]) {
            pix24_free(c->image_mapfunction[i]);
        }
    }
    free(c->image_mapfunction);
    for (int i = 0; i < 20; i++) {
        if (c->image_hitmarks[i]) {
            pix24_free(c->image_hitmarks[i]);
        }
    }
    free(c->image_hitmarks);
    for (int i = 0; i < 20; i++) {
        if (c->image_headicons[i]) {
            pix24_free(c->image_headicons[i]);
        }
    }
    free(c->image_headicons);
    pix24_free(c->image_mapflag);
    for (int i = 0; i < 8; i++) {
        pix24_free(c->image_crosses[i]);
    }
    free(c->image_crosses);
    pix24_free(c->image_mapdot0);
    pix24_free(c->image_mapdot1);
    pix24_free(c->image_mapdot2);
    pix24_free(c->image_mapdot3);
    pix8_free(c->image_scrollbar0);
    pix8_free(c->image_scrollbar1);
    pix8_free(c->image_redstone1);
    pix8_free(c->image_redstone2);
    pix8_free(c->image_redstone3);
    pix8_free(c->image_redstone1h);
    pix8_free(c->image_redstone2h);
    pix8_free(c->image_redstone1v);
    pix8_free(c->image_redstone2v);
    pix8_free(c->image_redstone3v);
    pix8_free(c->image_redstone1hv);
    pix8_free(c->image_redstone2hv);
    pixmap_free(c->area_backleft1);
    pixmap_free(c->area_backleft2);
    pixmap_free(c->area_backright1);
    pixmap_free(c->area_backright2);
    pixmap_free(c->area_backtop1);
    pixmap_free(c->area_backtop2);
    pixmap_free(c->area_backvmid1);
    pixmap_free(c->area_backvmid2);
    pixmap_free(c->area_backvmid3);
    pixmap_free(c->area_backhmid2);
    free(c->compass_mask_line_offsets);
    free(c->compass_mask_line_lengths);
    free(c->minimap_mask_line_offsets);
    free(c->minimap_mask_line_lengths);
    packet_free(c->out);
    packet_free(c->in);
    packet_free(c->login);
    for (int i = 0; i < MAX_PLAYER_COUNT; i++) {
        free(c->players[i]);
        if (c->player_appearance_buffer[i]) {
            packet_free(c->player_appearance_buffer[i]);
        }
    }
    for (int i = 0; i < MAX_NPC_COUNT; i++) {
        free(c->npcs[i]);
    }

    free(c->design_colors);

    linklist_free(c->projectiles);
    linklist_free(c->spotanims);
    linklist_free(c->merged_locations);
    linklist_free(c->spawned_locations);
    for (int level = 0; level < 4; level++) {
        for (int x = 0; x < 104; x++) {
            for (int z = 0; z < 104; z++) {
                if (c->level_obj_stacks[level][x][z]) {
                    linklist_free(c->level_obj_stacks[level][x][z]);
                }
            }
            free(c->level_obj_stacks[level][x]);
        }
        free(c->level_obj_stacks[level]);
    }
    free(c->level_obj_stacks);
    linklist_clear(c->locList);
    linklist_free(c->locList);
    free(c->levelTileFlags);
    free(c->levelHeightmap);
    world3d_free(c->scene, 104, 4, 104);
    for (int level = 0; level < 4; level++) {
        collisionmap_free(c->levelCollisionMap[level]);
    }
    free(c->chat_interface);
    free(c->textureBuffer);
    free(c->area_chatback_offsets);
    free(c->area_sidebar_offsets);
    free(c->area_viewport_offsets);

    free(c);
}

Client *client_new(void) {
    Client *c = calloc(1, sizeof(Client));

    c->shell = gameshell_new();
    c->image_sideicons = calloc(13, sizeof(Pix8 *));
    c->image_mapscene = calloc(65, sizeof(Pix8 *));
    c->image_mapfunction = calloc(65, sizeof(Pix24 *));
    c->image_hitmarks = calloc(20, sizeof(Pix24 *));
    c->image_headicons = calloc(20, sizeof(Pix24 *));
    c->image_crosses = calloc(8, sizeof(Pix24 *));
    c->compass_mask_line_offsets = calloc(33, sizeof(int));
    c->compass_mask_line_lengths = calloc(33, sizeof(int));
    c->minimap_mask_line_offsets = calloc(151, sizeof(int));
    c->minimap_mask_line_lengths = calloc(151, sizeof(int));

    c->out = packet_alloc(1);
    c->in = packet_alloc(1);
    c->login = packet_alloc(1);
    c->orbit_camera_pitch = 128;

    c->minimap_level = -1;
    c->sticky_chat_interface_id = -1;
    c->chat_interface_id = -1;
    c->viewport_interface_id = -1;
    c->sidebar_interface_id = -1;
    c->selected_tab = 3;
    c->flashing_tab = -1;
    c->design_gender_male = true;
    c->design_colors = malloc(5 * sizeof(int));
    memset(c->tab_interface_id, -1, sizeof(c->tab_interface_id));
    c->projectiles = linklist_new();
    c->spotanims = linklist_new();
    c->merged_locations = linklist_new();
    c->spawned_locations = linklist_new();
    c->level_obj_stacks = calloc(4, sizeof(LinkList ***));
    for (int level = 0; level < 4; level++) {
        c->level_obj_stacks[level] = calloc(104, sizeof(LinkList **));
        for (int x = 0; x < 104; x++) {
            c->level_obj_stacks[level][x] = calloc(104, sizeof(LinkList *));
            for (int z = 0; z < 104; z++) {
                c->level_obj_stacks[level][x][z] = linklist_new();
            }
        }
    }
    c->chat_interface = calloc(1, sizeof(Component));
    c->locList = linklist_new();
    c->local_pid = -1;
    c->chat_scroll_height = 78;
    c->cameraOffsetXModifier = 2;
    c->cameraOffsetZModifier = 2;
    c->cameraOffsetYawModifier = 1;
    c->minimapAngleModifier = 2;
    c->minimapZoomModifier = 1;
    c->reportAbuseInterfaceID = -1;
    c->projectX = -1;
    c->projectY = -1;
    c->textureBuffer = calloc(16384, sizeof(int8_t));
    c->wave_enabled = true;
    c->midiActive = true;
    return c;
}

#ifdef __wasm
#ifdef __EMSCRIPTEN__
void *client_openurl(const char *name, int *size) {
    void *buffer = NULL;
    int error = 0;
    char url[PATH_MAX];
    sprintf(url, "%s://%s:%d/%s", secured ? "https" : "http", _Client.socketip, _Custom.http_port, name);
    emscripten_wget_data(url, &buffer, size, &error);
    if (error) {
        rs2_error("Error downloading %s: %d\n", url, error);
        return NULL;
    }
    return buffer;
}
#else
void *client_openurl(const char *name, int *size) {
    char url[PATH_MAX];
    bool secured = false;
    sprintf(url, "%s://%s:%d/%s", secured ? "https" : "http", _Client.socketip, _Custom.http_port, name);

    FILE *file = fopen(url, "rb");
    if (!file) {
        rs2_error("Error downloading %s: %d\n", url);
        return NULL;
    }
    // TODO this will break on newer caches
    #define MAX_FETCH 1 << 20
    uint8_t *buffer = malloc(MAX_FETCH);
    *size = fread(buffer, 1, MAX_FETCH, file);
    fclose(file);

    return buffer;
}
#endif

Jagfile *load_archive(Client *c, const char *name, int crc, const char *display_name, int progress) {
    int retry = 5;
    // int8_t *data = signlink.cacheload(name);
    int8_t *data = NULL; // TODO cacheload
    int size = 0;
    if (data) {
        int crc_value = rs_crc32(data, size);
        if (crc_value != crc) {
            rs2_log("%s archive CRC check failed\n", display_name);
            free(data);
            data = NULL;
            size = 0;
        }
    }

    if (data) {
        return jagfile_new(data, size);
    }

    while (!data) {
        char message[PATH_MAX];
        snprintf(message, sizeof(message), "Requesting %s", display_name);
        client_draw_progress(c, message, progress);

        snprintf(message, sizeof(message), "%s%d", name, crc);
        data = client_openurl(message, &size);
        if (!data) {
            for (int i = retry; i > 0; i--) {
                snprintf(message, sizeof(message), "Error loading - Will retry in %d secs.", i);
                client_draw_progress(c, message, progress);
                rs2_sleep(1000);
            }

            retry *= 2;
            if (retry > 60) {
                retry = 60;
            }
        }
    }

    // signlink.cachesave(name, data);
    return jagfile_new(data, size);
}
#else
void *client_openurl(const char *name, int *size) {
    (void)name, (void)size;
    return NULL;
}
Jagfile *load_archive(Client *c, const char *name, int crc, const char *display_name, int progress) {
    // TODO
    (void)display_name, (void)progress;
    int8_t *data;
    int8_t *header = malloc(6);
    char filename[PATH_MAX];
#ifdef _arch_dreamcast
    snprintf(filename, sizeof(filename), "cache/client/%s.", name);
#elif defined(NXDK)
    snprintf(filename, sizeof(filename), "D:\\cache\\client\\%s", name);
#else
    snprintf(filename, sizeof(filename), "rom/cache/client/%s", name);
#endif
    // rs2_log("Loading %s\n", filename);
    // TODO: add load messages?
    (void)c;

#ifdef ANDROID
    SDL_RWops *file = SDL_RWFromFile(filename, "rb");
#else
    FILE *file = fopen(filename, "rb");
#endif
    if (!file) {
        rs2_error("Failed to open file\n", strerror(errno));
        free(header);
        return NULL;
    }

#ifdef ANDROID
    if (SDL_RWread(file, header, 1, 6) != 6) {
#else
    if (fread(header, 1, 6, file) != 6) {
#endif
        rs2_error("Failed to read header\n", strerror(errno));
    }
    Packet *packet = packet_new(header, 6);
    packet->pos = 3;
    int file_size = g3(packet) + 6;
    int total_read = 6;
    data = malloc(file_size);
    memcpy(data, header, total_read); // or packet->data instead of header
    size_t remaining = file_size - total_read;
#ifdef ANDROID
    if (SDL_RWread(file, data + total_read, 1, remaining) != remaining) {
#else
    if (fread(data + total_read, 1, remaining, file) != remaining) {
#endif
        rs2_error("Failed to read file\n", strerror(errno));
    }
#ifdef ANDROID
    SDL_RWclose(file);
#else
    fclose(file);
#endif
    packet_free(packet);

    int crc_value = rs_crc32(data, file_size);
    if (crc_value != crc) {
        rs2_log("%s archive CRC check failed (update archive_checksums if login says RuneScape has been updated) TODO downloading\n", display_name);
        // free(data);
        // data = NULL;
    }

    return jagfile_new(data, file_size);
}
#endif

void client_load_title(Client *c) {
    if (c->image_title2) {
        return;
    }

    if (c->shell->draw_area) {
        pixmap_free(c->shell->draw_area);
        c->shell->draw_area = NULL;
    }
    ve_release(c);
    if (c->area_chatback) {
        // other images are allocated at same time
        pixmap_free(c->area_chatback);
        pixmap_free(c->area_mapback);
        pixmap_free(c->area_sidebar);
        pixmap_free(c->area_viewport);
        pixmap_free(c->area_backbase1);
        pixmap_free(c->area_backbase2);
        pixmap_free(c->area_backhmid1);
        c->area_chatback = NULL;
        c->area_mapback = NULL;
        c->area_sidebar = NULL;
        c->area_viewport = NULL;
        c->area_backbase1 = NULL;
        c->area_backbase2 = NULL;
        c->area_backhmid1 = NULL;
    }

    c->image_title0 = pixmap_new(128, 265);
    pix2d_clear();

    c->image_title1 = pixmap_new(128, 265);
    pix2d_clear();

    c->image_title2 = pixmap_new(TITLE_TOP_WIDTH, TITLE_TOP_HEIGHT);
    pix2d_clear();

    c->image_title3 = pixmap_new(TITLE_BOTTOM_WIDTH, TITLE_BOTTOM_HEIGHT);
    pix2d_clear();

    c->image_title4 = pixmap_new(360, 200);
    pix2d_clear();

    c->image_title5 = pixmap_new(TITLE_LEFT_BASE_WIDTH, 267);
    pix2d_clear();

    c->image_title6 = pixmap_new(TITLE_RIGHT_BASE_WIDTH, 267);
    pix2d_clear();

    c->image_title7 = pixmap_new(TITLE_LEFT_MID_WIDTH, 79);
    pix2d_clear();

    c->image_title8 = pixmap_new(TITLE_RIGHT_MID_WIDTH, 79);
    pix2d_clear();

    if (c->archive_title) {
        client_load_title_background(c);
        client_load_title_images(c);
    }

    c->redraw_background = true;
}

void client_draw_progress(Client *c, const char *message, int progress) {
    if(progress<0)progress=0;else if(progress>100)progress=100;
    client_load_title(c);
    if (!c->archive_title) {
        gameshell_draw_progress(c->shell, message, progress);
    } else {
        if(c->title_restored){vita_title_draw_panel(c,true,message,progress);goto progress_background;}
            pixmap_bind(c->image_title4);
        if(c->title_restored){memset(c->image_title4->pixels,0,360*200*sizeof(int));pix8_draw(c->image_titlebox,0,0);}
        int x = 360;
        int y = 200;
        int offsetY = 20;
        drawStringCenter(c->font_bold12, x / 2, y / 2 - offsetY - 26, "RuneScape is loading - please wait...", WHITE);
        int midY = y / 2 - offsetY - 18;
        pix2d_draw_rect(x / 2 - 152, midY, PROGRESS_RED, 304, 34);
        pix2d_draw_rect(x / 2 - 151, midY + 1, BLACK, 302, 32);
        pix2d_fill_rect(x / 2 - 150, midY + 2, PROGRESS_RED, progress * 3, 30);
        pix2d_fill_rect(x / 2 - 150 + progress * 3, midY + 2, BLACK, 300 - progress * 3, 30);
        drawStringCenter(c->font_bold12, x / 2, y / 2 + 5 - offsetY, message, WHITE);
        client_draw_title_panel(c,true);
progress_background:
#ifdef GL11
        c->redraw_background = true;
        if (c->flame_active) {
            pixmap_draw(c->image_title0, TITLE_FLAME_LEFT_X, TITLE_Y(0));
            pixmap_draw(c->image_title1, TITLE_FLAME_RIGHT_X, TITLE_Y(0));
        }
#endif
        if (c->redraw_background) {
            c->redraw_background = false;
            if (!c->flame_active) {
                pixmap_draw(c->image_title0, TITLE_FLAME_LEFT_X, TITLE_Y(0));
                pixmap_draw(c->image_title1, TITLE_FLAME_RIGHT_X, TITLE_Y(0));
            }
            pixmap_draw(c->image_title2, TITLE_TOP_X, TITLE_TOP_Y);
            pixmap_draw(c->image_title3, TITLE_BOTTOM_X, TITLE_BOTTOM_Y);
            pixmap_draw(c->image_title5, TITLE_LEFT_BASE_X, TITLE_Y(265));
            pixmap_draw(c->image_title6, TITLE_X(574), TITLE_Y(265));
            pixmap_draw(c->image_title7, TITLE_LEFT_MID_X, TITLE_Y(186));
            pixmap_draw(c->image_title8, TITLE_X(574), TITLE_Y(186));
        }

        platform_update_surface();
    }
}

void client_ui474_draw_interface(Client*c,Component*r,int x,int y,int scroll){client_draw_interface(c,r,x,y,scroll);}
#endif
