// DOS platform layer - memory, port dispatch, interrupts, program services.

#include "dos/machine.hpp"

#include "internal.hpp"

#include <chrono>
#include <cstring>
#include <thread>

namespace dos {

alignas(64) std::uint8_t mem[kMemSize];

namespace {

MachineConfig g_config;
std::recursive_mutex g_interrupt_lock;
thread_local int t_cli_depth = 0;
thread_local Regs t_regs;

std::atomic<isr> g_vectors[256];

std::atomic<bool> g_quit_requested{false};
std::atomic<void (*)()> g_quit_handler{nullptr};
std::atomic<bool> g_quit_handler_ran{false};

using clock = std::chrono::steady_clock;
clock::time_point g_epoch = clock::now();
std::atomic<void (*)(std::uint64_t)> g_precise_sleep{nullptr};

std::mutex g_console_lock;
std::string g_console;

std::atomic<std::uint8_t> g_kbd_data{0};

void default_iret() {}

}  // namespace

//=========================================================================
// Memory
//=========================================================================

bool is_emulated(const void* p) noexcept
{
    auto a = static_cast<const std::uint8_t*>(p);
    return a >= mem && a < mem + kMemSize;
}

std::uint32_t linear_of(const void* p) noexcept
{
    return static_cast<std::uint32_t>(static_cast<const std::uint8_t*>(p) - mem);
}

std::uint8_t peek8(const void* p) noexcept
{
    if (is_emulated(p)) {
        const std::uint32_t a = linear_of(p);
        if (a >= kVideoBase && a < kVideoEnd)
            return vga().mem_read(a);
    }
    return ld(*static_cast<const std::uint8_t*>(p));
}

void poke8(void* p, std::uint8_t v) noexcept
{
    if (is_emulated(p)) {
        const std::uint32_t a = linear_of(p);
        if (a >= kVideoBase && a < kVideoEnd) {
            vga().mem_write(a, v);
            return;
        }
    }
    st(*static_cast<std::uint8_t*>(p), v);
}

std::uint16_t peek16(const void* p) noexcept
{
    auto a = static_cast<const std::uint8_t*>(p);
    return static_cast<std::uint16_t>(peek8(a) | (peek8(a + 1) << 8));
}

void poke16(void* p, std::uint16_t v) noexcept
{
    auto a = static_cast<std::uint8_t*>(p);
    poke8(a, static_cast<std::uint8_t>(v));
    poke8(a + 1, static_cast<std::uint8_t>(v >> 8));
}

void copy(void* dst, const void* src, std::size_t n) noexcept
{
    auto d = static_cast<std::uint8_t*>(dst);
    auto s = static_cast<const std::uint8_t*>(src);
    if (!is_emulated(d) && !is_emulated(s)) {
        std::memmove(d, s, n);
        return;
    }
    for (std::size_t i = 0; i < n; ++i)
        poke8(d + i, peek8(s + i));
}

void fill(void* dst, std::uint8_t v, std::size_t n) noexcept
{
    auto d = static_cast<std::uint8_t*>(dst);
    if (!is_emulated(d)) {
        std::memset(d, v, n);
        return;
    }
    for (std::size_t i = 0; i < n; ++i)
        poke8(d + i, v);
}

//=========================================================================
// Ports
//=========================================================================

std::uint8_t inb(std::uint16_t port)
{
    std::uint8_t v = 0xFF;
    if (port == 0x60)
        return g_kbd_data.load();
    if (port == 0x64)
        return 0x14;  // output buffer empty, self test ok
    if (port == 0x20 || port == 0x21 || port == 0xA0 || port == 0xA1)
        return 0;
    if (pit().port_read(port, v))
        return v;
    if (port == 0x201)
        return joystick_port_read();
    if (vga().port_read(port, v))
        return v;
    return 0xFF;
}

std::uint16_t inw(std::uint16_t port)
{
    const std::uint8_t lo = inb(port);
    return static_cast<std::uint16_t>(lo | (inb(static_cast<std::uint16_t>(port + 1)) << 8));
}

void outb(std::uint16_t port, std::uint8_t v)
{
    if (port == 0x20 || port == 0x21 || port == 0xA0 || port == 0xA1 || port == 0x60 || port == 0x64)
        return;  // PIC EOI / masks, keyboard controller commands
    if (pit().port_write(port, v))
        return;
    if (port == 0x201) {
        joystick_port_write(v);
        return;
    }
    vga().port_write(port, v);
}

void outw(std::uint16_t port, std::uint16_t v)
{
    outb(port, static_cast<std::uint8_t>(v));
    outb(static_cast<std::uint16_t>(port + 1), static_cast<std::uint8_t>(v >> 8));
}

void set_keyboard_data(std::uint8_t v) noexcept { g_kbd_data.store(v); }

//=========================================================================
// Registers and interrupts
//=========================================================================

Regs& regs() noexcept { return t_regs; }

isr get_vector(int n) noexcept { return g_vectors[n & 0xFF].load(); }

void set_vector(int n, isr handler) noexcept
{
    n &= 0xFF;
    if (!handler)
        handler = &default_iret;
    g_vectors[n].store(handler);

    // Mirror the vector in the real mode IVT so programs that inspect it
    // (e.g. "is a mouse driver installed?") see a far pointer to either an
    // IRET (nothing installed) or a far jump into a handler.
    const std::uint16_t off = static_cast<std::uint16_t>(0xE000 + n * 4);
    st(mem[n * 4 + 0], static_cast<std::uint8_t>(off));
    st(mem[n * 4 + 1], static_cast<std::uint8_t>(off >> 8));
    st(mem[n * 4 + 2], 0x00);
    st(mem[n * 4 + 3], 0xF0);
    st(mem[0xF0000 + off], handler == &default_iret ? 0xCF : 0xEA);
}

void software_interrupt(int n)
{
    if (isr h = get_vector(n))
        h();
}

void hardware_irq(int vector)
{
    std::lock_guard lock(g_interrupt_lock);
    try {
        software_interrupt(vector);
    } catch (const ProgramExit& e) {
        // An ISR asked to terminate: let the program thread finish.
        request_program_exit(e.code);
    }
}

std::recursive_mutex& interrupt_lock() noexcept { return g_interrupt_lock; }

void cli()
{
    g_interrupt_lock.lock();
    ++t_cli_depth;
}

void sti()
{
    if (t_cli_depth > 0) {
        --t_cli_depth;
        g_interrupt_lock.unlock();
    }
}

//=========================================================================
// Machine
//=========================================================================

void machine_reset(const MachineConfig& cfg)
{
    g_config = cfg;
    std::memset(mem, 0, sizeof(mem));
    for (int i = 0; i < 256; ++i)
        set_vector(i, nullptr);
    g_epoch = clock::now();

    bios_reset();
    vga().reset(cfg.card);
    pit().reset();
}

const MachineConfig& machine_config() noexcept { return g_config; }

//=========================================================================
// Program services
//=========================================================================

void exit_program(int code) { throw ProgramExit{code}; }

void request_quit() noexcept { g_quit_requested.store(true); }

bool quit_requested() noexcept { return g_quit_requested.load(); }

void set_quit_handler(void (*handler)()) noexcept { g_quit_handler.store(handler); }

void poll()
{
    if (g_quit_requested.load() && on_program_thread() && !g_quit_handler_ran.exchange(true)) {
        if (auto h = g_quit_handler.load())
            h();
        exit_program(0);
    }
    check_program_exit();
}

void idle()
{
    poll();
    sleep_until(now() + 0.001);
}

double now() noexcept
{
    return std::chrono::duration<double>(clock::now() - g_epoch).count();
}

void set_precise_sleep(void (*fn)(std::uint64_t ns)) noexcept { g_precise_sleep.store(fn); }

void sleep_until(double t)
{
    const double d = t - now();
    if (d <= 0)
        return;
    if (auto fn = g_precise_sleep.load()) {
        fn(static_cast<std::uint64_t>(d * 1e9));
        return;
    }
    // Coarse sleep, then yield for the last stretch.
    if (d > 0.002)
        std::this_thread::sleep_for(std::chrono::duration<double>(d - 0.0015));
    while (now() < t)
        std::this_thread::yield();
}

void console_write(std::string_view text)
{
    std::lock_guard lock(g_console_lock);
    g_console.append(text);
    if (g_console.size() > 16384)
        g_console.erase(0, g_console.size() - 16384);
}

std::string console_text()
{
    std::lock_guard lock(g_console_lock);
    return g_console;
}

void console_clear()
{
    std::lock_guard lock(g_console_lock);
    g_console.clear();
}

}  // namespace dos
