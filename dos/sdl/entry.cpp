// DOS platform layer - process entry point (SDL provides WinMain etc.).

#include "frontend.hpp"

#include <SDL3/SDL_main.h>

int main(int argc, char* argv[])
{
    return dos::sdl::main_entry(argc, argv);
}
