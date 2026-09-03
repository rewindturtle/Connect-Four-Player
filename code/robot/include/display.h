#ifndef DISPLAY_H
#define DISPLAY_H

#include "player.h"

#define LGFX_AUTODETECT
#include <LovyanGFX.hpp>

enum FaceState : uint8_t;


enum ScreenState : uint8_t {
    SCREEN_NULL,
    SCREEN_MAIN_MENU,
    SCREEN_SETTINGS_MENU
};


class Display {
    private:
        struct FaceTransform {
            float x = 0.f;
            float y = 0.f;
            float rotation = 0.f;
            float scaleX = 1.f;
            float scaleY = 1.f;
            bool visible = false;
        };

        LGFX _display;
        LGFX_Sprite _canvas;
        LGFX_Sprite _faceSprite;
        LGFX_Sprite _faceFrame;
        LGFX_Sprite _settingsValueSprite;
        ScreenState _screenState;
        ScreenState _previousScreenState;
        FaceTransform _faceTransform;
        uint32_t _faceRefreshTimer;
        bool _faceNeedsRedraw;
        bool _touchWasPressed;
        uint8_t _maxSearchDepth;

        void _drawFace(uint32_t now);
        void _showFace(FaceState state, float x, float y);
        void _drawBackground();
        void _drawUIButton(const char* text, int x, int y, int width, int height);
        void _drawUILabel(const char* text, int x, int y, bool compact = false);
        void _drawMaxSearchDepthValue();
        void _drawMainMenuScreen();
        void _drawSettingsMenuScreen();
        void _handleTouch(uint16_t x, uint16_t y);
        void _selectPlayStyle(int8_t direction);
        void _selectMaxSearchDepth(int8_t direction);
    public:
        Display();

        Display(const Display&) = delete;
        Display& operator=(const Display&) = delete;
        Display(Display&&) = delete;
        Display& operator=(Display&&) = delete;

        void init();
        void draw();
        void setScreen(ScreenState state);
        inline ScreenState getScreen() const {return _screenState;}
        void setFacePlayStyle(PlayStyle playStyle);
        inline uint8_t getMaxSearchDepth() const {return _maxSearchDepth;}
};

#endif // DISPLAY_H
