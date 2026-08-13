#include "display.h"
#include "face.h"
#include "platform.h"

#define LGFX_AUTODETECT
#include <LovyanGFX.hpp>

#ifdef C4_DESKTOP
    static LGFX display(480, 320, 2, 2);
#else
    static LGFX display;
#endif

static LGFX_Sprite canvas(&display);
static LGFX_Sprite faceSprite(&display);

#define DEG2RAD(d) (static_cast<float>(M_PI / 180.) * (d))

#define COLOUR_BG 0x0862
#define COLOUR_GRID 0x1988
#define COLOUR_CYAN 0x5E3B
#define COLOUR_CYAN_LIGHT 0x8EDD
#define COLOUR_CYAN_DIM 0x1A30
#define COLOUR_CYAN_FAINT 0x0929
#define COLOUR_TITLE 0xEF3A
#define COLOUR_BTN_FILL 0x08C4
#define COLOUR_BTN_FILL_HI 0x1125

#define FACE_SPRITE_W 160
#define FACE_SPRITE_H 150

#define BUTTON_WIDTH 160
#define BUTTON_HEIGHT 52
#define BUTTON_CORNER_RADIUS 4
#define BUTTON_GAP 16
#define BUTTON_PLAY_X (240 - BUTTON_WIDTH - (BUTTON_GAP / 2))
#define BUTTON_SETTINGS_X (240 + (BUTTON_GAP / 2))
#define MAIN_MENU_BUTTON_Y 210


static ScreenState screenState = SCREEN_MAIN_MENU;
static ScreenState previousScreenState = SCREEN_NULL;


#define FACE_REFRESH_RATE_MS 50
static uint32_t faceRefreshTimer = 0;

static float faceRotation = 0.f;
static float faceScaleX = 1.f;
static float faceScaleY = 1.f;
static float faceX = 0.f;
static float faceY = 0.f;
static bool faceVisible = false;


static uint16_t colourToRgb565(const Colour& c) {
    return ((c.r >> 3) << 11) | ((c.g >> 2) << 5) | (c.b >> 3);
}


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


static void drawRotatedRect(LovyanGFX& gfx, float cx, float cy, float w, float h, float angleDeg, uint16_t col) {
    float rad = DEG2RAD(angleDeg);
    float cosA = cosf(rad);
    float sinA = sinf(rad);
    float hw = 0.5f * w;
    float hh = 0.5f * h;

    float corners[4][2] = {
        {-hw, -hh},
        {hw, -hh},
        {hw, hh},
        {-hw, hh}
    };

    int px[4];
    int py[4];
    for (int i = 0; i < 4; i++) {
        px[i] = static_cast<int>(cx + corners[i][0] * cosA - corners[i][1] * sinA);
        py[i] = static_cast<int>(cy + corners[i][0] * sinA + corners[i][1] * cosA);
    }

    gfx.fillTriangle(px[0], py[0], px[1], py[1], px[2], py[2], col);
    gfx.fillTriangle(px[0], py[0], px[2], py[2], px[3], py[3], col);
}


static void drawThickBezier(LovyanGFX& gfx, float x0, float y0, float cx, float cy, float x1, float y1, float thickness, uint16_t col) {
    int half = static_cast<int>(0.5f * thickness);
    int x0i = static_cast<int>(x0);
    int y0i = static_cast<int>(y0);
    int cxi = static_cast<int>(cx);
    int cyi = static_cast<int>(cy);
    int x1i = static_cast<int>(x1);
    int y1i = static_cast<int>(y1);

    for (int i = -half; i <= half; ++i) {
        gfx.drawBezier(x0i, y0i + i, cxi, cyi + i, x1i, y1i + i, col);
    }
}


static void drawEye(LovyanGFX& gfx, const EyeParams& eye, float ox, float oy, uint16_t col) {
    float ex = eye.x + ox;
    float ey = eye.y + oy;

    switch (eye.shape) {
        case EYE_RECT:
            if (eye.angle == 0.f) {
                gfx.fillRect(static_cast<int>(ex - 0.5f * eye.width), static_cast<int>(ey - 0.5f * eye.height), static_cast<int>(eye.width),
                             static_cast<int>(eye.height), col);
            } else {
                drawRotatedRect(gfx, ex, ey, eye.width, eye.height, eye.angle, col);
            }
            break;
        case EYE_ARC:
            drawThickBezier(gfx, ex - 0.5f * eye.width, ey, ex, ey - eye.height, ex + 0.5f * eye.width, ey, eye.strokeWidth, col);
            break;
    }
}


static void drawEyebrow(LovyanGFX& gfx, const EyebrowParams& eyebrow, float ox, float oy, uint16_t col) {
    if (eyebrow.strokeWidth < 1.f) return;

    gfx.drawWideLine(eyebrow.x1 + ox, eyebrow.y1 + oy, eyebrow.x2 + ox, eyebrow.y2 + oy, eyebrow.strokeWidth, col);
}


static void drawMouth(LovyanGFX& gfx, const MouthParams& mouth, float ox, float oy, uint16_t col) {
    float mx = mouth.x + ox;
    float my = mouth.y + oy;
    float hw = 0.5f * mouth.width;

    switch (mouth.shape) {
        case MOUTH_BAR:
            gfx.fillRect(static_cast<int>(mx - hw), static_cast<int>(my - 0.5f * mouth.height), static_cast<int>(mouth.width),
                         static_cast<int>(mouth.height), col);
            break;
        case MOUTH_CURVE:
            drawThickBezier(gfx, mx - hw, my, mx, my + mouth.curve, mx + hw, my, mouth.strokeWidth, col);
            break;
        case MOUTH_D_OUTLINE: {
            int half = static_cast<int>(0.5f * mouth.strokeWidth);
            int x = static_cast<int>(mx - hw);
            int y = static_cast<int>(my);
            int w = static_cast<int>(mouth.width);
            for (int i = -half; i <= half; i++) {
                gfx.drawFastHLine(x, y + i, w , col);
            }

            drawThickBezier(gfx, mx - hw, my, mx, my + mouth.height, mx + hw, my, mouth.strokeWidth, col);
            break;
        }
    }
}


static void drawFace() {
    uint32_t now = getNow();
    if (now < faceRefreshTimer) return;
    faceRefreshTimer = now + FACE_REFRESH_RATE_MS;

    Face& face = Face::getFace();
    face.update(now);

    FaceOffset offset = face.getOffset();
    uint16_t col = colourToRgb565(face.getColour());

    faceSprite.fillSprite(COLOUR_BG);

    drawEyebrow(faceSprite, face.getLeftBrowParams(), offset.x, offset.y, col);
    drawEyebrow(faceSprite, face.getRightBrowParams(), offset.x, offset.y, col);
    drawEye(faceSprite, face.getLeftEyeParams(), offset.x, offset.y, col);
    drawEye(faceSprite, face.getRightEyeParams(), offset.x, offset.y, col);
    drawMouth(faceSprite, face.getMouthParams(), offset.x, offset.y, col);

    float halfW = (0.5f * FACE_SPRITE_W) * faceScaleX;
    float halfH = (0.5f * FACE_SPRITE_H) * faceScaleY;
    display.fillRect(static_cast<int>(faceX - halfW), static_cast<int>(faceY - halfH), static_cast<int>(2.f * halfW),
                     static_cast<int>(2.f * halfH), COLOUR_BG);

    faceSprite.pushRotateZoom(&display, faceX, faceY, faceRotation, faceScaleX, faceScaleY, COLOUR_BG);
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
    Face::getFace().setState(FACE_NEUTRAL);

    faceX = 240.f;
    faceY = 100.f;
    faceScaleX = 1.f;
    faceScaleY = 1.f;
    faceRotation = 0.f;
    faceVisible = true;
    
    drawBackground();
    drawUIButton("PLAY", BUTTON_PLAY_X, MAIN_MENU_BUTTON_Y);
    drawUIButton("SETTINGS", BUTTON_SETTINGS_X, MAIN_MENU_BUTTON_Y);
    canvas.pushSprite(0, 0);
}


void initDisplay() {
    display.init();
    display.setRotation(0);
    display.setBrightness(128);
    display.setColorDepth(16);

    canvas.setColorDepth(16);
    #ifndef C4_DESKTOP
        canvas.setPsram(true);
    #endif
    canvas.createSprite(480, 320);

    faceSprite.setColorDepth(16);
    #ifndef C4_DESKTOP
        faceSprite.setPsram(true);
    #endif
    faceSprite.createSprite(FACE_SPRITE_W, FACE_SPRITE_H);
}


void drawDisplay() {
    if (faceVisible) drawFace();
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
