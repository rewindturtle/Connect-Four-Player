#include "display.h"
#include "face.h"
#include "platform.h"

#include <math.h>
#include <string.h>

#define LGFX_AUTODETECT
#include <LovyanGFX.hpp>

#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 320

#ifdef C4_DESKTOP
    static LGFX display(SCREEN_WIDTH, SCREEN_HEIGHT, 2, 2);
#else
    static LGFX display;
#endif

static LGFX_Sprite canvas(&display);
static LGFX_Sprite faceSprite(&display);
static LGFX_Sprite faceFrame(&display);

static constexpr float DEG_TO_RAD = 0.01745329251f;

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
// Sized to the face sprite's diagonal so a rotated face still fits inside.
#define FACE_FRAME_SIZE 220
#define FACE_REFRESH_RATE_MS 50

#define BUTTON_WIDTH 160
#define BUTTON_HEIGHT 52
#define BUTTON_CORNER_RADIUS 4
#define BUTTON_GAP 16
#define BUTTON_PLAY_X (240 - BUTTON_WIDTH - (BUTTON_GAP / 2))
#define BUTTON_SETTINGS_X (240 + (BUTTON_GAP / 2))
#define MAIN_MENU_BUTTON_Y 210


struct FaceTransform {
    float x = 0.f;
    float y = 0.f;
    float rotation = 0.f;
    float scaleX = 1.f;
    float scaleY = 1.f;
    bool visible = false;
};


static ScreenState screenState = SCREEN_MAIN_MENU;
static ScreenState previousScreenState = SCREEN_NULL;

static FaceTransform faceTransform;
static uint32_t faceRefreshTimer = 0;
static bool faceNeedsRedraw = false;


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


static void drawRotatedRect(LovyanGFX& gfx, float cx, float cy, float w, float h, float angleDeg, uint16_t colour) {
    float rad = angleDeg * DEG_TO_RAD;
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

    gfx.fillTriangle(px[0], py[0], px[1], py[1], px[2], py[2], colour);
    gfx.fillTriangle(px[0], py[0], px[2], py[2], px[3], py[3], colour);
}


static void drawThickBezier(LovyanGFX& gfx, float x0, float y0, float cx, float cy, float x1, float y1, float thickness, uint16_t colour) {
    int half = static_cast<int>(0.5f * thickness);
    int x0i = static_cast<int>(x0);
    int y0i = static_cast<int>(y0);
    int cxi = static_cast<int>(cx);
    int cyi = static_cast<int>(cy);
    int x1i = static_cast<int>(x1);
    int y1i = static_cast<int>(y1);

    for (int i = -half; i <= half; ++i) {
        gfx.drawBezier(x0i, y0i + i, cxi, cyi + i, x1i, y1i + i, colour);
    }
}


static void drawEye(LovyanGFX& gfx, const EyeParams& eye, const FaceOffset& offset, uint16_t colour) {
    float ex = eye.x + offset.x;
    float ey = eye.y + offset.y;

    switch (eye.shape) {
        case EYE_RECT:
            if (eye.angle == 0.f) {
                gfx.fillRect(static_cast<int>(ex - 0.5f * eye.width), static_cast<int>(ey - 0.5f * eye.height), static_cast<int>(eye.width),
                             static_cast<int>(eye.height), colour);
            } else {
                drawRotatedRect(gfx, ex, ey, eye.width, eye.height, eye.angle, colour);
            }
            break;
        case EYE_ARC:
            drawThickBezier(gfx, ex - 0.5f * eye.width, ey, ex, ey - eye.height, ex + 0.5f * eye.width, ey, eye.strokeWidth, colour);
            break;
    }
}


static void drawEyebrow(LovyanGFX& gfx, const EyebrowParams& eyebrow, const FaceOffset& offset, uint16_t colour) {
    if (eyebrow.strokeWidth < 1.f) return;

    float x1 = eyebrow.x1 + offset.x;
    float y1 = eyebrow.y1 + offset.y;
    float x2 = eyebrow.x2 + offset.x;
    float y2 = eyebrow.y2 + offset.y;
    float dx = x2 - x1;
    float dy = y2 - y1;
    float length = sqrtf(dx * dx + dy * dy);
    if (length < 1.f) return;

    drawRotatedRect(gfx, 0.5f * (x1 + x2), 0.5f * (y1 + y2), length, eyebrow.strokeWidth, atan2f(dy, dx) / DEG_TO_RAD, colour);
}


static void drawMouth(LovyanGFX& gfx, const MouthParams& mouth, const FaceOffset& offset, uint16_t colour) {
    float mx = mouth.x + offset.x;
    float my = mouth.y + offset.y;
    float hw = 0.5f * mouth.width;

    switch (mouth.shape) {
        case MOUTH_BAR:
            gfx.fillRect(static_cast<int>(mx - hw), static_cast<int>(my - 0.5f * mouth.height), static_cast<int>(mouth.width),
                         static_cast<int>(mouth.height), colour);
            break;
        case MOUTH_CURVE:
            drawThickBezier(gfx, mx - hw, my, mx, my + mouth.curve, mx + hw, my, mouth.strokeWidth, colour);
            break;
        case MOUTH_D_OUTLINE: {
            int half = static_cast<int>(0.5f * mouth.strokeWidth);
            gfx.fillRect(static_cast<int>(mx - hw), static_cast<int>(my) - half, static_cast<int>(mouth.width), 2 * half + 1, colour);
            drawThickBezier(gfx, mx - hw, my, mx, my + mouth.height, mx + hw, my, mouth.strokeWidth, colour);
            break;
        }
    }
}


static void drawFace(uint32_t now) {
    Face& face = Face::getFace();
    face.update(now);

    if (!face.requiresDraw() && !faceNeedsRedraw) return;
    faceNeedsRedraw = false;

    const FaceOffset offset = face.getOffset();
    const uint16_t colour = colourToRgb565(face.getColour());

    faceSprite.fillSprite(COLOUR_BG);
    drawEyebrow(faceSprite, face.getLeftBrowParams(), offset, colour);
    drawEyebrow(faceSprite, face.getRightBrowParams(), offset, colour);
    drawEye(faceSprite, face.getLeftEyeParams(), offset, colour);
    drawEye(faceSprite, face.getRightEyeParams(), offset, colour);
    drawMouth(faceSprite, face.getMouthParams(), offset, colour);

    // The SDL backend presents every panel write on its own, so the transform is
    // composed off-screen and the finished frame blitted in a single call.
    const float centre = 0.5f * FACE_FRAME_SIZE;
    faceFrame.fillSprite(COLOUR_BG);
    faceSprite.pushRotateZoom(&faceFrame, centre, centre, faceTransform.rotation, faceTransform.scaleX, faceTransform.scaleY);
    faceFrame.pushSprite(&display, static_cast<int>(faceTransform.x - centre), static_cast<int>(faceTransform.y - centre));
}


static void showFace(FaceState state, float x, float y) {
    Face::getFace().setState(state);
    faceTransform = {x, y, 0.f, 1.f, 1.f, true};
    // The screen repaint underneath wipes the face, and the face may be idle.
    faceNeedsRedraw = true;
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
    showFace(FACE_NEUTRAL, 0.5f * SCREEN_WIDTH, 100.f);

    drawBackground();
    drawUIButton("PLAY", BUTTON_PLAY_X, MAIN_MENU_BUTTON_Y);
    drawUIButton("SETTINGS", BUTTON_SETTINGS_X, MAIN_MENU_BUTTON_Y);
    canvas.pushSprite(0, 0);
}


static void initSprite(LGFX_Sprite& sprite, int width, int height) {
    sprite.setColorDepth(16);
    #ifndef C4_DESKTOP
        sprite.setPsram(true);
    #endif
    sprite.createSprite(width, height);
}


void initDisplay() {
    display.init();
    display.setRotation(0);
    display.setBrightness(128);
    display.setColorDepth(16);

    initSprite(canvas, SCREEN_WIDTH, SCREEN_HEIGHT);
    initSprite(faceSprite, FACE_SPRITE_W, FACE_SPRITE_H);
    initSprite(faceFrame, FACE_FRAME_SIZE, FACE_FRAME_SIZE);
}


void drawDisplay() {
    const uint32_t now = getNow();

    if (screenState != previousScreenState) {
        switch (screenState) {
            case SCREEN_MAIN_MENU:
                drawMainMenuScreen();
                break;
            case SCREEN_SETTINGS_MENU:
            case SCREEN_NULL:
                break;
        }
        previousScreenState = screenState;
    }

    if (faceTransform.visible && now >= faceRefreshTimer) {
        faceRefreshTimer = now + FACE_REFRESH_RATE_MS;
        drawFace(now);
    }
}


void setScreen(ScreenState state) {
    screenState = state;
}
