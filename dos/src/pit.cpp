// DOS platform layer - 8253 programmable interval timer and PC speaker.
//
// Channel 0 drives IRQ0 (INT 8) at the programmed rate, channel 2 drives the
// speaker gated by port 61h. Time advances in generate(), which is called by
// the audio thread so IRQ0 and the speaker are sample accurate.

#include "dos/machine.hpp"

#include "internal.hpp"

#include <algorithm>
#include <cmath>

namespace dos {

namespace {
Pit g_pit;
std::atomic<std::uint32_t> g_ch0_reload{0};  // bit 16 set = pending, low 16 = divisor
std::atomic<std::uint16_t> g_ch0_count{0};
std::atomic<std::uint8_t> g_refresh_toggle{0};
}  // namespace

Pit& pit() noexcept { return g_pit; }

void Pit::reset()
{
    for (int i = 0; i < 3; ++i) {
        st(div_[i], 0);
        st(access_[i], 3);
        st(mode_[i], 3);
        hi_next_[i] = false;
        latched_[i] = false;
        read_hi_next_[i] = false;
    }
    st(port61_, 0);
    g_ch0_reload.store(0x10000);
}

bool Pit::port_read(std::uint16_t port, std::uint8_t& v)
{
    if (port == 0x61) {
        const std::uint8_t t = g_refresh_toggle.fetch_xor(0x30) ^ 0x30;
        v = static_cast<std::uint8_t>((ld(port61_) & 0x0F) | t);
        return true;
    }
    if (port < 0x40 || port > 0x42)
        return false;
    const int ch = port - 0x40;
    std::uint16_t value = latched_[ch] ? latch_value_[ch] : (ch == 0 ? g_ch0_count.load() : ld(div_[ch]));
    const std::uint8_t access = ld(access_[ch]);
    if (access == 1) {
        v = static_cast<std::uint8_t>(value);
        latched_[ch] = false;
    } else if (access == 2) {
        v = static_cast<std::uint8_t>(value >> 8);
        latched_[ch] = false;
    } else {
        v = read_hi_next_[ch] ? static_cast<std::uint8_t>(value >> 8) : static_cast<std::uint8_t>(value);
        if (read_hi_next_[ch])
            latched_[ch] = false;
        read_hi_next_[ch] = !read_hi_next_[ch];
    }
    return true;
}

bool Pit::port_write(std::uint16_t port, std::uint8_t v)
{
    if (port == 0x61) {
        st(port61_, v);
        return true;
    }
    if (port == 0x43) {
        const int ch = v >> 6;
        if (ch == 3)
            return true;  // 8254 read-back: not used by DOS games of the era
        const int access = (v >> 4) & 3;
        if (access == 0) {  // counter latch
            latched_[ch] = true;
            latch_value_[ch] = ch == 0 ? g_ch0_count.load() : ld(div_[ch]);
            read_hi_next_[ch] = false;
            return true;
        }
        st(access_[ch], access);
        st(mode_[ch], (v >> 1) & 7);
        hi_next_[ch] = false;
        read_hi_next_[ch] = false;
        return true;
    }
    if (port < 0x40 || port > 0x42)
        return false;

    const int ch = port - 0x40;
    const std::uint8_t access = ld(access_[ch]);
    std::uint16_t d = ld(div_[ch]);
    bool complete = true;
    if (access == 1) {
        d = v;
    } else if (access == 2) {
        d = static_cast<std::uint16_t>(v << 8);
    } else if (!hi_next_[ch]) {
        latch_lo_[ch] = v;
        hi_next_[ch] = true;
        complete = false;
    } else {
        d = static_cast<std::uint16_t>(latch_lo_[ch] | (v << 8));
        hi_next_[ch] = false;
    }
    if (complete) {
        st(div_[ch], d);
        if (ch == 0)
            g_ch0_reload.store(0x10000u | d);  // counting restarts with the new count
    }
    return true;
}

void Pit::generate(float* out, int frames, int sample_rate)
{
    const double ticks_per_sample = kClock / sample_rate;

    for (int i = 0; i < frames; ++i) {
        if (const std::uint32_t r = g_ch0_reload.exchange(0); r & 0x10000) {
            const std::uint16_t d = static_cast<std::uint16_t>(r);
            ch0_remaining_ = d ? d : 65536.0;
        }

        double remaining = ticks_per_sample;
        double high = 0.0;
        while (remaining > 0.0) {
            const double step = std::min(remaining, ch0_remaining_);

            // speaker: integrate the channel 2 output gated by port 61h
            const std::uint8_t p61 = ld(port61_);
            const std::uint16_t d2 = ld(div_[2]);
            const double period = d2 ? d2 : 65536.0;
            if (p61 & 1) {
                double t = step;
                if (t >= period) {
                    const double whole = std::floor(t / period);
                    if (p61 & 2)
                        high += whole * std::ceil(period / 2);
                    t -= whole * period;
                }
                while (t > 0.0) {
                    const double half = std::ceil(period / 2);
                    double seg;
                    if (ch2_phase_ < half) {
                        seg = std::min(t, half - ch2_phase_);
                        if (p61 & 2)
                            high += seg;
                    } else {
                        seg = std::min(t, period - ch2_phase_);
                    }
                    ch2_phase_ += seg;
                    t -= seg;
                    if (ch2_phase_ >= period)
                        ch2_phase_ -= period;
                }
            } else if (p61 & 2) {
                high += step;  // gate low: output stays high in mode 3
            }

            remaining -= step;
            ch0_remaining_ -= step;
            if (ch0_remaining_ <= 0.0) {
                const std::uint16_t d0 = ld(div_[0]);
                ch0_remaining_ += d0 ? d0 : 65536.0;
                hardware_irq(8);
                if (const std::uint32_t r = g_ch0_reload.exchange(0); r & 0x10000) {
                    const std::uint16_t d = static_cast<std::uint16_t>(r);
                    ch0_remaining_ = d ? d : 65536.0;
                }
            }
        }
        g_ch0_count.store(static_cast<std::uint16_t>(std::max(0.0, ch0_remaining_)));

        // DC blocking high-pass so a parked speaker cone is silent
        const double in = high / ticks_per_sample;
        const double y = in - hp_prev_in_ + 0.995 * hp_prev_out_;
        hp_prev_in_ = in;
        hp_prev_out_ = y;
        out[i] = static_cast<float>(y * 0.3);
    }
}

}  // namespace dos
