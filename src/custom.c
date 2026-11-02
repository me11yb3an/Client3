#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "allocator.h"
#include "client.h"
#include "custom.h"
#include "vita_enhanced.h"
#include "layoutcore.h"
#include "vita_wide_layout.h"
#include "datastruct/jstring.h"
#include "gameshell.h"
#include "platform.h"
#include "inputtracking.h"
#include "thirdparty/ini.h"

extern ClientData _Client;
extern InputTracking _InputTracking;

#if defined(__WII__) || defined(__3DS__) || defined(__WIIU__) || defined(__SWITCH__) || defined(__PSP__) || defined(__vita__) || defined(_arch_dreamcast) || defined(NXDK) || defined(__NDS__) || defined(ANDROID)
Custom _Custom = {.chat_era = 2, .http_port = 80, .show_performance = true};
#else
Custom _Custom = {.chat_era = 2, .http_port = 80};
#endif

// NOTE: having this file separate from client easily creates conflicts if other non-client entrypoints compile all c files

// macro assumes ini setting has the same name as the member variable
// works with pointers and non-pointers, no bounds checks for ints
#define INI_STR(dst, member)                                    \
    {                                                           \
        const char *tmp = ini_get(config, NULL, #member);       \
        if (tmp) {                                              \
            strncpy(dst->member, tmp, sizeof(dst->member) - 1); \
        }                                                       \
    }

#define INI_STR_LOG(dst, member) \
    INI_STR(dst, member);        \
    rs2_log("  %s: %s\n", #member, dst->member)

#define INI_STR_LOG_HIDDEN(dst, member)                \
    INI_STR(dst, member);                              \
    char *censored = jstring_to_asterisk(dst->member); \
    rs2_log("  %s: %s\n", #member, censored);          \
    free(censored)

#define INI_INT_LOG(dst, member, calc)                     \
    {                                                      \
        int tmp = 0;                                       \
        if (ini_sget(config, NULL, #member, "%d", &tmp)) { \
            *dst->member = tmp;                            \
            calc;                                          \
        }                                                  \
        rs2_log("  %s: %d\n", #member, *dst->member);      \
    }

bool load_ini_args(void) {
#ifdef NXDK
    ini_t *config = ini_load("D:\\config.ini");
#else
    ini_t *config = ini_load("config.ini");
#endif
    if (!config) {
        return false;
    }

    rs2_log("World config:\n");
    INI_STR_LOG((&_Client), socketip);
    INI_INT_LOG(&(&_Custom), http_port, );
    // world nodeid 1 = 10 (default)
    INI_INT_LOG(&(&_Client), nodeid, _Client.nodeid = 10 + _Client.nodeid - 1);
    INI_INT_LOG(&(&_Client), portoff, );
    INI_INT_LOG(&(&_Client), lowmem, );
#if defined(__PSP__) || defined(_arch_dreamcast) || defined(__NDS__) || defined(NXDK)
    // implicitly ignore highmem, avoids confusion as there's no way it'll load, except if xbox has mem expansion
    _Client.lowmem = true;
#endif
    INI_INT_LOG(&(&_Client), members, _Client.members = !_Client.members);

    rs2_log("\n");
    ini_free(config);
    return true;
}

void load_ini_config(Client *c) {
#ifdef NXDK
    ini_t *config = ini_load("D:\\config.ini");
#else
    ini_t *config = ini_load("config.ini");
#endif
    if (!config) {
        return;
    }

    rs2_log("Options config:\n");
    INI_STR_LOG((&_Client), rsa_exponent);
    INI_STR_LOG((&_Client), rsa_modulus);

    INI_STR_LOG(c, username);
    INI_STR_LOG_HIDDEN(c, password);

    INI_INT_LOG(&(&_Custom), hide_dns, );
    INI_INT_LOG(&(&_Custom), hide_debug_sprite, );
    INI_INT_LOG(&(&_Custom), allow_commands, );
    INI_INT_LOG(&(&_Custom), allow_debugprocs, );
    INI_INT_LOG(&(&_Custom), chat_era, );
    INI_INT_LOG(&(&_Custom), remember_username, );
    INI_INT_LOG(&(&_Custom), remember_password, );
    INI_INT_LOG(&(&_Custom), resizable, );
#ifdef ANDROID
    _Custom.resizable = true;
#endif
    INI_INT_LOG(&(&_Custom), item_outlines, );
    INI_INT_LOG(&(&_Custom), show_performance, );
#ifdef GL11
    _Custom.use_opengl11 = true;
#endif

    rs2_log("\n");
    ini_free(config);
}

/* void update_camera_editor(Client *c) {
    // holding ctrl
    int modifier = c->shell->action_key[5] == 1 ? 2 : 1;

    if (c->shell->action_key[6] == 1) {
        // holding shift
        if (c->shell->action_key[1] == 1) {
            // left
            c->cutsceneDstLocalTileX -= 1 * modifier;
            if (c->cutsceneDstLocalTileX < 1) {
                c->cutsceneDstLocalTileX = 1;
            }
        } else if (c->shell->action_key[2] == 1) {
            // right
            c->cutsceneDstLocalTileX += 1 * modifier;
            if (c->cutsceneDstLocalTileX > 102) {
                c->cutsceneDstLocalTileX = 102;
            }
        }

        if (c->shell->action_key[3] == 1) {
            // up
            if (c->shell->action_key[7] == 1) {
                // holding alt
                c->cutsceneDstHeight += 2 * modifier;
            } else {
                c->cutsceneDstLocalTileZ += 1;
                if (c->cutsceneDstLocalTileZ > 102) {
                    c->cutsceneDstLocalTileZ = 102;
                }
            }
        } else if (c->shell->action_key[4] == 1) {
            // down
            if (c->shell->action_key[7] == 1) {
                // holding alt
                c->cutsceneDstHeight -= 2 * modifier;
            } else {
                c->cutsceneDstLocalTileZ -= 1;
                if (c->cutsceneDstLocalTileZ < 1) {
                    c->cutsceneDstLocalTileZ = 1;
                }
            }
        }
    } else {
        if (c->shell->action_key[1] == 1) {
            // left
            c->cutsceneSrcLocalTileX -= 1 * modifier;
            if (c->cutsceneSrcLocalTileX < 1) {
                c->cutsceneSrcLocalTileX = 1;
            }
        } else if (c->shell->action_key[2] == 1) {
            // right
            c->cutsceneSrcLocalTileX += 1 * modifier;
            if (c->cutsceneSrcLocalTileX > 102) {
                c->cutsceneSrcLocalTileX = 102;
            }
        }

        if (c->shell->action_key[3] == 1) {
            // up
            if (c->shell->action_key[7] == 1) {
                // holding alt
                c->cutsceneSrcHeight += 2 * modifier;
            } else {
                c->cutsceneSrcLocalTileZ += 1 * modifier;
                if (c->cutsceneSrcLocalTileZ > 102) {
                    c->cutsceneSrcLocalTileZ = 102;
                }
            }
        } else if (c->shell->action_key[4] == 1) {
            // down
            if (c->shell->action_key[7] == 1) {
                // holding alt
                c->cutsceneSrcHeight -= 2 * modifier;
            } else {
                c->cutsceneSrcLocalTileZ -= 1 * modifier;
                if (c->cutsceneSrcLocalTileZ < 1) {
                    c->cutsceneSrcLocalTileZ = 1;
                }
            }
        }
    }

    c->cameraX = c->cutsceneSrcLocalTileX * 128 + 64;
    c->cameraZ = c->cutsceneSrcLocalTileZ * 128 + 64;
    c->cameraY = getHeightmapY(c, c->currentLevel, c->cutsceneSrcLocalTileX, c->cutsceneSrcLocalTileZ) - c->cutsceneSrcHeight;

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
} */

const char *classic_perf_display(int slot,const char *text){
#if defined(__vita__) && defined(GL11)
    static char displayed[6][128];
    static uint64_t updated[6];
    static unsigned valid;
    if(ve_enabled()||slot<0||slot>=6||strlen(text)>=sizeof displayed[0])return text;
    uint64_t now=rs2_now();
    if(!(valid&(1u<<slot))||now-updated[slot]>=250){
        strcpy(displayed[slot],text);updated[slot]=now;valid|=1u<<slot;
    }
    return displayed[slot];
#else
    (void)slot;return text;
#endif
}

static void draw_info_overlay_inner(Client *c,int x,int y,int local) {

    char buf[MAX_STR];
    if (_Custom.show_performance) {
        // skip corner of interfaces, makes text harder to read
        y += 13;
        // skip 2 possible spots of "Close Window"
        y += 13;
        y += 13;
        sprintf(buf, "LRU: %dK / %dK", bump_allocator_used() >> 10, bump_allocator_capacity() >> 10);
        drawStringRight(c->font_plain11, (local?x:PERF_RIGHT(x)), (local?y:PERF_BASELINE(y)), (local?buf:classic_perf_display(4,buf)), YELLOW, true);
        y += 13;
        sprintf(buf, "FPS: %d", c->shell->fps);
        drawStringRight(c->font_plain11, (local?x:PERF_RIGHT(x)), (local?y:PERF_BASELINE(y)), (local?buf:classic_perf_display(5,buf)), YELLOW, true);
        y += 13;
#if defined(__vita__)
#include <psp2/power.h>
        /* System-service queries are slow IPC. Sample at display cadence,
         * not once per rendered frame. FPS and allocator counts remain live. */
        static uint64_t power_at;
        static int power_ready, battery, arm_mhz, gpu_mhz;
        uint64_t power_now=rs2_now();
        if(!power_ready||power_now-power_at>=1000){
            battery=scePowerGetBatteryLifePercent();arm_mhz=scePowerGetArmClockFrequency();gpu_mhz=scePowerGetGpuClockFrequency();
            power_at=power_now;power_ready=1;
        }
        sprintf(buf, "BAT: %d%%", battery);
        drawStringRight(c->font_plain11, (local?x:PERF_RIGHT(x)), (local?y:PERF_BASELINE(y)), buf, YELLOW, true);
        y += 13;
        sprintf(buf, "CPU: %d MHz", arm_mhz);
        drawStringRight(c->font_plain11, (local?x:PERF_RIGHT(x)), (local?y:PERF_BASELINE(y)), buf, YELLOW, true);
        y += 13;
        sprintf(buf, "GPU: %d MHz", gpu_mhz);
        drawStringRight(c->font_plain11, (local?x:PERF_RIGHT(x)), (local?y:PERF_BASELINE(y)), buf, YELLOW, true);
        y += 13;
#endif
    }

    if (_Custom.camera_editor || _Custom.show_debug) {
        sprintf(buf, "Local Pos: %d, %d, %d", c->local_player->pathing_entity.x, c->local_player->pathing_entity.z, c->local_player->y);
        drawStringRight(c->font_plain11, x, y, buf, YELLOW, true);
        y += 13;

        sprintf(buf, "Camera Pos: %d, %d, %d", c->cameraX, c->cameraZ, c->cameraY);
        drawStringRight(c->font_plain11, x, y, buf, YELLOW, true);
        y += 13;

        sprintf(buf, "Camera Angle: %d, %d", c->cameraYaw, c->cameraPitch);
        drawStringRight(c->font_plain11, x, y, buf, YELLOW, true);
        y += 13;

        sprintf(buf, "Cutscene Source: %d, %d %d; %d, %d", c->cutsceneSrcLocalTileX, c->cutsceneSrcLocalTileZ, c->cutsceneSrcHeight, c->cutsceneMoveSpeed, c->cutsceneMoveAcceleration);
        drawStringRight(c->font_plain11, x, y, buf, YELLOW, true);
        y += 13;

        sprintf(buf, "Cutscene Destination: %d, %d %d; %d, %d", c->cutsceneDstLocalTileX, c->cutsceneDstLocalTileZ, c->cutsceneDstHeight, +c->cutsceneRotateSpeed, c->cutsceneRotateAcceleration);
        drawStringRight(c->font_plain11, x, y, buf, YELLOW, true);
        y += 13;
    }

    if (_Custom.camera_editor) {
        y += 13;
        drawStringRight(c->font_plain11, x, y, "Instructions:", YELLOW, true);
        y += 13;

        drawStringRight(c->font_plain11, x, y, "- Arrows to move Camera", YELLOW, true);
        y += 13;

        drawStringRight(c->font_plain11, x, y, "- Shift to control Source or Dest", YELLOW, true);
        y += 13;

        drawStringRight(c->font_plain11, x, y, "- Alt to control Height", YELLOW, true);
        y += 13;

        drawStringRight(c->font_plain11, x, y, "- Ctrl to control Modifier", YELLOW, true);
        y += 13;
    }
}

void draw_info_overlay(Client*c){draw_info_overlay_inner(c,507,13,0);}
void draw_info_overlay_local(Client*c,int x,int y){draw_info_overlay_inner(c,x,y,1);}

bool insideChatInputArea(Client *c) {
    if (!c->ingame || c->chat_interface_id>=0 || c->sticky_chat_interface_id>=0 ||
        c->viewport_interface_id>=0 || c->chatback_input_open || c->show_social_input ||
        c->modal_message[0] || (ve_enabled() && ve_chat_hidden(c))) return false;
    int x=ve_enabled()?ve_chat_text_x():lc_x(LC_CHAT,22);
    int y=ve_enabled()?ve_chat_text_y():lc_y(LC_CHAT,UI_BOTTOM(375));
    // Match the rendered separator and typing baseline (Enhanced 80/93, Classic 77/90).
    int top=y+(ve_enabled()?81:78);
    return c->shell->mouse_x>=x && c->shell->mouse_x<x+lc_chat_text_right(CHAT_TEXT_RIGHT) &&
           c->shell->mouse_y>=top && c->shell->mouse_y<y+96;
}

bool insideChatPopupArea(Client *c) {
    if (!c->ingame || !(c->chatback_input_open || c->show_social_input) ||
        (ve_enabled() && ve_chat_hidden(c))) return false;
    int x=ve_enabled()?ve_chat_text_x():lc_x(LC_CHAT,22);
    int y=ve_enabled()?ve_chat_text_y():lc_y(LC_CHAT,UI_BOTTOM(375));
    return c->shell->mouse_x>=x && c->shell->mouse_x<x+lc_chat_text_right(CHAT_TEXT_RIGHT) &&
           c->shell->mouse_y>=y && c->shell->mouse_y<y+96;
}

bool insideReportInterfaceTextArea(Client *c) {
    // custom: for report abuse input on mobile
    // actual component size is [233, 137, 58 14]
    // extended it a little bit for easier interaction, since the area to
    // touch is not obvious (it's a bit narrow)
    if (!c->ingame) {
        return false;
    }
    // either viewport or report-abuse interface Ids are bad
    if (c->viewport_interface_id == -1 || c->reportAbuseInterfaceID == -1) {
        return false;
    }
    // active viewport interface Id does not match
    if (c->viewport_interface_id != c->reportAbuseInterfaceID) {
        return false;
    }
    const int reportInputAreaX1 = 82;
    const int reportInputAreaY1 = 137;
    const int reportInputAreaX2 = reportInputAreaX1 + 366;
    const int reportInputAreaY2 = reportInputAreaY1 + 26;
    return c->shell->mouse_x >= reportInputAreaX1 && c->shell->mouse_x <= reportInputAreaX2 && c->shell->mouse_y >= reportInputAreaY1 && c->shell->mouse_y <= reportInputAreaY2;
}

bool insideUsernameArea(Client *c) {
    // 261 x 17
    const int usernameAreaX1 = 301;
    const int usernameAreaY1 = 262;
    const int usernameAreaX2 = usernameAreaX1 + 261;
    const int usernameAreaY2 = usernameAreaY1 + 17;
    return !c->ingame && c->title_screen_state == 2 && c->shell->mouse_x >= usernameAreaX1 && c->shell->mouse_x <= usernameAreaX2 && c->shell->mouse_y >= usernameAreaY1 && c->shell->mouse_y <= usernameAreaY2;
}

bool inPasswordArea(Client *c) {
    // 261 x 17
    const int passwordAreaX1 = 301;
    const int passwordAreaY1 = 279;
    const int passwordAreaX2 = passwordAreaX1 + 261;
    const int passwordAreaY2 = passwordAreaY1 + 17;
    return !c->ingame && c->title_screen_state == 2 && c->shell->mouse_x >= passwordAreaX1 && c->shell->mouse_x <= passwordAreaX2 && c->shell->mouse_y >= passwordAreaY1 && c->shell->mouse_y <= passwordAreaY2;
}

bool insideMobileInputArea(Client *c) {
    // custom: for mobile keyboard input
    return insideChatInputArea(c) || insideChatPopupArea(c) || insideUsernameArea(c) || inPasswordArea(c) || insideReportInterfaceTextArea(c);
}

bool insideTabArea(Client *c) {
    // 190 x 261
    const int tabAreaX1 = 562;
    const int tabAreaY1 = 231;
    const int tabAreaX2 = tabAreaX1 + 190;
    const int tabAreaY2 = tabAreaY1 + 261;
    return c->ingame && c->shell->mouse_x >= tabAreaX1 && c->shell->mouse_x <= tabAreaX2 && c->shell->mouse_y >= tabAreaY1 && c->shell->mouse_y <= tabAreaY2;
}

bool insideViewportArea(Client *c) {
    // 512 x 334
    const int viewportAreaX1 = 8;
    const int viewportAreaY1 = 11;
    const int viewportAreaX2 = viewportAreaX1 + 512;
    const int viewportAreaY2 = viewportAreaY1 + 334;
    return c->ingame && c->shell->mouse_x >= viewportAreaX1 && c->shell->mouse_x <= viewportAreaX2 && c->shell->mouse_y >= viewportAreaY1 && c->shell->mouse_y <= viewportAreaY2;
}
