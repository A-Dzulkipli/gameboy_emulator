#include <cstdint>
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#define private public
#include "doctest.h"
#include "../src/CPU.h"
#undef private

using namespace gb_emulator;

constexpr std::array<CPU<SimpleMem, NothingClock>::r8, 7> all_r8 = {
    CPU<SimpleMem, NothingClock>::r8::a,
    CPU<SimpleMem, NothingClock>::r8::b,
    CPU<SimpleMem, NothingClock>::r8::c,
    CPU<SimpleMem, NothingClock>::r8::d,
    CPU<SimpleMem, NothingClock>::r8::e,
    CPU<SimpleMem, NothingClock>::r8::h,
    CPU<SimpleMem, NothingClock>::r8::l,
};

constexpr std::array<CPU<SimpleMem, NothingClock>::r16, 4> all_r16 = {
    CPU<SimpleMem, NothingClock>::r16::bc,
    CPU<SimpleMem, NothingClock>::r16::de,
    CPU<SimpleMem, NothingClock>::r16::hl,
    CPU<SimpleMem, NothingClock>::r16::sp
};

class CPUTest {
public:
    SimpleMem mem;
    NothingClock clock;
    CPU<SimpleMem, NothingClock> cpu;
    CPUTest() : cpu(mem, clock) {}
};

// bit set helpers
TEST_CASE("test set single bit") {
    CPUTest fx;
    CHECK(fx.cpu.set_bit(0, 1, true) == 2);
    CHECK(fx.cpu.set_bit(1, 1, true) == 3);
    CHECK(fx.cpu.set_bit(3, 1, false) == 1);
}

TEST_CASE("test set flag") {
    CPUTest fx;
    CHECK(fx.cpu.set_flag(true, true, true, true) == 255 - 15);
    CHECK(fx.cpu.set_flag(true, false, true, false) == 255 - 15 - 64 - 16);
}

TEST_CASE("set_bit sets and clears individual bits") {
    CPUTest fx;
    // set from zero
    CHECK(fx.cpu.set_bit(0x00, 0, true) == 0x01);
    CHECK(fx.cpu.set_bit(0x00, 7, true) == 0x80);   // high bit
    CHECK(fx.cpu.set_bit(0x00, 4, true) == 0x10);
    // clear from all-set
    CHECK(fx.cpu.set_bit(0xFF, 0, false) == 0xFE);
    CHECK(fx.cpu.set_bit(0xFF, 7, false) == 0x7F);   // high bit
}

TEST_CASE("set_bit is idempotent and leaves other bits untouched") {
    CPUTest fx;
    // setting an already-set bit changes nothing
    CHECK(fx.cpu.set_bit(0x01, 0, true) == 0x01);
    // clearing an already-clear bit changes nothing
    CHECK(fx.cpu.set_bit(0x00, 3, false) == 0x00);
    // setting one bit preserves the rest
    CHECK(fx.cpu.set_bit(0b10100000, 1, true) == 0b10100010);
    // clearing one bit preserves the rest
    CHECK(fx.cpu.set_bit(0b10100010, 1, false) == 0b10100000);
}

TEST_CASE("set_bit across all 8 positions from zero") {
    CPUTest fx;
    for (int bit = 0; bit < 8; ++bit) {
        CAPTURE(bit);
        CHECK(fx.cpu.set_bit(0x00, bit, true) == (1 << bit));
    }
}

TEST_CASE("individual flag setters target correct bits") {
    CPUTest fx;
    CHECK(fx.cpu.set_z(0x00, true) == 0x80);
    CHECK(fx.cpu.set_n(0x00, true) == 0x40);
    CHECK(fx.cpu.set_h(0x00, true) == 0x20);
    CHECK(fx.cpu.set_c(0x00, true) == 0x10);
    // clearing
    CHECK(fx.cpu.set_z(0xFF, false) == 0x7F);
    CHECK(fx.cpu.set_c(0xFF, false) == 0xEF);
}

TEST_CASE("individual flag setters preserve other flags") {
    CPUTest fx;
    // set Z on a byte with N already set — both should be present
    CHECK(fx.cpu.set_z(0x40, true) == 0xC0);   // N(0x40) + Z(0x80)
    // clear H from all-flags — others survive
    CHECK(fx.cpu.set_h(0xF0, false) == 0xD0);  // 0xF0 minus 0x20
}

TEST_CASE("set_flag composes all four flags") {
    CPUTest fx;
    CHECK(fx.cpu.set_flag(true,  true,  true,  true)  == 0xF0);
    CHECK(fx.cpu.set_flag(false, false, false, false) == 0x00);
    CHECK(fx.cpu.set_flag(true,  false, false, false) == 0x80);  // Z only
    CHECK(fx.cpu.set_flag(false, true,  false, false) == 0x40);  // N only
    CHECK(fx.cpu.set_flag(false, false, true,  false) == 0x20);  // H only
    CHECK(fx.cpu.set_flag(false, false, false, true)  == 0x10);  // C only
    CHECK(fx.cpu.set_flag(true,  false, true,  false) == 0xA0);  // Z + H
}

TEST_CASE("set_flag never touches low nibble") {
    CPUTest fx;
    // every combination should leave bits 0-3 clear
    for (int i = 0; i < 16; ++i) {
        CAPTURE(i);
        std::uint8_t result = fx.cpu.set_flag(i & 8, i & 4, i & 2, i & 1);
        CHECK((result & 0x0F) == 0x00);
    }
}

TEST_CASE("LD r8,r8 - load_r8_r8") {
    for (const auto& source : all_r8) {
        for (const auto& dest : all_r8) {
            if (source == dest) continue;
            CAPTURE(static_cast<int>(source));
            CAPTURE(static_cast<int>(dest));
            CPUTest fx;
            fx.cpu.write_r8(source, 1);
            CHECK(fx.cpu.read_r8(source) == 1);
            CHECK(fx.cpu.read_r8(dest) == 0);
            fx.cpu.load_r8_r8(dest, source);
            CHECK(fx.cpu.read_r8(source) == 1);
            CHECK(fx.cpu.read_r8(dest) == 1);
        }
    }
}

TEST_CASE("LD r8,n8 - load_r8_n8") {
    for (const auto& dest : all_r8) {
        CAPTURE(static_cast<int>(dest));
        CPUTest fx;
        CHECK(fx.cpu.read_r8(dest) == 0);
        CHECK(fx.cpu.pc() == 0);
        CHECK(fx.cpu.read8(fx.cpu.pc()) == 0);
        fx.cpu.write8(fx.cpu.pc(), 1);
        CHECK(fx.cpu.pc() == 0);
        CHECK(fx.cpu.read8(fx.cpu.pc()) == 1);
        fx.cpu.load_r8_n8(dest);
        CHECK(fx.cpu.pc() == 1);
        CHECK(fx.cpu.read8(fx.cpu.pc() - 1) == 1);
        CHECK(fx.cpu.read8(fx.cpu.pc()) == 0);
        CHECK(fx.cpu.read_r8(dest) == 1);
    }
}

TEST_CASE("LD r8,n8 - load_r8_n8, val > 255") {
    for (const auto& dest : all_r8) {
        CAPTURE(static_cast<int>(dest));
        CPUTest fx;
        int val = 255;
        CHECK(fx.cpu.read_r8(dest) == 0);
        CHECK(fx.cpu.pc() == 0);
        CHECK(fx.cpu.read8(fx.cpu.pc()) == 0);
        fx.cpu.write8(fx.cpu.pc(), val);
        CHECK(fx.cpu.pc() == 0);
        CHECK(fx.cpu.read8(fx.cpu.pc()) == val);
        fx.cpu.load_r8_n8(dest);
        CHECK(fx.cpu.pc() == 1);
        CHECK(fx.cpu.read8(fx.cpu.pc() - 1) == val);
        CHECK(fx.cpu.read8(fx.cpu.pc()) == 0);
        CHECK(fx.cpu.read_r8(dest) == val);
    }

    for (const auto& dest : all_r8) {
        CAPTURE(static_cast<int>(dest));
        CPUTest fx;
        int val = 256;
        CHECK(fx.cpu.read_r8(dest) == 0);
        CHECK(fx.cpu.pc() == 0);
        CHECK(fx.cpu.read8(fx.cpu.pc()) == 0);
        fx.cpu.write8(fx.cpu.pc(), val);
        CHECK(fx.cpu.pc() == 0);
        CHECK(fx.cpu.read8(fx.cpu.pc()) == 0);
        fx.cpu.load_r8_n8(dest);
        CHECK(fx.cpu.pc() == 1);
        CHECK(fx.cpu.read8(fx.cpu.pc() - 1) == 0);
        CHECK(fx.cpu.read8(fx.cpu.pc()) == 0);
        CHECK(fx.cpu.read_r8(dest) == 0);
    }
}

TEST_CASE("LD r16,n16 load_r16_n16") {
    for (const auto& dest : all_r16) {
        CAPTURE(static_cast<int>(dest));
        CPUTest fx;
        int val = 255;
        CHECK(fx.cpu.read_r16(dest) == 0);
        CHECK(fx.cpu.pc() == 0);
        CHECK(fx.cpu.read8(fx.cpu.pc()) == 0);
        fx.cpu.write16(fx.cpu.pc(), val);
        CHECK(fx.cpu.pc() == 0);
        CHECK(fx.cpu.read8(fx.cpu.pc()) == val);
        CHECK(fx.cpu.read8(fx.cpu.pc()+1) == 0);
        fx.cpu.load_r16_n16(dest);
        CHECK(fx.cpu.pc() == 2);
        CHECK(fx.cpu.read8(fx.cpu.pc() - 1) == 0);
        CHECK(fx.cpu.read8(fx.cpu.pc() - 2) == val);
        CHECK(fx.cpu.read8(fx.cpu.pc()) == 0);
        CHECK(fx.cpu.read_r16(dest) == val);
    }

    for (const auto& dest : all_r16) {
        CAPTURE(static_cast<int>(dest));
        CPUTest fx;
        int val = 256;
        CHECK(fx.cpu.read_r16(dest) == 0);
        CHECK(fx.cpu.pc() == 0);
        CHECK(fx.cpu.read8(fx.cpu.pc()) == 0);
        fx.cpu.write16(fx.cpu.pc(), val);
        CHECK(fx.cpu.pc() == 0);
        CHECK(fx.cpu.read8(fx.cpu.pc()) == 0);
        CHECK(fx.cpu.read8(fx.cpu.pc()+1) == 1);
        fx.cpu.load_r16_n16(dest);
        CHECK(fx.cpu.pc() == 2);
        CHECK(fx.cpu.read8(fx.cpu.pc() - 1) == 1);
        CHECK(fx.cpu.read8(fx.cpu.pc() - 2) == 0);
        CHECK(fx.cpu.read8(fx.cpu.pc()) == 0);
        CHECK(fx.cpu.read_r16(dest) == 256);
    }
}

TEST_CASE("LD [HL],r8 - load_hl_r8") {
    std::vector<std::uint16_t> mems = {
        0,
        123,
        965,
        1092
    };
    for (const auto& source : all_r8) {
        for (const auto address : mems) {
            if (source == CPU<SimpleMem, NothingClock>::r8::h ||source == CPU<SimpleMem, NothingClock>::r8::l) continue;
            CPUTest fx;
            std::uint16_t val_lo = 255;
            CHECK(fx.cpu.read8(address) == 0);
            fx.cpu.write_r8(source, val_lo);
            CHECK(fx.cpu.read_r8(source) == val_lo);
            fx.cpu.hl(address);
            fx.cpu.load_hl_r8(source);
            CHECK(fx.cpu.read8(address) == val_lo);
            CHECK(fx.cpu.hl() == address);
        }
    }

    for (const auto& source : all_r8) {
        for (const auto address : mems) {
            if (source == CPU<SimpleMem, NothingClock>::r8::h ||source == CPU<SimpleMem, NothingClock>::r8::l) continue;
            CPUTest fx;
            std::uint16_t val = 256;
            std::uint16_t val_expected = 0;
            CHECK(fx.cpu.read8(address) == 0);
            fx.cpu.write_r8(source, val);
            CHECK(fx.cpu.read_r8(source) == val_expected);
            fx.cpu.hl(address);
            fx.cpu.load_hl_r8(source);
            CHECK(fx.cpu.read8(address) == val_expected);
            CHECK(fx.cpu.hl() == address);
        }
    }
}

TEST_CASE("LD [HL],n8 - load_hl_n8") {
    std::vector<std::uint16_t> mems = {
        123,
        965,
        1092
    };

    for (const auto address : mems) {
        CAPTURE(static_cast<int>(address));
        CPUTest fx;
        std::uint8_t val = 255;
        CHECK(fx.cpu.pc() == 0);
        fx.cpu.write8(fx.cpu.pc(), val);
        CHECK(fx.cpu.read8(fx.cpu.pc()) == val);
        CHECK(fx.cpu.pc() == 0);
        fx.cpu.hl(address);
        CHECK(fx.cpu.read8(address) == 0);
        CHECK(fx.cpu.hl() == address);
        fx.cpu.load_hl_n8();
        CHECK(fx.cpu.hl() == address);
        CHECK(fx.cpu.read8(address) == val);
    }
}
