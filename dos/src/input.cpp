// DOS platform layer - mouse driver (INT 33h) and game port (201h).

#include "dos/machine.hpp"

#include "internal.hpp"

#include <algorithm>

namespace dos {

namespace {

struct Mouse {
    std::atomic<int> x{320}, y{100};
    std::atomic<int> buttons{0};
    std::atomic<int> mickey_x{0}, mickey_y{0};
    std::atomic<int> frac_x{0}, frac_y{0};
    std::atomic<int> min_x{0}, max_x{639}, min_y{0}, max_y{199};
    std::atomic<int> ratio_x{8}, ratio_y{16};  // mickeys per 8 pixels
    std::atomic<int> cursor{-1};
    std::atomic<bool> installed{false};
    std::atomic<int> presses[3]{}, releases[3]{};
};

Mouse g_mouse;

struct Stick {
    std::atomic<int> x{0}, y{0}, buttons{0};
    std::atomic<bool> connected{false};
};

Stick g_sticks[2];
std::atomic<std::uint32_t> g_port201_reads{0};
std::uint16_t g_axis_count[4] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};

void clamp_position()
{
    g_mouse.x = std::clamp(g_mouse.x.load(), g_mouse.min_x.load(), g_mouse.max_x.load());
    g_mouse.y = std::clamp(g_mouse.y.load(), g_mouse.min_y.load(), g_mouse.max_y.load());
}

}  // namespace

void mouse_reset_state()
{
    g_mouse.x = 320;
    g_mouse.y = 100;
    g_mouse.buttons = 0;
    g_mouse.mickey_x = g_mouse.mickey_y = 0;
    g_mouse.min_x = 0;
    g_mouse.max_x = 639;
    g_mouse.min_y = 0;
    g_mouse.max_y = 199;
    g_mouse.cursor = -1;
    g_mouse.installed = false;
}

void mouse_motion(int dx, int dy)
{
    g_mouse.mickey_x += dx;
    g_mouse.mickey_y += dy;
    // mickeys -> pixels using the driver's ratio
    int fx = g_mouse.frac_x + dx * 8;
    int fy = g_mouse.frac_y + dy * 8;
    const int rx = std::max(1, g_mouse.ratio_x.load()), ry = std::max(1, g_mouse.ratio_y.load());
    g_mouse.x += fx / rx;
    g_mouse.y += fy / ry;
    g_mouse.frac_x = fx % rx;
    g_mouse.frac_y = fy % ry;
    clamp_position();
}

void mouse_buttons(int mask)
{
    const int old = g_mouse.buttons.exchange(mask);
    for (int b = 0; b < 3; ++b) {
        if ((mask & (1 << b)) && !(old & (1 << b)))
            ++g_mouse.presses[b];
        if (!(mask & (1 << b)) && (old & (1 << b)))
            ++g_mouse.releases[b];
    }
}

bool mouse_wants_capture() noexcept { return g_mouse.installed.load(); }

void int33_mouse_dispatch()
{
    Regs& r = regs();
    const std::uint16_t fn = r.a.x;
    switch (fn) {
    case 0x00:
    case 0x21:
        mouse_reset_state();
        g_mouse.installed = true;
        r.a.x = 0xFFFF;
        r.b.x = 2;
        break;
    case 0x01: ++g_mouse.cursor; break;
    case 0x02: --g_mouse.cursor; break;
    case 0x03:
        r.b.x = static_cast<std::uint16_t>(g_mouse.buttons.load());
        r.c.x = static_cast<std::uint16_t>(g_mouse.x.load());
        r.d.x = static_cast<std::uint16_t>(g_mouse.y.load());
        break;
    case 0x04:
        g_mouse.x = static_cast<std::int16_t>(r.c.x);
        g_mouse.y = static_cast<std::int16_t>(r.d.x);
        clamp_position();
        break;
    case 0x05:
    case 0x06: {
        const int b = std::min<int>(r.b.x, 2);
        r.a.x = static_cast<std::uint16_t>(g_mouse.buttons.load());
        r.b.x = static_cast<std::uint16_t>(fn == 0x05 ? g_mouse.presses[b].exchange(0) : g_mouse.releases[b].exchange(0));
        r.c.x = static_cast<std::uint16_t>(g_mouse.x.load());
        r.d.x = static_cast<std::uint16_t>(g_mouse.y.load());
        break;
    }
    case 0x07:
        g_mouse.min_x = std::min<std::int16_t>(r.c.x, r.d.x);
        g_mouse.max_x = std::max<std::int16_t>(r.c.x, r.d.x);
        clamp_position();
        break;
    case 0x08:
        g_mouse.min_y = std::min<std::int16_t>(r.c.x, r.d.x);
        g_mouse.max_y = std::max<std::int16_t>(r.c.x, r.d.x);
        clamp_position();
        break;
    case 0x0B:
        r.c.x = static_cast<std::uint16_t>(g_mouse.mickey_x.exchange(0));
        r.d.x = static_cast<std::uint16_t>(g_mouse.mickey_y.exchange(0));
        break;
    case 0x0F:
        g_mouse.ratio_x = std::max<int>(1, r.c.x);
        g_mouse.ratio_y = std::max<int>(1, r.d.x);
        break;
    default:
        break;
    }
}

//=========================================================================
// Game port: writing 201h fires the one-shots, each axis bit then reads 1
// for a number of reads proportional to the stick position (the resistance).
// An unconnected axis never times out, like an open circuit.
//=========================================================================

void joystick_update(int stick, int x, int y, int buttons, bool connected)
{
    if (stick < 0 || stick > 1)
        return;
    g_sticks[stick].x = x;
    g_sticks[stick].y = y;
    g_sticks[stick].buttons = buttons;
    g_sticks[stick].connected = connected;
}

void joystick_port_write(std::uint8_t)
{
    auto count = [](const Stick& s, bool yaxis) -> std::uint16_t {
        if (!s.connected.load())
            return 0xFFFF;
        const int v = yaxis ? s.y.load() : s.x.load();
        return static_cast<std::uint16_t>(4 + (v + 32768) * 72 / 65535);
    };
    g_axis_count[0] = count(g_sticks[0], false);
    g_axis_count[1] = count(g_sticks[0], true);
    g_axis_count[2] = count(g_sticks[1], false);
    g_axis_count[3] = count(g_sticks[1], true);
    g_port201_reads = 0;
}

std::uint8_t joystick_port_read()
{
    const std::uint32_t n = g_port201_reads++;
    std::uint8_t v = 0;
    for (int a = 0; a < 4; ++a)
        if (g_axis_count[a] == 0xFFFF || n < g_axis_count[a])
            v |= static_cast<std::uint8_t>(1 << a);
    const int b0 = g_sticks[0].buttons.load(), b1 = g_sticks[1].buttons.load();
    if (!(b0 & 1)) v |= 0x10;
    if (!(b0 & 2)) v |= 0x20;
    if (!(b1 & 1)) v |= 0x40;
    if (!(b1 & 2)) v |= 0x80;
    return v;
}

}  // namespace dos
