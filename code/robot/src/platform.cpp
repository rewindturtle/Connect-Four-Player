#include "platform.h"

#ifdef C4_DESKTOP

#include <random>

uint32_t randomU32() {
    static std::mt19937 gen(std::random_device{}());
    return gen();
}

#endif // C4_DESKTOP