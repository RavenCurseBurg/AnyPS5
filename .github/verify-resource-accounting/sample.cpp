#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/System/include/ProcessWait.hpp"
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <vector>

int main() {
    const pid_t child = fork();
    if (child == -1) return 3;
    if (child == 0) {
        std::vector<unsigned char> allocation(48u << 20, 0);
        volatile unsigned char* pages = allocation.data();
        for (std::size_t i = 0; i < allocation.size(); i += 4096) pages[i] = 37;
        _exit(42);
    }
    GuestResourceUsage waited{}, cumulative{}, self{};
    int status = 0;
    if (WaitProcess_nid_no_patch(child, &status, 0, &waited) != child ||
        !WIFEXITED(status) || WEXITSTATUS(status) != 42) return 3;
    rusage nativeChildren{}, before{}, after{};
    if (getrusage(RUSAGE_CHILDREN, &nativeChildren) ||
        getrusage_nid_postfix(-1, &cumulative) || getrusage(RUSAGE_SELF, &before) ||
        getrusage_nid_postfix(0, &self) || getrusage(RUSAGE_SELF, &after)) return 3;
    const long childKiB = nativeChildren.ru_maxrss / 1024;
    std::printf("Darwin native child bytes=%ld, wait KiB=%lld, cumulative KiB=%lld; native self bytes=%ld..%ld, guest self KiB=%lld\n",
        nativeChildren.ru_maxrss, (long long)waited.ru_maxrss, (long long)cumulative.ru_maxrss,
        before.ru_maxrss, after.ru_maxrss, (long long)self.ru_maxrss);
    return childKiB >= 45000 && waited.ru_maxrss == childKiB && cumulative.ru_maxrss == childKiB &&
        self.ru_maxrss >= before.ru_maxrss / 1024 && self.ru_maxrss <= after.ru_maxrss / 1024 &&
        cumulative.ru_minflt == nativeChildren.ru_minflt && cumulative.ru_majflt == nativeChildren.ru_majflt ? 0 : 2;
}
