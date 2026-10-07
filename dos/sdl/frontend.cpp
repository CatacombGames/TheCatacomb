// DOS platform layer - SDL3 host: window, audio, input, program thread.

#include "frontend.hpp"

#include "dos/runtime.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace dos::sdl {

namespace {

//=========================================================================
// Command line
//=========================================================================

struct Options {
    std::filesystem::path data_dir;
    bool data_dir_given = false;
    VideoCard card = VideoCard::VGA;
    int scale = 3;
    bool fullscreen = false;
    bool aspect = true;
    enum class Filter { Sharp, Nearest, Linear } filter = Filter::Sharp;
    bool mute = false;
    bool mouse = true;
    std::vector<char*> program_args;
};

void print_usage(const char* exe)
{
    std::printf(
        "usage: %s [options] [data directory] [program arguments]\n"
        "\n"
        "  --data <dir>            game data directory (default: current directory)\n"
        "  --video cga|ega|vga     emulated video card (default: vga)\n"
        "  --scale <n>             initial window scale (default: 3)\n"
        "  --fullscreen            start in fullscreen (toggle with Alt+Enter)\n"
        "  --no-aspect             square pixels instead of 4:3\n"
        "  --filter sharp|nearest|linear\n"
        "  --mute                  no sound\n"
        "  --no-mouse              no mouse driver\n"
        "  --help\n",
        exe);
}

// returns -1 to run, otherwise the exit code
int parse_options(int argc, char** argv, Options& o)
{
    o.program_args.push_back(argv[0]);
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto value = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "%s needs a value\n", name);
                return nullptr;
            }
            return argv[++i];
        };
        if (a == "--help" || a == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (a == "--data") {
            const char* v = value("--data");
            if (!v)
                return 1;
            o.data_dir = v;
            o.data_dir_given = true;
        } else if (a == "--video") {
            const char* v = value("--video");
            if (!v)
                return 1;
            const std::string s = v;
            if (s == "cga")
                o.card = VideoCard::CGA;
            else if (s == "ega")
                o.card = VideoCard::EGA;
            else if (s == "vga")
                o.card = VideoCard::VGA;
            else {
                std::fprintf(stderr, "unknown video card '%s'\n", v);
                return 1;
            }
        } else if (a == "--scale") {
            const char* v = value("--scale");
            if (!v)
                return 1;
            o.scale = std::clamp(std::atoi(v), 1, 10);
        } else if (a == "--fullscreen") {
            o.fullscreen = true;
        } else if (a == "--no-aspect") {
            o.aspect = false;
        } else if (a == "--filter") {
            const char* v = value("--filter");
            if (!v)
                return 1;
            const std::string s = v;
            o.filter = s == "nearest" ? Options::Filter::Nearest : (s == "linear" ? Options::Filter::Linear : Options::Filter::Sharp);
        } else if (a == "--mute") {
            o.mute = true;
        } else if (a == "--no-mouse") {
            o.mouse = false;
        } else if (!o.data_dir_given && a.rfind("--", 0) != 0) {
            std::error_code ec;
            if (std::filesystem::is_directory(a, ec)) {
                o.data_dir = a;
                o.data_dir_given = true;
                continue;
            }
            o.program_args.push_back(argv[i]);
        } else {
            o.program_args.push_back(argv[i]);
        }
    }
    o.program_args.push_back(nullptr);
    return -1;
}

//=========================================================================
// Keyboard: SDL scan codes -> XT set 1 (0x100 = E0 prefixed)
//=========================================================================

int xt_scancode(SDL_Scancode sc)
{
    switch (sc) {
    case SDL_SCANCODE_ESCAPE: return 0x01;
    case SDL_SCANCODE_1: return 0x02;
    case SDL_SCANCODE_2: return 0x03;
    case SDL_SCANCODE_3: return 0x04;
    case SDL_SCANCODE_4: return 0x05;
    case SDL_SCANCODE_5: return 0x06;
    case SDL_SCANCODE_6: return 0x07;
    case SDL_SCANCODE_7: return 0x08;
    case SDL_SCANCODE_8: return 0x09;
    case SDL_SCANCODE_9: return 0x0A;
    case SDL_SCANCODE_0: return 0x0B;
    case SDL_SCANCODE_MINUS: return 0x0C;
    case SDL_SCANCODE_EQUALS: return 0x0D;
    case SDL_SCANCODE_BACKSPACE: return 0x0E;
    case SDL_SCANCODE_TAB: return 0x0F;
    case SDL_SCANCODE_Q: return 0x10;
    case SDL_SCANCODE_W: return 0x11;
    case SDL_SCANCODE_E: return 0x12;
    case SDL_SCANCODE_R: return 0x13;
    case SDL_SCANCODE_T: return 0x14;
    case SDL_SCANCODE_Y: return 0x15;
    case SDL_SCANCODE_U: return 0x16;
    case SDL_SCANCODE_I: return 0x17;
    case SDL_SCANCODE_O: return 0x18;
    case SDL_SCANCODE_P: return 0x19;
    case SDL_SCANCODE_LEFTBRACKET: return 0x1A;
    case SDL_SCANCODE_RIGHTBRACKET: return 0x1B;
    case SDL_SCANCODE_RETURN: return 0x1C;
    case SDL_SCANCODE_LCTRL: return 0x1D;
    case SDL_SCANCODE_A: return 0x1E;
    case SDL_SCANCODE_S: return 0x1F;
    case SDL_SCANCODE_D: return 0x20;
    case SDL_SCANCODE_F: return 0x21;
    case SDL_SCANCODE_G: return 0x22;
    case SDL_SCANCODE_H: return 0x23;
    case SDL_SCANCODE_J: return 0x24;
    case SDL_SCANCODE_K: return 0x25;
    case SDL_SCANCODE_L: return 0x26;
    case SDL_SCANCODE_SEMICOLON: return 0x27;
    case SDL_SCANCODE_APOSTROPHE: return 0x28;
    case SDL_SCANCODE_GRAVE: return 0x29;
    case SDL_SCANCODE_LSHIFT: return 0x2A;
    case SDL_SCANCODE_BACKSLASH: return 0x2B;
    case SDL_SCANCODE_NONUSHASH: return 0x2B;
    case SDL_SCANCODE_Z: return 0x2C;
    case SDL_SCANCODE_X: return 0x2D;
    case SDL_SCANCODE_C: return 0x2E;
    case SDL_SCANCODE_V: return 0x2F;
    case SDL_SCANCODE_B: return 0x30;
    case SDL_SCANCODE_N: return 0x31;
    case SDL_SCANCODE_M: return 0x32;
    case SDL_SCANCODE_COMMA: return 0x33;
    case SDL_SCANCODE_PERIOD: return 0x34;
    case SDL_SCANCODE_SLASH: return 0x35;
    case SDL_SCANCODE_RSHIFT: return 0x36;
    case SDL_SCANCODE_KP_MULTIPLY: return 0x37;
    case SDL_SCANCODE_LALT: return 0x38;
    case SDL_SCANCODE_SPACE: return 0x39;
    case SDL_SCANCODE_CAPSLOCK: return 0x3A;
    case SDL_SCANCODE_F1: return 0x3B;
    case SDL_SCANCODE_F2: return 0x3C;
    case SDL_SCANCODE_F3: return 0x3D;
    case SDL_SCANCODE_F4: return 0x3E;
    case SDL_SCANCODE_F5: return 0x3F;
    case SDL_SCANCODE_F6: return 0x40;
    case SDL_SCANCODE_F7: return 0x41;
    case SDL_SCANCODE_F8: return 0x42;
    case SDL_SCANCODE_F9: return 0x43;
    case SDL_SCANCODE_F10: return 0x44;
    case SDL_SCANCODE_NUMLOCKCLEAR: return 0x45;
    case SDL_SCANCODE_SCROLLLOCK: return 0x46;
    case SDL_SCANCODE_KP_7: return 0x47;
    case SDL_SCANCODE_KP_8: return 0x48;
    case SDL_SCANCODE_KP_9: return 0x49;
    case SDL_SCANCODE_KP_MINUS: return 0x4A;
    case SDL_SCANCODE_KP_4: return 0x4B;
    case SDL_SCANCODE_KP_5: return 0x4C;
    case SDL_SCANCODE_KP_6: return 0x4D;
    case SDL_SCANCODE_KP_PLUS: return 0x4E;
    case SDL_SCANCODE_KP_1: return 0x4F;
    case SDL_SCANCODE_KP_2: return 0x50;
    case SDL_SCANCODE_KP_3: return 0x51;
    case SDL_SCANCODE_KP_0: return 0x52;
    case SDL_SCANCODE_KP_PERIOD: return 0x53;
    case SDL_SCANCODE_NONUSBACKSLASH: return 0x56;
    case SDL_SCANCODE_F11: return 0x57;
    case SDL_SCANCODE_F12: return 0x58;
    case SDL_SCANCODE_KP_ENTER: return 0x11C;
    case SDL_SCANCODE_RCTRL: return 0x11D;
    case SDL_SCANCODE_KP_DIVIDE: return 0x135;
    case SDL_SCANCODE_PRINTSCREEN: return 0x137;
    case SDL_SCANCODE_RALT: return 0x138;
    case SDL_SCANCODE_HOME: return 0x147;
    case SDL_SCANCODE_UP: return 0x148;
    case SDL_SCANCODE_PAGEUP: return 0x149;
    case SDL_SCANCODE_LEFT: return 0x14B;
    case SDL_SCANCODE_RIGHT: return 0x14D;
    case SDL_SCANCODE_END: return 0x14F;
    case SDL_SCANCODE_DOWN: return 0x150;
    case SDL_SCANCODE_PAGEDOWN: return 0x151;
    case SDL_SCANCODE_INSERT: return 0x152;
    case SDL_SCANCODE_DELETE: return 0x153;
    case SDL_SCANCODE_LGUI: return 0x15B;
    case SDL_SCANCODE_RGUI: return 0x15C;
    default: return 0;
    }
}

void send_key(int xt, bool down)
{
    if (!xt)
        return;
    if (xt & 0x100)
        keyboard_scancode(0xE0);
    keyboard_scancode(static_cast<std::uint8_t>((xt & 0x7F) | (down ? 0 : 0x80)));
}

//=========================================================================
// Host
//=========================================================================

class Host {
public:
    explicit Host(Options opts) : o_(std::move(opts)) {}
    int run();

private:
    bool init_video();
    void init_audio();
    void handle_event(const SDL_Event& e);
    void update_gamepads();
    void render();
    void release_all_keys();
    std::filesystem::path find_data_dir();
    bool check_required_files();

    Options o_;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* screen_ = nullptr;
    SDL_Texture* scaled_ = nullptr;
    int screen_w_ = 0, screen_h_ = 0, scaled_w_ = 0, scaled_h_ = 0;
    std::vector<std::uint32_t> rgb_;
    Frame frame_;
    std::uint64_t frame_serial_ = 0;

    SDL_AudioStream* audio_ = nullptr;
    std::thread timer_thread_;
    std::atomic<bool> timer_running_{false};

    std::thread program_;
    std::atomic<bool> program_done_{false};
    int exit_code_ = 0;
    double quit_deadline_ = -1;

    bool keys_down_[0x200]{};
    SDL_Gamepad* pads_[2]{};
    bool mouse_captured_ = false;
};

void SDLCALL audio_callback(void* userdata, SDL_AudioStream* stream, int additional, int)
{
    const bool mute = *static_cast<bool*>(userdata);
    static thread_local std::vector<float> buf;
    const int frames = additional / static_cast<int>(sizeof(float));
    if (frames <= 0)
        return;
    buf.resize(static_cast<std::size_t>(frames));
    pit().generate(buf.data(), frames, 48000);
    if (mute)
        std::fill(buf.begin(), buf.end(), 0.0f);
    SDL_PutAudioStreamData(stream, buf.data(), frames * static_cast<int>(sizeof(float)));
}

std::filesystem::path Host::find_data_dir()
{
    if (o_.data_dir_given)
        return o_.data_dir;
    std::error_code ec;
    const auto cwd = std::filesystem::current_path(ec);
    const char* const* req = dos_app_info.required_files;
    auto has_all = [&](const std::filesystem::path& dir) {
        fs::set_data_dir(dir);
        for (const char* const* f = req; f && *f; ++f)
            if (!fs::exists(*f))
                return false;
        return true;
    };
    if (has_all(cwd))
        return cwd;
    if (const char* base = SDL_GetBasePath()) {
        const std::filesystem::path b = base;
        if (has_all(b))
            return b;
    }
    return cwd;
}

bool Host::check_required_files()
{
    std::string missing;
    for (const char* const* f = dos_app_info.required_files; f && *f; ++f)
        if (!fs::exists(*f)) {
            missing += "  ";
            missing += *f;
            missing += "\n";
        }
    if (missing.empty())
        return true;
    std::string msg = std::string("The game data files were not found in\n  ") + fs::data_dir().string() +
                      "\n\nMissing:\n" + missing +
                      "\nRun the program from the game directory or pass the directory on the command line.";
    std::fprintf(stderr, "%s\n", msg.c_str());
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, dos_app_info.title, msg.c_str(), nullptr);
    return false;
}

bool Host::init_video()
{
    const int w = 320 * o_.scale;
    const int h = (o_.aspect ? 240 : 200) * o_.scale;
    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (o_.fullscreen)
        flags |= SDL_WINDOW_FULLSCREEN;
    if (!SDL_CreateWindowAndRenderer(dos_app_info.title, w, h, flags, &window_, &renderer_)) {
        std::fprintf(stderr, "SDL_CreateWindowAndRenderer: %s\n", SDL_GetError());
        return false;
    }
    if (!SDL_SetRenderVSync(renderer_, 1))
        SDL_SetRenderVSync(renderer_, SDL_RENDERER_VSYNC_ADAPTIVE);
    SDL_SetWindowMinimumSize(window_, 320, 200);
    return true;
}

void Host::init_audio()
{
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, "512");
    const SDL_AudioSpec spec{SDL_AUDIO_F32, 1, 48000};
    audio_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, &audio_callback, &o_.mute);
    if (audio_) {
        SDL_ResumeAudioStreamDevice(audio_);
        return;
    }
    // No audio device: keep the timer running in real time anyway.
    std::fprintf(stderr, "no audio: %s\n", SDL_GetError());
    timer_running_ = true;
    timer_thread_ = std::thread([this] {
        std::vector<float> buf;
        double t = now();
        while (timer_running_) {
            const double n = now();
            const int frames = static_cast<int>((n - t) * 48000);
            if (frames > 0) {
                buf.resize(static_cast<std::size_t>(frames));
                pit().generate(buf.data(), frames, 48000);
                t += frames / 48000.0;
            }
            SDL_DelayPrecise(1000000);
        }
    });
}

void Host::release_all_keys()
{
    for (int k = 0; k < 0x200; ++k)
        if (keys_down_[k]) {
            keys_down_[k] = false;
            send_key(k, false);
        }
}

void Host::handle_event(const SDL_Event& e)
{
    switch (e.type) {
    case SDL_EVENT_QUIT:
        request_quit();
        if (quit_deadline_ < 0)
            quit_deadline_ = now() + 2.0;
        break;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
        const bool down = e.type == SDL_EVENT_KEY_DOWN;
        if (down && e.key.scancode == SDL_SCANCODE_RETURN && (e.key.mod & SDL_KMOD_ALT)) {
            if (!e.key.repeat)
                SDL_SetWindowFullscreen(window_, !(SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN));
            break;
        }
        const int xt = xt_scancode(e.key.scancode);
        if (!xt)
            break;
        if (!down && !keys_down_[xt])
            break;  // its make code was swallowed (Alt+Enter)
        keys_down_[xt] = down;
        send_key(xt, down);  // repeats are the typematic make codes
        break;
    }
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        release_all_keys();
        mouse_buttons(0);
        break;
    case SDL_EVENT_MOUSE_MOTION:
        if (mouse_captured_)
            mouse_motion(static_cast<int>(std::lround(e.motion.xrel)), static_cast<int>(std::lround(e.motion.yrel)));
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        const SDL_MouseButtonFlags b = SDL_GetMouseState(nullptr, nullptr);
        int mask = 0;
        if (b & SDL_BUTTON_LMASK) mask |= 1;
        if (b & SDL_BUTTON_RMASK) mask |= 2;
        if (b & SDL_BUTTON_MMASK) mask |= 4;
        mouse_buttons(mask);
        break;
    }
    case SDL_EVENT_GAMEPAD_ADDED:
        for (auto& p : pads_)
            if (!p) {
                p = SDL_OpenGamepad(e.gdevice.which);
                break;
            }
        break;
    case SDL_EVENT_GAMEPAD_REMOVED:
        for (auto& p : pads_)
            if (p && SDL_GetGamepadID(p) == e.gdevice.which) {
                SDL_CloseGamepad(p);
                p = nullptr;
            }
        break;
    default:
        break;
    }
}

void Host::update_gamepads()
{
    for (int i = 0; i < 2; ++i) {
        SDL_Gamepad* p = pads_[i];
        if (!p) {
            joystick_update(i, 0, 0, 0, false);
            continue;
        }
        int x = SDL_GetGamepadAxis(p, SDL_GAMEPAD_AXIS_LEFTX);
        int y = SDL_GetGamepadAxis(p, SDL_GAMEPAD_AXIS_LEFTY);
        if (SDL_GetGamepadButton(p, SDL_GAMEPAD_BUTTON_DPAD_LEFT)) x = -32768;
        if (SDL_GetGamepadButton(p, SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) x = 32767;
        if (SDL_GetGamepadButton(p, SDL_GAMEPAD_BUTTON_DPAD_UP)) y = -32768;
        if (SDL_GetGamepadButton(p, SDL_GAMEPAD_BUTTON_DPAD_DOWN)) y = 32767;
        int buttons = 0;
        if (SDL_GetGamepadButton(p, SDL_GAMEPAD_BUTTON_SOUTH)) buttons |= 1;
        if (SDL_GetGamepadButton(p, SDL_GAMEPAD_BUTTON_EAST)) buttons |= 2;
        joystick_update(i, x, y, buttons, true);
    }
}

void Host::render()
{
    if (!take_retrace_frame(frame_, frame_serial_, 0.1))
        vga().snapshot(frame_);  // the program isn't waiting for retraces: scan out directly

    const int w = frame_.width, h = frame_.height;
    if (!screen_ || w != screen_w_ || h != screen_h_) {
        if (screen_)
            SDL_DestroyTexture(screen_);
        screen_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
        SDL_SetTextureScaleMode(screen_, o_.filter == Options::Filter::Linear ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
        screen_w_ = w;
        screen_h_ = h;
    }
    rgb_.resize(static_cast<std::size_t>(w) * h);
    for (std::size_t i = 0; i < rgb_.size(); ++i)
        rgb_[i] = frame_.palette[frame_.pixels[i]];
    SDL_UpdateTexture(screen_, nullptr, rgb_.data(), w * 4);

    int ow = 0, oh = 0;
    SDL_GetCurrentRenderOutputSize(renderer_, &ow, &oh);
    const double aspect = o_.aspect ? frame_.display_aspect : static_cast<double>(w) / h;
    double dw = ow, dh = ow / aspect;
    if (dh > oh) {
        dh = oh;
        dw = oh * aspect;
    }
    const SDL_FRect dst{static_cast<float>((ow - dw) / 2), static_cast<float>((oh - dh) / 2), static_cast<float>(dw), static_cast<float>(dh)};

    SDL_SetRenderTarget(renderer_, nullptr);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);

    if (o_.filter == Options::Filter::Sharp) {
        // integer nearest upscale, then a linear fit: sharp pixels without uneven rows
        const int kx = std::max(1, static_cast<int>(std::ceil(dw / w)));
        const int ky = std::max(1, static_cast<int>(std::ceil(dh / h)));
        if (!scaled_ || scaled_w_ != w * kx || scaled_h_ != h * ky) {
            if (scaled_)
                SDL_DestroyTexture(scaled_);
            scaled_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_TARGET, w * kx, h * ky);
            SDL_SetTextureScaleMode(scaled_, SDL_SCALEMODE_LINEAR);
            scaled_w_ = w * kx;
            scaled_h_ = h * ky;
        }
        if (scaled_) {
            SDL_SetRenderTarget(renderer_, scaled_);
            SDL_RenderTexture(renderer_, screen_, nullptr, nullptr);
            SDL_SetRenderTarget(renderer_, nullptr);
            SDL_RenderTexture(renderer_, scaled_, nullptr, &dst);
        } else {
            SDL_RenderTexture(renderer_, screen_, nullptr, &dst);
        }
    } else {
        SDL_RenderTexture(renderer_, screen_, nullptr, &dst);
    }
    SDL_RenderPresent(renderer_);
}

int Host::run()
{
    SDL_SetAppMetadata(dos_app_info.title, nullptr, nullptr);
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    // directories
    fs::set_data_dir(find_data_dir());
    if (char* pref = SDL_GetPrefPath(dos_app_info.organization, dos_app_info.application)) {
        fs::set_write_dir(pref);
        SDL_free(pref);
    } else {
        fs::set_write_dir(fs::data_dir());
    }
    if (!check_required_files()) {
        SDL_Quit();
        return 1;
    }

    if (!init_video()) {
        SDL_Quit();
        return 1;
    }

    MachineConfig cfg;
    cfg.card = o_.card;
    cfg.mouse = o_.mouse;
    machine_reset(cfg);
    set_precise_sleep([](std::uint64_t ns) { SDL_DelayPrecise(ns); });
    set_program_args(static_cast<int>(o_.program_args.size()) - 1, o_.program_args.data());

    init_audio();

    program_ = std::thread([this] {
        exit_code_ = run_program(dos_app_info);
        program_done_ = true;
        SDL_Event quit{};
        quit.type = SDL_EVENT_USER;
        SDL_PushEvent(&quit);
    });

    while (!program_done_) {
        SDL_Event e;
        while (SDL_PollEvent(&e))
            handle_event(e);
        update_gamepads();

        const bool want_capture = o_.mouse && mouse_wants_capture() && (SDL_GetWindowFlags(window_) & SDL_WINDOW_INPUT_FOCUS);
        if (want_capture != mouse_captured_) {
            SDL_SetWindowRelativeMouseMode(window_, want_capture);
            mouse_captured_ = want_capture;
        }

        const double t0 = now();
        render();
        if (now() - t0 < 0.002)
            SDL_DelayPrecise(8000000);  // no vsync (or minimized): don't spin

        if (quit_deadline_ >= 0 && now() > quit_deadline_ && !program_done_) {
            // The program doesn't respond (busy loop): give up on it.
            std::fprintf(stderr, "program did not quit, terminating\n");
            std::fflush(nullptr);
            std::_Exit(0);
        }
    }
    program_.join();

    // stop interrupts before tearing down
    if (audio_)
        SDL_DestroyAudioStream(audio_);
    timer_running_ = false;
    if (timer_thread_.joinable())
        timer_thread_.join();

    if (exit_code_ != 0) {
        const std::string text = console_text();
        if (!text.empty())
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, dos_app_info.title, text.c_str(), window_);
    }

    for (auto& p : pads_)
        if (p)
            SDL_CloseGamepad(p);
    if (scaled_)
        SDL_DestroyTexture(scaled_);
    if (screen_)
        SDL_DestroyTexture(screen_);
    SDL_DestroyRenderer(renderer_);
    SDL_DestroyWindow(window_);
    SDL_Quit();
    return exit_code_;
}

}  // namespace

int main_entry(int argc, char** argv)
{
    Options o;
    if (const int r = parse_options(argc, argv, o); r >= 0)
        return r;
    Host host(std::move(o));
    return host.run();
}

}  // namespace dos::sdl
