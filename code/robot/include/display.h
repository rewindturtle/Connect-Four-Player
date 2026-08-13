#ifndef DISPLAY_H
#define DISPLAY_H

enum ScreenState {
    SCREEN_NULL,
    SCREEN_MAIN_MENU,
    SCREEN_SETTINGS_MENU
};

void initDisplay();
void drawDisplay();
void setScreen(ScreenState state);

#endif // DISPLAY_H
