#include <lgfx/v1/platforms/sdl/Panel_sdl.hpp>

#ifdef SDL_h_

void setup();
void loop();


static int desktopUI(bool* running) {
    setup();
    while (*running) {
        loop();
    }
    return 0;
}


int main(int, char**) {
    return lgfx::Panel_sdl::main(desktopUI);
}

#endif
