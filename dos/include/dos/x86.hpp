// DOS platform layer - helpers for translating 16 bit assembly to C++.
//
// A segment register is represented by a byte pointer to the start of the
// segment (host memory, or emulated memory for MK_FP pointers such as the
// video window) and offsets are 16 bit, wrapping like on the 8086. The
// string instructions go through dos::peek8/poke8 so video memory accesses
// load and store the EGA/VGA latches exactly like the real instructions.
#pragma once

#include <cstdint>

#include "dos/machine.hpp"

namespace dos::x86 {

using seg = std::uint8_t*;
using cseg = const std::uint8_t*;

inline seg segment(void* p) noexcept { return static_cast<seg>(p); }
inline cseg segment(const void* p) noexcept { return static_cast<cseg>(p); }

inline std::uint8_t rdb(cseg s, std::uint16_t off) noexcept { return peek8(s + off); }
inline std::uint16_t rdw(cseg s, std::uint16_t off) noexcept
{
    return static_cast<std::uint16_t>(peek8(s + off) | (peek8(s + static_cast<std::uint16_t>(off + 1)) << 8));
}
inline void wrb(seg s, std::uint16_t off, std::uint8_t v) noexcept { poke8(s + off, v); }
inline void wrw(seg s, std::uint16_t off, std::uint16_t v) noexcept
{
    poke8(s + off, static_cast<std::uint8_t>(v));
    poke8(s + static_cast<std::uint16_t>(off + 1), static_cast<std::uint8_t>(v >> 8));
}

// string instructions, direction flag clear
inline std::uint8_t lodsb(cseg ds, std::uint16_t& si) noexcept { return rdb(ds, si++); }
inline std::uint16_t lodsw(cseg ds, std::uint16_t& si) noexcept
{
    const std::uint16_t v = rdw(ds, si);
    si = static_cast<std::uint16_t>(si + 2);
    return v;
}
inline void stosb(seg es, std::uint16_t& di, std::uint8_t v) noexcept { wrb(es, di++, v); }
inline void stosw(seg es, std::uint16_t& di, std::uint16_t v) noexcept
{
    wrw(es, di, v);
    di = static_cast<std::uint16_t>(di + 2);
}
inline void movsb(seg es, std::uint16_t& di, cseg ds, std::uint16_t& si) noexcept { stosb(es, di, lodsb(ds, si)); }
inline void movsw(seg es, std::uint16_t& di, cseg ds, std::uint16_t& si) noexcept
{
    // on the 8 bit video bus a word is read (low, high) then written (low,
    // high): with latched write modes both bytes get the high byte's latches
    stosw(es, di, lodsw(ds, si));
}

}  // namespace dos::x86
