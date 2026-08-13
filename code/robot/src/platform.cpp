#include "platform.h"

#ifdef C4_DESKTOP

#include <random>
#include <chrono>

uint32_t randomU32() {
    static std::mt19937 gen(std::random_device{}());
    return gen();
}


uint32_t getNow() {
    return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

#endif // C4_DESKTOP