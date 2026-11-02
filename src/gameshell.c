#include "ba_timing.h"
#include <stdio.h>
#ifdef BUILD_A_EMBEDDED
#include "embedded_server.h"
#endif
#include <stdlib.h>

#include "client.h"
#include "defines.h"
#include "gameshell.h"
#include "inputtracking.h"
#include "pixmap.h"
#include "platform.h"

#ifdef __3DS__
#include <3ds.h>
#endif

extern InputTracking _InputTracking;

bool update_touch = false;
int last_touch_x = 0;
int last_touch_y = 0;
int last_touch_button = 0;

static void gameshell_update_touch(Client *c) {
    if (update_touch) {
#ifdef BUILD_A_EMBEDDED
        FILE *trace=fopen("ux0:/data/RS2Vita/input-events.csv","a");
        if(trace){fprintf(trace,"%llu,TOUCH,%d,%d,%d\n",(unsigned long long)ba_timing_now(),last_touch_x,last_touch_y,last_touch_button);fclose(trace);}
#endif
        c->shell->mouse_click_x = last_touch_x;
        c->shell->mouse_click_y = last_touch_y;
        c->shell->mouse_click_button = last_touch_button;
        update_touch = false;
    }
}

GameShell *gameshell_new(void) {
    GameShell *shell = calloc(1, sizeof(GameShell));
    shell->deltime = 20;
    shell->mindel = 1;
    shell->otim = calloc(10, sizeof(uint64_t));
    // shell->sprite_cache = calloc(6, sizeof(Pix24));
    shell->refresh = true;
    shell->action_key = calloc(128, sizeof(int));
    shell->key_queue = calloc(128, sizeof(int));
    shell->key_queue_read_pos = 0;
    shell->key_queue_write_pos = 0;
    return shell;
}

void gameshell_free(GameShell *shell) {
    platform_free();
    if (shell->draw_area) {
        pixmap_free(shell->draw_area);
    }
    free(shell->otim);
    free(shell->action_key);
    free(shell->key_queue);
    free(shell);
}

void gameshell_init_application(Client *c, int width, int height) {
    c->shell->screen_width = width;
    c->shell->screen_height = height;
    platform_new(c->shell);
#if defined(playground) || defined(mapview)
    c->shell->draw_area = pixmap_new(c->shell->screen_width, c->shell->screen_height);
#endif
    gameshell_run(c);
}

#ifdef BUILD_A_EMBEDDED
#include <stdio.h>
#include "ba_pacing.inc" /* per-second draw-time histogram is recorded here */
#endif

void gameshell_run(Client *c) {
    if(h5_mode()=='S'){
        h5_mark("server_only_begin");
        if(ba_server_start("app0:/serverdata","ux0:/data/RS2Vita/players","app0:/sp/sp-bootstrap.js","app0:/sp/server.bundle.npc-unload.js"))return;
        while(!h5_step(ba_server_ready()==1,0,0)){uint64_t start=ba_timing_now();ba_server_pump();ba_timing_record(BA_SERVER_PUMP,ba_timing_now()-start);rs2_sleep(1);}
        for(;;)rs2_sleep(100);
    }
    gameshell_draw_progress(c->shell, "Loading...", 0);
    h5_mark("client_assets_begin");client_load(c);h5_mark("client_assets_ready");
#ifdef BUILD_A_EMBEDDED
    if(ba_run_paced(c))return;
#endif

#ifdef BUILD_A_EMBEDDED
    extern void ba_vita_sample(unsigned,unsigned);
    unsigned measured_frames=0,measured_ticks=0;
#endif
    int opos = 0;
    int ratio = 256;
    int delta = 1;
    int count = 0;
    for (int i = 0; i < 10; i++) {
        c->shell->otim[i] = rs2_now();
    }
    int64_t ntime;
    while (c->shell->state >= 0) {
#ifdef __3DS__
        if (!aptMainLoop()) {
            return;
        }
#endif
        if (c->shell->state > 0) {
            c->shell->state--;
            if (c->shell->state == 0) {
                gameshell_shutdown(c);
                return;
            }
        }
        int lastRatio = ratio;
        int lastDelta = delta;
        ratio = 300;
        delta = 1;
        ntime = rs2_now();
        if (c->shell->otim[opos] == 0L) {
            ratio = lastRatio;
            delta = lastDelta;
        } else if (ntime > c->shell->otim[opos]) {
            ratio = (int)((c->shell->deltime * 2560L) / (ntime - c->shell->otim[opos]));
        }
        if (ratio < 25) {
            ratio = 25;
        }
        if (ratio > 256) {
            ratio = 256;
            delta = (int)((int64_t)c->shell->deltime - (ntime - c->shell->otim[opos]) / 10L);
        }
        c->shell->otim[opos] = ntime;
        opos = (opos + 1) % 10;
        if (delta > 1) {
            for (int i = 0; i < 10; i++) {
                if (c->shell->otim[i] != 0L) {
                    c->shell->otim[i] += delta;
                }
            }
        }
        if (delta < c->shell->mindel) {
            delta = c->shell->mindel;
        }

        rs2_sleep(delta);
#if defined(__vita__) && (!defined(SDL) || SDL == 0)
        // Sample once per rendered frame; held keys still run each logic tick.
        platform_poll_events(c);
#endif
        while (count < 256) {
#if !defined(__vita__) || (defined(SDL) && SDL != 0)
            platform_poll_events(c);
#endif
            uint64_t logic_started=ba_timing_now();
            client_update(c);
            ba_timing_record(BA_LOGIC,ba_timing_now()-logic_started);
#ifdef BUILD_A_EMBEDDED
            measured_ticks++;
#endif
            c->shell->mouse_click_button = 0;
            c->shell->key_queue_read_pos = c->shell->key_queue_write_pos;
            count += ratio;
        }
        count &= 0xff;
        if (c->shell->deltime > 0) {
            c->shell->fps = ratio * 1000 / (c->shell->deltime * 256);
        }
#ifdef BUILD_A_EMBEDDED
        uint64_t pump_started=ba_timing_now();
        if(ba_server_ready()>=0) ba_server_pump();
        ba_timing_record(BA_SERVER_PUMP,ba_timing_now()-pump_started);
#endif
        uint64_t render_started=ba_timing_now();
        client_draw(c);
        ba_timing_record(BA_RENDER,ba_timing_now()-render_started);
        gameshell_update_touch(c); // update mouse after client_draw_scene to fix model picking (not needed for touch on release like client-ts)
        uint64_t present_started=ba_timing_now();
        platform_update_surface();
        ba_timing_record(BA_PRESENT,ba_timing_now()-present_started);
        ba_timing_flush();
#ifdef BUILD_A_EMBEDDED
        ba_vita_sample(++measured_frames,measured_ticks);
#endif
    }
    if (c->shell->state == -1) {
        gameshell_shutdown(c);
    }
}

void gameshell_destroy(Client *c) {
    c->shell->state = -1;
    // rs2_sleep(5000); // NOTE really this long?
    // gameshell_shutdown(c);
}

void gameshell_shutdown(Client *c) {
    c->shell->state = -2;
#ifdef BUILD_A_EMBEDDED
    ba_server_stop();
    h7_live_finish();
#endif
    client_unload(c);
    // rs2_sleep(1000); // NOTE: original
    exit(0);
}

void gameshell_set_framerate(GameShell *shell, int fps) {
    shell->deltime = 1000 / fps;
}

void key_pressed(GameShell *shell, int code, int ch) {
    shell->idle_cycles = 0;

    if (ch < 30) {
        ch = 0;
    }

    if (code == 37) {
        // KEY_LEFT
        ch = 1;
    } else if (code == 39) {
        // KEY_RIGHT
        ch = 2;
    } else if (code == 38) {
        // KEY_UP
        ch = 3;
    } else if (code == 40) {
        // KEY_DOWN
        ch = 4;
    } else if (code == 17) {
        // CONTROL
        ch = 5;
    } else if (code == 16) {
        // SHIFT
        ch = 6; // (custom)
    } else if (code == 18) {
        // ALT
        ch = 7; // (custom)
    } else if (code == 8) {
        // BACKSPACE
        ch = 8;
    } else if (code == 127) {
        // DELETE
        ch = 8;
    } else if (code == 9) {
        ch = 9;
    } else if (code == 10) {
        // ENTER
        ch = 10;
    } else if (code == 13) { // needed for windows?
        // ENTER
        ch = 13;
#ifdef __EMSCRIPTEN__
    } else if (code >= 112 && code <= 123) {
        ch = code + 1008 - 112;
#endif
    } else if (code == 36) {
        ch = 1000;
    } else if (code == 35) {
        ch = 1001;
    } else if (code == 33) {
        ch = 1002;
    } else if (code == 34) {
        ch = 1003;
    }

    if (ch > 0 && ch < 128) {
        shell->action_key[ch] = 1;
    }

    if (ch > 4) {
        shell->key_queue[shell->key_queue_write_pos] = ch;
        shell->key_queue_write_pos = shell->key_queue_write_pos + 1 & 0x7f;
    }

    if (_InputTracking.enabled) {
        inputtracking_key_pressed(&_InputTracking, ch);
    }
}

void key_released(GameShell *shell, int code, int ch) {
    shell->idle_cycles = 0;

    if (ch < 30) {
        ch = 0;
    }

    if (code == 37) {
        // KEY_LEFT
        ch = 1;
    } else if (code == 39) {
        // KEY_RIGHT
        ch = 2;
    } else if (code == 38) {
        // KEY_UP
        ch = 3;
    } else if (code == 40) {
        // KEY_DOWN
        ch = 4;
    } else if (code == 17) {
        // CONTROL
        ch = 5;
    } else if (code == 16) {
        // SHIFT
        ch = 6; // (custom)
    } else if (code == 18) {
        // ALT
        ch = 7; // (custom)
    } else if (code == 8) {
        // BACKSPACE
        ch = 8;
    } else if (code == 127) {
        // DELETE
        ch = 8;
    } else if (code == 9) {
        ch = 9;
    } else if (code == 10) {
        // ENTER
        ch = 10;
    } else if (code == 13) { // needed for windows?
        // ENTER
        ch = 13;
#ifdef __EMSCRIPTEN__
    } else if (code >= 112 && code <= 123) {
        ch = code + 1008 - 112;
#endif
    } else if (code == 36) {
        ch = 1000;
    } else if (code == 35) {
        ch = 1001;
    } else if (code == 33) {
        ch = 1002;
    } else if (code == 34) {
        ch = 1003;
    }

    if (ch > 0 && ch < 128) {
        shell->action_key[ch] = 0;
    }

    if (_InputTracking.enabled) {
        inputtracking_key_released(&_InputTracking, ch);
    }
}

int poll_key(GameShell *shell) {
    int key = -1;
    if (shell->key_queue_write_pos != shell->key_queue_read_pos) {
        key = shell->key_queue[shell->key_queue_read_pos];
        shell->key_queue_read_pos = shell->key_queue_read_pos + 1 & 0x7f;
    }
    return key;
}

void gameshell_draw_progress(GameShell *shell, const char *message, int progress) {
    h5_mark(message);if(h5_silent())return;
    // NOTE there's no update or paint to call refresh, only focus gained event
    if (shell->refresh) {
        platform_set_color(BLACK);
        platform_fill_rect(0, 0, shell->screen_width, shell->screen_height);
        shell->refresh = false;
    }

    int y = shell->screen_height / 2 - 18;

    // rgb 140, 17, 17 but we only take hex for simplicity
    platform_set_color(PROGRESS_RED);
    platform_draw_rect(shell->screen_width / 2 - 152, y, 304, 34);
    platform_fill_rect(shell->screen_width / 2 - 150, y + 2, progress * 3, 30);
    platform_set_color(BLACK);
    platform_fill_rect(shell->screen_width / 2 + progress * 3 - 150, y + 2, 300 - progress * 3, 30);

    platform_set_font("Helvetica", true, 13);
    platform_set_color(WHITE);
    platform_draw_string(message, (shell->screen_width - platform_string_width(message)) / 2, y + 22);

    platform_update_surface();
}
