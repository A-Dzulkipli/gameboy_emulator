#pragma once

#include <cstdint>
#include <unistd.h>
#include <vector>
#include <unordered_map>
#include <utility>

// #include "Clock.h"

namespace gb_emulator {
    template <typename ConcreteMem>
    class Mem {
    public:
        std::uint8_t read(std::uint16_t address) {
            return static_cast<ConcreteMem*>(this)->read_impl(address);
        }
        void write(std::uint16_t address, std::uint8_t data) {
            static_cast<ConcreteMem*>(this)->write_impl(address, data);
        }
    };

    class SimpleMem : public Mem<SimpleMem> {
    private:
        std::vector<std::uint8_t> mem;
    public:
        SimpleMem() : mem(1 << 16, 0) {}
        std::uint8_t read_impl(std::uint16_t address) {
            return mem[address];
        }
        void write_impl(std::uint16_t address, std::uint8_t data) {
            mem[address] = data;
        }
    };
}
