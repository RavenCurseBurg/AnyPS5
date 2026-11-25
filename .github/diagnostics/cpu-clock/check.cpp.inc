#include <algorithm>
#include <limits>
#include <sys/resource.h>

static std::int64_t hostMicros(bool system) {
    rusage usage{};
    Require(::getrusage(RUSAGE_SELF, &usage) == 0);
    const auto micros = [](const timeval& t) { return t.tv_sec * 1000000LL + t.tv_usec; };
    return micros(usage.ru_utime) + (system ? micros(usage.ru_stime) : 0);
}

int main(int argc, char** argv) {
    Require(argc == 2);
    const bool coarse = std::strcmp(argv[1], "coarse") == 0;
    constexpr int count = 8192;
    for (const int variant : {1, 2, 21, 102}) {
        int outside = 0, backwards = 0, nonmicro = 0, advances = 0;
        std::int64_t previous = 0;
        auto minimum = std::numeric_limits<std::int64_t>::max();
        std::int64_t largestMiss = 0;
        for (int sample = 0; sample < count; ++sample) {
            const auto low = 1000 * hostMicros(variant != 1);
            std::int64_t current;
            if (variant == 102) {
                KernelTimespec ts{-1,-1};
                Require(sceKernelClockGettime(2, &ts) == SCE_OK);
                current = ts.tv_sec * NANOS_PER_SECOND + ts.tv_nsec;
            } else {
                current = Nanos(variant);
            }
            const auto high = 1000 * hostMicros(variant != 1);
            if (current < low || current > high) {
                if (++outside <= 3) std::printf("outside API=%d sample=%d host-low=%lld guest=%lld host-high=%lld\n",
                    variant, sample, static_cast<long long>(low), static_cast<long long>(current), static_cast<long long>(high));
                largestMiss = std::max(largestMiss, std::max(low - current, current - high));
            }
            nonmicro += current % 1000 != 0;
            if (sample != 0 && current > previous) {
                ++advances;
                minimum = std::min(minimum, current - previous);
            }
            backwards += sample != 0 && current < previous;
            previous = current;
        }
        std::printf("clock=%d samples=%d outside=%d backwards=%d nonmicro=%d advances=%d min-ns=%lld largest-miss-ns=%lld\n",
            variant, count, outside, backwards, nonmicro, advances, static_cast<long long>(advances ? minimum : 0),
            static_cast<long long>(largestMiss));
        std::fflush(stdout);
        Require(coarse ? outside > 0 : outside == 0 && backwards == 0 && nonmicro == 0 && advances > 0);
    }
}
