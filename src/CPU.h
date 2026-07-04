#include <cstdint>
#include <vector>
#include <unordered_map>
#include <utility>

class CPUTest;

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

    template <typename Memory, typename Clock>
    class CPU {
    public:
        // 8 bit register reads
        std::uint8_t a() const {return a_;}
        std::uint8_t b() const {return b_;}
        std::uint8_t c() const {return c_;}
        std::uint8_t d() const {return d_;}
        std::uint8_t e() const {return e_;}
        std::uint8_t f() const {return f_;}
        std::uint8_t h() const {return h_;}
        std::uint8_t l() const {return l_;}

        // 16 bit register reads
        std::uint16_t pc() const {return pc_;}
        std::uint16_t sp() const {return sp_;}
        std::uint16_t af() const {return (a_ << 8) | f_;}
        std::uint16_t bc() const {return (b_ << 8) | c_;}
        std::uint16_t de() const {return (d_ << 8) | e_;}
        std::uint16_t hl() const {return (h_ << 8) | l_;}

        // 8 bit register writes
        void a(std::uint8_t val) {a_ = val;}
        void b(std::uint8_t val) {b_ = val;}
        void c(std::uint8_t val) {c_ = val;}
        void d(std::uint8_t val) {d_ = val;}
        void e(std::uint8_t val) {e_ = val;}
        void f(std::uint8_t val) {
            f_ = val & 0xF0;
        }
        void h(std::uint8_t val) {h_ = val;}
        void l(std::uint8_t val) {l_ = val;}

        //16 bit register writes
        void pc(std::uint16_t val) {pc_ = val;}
        void sp(std::uint16_t val) {sp_ = val;}

        void af(std::uint16_t val) {
            a_ = static_cast<uint8_t>(val >> 8);
            f(static_cast<uint8_t>(val));
        }
        void bc(std::uint16_t val) {
            b_ = static_cast<uint8_t>(val >> 8);
            c_ = static_cast<uint8_t>(val);
        }
        void de(std::uint16_t val) {
            d_ = static_cast<uint8_t>(val >> 8);
            e_ = static_cast<uint8_t>(val);
        }
        void hl(std::uint16_t val) {
            h_ = static_cast<uint8_t>(val >> 8);
            l_ = static_cast<uint8_t>(val);
        }

        //flag reads
        bool flag_z() const {return f_ & 0x80;}
        bool flag_n() const {return f_ & 0x40;}
        bool flag_h() const {return f_ & 0x20;}
        bool flag_c() const {return f_ & 0x10;}

        // constructors
        CPU(Memory& mem, Clock& clock) : a_{0}, b_{0}, c_{0}, d_{0}, e_{0}, f_{0}, h_{0}, l_{0}, pc_{0}, sp_{0}, mem_{mem}, clock_{clock} {}

    private:
        // registers
        std::uint8_t a_;
        std::uint8_t b_;
        std::uint8_t c_;
        std::uint8_t d_;
        std::uint8_t e_;
        std::uint8_t f_;
        std::uint8_t h_;
        std::uint8_t l_;
        std::uint16_t pc_;
        std::uint16_t sp_;

        // templated parameters
        Memory& mem_;
        Clock& clock_;

        // clock function left entirely to the clock implementation
        void tick(int cycles) {
            clock_.tick(cycles);
        }

        // memory access

        // bus access
        // primarily an implementation detail to be wrapped by helpers
        std::uint8_t bus_read(std::uint16_t address) {
            std::uint8_t val = mem_.read(address);
            tick(4);
            return val;
        }

        void bus_write(std::uint16_t address, std::uint8_t data) {
            mem_.write(address, data);
            tick(4);
        }

        // instruction fetching
        std::uint8_t fetch8() {
            return bus_read(pc_++);
        }

        std::uint16_t fetch16() {
            std::uint8_t lo = fetch8();
            std::uint8_t hi = fetch8();
            return (static_cast<std::uint16_t>(hi) << 8) | lo;
        }

        // read wrappers
        std::uint8_t read8(std::uint16_t address) {
            return bus_read(address);
        }

        // write wrappers
        void write8(std::uint16_t address, std::uint8_t data) {
            bus_write(address, data);
        }

        void write16(std::uint16_t address, std::uint16_t data) {
            std::uint8_t lo = static_cast<std::uint8_t>(data);
            std::uint8_t hi = static_cast<std::uint8_t>(data >> 8);
            write8(address, lo);
            write8(address + 1, hi);
        }

        // 8 bit register helpers
        enum class r8 {
            a, b, c, d, e, h, l
        };

        std::uint8_t read_r8(r8 source) {
            switch(source) {
                case r8::a: return a();
                case r8::b: return b();
                case r8::c: return c();
                case r8::d: return d();
                case r8::e: return e();
                case r8::h: return h();
                case r8::l: return l();
                default: std::unreachable();
            }
        }

        void write_r8(r8 dest, std::uint8_t val) {
            switch(dest) {
                case r8::a: a(val);
                break;
                case r8::b: b(val);
                break;
                case r8::c: c(val);
                break;
                case r8::d: d(val);
                break;
                case r8::e: e(val);
                break;
                case r8::h: h(val);
                break;
                case r8::l: l(val);
                break;
                default: std::unreachable();
            }
        }

        // 16 bit register helpers
        enum class r16 {
            bc, de, hl, sp
        };

        std::uint16_t read_r16(r16 source) {
            switch(source) {
                case r16::bc: return bc();
                case r16::de: return de();
                case r16::hl: return hl();
                case r16::sp: return sp();
                default: std::unreachable();
            }
        }

        void write_r16(r16 dest, std::uint16_t val) {
            switch(dest) {
                case r16::bc: bc(val);
                break;
                case r16::de: de(val);
                break;
                case r16::hl: hl(val);
                break;
                case r16::sp: sp(val);
                break;
                default: std::unreachable();
            }
        }

        // flag helpers
        std::uint8_t set_bit(std::uint8_t input, int bit, bool val) {
            return input & ~(1 << bit) | (1 << bit)*val;
        }

        std::uint8_t set_z(std::uint8_t input, bool val) {
            return set_bit(input, 7, val);
        }

        std::uint8_t set_n(std::uint8_t input, bool val) {
            return set_bit(input, 6, val);
        }

        std::uint8_t set_h(std::uint8_t input, bool val) {
            return set_bit(input, 5, val);
        }

        std::uint8_t set_c(std::uint8_t input, bool val) {
            return set_bit(input, 4, val);
        }

        std::uint8_t set_flag(bool z, bool n, bool h, bool c) {
            std::uint8_t flag = set_z(0, z);
            flag = set_n(flag, n);
            flag = set_h(flag, h);
            flag = set_c(flag, c);
            return flag;
        }

        // load instructions
        // LD r8,r8
        void load_r8_r8(r8 dest, r8 source) {
            write_r8(dest, read_r8(source));
        }

        // LD r8,n8
        void load_r8_n8(r8 dest) {
            write_r8(dest, fetch8());
        }

        // LD r16,n16
        void load_r16_n16(r16 dest) {
            std::uint16_t val = fetch16();
            write_r16(dest, val);
        }

        // LD [HL],r8
        void load_hl_r8(r8 source) {
            write8(hl(), read_r8(source));
        }
    };
}
