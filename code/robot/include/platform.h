#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>
#include <stddef.h>

// Acts as a shim between ESP32 and desktop compilations

#ifdef C4_DESKTOP
    #include <stdlib.h>

    uint32_t randomU32();
    inline void* c4Allocate(size_t bytes) {return malloc(bytes);}
    uint32_t getNow();
#else
    #include <Arduino.h>
    #include <esp_random.h>

    inline uint32_t randomU32() {return esp_random();}
    inline void* c4Allocate(size_t bytes) {return ps_malloc(bytes);}
    inline uint32_t getNow() {return static_cast<uint32_t>(millis());}
#endif

#endif // PLATFORM_H
