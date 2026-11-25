#define SDL_MAIN_HANDLED
#include "SDL.h"
#include "prx/libSceAudioIn/include/AudioIn.hpp"
#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include <cstdio>
#include <cstring>

int main(int argc, char** argv) {
    if (argc != 2 || SDL_WasInit(SDL_INIT_AUDIO) != 0 || sceAudioOutInit() != 0) return 1;
    const auto capture = [] { return sceAudioInOpen(0x10000000, 0, 0, 256, 48000, 2); };
    const auto playback = [] { return sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 1); };
    int input = -1;
    int output = -1;
    if (std::strcmp(argv[1], "playback-first") == 0) {
        output = playback();
        input = capture();
    } else {
        input = capture();
        output = playback();
    }
    const char* driver = SDL_GetCurrentAudioDriver();
    std::printf("order=%s capture=%d playback=%d driver=%s\n", argv[1], input, output, driver ? driver : "none");
    if (input <= 0 || output <= 0 || !driver || std::strcmp(driver, "dummy") != 0) return 2;
    if (sceAudioInClose(input) != 0 || sceAudioOutClose(output) != 0) return 3;
    if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0) return 4;
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    const auto remaining = SDL_WasInit(SDL_INIT_AUDIO);
    std::printf("SDL audio after closing both handles and dropping one initialization reference: %u\n", remaining);
    return remaining == 0 ? 0 : 5;
}
