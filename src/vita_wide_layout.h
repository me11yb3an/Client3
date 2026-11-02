#pragma once
// Classic panels retain their artwork and component coordinates.
#if defined(VITA_NATIVE_WIDESCREEN)
#if defined(GL11)
/* GPU feasibility probe only: private copied tree; software source guard remains intact. */
#endif
#define GAME_VIEW_WIDTH 683
#define GAME_VIEW_HEIGHT 346
#define UI_RIGHT(x) ((x) + 171)
#define UI_BOTTOM(y) ((y) + 12)
#define GAME_MENU_WIDTH 683
#define GAME_MENU_HEIGHT 346
#define SIDE_TABS_HEIGHT 78
#define CHAT_WIDTH 650
#define CHAT_CONTROLS_WIDTH 672
static inline int wide_chat_button_x(int x, int index) {
    const int old_centers[4] = {57, 186, 326, 462};
    return 84 + 168 * index + x - old_centers[index];
}
#define CHAT_BUTTON_X(x, index) wide_chat_button_x(x, index)
#define GAME_MODAL_X ((GAME_VIEW_WIDTH - 512) / 2)
#else
#define GAME_VIEW_WIDTH 512
#define GAME_VIEW_HEIGHT 334
#define UI_RIGHT(x) (x)
#define UI_BOTTOM(y) (y)
#define GAME_MENU_WIDTH 512
#define GAME_MENU_HEIGHT 334
#define SIDE_TABS_HEIGHT 66
#define CHAT_WIDTH 479
#define CHAT_CONTROLS_WIDTH 501
#define CHAT_BUTTON_X(x, index) (x)
#define GAME_MODAL_X 0
#endif
// Original guide assembly moves as one unit, including input and scrollbars.
static inline int guide_modal_offset(int id,int enhanced){return enhanced&&id==3559?-54:id==7386||id==7558?(enhanced?-44:-24):0;}
static inline int guide_modal_y(int id,int enhanced){return enhanced&&(id==7386||id==7558)?18:0;}
#define CHAT_TEXT_RIGHT (CHAT_WIDTH - 16)
#define CHAT_TEXT_WIDTH (CHAT_TEXT_RIGHT - 4)
static inline int game_world_input(int x, int y) {
    if (x <= 8 || y <= 11 || x >= 8 + GAME_VIEW_WIDTH || y >= 11 + GAME_VIEW_HEIGHT) return 0;
#ifdef VITA_NATIVE_WIDESCREEN
    // Entire classic frame regions consume input, including noninteractive backing.
    if (x >= UI_RIGHT(520)) return 0;
    if (y >= UI_BOTTOM(345) && y < UI_BOTTOM(375) && x < 562) return 0;
    if (y >= UI_BOTTOM(375) && (x < 501 || x >= UI_RIGHT(501))) return 0;
#endif
    return 1;
}

#ifdef VITA_NATIVE_WIDESCREEN
#define TITLE_X(x) ((x) + (960 - 789) / 2)
#define TITLE_Y(y) ((y) + (544 - 532) / 2)
#else
#define TITLE_X(x) (x)
#define TITLE_Y(y) (y)
#endif
// Login text baselines are 276 and 291 in the original title composition.
static inline int title_login_field_at(int x, int y) {
    if (x < TITLE_X(301) || x > TITLE_X(562)) return -1;
    if (y >= TITLE_Y(261) && y < TITLE_Y(276)) return 0;
    if (y >= TITLE_Y(276) && y < TITLE_Y(291)) return 1;
    return -1;
}

#ifdef VITA_NATIVE_WIDESCREEN
#define CHAT_BUTTON_LEFT(x, index) (168 * (index) + 8)
#define CHAT_BUTTON_RIGHT(x, index) (168 * (index) + 160)
#else
#define CHAT_BUTTON_LEFT(x, index) (x)
#define CHAT_BUTTON_RIGHT(x, index) (x)
#endif

static inline int title_button_contains(int x, int y, int center_x, int center_y) {
#ifdef VITA_NATIVE_WIDESCREEN
    return x >= center_x - 73 && x < center_x + 73 &&
           y >= center_y - 20 && y < center_y + 20;
#else
    return x >= center_x - 75 && x <= center_x + 75 &&
           y >= center_y - 20 && y <= center_y + 20;
#endif
}

// Title composition: preserve full original halves and expand their plain center join.
// Login panel and button coordinates remain unchanged.
#ifdef VITA_NATIVE_WIDESCREEN
#define TITLE_LEFT_MID_WIDTH 171
#define TITLE_RIGHT_MID_WIDTH 173
#define TITLE_LEFT_MID_X 128
#define TITLE_TOP_WIDTH 960
#define TITLE_TOP_HEIGHT 192
#define TITLE_TOP_X 0
#define TITLE_TOP_Y 0
#define TITLE_BOTTOM_WIDTH 960
#define TITLE_BOTTOM_HEIGHT 152
#define TITLE_BOTTOM_X 0
#define TITLE_BOTTOM_Y 392
#define TITLE_LEFT_BASE_WIDTH 299
#define TITLE_RIGHT_BASE_WIDTH 301
#define TITLE_LEFT_BASE_X 0
#define TITLE_FLAME_LEFT_X 0
#define TITLE_FLAME_RIGHT_X 832
#else
#define TITLE_LEFT_MID_WIDTH 86
#define TITLE_RIGHT_MID_WIDTH 87
#define TITLE_LEFT_MID_X 128
#define TITLE_TOP_WIDTH 533
#define TITLE_TOP_HEIGHT 186
#define TITLE_TOP_X 128
#define TITLE_TOP_Y 0
#define TITLE_BOTTOM_WIDTH 360
#define TITLE_BOTTOM_HEIGHT 146
#define TITLE_BOTTOM_X 214
#define TITLE_BOTTOM_Y 386
#define TITLE_LEFT_BASE_WIDTH 214
#define TITLE_RIGHT_BASE_WIDTH 215
#define TITLE_LEFT_BASE_X 0
#define TITLE_FLAME_LEFT_X 0
#define TITLE_FLAME_RIGHT_X 661
#endif
