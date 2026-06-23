#include <cstdint>

namespace gb_emulator {
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
        CPU() : a_{0}, b_{0}, c_{0}, d_{0}, e_{0}, f_{0}, h_{0}, l_{0}, pc_{0}, sp_{0} {}
    };
}
