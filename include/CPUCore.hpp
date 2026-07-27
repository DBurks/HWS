#pragma once
#include "PlatformConfig.hpp"
#include <array>
#include <cassert>
#include <iomanip>

enum class TraceContext {
    Exec,
    VectorIRQ,
    VectorNMI,
    VectorReset
};

template <typename Config>
class CPUCore {
public:
    using Addr = typename Config::AddrType;
    using Data = typename Config::DataType;
    using Reg  = typename Config::RegEnum;

private:
    std::array<Data, Config::RegisterCount> regs{};
    Addr pc{0};       
    Data status{0x24}; 
    bool trace_enabled_{true};

public:
    CPUCore() : regs{}, pc(0x0000), status(0x24) {}

    void set_trace_enabled(bool enable) { trace_enabled_ = enable; }
    [[nodiscard]] bool is_trace_enabled() const { return trace_enabled_; }

    template <typename BusType>
    void reset(BusType& bus, uint32_t& cycles) {

        regs[static_cast<size_t>(Reg::SP)] = 0xFD; 
        status = 0x24; // Sets I-flag (bit 2) and Unused (bit 5)

        // 2. Set Interrupt Disable (I) flag and Unused (U/B2) flag
        set_flag(Flag6502::Interrupt, true);
        set_flag(Flag6502::Unused, true);

        // 3. Read reset vector from $FFFC-$FFFD
        uint8_t low  = bus.read_raw(0xFFFC);
        uint8_t high = bus.read_raw(0xFFFD);
        pc = (static_cast<uint16_t>(high) << 8) | low;

        // 4. Reset sequence takes exactly 7 cycles
        cycles += 7;
    }

    inline Data get_reg(Reg register_index) const {
        return regs[static_cast<size_t>(register_index)];
    }
    inline void set_reg(Reg register_index, Data value) {
        regs[static_cast<size_t>(register_index)] = value;
    }
    inline Addr get_pc() const { return pc; }
    inline void set_pc(Addr target_address) { pc = target_address; }
    inline Data get_status() const { return status; }
    inline void set_status(Data s) { status = s; }

    inline bool get_flag(uint8_t flag_bitmask) const {
        return (status & flag_bitmask) != 0;
    }
    inline void set_flag(uint8_t flag_bitmask, bool state) {
        if (state) status |= flag_bitmask;
        else       status &= ~flag_bitmask;
    }

private:
    // ---- Flag Helpers ----
    inline void update_nz_flags(Data value) {
        const uint8_t v8 = static_cast<uint8_t>(value);
        bool z = (v8 == 0);
        bool n = (v8 & 0x80) != 0;

        set_flag(Flag6502::Zero, z);
        set_flag(Flag6502::Negative, n);
    }

    // ---- Bus Access ----
    template <typename BusType>
    inline Data read_byte(BusType& bus, Addr address, uint32_t& cycles) {
        return bus.read(address, cycles);
    }
    template <typename BusType>
    inline void write_byte(BusType& bus, Addr address, Data data, uint32_t& cycles) {
        bus.write(address, data, cycles);
    }

    // ---- Stack Helpers ----
    inline Addr get_push_address() {
        return Config::StackBase | static_cast<Addr>(regs[static_cast<size_t>(Reg::SP)]);
    }
    inline Addr get_pull_address() {
        regs[static_cast<size_t>(Reg::SP)]++;
        return Config::StackBase | static_cast<Addr>(regs[static_cast<size_t>(Reg::SP)]);
    }

    // ---- Addressing Mode Fetchers ----
    // Each fetcher reads operand bytes from the instruction stream,
    // advances PC, and returns the effective address for the operand.

    // Immediate: no bus read needed — just advances PC past the operand byte
    inline Addr fetch_immediate() { return pc++; }

    template <typename BusType>
    inline Addr fetch_zero_page(BusType& bus, uint32_t& cycles) {
        Data zp_addr = read_byte(bus, pc++, cycles);
        return static_cast<Addr>(zp_addr);
    }
    template <typename BusType>
    inline Addr fetch_zero_page_x(BusType& bus, uint32_t& cycles) {
        Data zp_addr = read_byte(bus, pc++, cycles);
        return static_cast<Addr>(zp_addr + regs[static_cast<size_t>(Reg::X)]) & 0xFF;
    }
    template <typename BusType>
    inline Addr fetch_zero_page_y(BusType& bus, uint32_t& cycles) {
        Data zp_addr = read_byte(bus, pc++, cycles);
        return static_cast<Addr>(zp_addr + regs[static_cast<size_t>(Reg::Y)]) & 0xFF;
    }
    template <typename BusType>
    inline Addr fetch_absolute(BusType& bus, uint32_t& cycles) {
        Data low  = read_byte(bus, pc++, cycles);
        Data high = read_byte(bus, pc++, cycles);
        return (static_cast<Addr>(high) << 8) | low;
    }
    template <typename BusType>
    inline Addr fetch_absolute_x(BusType& bus, uint32_t& cycles) {
        Data low  = read_byte(bus, pc++, cycles);
        Data high = read_byte(bus, pc++, cycles);
        Addr base = (static_cast<Addr>(high) << 8) | low;
        Addr result = base + regs[static_cast<size_t>(Reg::X)];
        if ((base & 0xFF00) != (result & 0xFF00)) cycles += 1;
        return result;
    }
    template <typename BusType>
    inline Addr fetch_absolute_y(BusType& bus, uint32_t& cycles) {
        Data low  = read_byte(bus, pc++, cycles);
        Data high = read_byte(bus, pc++, cycles);
        Addr base = (static_cast<Addr>(high) << 8) | low;
        Addr result = base + regs[static_cast<size_t>(Reg::Y)];
        if ((base & 0xFF00) != (result & 0xFF00)) cycles += 1;
        return result;
    }
    template <typename BusType>
    inline Addr fetch_indirect(BusType& bus, uint32_t& cycles) {
        Data low  = read_byte(bus, pc++, cycles);
        Data high = read_byte(bus, pc++, cycles);
        Addr pointer_addr = (static_cast<Addr>(high) << 8) | low;
        Addr pointer_high = (pointer_addr & 0xFF00) | static_cast<Addr>(static_cast<Data>(pointer_addr + 1));
        Data target_low  = read_byte(bus, pointer_addr, cycles);
        Data target_high = read_byte(bus, pointer_high, cycles);
        return (static_cast<Addr>(target_high) << 8) | target_low;
    }
    template <typename BusType>
    inline Addr fetch_izx(BusType& bus, uint32_t& cycles) {
        Data zp_base = read_byte(bus, pc++, cycles);
        Addr pointer_addr = static_cast<Addr>(zp_base + regs[static_cast<size_t>(Reg::X)]) & 0xFF;
        Data p_low  = read_byte(bus, pointer_addr, cycles);
        Data p_high = read_byte(bus, static_cast<Addr>(pointer_addr + 1) & 0xFF, cycles);
        return (static_cast<Addr>(p_high) << 8) | p_low;
    }
    template <typename BusType>
    inline Addr fetch_izy(BusType& bus, uint32_t& cycles) {
        Data zp = read_byte(bus, pc++, cycles);
        Data p_low  = read_byte(bus, static_cast<Addr>(zp), cycles);
        Data p_high = read_byte(bus, static_cast<Addr>(zp + 1) & 0xFF, cycles);
        Addr base = (static_cast<Addr>(p_high) << 8) | p_low;
        Addr result = base + regs[static_cast<size_t>(Reg::Y)];
        if ((base & 0xFF00) != (result & 0xFF00)) cycles += 1;
        return result;
    }
    template <typename BusType>
    inline Addr fetch_relative(BusType& bus, uint32_t& cycles) {
        auto offset = static_cast<std::make_signed_t<Data>>(read_byte(bus, pc++, cycles));
        return static_cast<Addr>(pc + offset);
    }

    // ---- Handler Type ----
    template <typename BusType>
    using Handler = void(CPUCore::*)(BusType&, uint32_t&);

    // ================================================================
    // Individual Opcode Handlers
    // ================================================================

    template <typename BusType> void h_nop(BusType&, uint32_t&) {}

    // ---- External Hardware Interrupt Signals ----
    template <typename BusType>
    void signal_nmi(BusType& bus, uint32_t& cycles) {
        // NMI: Vector $FFFA-$FFFB, Break flag = false, I-flag unaffected
        trigger_interrupt(bus, cycles, 0xFFFA, false, true);
    }

    template <typename BusType>
    void signal_irq(BusType& bus, uint32_t& cycles) {
        // IRQ: Only triggers if Interrupt Disable flag (I) is clear
        if (!get_flag(Flag6502::Interrupt)) {
            // Vector $FFFE-$FFFF, Break flag = false, I-flag set = true
            trigger_interrupt(bus, cycles, 0xFFFE, false, true);
        }
    }

    template <typename BusType>
    void h_brk(BusType& bus, uint32_t& cycles) {
        pc++; // BRK skips the signature/padding byte
        trigger_interrupt(bus, cycles, 0xFFFE, true, true);
    }

    // -- Loads --
    template <typename BusType>
    void h_lda_imm(BusType& bus , uint32_t& cycles) {
        Data val = read_byte(bus, fetch_immediate(), cycles);
        set_reg(Reg::A, val); update_nz_flags(val);
    }
    template <typename BusType>
    void h_lda(BusType& bus, uint32_t& cycles, Addr ea) {
        Data val = read_byte(bus, ea, cycles);
        set_reg(Reg::A, val); update_nz_flags(val);
    }
    template <typename BusType>
    void h_ldx_imm(BusType& bus, uint32_t& cycles) {
        Data val = read_byte(bus, fetch_immediate(), cycles);
        set_reg(Reg::X, val); update_nz_flags(val);
    }
    template <typename BusType>
    void h_ldx(BusType& bus, uint32_t& cycles, Addr ea) {
        Data val = read_byte(bus, ea, cycles);
        set_reg(Reg::X, val); update_nz_flags(val);
    }
    template <typename BusType>
    void h_ldy_imm(BusType& bus, uint32_t& cycles) {
        Data val = read_byte(bus, fetch_immediate(), cycles);
        set_reg(Reg::Y, val); update_nz_flags(val);
    }
    template <typename BusType>
    void h_ldy(BusType& bus, uint32_t& cycles, Addr ea) {
        Data val = read_byte(bus, ea, cycles);
        set_reg(Reg::Y, val); update_nz_flags(val);
    }
    template <typename BusType>
    void h_lda_izx(BusType& bus, uint32_t& cycles) {
        // 1. Fetch zero-page base offset
        uint8_t zp_base = read_byte(bus, pc++, cycles);

        // 2. Add X with Zero Page wrapping ($00-$FF)
        uint8_t ptr = static_cast<uint8_t>(zp_base + get_reg(Reg::X));

        // 3. Read effective address vector from Zero Page (handling byte wraparound for high byte)
        uint8_t low  = read_byte(bus, ptr, cycles);
        uint8_t high = read_byte(bus, static_cast<uint8_t>(ptr + 1), cycles);
        Addr effective_addr = static_cast<Addr>(low | (high << 8));
        Data val = read_byte(bus, effective_addr, cycles);
        // 4. Load byte into A and set N/Z flags
        set_reg(Reg::A, val); update_nz_flags(val);
    }

    // -- Stores --
    template <typename BusType>
    void h_sta(BusType& bus, uint32_t& cycles, Addr ea) { write_byte(bus, ea, get_reg(Reg::A), cycles); }
    template <typename BusType>
    void h_stx(BusType& bus, uint32_t& cycles, Addr ea) { write_byte(bus, ea, get_reg(Reg::X), cycles); }
    template <typename BusType>
    void h_sty(BusType& bus, uint32_t& cycles, Addr ea) { write_byte(bus, ea, get_reg(Reg::Y), cycles); }

    // -- Transfers --
    template <typename BusType>
    void h_tax(BusType&, uint32_t&) { set_reg(Reg::X, get_reg(Reg::A)); update_nz_flags(get_reg(Reg::X)); }
    template <typename BusType>
    void h_tay(BusType&, uint32_t&) { set_reg(Reg::Y, get_reg(Reg::A)); update_nz_flags(get_reg(Reg::Y)); }
    template <typename BusType>
    void h_txa(BusType&, uint32_t&) { set_reg(Reg::A, get_reg(Reg::X)); update_nz_flags( get_reg(Reg::A)); }
    template <typename BusType>
    void h_tya(BusType&, uint32_t&) { set_reg(Reg::A, get_reg(Reg::Y)); update_nz_flags( get_reg(Reg::A)); }
    template <typename BusType>
    void h_tsx(BusType&, uint32_t&) { set_reg(Reg::X, get_reg(Reg::SP)); update_nz_flags( get_reg(Reg::X)); }
    template <typename BusType>
    void h_txs(BusType&, uint32_t&) { regs[static_cast<size_t>(Reg::SP)] = get_reg(Reg::X); }

    // -- Stack --
    template <typename BusType>
    void h_pha(BusType& bus, uint32_t& cycles) {
        write_byte(bus, get_push_address(), get_reg(Reg::A), cycles);
        regs[static_cast<size_t>(Reg::SP)]--;
    }
    template <typename BusType>
    void h_php(BusType& bus, uint32_t& cycles) {
        write_byte(bus, get_push_address(), status | Flag6502::Break | Flag6502::Unused, cycles);
        regs[static_cast<size_t>(Reg::SP)]--;
    }
    template <typename BusType>
    void h_pla(BusType& bus, uint32_t& cycles) {
        Data val = read_byte(bus, get_pull_address(), cycles);
        set_reg(Reg::A, val); update_nz_flags(val);
    }
    template <typename BusType>
    void h_plp(BusType& bus, uint32_t& cycles) {
        Data val = read_byte(bus, get_pull_address(), cycles);
        // Clear old flags and assign pulled value, ensuring Unused bit is always 1 
        // and Break flag behavior matches the hardware standard when pulled.
        status = (val & ~Flag6502::Break) | Flag6502::Unused;
    }

    // -- Inc/Dec --
    // -- Inc/Dec --
    template <typename BusType>
    void h_inx(BusType&, uint32_t&) {
        Data v = static_cast<Data>(get_reg(Reg::X) + 1);
        set_reg(Reg::X, v);
        update_nz_flags(v);
    }

    template <typename BusType>
    void h_iny(BusType&, uint32_t&) {
        Data v = static_cast<Data>(get_reg(Reg::Y) + 1);
        set_reg(Reg::Y, v);
        update_nz_flags(v);
    }

    template <typename BusType>
    void h_dex(BusType&, uint32_t&) {
        Data v = static_cast<Data>(get_reg(Reg::X) - 1);
        set_reg(Reg::X, v);
        update_nz_flags(v);
    }

    template <typename BusType>
    void h_dey(BusType&, uint32_t&) {
        Data v = static_cast<Data>(get_reg(Reg::Y) - 1);
        set_reg(Reg::Y, v);
        update_nz_flags(v);
    }

    template <typename BusType>
    void h_inc(BusType& bus, uint32_t& cycles, Addr ea) {
        Data v = static_cast<Data>(read_byte(bus, ea, cycles) + 1);
        write_byte(bus, ea, v, cycles);
        update_nz_flags(v);
    }

    template <typename BusType>
    void h_dec(BusType& bus, uint32_t& cycles, Addr ea) {
        Data v = static_cast<Data>(read_byte(bus, ea, cycles) - 1);
        write_byte(bus, ea, v, cycles);
        update_nz_flags(v);
    }

    // -- ALU (immediate variants) --
    template <typename BusType>
    void h_and_imm(BusType& bus, uint32_t& cycles) {
        Data val = read_byte(bus, fetch_immediate(), cycles);
        Data r = static_cast<Data>(get_reg(Reg::A) & val);
        set_reg(Reg::A, r);
        update_nz_flags(r);
    }
    template <typename BusType>
    void h_and(BusType& bus, uint32_t& cycles, Addr ea) {
        Data val = read_byte(bus, ea, cycles);
        Data r = static_cast<Data>(get_reg(Reg::A) & val);
        set_reg(Reg::A, r);
        update_nz_flags(r);
    }
    template <typename BusType>
    void h_ora_imm(BusType& bus, uint32_t& cycles) {
        Data val = read_byte(bus, fetch_immediate(), cycles);
        Data r = static_cast<Data>(get_reg(Reg::A) | val);
        set_reg(Reg::A, r);
        update_nz_flags(r);
    }
    template <typename BusType>
    void h_ora(BusType& bus, uint32_t& cycles, Addr ea) {
        Data val = read_byte(bus, ea, cycles);
        Data r = static_cast<Data>(get_reg(Reg::A) | val);
        set_reg(Reg::A, r);
        update_nz_flags(r);
    }
    template <typename BusType>
    void h_eor_imm(BusType& bus, uint32_t& cycles) {
        Data val = read_byte(bus, fetch_immediate(), cycles);
        Data r = static_cast<Data>(get_reg(Reg::A) ^ val);
        set_reg(Reg::A, r);
        update_nz_flags(r);
    }
    template <typename BusType>
    void h_eor(BusType& bus, uint32_t& cycles, Addr ea) {
        Data val = read_byte(bus, ea, cycles);
        Data r = static_cast<Data>(get_reg(Reg::A) ^ val);
        set_reg(Reg::A, r);
        update_nz_flags(r);
    }
    template <typename BusType>
    void h_cmp_imm(BusType& bus, uint32_t& cycles) {
        Data val = read_byte(bus, fetch_immediate(), cycles);
        Data a = get_reg(Reg::A);
        Data r = static_cast<Data>(static_cast<uint16_t>(a) - static_cast<uint16_t>(val));
        set_flag(Flag6502::Carry, a >= val);
        update_nz_flags(r);
    }
    template <typename BusType>
    void h_cmp(BusType& bus, uint32_t& cycles, Addr ea) {
        Data val = read_byte(bus, ea, cycles);
        Data a = get_reg(Reg::A);
        Data r = static_cast<Data>(static_cast<uint16_t>(a) - static_cast<uint16_t>(val));
        set_flag(Flag6502::Carry, a >= val);
        update_nz_flags(r);
    }
    template <typename BusType>
    void h_cpx_imm(BusType& bus, uint32_t& cycles) {
        Data val = read_byte(bus, fetch_immediate(), cycles);
        Data x = get_reg(Reg::X);
        Data r = static_cast<Data>(static_cast<uint16_t>(x) - static_cast<uint16_t>(val));
        set_flag(Flag6502::Carry, x >= val);
        update_nz_flags(r);
    }
    template <typename BusType>
    void h_cpx(BusType& bus, uint32_t& cycles, Addr ea) {
        Data val = read_byte(bus, ea, cycles);
        Data x = get_reg(Reg::X);
        Data r = static_cast<Data>(static_cast<uint16_t>(x) - static_cast<uint16_t>(val));
        set_flag(Flag6502::Carry, x >= val);
        update_nz_flags(r);
    }
    template <typename BusType>
    void h_cpy_imm(BusType& bus, uint32_t& cycles) {
        Data val = read_byte(bus, fetch_immediate(), cycles);
        Data y = get_reg(Reg::Y);
        Data r = static_cast<Data>(static_cast<uint16_t>(y) - static_cast<uint16_t>(val));
        set_flag(Flag6502::Carry, y >= val);
        update_nz_flags(r);
    }
    template <typename BusType>
    void h_cpy(BusType& bus, uint32_t& cycles, Addr ea) {
        Data val = read_byte(bus, ea, cycles);
        Data y = get_reg(Reg::Y);
        Data r = static_cast<Data>(static_cast<uint16_t>(y) - static_cast<uint16_t>(val));
        set_flag(Flag6502::Carry, y >= val);
        update_nz_flags(r);
    }
    template <typename BusType>
    void h_adc_imm(BusType& bus, uint32_t& cycles) {
        Data val = read_byte(bus, fetch_immediate(), cycles);
        Data a = get_reg(Reg::A);
        uint16_t c = get_flag(Flag6502::Carry) ? 1 : 0;
        
        if (get_flag(Flag6502::Decimal)) {
            // BCD Decimal Mode Addition
            int al = (a & 0x0F) + (val & 0x0F) + c;
            int ah = (a >> 4) + (val >> 4);
            if (al > 0x09) {
                al += 0x06;
                ah += 0x01;
            }
            uint16_t bin_sum = static_cast<uint16_t>(a) + static_cast<uint16_t>(val) + c;
            set_flag(Flag6502::Overflow, ((a ^ val) & 0x80) == 0 && ((a ^ static_cast<Data>(bin_sum)) & 0x80) != 0);
            
            if (ah > 0x09) {
                ah += 0x06;
            }
            set_flag(Flag6502::Carry, ah > 0x0F);
            
            Data r = static_cast<Data>((ah << 4) | (al & 0x0F));
            set_reg(Reg::A, r);
            update_nz_flags(r);
        } else {
            // Standard Binary Addition
            uint16_t sum = static_cast<uint16_t>(a) + static_cast<uint16_t>(val) + c;
            set_flag(Flag6502::Carry, sum > 0xFF);
            set_flag(Flag6502::Overflow, ((a ^ val) & 0x80) == 0 && ((a ^ static_cast<Data>(sum)) & 0x80) != 0);
            Data r = static_cast<Data>(sum);
            set_reg(Reg::A, r);
            update_nz_flags(r);
        }
    }
    template <typename BusType>
    void h_adc(BusType& bus, uint32_t& cycles, Addr ea) {
        Data val = read_byte(bus, ea, cycles);
        Data a = get_reg(Reg::A);
        uint16_t c = get_flag(Flag6502::Carry) ? 1 : 0;
        
        if (get_flag(Flag6502::Decimal)) {
            // BCD Addition
            int al = (a & 0x0F) + (val & 0x0F) + c;
            int ah = (a >> 4) + (val >> 4);
            if (al > 0x09) {
                al += 0x06;
                ah += 0x01;
            }
            // Overflow for decimal mode uses binary operands before decimal adjustment
            uint16_t bin_sum = static_cast<uint16_t>(a) + static_cast<uint16_t>(val) + c;
            set_flag(Flag6502::Overflow, ((a ^ val) & 0x80) == 0 && ((a ^ static_cast<Data>(bin_sum)) & 0x80) != 0);
            
            if (ah > 0x09) {
                ah += 0x06;
            }
            set_flag(Flag6502::Carry, ah > 0x0F);
            
            Data r = static_cast<Data>((ah << 4) | (al & 0x0F));
            set_reg(Reg::A, r);
            update_nz_flags(r); // Note: On RP2A03 (NES), N/Z flags are based on binary result, 
                                // but standard MOS 6502 uses the decimal result. Adjust if needed.
        } else {
            // Standard Binary Addition
            uint16_t sum = static_cast<uint16_t>(a) + static_cast<uint16_t>(val) + c;
            set_flag(Flag6502::Carry, sum > 0xFF);
            set_flag(Flag6502::Overflow, ((a ^ val) & 0x80) == 0 && ((a ^ static_cast<Data>(sum)) & 0x80) != 0);
            Data r = static_cast<Data>(sum);
            set_reg(Reg::A, r);
            update_nz_flags(r);
        }
    }
    template <typename BusType>
    void h_sbc_imm(BusType& bus, uint32_t& cycles) {
        Data val = read_byte(bus, fetch_immediate(), cycles);
        Data a = get_reg(Reg::A);
        uint16_t borrow = get_flag(Flag6502::Carry) ? 0 : 1;
        
        if (get_flag(Flag6502::Decimal)) {
            // BCD Decimal Mode Subtraction
            int al = (a & 0x0F) - (val & 0x0F) - borrow;
            int ah = (a >> 4) - (val >> 4);
            if (al < 0) {
                al -= 0x06;
                ah -= 0x01;
            }
            if (ah < 0) {
                ah -= 0x06;
            }
            
            uint16_t bin_diff = static_cast<uint16_t>(a) - static_cast<uint16_t>(val) - borrow;
            set_flag(Flag6502::Overflow, ((a ^ val) & 0x80) != 0 && ((a ^ static_cast<Data>(bin_diff)) & 0x80) != 0);
            set_flag(Flag6502::Carry, bin_diff <= 0xFF);
            
            Data r = static_cast<Data>((ah << 4) | (al & 0x0F));
            set_reg(Reg::A, r);
            update_nz_flags(r);
        } else {
            // Standard Binary Subtraction
            uint16_t diff = static_cast<uint16_t>(a) - static_cast<uint16_t>(val) - borrow;
            set_flag(Flag6502::Carry, diff <= 0xFF);
            set_flag(Flag6502::Overflow, ((a ^ val) & 0x80) != 0 && ((a ^ static_cast<Data>(diff)) & 0x80) != 0);
            Data r = static_cast<Data>(diff);
            set_reg(Reg::A, r);
            update_nz_flags(r);
        }
    }
    template <typename BusType>
    void h_sbc(BusType& bus, uint32_t& cycles, Addr ea) {
        Data val = read_byte(bus, ea, cycles);
        Data a = get_reg(Reg::A);
        uint16_t borrow = get_flag(Flag6502::Carry) ? 0 : 1;
        
        if (get_flag(Flag6502::Decimal)) {
            // BCD Subtraction
            int al = (a & 0x0F) - (val & 0x0F) - borrow;
            int ah = (a >> 4) - (val >> 4);
            if (al < 0) {
                al -= 0x06;
                ah -= 0x01;
            }
            if (ah < 0) {
                ah -= 0x06;
            }
            
            // Standard binary difference for overflow calculation
            uint16_t bin_diff = static_cast<uint16_t>(a) - static_cast<uint16_t>(val) - borrow;
            set_flag(Flag6502::Overflow, ((a ^ val) & 0x80) != 0 && ((a ^ static_cast<Data>(bin_diff)) & 0x80) != 0);
            set_flag(Flag6502::Carry, bin_diff <= 0xFF);
            
            Data r = static_cast<Data>((ah << 4) | (al & 0x0F));
            set_reg(Reg::A, r);
            update_nz_flags(r);
        } else {
            // Standard Binary Subtraction
            uint16_t diff = static_cast<uint16_t>(a) - static_cast<uint16_t>(val) - borrow;
            set_flag(Flag6502::Carry, diff <= 0xFF);
            set_flag(Flag6502::Overflow, ((a ^ val) & 0x80) != 0 && ((a ^ static_cast<Data>(diff)) & 0x80) != 0);
            Data r = static_cast<Data>(diff);
            set_reg(Reg::A, r);
            update_nz_flags(r);
        }
    }

    // -- Branches --
    template <typename BusType>
    void h_branch(BusType& bus, uint32_t& cycles, bool cond) {

        // 1. Fetch offset and interpret as signed 8-bit integer
        int8_t offset = static_cast<int8_t>(read_byte(bus, pc++, cycles));

        if (cond) {
            // 2. Add signed offset to unsigned PC, explicitly casting back to 16-bit Addr
            // (In C++, pc + offset promotes to 32-bit signed int; static_cast<Addr> cleanly handles 16-bit wrap)
            Addr target = static_cast<Addr>(pc + offset);

            // 3. Page boundary check (+2 cycles if page crossed, +1 if target is on same page)
            if ((pc & 0xFF00) != (target & 0xFF00)) {
                cycles += 2;
            } else {
                cycles += 1;
            }

            pc = target;
        }
    }
    template <typename BusType> void h_bne(BusType& bus, uint32_t& cycles) { h_branch(bus, cycles, !get_flag(Flag6502::Zero)); }
    template <typename BusType> void h_beq(BusType& bus, uint32_t& cycles) { h_branch(bus, cycles,  get_flag(Flag6502::Zero)); }
    template <typename BusType> void h_bcc(BusType& bus, uint32_t& cycles) { h_branch(bus, cycles, !get_flag(Flag6502::Carry)); }
    template <typename BusType> void h_bcs(BusType& bus, uint32_t& cycles) { h_branch(bus, cycles,  get_flag(Flag6502::Carry)); }
    template <typename BusType> void h_bpl(BusType& bus, uint32_t& cycles) { h_branch(bus, cycles, !get_flag(Flag6502::Negative)); }
    template <typename BusType> void h_bmi(BusType& bus, uint32_t& cycles) { h_branch(bus, cycles,  get_flag(Flag6502::Negative)); }
    template <typename BusType> void h_bvc(BusType& bus, uint32_t& cycles) { h_branch(bus, cycles, !get_flag(Flag6502::Overflow)); }
    template <typename BusType> void h_bvs(BusType& bus, uint32_t& cycles) { h_branch(bus, cycles,  get_flag(Flag6502::Overflow)); }

    // -- Flag ops --
    template <typename BusType> void h_clc(BusType&, uint32_t&) { set_flag(Flag6502::Carry, false); }
    template <typename BusType> void h_sec(BusType&, uint32_t&) { set_flag(Flag6502::Carry, true); }
    template <typename BusType> void h_cld(BusType&, uint32_t&) { set_flag(Flag6502::Decimal, false); }
    template <typename BusType> void h_sed(BusType&, uint32_t&) { set_flag(Flag6502::Decimal, true); }
    template <typename BusType> void h_cli(BusType&, uint32_t&) { set_flag(Flag6502::Interrupt, false); }
    template <typename BusType> void h_sei(BusType&, uint32_t&) { set_flag(Flag6502::Interrupt, true); }
    template <typename BusType> void h_clv(BusType&, uint32_t&) { set_flag(Flag6502::Overflow, false); }

    // -- JMP / JSR / RTS / RTI --
    template <typename BusType>
    void h_jmp_abs(BusType& bus, uint32_t& cycles) { pc = fetch_absolute(bus, cycles); }
    template <typename BusType>
    void h_jmp_ind(BusType& bus, uint32_t& cycles) { pc = fetch_indirect(bus, cycles); }
    template <typename BusType>
    void h_jsr(BusType& bus, uint32_t& cycles) {
        Data target_low  = read_byte(bus, pc++, cycles);
        Data target_high = read_byte(bus, pc++, cycles);
        Addr target = (static_cast<Addr>(target_high) << 8) | target_low;
        Addr return_addr = pc - 1;
        write_byte(bus, get_push_address(), static_cast<Data>(return_addr >> 8), cycles);
        regs[static_cast<size_t>(Reg::SP)]--;
        write_byte(bus, get_push_address(), static_cast<Data>(return_addr & 0xFF), cycles);
        regs[static_cast<size_t>(Reg::SP)]--;
        pc = target;
    }
    template <typename BusType>
    void h_rts(BusType& bus, uint32_t& cycles) {
        Data ret_low  = read_byte(bus, get_pull_address(), cycles);
        Data ret_high = read_byte(bus, get_pull_address(), cycles);
        pc = ((static_cast<Addr>(ret_high) << 8) | ret_low) + 1;
    }
    template <typename BusType>
    void h_rti(BusType& bus, uint32_t& cycles) {
        Data val = read_byte(bus, get_pull_address(), cycles);
        status = (val & ~Flag6502::Break) | Flag6502::Unused;
        Data ret_low  = read_byte(bus, get_pull_address(), cycles);
        Data ret_high = read_byte(bus, get_pull_address(), cycles);
        pc = (static_cast<Addr>(ret_high) << 8) | ret_low;
    }

    // -- BIT --
    template <typename BusType>
    void h_bit_zp(BusType& bus, uint32_t& cycles) {
        Addr addr = fetch_zero_page(bus, cycles);
        Data val = read_byte(bus, addr, cycles);
        set_flag(Flag6502::Zero, (get_reg(Reg::A) & val) == 0);
        set_flag(Flag6502::Overflow, (val & 0x40) != 0);
        set_flag(Flag6502::Negative, (val & 0x80) != 0);
    }
    template <typename BusType>
    void h_bit_abs(BusType& bus, uint32_t& cycles) {
        Addr addr = fetch_absolute(bus, cycles);
        Data val = read_byte(bus, addr, cycles);
        set_flag(Flag6502::Zero, (get_reg(Reg::A) & val) == 0);
        set_flag(Flag6502::Overflow, (val & 0x40) != 0);
        set_flag(Flag6502::Negative, (val & 0x80) != 0);
    }

    // -- Shifts/Rotates (accumulator) --
    template <typename BusType>
    void h_asl_acc(BusType&, uint32_t&) {
        Data v = get_reg(Reg::A);
        set_flag(Flag6502::Carry, (v & 0x80) != 0);
        v <<= 1; set_reg(Reg::A, v); update_nz_flags(v);
    }
    template <typename BusType>
    void h_lsr_acc(BusType&, uint32_t&) {
        Data v = get_reg(Reg::A);
        set_flag(Flag6502::Carry, (v & 0x01) != 0);
        v >>= 1; set_reg(Reg::A, v); update_nz_flags(v);
    }
    template <typename BusType>
    void h_rol_acc(BusType&, uint32_t&) {
        Data v = get_reg(Reg::A);
        bool old_carry = get_flag(Flag6502::Carry);
        set_flag(Flag6502::Carry, (v & 0x80) != 0);
        v = static_cast<Data>((v << 1) | (old_carry ? 1 : 0));
        set_reg(Reg::A, v); update_nz_flags(v);
    }
    template <typename BusType>
    void h_ror_acc(BusType&, uint32_t&) {
        Data v = get_reg(Reg::A);
        bool old_carry = get_flag(Flag6502::Carry);
        set_flag(Flag6502::Carry, (v & 0x01) != 0);
        v = static_cast<Data>((v >> 1) | (old_carry ? 0x80 : 0));
        set_reg(Reg::A, v); update_nz_flags(v);
    }

    // -- Shifts/Rotates (memory) --
    template <typename BusType>
    void h_asl(BusType& bus, uint32_t& cycles, Addr addr) {
        Data v = read_byte(bus, addr, cycles);
        set_flag(Flag6502::Carry, (v & 0x80) != 0);
        v <<= 1; write_byte(bus, addr, v, cycles); update_nz_flags(v);
    }
    template <typename BusType>
    void h_lsr(BusType& bus, uint32_t& cycles, Addr addr) {
        Data v = read_byte(bus, addr, cycles);
        set_flag(Flag6502::Carry, (v & 0x01) != 0);
        v >>= 1; write_byte(bus, addr, v, cycles); update_nz_flags(v);
    }
    template <typename BusType>
    void h_rol(BusType& bus, uint32_t& cycles, Addr addr) {
        Data v = read_byte(bus, addr, cycles);
        bool old_carry = get_flag(Flag6502::Carry);
        set_flag(Flag6502::Carry, (v & 0x80) != 0);
        v = static_cast<Data>((v << 1) | (old_carry ? 1 : 0));
        write_byte(bus, addr, v, cycles); update_nz_flags(v);
    }
    template <typename BusType>
    void h_ror(BusType& bus, uint32_t& cycles, Addr addr) {
        Data v = read_byte(bus, addr, cycles);
        bool old_carry = get_flag(Flag6502::Carry);
        set_flag(Flag6502::Carry, (v & 0x01) != 0);
        v = static_cast<Data>((v >> 1) | (old_carry ? 0x80 : 0));
        write_byte(bus, addr, v, cycles); update_nz_flags(v);
    }

    // ---- Handler Table Builder ----
    template <typename BusType>
    static constexpr std::array<Handler<BusType>, 256> build_handler_table() {
        std::array<Handler<BusType>, 256> t{};
        auto set = [&](uint8_t op, Handler<BusType> h) { t[op] = h; };

        set(0x00, &CPUCore::template h_brk<BusType>);
        set(0x08, &CPUCore::template h_php<BusType>);
        set(0x09, &CPUCore::template h_ora_imm<BusType>);
        set(0x0A, &CPUCore::template h_asl_acc<BusType>);

        set(0x10, &CPUCore::template h_bpl<BusType>);
        set(0x18, &CPUCore::template h_clc<BusType>);

        set(0x20, &CPUCore::template h_jsr<BusType>);
        set(0x24, &CPUCore::template h_bit_zp<BusType>);
        set(0x28, &CPUCore::template h_plp<BusType>);
        set(0x29, &CPUCore::template h_and_imm<BusType>);
        set(0x2A, &CPUCore::template h_rol_acc<BusType>);
        set(0x2C, &CPUCore::template h_bit_abs<BusType>);

        set(0x30, &CPUCore::template h_bmi<BusType>);
        set(0x38, &CPUCore::template h_sec<BusType>);

        set(0x40, &CPUCore::template h_rti<BusType>);
        set(0x48, &CPUCore::template h_pha<BusType>);
        set(0x49, &CPUCore::template h_eor_imm<BusType>);
        set(0x4A, &CPUCore::template h_lsr_acc<BusType>);
        set(0x4C, &CPUCore::template h_jmp_abs<BusType>);

        set(0x50, &CPUCore::template h_bvc<BusType>);
        set(0x58, &CPUCore::template h_cli<BusType>);

        set(0x60, &CPUCore::template h_rts<BusType>);
        set(0x68, &CPUCore::template h_pla<BusType>);
        set(0x69, &CPUCore::template h_adc_imm<BusType>);
        set(0x6A, &CPUCore::template h_ror_acc<BusType>);
        set(0x6C, &CPUCore::template h_jmp_ind<BusType>);

        set(0x70, &CPUCore::template h_bvs<BusType>);
        set(0x78, &CPUCore::template h_sei<BusType>);

        set(0x88, &CPUCore::template h_dey<BusType>);
        set(0x8A, &CPUCore::template h_txa<BusType>);

        set(0x90, &CPUCore::template h_bcc<BusType>);
        set(0x98, &CPUCore::template h_tya<BusType>);
        set(0x9A, &CPUCore::template h_txs<BusType>);

        set(0xA1, &CPUCore::template h_lda_izx<BusType>);
        set(0xA2, &CPUCore::template h_ldx_imm<BusType>);
        set(0xA8, &CPUCore::template h_tay<BusType>);
        set(0xA9, &CPUCore::template h_lda_imm<BusType>);
        set(0xAA, &CPUCore::template h_tax<BusType>);

        set(0xB0, &CPUCore::template h_bcs<BusType>);
        set(0xB8, &CPUCore::template h_clv<BusType>);
        set(0xBA, &CPUCore::template h_tsx<BusType>);

        set(0xC0, &CPUCore::template h_cpy_imm<BusType>);
        set(0xC8, &CPUCore::template h_iny<BusType>);
        set(0xC9, &CPUCore::template h_cmp_imm<BusType>);
        set(0xCA, &CPUCore::template h_dex<BusType>);

        set(0xD0, &CPUCore::template h_bne<BusType>);
        set(0xD8, &CPUCore::template h_cld<BusType>);

        set(0xE0, &CPUCore::template h_cpx_imm<BusType>);
        set(0xE8, &CPUCore::template h_inx<BusType>);
        set(0xE9, &CPUCore::template h_sbc_imm<BusType>);
        set(0xEA, &CPUCore::template h_nop<BusType>);

        set(0xF0, &CPUCore::template h_beq<BusType>);
        set(0xF8, &CPUCore::template h_sed<BusType>);

        return t;
    }

public:
    // ---- Step Dispatch ----
    template <typename BusType>
    void step(BusType& bus, uint32_t& cycle_accumulator) {
        static_assert(SystemBusType<BusType, Config>);

        // 1. Poll Interrupts FIRST (Pre-Fetch)
        if (bus.consume_nmi()) {
            signal_nmi(bus, cycle_accumulator);
            print_cpu_trace(bus, pc, TraceContext::VectorNMI);
            return;
        }

        if (bus.get_irq_line() && !get_flag(Flag6502::Interrupt)) {
            signal_irq(bus, cycle_accumulator);
            print_cpu_trace(bus, pc, TraceContext::VectorIRQ);
            return;
        }

        // 2. Execute Instruction
        const Addr executed_pc = pc;
        uint32_t start = cycle_accumulator;
        Data opcode = read_byte(bus, pc++, cycle_accumulator);
        const auto& info = kOpcodeTable[opcode >> 4][opcode & 0x0F];

        if (info.instr == Instruction::XXX) {
            uint32_t elapsed = cycle_accumulator - start;
            if (elapsed < 2) cycle_accumulator += (2 - elapsed);
        } else {
            static constexpr auto handlers = build_handler_table<BusType>();

            if (auto handler = handlers[opcode]; handler) {
                (this->*handler)(bus, cycle_accumulator);
            } else {
                dispatch_generic(bus, cycle_accumulator, opcode, info);
            }

            uint32_t elapsed = cycle_accumulator - start;
            if (elapsed < info.cycles) {
                cycle_accumulator += (info.cycles - elapsed);
            }
        }

        // 3. Log Executed Instruction
        print_cpu_trace(bus, executed_pc, TraceContext::Exec);
    }

private:
    // ---- Unified Interrupt / Trap Core Routine ----
    template <typename BusType>
    void trigger_interrupt(BusType& bus, uint32_t& cycles, Addr vector_addr, bool set_break_flag, bool set_i_flag) {
        const uint32_t start_cycles = cycles;

        // 1. Push PC high and low bytes
        write_byte(bus, get_push_address(), static_cast<Data>(pc >> 8), cycles);
        regs[static_cast<size_t>(Reg::SP)]--;
 
        write_byte(bus, get_push_address(), static_cast<Data>(pc & 0xFF), cycles);
        regs[static_cast<size_t>(Reg::SP)]--;
 
        // 2. Push Status Register with appropriate Break flag handling
        Data flags_to_push = (status & ~Flag6502::Break) | Flag6502::Unused;
        if (set_break_flag) {
            flags_to_push |= Flag6502::Break;
        }
        write_byte(bus, get_push_address(), flags_to_push, cycles);
        regs[static_cast<size_t>(Reg::SP)]--;
 
        // 3. Set Interrupt Disable flag if requested (true for BRK, IRQ, and NMI)
        if (set_i_flag) {
            set_flag(Flag6502::Interrupt, true);
        }
 
        // 4. Fetch vector and load PC
        Data vector_low  = read_byte(bus, vector_addr, cycles);
        Data vector_high = read_byte(bus, static_cast<Addr>(vector_addr + 1), cycles);
        pc = (static_cast<Addr>(vector_high) << 8) | vector_low;
 
        // 5. Precise cycle clamp:
        // - BRK (set_break_flag = true): 6 cycles here (+1 opcode fetch in step() = 7 total)
        // - Hardware IRQ / NMI (set_break_flag = false): 7 cycles here (no opcode fetch = 7 total)
        const uint32_t target_cycles = set_break_flag ? 6 : 7;
        cycles = start_cycles + target_cycles;
    }

    // ---- Generic Dispatch ----
    template <typename BusType>
    void dispatch_generic(BusType& bus, uint32_t& cycles, Data, const OpcodeInfo& info) {
        Addr ea = 0;

        switch (info.mode) {
            case AddressingMode::IMP: case AddressingMode::ACC: break;
            case AddressingMode::IMM: ea = fetch_immediate(); break;
            case AddressingMode::ZP:  ea = fetch_zero_page(bus, cycles); break;
            case AddressingMode::ZPX: ea = fetch_zero_page_x(bus, cycles); break;
            case AddressingMode::ZPY: ea = fetch_zero_page_y(bus, cycles); break;
            case AddressingMode::ABS: ea = fetch_absolute(bus, cycles); break;
            case AddressingMode::ABX: ea = fetch_absolute_x(bus, cycles); break;
            case AddressingMode::ABY: ea = fetch_absolute_y(bus, cycles); break;
            case AddressingMode::IZX: ea = fetch_izx(bus, cycles); break;
            case AddressingMode::IZY: ea = fetch_izy(bus, cycles); break;
            case AddressingMode::IND: ea = fetch_indirect(bus, cycles); break;
            case AddressingMode::REL: ea = fetch_relative(bus, cycles); break;
        }

        switch (info.instr) {
            case Instruction::LDA: h_lda(bus, cycles, ea); break;
            case Instruction::LDX: h_ldx(bus, cycles, ea); break;
            case Instruction::LDY: h_ldy(bus, cycles, ea); break;
            case Instruction::STA: h_sta(bus, cycles, ea); break;
            case Instruction::STX: h_stx(bus, cycles, ea); break;
            case Instruction::STY: h_sty(bus, cycles, ea); break;
            case Instruction::AND: h_and(bus, cycles, ea); break;
            case Instruction::ORA: h_ora(bus, cycles, ea); break;
            case Instruction::EOR: h_eor(bus, cycles, ea); break;
            case Instruction::CMP: h_cmp(bus, cycles, ea); break;
            case Instruction::CPX: h_cpx(bus, cycles, ea); break;
            case Instruction::CPY: h_cpy(bus, cycles, ea); break;
            case Instruction::ADC: h_adc(bus, cycles, ea); break;
            case Instruction::SBC: h_sbc(bus, cycles, ea); break;
            case Instruction::INC: h_inc(bus, cycles, ea); break;
            case Instruction::DEC: h_dec(bus, cycles, ea); break;
            case Instruction::ASL: h_asl(bus, cycles, ea); break;
            case Instruction::LSR: h_lsr(bus, cycles, ea); break;
            case Instruction::ROL: h_rol(bus, cycles, ea); break;
            case Instruction::ROR: h_ror(bus, cycles, ea); break;
            case Instruction::BIT: {
                Data val = read_byte(bus, ea, cycles);
                Data a_val = get_reg(Reg::A);

                // Z flag set if (A & M) == 0
                set_flag(Flag6502::Zero, (a_val & val) == 0);

                // V flag gets bit 6 of fetched memory value
                set_flag(Flag6502::Overflow, (val & Flag6502::Overflow) != 0);

                // N flag gets bit 7 of fetched memory value
                set_flag(Flag6502::Negative, (val & Flag6502::Negative) != 0);
                break;
            }
            case Instruction::JMP:
                pc = ea;
                break;
            default: break;
        }
    }

    template <typename BusType>
    void print_cpu_trace(BusType& bus, Addr trace_pc, TraceContext ctx = TraceContext::Exec) {
        if constexpr (requires { { Config::kEnableTrace } -> std::convertible_to<bool>; }) {
            if constexpr (!Config::kEnableTrace) return;
        }

        if (!trace_enabled_) return;

        const char* tag = "[EXEC]   ";
        switch (ctx) {
            case TraceContext::Exec:        tag = "[EXEC]   "; break;
            case TraceContext::VectorIRQ:   tag = "[IRQ VEC]"; break;
            case TraceContext::VectorNMI:   tag = "[NMI VEC]"; break;
            case TraceContext::VectorReset: tag = "[RST VEC]"; break;
        }

        uint8_t op = bus.peek(trace_pc);
        uint8_t b1 = bus.peek(static_cast<Addr>(trace_pc + 1));
        uint8_t b2 = bus.peek(static_cast<Addr>(trace_pc + 2));

        char p_str[9];
        p_str[0] = get_flag(Flag6502::Negative)  ? 'N' : 'n';
        p_str[1] = get_flag(Flag6502::Overflow)  ? 'V' : 'v';
        p_str[2] = '-';
        p_str[3] = get_flag(Flag6502::Break)     ? 'B' : 'b';
        p_str[4] = get_flag(Flag6502::Decimal)   ? 'D' : 'd';
        p_str[5] = get_flag(Flag6502::Interrupt) ? 'I' : 'i';
        p_str[6] = get_flag(Flag6502::Zero)      ? 'Z' : 'z';
        p_str[7] = get_flag(Flag6502::Carry)     ? 'C' : 'c';
        p_str[8] = '\0';

        // Access via get_reg() (or swap to member names like a_, x_, y_, sp_, status_ if direct)
        const auto reg_a  = static_cast<int>(get_reg(Reg::A));
        const auto reg_x  = static_cast<int>(get_reg(Reg::X));
        const auto reg_y  = static_cast<int>(get_reg(Reg::Y));
        const auto reg_sp = static_cast<int>(get_reg(Reg::SP));
        const auto reg_p  = static_cast<int>(status);

        std::cout << tag << " "
                  << "PC: $" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << trace_pc << " | "
                  << "Op: "  << std::setw(2) << static_cast<int>(op) << " "
                             << std::setw(2) << static_cast<int>(b1) << " "
                             << std::setw(2) << static_cast<int>(b2) << " | "
                  << "A: $"  << std::setw(2) << reg_a << " "
                  << "X: $"  << std::setw(2) << reg_x << " "
                  << "Y: $"  << std::setw(2) << reg_y << " "
                  << "SP: $" << std::setw(2) << reg_sp << " | "
                  << "P: $"  << std::setw(2) << reg_p  << " [" << p_str << "]\n"
                  << std::dec;

    }
};
