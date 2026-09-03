#include "display.h"
#include "face.h"
#include "platform.h"
#include "player.h"

#include <math.h>
#include <string.h>

#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 320

#define COLOUR_BG 0x0862
#define COLOUR_GRID 0x1988
#define COLOUR_CYAN_DIM 0x1A30
#define COLOUR_CYAN_FAINT 0x0929
#define COLOUR_TITLE 0xEF3A

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

#define SETTINGS_ROW_Y 215
#define SETTINGS_ROW_HEIGHT 44
#define SETTINGS_TITLE_X 68
#define SETTINGS_LEFT_ARROW_X 132
#define SETTINGS_ARROW_WIDTH 44
#define SETTINGS_STYLE_NAME_X 276
#define SETTINGS_RIGHT_ARROW_X 378
#define SETTINGS_BACK_X 372
#define SETTINGS_BACK_Y 270
#define SETTINGS_BACK_WIDTH 88
#define SETTINGS_BACK_HEIGHT 38


static const char* const PLAY_STYLE_NAMES[PLAY_STYLE_COUNT] = {
    "STANDARD",
    "MISTAKES",
    "PROLONG",
    "CENTER",
    "EDGE",
    "STACKER",
    "SPREADER",
    "PACIFIST",
    "COPYCAT",
    "TRAP"
};


static uint16_t colourToRgb565(const Colour& c) {
    return ((c.r >> 3) << 11) | ((c.g >> 2) << 5) | (c.b >> 3);
}


static Colour dimColour(const Colour& colour, uint8_t brightness) {
    return {
        static_cast<uint8_t>((static_cast<uint16_t>(colour.r) * brightness) / 255),
        static_cast<uint8_t>((static_cast<uint16_t>(colour.g) * brightness) / 255),
        static_cast<uint8_t>((static_cast<uint16_t>(colour.b) * brightness) / 255)
    };
}


static Colour lightenColour(const Colour& colour, uint8_t amount) {
    return {
        static_cast<uint8_t>(colour.r + ((255 - colour.r) * amount) / 255),
        static_cast<uint8_t>(colour.g + ((255 - colour.g) * amount) / 255),
        static_cast<uint8_t>(colour.b + ((255 - colour.b) * amount) / 255)
    };
}


static bool pointInRect(uint16_t px, uint16_t py, int x, int y, int width, int height) {
    return px >= x && px < x + width && py >= y && py < y + height;
}


static void drawRotatedRect(LovyanGFX& gfx, float cx, float cy, float w, float h, float angleDeg, uint16_t colour) {
    float rad = angleDeg * 0.01745329251f;
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

    drawRotatedRect(gfx, 0.5f * (x1 + x2), 0.5f * (y1 + y2), length, eyebrow.strokeWidth,
                    atan2f(dy, dx) / 0.01745329251f, colour);
}


static void drawTriangle(LovyanGFX& gfx, const TriangleParams& triangle, const FaceOffset& offset, uint16_t colour) {
    int x1 = static_cast<int>(triangle.x1 + offset.x);
    int y1 = static_cast<int>(triangle.y1 + offset.y);
    int x2 = static_cast<int>(triangle.x2 + offset.x);
    int y2 = static_cast<int>(triangle.y2 + offset.y);
    int x3 = static_cast<int>(triangle.x3 + offset.x);
    int y3 = static_cast<int>(triangle.y3 + offset.y);

    if (triangle.filled) {
        gfx.fillTriangle(x1, y1, x2, y2, x3, y3, colour);
        return;
    }

    if (triangle.strokeWidth < 1.f) return;

    const float points[3][2] = {
        {static_cast<float>(x1), static_cast<float>(y1)},
        {static_cast<float>(x2), static_cast<float>(y2)},
        {static_cast<float>(x3), static_cast<float>(y3)}
    };
    for (uint8_t i = 0; i < 3; ++i) {
        uint8_t next = (i + 1) % 3;
        float dx = points[next][0] - points[i][0];
        float dy = points[next][1] - points[i][1];
        float length = sqrtf(dx * dx + dy * dy);
        drawRotatedRect(gfx,
                        0.5f * (points[i][0] + points[next][0]),
                        0.5f * (points[i][1] + points[next][1]),
                        length, triangle.strokeWidth,
                        atan2f(dy, dx) / 0.01745329251f, colour);
    }
}


static void drawDecorationLine(LovyanGFX& gfx, const DecorationLineParams& line,
                               const FaceOffset& offset, uint16_t colour) {
    if (line.strokeWidth < 1.f) return;

    float x1 = line.x1 + offset.x;
    float y1 = line.y1 + offset.y;
    float x2 = line.x2 + offset.x;
    float y2 = line.y2 + offset.y;
    float dx = x2 - x1;
    float dy = y2 - y1;
    float length = sqrtf(dx * dx + dy * dy);
    if (length < 1.f) return;

    drawRotatedRect(gfx, 0.5f * (x1 + x2), 0.5f * (y1 + y2), length,
                    line.strokeWidth, atan2f(dy, dx) / 0.01745329251f, colour);
}


static void drawTeardrop(LovyanGFX& gfx, const TeardropParams& teardrop,
                         const FaceOffset& offset, uint16_t colour) {
    if (!teardrop.visible || teardrop.width < 1.f || teardrop.height < 1.f) return;

    float x = teardrop.x + offset.x;
    float tipY = teardrop.y + offset.y;
    float radius = 0.5f * teardrop.width;
    float bulbY = tipY + teardrop.height - radius;

    gfx.fillTriangle(static_cast<int>(x), static_cast<int>(tipY),
                     static_cast<int>(x - radius), static_cast<int>(bulbY),
                     static_cast<int>(x + radius), static_cast<int>(bulbY), colour);
    gfx.fillCircle(static_cast<int>(x), static_cast<int>(bulbY),
                   static_cast<int>(radius), colour);
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
        case MOUTH_SMIRK: {
            float halfRise = 0.5f * mouth.height;
            drawThickBezier(gfx, mx - hw, my + halfRise,
                            mx, my + mouth.curve,
                            mx + hw, my - halfRise,
                            mouth.strokeWidth, colour);
            break;
        }
        case MOUTH_DOTS: {
            int radius = static_cast<int>(0.5f * mouth.height);
            if (radius < 1) radius = 1;
            gfx.fillCircle(static_cast<int>(mx - hw), static_cast<int>(my), radius, colour);
            gfx.fillCircle(static_cast<int>(mx), static_cast<int>(my), radius, colour);
            gfx.fillCircle(static_cast<int>(mx + hw), static_cast<int>(my), radius, colour);
            break;
        }
    }
}


void Display::_drawFace(uint32_t now) {
    Face& face = Face::getFace();
    face.update(now);

    if (!face.requiresDraw() && !_faceNeedsRedraw) return;
    _faceNeedsRedraw = false;

    const FaceOffset offset = face.getOffset();
    const uint16_t colour = colourToRgb565(face.getColour());
    const FaceDecorations& decorations = face.getDecorations();

    _faceSprite.fillSprite(COLOUR_BG);
    drawTriangle(_faceSprite, decorations.leftEar, offset, colour);
    drawTriangle(_faceSprite, decorations.rightEar, offset, colour);
    drawEyebrow(_faceSprite, face.getLeftBrowParams(), offset, colour);
    drawEyebrow(_faceSprite, face.getRightBrowParams(), offset, colour);
    drawEye(_faceSprite, face.getLeftEyeParams(), offset, colour);
    drawEye(_faceSprite, face.getRightEyeParams(), offset, colour);
    drawTriangle(_faceSprite, decorations.nose, offset, colour);
    drawMouth(_faceSprite, face.getMouthParams(), offset, colour);
    drawTriangle(_faceSprite, decorations.accent, offset, colour);
    drawDecorationLine(_faceSprite, decorations.accentLine1, offset, colour);
    drawDecorationLine(_faceSprite, decorations.accentLine2, offset, colour);
    drawTeardrop(_faceSprite, decorations.teardrop, offset, colour);

    // The SDL backend presents every panel write on its own, so the transform is
    // composed off-screen and the finished frame blitted in a single call.
    const float centre = 0.5f * FACE_FRAME_SIZE;
    _faceFrame.fillSprite(COLOUR_BG);
    _faceSprite.pushRotateZoom(&_faceFrame, centre, centre, _faceTransform.rotation, _faceTransform.scaleX, _faceTransform.scaleY);
    _faceFrame.pushSprite(&_display, static_cast<int>(_faceTransform.x - centre), static_cast<int>(_faceTransform.y - centre));
}


void Display::_showFace(FaceState state, float x, float y) {
    Face& face = Face::getFace();
    face.setState(state);

    _faceTransform = {x, y, 0.f, 1.f, 1.f, true};
    // The screen repaint underneath wipes the face, and the face may be idle.
    _faceNeedsRedraw = true;
}


void Display::_drawBackground() {
    _canvas.fillScreen(COLOUR_BG);
}


void Display::_drawUIButton(const char* text, int x, int y, int width, int height) {
    Colour faceColour = Face::getFace().getPersonalityColour();
    uint16_t fillColour = colourToRgb565(dimColour(faceColour, 48));
    uint16_t outlineColour = colourToRgb565(faceColour);
    uint16_t textColour = colourToRgb565(lightenColour(faceColour, 76));

    _canvas.fillRoundRect(x, y, width, height, BUTTON_CORNER_RADIUS, fillColour);
    _canvas.drawRoundRect(x, y, width, height, BUTTON_CORNER_RADIUS, outlineColour);

    _canvas.setFont(&fonts::FreeSans9pt7b);
    _canvas.setTextColor(textColour);
    _canvas.setTextDatum(textdatum_t::middle_centre);
    _canvas.drawString(text, x + (width / 2), y + (height / 2));
}


void Display::_drawUILabel(const char* text, int x, int y) {
    Colour faceColour = Face::getFace().getPersonalityColour();
    _canvas.setFont(&fonts::FreeSans9pt7b);
    _canvas.setTextColor(colourToRgb565(lightenColour(faceColour, 76)));
    _canvas.setTextDatum(textdatum_t::middle_centre);
    _canvas.drawString(text, x, y);
}


void Display::_drawMainMenuScreen() {
    _showFace(FACE_NEUTRAL, 0.5f * SCREEN_WIDTH, 100.f);

    _drawBackground();
    _drawUIButton("PLAY", BUTTON_PLAY_X, MAIN_MENU_BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT);
    _drawUIButton("SETTINGS", BUTTON_SETTINGS_X, MAIN_MENU_BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT);
    _canvas.pushSprite(0, 0);
}


void Display::_drawSettingsMenuScreen() {
    if (!_faceTransform.visible) {
        _showFace(FACE_NEUTRAL, 0.5f * SCREEN_WIDTH, 100.f);
    }

    _drawBackground();
    _drawUILabel("PLAY STYLE", SETTINGS_TITLE_X, SETTINGS_ROW_Y + (SETTINGS_ROW_HEIGHT / 2));
    _drawUIButton("<", SETTINGS_LEFT_ARROW_X, SETTINGS_ROW_Y, SETTINGS_ARROW_WIDTH, SETTINGS_ROW_HEIGHT);
    _drawUILabel(PLAY_STYLE_NAMES[Face::getFace().getPersonality()],
                 SETTINGS_STYLE_NAME_X, SETTINGS_ROW_Y + (SETTINGS_ROW_HEIGHT / 2));
    _drawUIButton(">", SETTINGS_RIGHT_ARROW_X, SETTINGS_ROW_Y, SETTINGS_ARROW_WIDTH, SETTINGS_ROW_HEIGHT);
    _drawUIButton("BACK", SETTINGS_BACK_X, SETTINGS_BACK_Y, SETTINGS_BACK_WIDTH, SETTINGS_BACK_HEIGHT);
    _canvas.pushSprite(0, 0);

    // The full-screen canvas repaint erased the independently rendered face.
    _faceNeedsRedraw = true;
}


void Display::_selectPlayStyle(int8_t direction) {
    int next = static_cast<int>(Face::getFace().getPersonality()) + direction;
    if (next < 0) next = PLAY_STYLE_COUNT - 1;
    if (next >= PLAY_STYLE_COUNT) next = 0;
    setFacePlayStyle(static_cast<PlayStyle>(next));
}


void Display::_handleTouch(uint16_t x, uint16_t y) {
    switch (_screenState) {
        case SCREEN_MAIN_MENU:
            if (pointInRect(x, y, BUTTON_SETTINGS_X, MAIN_MENU_BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT)) {
                setScreen(SCREEN_SETTINGS_MENU);
            }
            break;
        case SCREEN_SETTINGS_MENU:
            if (pointInRect(x, y, SETTINGS_LEFT_ARROW_X, SETTINGS_ROW_Y,
                            SETTINGS_ARROW_WIDTH, SETTINGS_ROW_HEIGHT)) {
                _selectPlayStyle(-1);
            } else if (pointInRect(x, y, SETTINGS_RIGHT_ARROW_X, SETTINGS_ROW_Y,
                                   SETTINGS_ARROW_WIDTH, SETTINGS_ROW_HEIGHT)) {
                _selectPlayStyle(1);
            } else if (pointInRect(x, y, SETTINGS_BACK_X, SETTINGS_BACK_Y,
                                   SETTINGS_BACK_WIDTH, SETTINGS_BACK_HEIGHT)) {
                setScreen(SCREEN_MAIN_MENU);
            }
            break;
        case SCREEN_NULL:
            break;
    }
}


static void initSprite(LGFX_Sprite& sprite, int width, int height) {
    sprite.setColorDepth(16);
    #ifndef C4_DESKTOP
        sprite.setPsram(true);
    #endif
    sprite.createSprite(width, height);
}


Display::Display()
    #ifdef C4_DESKTOP
    : _display(SCREEN_WIDTH, SCREEN_HEIGHT, 2, 2),
    #else
    : _display(),
    #endif
      _canvas(&_display),
      _faceSprite(&_display),
      _faceFrame(&_display),
      _screenState(SCREEN_MAIN_MENU),
      _previousScreenState(SCREEN_NULL),
      _faceTransform(),
      _faceRefreshTimer(0),
      _faceNeedsRedraw(false),
      _touchWasPressed(false) {}


void Display::init() {
    _display.init();
    _display.setRotation(0);
    _display.setBrightness(128);
    _display.setColorDepth(16);

    initSprite(_canvas, SCREEN_WIDTH, SCREEN_HEIGHT);
    initSprite(_faceSprite, FACE_SPRITE_W, FACE_SPRITE_H);
    initSprite(_faceFrame, FACE_FRAME_SIZE, FACE_FRAME_SIZE);

    PlayStyle playStyle = static_cast<PlayStyle>(randomU32() % static_cast<uint32_t>(PLAY_STYLE_COUNT));
    Face::getFace().setPersonality(playStyle);
}


void Display::draw() {
    const uint32_t now = getNow();

    uint16_t touchX;
    uint16_t touchY;
    bool touchIsPressed = _display.getTouch(&touchX, &touchY) != 0;
    if (touchIsPressed && !_touchWasPressed) {
        _handleTouch(touchX, touchY);
    }
    _touchWasPressed = touchIsPressed;

    if (_screenState != _previousScreenState) {
        switch (_screenState) {
            case SCREEN_MAIN_MENU:
                _drawMainMenuScreen();
                break;
            case SCREEN_SETTINGS_MENU:
                _drawSettingsMenuScreen();
                break;
            case SCREEN_NULL:
                break;
        }
        _previousScreenState = _screenState;
    }

    if (_faceTransform.visible && (_faceNeedsRedraw || now >= _faceRefreshTimer)) {
        _faceRefreshTimer = now + FACE_REFRESH_RATE_MS;
        _drawFace(now);
    }
}


void Display::setScreen(ScreenState state) {
    _screenState = state;
}


void Display::setFacePlayStyle(PlayStyle playStyle) {
    if (playStyle >= PLAY_STYLE_COUNT) playStyle = STANDARD_PLAY_STYLE;
    Face::getFace().setPersonality(playStyle);
    _faceNeedsRedraw = true;
    _previousScreenState = SCREEN_NULL;
}
