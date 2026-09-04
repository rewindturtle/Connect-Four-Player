#include "display.h"
#include "face.h"
#include "platform.h"
#include "player.h"

#include <math.h>
#include <stdio.h>

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
#define MENU_FACE_Y 100.f
#define MAIN_MENU_BUTTON_Y 210

#define SETTINGS_PLAY_STYLE_Y 150
#define SETTINGS_MAX_DEPTH_Y 178
#define SETTINGS_MAX_TIME_Y 206
#define SETTINGS_IDLE_SEARCH_Y 234
#define SETTINGS_PRELOAD_MEMO_Y 150
#define SETTINGS_CAN_PANIC_Y 178
#define SETTINGS_MISTAKE_PROBABILITY_Y 206
#define SETTINGS_ROW_HEIGHT 26
#define SETTINGS_TITLE_X 130
#define SETTINGS_LEFT_ARROW_X 240
#define SETTINGS_ARROW_WIDTH 30
#define SETTINGS_STYLE_NAME_X 327
#define SETTINGS_RIGHT_ARROW_X 384
#define SETTINGS_VALUE_X 272
#define SETTINGS_VALUE_WIDTH 110
#define SETTINGS_CHECKBOX_SIZE 18
#define SETTINGS_PAGE_BUTTON_X 300
#define SETTINGS_PAGE_BUTTON_Y 281
#define SETTINGS_PAGE_BUTTON_WIDTH 80
#define SETTINGS_BACK_X 388
#define SETTINGS_BACK_Y 281
#define SETTINGS_BACK_WIDTH 80
#define SETTINGS_BACK_HEIGHT 26

#define MIN_SEARCH_DEPTH 1
#define MAX_SEARCH_DEPTH 42
#define DEFAULT_MAX_SEARCH_DEPTH 6
#define MIN_THINKING_TIME_SECONDS 1
#define MAX_THINKING_TIME_SECONDS 99
#define DEFAULT_MAX_THINKING_TIME_SECONDS 10
#define MIN_MISTAKE_PROBABILITY_PERCENT 0
#define MAX_MISTAKE_PROBABILITY_PERCENT 100
#define MISTAKE_PROBABILITY_STEP_PERCENT 5
#define DEFAULT_MISTAKE_PROBABILITY_PERCENT 20
#define DEFAULT_IDLE_SEARCH_ENABLED false
#define DEFAULT_MEMO_PRELOAD_ENABLED false
#define DEFAULT_CAN_PANIC false


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


static inline uint16_t colourToRgb565(const Colour& c) {
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

    // Seed the transform frame with the menu underneath it. This lets settings
    // sit close to the visible face without its larger rotation-safe frame
    // erasing them on an animated redraw.
    const float centre = 0.5f * FACE_FRAME_SIZE;
    const int frameX = static_cast<int>(_faceTransform.x - centre);
    const int frameY = static_cast<int>(_faceTransform.y - centre);
    _faceFrame.fillSprite(COLOUR_BG);
    _canvas.pushSprite(&_faceFrame, -frameX, -frameY);
    _faceSprite.pushRotateZoom(&_faceFrame, centre, centre, _faceTransform.rotation,
                               _faceTransform.scaleX, _faceTransform.scaleY, COLOUR_BG);
    _faceFrame.pushSprite(&_display, frameX, frameY);
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


void Display::_drawUIButton(const char* text, int x, int y, int width, int height, bool highlighted) {
    Colour faceColour = Face::getFace().getPersonalityColour();
    uint16_t fillColour = highlighted
                        ? colourToRgb565(lightenColour(faceColour, 76))
                        : colourToRgb565(dimColour(faceColour, 48));
    uint16_t outlineColour = highlighted
                           ? colourToRgb565(lightenColour(faceColour, 160))
                           : colourToRgb565(faceColour);
    uint16_t textColour = highlighted
                        ? COLOUR_BG
                        : colourToRgb565(lightenColour(faceColour, 76));

    _canvas.fillRoundRect(x, y, width, height, BUTTON_CORNER_RADIUS, fillColour);
    _canvas.drawRoundRect(x, y, width, height, BUTTON_CORNER_RADIUS, outlineColour);

    _canvas.setFont(&fonts::FreeSans9pt7b);
    _canvas.setTextColor(textColour);
    _canvas.setTextDatum(textdatum_t::middle_centre);
    _canvas.drawString(text, x + (width / 2), y + (height / 2));
}


bool Display::_getButtonLayout(UIButton button, const char*& text,
                               int& x, int& y, int& width, int& height) const {
    width = SETTINGS_ARROW_WIDTH;
    height = SETTINGS_ROW_HEIGHT;

    switch (button) {
        case UI_BUTTON_PLAY:
            text = "PLAY";
            x = BUTTON_PLAY_X;
            y = MAIN_MENU_BUTTON_Y;
            width = BUTTON_WIDTH;
            height = BUTTON_HEIGHT;
            return true;
        case UI_BUTTON_SETTINGS:
            text = "SETTINGS";
            x = BUTTON_SETTINGS_X;
            y = MAIN_MENU_BUTTON_Y;
            width = BUTTON_WIDTH;
            height = BUTTON_HEIGHT;
            return true;
        case UI_BUTTON_PLAY_STYLE_PREVIOUS:
            text = "<";
            x = SETTINGS_LEFT_ARROW_X;
            y = SETTINGS_PLAY_STYLE_Y;
            return true;
        case UI_BUTTON_PLAY_STYLE_NEXT:
            text = ">";
            x = SETTINGS_RIGHT_ARROW_X;
            y = SETTINGS_PLAY_STYLE_Y;
            return true;
        case UI_BUTTON_MAX_DEPTH_DECREASE:
            text = "<";
            x = SETTINGS_LEFT_ARROW_X;
            y = SETTINGS_MAX_DEPTH_Y;
            return true;
        case UI_BUTTON_MAX_DEPTH_INCREASE:
            text = ">";
            x = SETTINGS_RIGHT_ARROW_X;
            y = SETTINGS_MAX_DEPTH_Y;
            return true;
        case UI_BUTTON_MAX_TIME_DECREASE:
            text = "<";
            x = SETTINGS_LEFT_ARROW_X;
            y = SETTINGS_MAX_TIME_Y;
            return true;
        case UI_BUTTON_MAX_TIME_INCREASE:
            text = ">";
            x = SETTINGS_RIGHT_ARROW_X;
            y = SETTINGS_MAX_TIME_Y;
            return true;
        case UI_BUTTON_MISTAKE_PROBABILITY_DECREASE:
            text = "<";
            x = SETTINGS_LEFT_ARROW_X;
            y = SETTINGS_MISTAKE_PROBABILITY_Y;
            return true;
        case UI_BUTTON_MISTAKE_PROBABILITY_INCREASE:
            text = ">";
            x = SETTINGS_RIGHT_ARROW_X;
            y = SETTINGS_MISTAKE_PROBABILITY_Y;
            return true;
        case UI_BUTTON_PREVIOUS_PAGE:
            text = "PREV";
            x = SETTINGS_PAGE_BUTTON_X;
            y = SETTINGS_PAGE_BUTTON_Y;
            width = SETTINGS_PAGE_BUTTON_WIDTH;
            return true;
        case UI_BUTTON_NEXT_PAGE:
            text = "NEXT";
            x = SETTINGS_PAGE_BUTTON_X;
            y = SETTINGS_PAGE_BUTTON_Y;
            width = SETTINGS_PAGE_BUTTON_WIDTH;
            return true;
        case UI_BUTTON_BACK:
            text = "BACK";
            x = SETTINGS_BACK_X;
            y = SETTINGS_BACK_Y;
            width = SETTINGS_BACK_WIDTH;
            height = SETTINGS_BACK_HEIGHT;
            return true;
        case UI_BUTTON_NONE:
            return false;
    }

    return false;
}


Display::UIButton Display::_buttonAt(uint16_t x, uint16_t y) const {
    switch (_screenState) {
        case SCREEN_MAIN_MENU:
            if (pointInRect(x, y, BUTTON_PLAY_X, MAIN_MENU_BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT)) {
                return UI_BUTTON_PLAY;
            }
            if (pointInRect(x, y, BUTTON_SETTINGS_X, MAIN_MENU_BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT)) {
                return UI_BUTTON_SETTINGS;
            }
            break;
        case SCREEN_SETTINGS_MENU:
            if (pointInRect(x, y, SETTINGS_LEFT_ARROW_X, SETTINGS_PLAY_STYLE_Y,
                            SETTINGS_ARROW_WIDTH, SETTINGS_ROW_HEIGHT)) {
                return UI_BUTTON_PLAY_STYLE_PREVIOUS;
            }
            if (pointInRect(x, y, SETTINGS_RIGHT_ARROW_X, SETTINGS_PLAY_STYLE_Y,
                            SETTINGS_ARROW_WIDTH, SETTINGS_ROW_HEIGHT)) {
                return UI_BUTTON_PLAY_STYLE_NEXT;
            }
            if (pointInRect(x, y, SETTINGS_LEFT_ARROW_X, SETTINGS_MAX_DEPTH_Y,
                            SETTINGS_ARROW_WIDTH, SETTINGS_ROW_HEIGHT)) {
                return UI_BUTTON_MAX_DEPTH_DECREASE;
            }
            if (pointInRect(x, y, SETTINGS_RIGHT_ARROW_X, SETTINGS_MAX_DEPTH_Y,
                            SETTINGS_ARROW_WIDTH, SETTINGS_ROW_HEIGHT)) {
                return UI_BUTTON_MAX_DEPTH_INCREASE;
            }
            if (pointInRect(x, y, SETTINGS_LEFT_ARROW_X, SETTINGS_MAX_TIME_Y,
                            SETTINGS_ARROW_WIDTH, SETTINGS_ROW_HEIGHT)) {
                return UI_BUTTON_MAX_TIME_DECREASE;
            }
            if (pointInRect(x, y, SETTINGS_RIGHT_ARROW_X, SETTINGS_MAX_TIME_Y,
                            SETTINGS_ARROW_WIDTH, SETTINGS_ROW_HEIGHT)) {
                return UI_BUTTON_MAX_TIME_INCREASE;
            }
            if (pointInRect(x, y, SETTINGS_PAGE_BUTTON_X, SETTINGS_PAGE_BUTTON_Y,
                            SETTINGS_PAGE_BUTTON_WIDTH, SETTINGS_ROW_HEIGHT)) {
                return UI_BUTTON_NEXT_PAGE;
            }
            if (pointInRect(x, y, SETTINGS_BACK_X, SETTINGS_BACK_Y,
                            SETTINGS_BACK_WIDTH, SETTINGS_BACK_HEIGHT)) {
                return UI_BUTTON_BACK;
            }
            break;
        case SCREEN_MEMO_SETTINGS_MENU:
            if (Face::getFace().getPersonality() == MISTAKES_PLAY_STYLE
                && pointInRect(x, y, SETTINGS_LEFT_ARROW_X, SETTINGS_MISTAKE_PROBABILITY_Y,
                               SETTINGS_ARROW_WIDTH, SETTINGS_ROW_HEIGHT)) {
                return UI_BUTTON_MISTAKE_PROBABILITY_DECREASE;
            }
            if (Face::getFace().getPersonality() == MISTAKES_PLAY_STYLE
                && pointInRect(x, y, SETTINGS_RIGHT_ARROW_X, SETTINGS_MISTAKE_PROBABILITY_Y,
                               SETTINGS_ARROW_WIDTH, SETTINGS_ROW_HEIGHT)) {
                return UI_BUTTON_MISTAKE_PROBABILITY_INCREASE;
            }
            if (pointInRect(x, y, SETTINGS_PAGE_BUTTON_X, SETTINGS_PAGE_BUTTON_Y,
                            SETTINGS_PAGE_BUTTON_WIDTH, SETTINGS_ROW_HEIGHT)) {
                return UI_BUTTON_PREVIOUS_PAGE;
            }
            if (pointInRect(x, y, SETTINGS_BACK_X, SETTINGS_BACK_Y,
                            SETTINGS_BACK_WIDTH, SETTINGS_BACK_HEIGHT)) {
                return UI_BUTTON_BACK;
            }
            break;
        case SCREEN_NULL:
            break;
    }

    return UI_BUTTON_NONE;
}


void Display::_redrawButton(UIButton button, bool highlighted) {
    const char* text;
    int x;
    int y;
    int width;
    int height;
    if (!_getButtonLayout(button, text, x, y, width, height)) return;

    _drawUIButton(text, x, y, width, height, highlighted);

    // Copy only the affected display region. The backing canvas retains the
    // press state for subsequent animated face redraws.
    _buttonSprite.fillSprite(COLOUR_BG);
    _canvas.pushSprite(&_buttonSprite, -x, -y);
    _buttonSprite.pushSprite(x, y);
    _faceNeedsRedraw = true;
}


void Display::_activateButton(UIButton button) {
    switch (button) {
        case UI_BUTTON_SETTINGS:
            setScreen(SCREEN_SETTINGS_MENU);
            break;
        case UI_BUTTON_PLAY_STYLE_PREVIOUS:
            _selectPlayStyle(-1);
            break;
        case UI_BUTTON_PLAY_STYLE_NEXT:
            _selectPlayStyle(1);
            break;
        case UI_BUTTON_MAX_DEPTH_DECREASE:
            _selectMaxSearchDepth(-1);
            break;
        case UI_BUTTON_MAX_DEPTH_INCREASE:
            _selectMaxSearchDepth(1);
            break;
        case UI_BUTTON_MAX_TIME_DECREASE:
            _selectMaxThinkingTime(-1);
            break;
        case UI_BUTTON_MAX_TIME_INCREASE:
            _selectMaxThinkingTime(1);
            break;
        case UI_BUTTON_MISTAKE_PROBABILITY_DECREASE:
            _selectMistakeProbability(-1);
            break;
        case UI_BUTTON_MISTAKE_PROBABILITY_INCREASE:
            _selectMistakeProbability(1);
            break;
        case UI_BUTTON_PREVIOUS_PAGE:
            setScreen(SCREEN_SETTINGS_MENU);
            break;
        case UI_BUTTON_NEXT_PAGE:
            setScreen(SCREEN_MEMO_SETTINGS_MENU);
            break;
        case UI_BUTTON_BACK:
            setScreen(SCREEN_MAIN_MENU);
            break;
        case UI_BUTTON_PLAY:
        case UI_BUTTON_NONE:
            break;
    }
}


void Display::_releaseButton(uint16_t x, uint16_t y) {
    UIButton releasedButton = _pressedButton;
    if (releasedButton == UI_BUTTON_NONE) return;

    bool activate = _buttonAt(x, y) == releasedButton;
    _pressedButton = UI_BUTTON_NONE;
    _redrawButton(releasedButton, false);
    if (activate) _activateButton(releasedButton);
}


void Display::_drawUILabel(const char* text, int x, int y, bool compact) {
    Colour faceColour = Face::getFace().getPersonalityColour();
    if (compact) {
        _canvas.setFont(&fonts::Font2);
    } else {
        _canvas.setFont(&fonts::FreeSans9pt7b);
    }
    _canvas.setTextColor(colourToRgb565(lightenColour(faceColour, 76)));
    _canvas.setTextDatum(textdatum_t::middle_centre);
    _canvas.drawString(text, x, y);
}


void Display::_drawSettingValue(uint8_t value, int y, const char* suffix) {
    char valueText[6];
    snprintf(valueText, sizeof(valueText), "%u%s", static_cast<unsigned>(value), suffix);

    Colour faceColour = Face::getFace().getPersonalityColour();
    _settingsValueSprite.fillSprite(COLOUR_BG);
    _settingsValueSprite.setFont(&fonts::FreeSans9pt7b);
    _settingsValueSprite.setTextColor(colourToRgb565(lightenColour(faceColour, 76)));
    _settingsValueSprite.setTextDatum(textdatum_t::middle_centre);
    _settingsValueSprite.drawString(valueText, SETTINGS_VALUE_WIDTH / 2, SETTINGS_ROW_HEIGHT / 2);
    _settingsValueSprite.pushSprite(&_canvas, SETTINGS_VALUE_X, y);
    _settingsValueSprite.pushSprite(SETTINGS_VALUE_X, y);
}


void Display::_drawCheckbox(LGFX_Sprite& target, int centerX, int centerY, bool checked) {
    Colour faceColour = Face::getFace().getPersonalityColour();
    uint16_t outlineColour = colourToRgb565(faceColour);
    int x = centerX - (SETTINGS_CHECKBOX_SIZE / 2);
    int y = centerY - (SETTINGS_CHECKBOX_SIZE / 2);

    target.fillRect(x, y, SETTINGS_CHECKBOX_SIZE, SETTINGS_CHECKBOX_SIZE,
                    checked ? outlineColour : COLOUR_BG);
    target.drawRect(x, y, SETTINGS_CHECKBOX_SIZE, SETTINGS_CHECKBOX_SIZE, outlineColour);
}


void Display::_drawCheckboxValue(bool checked, int y) {
    _settingsValueSprite.fillSprite(COLOUR_BG);
    _drawCheckbox(_settingsValueSprite, SETTINGS_VALUE_WIDTH / 2,
                  SETTINGS_ROW_HEIGHT / 2, checked);
    _settingsValueSprite.pushSprite(&_canvas, SETTINGS_VALUE_X, y);
    _settingsValueSprite.pushSprite(SETTINGS_VALUE_X, y);
}


void Display::_drawMainMenuScreen() {
    _showFace(FACE_NEUTRAL, 0.5f * SCREEN_WIDTH, MENU_FACE_Y);

    _drawBackground();
    _drawUIButton("PLAY", BUTTON_PLAY_X, MAIN_MENU_BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT);
    _drawUIButton("SETTINGS", BUTTON_SETTINGS_X, MAIN_MENU_BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT);
    _canvas.pushSprite(0, 0);
}


void Display::_drawSettingsMenuScreen() {
    if (!_faceTransform.visible) {
        _showFace(FACE_NEUTRAL, 0.5f * SCREEN_WIDTH, MENU_FACE_Y);
    }
    _faceTransform.y = MENU_FACE_Y;
    _faceTransform.scaleX = 1.f;
    _faceTransform.scaleY = 1.f;

    char depthText[3];
    snprintf(depthText, sizeof(depthText), "%u", static_cast<unsigned>(_maxSearchDepth));
    char timeText[6];
    snprintf(timeText, sizeof(timeText), "%u s", static_cast<unsigned>(_maxThinkingTimeSeconds));

    _drawBackground();
    _drawUILabel("PLAY STYLE", SETTINGS_TITLE_X,
                 SETTINGS_PLAY_STYLE_Y + (SETTINGS_ROW_HEIGHT / 2), true);
    _drawUIButton("<", SETTINGS_LEFT_ARROW_X, SETTINGS_PLAY_STYLE_Y,
                  SETTINGS_ARROW_WIDTH, SETTINGS_ROW_HEIGHT);
    _drawUILabel(PLAY_STYLE_NAMES[Face::getFace().getPersonality()],
                 SETTINGS_STYLE_NAME_X, SETTINGS_PLAY_STYLE_Y + (SETTINGS_ROW_HEIGHT / 2));
    _drawUIButton(">", SETTINGS_RIGHT_ARROW_X, SETTINGS_PLAY_STYLE_Y,
                  SETTINGS_ARROW_WIDTH, SETTINGS_ROW_HEIGHT);

    _drawUILabel("MAX SEARCH DEPTH", SETTINGS_TITLE_X,
                 SETTINGS_MAX_DEPTH_Y + (SETTINGS_ROW_HEIGHT / 2), true);
    _drawUIButton("<", SETTINGS_LEFT_ARROW_X, SETTINGS_MAX_DEPTH_Y,
                  SETTINGS_ARROW_WIDTH, SETTINGS_ROW_HEIGHT);
    _drawUILabel(depthText, SETTINGS_STYLE_NAME_X,
                 SETTINGS_MAX_DEPTH_Y + (SETTINGS_ROW_HEIGHT / 2));
    _drawUIButton(">", SETTINGS_RIGHT_ARROW_X, SETTINGS_MAX_DEPTH_Y,
                  SETTINGS_ARROW_WIDTH, SETTINGS_ROW_HEIGHT);

    _drawUILabel("MAX THINKING TIME", SETTINGS_TITLE_X,
                 SETTINGS_MAX_TIME_Y + (SETTINGS_ROW_HEIGHT / 2), true);
    _drawUIButton("<", SETTINGS_LEFT_ARROW_X, SETTINGS_MAX_TIME_Y,
                  SETTINGS_ARROW_WIDTH, SETTINGS_ROW_HEIGHT);
    _drawUILabel(timeText, SETTINGS_STYLE_NAME_X,
                 SETTINGS_MAX_TIME_Y + (SETTINGS_ROW_HEIGHT / 2));
    _drawUIButton(">", SETTINGS_RIGHT_ARROW_X, SETTINGS_MAX_TIME_Y,
                  SETTINGS_ARROW_WIDTH, SETTINGS_ROW_HEIGHT);

    _drawUILabel("IDLE SEARCH", SETTINGS_TITLE_X,
                 SETTINGS_IDLE_SEARCH_Y + (SETTINGS_ROW_HEIGHT / 2), true);
    _drawCheckbox(_canvas, SETTINGS_STYLE_NAME_X,
                  SETTINGS_IDLE_SEARCH_Y + (SETTINGS_ROW_HEIGHT / 2), _idleSearchEnabled);

    _drawUIButton("NEXT", SETTINGS_PAGE_BUTTON_X, SETTINGS_PAGE_BUTTON_Y,
                  SETTINGS_PAGE_BUTTON_WIDTH, SETTINGS_ROW_HEIGHT);
    _drawUIButton("BACK", SETTINGS_BACK_X, SETTINGS_BACK_Y, SETTINGS_BACK_WIDTH, SETTINGS_BACK_HEIGHT);
    _canvas.pushSprite(0, 0);

    // The full-screen canvas repaint erased the independently rendered face.
    _faceNeedsRedraw = true;
}


void Display::_drawMemoSettingsMenuScreen() {
    if (!_faceTransform.visible) {
        _showFace(FACE_NEUTRAL, 0.5f * SCREEN_WIDTH, MENU_FACE_Y);
    }
    _faceTransform.y = MENU_FACE_Y;
    _faceTransform.scaleX = 1.f;
    _faceTransform.scaleY = 1.f;

    _drawBackground();
    _drawUILabel("PRELOAD MEMO", SETTINGS_TITLE_X,
                 SETTINGS_PRELOAD_MEMO_Y + (SETTINGS_ROW_HEIGHT / 2), true);
    _drawCheckbox(_canvas, SETTINGS_STYLE_NAME_X,
                  SETTINGS_PRELOAD_MEMO_Y + (SETTINGS_ROW_HEIGHT / 2), _memoPreloadEnabled);

    _drawUILabel("CAN PANIC", SETTINGS_TITLE_X,
                 SETTINGS_CAN_PANIC_Y + (SETTINGS_ROW_HEIGHT / 2), true);
    _drawCheckbox(_canvas, SETTINGS_STYLE_NAME_X,
                  SETTINGS_CAN_PANIC_Y + (SETTINGS_ROW_HEIGHT / 2), _canPanic);

    if (Face::getFace().getPersonality() == MISTAKES_PLAY_STYLE) {
        char probabilityText[5];
        snprintf(probabilityText, sizeof(probabilityText), "%u%%",
                 static_cast<unsigned>(_mistakeProbabilityPercent));
        _drawUILabel("MISTAKE PROBABILITY", SETTINGS_TITLE_X,
                     SETTINGS_MISTAKE_PROBABILITY_Y + (SETTINGS_ROW_HEIGHT / 2), true);
        _drawUIButton("<", SETTINGS_LEFT_ARROW_X, SETTINGS_MISTAKE_PROBABILITY_Y,
                      SETTINGS_ARROW_WIDTH, SETTINGS_ROW_HEIGHT);
        _drawUILabel(probabilityText, SETTINGS_STYLE_NAME_X,
                     SETTINGS_MISTAKE_PROBABILITY_Y + (SETTINGS_ROW_HEIGHT / 2));
        _drawUIButton(">", SETTINGS_RIGHT_ARROW_X, SETTINGS_MISTAKE_PROBABILITY_Y,
                      SETTINGS_ARROW_WIDTH, SETTINGS_ROW_HEIGHT);
    }

    _drawUIButton("PREV", SETTINGS_PAGE_BUTTON_X, SETTINGS_PAGE_BUTTON_Y,
                  SETTINGS_PAGE_BUTTON_WIDTH, SETTINGS_ROW_HEIGHT);
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


void Display::_selectMaxSearchDepth(int8_t direction) {
    int depth = static_cast<int>(_maxSearchDepth) + direction;
    if (depth < MIN_SEARCH_DEPTH) depth = MIN_SEARCH_DEPTH;
    if (depth > MAX_SEARCH_DEPTH) depth = MAX_SEARCH_DEPTH;
    if (depth == _maxSearchDepth) return;

    _maxSearchDepth = static_cast<uint8_t>(depth);
    _drawSettingValue(_maxSearchDepth, SETTINGS_MAX_DEPTH_Y);
}


void Display::_selectMaxThinkingTime(int8_t direction) {
    int seconds = static_cast<int>(_maxThinkingTimeSeconds) + direction;
    if (seconds < MIN_THINKING_TIME_SECONDS) seconds = MIN_THINKING_TIME_SECONDS;
    if (seconds > MAX_THINKING_TIME_SECONDS) seconds = MAX_THINKING_TIME_SECONDS;
    if (seconds == _maxThinkingTimeSeconds) return;

    _maxThinkingTimeSeconds = static_cast<uint8_t>(seconds);
    _drawSettingValue(_maxThinkingTimeSeconds, SETTINGS_MAX_TIME_Y, " s");
}


void Display::_selectMistakeProbability(int8_t direction) {
    int probability = static_cast<int>(_mistakeProbabilityPercent)
                    + direction * MISTAKE_PROBABILITY_STEP_PERCENT;
    if (probability < MIN_MISTAKE_PROBABILITY_PERCENT) probability = MIN_MISTAKE_PROBABILITY_PERCENT;
    if (probability > MAX_MISTAKE_PROBABILITY_PERCENT) probability = MAX_MISTAKE_PROBABILITY_PERCENT;
    if (probability == _mistakeProbabilityPercent) return;

    _mistakeProbabilityPercent = static_cast<uint8_t>(probability);
    _drawSettingValue(_mistakeProbabilityPercent, SETTINGS_MISTAKE_PROBABILITY_Y, "%");
}


void Display::_toggleIdleSearch() {
    _idleSearchEnabled = !_idleSearchEnabled;
    _drawCheckboxValue(_idleSearchEnabled, SETTINGS_IDLE_SEARCH_Y);
}


void Display::_toggleMemoPreload() {
    _memoPreloadEnabled = !_memoPreloadEnabled;
    _drawCheckboxValue(_memoPreloadEnabled, SETTINGS_PRELOAD_MEMO_Y);
}


void Display::_toggleCanPanic() {
    _canPanic = !_canPanic;
    _drawCheckboxValue(_canPanic, SETTINGS_CAN_PANIC_Y);
}


void Display::_handleTouch(uint16_t x, uint16_t y) {
    UIButton button = _buttonAt(x, y);
    if (button != UI_BUTTON_NONE) {
        _pressedButton = button;
        _redrawButton(button, true);
        return;
    }

    switch (_screenState) {
        case SCREEN_MAIN_MENU:
            break;
        case SCREEN_SETTINGS_MENU:
            if (pointInRect(x, y, SETTINGS_VALUE_X, SETTINGS_IDLE_SEARCH_Y,
                            SETTINGS_VALUE_WIDTH, SETTINGS_ROW_HEIGHT)) {
                _toggleIdleSearch();
            }
            break;
        case SCREEN_MEMO_SETTINGS_MENU:
            if (pointInRect(x, y, SETTINGS_VALUE_X, SETTINGS_PRELOAD_MEMO_Y,
                            SETTINGS_VALUE_WIDTH, SETTINGS_ROW_HEIGHT)) {
                _toggleMemoPreload();
            } else if (pointInRect(x, y, SETTINGS_VALUE_X, SETTINGS_CAN_PANIC_Y,
                                   SETTINGS_VALUE_WIDTH, SETTINGS_ROW_HEIGHT)) {
                _toggleCanPanic();
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
      _settingsValueSprite(&_display),
      _buttonSprite(&_display),
      _screenState(SCREEN_MAIN_MENU),
      _previousScreenState(SCREEN_NULL),
      _faceTransform(),
      _faceRefreshTimer(0),
      _faceNeedsRedraw(false),
      _touchWasPressed(false),
      _pressedButton(UI_BUTTON_NONE),
      _lastTouchX(0),
      _lastTouchY(0),
      _maxSearchDepth(DEFAULT_MAX_SEARCH_DEPTH),
      _maxThinkingTimeSeconds(DEFAULT_MAX_THINKING_TIME_SECONDS),
      _mistakeProbabilityPercent(DEFAULT_MISTAKE_PROBABILITY_PERCENT),
      _idleSearchEnabled(DEFAULT_IDLE_SEARCH_ENABLED),
      _memoPreloadEnabled(DEFAULT_MEMO_PRELOAD_ENABLED),
      _canPanic(DEFAULT_CAN_PANIC) {}


void Display::init() {
    _display.init();
    _display.setRotation(0);
    _display.setBrightness(128);
    _display.setColorDepth(16);

    initSprite(_canvas, SCREEN_WIDTH, SCREEN_HEIGHT);
    initSprite(_faceSprite, FACE_SPRITE_W, FACE_SPRITE_H);
    initSprite(_faceFrame, FACE_FRAME_SIZE, FACE_FRAME_SIZE);
    initSprite(_settingsValueSprite, SETTINGS_VALUE_WIDTH, SETTINGS_ROW_HEIGHT);
    initSprite(_buttonSprite, BUTTON_WIDTH, BUTTON_HEIGHT);

    PlayStyle playStyle = static_cast<PlayStyle>(randomU32() % static_cast<uint32_t>(PLAY_STYLE_COUNT));
    Face::getFace().setPersonality(playStyle);
}


void Display::draw() {
    const uint32_t now = getNow();

    uint16_t touchX;
    uint16_t touchY;
    bool touchIsPressed = _display.getTouch(&touchX, &touchY) != 0;
    if (touchIsPressed) {
        _lastTouchX = touchX;
        _lastTouchY = touchY;

        if (!_touchWasPressed) {
            _handleTouch(touchX, touchY);
        }
    } else if (_touchWasPressed) {
        _releaseButton(_lastTouchX, _lastTouchY);
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
            case SCREEN_MEMO_SETTINGS_MENU:
                _drawMemoSettingsMenuScreen();
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
