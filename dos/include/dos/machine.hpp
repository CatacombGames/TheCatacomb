// DOS platform layer - emulated PC machine.
//
// This is the SDL-free core of the platform: conventional memory, the I/O
// port space, the interrupt vector table, and the devices DOS games talk to
// directly (EGA/VGA, 8253 PIT + PC speaker, keyboard controller, game port,
// mouse driver, BIOS data area).
//
// Threading model
//   * The DOS program runs on its own thread (the "CPU").
//   * Hardware interrupts are delivered from other threads: IRQ0 (INT 8) from
//     the audio thread, IRQ1 (INT 9) from the main/event thread.
//   * interrupt_lock() is the CPU interrupt flag: ISRs run while holding it
//     and cli()/disable() take it, so code that disables interrupts is never
//     interrupted, exactly like on real hardware.
//   * Memory shared with ISRs and the display is accessed with relaxed
//     atomics so the emulation is free of data races.
#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace dos {

//=========================================================================
// Relaxed atomic helpers for state shared between threads
//=========================================================================

#if defined(_MSC_VER) && !defined(__clang__)
template <class T>
inline T ld(const T& v) noexcept
{
    return std::atomic_ref<T>(const_cast<T&>(v)).load(std::memory_order_relaxed);
}

template <class T, class U>
inline void st(T& v, U x) noexcept
{
    std::atomic_ref<T>(v).store(static_cast<T>(x), std::memory_order_relaxed);
}
#else
// GCC/Clang builtins: same semantics, also available with older libc++
template <class T>
inline T ld(const T& v) noexcept
{
    T r;
    __atomic_load(&v, &r, __ATOMIC_RELAXED);
    return r;
}

template <class T, class U>
inline void st(T& v, U x) noexcept
{
    T t = static_cast<T>(x);
    __atomic_store(&v, &t, __ATOMIC_RELAXED);
}
#endif

//=========================================================================
// Conventional memory
//=========================================================================

inline constexpr std::uint32_t kMemSize = 0x110000;  // 1MB + HMA (FFFF:FFFF)
inline constexpr std::uint32_t kVideoBase = 0xA0000;
inline constexpr std::uint32_t kVideoEnd = 0xC0000;

// The emulated first megabyte: IVT, BIOS data area, video aperture...
// Program data lives in host memory; far pointers made with MK_FP point here.
extern std::uint8_t mem[kMemSize];

inline std::uint8_t* linear(std::uint32_t addr) noexcept { return mem + addr; }
inline std::uint8_t* real_ptr(std::uint16_t seg, std::uint16_t off) noexcept
{
    return mem + ((std::uint32_t(seg) << 4) + off);
}
// A 16:16 far pointer as stored in files and data structures of DOS programs.
struct FarPtr {
    std::uint16_t off;
    std::uint16_t seg;
    constexpr std::uint32_t linear() const noexcept { return (std::uint32_t(seg) << 4) + off; }
};
static_assert(sizeof(FarPtr) == 4);

bool is_emulated(const void* p) noexcept;
std::uint32_t linear_of(const void* p) noexcept;  // requires is_emulated(p)

// Byte/word access through any pointer. Accesses inside the A000-BFFF
// window are routed to the video card (planes, latches, write modes).
std::uint8_t peek8(const void* p) noexcept;
void poke8(void* p, std::uint8_t v) noexcept;
std::uint16_t peek16(const void* p) noexcept;
void poke16(void* p, std::uint16_t v) noexcept;
void copy(void* dst, const void* src, std::size_t n) noexcept;  // ascending, byte by byte
void fill(void* dst, std::uint8_t v, std::size_t n) noexcept;

//=========================================================================
// I/O ports
//=========================================================================

std::uint8_t inb(std::uint16_t port);
std::uint16_t inw(std::uint16_t port);
void outb(std::uint16_t port, std::uint8_t v);
void outw(std::uint16_t port, std::uint16_t v);  // low byte to port, high byte to port+1

//=========================================================================
// CPU registers (Borland pseudo-registers, int86) and interrupts
//=========================================================================

union Reg16 {
    std::uint16_t x;
    std::uint8_t b[2];  // [0] = low, [1] = high
};

struct Regs {
    Reg16 a{}, b{}, c{}, d{};
    std::uint16_t si = 0, di = 0, bp = 0, es = 0, ds = 0, flags = 0;
    bool carry = false;
};

// Per thread, like the register file each "context" sees.
Regs& regs() noexcept;

using isr = void (*)();

isr get_vector(int n) noexcept;
void set_vector(int n, isr handler) noexcept;
void software_interrupt(int n);  // INT n with the current regs()
void hardware_irq(int vector); // deliver from a device thread (takes the interrupt lock)

std::recursive_mutex& interrupt_lock() noexcept;
void cli();  // nests
void sti();

//=========================================================================
// Machine configuration
//=========================================================================

enum class VideoCard : std::uint8_t { CGA, EGA, VGA };

struct MachineConfig {
    VideoCard card = VideoCard::VGA;
    bool mouse = true;
};

void machine_reset(const MachineConfig& cfg);
const MachineConfig& machine_config() noexcept;

//=========================================================================
// Video card
//=========================================================================

struct Frame {
    int width = 320;
    int height = 200;
    double display_aspect = 4.0 / 3.0;     // aspect of the whole picture on a CRT
    std::vector<std::uint8_t> pixels;     // DAC indexes, width*height
    std::array<std::uint32_t, 256> palette{};  // 0x00RRGGBB
    std::uint64_t serial = 0;              // retrace counter at capture time
};

class Vga {
public:
    void reset(VideoCard card);
    void set_mode(std::uint8_t mode, bool clear_memory);
    std::uint8_t mode() const noexcept { return ld(mode_); }
    VideoCard card() const noexcept { return card_; }

    std::uint8_t mem_read(std::uint32_t addr) noexcept;  // addr in A0000..BFFFF
    void mem_write(std::uint32_t addr, std::uint8_t v) noexcept;

    bool port_read(std::uint16_t port, std::uint8_t& v);
    bool port_write(std::uint16_t port, std::uint8_t v);

    void set_dac(int index, std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept;
    void set_palette_reg(int index, std::uint8_t v) noexcept;
    void set_cga_color_select(std::uint8_t v) noexcept;

    // Vertical retrace timing (game thread polling port 3DA)
    double refresh_hz() const noexcept;
    std::uint64_t retrace_count() const noexcept;  // number of retraces since reset

    void snapshot(Frame& out) const;

private:
    std::uint8_t status_read();
    void load_default_dac_16();
    void load_default_dac_256();

    VideoCard card_ = VideoCard::VGA;
    std::uint8_t mode_ = 3;

    // memory
    std::uint8_t planes_[4][0x10000]{};
    std::uint8_t cga_[0x8000]{};  // B8000-BFFFF linear (CGA graphics and text)
    std::uint8_t latch_[4]{};

    // registers
    std::uint8_t seq_index_ = 0, seq_[8]{};
    std::uint8_t gc_index_ = 0, gc_[16]{};
    std::uint8_t crtc_index_ = 0, crtc_[32]{};
    std::uint8_t attr_index_ = 0, attr_[32]{};
    bool attr_flip_ = false;
    std::uint8_t dac_read_ = 0, dac_write_ = 0, dac_step_ = 0, dac_read_step_ = 0;
    std::uint8_t dac_[256][3]{};
    std::uint8_t dac_pending_[3]{};
    std::uint8_t cga_mode_ctl_ = 0, cga_color_ = 0;
    std::uint8_t misc_out_ = 0x63;

    // status register emulation
    int polls_same_ = 0;
    bool last_retrace_ = false;
    std::uint8_t hblank_toggle_ = 0;
};

Vga& vga() noexcept;

//=========================================================================
// 8253 timer and PC speaker
//=========================================================================

class Pit {
public:
    static constexpr double kClock = 1193181.6666;

    void reset();
    bool port_read(std::uint16_t port, std::uint8_t& v);
    bool port_write(std::uint16_t port, std::uint8_t v);

    // Advance emulated time and synthesize speaker audio (mono float).
    // Fires IRQ0 (INT 8) at the programmed rate. Called by the audio
    // thread, or by a timer thread when there is no audio device.
    void generate(float* out, int frames, int sample_rate);

    std::uint16_t channel_divisor(int ch) const noexcept { return ld(div_[ch]); }

private:
    std::uint16_t div_[3] = {0, 0, 0};       // 0 = 65536
    std::uint8_t access_[3] = {3, 3, 3};     // 1=lo 2=hi 3=lo/hi
    std::uint8_t mode_[3] = {3, 3, 3};
    bool hi_next_[3] = {false, false, false};
    std::uint8_t latch_lo_[3]{};
    bool latched_[3] = {false, false, false};
    std::uint16_t latch_value_[3]{};
    bool read_hi_next_[3] = {false, false, false};
    std::uint8_t port61_ = 0;

    // synthesis state (audio thread only)
    double ch0_remaining_ = 65536.0;
    double ch2_phase_ = 0.0;
    double hp_prev_in_ = 0.0, hp_prev_out_ = 0.0;
};

Pit& pit() noexcept;

//=========================================================================
// Keyboard (8042 port 60h, IRQ1, BIOS buffer) - set 1 scan codes
//=========================================================================

// Deliver a raw scan code byte (make, break = make|0x80, or 0xE0 prefix).
void keyboard_scancode(std::uint8_t code);
void bios_int9();                       // default INT 9 handler
std::uint16_t bios_key(int cmd);        // INT 16h fn 0/1/2 (also 10h/11h/12h)

//=========================================================================
// Mouse driver (INT 33h) and game port (201h)
//=========================================================================

void mouse_motion(int dx, int dy);      // mickeys
void mouse_buttons(int mask);           // bit0 left, bit1 right, bit2 middle
bool mouse_wants_capture() noexcept;    // driver reset and cursor in use
void joystick_update(int stick, int x, int y, int buttons, bool connected);  // x,y -32768..32767

//=========================================================================
// Program services
//=========================================================================

// Terminates the DOS program (unwinds the program thread).
struct ProgramExit {
    int code;
};
[[noreturn]] void exit_program(int code);

// Called by the program while it waits; lets the machine run and handles
// requests from the host (window closed...).
void idle();
void poll();

void request_quit() noexcept;           // host asked to close
bool quit_requested() noexcept;
void set_quit_handler(void (*handler)()) noexcept;

// High resolution machine clock (seconds since machine reset).
double now() noexcept;
void sleep_until(double t);

// Text written by the program to the console (puts/printf in text mode).
void console_write(std::string_view text);
std::string console_text();

//=========================================================================
// File system: DOS names resolved case-insensitively.
//   reads:  write directory first, then data directory
//   writes: write directory (SDL pref path)
//=========================================================================

namespace fs {
void set_data_dir(const std::filesystem::path& p);
void set_write_dir(const std::filesystem::path& p);
const std::filesystem::path& data_dir();
const std::filesystem::path& write_dir();
std::filesystem::path resolve_read(std::string_view dos_name);
std::filesystem::path resolve_write(std::string_view dos_name);
bool exists(std::string_view dos_name);
}  // namespace fs

}  // namespace dos
