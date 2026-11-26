#include "prx/libSceAudioIn/include/AudioIn.hpp"
#undef SDL_OpenAudioDevice
#undef SDL_CloseAudioDevice
#include "SDL.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <stdexcept>
#include <thread>

namespace {

using namespace std::chrono_literals;
std::atomic<int> opens{}, closes{};
std::promise<void> releaseOpen, enteredClose, releaseClose;

void Require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}

int Open() {
    return sceAudioInOpen(0x10000000, 0, 0, 256, 16000, 0);
}

}

extern "C" SDL_AudioDeviceID SDLCALL TestOpenAudioDevice(const char* name, int capture,
        const SDL_AudioSpec* desired, SDL_AudioSpec* obtained, int changes) {
    if (++opens == 1) releaseOpen.get_future().wait_for(8s);
    return SDL_OpenAudioDevice(name, capture, desired, obtained, changes);
}

extern "C" void SDLCALL TestCloseAudioDevice(SDL_AudioDeviceID device) {
    if (++closes == 1) {
        enteredClose.set_value();
        releaseClose.get_future().wait_for(8s);
    }
    SDL_CloseAudioDevice(device);
}

int main() {
    SDL_SetMainReady();
    Require(sceAudioInInit() == 0, "AudioIn initialization failed");
    const int silent = Open();
    Require(silent > 0 && sceAudioInGetSilentState(silent) == 1, "timed-out public open must return a silent port");
    std::array<std::int16_t, 256> samples;
    samples.fill(-1);
    Require(sceAudioInInput(silent, samples.data()) == 256 && std::all_of(samples.begin(), samples.end(),
            [](auto x) { return x == 0; }), "a timed-out port must deliver silence");
    auto checkPending = [] {
        const int other = Open();
        Require(other > 0 && sceAudioInGetSilentState(other) == 1 && opens == 1,
                "pending capture must not start a second backend open");
        Require(sceAudioInClose(other) == 0, "cannot close pending fallback");
    };
    checkPending();
    releaseOpen.set_value();
    Require(enteredClose.get_future().wait_for(2s) == std::future_status::ready, "late device was not closed");
    checkPending();
    releaseClose.set_value();
    const auto until = std::chrono::steady_clock::now() + 2s;
    do {
        const int port = Open();
        Require(port > 0, "cannot open after cleanup");
        const bool captured = sceAudioInGetSilentState(port) == 0;
        Require(sceAudioInClose(port) == 0, "cannot close retried port");
        if (captured) {
            Require(opens == 2 && closes == 2 && sceAudioInGetSilentState(silent) == 1,
                    "late and retried devices must each close once and the original port must stay silent");
            Require(sceAudioInClose(silent) == 0, "cannot close original port");
            return 0;
        }
        std::this_thread::sleep_for(5ms);
    } while (std::chrono::steady_clock::now() < until);
    throw std::runtime_error("capture must recover after the abandoned device closes");
}
