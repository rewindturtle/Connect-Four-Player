#ifndef UI_H
#define UI_H

enum ScreenState {
    SCREEN_NULL,
    SCREEN_MAIN_MENU,
    SCREEN_SETTINGS_MENU
};

void initUI();
void drawUI();
void setScreen(ScreenState state);

#endif // UI_H
