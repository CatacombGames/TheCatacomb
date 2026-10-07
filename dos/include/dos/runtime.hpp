// DOS platform layer - services used by host frontends (the SDL frontend).
#pragma once

#include "dos/app.hpp"
#include "dos/machine.hpp"

namespace dos {

// Runs the program on the calling thread, returns its exit code.
int run_program(const AppInfo& app);

// The latest frame captured when the program saw a vertical retrace.
// Returns false if the program hasn't synchronised with the retrace for
// max_age seconds (the frontend should then capture the screen itself).
bool take_retrace_frame(Frame& out, std::uint64_t& serial, double max_age);

// Optional high precision sleep provided by the frontend.
void set_precise_sleep(void (*fn)(std::uint64_t ns)) noexcept;

// argc/argv as seen by the DOS program (platform options removed).
void set_program_args(int argc, char** argv);

}  // namespace dos
