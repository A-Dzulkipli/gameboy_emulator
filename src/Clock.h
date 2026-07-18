#pragma once

#include <cstdint>
#include <unistd.h>
#include <vector>
#include <unordered_map>
#include <utility>

namespace gb_emulator {
    template <typename ConcreteClock>
    class Clock {
    public:
        void tick(int cycles) {
            static_cast<ConcreteClock*>(this)->tick_impl(cycles);
        }
    };

    class NothingClock : public Clock<NothingClock> {
    public:
        void tick_impl(int cycles) {}
    };
}
