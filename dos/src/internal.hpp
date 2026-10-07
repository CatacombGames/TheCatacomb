// DOS platform layer - declarations shared between the platform sources.
#pragma once

#include "dos/machine.hpp"

namespace dos {

// machine.cpp
void set_keyboard_data(std::uint8_t v) noexcept;
void console_clear();
void set_precise_sleep(void (*fn)(std::uint64_t ns)) noexcept;

// bios.cpp
void bios_reset();
void bios_on_mode_set();

// input.cpp
std::uint8_t joystick_port_read();
void joystick_port_write(std::uint8_t v);
void mouse_reset_state();

// runtime.cpp
bool on_program_thread() noexcept;
void set_program_thread() noexcept;
void request_program_exit(int code) noexcept;  // from a non-program thread
void check_program_exit();                     // throws ProgramExit on the program thread
void on_retrace();                             // a retrace was observed by the program

// BIOS data area offsets (segment 0x40)
inline constexpr std::uint32_t kBda = 0x400;
inline constexpr std::uint32_t kBdaShift = kBda + 0x17;
inline constexpr std::uint32_t kBdaShift2 = kBda + 0x18;
inline constexpr std::uint32_t kBdaKbHead = kBda + 0x1A;
inline constexpr std::uint32_t kBdaKbTail = kBda + 0x1C;
inline constexpr std::uint32_t kBdaKbBuf = kBda + 0x1E;  // 16 words
inline constexpr std::uint32_t kBdaVideoMode = kBda + 0x49;
inline constexpr std::uint32_t kBdaColumns = kBda + 0x4A;
inline constexpr std::uint32_t kBdaCrtcBase = kBda + 0x63;
inline constexpr std::uint32_t kBdaTicks = kBda + 0x6C;
inline constexpr std::uint32_t kBdaKbStart = kBda + 0x80;
inline constexpr std::uint32_t kBdaKbEnd = kBda + 0x82;
inline constexpr std::uint32_t kBdaKbFlags3 = kBda + 0x96;

inline std::uint16_t bda16(std::uint32_t a) noexcept
{
    return static_cast<std::uint16_t>(ld(mem[a]) | (ld(mem[a + 1]) << 8));
}

inline void set_bda16(std::uint32_t a, std::uint16_t v) noexcept
{
    st(mem[a], static_cast<std::uint8_t>(v));
    st(mem[a + 1], static_cast<std::uint8_t>(v >> 8));
}

}  // namespace dos
