#include "display.h"

#define LGFX_AUTODETECT
#include <LovyanGFX.hpp>

#ifdef C4_DESKTOP
    static LGFX display(480, 320, 2, 2);
#else
    static LGFX display;
#endif

static LGFX_Sprite canvas(&display);
static LGFX_Sprite face(&display);

#define COLOUR_BG 0x0862
#define COLOUR_GRID 0x1988
#define COLOUR_CYAN 0x5E3B
#define COLOUR_CYAN_LIGHT 0x8EDD
#define COLOUR_CYAN_DIM 0x1A30
#define COLOUR_CYAN_FAINT 0x0929
#define COLOUR_TITLE 0xEF3A
#define COLOUR_BTN_FILL 0x08C4
#define COLOUR_BTN_FILL_HI 0x1125

#define BUTTON_WIDTH 160
#define BUTTON_HEIGHT 52
#define BUTTON_CORNER_RADIUS 4
#define BUTTON_GAP 16
#define BUTTON_PLAY_X (240 - BUTTON_WIDTH - (BUTTON_GAP / 2))
#define BUTTON_SETTINGS_X (240 + (BUTTON_GAP / 2))
#define MAIN_MENU_BUTTON_Y 210


static ScreenState screenState = SCREEN_MAIN_MENU;
static ScreenState previousScreenState = SCREEN_NULL;


enum FaceBlinkState {
    BLINK_IDLE,
    BLINK_CLOSING,
    BLINK_CLOSED,
    BLINK_OPENING
};


static void drawText(const char* text, int centerX, int y, int spacing) {
    int len = strlen(text);
    if (len == 0) return;

    int charWidth = canvas.textWidth("A");
    int w = charWidth + spacing;
    int totalWidth = len * w - spacing;
    int x = centerX - (totalWidth / 2);

    char c[2] = {0, 0};
    for (int i = 0; i < len; ++i) {
        c[0] = text[i];
        canvas.drawString(c, x, y);
        x += w;
    }
}


static void drawBackground() {
    canvas.fillScreen(COLOUR_BG);
}


static void drawUIButton(const char* text, int x, int y) {
    canvas.fillRoundRect(x, y, BUTTON_WIDTH, BUTTON_HEIGHT, BUTTON_CORNER_RADIUS, COLOUR_BTN_FILL_HI);
    canvas.drawRoundRect(x, y, BUTTON_WIDTH, BUTTON_HEIGHT, BUTTON_CORNER_RADIUS, COLOUR_CYAN);

    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextColor(COLOUR_CYAN_LIGHT);
    canvas.setTextDatum(textdatum_t::middle_centre);
    canvas.drawString(text, x + (BUTTON_WIDTH / 2), y + (BUTTON_HEIGHT / 2));
}


static void drawMainMenuScreen() {
    drawBackground();
    drawUIButton("PLAY", BUTTON_PLAY_X, MAIN_MENU_BUTTON_Y);
    drawUIButton("SETTINGS", BUTTON_SETTINGS_X, MAIN_MENU_BUTTON_Y);
    canvas.pushSprite(0, 0);
}


void initUI() {
    display.init();
    display.setRotation(0);
    display.setBrightness(128);
    display.setColorDepth(16);

    canvas.setColorDepth(16);
    #ifndef C4_DESKTOP
        canvas.setPsram(true);
    #endif
    canvas.createSprite(480, 320);

    face.setColorDepth(16);
    #ifndef C4_DESKTOP
        face.setPsram(true);
    #endif
    face.createSprite(480, 320);
}


void drawUI() {
    if (screenState == previousScreenState) return;

    switch (screenState) {
        case SCREEN_MAIN_MENU:
            drawMainMenuScreen();
            break;
        case SCREEN_SETTINGS_MENU:
            break;
        case SCREEN_NULL:
            break;
    }

    previousScreenState = screenState;
}


void setScreen(ScreenState state) {
    screenState = state;
}
