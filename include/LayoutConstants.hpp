#pragma once
#include <switch.h>
#include <ctime>

static constexpr s32 SCREEN_W   = 1920;
static constexpr s32 SCREEN_H   = 1080;

static constexpr s32 SIDEBAR_W  = 250;
static constexpr s32 CONTENT_X  = SIDEBAR_W;
static constexpr s32 CONTENT_W  = SCREEN_W - SIDEBAR_W;
static constexpr s32 CONTENT_CX = CONTENT_X + CONTENT_W / 2; // 1085

// RIGHT_X defined early so PLAYER_CX can reference it
static constexpr s32 RIGHT_X    = 1100;

// Player block centered between sidebar and right panel left edge.
// Vertical centering: block height = ART(520) + gap(28) + track(58) + artist+gap(90) + btn(77) = 773px
// → ART_Y = (1080 - 773) / 2 = 154
static constexpr s32 ART_SIZE   = 520;
static constexpr s32 PLAYER_CX  = (SIDEBAR_W + RIGHT_X) / 2; // 675
static constexpr s32 ART_X      = PLAYER_CX - ART_SIZE / 2;  // 415
static constexpr s32 ART_Y      = 154;

static constexpr s32 TRACK_Y    = ART_Y + ART_SIZE + 28;     // 702
static constexpr s32 ARTIST_Y   = TRACK_Y + 58;              // 760

static constexpr s32 CTRL_Y     = ARTIST_Y + 90;             // 850
static constexpr s32 CTRL_SMALL = 70;
static constexpr s32 CTRL_LARGE = 84;
static constexpr s32 CTRL_GAP   = 110;

static constexpr s32 PREV_CX    = PLAYER_CX - CTRL_GAP;      // 565
static constexpr s32 PLAY_CX    = PLAYER_CX;                 // 675
static constexpr s32 NEXT_CX    = PLAYER_CX + CTRL_GAP;      // 785

static constexpr s32 CTRL_BTN_CY   = CTRL_Y + CTRL_SMALL / 2; // shared center Y = 885
static constexpr s32 CTRL_ICON_SM  = 38;                       // icon size in prev/next circles
static constexpr s32 CTRL_ICON_LG  = 46;                       // icon size in play/pause circle

static constexpr s32 TAB_H      = 64;
static constexpr s32 TAB1_Y     = 280;
static constexpr s32 TAB2_Y     = TAB1_Y + TAB_H + 4;        // 348
static constexpr s32 TAB3_Y     = TAB2_Y + TAB_H + 4;        // 416

// Right panel — same margin on the right as the player content has from the sidebar (165 px)
static constexpr s32 RIGHT_MARGIN = ART_X - SIDEBAR_W;                  // 165
static constexpr s32 RIGHT_W      = SCREEN_W - RIGHT_X - RIGHT_MARGIN;  // 655
static constexpr s32 RIGHT_TAB_H  = 52;
static constexpr s32 RIGHT_TAB_W  = RIGHT_W / 2;                        // 327

// Right-panel horizontal geometry
static constexpr s32 RART_IMG_SIZE  = 200;
static constexpr s32 RART_IMG_X     = RIGHT_X + 28;                                // 1128
static constexpr s32 RART_INFO_X    = RART_IMG_X + RART_IMG_SIZE + 20;             // 1348
static constexpr s32 RART_INFO_W    = SCREEN_W - RIGHT_MARGIN - RART_INFO_X - 10;  // ~387
static constexpr s32 RALBUM_IMG_SIZE = 100;
static constexpr s32 RALBUM_TEXT_X   = RART_IMG_X + RALBUM_IMG_SIZE + 14;          // 1242
static constexpr s32 RALBUM_FULL_W   = SCREEN_W - RIGHT_MARGIN - RART_IMG_X - 10;  // ~617
static constexpr s32 RALBUM_TEXT_W   = RALBUM_FULL_W - RALBUM_IMG_SIZE - 14;       // ~503

// Flex-column space-around vertical layout
// Content area Y=208, H=726. Blocks: artist 200px + album 120px = 320px.
// space-around (2 items): outer=g, inner=2g → 4g = 406 → g=101
static constexpr s32 RCONT_Y         = ART_Y + RIGHT_TAB_H + 2;                    // 208
static constexpr s32 RCONT_H         = CTRL_Y + CTRL_LARGE - RCONT_Y;              // 726
static constexpr s32 RFLEX_G         = (RCONT_H - RART_IMG_SIZE - 120) / 4;        // 101
static constexpr s32 RART_BLOCK_Y    = RCONT_Y + RFLEX_G;                          // 309
static constexpr s32 RALBUM_BLOCK_Y  = RART_BLOCK_Y + RART_IMG_SIZE + 2 * RFLEX_G; // 711

// Artist info text Y (generous spacing alongside the 200px image)
static constexpr s32 RART_NAME_Y     = RART_BLOCK_Y;                               // 309
static constexpr s32 RART_GENRES_Y   = RART_NAME_Y   + 52;                         // 361
static constexpr s32 RART_FOLLOW_Y   = RART_GENRES_Y + 42;                         // 403
static constexpr s32 RART_POP_Y      = RART_FOLLOW_Y + 36;                         // 439

// Album info text Y
static constexpr s32 RALBUM_SEP_Y    = RART_BLOCK_Y + RART_IMG_SIZE + RFLEX_G;    // 610
static constexpr s32 RALBUM_IMG_Y    = RALBUM_BLOCK_Y + 24;                        // 735  (image starts below label)
static constexpr s32 RALBUM_HDR_Y    = RALBUM_BLOCK_Y - 8;                         // 703  (label a few px above image gap)
static constexpr s32 RALBUM_NAME_Y   = RALBUM_IMG_Y;                               // 735  (title aligned with image top)
static constexpr s32 RALBUM_INFO1_Y  = RALBUM_NAME_Y + 36;                         // 771
static constexpr s32 RALBUM_INFO2_Y  = RALBUM_INFO1_Y + 30;                        // 801

static constexpr time_t REFRESH_INTERVAL_SECS = 5;

static constexpr s32 SPINNER_SIZE = 128;
static constexpr s32 SPINNER_X    = PLAYER_CX - SPINNER_SIZE / 2;
static constexpr s32 SPINNER_Y    = ART_Y + ART_SIZE / 2 - SPINNER_SIZE / 2;

// --- User tab layout ---

static constexpr s32 UAVATAR_SIZE   = 200;
static constexpr s32 UBLOCK_Y       = (SCREEN_H - 250) / 2;        // 415
static constexpr s32 UAVATAR_X      = CONTENT_X + 120;             // 370
static constexpr s32 UAVATAR_Y      = UBLOCK_Y;
static constexpr s32 UINFO_X        = UAVATAR_X + UAVATAR_SIZE + 60; // 630
static constexpr s32 UNAME_Y        = UBLOCK_Y;
static constexpr s32 UEMAIL_Y       = UNAME_Y + 68;
static constexpr s32 UPLAN_Y        = UEMAIL_Y + 46;
static constexpr s32 UFOLLOWERS_Y   = UPLAN_Y + 46;

// Queue tab card layout
static constexpr s32 RQUEUE_ITEM_H   = 110;
static constexpr s32 RQUEUE_GAP      = 14;
static constexpr s32 RQUEUE_TOTAL_H  = 5 * RQUEUE_ITEM_H + 4 * RQUEUE_GAP;  // 606
static constexpr s32 RQUEUE_START_Y  = RCONT_Y + (RCONT_H - RQUEUE_TOTAL_H) / 2;  // 268
static constexpr s32 RQUEUE_CARD_X   = RIGHT_X + 10;   // 1110
static constexpr s32 RQUEUE_CARD_W   = RIGHT_W - 20;   // 635
static constexpr s32 RQUEUE_IMG_SIZE = 80;

// Audio bars animation (inside first queue card, right-aligned)
static constexpr s32 BAR_W          = 8;
static constexpr s32 BAR_GAP        = 6;
static constexpr s32 BAR_MAX_H      = 46;
static constexpr s32 BAR_MIN_H      = 10;
static constexpr s32 BARS_TOTAL_W   = 3 * BAR_W + 2 * BAR_GAP;   // 36
static constexpr s32 BARS_RIGHT_PAD = 28;
static constexpr s32 BARS_X0        = RQUEUE_CARD_X + RQUEUE_CARD_W - BARS_RIGHT_PAD - BARS_TOTAL_W;
static constexpr s32 CARD0_CY       = RQUEUE_START_Y + (RQUEUE_ITEM_H - 2) / 2;
static constexpr s32 RQUEUE_IMG_X    = RQUEUE_CARD_X + 12;                          // 1122
static constexpr s32 RQUEUE_TEXT_X   = RQUEUE_IMG_X + RQUEUE_IMG_SIZE + 14;         // 1216
static constexpr s32 RQUEUE_TEXT_W   = RQUEUE_CARD_X + RQUEUE_CARD_W - RQUEUE_TEXT_X - 10; // ~479
