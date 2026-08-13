#include "ui.h"

#define LGFX_AUTODETECT
#include <LovyanGFX.h>

static LGFX lcd;
static LGFX_Sprite sprite(&lcd);


void initUI() {
    lcd.init();
    lcd.setRotation(1);
    lcd.setBrightness(128);
    lcd.setColorDepth(24);
}
