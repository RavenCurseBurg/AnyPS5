#include <sys/time.h>
#include <sys/resource.h>
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

static int64_t micros(struct timeval value) {
    return (int64_t)value.tv_sec * 1000000 + value.tv_usec;
}

static int64_t usage_ns(int system) {
    struct rusage usage;
    assert(getrusage(RUSAGE_SELF, &usage) == 0);
    return (micros(usage.ru_utime) + (system ? micros(usage.ru_stime) : 0)) * 1000;
}

static void test_clock(clockid_t clock, const char *name, int system) {
    struct timespec now, resolution;
    assert(clock_getres(clock, &resolution) == 0);
    const int samples = 8192;
    int outside = 0, backwards = 0, nonmicro = 0, advances = 0;
    int64_t previous = 0, minimum = INT64_MAX, largest_miss = 0;
    volatile uint64_t work = 1;
    for (int i = 0; i < samples; ++i) {
        for (int j = 0; j < 2048; ++j) work = work * 1664525U + 1013904223U;
        const int64_t low = usage_ns(system);
        assert(clock_gettime(clock, &now) == 0);
        const int64_t current = (int64_t)now.tv_sec * 1000000000 + now.tv_nsec;
        const int64_t high = usage_ns(system);
        if (current < low || current > high) {
            const int64_t miss = current < low ? low - current : current - high;
            if (miss > largest_miss) largest_miss = miss;
            if (++outside <= 3) printf("outside %s low=%jd current=%jd high=%jd\n", name, (intmax_t)low, (intmax_t)current, (intmax_t)high);
        }
        if (i && current > previous) {
            ++advances;
            if (current - previous < minimum) minimum = current - previous;
        }
        if (i && current < previous) ++backwards;
        if (current % 1000) ++nonmicro;
        previous = current;
    }
    printf("native %s id=%d samples=%d outside=%d backwards=%d nonmicro=%d advances=%d min-ns=%jd largest-miss-ns=%jd getres-ns=%jd\n",
           name, (int)clock, samples, outside, backwards, nonmicro, advances, (intmax_t)(advances ? minimum : 0),
           (intmax_t)largest_miss, (intmax_t)resolution.tv_sec * 1000000000 + resolution.tv_nsec);
}

int main(void) {
    assert(geteuid() != 0);
    printf("uid=%d tick-rate=%ld\n", (int)geteuid(), sysconf(_SC_CLK_TCK));
    test_clock(CLOCK_VIRTUAL, "CLOCK_VIRTUAL", 0);
    test_clock(CLOCK_PROF, "CLOCK_PROF", 1);
    struct timespec value;
    errno = 0;
    const int rc = clock_gettime((clockid_t)21, &value);
    printf("numeric id 21: rc=%d errno=%d\n", rc, rc ? errno : 0);
    return 0;
}
