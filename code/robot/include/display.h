#ifndef DISPLAY_H
#define DISPLAY_H

#include "player.h"

#define LGFX_AUTODETECT
#include <LovyanGFX.hpp>

enum FaceState : uint8_t;


enum ScreenState : uint8_t {
    SCREEN_NULL,
    SCREEN_MAIN_MENU,
    SCREEN_GAME_SETUP_MENU,
    SCREEN_SETTINGS_MENU,
    SCREEN_MEMO_SETTINGS_MENU
};


enum FirstPlayerOption : uint8_t {
    FIRST_PLAYER_COMPUTER,
    FIRST_PLAYER_HUMAN,
    FIRST_PLAYER_RANDOM,
    FIRST_PLAYER_OPTION_COUNT
};


class Display {
    private:
        enum UIButton : uint8_t {
            UI_BUTTON_NONE,
            UI_BUTTON_PLAY,
            UI_BUTTON_SETTINGS,
            UI_BUTTON_FIRST_PLAYER_PREVIOUS,
            UI_BUTTON_FIRST_PLAYER_NEXT,
            UI_BUTTON_START,
            UI_BUTTON_PLAY_STYLE_PREVIOUS,
            UI_BUTTON_PLAY_STYLE_NEXT,
            UI_BUTTON_MAX_DEPTH_DECREASE,
            UI_BUTTON_MAX_DEPTH_INCREASE,
            UI_BUTTON_MAX_TIME_DECREASE,
            UI_BUTTON_MAX_TIME_INCREASE,
            UI_BUTTON_MISTAKE_PROBABILITY_DECREASE,
            UI_BUTTON_MISTAKE_PROBABILITY_INCREASE,
            UI_BUTTON_PREVIOUS_PAGE,
            UI_BUTTON_NEXT_PAGE,
            UI_BUTTON_BACK
        };

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
        LGFX_Sprite _buttonSprite;
        ScreenState _screenState;
        ScreenState _previousScreenState;
        FaceTransform _faceTransform;
        uint32_t _faceRefreshTimer;
        bool _faceNeedsRedraw;
        bool _touchWasPressed;
        UIButton _pressedButton;
        uint16_t _lastTouchX;
        uint16_t _lastTouchY;
        FirstPlayerOption _firstPlayerOption;
        uint8_t _maxSearchDepth;
        uint8_t _maxThinkingTimeSeconds;
        uint8_t _mistakeProbabilityPercent;
        bool _idleSearchEnabled;
        bool _memoPreloadEnabled;
        bool _canPanic;

        void _drawFace(uint32_t now);
        void _showFace(FaceState state, float x, float y);
        void _drawBackground();
        void _drawUIButton(const char* text, int x, int y, int width, int height, bool highlighted = false);
        bool _getButtonLayout(UIButton button, const char*& text, int& x, int& y, int& width, int& height) const;
        UIButton _buttonAt(uint16_t x, uint16_t y) const;
        void _redrawButton(UIButton button, bool highlighted);
        void _activateButton(UIButton button);
        void _releaseButton(uint16_t x, uint16_t y);
        void _drawUILabel(const char* text, int x, int y, bool compact = false);
        void _drawSettingTextValue(const char* text, int y, bool compact = false);
        void _drawSettingValue(uint8_t value, int y, const char* suffix = "");
        void _drawCheckbox(LGFX_Sprite& target, int centerX, int centerY, bool checked);
        void _drawCheckboxValue(bool checked, int y);
        void _drawMainMenuScreen();
        void _drawGameSetupMenuScreen();
        void _drawSettingsMenuScreen();
        void _drawMemoSettingsMenuScreen();
        void _handleTouch(uint16_t x, uint16_t y);
        const char* _getFirstPlayerName() const;
        void _selectFirstPlayer(int8_t direction);
        void _selectPlayStyle(int8_t direction);
        void _selectMaxSearchDepth(int8_t direction);
        void _selectMaxThinkingTime(int8_t direction);
        void _selectMistakeProbability(int8_t direction);
        void _toggleIdleSearch();
        void _toggleMemoPreload();
        void _toggleCanPanic();
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
        inline FirstPlayerOption getFirstPlayerOption() const {return _firstPlayerOption;}
        inline uint8_t getMaxSearchDepth() const {return _maxSearchDepth;}
        inline uint8_t getMaxThinkingTimeSeconds() const {return _maxThinkingTimeSeconds;}
        inline uint8_t getMistakeProbabilityPercent() const {return _mistakeProbabilityPercent;}
        inline bool isIdleSearchEnabled() const {return _idleSearchEnabled;}
        inline bool isMemoPreloadEnabled() const {return _memoPreloadEnabled;}
        inline bool canPanic() const {return _canPanic;}
};

#endif // DISPLAY_H
