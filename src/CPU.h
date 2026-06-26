#include <cstdint>
#include <vector>
#include <unordered_map>

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

    template <typename Memory>
    class CPU {
    private:
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

        Memory& mem_;

        void tick(int cycles) {}

        std::uint8_t bus_read(std::uint16_t address) {
            std::uint8_t val = mem_.read(address);
            tick(4);
            return val;
        }

        std::uint8_t fetch8() {
            return bus_read(pc_++);
        }

        enum class r8 {
            a, b, c, d, e, f, h, l
        };

        std::uint8_t read_r8(r8 source) {
            switch(source) {
                case r8::a: return a();
                case r8::b: return b();
                case r8::c: return c();
                case r8::d: return d();
                case r8::e: return e();
                case r8::f: return f();
                case r8::h: return h();
                case r8::l: return l();
            }
        }

        void write_r8(r8 dest, std::uint8_t val) {
            switch(dest) {
                case r8::a: a(val);
                break;
                case r8::b: b(val);
                break;

            }
        }

    public:
        std::uint8_t a() const {return a_;}
        std::uint8_t b() const {return b_;}
        std::uint8_t c() const {return c_;}
        std::uint8_t d() const {return d_;}
        std::uint8_t e() const {return e_;}
        std::uint8_t f() const {return f_;}
        std::uint8_t h() const {return h_;}
        std::uint8_t l() const {return l_;}

        std::uint16_t pc() const {return pc_;}
        std::uint16_t sp() const {return sp_;}
        std::uint16_t af() const {return (a_ << 8) | f_;}
        std::uint16_t bc() const {return (b_ << 8) | c_;}
        std::uint16_t de() const {return (d_ << 8) | e_;}
        std::uint16_t hl() const {return (h_ << 8) | l_;}

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

        bool flag_z() const {return f_ & 0x80;}
        bool flag_n() const {return f_ & 0x40;}
        bool flag_h() const {return f_ & 0x20;}
        bool flag_c() const {return f_ & 0x10;}

        CPU(Memory& mem) : a_{0}, b_{0}, c_{0}, d_{0}, e_{0}, f_{0}, h_{0}, l_{0}, pc_{0}, sp_{0}, mem_{mem} {}

        // enum class Op8 {
        //     a, b, c, d, e, h, l,
        //     imm8,
        //     mem_bc, mem_de, mem_hl,
        //     mem_hl_inc, mem_hl_dec,
        //     mem_imm16,
        //     high_c,
        //     high_n
        // };

        // enum class Op16 {
        //     bc, de, hl, sp,
        //     af,
        //     imm16
        // };




    };
}
