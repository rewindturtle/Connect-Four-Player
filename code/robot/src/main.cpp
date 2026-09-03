#include "display.h"

#ifndef C4_DESKTOP
    #include <Arduino.h>
    #include <freertos/FreeRTOS.h>
    #include <freertos/task.h>
#endif


#define GAME_LOGIC_CORE 0
#define PRESENTATION_CORE 1
#define GAME_LOGIC_STACK_BYTES 16384


Display display;


// Board updates, input handling, and Player::chooseMove() belong here. On the
// ESP32 this function is called only by the task pinned to GAME_LOGIC_CORE.
static void updateGameLogic() {}


#ifndef C4_DESKTOP
    static void gameLogicTask(void*) {
        configASSERT(xPortGetCoreID() == GAME_LOGIC_CORE);

        for (;;) {
            updateGameLogic();

            // Planning runs at idle priority, but blocking for one tick also gives
            // core 0's system and idle tasks an unconditional opportunity to run.
            vTaskDelay(1);
        }
    }
#endif


void setup() {
    display.init();

    #ifndef C4_DESKTOP
        static_assert(portNUM_PROCESSORS >= 2, "Game and presentation tasks require two cores");
        static_assert(ARDUINO_RUNNING_CORE == PRESENTATION_CORE, "Arduino loop must run on the presentation core");
        configASSERT(xPortGetCoreID() == PRESENTATION_CORE);

        BaseType_t created = xTaskCreatePinnedToCore(gameLogicTask, "gameLogic", GAME_LOGIC_STACK_BYTES,
                                                     nullptr, tskIDLE_PRIORITY, nullptr, GAME_LOGIC_CORE);

        configASSERT(created == pdPASS);
    #endif
}


void loop() {
    #ifdef C4_DESKTOP
        // The desktop build has no second MCU core, so both sides advance on
        // the SDL thread while retaining the same ownership boundary.
        updateGameLogic();
    #endif

    display.draw();

    #ifndef C4_DESKTOP
        // Presentation work owns core 1 but should still yield between frames.
        vTaskDelay(1);
    #endif
}
