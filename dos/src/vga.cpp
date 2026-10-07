// DOS platform layer - CGA / EGA / VGA emulation.
//
// Emulates what DOS games program directly: BIOS modes 03h-06h, 0Dh, 0Eh,
// 10h, 12h, 13h; planar memory with latches, read modes 0/1, write modes 0-3,
// map mask, set/reset, bit mask, rotate/function; CRTC start address, offset
// and line compare; attribute palette, DAC; the input status register with a
// vertical retrace generated from the real time clock.

#include "dos/machine.hpp"

#include "internal.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace dos {

namespace {

Vga g_vga;

constexpr std::uint8_t kSeqMapMask = 2;
constexpr std::uint8_t kSeqMemMode = 4;
constexpr std::uint8_t kGcSetReset = 0;
constexpr std::uint8_t kGcEnableSetReset = 1;
constexpr std::uint8_t kGcColorCompare = 2;
constexpr std::uint8_t kGcRotate = 3;
constexpr std::uint8_t kGcReadMap = 4;
constexpr std::uint8_t kGcMode = 5;
constexpr std::uint8_t kGcMisc = 6;
constexpr std::uint8_t kGcDontCare = 7;
constexpr std::uint8_t kGcBitMask = 8;

struct ModeInfo {
    std::uint8_t mode;
    int width, height;
    enum Kind { Text, Cga2, Cga4, Planar, Chain4 } kind;
    std::uint8_t offset;   // CRTC offset register
    std::uint8_t max_scan; // CRTC max scan line (VGA)
    bool vga_only;
};

constexpr ModeInfo kModes[] = {
    {0x00, 320, 200, ModeInfo::Text, 20, 0x0F, false},
    {0x01, 320, 200, ModeInfo::Text, 20, 0x0F, false},
    {0x02, 640, 200, ModeInfo::Text, 40, 0x0F, false},
    {0x03, 640, 200, ModeInfo::Text, 40, 0x0F, false},
    {0x04, 320, 200, ModeInfo::Cga4, 20, 0xC1, false},
    {0x05, 320, 200, ModeInfo::Cga4, 20, 0xC1, false},
    {0x06, 640, 200, ModeInfo::Cga2, 40, 0xC1, false},
    {0x0D, 320, 200, ModeInfo::Planar, 20, 0xC1, false},
    {0x0E, 640, 200, ModeInfo::Planar, 40, 0xC1, false},
    {0x10, 640, 350, ModeInfo::Planar, 40, 0x40, false},
    {0x12, 640, 480, ModeInfo::Planar, 40, 0x40, true},
    {0x13, 320, 200, ModeInfo::Chain4, 40, 0x41, true},
};

const ModeInfo& mode_info(std::uint8_t mode)
{
    for (const auto& m : kModes)
        if (m.mode == mode)
            return m;
    return kModes[3];
}

// 6-bit DAC values for the 64 EGA colours as seen in 200 line modes
// (bit 4 = intensity, colour 6 is brown), and in 350/480 line modes (rgbRGB).
void dac_entry_cga(int i, std::uint8_t out[3])
{
    const int in = (i >> 4) & 1;
    int r = ((i >> 2) & 1) * 0x2A + in * 0x15;
    int g = ((i >> 1) & 1) * 0x2A + in * 0x15;
    int b = (i & 1) * 0x2A + in * 0x15;
    if ((i & 0x17) == 0x06)
        g = 0x15;  // brown
    out[0] = static_cast<std::uint8_t>(r);
    out[1] = static_cast<std::uint8_t>(g);
    out[2] = static_cast<std::uint8_t>(b);
}

void dac_entry_ega(int i, std::uint8_t out[3])
{
    out[0] = static_cast<std::uint8_t>(((i >> 2) & 1) * 0x2A + ((i >> 5) & 1) * 0x15);
    out[1] = static_cast<std::uint8_t>(((i >> 1) & 1) * 0x2A + ((i >> 4) & 1) * 0x15);
    out[2] = static_cast<std::uint8_t>((i & 1) * 0x2A + ((i >> 3) & 1) * 0x15);
}

inline std::uint8_t rotate_right(std::uint8_t v, int n)
{
    n &= 7;
    return static_cast<std::uint8_t>((v >> n) | (v << ((8 - n) & 7)));
}

inline std::uint8_t alu(std::uint8_t data, std::uint8_t latch, std::uint8_t func)
{
    switch ((func >> 3) & 3) {
    case 1: return data & latch;
    case 2: return data | latch;
    case 3: return data ^ latch;
    default: return data;
    }
}

inline std::uint32_t to_rgb(const std::uint8_t c[3])
{
    auto e = [](std::uint8_t v) { v &= 0x3F; return static_cast<std::uint32_t>((v << 2) | (v >> 4)); };
    return (e(c[0]) << 16) | (e(c[1]) << 8) | e(c[2]);
}

}  // namespace

Vga& vga() noexcept { return g_vga; }

void Vga::reset(VideoCard card)
{
    card_ = card;
    std::memset(planes_, 0, sizeof(planes_));
    std::memset(cga_, 0, sizeof(cga_));
    polls_same_ = 0;
    last_retrace_ = false;
    set_mode(3, true);
}

void Vga::load_default_dac_16()
{
    const ModeInfo& mi = mode_info(ld(mode_));
    const bool lines200 = mi.height == 200 && mi.kind != ModeInfo::Text;
    for (int i = 0; i < 256; ++i) {
        std::uint8_t c[3];
        if (lines200)
            dac_entry_cga(i & 0x3F, c);
        else
            dac_entry_ega(i & 0x3F, c);
        for (int k = 0; k < 3; ++k)
            st(dac_[i][k], c[k]);
    }
}

void Vga::load_default_dac_256()
{
    static const std::uint8_t ega16[16] = {0, 1, 2, 3, 4, 5, 0x14, 7, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F};
    static const std::uint8_t grays[16] = {0, 5, 8, 11, 14, 17, 20, 24, 28, 32, 36, 40, 45, 50, 56, 63};
    std::uint8_t c[3];
    for (int i = 0; i < 16; ++i) {
        dac_entry_ega(ega16[i], c);
        for (int k = 0; k < 3; ++k)
            st(dac_[i][k], c[k]);
    }
    for (int i = 0; i < 16; ++i)
        for (int k = 0; k < 3; ++k)
            st(dac_[16 + i][k], grays[i]);
    // 9 groups of 24 hues: 3 intensities x 3 saturations
    int idx = 32;
    const int high[3] = {63, 28, 16};
    for (int v = 0; v < 3; ++v) {
        for (int s = 0; s < 3; ++s) {
            const int hi = high[v];
            const int lo = s == 0 ? 0 : (s == 1 ? hi * 31 / 63 : hi * 45 / 63);
            // ramp between lo and hi in 4 steps (plus the two extremes)
            int ramp[5];
            for (int k = 0; k < 5; ++k)
                ramp[k] = lo + (hi - lo) * k / 4;
            for (int h = 0; h < 24; ++h) {
                const int seg = h / 4, step = h % 4;
                int r = lo, g = lo, b = lo;
                switch (seg) {
                case 0: b = hi; r = ramp[step]; break;          // blue -> magenta
                case 1: r = hi; b = ramp[4 - step]; break;      // magenta -> red
                case 2: r = hi; g = ramp[step]; break;          // red -> yellow
                case 3: g = hi; r = ramp[4 - step]; break;      // yellow -> green
                case 4: g = hi; b = ramp[step]; break;          // green -> cyan
                default: b = hi; g = ramp[4 - step]; break;     // cyan -> blue
                }
                st(dac_[idx][0], r);
                st(dac_[idx][1], g);
                st(dac_[idx][2], b);
                ++idx;
            }
        }
    }
    for (; idx < 256; ++idx)
        for (int k = 0; k < 3; ++k)
            st(dac_[idx][k], 0);
}

void Vga::set_mode(std::uint8_t mode, bool clear_memory)
{
    const ModeInfo& mi = mode_info(mode);
    if (card_ == VideoCard::CGA && mi.kind != ModeInfo::Text && mi.kind != ModeInfo::Cga2 && mi.kind != ModeInfo::Cga4)
        return;  // the BIOS ignores modes the card can't do
    if (card_ == VideoCard::EGA && mi.vga_only)
        return;

    st(mode_, mi.mode);

    // sequencer
    for (auto& r : seq_)
        st(r, 0);
    st(seq_[kSeqMapMask], 0x0F);
    st(seq_[kSeqMemMode], mi.kind == ModeInfo::Chain4 ? 0x0E : (mi.kind == ModeInfo::Planar ? 0x06 : 0x02));

    // graphics controller
    for (auto& r : gc_)
        st(r, 0);
    st(gc_[kGcMode], mi.kind == ModeInfo::Chain4 ? 0x40 : (mi.kind == ModeInfo::Cga4 ? 0x30 : 0x00));
    st(gc_[kGcMisc], mi.kind == ModeInfo::Text ? 0x0E : (mi.kind == ModeInfo::Cga2 || mi.kind == ModeInfo::Cga4 ? 0x0F : 0x05));
    st(gc_[kGcDontCare], 0x0F);
    st(gc_[kGcBitMask], 0xFF);
    for (auto& l : latch_)
        st(l, 0);

    // CRTC
    for (auto& r : crtc_)
        st(r, 0);
    st(crtc_[0x13], mi.offset);
    st(crtc_[0x09], card_ == VideoCard::VGA ? mi.max_scan : static_cast<std::uint8_t>(mi.max_scan & 0x40));
    st(crtc_[0x07], 0x10);  // line compare bit 8
    st(crtc_[0x18], 0xFF);  // line compare = 0x3FF (off)
    st(crtc_[0x17], 0xA3);

    // attribute controller
    static const std::uint8_t pal200[16] = {0, 1, 2, 3, 4, 5, 6, 7, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17};
    static const std::uint8_t pal350[16] = {0, 1, 2, 3, 4, 5, 0x14, 7, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F};
    const bool lines200 = mi.height == 200 && mi.kind != ModeInfo::Text;
    for (int i = 0; i < 16; ++i)
        st(attr_[i], mi.kind == ModeInfo::Chain4 ? i : (lines200 ? pal200[i] : pal350[i]));
    st(attr_[0x10], mi.kind == ModeInfo::Chain4 ? 0x41 : (mi.kind == ModeInfo::Text ? 0x0C : 0x01));
    st(attr_[0x11], 0);
    st(attr_[0x12], 0x0F);
    st(attr_[0x13], 0);
    st(attr_[0x14], 0);
    attr_flip_ = false;

    st(cga_mode_ctl_, 0);
    if (mi.kind == ModeInfo::Cga4) {
        set_cga_color_select(0x30);  // palette 1, high intensity (what the BIOS sets)
    } else if (mi.kind == ModeInfo::Cga2) {
        st(cga_color_, 0x0F);
        st(attr_[0], 0);
        st(attr_[1], 0x17);
    }

    // DAC
    if (mi.kind == ModeInfo::Chain4)
        load_default_dac_256();
    else
        load_default_dac_16();
    st(dac_read_, 0);
    st(dac_write_, 0);
    dac_step_ = dac_read_step_ = 0;

    if (clear_memory) {
        if (mi.kind == ModeInfo::Planar || mi.kind == ModeInfo::Chain4) {
            for (auto& plane : planes_)
                for (auto& b : plane)
                    st(b, 0);
        } else if (mi.kind == ModeInfo::Text) {
            for (std::size_t i = 0; i < sizeof(cga_); i += 2) {
                st(cga_[i], ' ');
                st(cga_[i + 1], 0x07);
            }
        } else {
            for (auto& b : cga_)
                st(b, 0);
        }
    }
}

//=========================================================================
// Memory
//=========================================================================

std::uint8_t Vga::mem_read(std::uint32_t addr) noexcept
{
    const std::uint8_t mode = ld(mode_);
    if (mode < 0x0D) {
        if (addr < 0xB8000 || (card_ == VideoCard::CGA && addr >= 0xBC000))
            return 0xFF;
        return ld(cga_[(addr - 0xB8000) & 0x7FFF]);
    }
    if (addr >= 0xB0000)
        return 0xFF;
    std::uint32_t off = addr - kVideoBase;

    if (ld(seq_[kSeqMemMode]) & 0x08) {  // chain 4
        const std::uint32_t o = (off >> 2) & 0xFFFF;
        for (int p = 0; p < 4; ++p)
            st(latch_[p], ld(planes_[p][o]));
        return ld(planes_[off & 3][o]);
    }

    off &= 0xFFFF;
    for (int p = 0; p < 4; ++p)
        st(latch_[p], ld(planes_[p][off]));

    const std::uint8_t gmode = ld(gc_[kGcMode]);
    if (!(gmode & 0x08))
        return ld(latch_[ld(gc_[kGcReadMap]) & 3]);

    // read mode 1: colour compare
    const std::uint8_t cc = ld(gc_[kGcColorCompare]);
    const std::uint8_t care = ld(gc_[kGcDontCare]);
    std::uint8_t result = 0xFF;
    for (int p = 0; p < 4; ++p) {
        if (!(care & (1 << p)))
            continue;
        const std::uint8_t want = (cc & (1 << p)) ? 0xFF : 0x00;
        result &= static_cast<std::uint8_t>(~(ld(latch_[p]) ^ want));
    }
    return result;
}

void Vga::mem_write(std::uint32_t addr, std::uint8_t v) noexcept
{
    const std::uint8_t mode = ld(mode_);
    if (mode < 0x0D) {
        if (addr < 0xB8000 || (card_ == VideoCard::CGA && addr >= 0xBC000))
            return;
        st(cga_[(addr - 0xB8000) & 0x7FFF], v);
        return;
    }
    if (addr >= 0xB0000)
        return;
    std::uint32_t off = addr - kVideoBase;
    const std::uint8_t map_mask = ld(seq_[kSeqMapMask]);

    if (ld(seq_[kSeqMemMode]) & 0x08) {  // chain 4
        const int p = off & 3;
        if (map_mask & (1 << p))
            st(planes_[p][(off >> 2) & 0xFFFF], v);
        return;
    }

    off &= 0xFFFF;
    const std::uint8_t gmode = ld(gc_[kGcMode]);
    const std::uint8_t func = ld(gc_[kGcRotate]);
    const std::uint8_t bit_mask = ld(gc_[kGcBitMask]);
    const std::uint8_t set_reset = ld(gc_[kGcSetReset]);
    const std::uint8_t enable_sr = ld(gc_[kGcEnableSetReset]);

    for (int p = 0; p < 4; ++p) {
        if (!(map_mask & (1 << p)))
            continue;
        const std::uint8_t latch = ld(latch_[p]);
        std::uint8_t d;
        switch (gmode & 3) {
        case 0: {
            d = (enable_sr & (1 << p)) ? ((set_reset & (1 << p)) ? 0xFF : 0x00) : rotate_right(v, func & 7);
            d = alu(d, latch, func);
            d = static_cast<std::uint8_t>((d & bit_mask) | (latch & ~bit_mask));
            break;
        }
        case 1:
            d = latch;
            break;
        case 2: {
            d = (v & (1 << p)) ? 0xFF : 0x00;
            d = alu(d, latch, func);
            d = static_cast<std::uint8_t>((d & bit_mask) | (latch & ~bit_mask));
            break;
        }
        default: {
            const std::uint8_t m = rotate_right(v, func & 7) & bit_mask;
            d = (set_reset & (1 << p)) ? 0xFF : 0x00;
            d = alu(d, latch, func);
            d = static_cast<std::uint8_t>((d & m) | (latch & ~m));
            break;
        }
        }
        st(planes_[p][off], d);
    }
}

//=========================================================================
// Ports
//=========================================================================

double Vga::refresh_hz() const noexcept
{
    const ModeInfo& mi = mode_info(ld(mode_));
    if (card_ == VideoCard::VGA)
        return mi.height == 480 ? 59.94 : 70.086;
    if (mi.height == 350)
        return 60.0;
    return 59.923;  // 15.7 kHz CGA timing (262 lines)
}

std::uint64_t Vga::retrace_count() const noexcept
{
    return static_cast<std::uint64_t>(now() * refresh_hz());
}

std::uint8_t Vga::status_read()
{
    attr_flip_ = false;

    const double hz = refresh_hz();
    const double period = 1.0 / hz;
    // vertical sync lasts a couple of scan lines
    const double retrace_len = card_ == VideoCard::VGA ? period * 2.0 / 449.0 : period * 3.0 / 262.0;

    double t = now();
    double frame = std::floor(t * hz);
    double phase = t - frame * period;
    bool retrace = phase < retrace_len;

    // A program polling the status register is waiting for a state change:
    // sleep until it happens instead of spinning.
    if (retrace == last_retrace_)
        ++polls_same_;
    else
        polls_same_ = 0;

    if (polls_same_ >= 2) {
        if (!retrace) {
            sleep_until((frame + 1) * period);
            retrace = true;
        } else {
            sleep_until(frame * period + retrace_len);
            retrace = false;
        }
        polls_same_ = 0;
    }

    if (retrace && !last_retrace_)
        on_retrace();
    last_retrace_ = retrace;

    std::uint8_t v = 0;
    if (retrace)
        v |= 0x09;
    else
        v |= (hblank_toggle_ ^= 1);  // display enable toggles with horizontal blanking
    return v;
}

bool Vga::port_read(std::uint16_t port, std::uint8_t& v)
{
    if (port == 0x3DA || port == 0x3BA) {
        poll();
        v = status_read();
        return true;
    }
    if (card_ == VideoCard::CGA) {
        switch (port) {
        case 0x3D5: v = ld(crtc_[crtc_index_ & 0x1F]); return true;
        default: return false;
        }
    }
    switch (port) {
    case 0x3C0: v = attr_index_; return true;
    case 0x3C1: v = ld(attr_[attr_index_ & 0x1F]); return true;
    case 0x3C2: v = 0x10; return true;
    case 0x3C4: v = seq_index_; return true;
    case 0x3C5: v = ld(seq_[seq_index_ & 7]); return true;
    case 0x3C6: v = 0xFF; return true;
    case 0x3C7: v = dac_read_step_ ? 0x00 : 0x03; return true;
    case 0x3C8: v = ld(dac_write_); return true;
    case 0x3C9: {
        if (card_ != VideoCard::VGA)
            return false;
        const std::uint8_t idx = ld(dac_read_);
        v = ld(dac_[idx][dac_read_step_]);
        if (++dac_read_step_ == 3) {
            dac_read_step_ = 0;
            st(dac_read_, static_cast<std::uint8_t>(idx + 1));
        }
        return true;
    }
    case 0x3CC: v = misc_out_; return true;
    case 0x3CE: v = gc_index_; return true;
    case 0x3CF: v = ld(gc_[gc_index_ & 0x0F]); return true;
    case 0x3B4:
    case 0x3D4: v = crtc_index_; return true;
    case 0x3B5:
    case 0x3D5: v = ld(crtc_[crtc_index_ & 0x1F]); return true;
    default: return false;
    }
}

void Vga::set_dac(int index, std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept
{
    index &= 0xFF;
    st(dac_[index][0], r & 0x3F);
    st(dac_[index][1], g & 0x3F);
    st(dac_[index][2], b & 0x3F);
}

void Vga::set_palette_reg(int index, std::uint8_t v) noexcept { st(attr_[index & 0x1F], v); }

void Vga::set_cga_color_select(std::uint8_t v) noexcept
{
    st(cga_color_, v);
    const ModeInfo& mi = mode_info(ld(mode_));
    // Express the CGA colour select register through the attribute palette.
    auto irgb = [](int c) { return static_cast<std::uint8_t>((c & 7) | ((c & 8) << 1)); };
    if (mi.kind == ModeInfo::Cga4) {
        const int intensity = (v & 0x10) ? 8 : 0;
        const bool pal1 = (v & 0x20) != 0;
        st(attr_[0], irgb(v & 0x0F));
        st(attr_[1], irgb((pal1 ? 3 : 2) | intensity));
        st(attr_[2], irgb((pal1 ? 5 : 4) | intensity));
        st(attr_[3], irgb((pal1 ? 7 : 6) | intensity));
    } else if (mi.kind == ModeInfo::Cga2) {
        st(attr_[0], 0);
        st(attr_[1], irgb(v & 0x0F));
    }
}

bool Vga::port_write(std::uint16_t port, std::uint8_t v)
{
    if (port == 0x3D8) {
        st(cga_mode_ctl_, v);
        return true;
    }
    if (port == 0x3D9) {
        set_cga_color_select(v);
        return true;
    }
    if (port == 0x3D4 || port == 0x3B4) {
        crtc_index_ = v & 0x1F;
        return true;
    }
    if (port == 0x3D5 || port == 0x3B5) {
        st(crtc_[crtc_index_ & 0x1F], v);
        return true;
    }
    if (card_ == VideoCard::CGA)
        return false;

    switch (port) {
    case 0x3C0:
        if (!attr_flip_)
            attr_index_ = v & 0x3F;
        else
            st(attr_[attr_index_ & 0x1F], v);
        attr_flip_ = !attr_flip_;
        return true;
    case 0x3C2: misc_out_ = v; return true;
    case 0x3C4: seq_index_ = v & 7; return true;
    case 0x3C5: st(seq_[seq_index_ & 7], v); return true;
    case 0x3C6: return true;
    case 0x3C7:
        if (card_ != VideoCard::VGA) return false;
        st(dac_read_, v);
        dac_read_step_ = 0;
        return true;
    case 0x3C8:
        if (card_ != VideoCard::VGA) return false;
        st(dac_write_, v);
        dac_step_ = 0;
        return true;
    case 0x3C9: {
        if (card_ != VideoCard::VGA) return false;
        dac_pending_[dac_step_] = v & 0x3F;
        if (++dac_step_ == 3) {
            dac_step_ = 0;
            const std::uint8_t idx = ld(dac_write_);
            set_dac(idx, dac_pending_[0], dac_pending_[1], dac_pending_[2]);
            st(dac_write_, static_cast<std::uint8_t>(idx + 1));
        }
        return true;
    }
    case 0x3CE: gc_index_ = v & 0x0F; return true;
    case 0x3CF: st(gc_[gc_index_ & 0x0F], v); return true;
    default: return false;
    }
}

//=========================================================================
// Scan out
//=========================================================================

void Vga::snapshot(Frame& out) const
{
    const std::uint8_t mode = ld(mode_);
    const ModeInfo& mi = mode_info(mode);

    out.width = mi.width;
    out.height = mi.height;
    out.display_aspect = 4.0 / 3.0;
    if (mi.kind == ModeInfo::Text) {
        out.width = 640;
        out.height = 400;
    }
    out.pixels.assign(static_cast<std::size_t>(out.width) * out.height, 0);

    for (int i = 0; i < 256; ++i) {
        std::uint8_t c[3] = {ld(dac_[i][0]), ld(dac_[i][1]), ld(dac_[i][2])};
        out.palette[i] = to_rgb(c);
    }

    auto attr = [&](int c) -> std::uint8_t {
        if (mi.kind == ModeInfo::Chain4)
            return static_cast<std::uint8_t>(c);
        return static_cast<std::uint8_t>(ld(attr_[c & 0x0F]) & 0x3F);
    };

    std::uint8_t* px = out.pixels.data();
    switch (mi.kind) {
    case ModeInfo::Text:
        // No ROM font: show the background colour of each cell.
        for (int y = 0; y < out.height; ++y)
            for (int x = 0; x < out.width; ++x) {
                const int cell = (y / 16) * 80 + x / 8;
                const std::uint8_t a = ld(cga_[(cell * 2 + 1) & 0x7FFF]);
                px[y * out.width + x] = attr((a >> 4) & 7);
            }
        break;

    case ModeInfo::Cga4:
        for (int y = 0; y < 200; ++y) {
            const std::uint8_t* row = cga_ + (y & 1) * 0x2000 + (y >> 1) * 80;
            for (int x = 0; x < 320; ++x) {
                const int c = (ld(row[x >> 2]) >> (6 - 2 * (x & 3))) & 3;
                px[y * 320 + x] = attr(c);
            }
        }
        break;

    case ModeInfo::Cga2:
        for (int y = 0; y < 200; ++y) {
            const std::uint8_t* row = cga_ + (y & 1) * 0x2000 + (y >> 1) * 80;
            for (int x = 0; x < 640; ++x) {
                const int c = (ld(row[x >> 3]) >> (7 - (x & 7))) & 1;
                px[y * 640 + x] = attr(c);
            }
        }
        break;

    case ModeInfo::Planar:
    case ModeInfo::Chain4: {
        const std::uint32_t start = (ld(crtc_[0x0C]) << 8) | ld(crtc_[0x0D]);
        const std::uint32_t line = ld(crtc_[0x13]) * 2u;
        const std::uint32_t lc = ld(crtc_[0x18]) | ((ld(crtc_[0x07]) & 0x10) << 4) | ((ld(crtc_[0x09]) & 0x40) << 3);
        const int scan_per_line = card_ == VideoCard::VGA && mi.height <= 240 ? (ld(crtc_[0x09]) & 0x1F) + 1 : 1;
        const std::uint8_t plane_enable = ld(attr_[0x12]) & 0x0F;
        int split_y = -1;
        for (int y = 0; y < mi.height; ++y) {
            if (split_y < 0 && static_cast<std::uint32_t>(y * scan_per_line) > lc)
                split_y = y;
            const std::uint32_t base = split_y >= 0 ? (y - split_y) * line : start + y * line;
            std::uint8_t* dst = px + y * mi.width;
            if (mi.kind == ModeInfo::Chain4) {
                for (int x = 0; x < mi.width; ++x)
                    dst[x] = ld(planes_[x & 3][(base + (x >> 2)) & 0xFFFF]);
            } else {
                for (int xb = 0; xb < mi.width / 8; ++xb) {
                    const std::uint32_t o = (base + xb) & 0xFFFF;
                    const std::uint8_t p0 = ld(planes_[0][o]), p1 = ld(planes_[1][o]);
                    const std::uint8_t p2 = ld(planes_[2][o]), p3 = ld(planes_[3][o]);
                    for (int bit = 0; bit < 8; ++bit) {
                        const int s = 7 - bit;
                        int c = ((p0 >> s) & 1) | (((p1 >> s) & 1) << 1) | (((p2 >> s) & 1) << 2) | (((p3 >> s) & 1) << 3);
                        dst[xb * 8 + bit] = attr(c & plane_enable);
                    }
                }
            }
        }
        break;
    }
    }
    out.serial = retrace_count();
}

}  // namespace dos
