// DOS platform layer - program thread plumbing and frame hand-off.

#include "dos/runtime.hpp"

#include "internal.hpp"

#include <thread>

namespace dos {

namespace {

std::atomic<std::thread::id> g_program_thread{};
std::atomic<bool> g_exit_requested{false};
std::atomic<int> g_exit_code{0};

std::mutex g_frame_lock;
Frame g_front;
std::uint64_t g_front_serial = 0;
std::atomic<double> g_front_time{-1.0};

}  // namespace

bool on_program_thread() noexcept { return g_program_thread.load() == std::this_thread::get_id(); }

void set_program_thread() noexcept { g_program_thread.store(std::this_thread::get_id()); }

void request_program_exit(int code) noexcept
{
    g_exit_code.store(code);
    g_exit_requested.store(true);
}

void check_program_exit()
{
    if (g_exit_requested.load() && on_program_thread())
        throw ProgramExit{g_exit_code.load()};
}

void on_retrace()
{
    std::lock_guard lock(g_frame_lock);
    vga().snapshot(g_front);
    g_front.serial = ++g_front_serial;
    g_front_time.store(now());
}

bool take_retrace_frame(Frame& out, std::uint64_t& serial, double max_age)
{
    const double t = g_front_time.load();
    if (t < 0 || now() - t > max_age)
        return false;
    std::lock_guard lock(g_frame_lock);
    if (g_front.serial == serial)
        return true;  // nothing new, but the program is in sync with the retrace
    out = g_front;
    serial = g_front.serial;
    return true;
}

int run_program(const AppInfo& app)
{
    set_program_thread();
    set_quit_handler(app.quit_request);
    int code = 0;
    try {
        if (app.entry)
            app.entry();
    } catch (const ProgramExit& e) {
        code = e.code;
    }
    // stop interrupts reaching program code that is going away
    {
        std::lock_guard lock(interrupt_lock());
        for (int v : {0x08, 0x09, 0x1C})
            set_vector(v, nullptr);
    }
    return code;
}

}  // namespace dos
