// DOS platform layer - BIOS, DOS and mouse driver services.
//
// Software interrupts reach these through the vector table, so a program
// can hook any of them with setvect() and chain to the original.

#include "dos/machine.hpp"

#include "internal.hpp"

#include <ctime>

namespace dos {

namespace {

void int08_timer()
{
    std::uint32_t t = bda16(kBdaTicks) | (std::uint32_t(bda16(kBdaTicks + 2)) << 16);
    if (++t >= 0x1800B0) {  // midnight
        t = 0;
        st(mem[kBda + 0x70], 1);
    }
    set_bda16(kBdaTicks, static_cast<std::uint16_t>(t));
    set_bda16(kBdaTicks + 2, static_cast<std::uint16_t>(t >> 16));
    software_interrupt(0x1C);
}

void int10_video()
{
    Regs& r = regs();
    const std::uint8_t ah = r.a.b[1], al = r.a.b[0];
    Vga& v = vga();
    switch (ah) {
    case 0x00:
        v.set_mode(al & 0x7F, !(al & 0x80));
        bios_on_mode_set();
        break;
    case 0x0B:
        if (r.b.b[1] == 0) {
            v.set_cga_color_select(static_cast<std::uint8_t>((r.b.b[0] & 0x1F) | 0x20));
        } else {
            v.set_cga_color_select(static_cast<std::uint8_t>(0x10 | ((r.b.b[0] & 1) << 5)));
        }
        break;
    case 0x0E: {
        const char c = static_cast<char>(al);
        console_write(std::string_view(&c, 1));
        break;
    }
    case 0x0F:
        r.a.b[0] = v.mode();
        r.a.b[1] = v.mode() <= 1 || v.mode() == 4 || v.mode() == 5 || v.mode() == 0x0D || v.mode() == 0x13 ? 40 : 80;
        r.b.b[1] = 0;
        break;
    case 0x10:
        if (machine_config().card == VideoCard::CGA)
            break;
        switch (al) {
        case 0x00: v.set_palette_reg(r.b.b[0] & 0x0F, r.b.b[1]); break;
        case 0x01: v.set_palette_reg(0x11, r.b.b[1]); break;
        case 0x02: {
            const std::uint8_t* p = real_ptr(r.es, r.d.x);
            for (int i = 0; i < 16; ++i)
                v.set_palette_reg(i, ld(p[i]));
            v.set_palette_reg(0x11, ld(p[16]));
            break;
        }
        case 0x10:
            v.set_dac(r.b.x, r.d.b[1], r.c.b[1], r.c.b[0]);
            break;
        case 0x12: {
            const std::uint8_t* p = real_ptr(r.es, r.d.x);
            for (int i = 0; i < r.c.x; ++i)
                v.set_dac(r.b.x + i, ld(p[i * 3]), ld(p[i * 3 + 1]), ld(p[i * 3 + 2]));
            break;
        }
        default: break;
        }
        break;
    case 0x12:
        if (r.b.b[0] == 0x10 && machine_config().card != VideoCard::CGA) {
            r.b.b[1] = 0;     // colour
            r.b.b[0] = 3;     // 256K
            r.c.x = 0x0009;   // switches: EGA colour display
        }
        break;
    case 0x1A:
        if (al == 0 && machine_config().card == VideoCard::VGA) {
            r.a.b[0] = 0x1A;
            r.b.b[0] = 0x08;  // VGA with colour display
            r.b.b[1] = 0x00;
        }
        break;
    default:
        break;
    }
}

void int11_equipment() { regs().a.x = 0x0021; }  // colour 80x25, no floppy

void int12_memory() { regs().a.x = 640; }

void int16_keyboard()
{
    Regs& r = regs();
    const int fn = r.a.b[1];
    const std::uint16_t key = bios_key(fn);
    if ((fn & 0xEF) == 1) {
        r.a.x = key;
        r.flags = key ? 0 : 0x40;  // ZF when no key
    } else {
        r.a.x = key;
    }
}

void int1a_time()
{
    Regs& r = regs();
    if (r.a.b[1] == 0) {
        r.c.x = bda16(kBdaTicks + 2);
        r.d.x = bda16(kBdaTicks);
        r.a.b[0] = ld(mem[kBda + 0x70]);
        st(mem[kBda + 0x70], 0);
    }
}

void int21_dos()
{
    Regs& r = regs();
    const std::uint8_t ah = r.a.b[1];
    switch (ah) {
    case 0x02: {
        const char c = static_cast<char>(r.d.b[0]);
        console_write(std::string_view(&c, 1));
        break;
    }
    case 0x2A:
    case 0x2C: {
        const std::time_t t = std::time(nullptr);
        std::tm tm{};
#if defined(_WIN32)
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        if (ah == 0x2A) {
            r.c.x = static_cast<std::uint16_t>(tm.tm_year + 1900);
            r.d.b[1] = static_cast<std::uint8_t>(tm.tm_mon + 1);
            r.d.b[0] = static_cast<std::uint8_t>(tm.tm_mday);
            r.a.b[0] = static_cast<std::uint8_t>(tm.tm_wday);
        } else {
            r.c.b[1] = static_cast<std::uint8_t>(tm.tm_hour);
            r.c.b[0] = static_cast<std::uint8_t>(tm.tm_min);
            r.d.b[1] = static_cast<std::uint8_t>(tm.tm_sec);
            r.d.b[0] = static_cast<std::uint8_t>(static_cast<int>(now() * 100) % 100);
        }
        break;
    }
    case 0x30:
        r.a.x = 0x0005;  // DOS 5.0
        r.b.x = 0;
        break;
    case 0x4C:
        exit_program(r.a.b[0]);
    default:
        break;
    }
}

}  // namespace

void int33_mouse_dispatch();  // input.cpp

void bios_reset()
{
    // vectors
    set_vector(0x08, &int08_timer);
    set_vector(0x09, &bios_int9);
    set_vector(0x10, &int10_video);
    set_vector(0x11, &int11_equipment);
    set_vector(0x12, &int12_memory);
    set_vector(0x16, &int16_keyboard);
    set_vector(0x1A, &int1a_time);
    set_vector(0x21, &int21_dos);
    if (machine_config().mouse)
        set_vector(0x33, &int33_mouse_dispatch);

    // BIOS data area
    set_bda16(kBdaKbHead, 0x1E);
    set_bda16(kBdaKbTail, 0x1E);
    set_bda16(kBdaKbStart, 0x1E);
    set_bda16(kBdaKbEnd, 0x3E);
    st(mem[kBdaShift], 0);
    st(mem[kBdaShift2], 0);
    st(mem[kBdaKbFlags3], 0x10);  // enhanced keyboard
    set_bda16(kBda + 0x10, 0x0021);
    set_bda16(kBda + 0x13, 640);
    set_bda16(kBdaCrtcBase, 0x3D4);
    st(mem[kBdaVideoMode], 3);
    set_bda16(kBdaColumns, 80);

    const std::time_t t = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    const std::uint32_t secs = static_cast<std::uint32_t>(tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec);
    const std::uint32_t ticks = static_cast<std::uint32_t>(secs * 18.2065);
    set_bda16(kBdaTicks, static_cast<std::uint16_t>(ticks));
    set_bda16(kBdaTicks + 2, static_cast<std::uint16_t>(ticks >> 16));

    mouse_reset_state();
}

void bios_on_mode_set()
{
    const std::uint8_t m = vga().mode();
    st(mem[kBdaVideoMode], m);
    set_bda16(kBdaColumns, (m <= 1 || m == 4 || m == 5 || m == 0x0D || m == 0x13) ? 40 : 80);
    console_clear();
}

}  // namespace dos
