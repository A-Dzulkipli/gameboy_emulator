#include <cstdint>
#include <vector>
#include <unordered_map>
#include <utility>

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
    friend class CPUTest;
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

        void bus_write(std::uint16_t address, std::uint8_t data) {
            mem_.write(address, data);
            tick(4);
        }

        std::uint8_t fetch8() {
            return bus_read(pc_++);
        }

        std::uint16_t fetch16() {
            std::uint8_t lo = fetch8();
            std::uint8_t hi = fetch8();
            return (static_cast<std::uint16_t>(hi) << 8) | lo;
        }

        void write8(std::uint16_t address, std::uint8_t data) {
            bus_write(address, data);
        }

        void write16(std::uint16_t address, std::uint16_t data) {
            std::uint8_t lo = static_cast<std::uint8_t>(data);
            std::uint8_t hi = static_cast<std::uint8_t>(data >> 8);
            write8(address, lo);
            write8(address + 1, hi);
        }

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

        void load_r8_r8(r8 dest, r8 source) {
            write_r8(dest, read_r8(source));
        }

        void load_r8_n8(r8 dest) {
            write_r8(dest, fetch8());
        }

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

        void load_r16_n16(r16 dest) {
            std::uint16_t val = fetch16();
            write_r16(dest, val);
        }

        void load_hl_r8(r8 source) {
            write8(hl(), read_r8(source));
        }

        void load_hl_n8() {
            write8(hl(), fetch8());
        }

        void load_r8_hl(r8 dest) {
            write_r8(dest, bus_read(hl()));
        }

        // LD [r16],A
        void load_r16_mem_A(r16 address_register) {
            write8(read_r16(address_register), a());
        }

        // LD [n16],A
        void load_n16_mem_a() {
            std::uint16_t address = fetch16();
            write8(address, a());
        }

        // LDH [n16],A
        void loadh_n16_mem_a() {
            std::uint8_t address_lo = fetch8();
            std::uint16_t address = 0xFF00 | static_cast<std::uint16_t>(address_lo);
            write8(address, a());
        }

        // LDH [C],A
        void loadh_c_mem_a() {
            std::uint8_t address_lo = c();
            std::uint16_t address = 0xFF00 | static_cast<std::uint16_t>(address_lo);
            write8(address, a());
        }

        // LD A,[r16]
        void load_a_r16_mem(r16 source_address) {
            write_r8(r8::a, bus_read(read_r16(source_address)));
        }

        // LD A,[n16]
        void load_a_n16_mem() {
            std::uint16_t address = fetch16();
            write_r8(r8::a, bus_read(address));
        }

        // LDH A,[n16]
        void loadh_a_n16_mem() {
            std::uint8_t address_8 = fetch8();
            std::uint16_t address = static_cast<std::uint16_t>(address_8) | 0xFF00;
            write_r8(r8::a, bus_read(address));
        }

        // LDH A,[C]
        void loadh_a_c_mem() {
            std::uint8_t lo = c();
            std::uint16_t address = static_cast<std::uint16_t>(lo) | 0xFF00;
            write_r8(r8::a, bus_read(address));
        }

        // LD [HLI],A
        void load_hli_mem_a() {
            std::uint16_t address = hl();
            write8(address, a());
            write_r16(r16::hl, hl() + 1);
        }

        // LD [HLD],A
        void load_hld_mem_a() {
            std::uint16_t address = hl();
            write8(address, a());
            write_r16(r16::hl, hl() - 1);
        }

        // LD A,[HLD]
        void load_a_hld_mem() {
            std::uint16_t address = hl();
            std::uint8_t data = bus_read(address);
            write_r8(r8::a, data);
            write_r16(r16::hl, hl() - 1);
        }

        // LD A,[HLI]
        void load_a_hli_mem() {
            std::uint16_t address = hl();
            std::uint8_t data = bus_read(address);
            write_r8(r8::a, data);
            write_r16(r16::hl, hl() + 1);
        }

        std::uint8_t adc_a_r8_set_flags(std::uint8_t a_data, std::uint8_t source_data, bool carry) {
            std::uint16_t sum = a_data + source_data + carry;
            std::uint8_t  result = static_cast<std::uint8_t>(sum);
            bool half = (a_data & 0xF) + (source_data & 0xF) + carry > 0xF;

            std::uint8_t new_f =
                  0x80 * (result == 0)
                | 0x20 * half
                | 0x10 * (sum > 0xFF);
            return new_f;
        }

        //ADC A,r8
        void adc_a_r8(r8 source) {
            std::uint8_t a_data = a();
            std::uint8_t source_data = read_r8(source);
            bool carry = flag_c();
            std::uint8_t new_f = adc_a_r8_set_flags(a_data, source_data, carry);
            std::uint8_t result = a_data + source_data + carry;

            a(result);
            f(new_f);
        }

        // ADC A,[HL]
        void adc_a_hl_mem() {
            std::uint8_t source_data = bus_read(hl());
            bool carry = flag_c();
            std::uint8_t a_data = a();
            std::uint8_t new_f = adc_a_r8_set_flags(a_data, source_data, carry);
            std::uint8_t result = a_data + source_data + carry;
            a(result);
            f(new_f);
        }

        // ADC A,n8
        void adc_a_n8() {
            std::uint8_t source_data = fetch8();
            bool carry = flag_c();
            std::uint8_t a_data = a();
            std::uint8_t new_f = adc_a_r8_set_flags(a_data, source_data, carry);
            std::uint8_t result = a_data + source_data + carry;
            a(result);
            f(new_f);
        }

        // ADD A,r8
        void add_a_r8(r8 source) {
            std::uint8_t source_data = read_r8(source);
            std::uint8_t a_data = a();
            std::uint8_t new_f = adc_a_r8_set_flags(a_data, source_data, false);
            std::uint8_t result = a_data + source_data;
            a(result);
            f(new_f);
        }

        // ADD A,[HL]
        void add_a_hl_mem() {
            std::uint8_t source_data = bus_read(hl());
            std::uint8_t a_data = a();
            std::uint8_t new_f = adc_a_r8_set_flags(a_data, source_data, false);
            std::uint8_t result = a_data + source_data;
            a(result);
            f(new_f);
        }

        // ADD A,n8
        void add_a_n8() {
            std::uint8_t source_data = fetch8();
            std::uint8_t a_data = a();
            std::uint8_t new_f = adc_a_r8_set_flags(a_data, source_data, false);
            std::uint8_t result = a_data + source_data;
            a(result);
            f(new_f);
        }

        std::uint8_t cp_a_r8_set_flags(std::uint8_t a_data, std::uint8_t source_data) {
            std::uint8_t lo_a = a_data & 0xF;
            std::uint8_t lo_source = source_data & 0xF;
            std::uint8_t result = a_data - source_data;
            std::uint8_t new_flag = 0 |
                0x80*(result == 0) |
                0x40 |
                0x20*(lo_a < lo_source) |
                0x10*(source_data > a_data);
            return new_flag;
        }

        // CP A,r8
        void cp_a_r8(r8 source) {
            std::uint8_t source_data = read_r8(source);
            std::uint8_t a_data = a();
            std::uint8_t new_f = cp_a_r8_set_flags(a_data, source_data);
            f(new_f);
        }

        // CP A,[HL]
        void cp_a_hl_mem() {
            std::uint8_t source_data = bus_read(hl());
            std::uint8_t a_data = a();
            std::uint8_t new_f = cp_a_r8_set_flags(a_data, source_data);
            f(new_f);
        }

        // CP A,n8
        void cp_a_n8() {
            std::uint8_t source_data = fetch8();
            std::uint8_t a_data = a();
            std::uint8_t new_f = cp_a_r8_set_flags(a_data, source_data);
            f(new_f);
        }

        std::uint8_t dec_r8_set_flags(std::uint8_t data) {
            std::uint8_t lo = data & 0xF;
            std::uint8_t new_flag = 0x80*(data - 1 == 0) | 0x40 | 0x20*(lo == 0) | 0x10*flag_c();
            return new_flag;
        }

        // DEC r8
        void dec_r8(r8 reg) {
            std::uint8_t data = read_r8(reg);
            std::uint8_t result = data - 1;
            std::uint8_t new_f = dec_r8_set_flags(data);
            write_r8(reg, result);
            f(new_f);
        }

        // DEC [HL]
        void dec_hl_mem() {
            std::uint8_t data = bus_read(hl());
            std::uint8_t result = data - 1;
            std::uint8_t new_f = dec_r8_set_flags(data);
            write8(hl(), result);
            f(new_f);
        }

        std::uint8_t inc_r8_set_flags(std::uint8_t data) {
            std::uint8_t lo = data & 0xF;
            std::uint8_t new_flag = 0x80*(((data + 1) & 0xFF) == 0) |
                0x40*0 |
                0x20*(lo + 1 > 0xF) |
                0x10*flag_c();
            return new_flag;
        }

        // INC r8
        void inc_r8(r8 reg) {
            std::uint8_t data = read_r8(reg);
            std::uint8_t result = data + 1;
            std::uint8_t new_f = inc_r8_set_flags(data);
            write_r8(reg, result);
            f(new_f);
        }

        // INC [HL]
        void inc_hl_mem() {
            std::uint8_t data = bus_read(hl());
            std::uint8_t result = data + 1;
            std::uint8_t new_f = inc_r8_set_flags(data);
            write8(hl(), result);
            f(new_f);
        }

        std::uint8_t sbc_a_r8_set_flags(std::uint8_t a_data, std::uint8_t data, bool carry) {
            std::uint8_t lo_a = a_data & 0xF;
            std::uint8_t lo_data = data & 0xF;
            std::uint8_t new_flag = 0x80*(a_data - data - carry == 0) |
                0x40 |
                0x20*(lo_a < lo_data + carry) |
                0x10*(a_data < data + carry);
            return new_flag;
        }

        // SBC A,r8
        void sbc_a_r8(r8 reg) {
            std::uint8_t data = read_r8(reg);
            std::uint8_t a_data = a();
            bool carry = flag_c();
            std::uint8_t result = a_data - data - carry;
            std::uint8_t new_f = sbc_a_r8_set_flags(a_data, data, carry);
            a(result);
            f(new_f);
        }

        // SBC A,[HL]
        void sbc_a_hl_mem() {
            std::uint8_t data = bus_read(hl());
            std::uint8_t a_data = a();
            bool carry = flag_c();
            std::uint8_t result = a_data - data - carry;
            std::uint8_t new_f = sbc_a_r8_set_flags(a_data, data, carry);
            a(result);
            f(new_f);
        }

        // SBC A,n8
        void sbc_a_n8() {
            std::uint8_t data = fetch8();
            std::uint8_t a_data = a();
            bool carry = flag_c();
            std::uint8_t result = a_data - data - carry;
            std::uint8_t new_f = sbc_a_r8_set_flags(a_data, data, carry);
            a(result);
            f(new_f);
        }

        // SUB A,r8
        void sub_a_r8(r8 reg) {
            std::uint8_t data = read_r8(reg);
            std::uint8_t a_data = a();
            std::uint8_t result = a_data - data;
            std::uint8_t new_f = sbc_a_r8_set_flags(a_data, data, false);
            a(result);
            f(new_f);
        }

        // SUB A,[HL]
        void sub_a_hl_mem() {
            std::uint8_t data = bus_read(hl());
            std::uint8_t a_data = a();
            std::uint8_t result = a_data - data;
            std::uint8_t new_f = sbc_a_r8_set_flags(a_data, data, false);
            a(result);
            f(new_f);
        }

        // SUB A,n8
        void sub_a_n8() {
            std::uint8_t data = fetch8();
            std::uint8_t a_data = a();
            std::uint8_t result = a_data - data;
            std::uint8_t new_f = sbc_a_r8_set_flags(a_data, data, false);
            a(result);
            f(new_f);
        }

        std::uint8_t add_hl_r16_set_flags(std::uint16_t hl_data, std::uint16_t data) {
            std::uint16_t hl_data_up_to_bit_11 = hl_data & 4095;
            std::uint16_t data_up_to_bit_11 = data & 4095;
            std::uint8_t new_flag = 0x80*flag_z() |
                0x40*0 |
                0x20*(hl_data_up_to_bit_11 + data_up_to_bit_11 >= (1 << 12)) |
                0x10*(hl_data + data >= (1 << 16));
            return new_flag;
        }

        // ADD HL,r16
        void add_hl_r16(r16 reg) {
            tick(4);
            std::uint16_t data = read_r16(reg);
            std::uint16_t hl_data = hl();
            std::uint8_t new_f = add_hl_r16_set_flags(hl_data, data);
            hl(data + hl_data);
            f(new_f);
        }

        // DEC r16
        void dec_r16(r16 reg) {
            tick(4);
            std::uint16_t data = read_r16(reg);
            write_r16(reg, data-1);
        }

        // INC r16
        void inc_r16(r16 reg) {
            tick(4);
            std::uint16_t data = read_r16(reg);
            write_r16(reg, data+1);
        }

        std::uint8_t and_a_r8_set_flags(std::uint8_t a_data, std::uint8_t data) {
            std::uint8_t new_flag = 0x80*((a_data & data) == 0) |
                0x40*0 |
                0x20 |
                0x10*0;
            return new_flag;
        }

        // AND A,r8
        void and_a_r8(r8 source) {
            std::uint8_t data = read_r8(source);
            std::uint8_t a_data = a();
            std::uint8_t new_f = and_a_r8_set_flags(a_data, data);
            a(data & a_data);
            f(new_f);
        }

        // AND A,[HL]
        void and_a_hl_mem() {
            std::uint8_t data = bus_read(hl());
            std::uint8_t a_data = a();
            std::uint8_t new_f = and_a_r8_set_flags(a_data, data);
            a(data & a_data);;
            f(new_f);
        }

        // AND A,n8
        void and_a_n8() {
            std::uint8_t data = fetch8();
            std::uint8_t a_data = a();
            std::uint8_t new_f = and_a_r8_set_flags(a_data, data);
            a(data & a_data);;
            f(new_f);
        }

        // CPL
        void cpl() {
            a(~a());
            f(0x80*flag_z() | 0x40 | 0x20 | 0x10*flag_c());
        }

        std::uint8_t or_a_r8_set_flags(std::uint8_t a_data, std::uint8_t data) {
            std::uint8_t new_flag = 0x80*((a_data | data) == 0);
            return new_flag;
        }

        // OR A,r8
        void or_a_r8(r8 source) {
            std::uint8_t data = read_r8(source);
            std::uint8_t a_data = a();
            std::uint8_t new_f = or_a_r8_set_flags(a_data, data);
            a(a_data | data);
            f(new_f);
        }

        // OR A,[HL]
        void or_a_hl_mem() {
            std::uint8_t data = bus_read(hl());
            std::uint8_t a_data = a();
            std::uint8_t new_f = or_a_r8_set_flags(a_data, data);
            a(a_data | data);
            f(new_f);
        }

        // OR A,n8
        void or_a_n8() {
            std::uint8_t data = fetch8();
            std::uint8_t a_data = a();
            std::uint8_t new_f = or_a_r8_set_flags(a_data, data);
            a(a_data | data);
            f(new_f);
        }

        std::uint8_t xor_a_r8_set_flags(std::uint8_t a_data, std::uint8_t data) {
            std::uint8_t new_flag = 0x80*((a_data ^ data) == 0);
            return new_flag;
        }

        // XOR A,r8
        void xor_a_r8(r8 source) {
            std::uint8_t data = read_r8(source);
            std::uint8_t a_data = a();
            std::uint8_t new_f = xor_a_r8_set_flags(a_data, data);
            a(a_data ^ data);
            f(new_f);
        }

        // XOR A,[HL]
        void xor_a_hl_mem() {
            std::uint8_t data = bus_read(hl());
            std::uint8_t a_data = a();
            std::uint8_t new_f = xor_a_r8_set_flags(a_data, data);
            a(a_data ^ data);
            f(new_f);
        }

        // XOR A,n8
        void xor_a_n8() {
            std::uint8_t data = fetch8();
            std::uint8_t a_data = a();
            std::uint8_t new_f = xor_a_r8_set_flags(a_data, data);
            a(a_data ^ data);
            f(new_f);
        }

        enum class u3 {
            u_0, u_1, u_2, u_3, u_4, u_5, u_6, u_7
        };

        int u3_to_int(u3 bit) {
            switch(bit) {
                case u3::u_0: return 0;
                case u3::u_1: return 1;
                case u3::u_2: return 2;
                case u3::u_3: return 3;
                case u3::u_4: return 4;
                case u3::u_5: return 5;
                case u3::u_6: return 6;
                case u3::u_7: return 7;
                default: std::unreachable();
            }
        }

        std::uint8_t bit_u3_r8_set_flags(int bit_num, std::uint8_t data) {
            std::uint8_t new_flag = 0x80*((data & (1 << bit_num)) == 0) | 0x20;
            return new_flag;
        }

        // BIT u3,r8
        void bit_u3_r8(u3 bit, r8 reg) {
            int bit_num = u3_to_int(bit);
            std::uint8_t data = read_r8(reg);
            std::uint8_t new_f = bit_u3_r8_set_flags(bit_num, data);
            f(new_f);
        }

        // BIT u3,[HL]
        void bit_u3_hl_mem(u3 bit) {
            int bit_num = u3_to_int(bit);
            std::uint8_t data = bus_read(hl());
            std::uint8_t new_f = bit_u3_r8_set_flags(bit_num, data);
            f(new_f);
        }

        // RES u3,r8
        void res_u3_r8(u3 bit, r8 reg) {
            std::uint8_t data = read_r8(reg);
            int bit_num = u3_to_int(bit);
            data &= ~(1 << bit_num);
            write_r8(reg, data);
        }

        // RES u3,[HL]
        void res_u3_hl_mem(u3 bit) {
            std::uint8_t data = bus_read(hl());
            int bit_num = u3_to_int(bit);
            data &= ~(1 << bit_num);
            write8(hl(), data);
        }

        // SET u3,r8
        void set_u3_r8(u3 bit, r8 reg) {
            std::uint8_t data = read_r8(reg);
            int bit_num = u3_to_int(bit);
            data |= (1 << bit_num);
            write_r8(reg, data);
        }

        // SET u3,[HL]
        void set_u3_hl_mem(u3 bit) {
            std::uint8_t data = bus_read(hl());
            int bit_num = u3_to_int(bit);
            data |= (1 << bit_num);
            write8(hl(), data);
        }

        std::uint8_t rl_r8_set_flags(std::uint16_t data) {
            data <<= 1;
            data |= 0x1*flag_c();
            std::uint8_t new_flag = 0x80*(static_cast<uint8_t>(data) == 0) |
                0x40*0 |
                0x20*0 |
                0x10*((data & (1 << 8)) != 0);
            return new_flag;
        }

        // RL r8
        void rl_r8(r8 reg) {
            std::uint16_t data = static_cast<uint16_t>(read_r8(reg));
            std::uint8_t new_f = rl_r8_set_flags(data);
            data <<= 1;
            data |= 0x1*(flag_c());
            write_r8(reg, static_cast<uint8_t>(data));
            f(new_f);
        }

        // RL [HL]
        void rl_hl_mem() {
            std::uint16_t data = bus_read(hl());
            std::uint8_t new_f = rl_r8_set_flags(data);
            data <<= 1;
            data |= 0x1*flag_c();
            write8(hl(), static_cast<uint8_t>(data));
            f(new_f);
        }

        std::uint8_t rla_set_flags(std::uint16_t data) {
            std::uint8_t new_flag = rl_r8_set_flags(data);
            new_flag &= ~0x80;
            return new_flag;
        }

        // RLA
        void rla() {
            std::uint16_t data = static_cast<uint16_t>(a());
            std::uint8_t new_f = rla_set_flags(data);
            data <<= 1;
            data |= 0x1*flag_c();
            a(static_cast<uint8_t>(data));
            f(new_f);
        }



        // LD SP,n16
        void load_sp_n16() {
            std::uint16_t data = fetch16();
            write_r16(r16::sp, data);
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

        std::uint16_t inc_pc() {
            std::uint16_t val = pc();
            pc_++;
            return val;
        }

        std::uint16_t dec_pc() {
            pc_--;
            std::uint16_t val = pc();
            return val;
        }

        std::uint16_t inc_sp() {
            std::uint16_t val = sp();
            sp_++;
            return val;
        }

        std::uint16_t dec_sp() {
            sp_--;
            std::uint16_t val = sp();
            return val;
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
