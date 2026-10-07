// DOS platform layer - keyboard controller and BIOS keyboard services.
//
// The host delivers XT (set 1) scan codes. Each byte is latched in port 60h
// and raises IRQ1 (INT 9). The default INT 9 handler is a BIOS one that
// maintains the shift flags and the type-ahead buffer in the BIOS data area,
// so programs hooking INT 9 and chaining to it behave like on a real PC.

#include "dos/machine.hpp"

#include "internal.hpp"

#include <cctype>

namespace dos {

namespace {

// ASCII for scan codes 00-58h (US layout), unshifted and shifted.
constexpr char kNormal[0x59] = {
    0,    27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 8,   9,     // 00-0F
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 13,  0,   'a', 's',  // 10-1F
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,  '\\', 'z', 'x', 'c', 'v', // 20-2F
    'b', 'n', 'm', ',', '.', '/', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0,    // 30-3F
    0,   0,   0,   0,   0,   0,   0,   '7', '8', '9', '-', '4', '5', '6', '+', '1',  // 40-4F
    '2', '3', '0', '.', 0,   0,   0,   0,   0,                                       // 50-58
};

constexpr char kShifted[0x59] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 8,   0,    // 00-0F
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 13,  0,   'A', 'S',  // 10-1F
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,   '|', 'Z', 'X', 'C', 'V',  // 20-2F
    'B', 'N', 'M', '<', '>', '?', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0,    // 30-3F
    0,   0,   0,   0,   0,   0,   0,   '7', '8', '9', '-', '4', '5', '6', '+', '1',  // 40-4F
    '2', '3', '0', '.', 0,   0,   0,   0,   0,                                       // 50-58
};

bool keypad_nav(std::uint8_t sc) { return sc >= 0x47 && sc <= 0x53 && sc != 0x4A && sc != 0x4E; }

void enqueue(std::uint16_t key)
{
    const std::uint16_t start = bda16(kBdaKbStart) ? bda16(kBdaKbStart) : 0x1E;
    const std::uint16_t end = bda16(kBdaKbEnd) ? bda16(kBdaKbEnd) : 0x3E;
    const std::uint16_t tail = bda16(kBdaKbTail);
    std::uint16_t next = static_cast<std::uint16_t>(tail + 2);
    if (next >= end)
        next = start;
    if (next == bda16(kBdaKbHead))
        return;  // buffer full (the BIOS would beep)
    set_bda16(kBda + tail, key);
    set_bda16(kBdaKbTail, next);
}

// Translate a make code into a BIOS key word, 0 if it produces none.
std::uint16_t translate(std::uint8_t sc, bool extended, std::uint8_t flags)
{
    const bool shift = (flags & 0x03) != 0;
    const bool ctrl = (flags & 0x04) != 0;
    const bool alt = (flags & 0x08) != 0;
    const bool numlock = (flags & 0x20) != 0;
    const bool caps = (flags & 0x40) != 0;
    const std::uint16_t s = static_cast<std::uint16_t>(sc << 8);

    if (sc >= 0x3B && sc <= 0x44) {  // F1-F10
        if (alt) return static_cast<std::uint16_t>((sc + 0x2D) << 8);
        if (ctrl) return static_cast<std::uint16_t>((sc + 0x23) << 8);
        if (shift) return static_cast<std::uint16_t>((sc + 0x19) << 8);
        return s;
    }
    if (sc == 0x57 || sc == 0x58)
        return 0;  // F11/F12 only exist for the enhanced services

    if (extended) {
        if (sc == 0x1C) return ctrl ? 0x1C0A : 0x1C0D;  // grey enter
        if (sc == 0x35) return 0x352F;                   // grey /
    }

    if (keypad_nav(sc)) {
        const bool digits = !extended && (numlock != shift);
        if (ctrl) {
            switch (sc) {
            case 0x47: return 0x7700;
            case 0x49: return 0x8400;
            case 0x4B: return 0x7300;
            case 0x4D: return 0x7400;
            case 0x4F: return 0x7500;
            case 0x51: return 0x7600;
            default: return 0;
            }
        }
        if (alt)
            return 0;
        if (digits)
            return static_cast<std::uint16_t>(s | static_cast<std::uint8_t>(kNormal[sc]));
        if (sc == 0x4C)
            return 0;
        return s;
    }

    if (sc >= 0x59)
        return 0;

    const char base = kNormal[sc];
    if (alt) {
        if (sc >= 0x02 && sc <= 0x0D)
            return static_cast<std::uint16_t>((sc + 0x76) << 8);  // alt-1..alt-=
        if (std::isalpha(static_cast<unsigned char>(base)))
            return s;
        return 0;
    }
    if (ctrl) {
        if (std::isalpha(static_cast<unsigned char>(base)))
            return static_cast<std::uint16_t>(s | (base - 'a' + 1));
        switch (sc) {
        case 0x01: return 0x011B;
        case 0x0E: return 0x0E7F;
        case 0x1A: return 0x1A1B;
        case 0x1B: return 0x1B1D;
        case 0x1C: return 0x1C0A;
        case 0x2B: return 0x2B1C;
        case 0x39: return 0x3920;
        default: return 0;
        }
    }
    if (sc == 0x0F && shift)
        return 0x0F00;  // shift-tab

    char c = shift ? kShifted[sc] : base;
    if (std::isalpha(static_cast<unsigned char>(base)) && caps)
        c = shift ? static_cast<char>(std::tolower(static_cast<unsigned char>(c)))
                  : static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (!c)
        return 0;
    return static_cast<std::uint16_t>(s | static_cast<std::uint8_t>(c));
}

}  // namespace

void keyboard_scancode(std::uint8_t code)
{
    std::lock_guard lock(interrupt_lock());
    set_keyboard_data(code);
    hardware_irq(9);
}

void bios_int9()
{
    const std::uint8_t code = inb(0x60);
    std::uint8_t flags3 = ld(mem[kBdaKbFlags3]);

    if (code == 0xE0) {
        st(mem[kBdaKbFlags3], flags3 | 0x02);
        return;
    }
    const bool extended = (flags3 & 0x02) != 0;
    st(mem[kBdaKbFlags3], flags3 & ~0x02);

    const bool release = (code & 0x80) != 0;
    const std::uint8_t sc = code & 0x7F;
    std::uint8_t flags = ld(mem[kBdaShift]);
    std::uint8_t flags2 = ld(mem[kBdaShift2]);

    auto set_bit = [](std::uint8_t& f, std::uint8_t bit, bool on) {
        f = static_cast<std::uint8_t>(on ? (f | bit) : (f & ~bit));
    };

    switch (sc) {
    case 0x2A:  // left shift (E0 2A is a fake shift sent with grey keys)
        if (!extended)
            set_bit(flags, 0x02, !release);
        st(mem[kBdaShift], flags);
        return;
    case 0x36:
        if (!extended)
            set_bit(flags, 0x01, !release);
        st(mem[kBdaShift], flags);
        return;
    case 0x1D:
        set_bit(flags, 0x04, !release);
        if (!extended)
            set_bit(flags2, 0x01, !release);
        st(mem[kBdaShift], flags);
        st(mem[kBdaShift2], flags2);
        return;
    case 0x38:
        set_bit(flags, 0x08, !release);
        if (!extended)
            set_bit(flags2, 0x02, !release);
        st(mem[kBdaShift], flags);
        st(mem[kBdaShift2], flags2);
        return;
    case 0x3A:
    case 0x45:
    case 0x46: {
        const std::uint8_t toggle = sc == 0x3A ? 0x40 : (sc == 0x45 ? 0x20 : 0x10);
        if (!release && !(flags2 & toggle))
            flags ^= toggle;
        set_bit(flags2, toggle, !release);
        st(mem[kBdaShift], flags);
        st(mem[kBdaShift2], flags2);
        return;
    }
    default:
        break;
    }

    if (release)
        return;
    if (sc == 0x52 && !(flags & 0x0C))
        st(mem[kBdaShift], flags ^ 0x80);  // insert state
    if (const std::uint16_t key = translate(sc, extended, flags))
        enqueue(key);
}

std::uint16_t bios_key(int cmd)
{
    switch (cmd & 0xEF) {
    case 0:
        for (;;) {
            {
                std::lock_guard lock(interrupt_lock());
                const std::uint16_t head = bda16(kBdaKbHead);
                if (head != bda16(kBdaKbTail)) {
                    const std::uint16_t key = bda16(kBda + head);
                    std::uint16_t next = static_cast<std::uint16_t>(head + 2);
                    if (next >= (bda16(kBdaKbEnd) ? bda16(kBdaKbEnd) : 0x3E))
                        next = bda16(kBdaKbStart) ? bda16(kBdaKbStart) : 0x1E;
                    set_bda16(kBdaKbHead, next);
                    return key;
                }
            }
            idle();
        }
    case 1: {
        // A program spinning on "key available?" gets a short nap every
        // so often so it doesn't burn a host core.
        static thread_local int empty_polls = 0;
        poll();
        {
            std::lock_guard lock(interrupt_lock());
            const std::uint16_t head = bda16(kBdaKbHead);
            if (head != bda16(kBdaKbTail)) {
                empty_polls = 0;
                return bda16(kBda + head);
            }
        }
        if (++empty_polls >= 256) {
            empty_polls = 0;
            sleep_until(now() + 0.0005);
        }
        return 0;
    }
    default:
        poll();
        return ld(mem[kBdaShift]);
    }
}

}  // namespace dos
