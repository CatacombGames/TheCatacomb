// DOS platform layer - what a program tells the platform about itself.
//
// Every program built on the platform defines exactly one
//
//     extern const dos::AppInfo dos_app_info = { ... };
//
// The platform owns the real main(): it parses its own command line options,
// opens the window and audio device, resets the emulated machine and runs
// `entry` on the program thread.
#pragma once

namespace dos {

struct AppInfo {
    const char* title;          // window title
    const char* organization;   // used for the per user write directory
    const char* application;    // (SDL_GetPrefPath)
    const char* const* required_files;  // nullptr terminated, checked before start (may be nullptr)
    void (*entry)();            // runs the DOS program; return or exit() to stop
    void (*quit_request)();     // the window was closed (runs on the program thread); nullptr = exit(0)
};

}  // namespace dos

extern const dos::AppInfo dos_app_info;
