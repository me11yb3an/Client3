#pragma once

#include "client.h"
#include <stdbool.h>

typedef struct {
    bool show_performance;
    bool show_debug;
    bool camera_editor;

    bool remember_username;
    bool remember_password;
    bool hide_dns;
    bool hide_debug_sprite;
    bool allow_commands;
    bool allow_debugprocs;
    int http_port;
    int chat_era; // 0 - early beta, 1 - late beta, 2 - launch
    bool resizable;
    bool item_outlines;
    bool use_opengl11;
} Custom;

bool load_ini_args(void);
void load_ini_config(Client *c);
void update_camera_editor(Client *c);
void draw_info_overlay(Client *c);

// touch
bool insideChatInputArea(Client *c);
bool insideChatPopupArea(Client *c);
bool insideReportInterfaceTextArea(Client *c);
bool insideUsernameArea(Client *c);
bool inPasswordArea(Client *c);
bool insideMobileInputArea(Client *c);
bool insideTabArea(Client *c);
bool insideViewportArea(Client *c);


// Performance text is drawn into the viewport surface, not full-screen space.
// Translate the existing block uniformly, preserving every line's spacing.
#if defined(__vita__) && defined(VITA_NATIVE_WIDESCREEN)
#include "vita_wide_layout.h"
#define PERF_VIEW_MARGIN 8
#define PERF_RIGHT(original) (GAME_VIEW_WIDTH - PERF_VIEW_MARGIN)
#define PERF_BASELINE(original) ((original) - (13 * 4 - (PERF_VIEW_MARGIN + 12)))
#else
#define PERF_RIGHT(original) (original)
#define PERF_BASELINE(original) (original)
#endif

void draw_info_overlay_local(Client*c,int x,int y);

/* Display-only Classic diagnostics, sampled at four updates per second. */
const char *classic_perf_display(int slot,const char *text);
