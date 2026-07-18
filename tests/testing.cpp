#include <cstdint>
#include <vector>
#include <filesystem>
#include <fstream>
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#define private public
#include "doctest.h"
#include "../src/CPU.h"

#undef private
#include "json.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

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

namespace gb_emulator {
    class CountingClock : public Clock<CountingClock> {
    public:
        int cycles = 0;
        void tick_impl(int c) { cycles += c; }
    };
}

class CPUCycleTest {
public:
    SimpleMem mem;
    gb_emulator::CountingClock clock;
    CPU<SimpleMem, gb_emulator::CountingClock> cpu;
    CPUCycleTest() : cpu(mem, clock) {}
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
        CHECK(fx.cpu.bus_read_no_tick(fx.cpu.pc()) == 0);
        fx.cpu.bus_write_no_tick(fx.cpu.pc(), 1);
        CHECK(fx.cpu.pc() == 0);
        CHECK(fx.cpu.bus_read_no_tick(fx.cpu.pc()) == 1);
        fx.cpu.load_r8_n8(dest);
        CHECK(fx.cpu.pc() == 1);
        CHECK(fx.cpu.bus_read_no_tick(fx.cpu.pc() - 1) == 1);
        CHECK(fx.cpu.bus_read_no_tick(fx.cpu.pc()) == 0);
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
        CHECK(fx.cpu.bus_read_no_tick(fx.cpu.pc()) == 0);
        fx.cpu.bus_write_no_tick(fx.cpu.pc(), val);
        CHECK(fx.cpu.pc() == 0);
        CHECK(fx.cpu.bus_read_no_tick(fx.cpu.pc()) == val);
        fx.cpu.load_r8_n8(dest);
        CHECK(fx.cpu.pc() == 1);
        CHECK(fx.cpu.bus_read_no_tick(fx.cpu.pc() - 1) == val);
        CHECK(fx.cpu.bus_read_no_tick(fx.cpu.pc()) == 0);
        CHECK(fx.cpu.read_r8(dest) == val);
    }

    for (const auto& dest : all_r8) {
        CAPTURE(static_cast<int>(dest));
        CPUTest fx;
        int val = 256;
        CHECK(fx.cpu.read_r8(dest) == 0);
        CHECK(fx.cpu.pc() == 0);
        CHECK(fx.cpu.bus_read_no_tick(fx.cpu.pc()) == 0);
        fx.cpu.bus_write_no_tick(fx.cpu.pc(), val & 0xFF);
        CHECK(fx.cpu.pc() == 0);
        CHECK(fx.cpu.bus_read_no_tick(fx.cpu.pc()) == 0);
        fx.cpu.load_r8_n8(dest);
        CHECK(fx.cpu.pc() == 1);
        CHECK(fx.cpu.bus_read_no_tick(fx.cpu.pc() - 1) == 0);
        CHECK(fx.cpu.bus_read_no_tick(fx.cpu.pc()) == 0);
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
        CHECK(fx.cpu.bus_read_no_tick(fx.cpu.pc()) == 0);
        fx.cpu.bus_write_no_tick(fx.cpu.pc(), val & 0xFF);
        fx.cpu.bus_write_no_tick(fx.cpu.pc() + 1, val >> 8);
        CHECK(fx.cpu.pc() == 0);
        CHECK(fx.cpu.bus_read_no_tick(fx.cpu.pc()) == val);
        CHECK(fx.cpu.bus_read_no_tick(fx.cpu.pc()+1) == 0);
        fx.cpu.load_r16_n16(dest);
        CHECK(fx.cpu.pc() == 2);
        CHECK(fx.cpu.bus_read_no_tick(fx.cpu.pc() - 1) == 0);
        CHECK(fx.cpu.bus_read_no_tick(fx.cpu.pc() - 2) == val);
        CHECK(fx.cpu.bus_read_no_tick(fx.cpu.pc()) == 0);
        CHECK(fx.cpu.read_r16(dest) == val);
    }

    for (const auto& dest : all_r16) {
        CAPTURE(static_cast<int>(dest));
        CPUTest fx;
        int val = 256;
        CHECK(fx.cpu.read_r16(dest) == 0);
        CHECK(fx.cpu.pc() == 0);
        CHECK(fx.cpu.bus_read_no_tick(fx.cpu.pc()) == 0);
        fx.cpu.bus_write_no_tick(fx.cpu.pc(), val & 0xFF);
        fx.cpu.bus_write_no_tick(fx.cpu.pc() + 1, val >> 8);
        CHECK(fx.cpu.pc() == 0);
        CHECK(fx.cpu.bus_read_no_tick(fx.cpu.pc()) == 0);
        CHECK(fx.cpu.bus_read_no_tick(fx.cpu.pc()+1) == 1);
        fx.cpu.load_r16_n16(dest);
        CHECK(fx.cpu.pc() == 2);
        CHECK(fx.cpu.bus_read_no_tick(fx.cpu.pc() - 1) == 1);
        CHECK(fx.cpu.bus_read_no_tick(fx.cpu.pc() - 2) == 0);
        CHECK(fx.cpu.bus_read_no_tick(fx.cpu.pc()) == 0);
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
            CHECK(fx.cpu.bus_read_no_tick(address) == 0);
            fx.cpu.write_r8(source, val_lo);
            CHECK(fx.cpu.read_r8(source) == val_lo);
            fx.cpu.hl(address);
            fx.cpu.load_hl_r8(source);
            CHECK(fx.cpu.bus_read_no_tick(address) == val_lo);
            CHECK(fx.cpu.hl() == address);
        }
    }

    for (const auto& source : all_r8) {
        for (const auto address : mems) {
            if (source == CPU<SimpleMem, NothingClock>::r8::h ||source == CPU<SimpleMem, NothingClock>::r8::l) continue;
            CPUTest fx;
            std::uint16_t val = 256;
            std::uint16_t val_expected = 0;
            CHECK(fx.cpu.bus_read_no_tick(address) == 0);
            fx.cpu.write_r8(source, val);
            CHECK(fx.cpu.read_r8(source) == val_expected);
            fx.cpu.hl(address);
            fx.cpu.load_hl_r8(source);
            CHECK(fx.cpu.bus_read_no_tick(address) == val_expected);
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
        fx.cpu.bus_write_no_tick(fx.cpu.pc(), val);
        CHECK(fx.cpu.bus_read_no_tick(fx.cpu.pc()) == val);
        CHECK(fx.cpu.pc() == 0);
        fx.cpu.hl(address);
        CHECK(fx.cpu.bus_read_no_tick(address) == 0);
        CHECK(fx.cpu.hl() == address);
        fx.cpu.load_hl_n8();
        CHECK(fx.cpu.hl() == address);
        CHECK(fx.cpu.bus_read_no_tick(address) == val);
    }
}

TEST_CASE("LD r8,[HL] - load_r8_hl") {
    std::vector<std::uint16_t> mems = {
        123,
        965,
        1092
    };

    for (const auto& reg : all_r8) {
        for (const auto address : mems) {
            if (reg == CPU<SimpleMem, NothingClock>::r8::h ||reg == CPU<SimpleMem, NothingClock>::r8::l) continue;
            CAPTURE(static_cast<int>(reg));
            CPUTest fx;
            std::uint8_t val = 255;
            // initial state
            CHECK(fx.cpu.bus_read_no_tick(address) == 0);
            CHECK(fx.cpu.read_r8(reg) == 0);
            CHECK(fx.cpu.hl() == 0);
            fx.cpu.bus_write_no_tick(address, val);
            fx.cpu.hl(address);
            // val loaded into address
            // address loaded into hl
            // register still 0
            CHECK(fx.cpu.bus_read_no_tick(address) == val);
            CHECK(fx.cpu.hl() == address);
            CHECK(fx.cpu.read_r8(reg) == 0);
            fx.cpu.load_r8_hl(reg);
            // [address] still val
            // hl still address
            // register now val
            CHECK(fx.cpu.bus_read_no_tick(address) == val);
            CHECK(fx.cpu.hl() == address);
            CHECK(fx.cpu.read_r8(reg) == val);
            fx.cpu.load_r8_hl(reg);
            // idempotence
            CHECK(fx.cpu.bus_read_no_tick(address) == val);
            CHECK(fx.cpu.hl() == address);
            CHECK(fx.cpu.read_r8(reg) == val);
        }
    }
}

TEST_CASE("LD [r16],A - load_r16_mem_a") {
    std::vector<std::uint16_t> mems = {
        123,
        965,
        1092
    };

    for (const auto& reg : all_r16) {
        for (const auto address : mems) {
            CPUTest fx;
            std::uint8_t val = 255;
            fx.cpu.a(val);
            fx.cpu.write_r16(reg, address);
            CHECK(fx.cpu.bus_read_no_tick(address) == 0);
            fx.cpu.load_r16_mem_a(reg);
            CHECK(fx.cpu.bus_read_no_tick(address) == val);
            CHECK(fx.cpu.a() == val);
            CHECK(fx.cpu.read_r16(reg) == address);
        }
    }
}

TEST_CASE("LD [n16],A - load_n16_mem_a") {
    std::vector<std::uint16_t> mems = {
        123,
        965,
        1092
    };

    for (const auto address : mems) {
        CPUTest fx;
        std::uint8_t val = 255;
        fx.cpu.bus_write_no_tick(fx.cpu.pc(), address & 0xFF);
        fx.cpu.bus_write_no_tick(fx.cpu.pc() + 1, address >> 8);
        fx.cpu.a(val);
        CHECK(fx.cpu.bus_read_no_tick(address) == 0);
        fx.cpu.load_n16_mem_a();
        CHECK(fx.cpu.bus_read_no_tick(address) == val);
    }
}

TEST_CASE("LDH [n16],A - loadh_n16_mem_a") {
    std::vector<std::uint8_t> mems = {
        123 % 256,
        965 % 256,
        1092 % 256
    };

    for (const auto address : mems) {
        CPUTest fx;
        std::uint8_t val = 255;
        fx.cpu.bus_write_no_tick(fx.cpu.pc(), address);
        fx.cpu.a(val);
        CHECK(fx.cpu.bus_read_no_tick(static_cast<std::uint16_t>(address) | 0xFF00) == 0);
        fx.cpu.loadh_n16_mem_a();
        CHECK(fx.cpu.bus_read_no_tick(static_cast<std::uint16_t>(address) | 0xFF00) == val);
    }
}

TEST_CASE("LDH [C],A - loadh_c_mem_a") {
    std::vector<std::uint8_t> mems = {
        123 % 256,
        965 % 256,
        1092 % 256
    };

    for (const auto address : mems) {
        CPUTest fx;
        std::uint8_t val = 255;
        fx.cpu.c(address);
        fx.cpu.a(val);
        CHECK(fx.cpu.bus_read_no_tick(static_cast<std::uint16_t>(address) | 0xFF00) == 0);
        fx.cpu.loadh_c_mem_a();
        CHECK(fx.cpu.bus_read_no_tick(static_cast<std::uint16_t>(address) | 0xFF00) == val);
    }
}

TEST_CASE("LD A,[r16] - load_a_r16_mem") {
    std::vector<std::uint16_t> mems = {
        123,
        965,
        1092
    };

    for (const auto& reg :all_r16) {
        for (const auto address : mems) {
            CAPTURE(static_cast<int>(reg));
            CPUTest fx;
            std::uint8_t val = 255;
            fx.cpu.write_r16(reg, address);
            fx.cpu.bus_write_no_tick(address, val);
            CHECK(fx.cpu.a() == 0);
            fx.cpu.load_a_r16_mem(reg);
            CHECK(fx.cpu.a() == val);
        }
    }
}

TEST_CASE("LD A,[n16] - load_a_n16_mem") {
    std::vector<std::uint16_t> mems = {
        123,
        965,
        1092
    };

    for (const auto address : mems) {
        CPUTest fx;
        std::uint8_t val = 255;
        fx.cpu.bus_write_no_tick(fx.cpu.pc(), address & 0xFF);
        fx.cpu.bus_write_no_tick(fx.cpu.pc() + 1, address >> 8);
        fx.cpu.bus_write_no_tick(address, val);
        CHECK(fx.cpu.a() == 0);
        fx.cpu.load_a_n16_mem();
        CHECK(fx.cpu.a() == val);
    }
}

TEST_CASE("LDH A,[n16] - loadh_a_n16_mem") {
    std::vector<std::uint8_t> mems = {
        123 % 256,
        965 % 256,
        1092 % 256
    };

    for (const auto address : mems) {
        CPUTest fx;
        std::uint8_t val = 255;
        fx.cpu.bus_write_no_tick(fx.cpu.pc(), address);
        fx.cpu.bus_write_no_tick(static_cast<std::uint16_t>(address) | 0xFF00, val);
        CHECK(fx.cpu.a() == 0);
        fx.cpu.loadh_a_n16_mem();
        CHECK(fx.cpu.a() == val);
    }
}

// ============ ALU: ADD / ADC ============

TEST_CASE("ADD A,r8 - flags and result") {
    CPUTest fx;
    SUBCASE("basic add, no flags") {
        fx.cpu.a(0x10); fx.cpu.b(0x11);
        fx.cpu.add_a_r8(CPU<SimpleMem,NothingClock>::r8::b);
        CHECK(fx.cpu.a() == 0x21);
        CHECK(fx.cpu.flag_z() == false);
        CHECK(fx.cpu.flag_n() == false);
        CHECK(fx.cpu.flag_h() == false);
        CHECK(fx.cpu.flag_c() == false);
    }
    SUBCASE("zero result sets Z and C via overflow") {
        fx.cpu.a(0x01); fx.cpu.b(0xFF);
        fx.cpu.add_a_r8(CPU<SimpleMem,NothingClock>::r8::b);
        CHECK(fx.cpu.a() == 0x00);
        CHECK(fx.cpu.flag_z() == true);
        CHECK(fx.cpu.flag_h() == true);   // 0x1+0xF nibble carry
        CHECK(fx.cpu.flag_c() == true);
    }
    SUBCASE("half-carry only") {
        fx.cpu.a(0x0F); fx.cpu.b(0x01);
        fx.cpu.add_a_r8(CPU<SimpleMem,NothingClock>::r8::b);
        CHECK(fx.cpu.a() == 0x10);
        CHECK(fx.cpu.flag_h() == true);
        CHECK(fx.cpu.flag_c() == false);
        CHECK(fx.cpu.flag_z() == false);
    }
    SUBCASE("N always cleared for ADD") {
        fx.cpu.f(0x40);  // N pre-set
        fx.cpu.a(0x01); fx.cpu.b(0x01);
        fx.cpu.add_a_r8(CPU<SimpleMem,NothingClock>::r8::b);
        CHECK(fx.cpu.flag_n() == false);
    }
}

TEST_CASE("ADC A,r8 - carry-in participates") {
    CPUTest fx;
    SUBCASE("carry-in adds one") {
        fx.cpu.a(0x10); fx.cpu.b(0x10); fx.cpu.f(0x10);  // C set
        fx.cpu.adc_a_r8(CPU<SimpleMem,NothingClock>::r8::b);
        CHECK(fx.cpu.a() == 0x21);   // 0x10+0x10+1
    }
    SUBCASE("carry-in triggers half-carry at nibble boundary") {
        fx.cpu.a(0x0F); fx.cpu.b(0x00); fx.cpu.f(0x10);  // C set
        fx.cpu.adc_a_r8(CPU<SimpleMem,NothingClock>::r8::b);
        CHECK(fx.cpu.a() == 0x10);
        CHECK(fx.cpu.flag_h() == true);   // 0xF+0+1 carries nibble
    }
    SUBCASE("carry-in triggers full carry and zero") {
        fx.cpu.a(0xFF); fx.cpu.b(0x00); fx.cpu.f(0x10);  // C set
        fx.cpu.adc_a_r8(CPU<SimpleMem,NothingClock>::r8::b);
        CHECK(fx.cpu.a() == 0x00);   // 0xFF+0+1 wraps
        CHECK(fx.cpu.flag_z() == true);
        CHECK(fx.cpu.flag_c() == true);
        CHECK(fx.cpu.flag_h() == true);
    }
}

// ============ ALU: SUB / SBC / CP ============

TEST_CASE("SUB A,r8 - flags and result") {
    CPUTest fx;
    SUBCASE("basic subtract, N set") {
        fx.cpu.a(0x20); fx.cpu.b(0x10);
        fx.cpu.sub_a_r8(CPU<SimpleMem,NothingClock>::r8::b);
        CHECK(fx.cpu.a() == 0x10);
        CHECK(fx.cpu.flag_n() == true);
        CHECK(fx.cpu.flag_z() == false);
        CHECK(fx.cpu.flag_h() == false);
        CHECK(fx.cpu.flag_c() == false);
    }
    SUBCASE("equal operands set Z") {
        fx.cpu.a(0x42); fx.cpu.b(0x42);
        fx.cpu.sub_a_r8(CPU<SimpleMem,NothingClock>::r8::b);
        CHECK(fx.cpu.a() == 0x00);
        CHECK(fx.cpu.flag_z() == true);
    }
    SUBCASE("borrow sets C and H") {
        fx.cpu.a(0x10); fx.cpu.b(0x20);
        fx.cpu.sub_a_r8(CPU<SimpleMem,NothingClock>::r8::b);
        CHECK(fx.cpu.a() == 0xF0);   // 0x10-0x20 wraps
        CHECK(fx.cpu.flag_c() == true);
    }
    SUBCASE("half-borrow only") {
        fx.cpu.a(0x10); fx.cpu.b(0x01);
        fx.cpu.sub_a_r8(CPU<SimpleMem,NothingClock>::r8::b);
        CHECK(fx.cpu.a() == 0x0F);
        CHECK(fx.cpu.flag_h() == true);   // low nibble 0 < 1
        CHECK(fx.cpu.flag_c() == false);
    }
}

TEST_CASE("SBC A,r8 - carry-in participates") {
    CPUTest fx;
    SUBCASE("carry-in subtracts one") {
        fx.cpu.a(0x20); fx.cpu.b(0x10); fx.cpu.f(0x10);  // C set
        fx.cpu.sbc_a_r8(CPU<SimpleMem,NothingClock>::r8::b);
        CHECK(fx.cpu.a() == 0x0F);   // 0x20-0x10-1
        CHECK(fx.cpu.flag_n() == true);
    }
    SUBCASE("carry-in causes half-borrow") {
        fx.cpu.a(0x10); fx.cpu.b(0x00); fx.cpu.f(0x10);  // C set
        fx.cpu.sbc_a_r8(CPU<SimpleMem,NothingClock>::r8::b);
        CHECK(fx.cpu.a() == 0x0F);   // 0x10-0-1
        CHECK(fx.cpu.flag_h() == true);   // low nibble 0 < 0+1
    }
    SUBCASE("carry-in causes full borrow to zero") {
        fx.cpu.a(0x00); fx.cpu.b(0xFF); fx.cpu.f(0x10);  // C set
        fx.cpu.sbc_a_r8(CPU<SimpleMem,NothingClock>::r8::b);
        CHECK(fx.cpu.a() == 0x00);   // 0-0xFF-1 = -0x100 wraps to 0
        CHECK(fx.cpu.flag_z() == true);
        CHECK(fx.cpu.flag_c() == true);
    }
}

TEST_CASE("CP A,r8 - compare sets flags, preserves A") {
    CPUTest fx;
    SUBCASE("equal sets Z, A unchanged") {
        fx.cpu.a(0x42); fx.cpu.b(0x42);
        fx.cpu.cp_a_r8(CPU<SimpleMem,NothingClock>::r8::b);
        CHECK(fx.cpu.a() == 0x42);   // A NOT modified
        CHECK(fx.cpu.flag_z() == true);
        CHECK(fx.cpu.flag_n() == true);
    }
    SUBCASE("A less than operand sets C") {
        fx.cpu.a(0x10); fx.cpu.b(0x20);
        fx.cpu.cp_a_r8(CPU<SimpleMem,NothingClock>::r8::b);
        CHECK(fx.cpu.a() == 0x10);   // preserved
        CHECK(fx.cpu.flag_c() == true);
        CHECK(fx.cpu.flag_z() == false);
    }
}

// ============ INC / DEC r8 ============

TEST_CASE("INC r8 - flags, C preserved") {
    CPUTest fx;
    SUBCASE("basic increment") {
        fx.cpu.b(0x0F);
        fx.cpu.inc_r8(CPU<SimpleMem,NothingClock>::r8::b);
        CHECK(fx.cpu.b() == 0x10);
        CHECK(fx.cpu.flag_h() == true);   // 0xF+1 nibble carry
        CHECK(fx.cpu.flag_z() == false);
        CHECK(fx.cpu.flag_n() == false);
    }
    SUBCASE("wrap to zero sets Z") {
        fx.cpu.b(0xFF);
        fx.cpu.inc_r8(CPU<SimpleMem,NothingClock>::r8::b);
        CHECK(fx.cpu.b() == 0x00);
        CHECK(fx.cpu.flag_z() == true);
        CHECK(fx.cpu.flag_h() == true);
    }
    SUBCASE("C flag preserved") {
        fx.cpu.f(0x10);  // C set
        fx.cpu.b(0x01);
        fx.cpu.inc_r8(CPU<SimpleMem,NothingClock>::r8::b);
        CHECK(fx.cpu.flag_c() == true);   // untouched by INC
    }
}

TEST_CASE("DEC r8 - flags, C preserved") {
    CPUTest fx;
    SUBCASE("basic decrement") {
        fx.cpu.b(0x10);
        fx.cpu.dec_r8(CPU<SimpleMem,NothingClock>::r8::b);
        CHECK(fx.cpu.b() == 0x0F);
        CHECK(fx.cpu.flag_h() == true);   // borrow from bit 4
        CHECK(fx.cpu.flag_n() == true);
    }
    SUBCASE("decrement to zero sets Z") {
        fx.cpu.b(0x01);
        fx.cpu.dec_r8(CPU<SimpleMem,NothingClock>::r8::b);
        CHECK(fx.cpu.b() == 0x00);
        CHECK(fx.cpu.flag_z() == true);
    }
    SUBCASE("wrap from zero") {
        fx.cpu.b(0x00);
        fx.cpu.dec_r8(CPU<SimpleMem,NothingClock>::r8::b);
        CHECK(fx.cpu.b() == 0xFF);
        CHECK(fx.cpu.flag_h() == true);   // low nibble 0 borrows
        CHECK(fx.cpu.flag_z() == false);
    }
    SUBCASE("C preserved") {
        fx.cpu.f(0x10);
        fx.cpu.b(0x05);
        fx.cpu.dec_r8(CPU<SimpleMem,NothingClock>::r8::b);
        CHECK(fx.cpu.flag_c() == true);
    }
}

// ============ Logical: AND / OR / XOR ============

TEST_CASE("AND A,r8 - H set, C/N cleared") {
    CPUTest fx;
    fx.cpu.a(0xF0); fx.cpu.b(0x0F);
    fx.cpu.and_a_r8(CPU<SimpleMem,NothingClock>::r8::b);
    CHECK(fx.cpu.a() == 0x00);
    CHECK(fx.cpu.flag_z() == true);
    CHECK(fx.cpu.flag_h() == true);    // AND uniquely sets H
    CHECK(fx.cpu.flag_n() == false);
    CHECK(fx.cpu.flag_c() == false);
}

TEST_CASE("OR A,r8 - all flags but Z cleared") {
    CPUTest fx;
    SUBCASE("nonzero result") {
        fx.cpu.a(0xF0); fx.cpu.b(0x0F);
        fx.cpu.or_a_r8(CPU<SimpleMem,NothingClock>::r8::b);
        CHECK(fx.cpu.a() == 0xFF);
        CHECK(fx.cpu.flag_z() == false);
        CHECK(fx.cpu.flag_h() == false);
    }
    SUBCASE("zero result sets Z") {
        fx.cpu.a(0x00); fx.cpu.b(0x00);
        fx.cpu.or_a_r8(CPU<SimpleMem,NothingClock>::r8::b);
        CHECK(fx.cpu.flag_z() == true);
    }
}

TEST_CASE("XOR A,r8") {
    CPUTest fx;
    SUBCASE("self-xor zeroes and sets Z") {
        fx.cpu.a(0xFF); fx.cpu.b(0xFF);
        fx.cpu.xor_a_r8(CPU<SimpleMem,NothingClock>::r8::b);
        CHECK(fx.cpu.a() == 0x00);
        CHECK(fx.cpu.flag_z() == true);
    }
    SUBCASE("distinct bits") {
        fx.cpu.a(0xF0); fx.cpu.b(0x0F);
        fx.cpu.xor_a_r8(CPU<SimpleMem,NothingClock>::r8::b);
        CHECK(fx.cpu.a() == 0xFF);
        CHECK(fx.cpu.flag_z() == false);
    }
}

// ============ CPL ============

TEST_CASE("CPL - complement A, N and H set, Z/C preserved") {
    CPUTest fx;
    fx.cpu.a(0b10100101);
    fx.cpu.f(0x90);   // Z and C set (0x80 | 0x10)
    fx.cpu.cpl();
    CHECK(fx.cpu.a() == 0b01011010);
    CHECK(fx.cpu.flag_n() == true);
    CHECK(fx.cpu.flag_h() == true);
    CHECK(fx.cpu.flag_z() == true);   // preserved
    CHECK(fx.cpu.flag_c() == true);   // preserved
}

// ============ 16-bit arithmetic ============

TEST_CASE("ADD HL,r16 - Z preserved, bit-11 half-carry") {
    CPUTest fx;
    SUBCASE("basic add") {
        fx.cpu.hl(0x0100); fx.cpu.bc(0x0200);
        fx.cpu.add_hl_r16(CPU<SimpleMem,NothingClock>::r16::bc);
        CHECK(fx.cpu.hl() == 0x0300);
        CHECK(fx.cpu.flag_n() == false);
    }
    SUBCASE("bit-11 half-carry") {
        fx.cpu.hl(0x0FFF); fx.cpu.bc(0x0001);
        fx.cpu.add_hl_r16(CPU<SimpleMem,NothingClock>::r16::bc);
        CHECK(fx.cpu.hl() == 0x1000);
        CHECK(fx.cpu.flag_h() == true);   // carry out of bit 11
    }
    SUBCASE("bit-15 carry") {
        fx.cpu.hl(0xFFFF); fx.cpu.bc(0x0001);
        fx.cpu.add_hl_r16(CPU<SimpleMem,NothingClock>::r16::bc);
        CHECK(fx.cpu.hl() == 0x0000);
        CHECK(fx.cpu.flag_c() == true);
    }
    SUBCASE("Z is preserved, not computed") {
        fx.cpu.f(0x80);  // Z set
        fx.cpu.hl(0x0000); fx.cpu.bc(0x0000);
        fx.cpu.add_hl_r16(CPU<SimpleMem,NothingClock>::r16::bc);
        CHECK(fx.cpu.flag_z() == true);   // preserved despite 0 result
    }
}

TEST_CASE("INC r16 / DEC r16 - no flags affected") {
    CPUTest fx;
    fx.cpu.f(0xF0);  // all flags set
    fx.cpu.bc(0x00FF);
    fx.cpu.inc_r16(CPU<SimpleMem,NothingClock>::r16::bc);
    CHECK(fx.cpu.bc() == 0x0100);
    CHECK(fx.cpu.f() == 0xF0);   // flags untouched
    fx.cpu.dec_r16(CPU<SimpleMem,NothingClock>::r16::bc);
    CHECK(fx.cpu.bc() == 0x00FF);
    CHECK(fx.cpu.f() == 0xF0);
}

// ============ SP-relative (the flag-trap instructions) ============

TEST_CASE("ADD SP,e8 - flags from unsigned low byte, Z=N=0") {
    CPUTest fx;
    SUBCASE("positive offset, half-carry") {
        fx.cpu.sp(0x000F);
        fx.cpu.bus_write_no_tick(fx.cpu.pc(), 0x01);  // e8 = +1
        fx.cpu.add_sp_e8();
        CHECK(fx.cpu.sp() == 0x0010);
        CHECK(fx.cpu.flag_h() == true);    // low nibble 0xF+1
        CHECK(fx.cpu.flag_z() == false);
        CHECK(fx.cpu.flag_n() == false);
    }
    SUBCASE("carry from low byte") {
        fx.cpu.sp(0x00FF);
        fx.cpu.bus_write_no_tick(fx.cpu.pc(), 0x01);
        fx.cpu.add_sp_e8();
        CHECK(fx.cpu.sp() == 0x0100);
        CHECK(fx.cpu.flag_c() == true);
        CHECK(fx.cpu.flag_h() == true);
    }
    SUBCASE("negative offset subtracts from SP") {
        fx.cpu.sp(0x0100);
        fx.cpu.bus_write_no_tick(fx.cpu.pc(), 0xFF);  // e8 = -1
        fx.cpu.add_sp_e8();
        CHECK(fx.cpu.sp() == 0x00FF);
        // flags computed from unsigned 0xFF + low byte 0x00
        CHECK(fx.cpu.flag_z() == false);
    }
}

TEST_CASE("LD HL,SP+e8 - same flags, writes HL, SP unchanged") {
    CPUTest fx;
    fx.cpu.sp(0x000F);
    fx.cpu.bus_write_no_tick(fx.cpu.pc(), 0x01);
    fx.cpu.load_hl_sp_e8();
    CHECK(fx.cpu.hl() == 0x0010);
    CHECK(fx.cpu.sp() == 0x000F);      // SP untouched
    CHECK(fx.cpu.flag_h() == true);
    CHECK(fx.cpu.flag_z() == false);
}

// ============ LD SP,HL / LD [n16],SP ============

TEST_CASE("LD SP,HL") {
    CPUTest fx;
    fx.cpu.hl(0xBEEF);
    fx.cpu.load_sp_hl();
    // NOTE: asserting intended semantics — SP should become HL
    CHECK(fx.cpu.sp() == 0xBEEF);
    CHECK(fx.cpu.hl() == 0xBEEF);
}

TEST_CASE("LD [n16],SP - stores SP little-endian") {
    CPUTest fx;
    fx.cpu.sp(0x1234);
    fx.cpu.bus_write_no_tick(fx.cpu.pc(), 0x00);
    fx.cpu.bus_write_no_tick(fx.cpu.pc() + 1, 0xC0);  // address operand 0xC000
    fx.cpu.load_n16_mem_sp();
    CHECK(fx.cpu.bus_read_no_tick(0xC000) == 0x34);  // low
    CHECK(fx.cpu.bus_read_no_tick(0xC001) == 0x12);  // high
}

// ============ Stack: PUSH / POP ============

TEST_CASE("PUSH / POP r16 round-trip") {
    CPUTest fx;
    for (auto reg : all_r16) {
        if (reg == CPU<SimpleMem,NothingClock>::r16::sp) continue; // PUSH SP not a thing
        CAPTURE(static_cast<int>(reg));
        CPUTest f;
        f.cpu.sp(0xFFFE);
        f.cpu.write_r16(reg, 0xBEEF);
        f.cpu.push_r16(reg);
        CHECK(f.cpu.sp() == 0xFFFC);       // SP decremented by 2
        f.cpu.write_r16(reg, 0x0000);      // clobber
        f.cpu.pop_r16(reg);
        CHECK(f.cpu.read_r16(reg) == 0xBEEF);
        CHECK(f.cpu.sp() == 0xFFFE);       // SP restored
    }
}

TEST_CASE("PUSH stack byte order - high at higher address") {
    CPUTest fx;
    fx.cpu.sp(0xFFFE);
    fx.cpu.bc(0x1234);
    fx.cpu.push_r16(CPU<SimpleMem,NothingClock>::r16::bc);
    // after push: SP=0xFFFC, low byte at SP, high byte at SP+1
    CHECK(fx.cpu.bus_read_no_tick(0xFFFC) == 0x34);  // low
    CHECK(fx.cpu.bus_read_no_tick(0xFFFD) == 0x12);  // high
}

TEST_CASE("PUSH/POP AF - F low nibble masked") {
    CPUTest fx;
    fx.cpu.sp(0xFFFE);
    fx.cpu.a(0x12);
    fx.cpu.f(0xF0);
    fx.cpu.push_af();
    fx.cpu.a(0x00); fx.cpu.f(0x00);
    fx.cpu.pop_af();
    CHECK(fx.cpu.a() == 0x12);
    CHECK(fx.cpu.f() == 0xF0);
}

TEST_CASE("POP AF cannot set F low nibble") {
    CPUTest fx;
    fx.cpu.sp(0xFFFC);
    fx.cpu.bus_write_no_tick(0xFFFC, 0xFF);  // low byte (F) = 0xFF
    fx.cpu.bus_write_no_tick(0xFFFD, 0x12);  // high byte (A)
    fx.cpu.pop_af();
    CHECK(fx.cpu.a() == 0x12);
    CHECK(fx.cpu.f() == 0xF0);    // low nibble of F forced zero
}

// ============ Rotates (register variants, CB-prefixed: set Z by result) ============

TEST_CASE("RLC r8 - rotate left circular") {
    CPUTest fx;
    SUBCASE("bit 7 wraps to bit 0 and carry") {
        fx.cpu.b(0x80);
        fx.cpu.rlc_r8(CPU<SimpleMem,NothingClock>::r8::b);
        CHECK(fx.cpu.b() == 0x01);        // 0x80 rotated
        CHECK(fx.cpu.flag_c() == true);
        CHECK(fx.cpu.flag_z() == false);
    }
    SUBCASE("zero stays zero, Z set") {
        fx.cpu.b(0x00);
        fx.cpu.rlc_r8(CPU<SimpleMem,NothingClock>::r8::b);
        CHECK(fx.cpu.b() == 0x00);
        CHECK(fx.cpu.flag_z() == true);
        CHECK(fx.cpu.flag_c() == false);
    }
}

TEST_CASE("RRC r8 - rotate right circular") {
    CPUTest fx;
    fx.cpu.b(0x01);
    fx.cpu.rrc_r8(CPU<SimpleMem,NothingClock>::r8::b);
    CHECK(fx.cpu.b() == 0x80);           // bit 0 wraps to bit 7
    CHECK(fx.cpu.flag_c() == true);
}

TEST_CASE("RL r8 - rotate left through carry") {
    CPUTest fx;
    SUBCASE("carry-in fills bit 0") {
        fx.cpu.b(0x00);
        fx.cpu.f(0x10);   // C set
        fx.cpu.rl_r8(CPU<SimpleMem,NothingClock>::r8::b);
        CHECK(fx.cpu.b() == 0x01);        // carry rotated into bit 0
        CHECK(fx.cpu.flag_c() == false);  // old bit 7 was 0
    }
    SUBCASE("bit 7 goes to carry") {
        fx.cpu.b(0x80);
        fx.cpu.f(0x00);   // C clear
        fx.cpu.rl_r8(CPU<SimpleMem,NothingClock>::r8::b);
        CHECK(fx.cpu.b() == 0x00);
        CHECK(fx.cpu.flag_c() == true);
        CHECK(fx.cpu.flag_z() == true);
    }
}

TEST_CASE("RR r8 - rotate right through carry") {
    CPUTest fx;
    SUBCASE("carry-in fills bit 7") {
        fx.cpu.b(0x00);
        fx.cpu.f(0x10);   // C set
        fx.cpu.rr_r8(CPU<SimpleMem,NothingClock>::r8::b);
        CHECK(fx.cpu.b() == 0x80);        // carry into bit 7
        CHECK(fx.cpu.flag_c() == false);
    }
    SUBCASE("bit 0 goes to carry") {
        fx.cpu.b(0x01);
        fx.cpu.f(0x00);
        fx.cpu.rr_r8(CPU<SimpleMem,NothingClock>::r8::b);
        CHECK(fx.cpu.b() == 0x00);
        CHECK(fx.cpu.flag_c() == true);
        CHECK(fx.cpu.flag_z() == true);
    }
}

// ============ Accumulator rotates (RLCA/RRCA/RLA/RRA: Z always cleared) ============

TEST_CASE("RLCA - like RLC but Z always cleared") {
    CPUTest fx;
    fx.cpu.a(0x00);
    fx.cpu.rlca();
    CHECK(fx.cpu.a() == 0x00);
    CHECK(fx.cpu.flag_z() == false);   // NOT set despite zero result
}

TEST_CASE("RRCA - Z always cleared") {
    CPUTest fx;
    fx.cpu.a(0x00);
    fx.cpu.rrca();
    CHECK(fx.cpu.flag_z() == false);
}

TEST_CASE("RLA - through carry, Z always cleared") {
    CPUTest fx;
    fx.cpu.a(0x80);
    fx.cpu.f(0x00);
    fx.cpu.rla();
    CHECK(fx.cpu.a() == 0x00);
    CHECK(fx.cpu.flag_c() == true);
    CHECK(fx.cpu.flag_z() == false);   // cleared even though result 0
}

TEST_CASE("RRA - through carry, Z always cleared") {
    CPUTest fx;
    fx.cpu.a(0x01);
    fx.cpu.f(0x00);
    fx.cpu.rra();
    CHECK(fx.cpu.a() == 0x00);
    CHECK(fx.cpu.flag_c() == true);
    CHECK(fx.cpu.flag_z() == false);
}

// ============ Shifts ============

TEST_CASE("SLA r8 - shift left, zero into bit 0") {
    CPUTest fx;
    fx.cpu.b(0x80);
    fx.cpu.sla_r8(CPU<SimpleMem,NothingClock>::r8::b);
    CHECK(fx.cpu.b() == 0x00);
    CHECK(fx.cpu.flag_c() == true);
    CHECK(fx.cpu.flag_z() == true);
}

TEST_CASE("SRA r8 - shift right, sign preserved") {
    CPUTest fx;
    SUBCASE("bit 7 preserved") {
        fx.cpu.b(0x80);
        fx.cpu.sra_r8(CPU<SimpleMem,NothingClock>::r8::b);
        CHECK(fx.cpu.b() == 0xC0);        // sign bit stays
        CHECK(fx.cpu.flag_c() == false);
    }
    SUBCASE("bit 0 to carry") {
        fx.cpu.b(0x01);
        fx.cpu.sra_r8(CPU<SimpleMem,NothingClock>::r8::b);
        CHECK(fx.cpu.b() == 0x00);
        CHECK(fx.cpu.flag_c() == true);
        CHECK(fx.cpu.flag_z() == true);
    }
}

TEST_CASE("SRL r8 - shift right, zero into bit 7") {
    CPUTest fx;
    fx.cpu.b(0x80);
    fx.cpu.srl_r8(CPU<SimpleMem,NothingClock>::r8::b);
    CHECK(fx.cpu.b() == 0x40);           // zero-filled top
    CHECK(fx.cpu.flag_c() == false);
    fx.cpu.b(0x01);
    fx.cpu.srl_r8(CPU<SimpleMem,NothingClock>::r8::b);
    CHECK(fx.cpu.b() == 0x00);
    CHECK(fx.cpu.flag_c() == true);
    CHECK(fx.cpu.flag_z() == true);
}

TEST_CASE("SWAP r8 - nibble swap, only Z flag") {
    CPUTest fx;
    SUBCASE("swaps nibbles") {
        fx.cpu.b(0x12);
        fx.cpu.swap_r8(CPU<SimpleMem,NothingClock>::r8::b);
        CHECK(fx.cpu.b() == 0x21);
        CHECK(fx.cpu.flag_z() == false);
        CHECK(fx.cpu.flag_c() == false);
    }
    SUBCASE("zero sets Z") {
        fx.cpu.b(0x00);
        fx.cpu.swap_r8(CPU<SimpleMem,NothingClock>::r8::b);
        CHECK(fx.cpu.flag_z() == true);
    }
}

// ============ BIT / RES / SET ============

TEST_CASE("BIT u3,r8 - Z = inverted bit, H set, C preserved") {
    CPUTest fx;
    SUBCASE("set bit gives Z=0") {
        fx.cpu.b(0x80);
        fx.cpu.f(0x10);  // C set, to check preservation
        fx.cpu.bit_u3_r8(7, CPU<SimpleMem,NothingClock>::r8::b);
        CHECK(fx.cpu.flag_z() == false);   // bit 7 is set
        CHECK(fx.cpu.flag_h() == true);
        CHECK(fx.cpu.flag_n() == false);
        CHECK(fx.cpu.flag_c() == true);    // preserved
    }
    SUBCASE("clear bit gives Z=1") {
        fx.cpu.b(0x00);
        fx.cpu.bit_u3_r8(3, CPU<SimpleMem,NothingClock>::r8::b);
        CHECK(fx.cpu.flag_z() == true);    // bit 3 is clear
    }
    SUBCASE("BIT does not modify the register") {
        fx.cpu.b(0xAB);
        fx.cpu.bit_u3_r8(0, CPU<SimpleMem,NothingClock>::r8::b);
        CHECK(fx.cpu.b() == 0xAB);
    }
}

TEST_CASE("RES u3,r8 - clears bit, no flags") {
    CPUTest fx;
    fx.cpu.f(0xF0);  // all flags
    fx.cpu.b(0xFF);
    fx.cpu.res_u3_r8(3, CPU<SimpleMem,NothingClock>::r8::b);
    CHECK(fx.cpu.b() == 0xF7);           // bit 3 cleared
    CHECK(fx.cpu.f() == 0xF0);           // flags untouched
}

TEST_CASE("SET u3,r8 - sets bit, no flags") {
    CPUTest fx;
    fx.cpu.f(0x00);
    fx.cpu.b(0x00);
    fx.cpu.set_u3_r8(3, CPU<SimpleMem,NothingClock>::r8::b);
    CHECK(fx.cpu.b() == 0x08);
    CHECK(fx.cpu.f() == 0x00);
}

// ============ Control flow: JP / CALL ============

TEST_CASE("JP n16 - unconditional jump") {
    CPUTest fx;
    fx.cpu.bus_write_no_tick(fx.cpu.pc(), 0x00);
    fx.cpu.bus_write_no_tick(fx.cpu.pc() + 1, 0xC0);
    fx.cpu.jp_n16();
    CHECK(fx.cpu.pc() == 0xC000);
}

TEST_CASE("JP HL") {
    CPUTest fx;
    fx.cpu.hl(0xBEEF);
    fx.cpu.jp_hl();
    CHECK(fx.cpu.pc() == 0xBEEF);
}

TEST_CASE("JP cc,n16 - conditional") {
    CPUTest fx;
    SUBCASE("taken when condition true") {
        fx.cpu.f(0x80);  // Z set
        fx.cpu.pc(0x0000);
        fx.cpu.bus_write_no_tick(0x0000, 0x00);
        fx.cpu.bus_write_no_tick(0x0001, 0xC0);
        fx.cpu.jp_cc_n16(CPU<SimpleMem,NothingClock>::cc::z);
        CHECK(fx.cpu.pc() == 0xC000);
    }
    SUBCASE("not taken when condition false, PC past operand") {
        fx.cpu.f(0x00);  // Z clear
        fx.cpu.pc(0x0000);
        fx.cpu.bus_write_no_tick(0x0000, 0x00);
        fx.cpu.bus_write_no_tick(0x0001, 0xC0);
        fx.cpu.jp_cc_n16(CPU<SimpleMem,NothingClock>::cc::z);
        CHECK(fx.cpu.pc() == 0x0002);   // advanced past the 2 address bytes
    }
}

TEST_CASE("CALL n16 - pushes return address, jumps") {
    CPUTest fx;
    fx.cpu.sp(0xFFFE);
    fx.cpu.pc(0x0000);
    fx.cpu.bus_write_no_tick(0x0000, 0x00);   // target
    fx.cpu.bus_write_no_tick(0x0001, 0xC0);
    fx.cpu.call_n16();
    CHECK(fx.cpu.pc() == 0xC000);      // jumped
    CHECK(fx.cpu.sp() == 0xFFFC);      // SP decremented
    // return address = 0x0002 (after the 2 operand bytes)
    CHECK(fx.cpu.bus_read_no_tick(0xFFFC) == 0x02);  // low
    CHECK(fx.cpu.bus_read_no_tick(0xFFFD) == 0x00);  // high
}

TEST_CASE("CALL then simulated RET restores PC") {
    CPUTest fx;
    fx.cpu.sp(0xFFFE);
    fx.cpu.pc(0x0000);
    fx.cpu.bus_write_no_tick(0x0000, 0x00);
    fx.cpu.bus_write_no_tick(0x0001, 0xC0);
    fx.cpu.call_n16();
    // pop the return address back
    std::uint16_t ret = fx.cpu.pop16();
    CHECK(ret == 0x0002);
    CHECK(fx.cpu.sp() == 0xFFFE);
}

TEST_CASE("CALL cc,n16 - conditional") {
    SUBCASE("taken") {
        CPUTest fx;
        fx.cpu.sp(0xFFFE);
        fx.cpu.f(0x80);  // Z set
        fx.cpu.bus_write_no_tick(0x0000, 0x00);
        fx.cpu.bus_write_no_tick(0x0001, 0xC0);
        fx.cpu.call_cc_n16(CPU<SimpleMem,NothingClock>::cc::z);
        CHECK(fx.cpu.pc() == 0xC000);
        CHECK(fx.cpu.sp() == 0xFFFC);
    }
    SUBCASE("not taken - no push, PC past operand") {
        CPUTest fx;
        fx.cpu.sp(0xFFFE);
        fx.cpu.f(0x00);  // Z clear
        fx.cpu.bus_write_no_tick(0x0000, 0x00);
        fx.cpu.bus_write_no_tick(0x0001, 0xC0);
        fx.cpu.call_cc_n16(CPU<SimpleMem,NothingClock>::cc::z);
        CHECK(fx.cpu.pc() == 0x0002);   // consumed operand, didn't jump
        CHECK(fx.cpu.sp() == 0xFFFE);   // no push
    }
}

// ============ Cycle counting (CountingClock) ============

TEST_CASE("cycle counts - representative instructions") {
    SUBCASE("ADD A,r8 = 1 M-cycle (register, no fetch/bus)") {
        CPUCycleTest fx;
        fx.cpu.a(1); fx.cpu.b(1);
        fx.cpu.add_a_r8(CPU<SimpleMem,gb_emulator::CountingClock>::r8::b);
        CHECK(fx.clock.cycles == 0);  // NOTE: opcode fetch is upstream in dispatch,
                                      // handler itself does no bus access → 0 ticks here
    }
    SUBCASE("ADD A,n8 = immediate fetch = 4 T-cycles") {
        CPUCycleTest fx;
        fx.cpu.bus_write_no_tick(fx.cpu.pc(), 0x01);
        fx.cpu.add_a_n8();
        CHECK(fx.clock.cycles == 4);  // one fetch8
    }
    SUBCASE("ADD A,[HL] = one bus read = 4") {
        CPUCycleTest fx;
        fx.cpu.hl(0xC000);
        fx.cpu.bus_write_no_tick(0xC000, 0x01);
        fx.cpu.add_a_hl_mem();
        CHECK(fx.clock.cycles == 4);
    }
    SUBCASE("ADD HL,r16 = internal cycle = 4") {
        CPUCycleTest fx;
        fx.cpu.hl(0x0100); fx.cpu.bc(0x0200);
        fx.cpu.add_hl_r16(CPU<SimpleMem,gb_emulator::CountingClock>::r16::bc);
        CHECK(fx.clock.cycles == 4);  // internal tick(4)
    }
    SUBCASE("ADD SP,e8 internal cycles = fetch(4) + 8 = 12") {
        CPUCycleTest fx;
        fx.cpu.bus_write_no_tick(fx.cpu.pc(), 0x01);
        fx.cpu.add_sp_e8();
        CHECK(fx.clock.cycles == 12);  // fetch8 (4) + tick(8)
    }
    SUBCASE("PUSH r16 = internal(4) + 2 writes(8) = 12") {
        CPUCycleTest fx;
        fx.cpu.sp(0xFFFE);
        fx.cpu.bc(0x1234);
        fx.cpu.push_r16(CPU<SimpleMem,gb_emulator::CountingClock>::r16::bc);
        CHECK(fx.clock.cycles == 12);
    }
    SUBCASE("POP r16 = 2 reads = 8") {
        CPUCycleTest fx;
        fx.cpu.sp(0xFFFC);
        fx.cpu.pop_r16(CPU<SimpleMem,gb_emulator::CountingClock>::r16::bc);
        CHECK(fx.clock.cycles == 8);
    }
}

// ================= NEW TEST COVERAGE =================

TEST_CASE("LD [HLI],A / LD [HLD],A") {
    CPUTest fx;
    SUBCASE("LD [HLI],A") {
        fx.cpu.hl(0xC000);
        fx.cpu.a(0x42);
        fx.cpu.load_hli_mem_a();
        CHECK(fx.cpu.bus_read_no_tick(0xC000) == 0x42);
        CHECK(fx.cpu.hl() == 0xC001);
    }
    SUBCASE("LD [HLD],A") {
        fx.cpu.hl(0xC000);
        fx.cpu.a(0x42);
        fx.cpu.load_hld_mem_a();
        CHECK(fx.cpu.bus_read_no_tick(0xC000) == 0x42);
        CHECK(fx.cpu.hl() == 0xBFFF);
    }
}

TEST_CASE("LD A,[HLI] / LD A,[HLD]") {
    CPUTest fx;
    SUBCASE("LD A,[HLI]") {
        fx.cpu.hl(0xC000);
        fx.cpu.bus_write_no_tick(0xC000, 0x42);
        fx.cpu.load_a_hli_mem();
        CHECK(fx.cpu.a() == 0x42);
        CHECK(fx.cpu.hl() == 0xC001);
    }
    SUBCASE("LD A,[HLD]") {
        fx.cpu.hl(0xC000);
        fx.cpu.bus_write_no_tick(0xC000, 0x42);
        fx.cpu.load_a_hld_mem();
        CHECK(fx.cpu.a() == 0x42);
        CHECK(fx.cpu.hl() == 0xBFFF);
    }
}

TEST_CASE("DAA - Decimal Adjust Accumulator") {
    CPUTest fx;
    SUBCASE("Addition out of bounds") {
        fx.cpu.a(0x15);
        fx.cpu.b(0x27);
        fx.cpu.add_a_r8(CPU<SimpleMem,NothingClock>::r8::b);
        fx.cpu.daa();
        CHECK(fx.cpu.a() == 0x42); // 15 + 27 in BCD
    }
    SUBCASE("After subtraction with carry") {
        fx.cpu.a(0x22);
        fx.cpu.bus_write_no_tick(fx.cpu.pc(), 0x33);
        fx.cpu.sub_a_n8();
        fx.cpu.daa();
        CHECK(fx.cpu.a() == 0x89);
        CHECK(fx.cpu.flag_c() == true);
    }
}

TEST_CASE("SCF / CCF - Carry Flag Operations") {
    CPUTest fx;
    SUBCASE("SCF sets carry, clears N/H") {
        fx.cpu.f(0x00);
        fx.cpu.scf();
        CHECK(fx.cpu.flag_c() == true);
        CHECK(fx.cpu.flag_n() == false);
        CHECK(fx.cpu.flag_h() == false);
    }
    SUBCASE("CCF flips carry, clears N/H") {
        fx.cpu.f(0x00);
        fx.cpu.scf(); // C=1
        fx.cpu.ccf(); // C=0
        CHECK(fx.cpu.flag_c() == false);
        CHECK(fx.cpu.flag_n() == false);
        CHECK(fx.cpu.flag_h() == false);
    }
}

TEST_CASE("JR / JR cc - Relative Jumps") {
    CPUTest fx;
    SUBCASE("Unconditional JR jumps correctly via relative signed offset") {
        fx.cpu.pc(0x1000);
        fx.cpu.bus_write_no_tick(0x1000, static_cast<std::uint8_t>(-5));
        fx.cpu.jr_n16();
        // Read 1 byte -> PC=1001, jumps by -5 -> PC=0xFFC (1001 - 5 = 0x0FFC)
        CHECK(fx.cpu.pc() == 0x1001 - 5);
    }
    SUBCASE("JR cc conditional taken") {
        fx.cpu.pc(0x1000);
        fx.cpu.bus_write_no_tick(0x1000, 0x05);
        fx.cpu.f(0x80); // Z set
        fx.cpu.jr_cc_n16(CPU<SimpleMem,NothingClock>::cc::z);
        CHECK(fx.cpu.pc() == 0x1001 + 5);
    }
    SUBCASE("JR cc conditional not taken") {
        fx.cpu.pc(0x1000);
        fx.cpu.bus_write_no_tick(0x1000, 0x05);
        fx.cpu.f(0x00); // Z clear
        fx.cpu.jr_cc_n16(CPU<SimpleMem,NothingClock>::cc::z);
        CHECK(fx.cpu.pc() == 0x1001); // Only advanced past operand
    }
}

TEST_CASE("RET / RET cc / RETI") {
    CPUTest fx;
    SUBCASE("Unconditional RET") {
        fx.cpu.sp(0xFFFC);
        fx.cpu.bus_write_no_tick(0xFFFC, 0x00);
        fx.cpu.bus_write_no_tick(0xFFFD, 0xC0);
        fx.cpu.ret();
        CHECK(fx.cpu.pc() == 0xC000);
        CHECK(fx.cpu.sp() == 0xFFFE);
    }
    SUBCASE("RET cc taken") {
        fx.cpu.sp(0xFFFC);
        fx.cpu.bus_write_no_tick(0xFFFC, 0x00);
        fx.cpu.bus_write_no_tick(0xFFFD, 0xC0);
        fx.cpu.f(0x80); // Z set
        fx.cpu.ret_cc(CPU<SimpleMem,NothingClock>::cc::z);
        CHECK(fx.cpu.pc() == 0xC000);
        CHECK(fx.cpu.sp() == 0xFFFE);
    }
    SUBCASE("RET cc not taken") {
        fx.cpu.sp(0xFFFC);
        fx.cpu.pc(0x1000);
        fx.cpu.bus_write_no_tick(0xFFFC, 0x00);
        fx.cpu.bus_write_no_tick(0xFFFD, 0xC0);
        fx.cpu.f(0x00); // Z clear
        fx.cpu.ret_cc(CPU<SimpleMem,NothingClock>::cc::z);
        CHECK(fx.cpu.pc() == 0x1000);
        CHECK(fx.cpu.sp() == 0xFFFC);
    }
    SUBCASE("RETI enables interrupts and returns") {
        fx.cpu.sp(0xFFFC);
        fx.cpu.bus_write_no_tick(0xFFFC, 0x00);
        fx.cpu.bus_write_no_tick(0xFFFD, 0xC0);
        fx.cpu.IME_ = false;
        fx.cpu.reti();
        CHECK(fx.cpu.pc() == 0xC000);
        CHECK(fx.cpu.sp() == 0xFFFE);
        CHECK(fx.cpu.IME_ == true);
    }
}

TEST_CASE("RST - Restart Vectors") {
    CPUTest fx;
    fx.cpu.sp(0xFFFE);
    fx.cpu.pc(0x1234);
    fx.cpu.rst_vec(0x38);
    CHECK(fx.cpu.pc() == 0x0038);
    CHECK(fx.cpu.bus_read_no_tick(0xFFFC) == 0x34); // Low
    CHECK(fx.cpu.bus_read_no_tick(0xFFFD) == 0x12); // High
}

TEST_CASE("DI / EI / HALT / STOP") {
    CPUTest fx;
    SUBCASE("EI / DI toggles IME") {
        fx.cpu.ei();
        CHECK(fx.cpu.IME_ == true);
        fx.cpu.di();
        CHECK(fx.cpu.IME_ == false);
    }
    SUBCASE("HALT without pending interrupts halts normally") {
        fx.cpu.ei();
        fx.cpu.bus_write_no_tick(0xFFFF, 0x00); // IE
        fx.cpu.bus_write_no_tick(0xFF0F, 0x00); // IF
        fx.cpu.halt();
        CHECK(fx.cpu.halted_ == true);
        CHECK(fx.cpu.halt_bug_ == false);
    }
    SUBCASE("HALT bug triggers with disabled IME and pending interrupts") {
        fx.cpu.di();
        fx.cpu.bus_write_no_tick(0xFFFF, 0x01); // IE (VBLANK enabled)
        fx.cpu.bus_write_no_tick(0xFF0F, 0x01); // IF (VBLANK requested)
        fx.cpu.halt();
        CHECK(fx.cpu.halted_ == false);
        CHECK(fx.cpu.halt_bug_ == true);
    }
    SUBCASE("STOP consumes extra byte") {
        fx.cpu.pc(0x1000);
        fx.cpu.stop();
        CHECK(fx.cpu.pc() == 0x1001);
    }
}

TEST_CASE("op_code_decoder dispatch smoke tests") {
    CPUTest fx;
    SUBCASE("NOP") {
        fx.cpu.pc(0x1000);
        fx.cpu.op_code_decoder(0x00);
        CHECK(fx.cpu.pc() == 0x1000); // No fetch occurs internally
    }
    SUBCASE("LD B,D") {
        fx.cpu.b(0);
        fx.cpu.d(0x42);
        fx.cpu.op_code_decoder(0b01'000'010); // 0x42
        CHECK(fx.cpu.b() == 0x42);
    }
    SUBCASE("LD A,n8") {
        fx.cpu.pc(0x1000);
        fx.cpu.bus_write_no_tick(0x1000, 0x99);
        fx.cpu.a(0x00);
        fx.cpu.op_code_decoder(0b00'111'110); // 0x3E is LD A,n8
        CHECK(fx.cpu.a() == 0x99);
        CHECK(fx.cpu.pc() == 0x1001); // PC should advance by 1 byte
    }
    SUBCASE("CP A,n8") {
        fx.cpu.pc(0x1000);
        fx.cpu.bus_write_no_tick(0x1000, 0x99);
        fx.cpu.a(0x99);
        fx.cpu.op_code_decoder(0xFE); // 0xFE is CP A,n8
        CHECK(fx.cpu.flag_z() == true);
        CHECK(fx.cpu.pc() == 0x1001); // PC should advance by 1 byte
    }
}

void setup_cpu_test_no_cycles(CPUTest& fx, const json& j) {
    fx.cpu.pc(j["initial"]["pc"].get<int>());
    fx.cpu.sp(j["initial"]["sp"].get<int>());
    fx.cpu.a(j["initial"]["a"].get<int>());
    fx.cpu.b(j["initial"]["b"].get<int>());
    fx.cpu.c(j["initial"]["c"].get<int>());
    fx.cpu.d(j["initial"]["d"].get<int>());
    fx.cpu.e(j["initial"]["e"].get<int>());
    fx.cpu.f(j["initial"]["f"].get<int>());
    fx.cpu.h(j["initial"]["h"].get<int>());
    fx.cpu.l(j["initial"]["l"].get<int>());
    // fx.cpu.IME(j["initial"]["ime"].get<int>());
    // std::vector<std::vector<int>> ram = j["ram"].get<std::vector<std::vector<int>>>();
    for (const auto& pair : j["initial"]["ram"]) {
        fx.cpu.bus_write_no_tick(pair[0], pair[1]);
    }
}

// void compare_against_final_no_cycles(const CPUTest& fx, const json& j) {
//     CHECK_MESSAGE(fx.cpu.pc() == j["final"]["pc"], "pc");
//     CHECK_MESSAGE(fx.cpu.sp() == j["final"]["sp"], "pc");
//     CHECK_MESSAGE(fx.cpu.a() == j["final"]["a"], "pc");
//     CHECK_MESSAGE(fx.cpu.b() == j["final"]["b"], "pc");
//     CHECK_MESSAGE(fx.cpu.c() == j["final"]["c"], "pc");
//     CHECK_MESSAGE(fx.cpu.d() == j["final"]["d"], "pc");
//     CHECK_MESSAGE(fx.cpu.e() == j["final"]["e"], "pc");
//     CHECK_MESSAGE(fx.cpu.f() == j["final"]["f"], "pc");
//     CHECK_MESSAGE(fx.cpu.h() == j["final"]["h"], "pc");
//     CHECK_MESSAGE(fx.cpu.l() == j["final"]["l"], "pc");
//     for (const auto& pair : j["final"]["ram"]) {
//         CHECK_MESSAGE(fx.cpu.bus_read_no_tick(pair[0]) == pair[1], std::string("ram address: " + to_string(pair[0])));
//     }
// }

bool compare_against_final_no_cycles(const CPUTest& fx, const json& j) {
    bool pass = true;
    CHECK_MESSAGE(fx.cpu.pc() == j["final"]["pc"], "pc");
    pass &= fx.cpu.pc() == j["final"]["pc"];
    CHECK_MESSAGE(fx.cpu.sp() == j["final"]["sp"], "pc");
    pass &= fx.cpu.sp() == j["final"]["sp"];
    CHECK_MESSAGE(fx.cpu.a() == j["final"]["a"], "pc");
    pass &= fx.cpu.a() == j["final"]["a"];
    CHECK_MESSAGE(fx.cpu.b() == j["final"]["b"], "pc");
    pass &= fx.cpu.b() == j["final"]["b"];
    CHECK_MESSAGE(fx.cpu.c() == j["final"]["c"], "pc");
    pass &= fx.cpu.c() == j["final"]["c"];
    CHECK_MESSAGE(fx.cpu.d() == j["final"]["d"], "pc");
    pass &= fx.cpu.d() == j["final"]["d"];
    CHECK_MESSAGE(fx.cpu.e() == j["final"]["e"], "pc");
    pass &= fx.cpu.e() == j["final"]["e"];
    CHECK_MESSAGE(fx.cpu.f() == j["final"]["f"], "pc");
    pass &= fx.cpu.f() == j["final"]["f"];
    CHECK_MESSAGE(fx.cpu.h() == j["final"]["h"], "pc");
    pass &= fx.cpu.h() == j["final"]["h"];
    CHECK_MESSAGE(fx.cpu.l() == j["final"]["l"], "pc");
    pass &= fx.cpu.l() == j["final"]["l"];
    for (const auto& pair : j["final"]["ram"]) {
        CHECK_MESSAGE(fx.cpu.bus_read_no_tick(pair[0]) == pair[1], std::string("ram address: " + to_string(pair[0])));
        pass &= fx.cpu.bus_read_no_tick(pair[0]) == pair[1];
    }
    return pass;
}

// TEST_CASE("SM83 Tests") {
//     std::string test_path = "./sm83/v1";
//     fs::path test_iter = fs::path(test_path);
//     // std::vector<std::string> tests;
//     for (const auto& test : fs::directory_iterator(test_iter)) {
//         // tests.push_back(test.path().string());
//         json j_test;
//         std::ifstream json_test(test.path().string());
//         json_test >> j_test;
//         CPUTest fx;
//         setup_cpu_test_no_cycles(fx, j_test);
//         fx.cpu.step();
//         compare_against_final_no_cycles(fx, j_test);
//     }
// }

TEST_CASE("SM83 Tests") {
    std::string test_path = "./sm83/v1";
    fs::path test_iter = fs::path(test_path);

    // int i = 0;

    for (const auto& test_file : fs::directory_iterator(test_iter)) {
        // if (test_file.path().string() != "./sm83/v1/00.json") continue;
        // i++;
        json j_test_array;
        std::ifstream json_file(test_file.path().string());
        json_file >> j_test_array;

        int i = 0;
        bool fail = false;

        // Iterate over each test object within the array
        for (const auto& test_case : j_test_array) {
            i++;
            INFO("Instruction Test Name: ", test_case["name"].get<std::string>());
            CPUTest fx;
            setup_cpu_test_no_cycles(fx, test_case);
            fx.cpu.step();
            if (!compare_against_final_no_cycles(fx, test_case)) {
                fail = true;
                break;
            }
            if (fail) break;
            // if (i == 4) break;
        }
        if (fail) break;
    }
}
