#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/Status.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

namespace {

void Expect(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", what);
    std::abort();
}

struct Memory {
    std::size_t allocations = 0;
    std::size_t frees = 0;
    std::size_t live = 0;
    std::size_t largest = 0;
};

void* APS5_VABI Allocate(const void* context, std::size_t size) {
    auto& memory = *static_cast<Memory*>(const_cast<void*>(context));
    if (size > memory.largest) memory.largest = size;
    void* result = std::malloc(size);
    if (result) {
        ++memory.allocations;
        ++memory.live;
    }
    return result;
}

void* APS5_VABI Reallocate(const void* context, void* pointer, std::size_t size) {
    if (!pointer) return Allocate(context, size);
    auto& memory = *static_cast<Memory*>(const_cast<void*>(context));
    if (size > memory.largest) memory.largest = size;
    return std::realloc(pointer, size);
}

void APS5_VABI Release(const void* context, void* pointer) {
    auto& memory = *static_cast<Memory*>(const_cast<void*>(context));
    if (pointer) {
        ++memory.frees;
        --memory.live;
    }
    std::free(pointer);
}

void Allocator() {
    Memory memory;
    std::int32_t status = 15;
    u_setMemoryFunctions_nid_postfix(&memory, Allocate, Reallocate, Release, &status);
    Expect(status == 15, "allocator input failure");
    status = 0;
    u_setMemoryFunctions_nid_postfix(&memory, nullptr, Reallocate, Release, &status);
    Expect(status == 1, "allocator argument error");
    status = 0;
    u_setMemoryFunctions_nid_postfix(&memory, Allocate, Reallocate, Release, &status);
    Expect(status == 0, "install guest allocator");
    auto* warm = ubrk_open(1, "en", u"warm cache", -1, &status);
    Expect(warm && status <= 0, "warm iterator");
    while (ubrk_next(warm) != -1) {}
    ubrk_close(warm);
    const auto cached = memory.live;
    const auto allocated = memory.allocations;
    status = 0;
    char16_t text[] = u"abc";
    auto* iterator = ubrk_open(0, "en", text, 3, &status);
    Expect(iterator && status <= 0, "iterator with guest allocator");
    Expect(memory.allocations > allocated, "native ICU uses guest allocator");
    ubrk_close(iterator);
    const auto settled = memory.live;
    status = 0;
    iterator = ubrk_open(0, "en", text, 3, &status);
    Expect(iterator && status <= 0, "second iterator with guest allocator");
    const auto beforeClose = memory.frees;
    ubrk_close(iterator);
    Expect(memory.frees > beforeClose && memory.live == settled, "close frees all per-handle allocations");
    const std::u16string large(100000, u'a');
    memory.largest = 0;
    status = 0;
    iterator = ubrk_open(0, "en", large.data(), static_cast<std::int32_t>(large.size()), &status);
    Expect(iterator && status <= 0, "borrow large caller buffer");
    Expect(memory.largest < large.size() * sizeof(char16_t), "open does not allocate a copy of caller text");
    ubrk_close(iterator);
    status = 0;
    iterator = ubrk_open(0, "en", u"abcd", 4, &status);
    Expect(iterator && status <= 0 && ubrk_next(iterator) == 1, "caller iterator positioned");
    auto* supplied = static_cast<UBreakIterator*>(iterator);
    status = 15;
    Expect(!usearch_open_nid_postfix(u"a", 1, u"ab", 2, "en", supplied, &status) && status == 15,
           "search preserves incoming failure");
    status = 0;
    Expect(!usearch_open_nid_postfix(u"a", 1, u"ab", 2, "en", supplied, &status) && status == Icu::UnsupportedError,
           "search rejects an independently owned iterator");
    Expect(ubrk_current(iterator) == 1 && ubrk_last(iterator) == 4, "caller keeps the iterator position and text");
    ubrk_close(iterator);
    status = 0;
    Expect(usearch_open_nid_postfix(u"a", 1, u"ab", 2, "en", nullptr, &status) && status <= 0,
           "search opens with the guest allocator");
    u_cleanup_nid_postfix();
    Expect(memory.live == 0 && memory.allocations == memory.frees, "cleanup frees retained ICU caches before allocator reset");
    status = 0;
    iterator = ubrk_open(0, "en", text, 3, &status);
    Expect(iterator && status <= 0, "embedded data survives cleanup");
    ubrk_close(iterator);
    status = 0;
    Expect(usearch_open_nid_postfix(u"b", 1, u"ab", 2, "en", nullptr, &status) && status <= 0,
           "search reopens after cleanup");
    u_cleanup_nid_postfix();
    std::printf("guest allocator: %zu allocations, %zu frees, %zu cached after warmup\n", memory.allocations, memory.frees, cached);
}

void Registration() {
    std::int32_t status = 15;
    Icu::SetCommonData(nullptr, &status);
    Expect(status == 15, "common data preserves failure");
    Icu::SetCommonData(nullptr, nullptr);
    status = 0;
    Icu::SetCommonData(nullptr, &status);
    Expect(status == 1, "null common data");
    alignas(16) unsigned char invalid[64] = {};
    status = 0;
    Icu::SetCommonData(invalid, &status);
    Expect(status == 3, "invalid common data header");
    std::ifstream input(ANYPS5_ICU_PACKAGED_DATA, std::ios::binary);
    Expect(input.good(), "packaged data file");
    const std::vector<char> bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    std::vector<std::max_align_t> aligned((bytes.size() + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t));
    std::memcpy(aligned.data(), bytes.data(), bytes.size());
    status = 0;
    Icu::SetCommonData(aligned.data(), &status);
    Expect(status == 0, "register valid source-built common data");
    status = 15;
    Icu::SetFileAccess(3, &status);
    Expect(status == 15, "file access ignores failed status");
    Icu::SetFileAccess(3, nullptr);
    status = 0;
    auto* iterator = ubrk_open(0, "en", u"a\u0301", 2, &status);
    Expect(iterator && status <= 0 && ubrk_next(iterator) == 2, "registered data supports break iteration with no file access");
    ubrk_close(iterator);
    Icu::SetFileAccess(0, nullptr);
    u_cleanup_nid_postfix();
}

}

int main(int argc, char**) {
    if (argc == 2) Registration();
    else Allocator();
}
