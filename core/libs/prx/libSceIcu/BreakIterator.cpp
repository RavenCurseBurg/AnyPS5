#include "prx/libSceIcu/BreakIterator.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/ubrk.h"
#include "unicode/uenum.h"
#include "unicode/uloc.h"
#include <mutex>
#include <new>
#include <string>
#include <unordered_map>

#undef ubrk_open
#undef ubrk_close
#undef ubrk_setText
#undef ubrk_first
#undef ubrk_last
#undef ubrk_next
#undef ubrk_previous
#undef ubrk_current
#undef ubrk_following
#undef ubrk_preceding
#undef ubrk_isBoundary
#undef ubrk_getRuleStatus
#undef ubrk_countAvailable
#undef ubrk_getAvailable

namespace {

struct BreakRules {
    UBreakIteratorType type;
    std::string locale;
};

struct Registry {
    std::mutex mutex;
    std::unordered_map<const UBreakIterator*, BreakRules> iterators;
};

Registry& Iterators() {
    static Registry registry;
    return registry;
}

void Register(UBreakIterator* iterator, UBreakIteratorType type, const char* locale) {
    auto& registry = Iterators();
    const std::lock_guard lock(registry.mutex);
    registry.iterators.try_emplace(iterator, type, locale);
}

BreakRules RegisteredRules(const void* iterator, const char* caller) {
    auto& registry = Iterators();
    const std::lock_guard lock(registry.mutex);
    const auto found = registry.iterators.find(static_cast<const UBreakIterator*>(iterator));
    if (found == registry.iterators.end()) NotImplemented_nid_no_patch(caller);
    return found->second;
}

bool Unregister(const UBreakIterator* iterator) {
    auto& registry = Iterators();
    const std::lock_guard lock(registry.mutex);
    return registry.iterators.erase(iterator) != 0;
}

}

namespace Icu::BreakIterator {

void* OpenForSearch(const void* iterator, std::int32_t& status, const char* caller) {
    if (!iterator) return nullptr;
    try {
        const auto rules = RegisteredRules(iterator, caller);
        auto code = static_cast<UErrorCode>(status);
        auto* result = ubrk_open_78(rules.type, rules.locale.c_str(), nullptr, 0, &code);
        if (U_FAILURE(code)) status = code;
        return result;
    } catch (const std::bad_alloc&) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return nullptr;
    }
}

void CloseForSearch(void* iterator) {
    ubrk_close_78(static_cast<UBreakIterator*>(iterator));
}

}

extern "C" {

void* APS5_VABI ubrk_open(std::int32_t type, const char* locale, const char16_t* text, std::int32_t length,
                         std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    auto status = static_cast<UErrorCode>(*error);
    if (U_FAILURE(status)) return nullptr;
    const auto kind = static_cast<UBreakIteratorType>(type);
    const char* requestedLocale = locale ? locale : uloc_getDefault_78();
    auto* result = ubrk_open_78(kind, requestedLocale, text, length, &status);
    *error = status;
    if (result) {
        try {
            Register(result, kind, requestedLocale);
        } catch (const std::bad_alloc&) {
            ubrk_close_78(result);
            *error = U_MEMORY_ALLOCATION_ERROR;
            return nullptr;
        } catch (...) {
            ubrk_close_78(result);
            throw;
        }
    }
    return result;
}

void APS5_VABI ubrk_close(void* iterator) {
    if (!iterator) return;
    if (!Unregister(static_cast<UBreakIterator*>(iterator))) NotImplemented_nid_no_patch(__func__);
    ubrk_close_78(static_cast<UBreakIterator*>(iterator));
}

void APS5_VABI ubrk_setText(void* iterator, const char16_t* text, std::int32_t length, std::int32_t* error) {
    if (!iterator || !error) APS5_INVALID_ARG_EX;
    auto status = static_cast<UErrorCode>(*error);
    ubrk_setText_78(static_cast<UBreakIterator*>(iterator), text, length, &status);
    *error = status;
}

std::int32_t APS5_VABI ubrk_first(void* iterator) {
    if (!iterator) APS5_INVALID_ARG_EX;
    return ubrk_first_78(static_cast<UBreakIterator*>(iterator));
}

std::int32_t APS5_VABI ubrk_last(void* iterator) {
    if (!iterator) APS5_INVALID_ARG_EX;
    return ubrk_last_78(static_cast<UBreakIterator*>(iterator));
}

std::int32_t APS5_VABI ubrk_next(void* iterator) {
    if (!iterator) APS5_INVALID_ARG_EX;
    return ubrk_next_78(static_cast<UBreakIterator*>(iterator));
}

std::int32_t APS5_VABI ubrk_previous(void* iterator) {
    if (!iterator) APS5_INVALID_ARG_EX;
    return ubrk_previous_78(static_cast<UBreakIterator*>(iterator));
}

std::int32_t APS5_VABI ubrk_current(const void* iterator) {
    if (!iterator) APS5_INVALID_ARG_EX;
    return ubrk_current_78(static_cast<const UBreakIterator*>(iterator));
}

std::int32_t APS5_VABI ubrk_following(void* iterator, std::int32_t offset) {
    if (!iterator) APS5_INVALID_ARG_EX;
    return ubrk_following_78(static_cast<UBreakIterator*>(iterator), offset);
}

std::int32_t APS5_VABI ubrk_preceding(void* iterator, std::int32_t offset) {
    if (!iterator) APS5_INVALID_ARG_EX;
    return ubrk_preceding_78(static_cast<UBreakIterator*>(iterator), offset);
}

std::int8_t APS5_VABI ubrk_isBoundary(void* iterator, std::int32_t offset) {
    if (!iterator) APS5_INVALID_ARG_EX;
    return ubrk_isBoundary_78(static_cast<UBreakIterator*>(iterator), offset);
}

std::int32_t APS5_VABI ubrk_getRuleStatus(void* iterator) {
    if (!iterator) APS5_INVALID_ARG_EX;
    return ubrk_getRuleStatus_78(static_cast<UBreakIterator*>(iterator));
}

std::int32_t APS5_VABI ubrk_countAvailable() {
    return ubrk_countAvailable_78();
}

const char* APS5_VABI ubrk_getAvailable(std::int32_t index) {
    const auto count = ubrk_countAvailable_78();
    if (index < 0 || index == count) {
        if (count > 0) APS5_INVALID_ARG_EX;
        UErrorCode status = U_ZERO_ERROR;
        auto* available = uloc_openAvailableByType(ULOC_AVAILABLE_DEFAULT, &status);
        uenum_close(available);
        if (U_SUCCESS(status)) APS5_INVALID_ARG_EX;
    }
    return ubrk_getAvailable_78(index);
}

}
