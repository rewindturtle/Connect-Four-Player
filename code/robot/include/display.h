#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

enum ScreenState {
    SCREEN_NULL,
    SCREEN_MAIN_MENU,
    SCREEN_SETTINGS_MENU
};

void initDisplay();
void drawDisplay();
void setScreen(ScreenState state);
void setFacePlayStyle(uint8_t playStyle);

#endif // DISPLAY_H
