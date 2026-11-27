#include "prx/libSceIcu/CollatorHost.hpp"
#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

namespace {

constexpr std::int32_t UcolDefault = -1;
constexpr std::size_t LocaleBytes = 4096;
constexpr std::size_t PageBytes = 4096;

struct Registry {
    std::mutex mutex;
    std::unordered_map<const void*, std::shared_ptr<void>> collators;
};

Registry& Collators() {
    static Registry registry;
    return registry;
}

bool CopyIn(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return ok;
}

std::int32_t ReadStatus(const std::int32_t* status, const char* caller) {
    Icu::RequireStatus(status);
    std::int32_t value = 0;
    if (!CopyIn(&value, status, sizeof(value))) NotImplemented_nid_no_patch(caller);
    return value;
}

bool Failure(const std::int32_t* status, const char* caller) {
    return ReadStatus(status, caller) > 0;
}

void WriteStatus(std::int32_t* status, std::int32_t incoming, std::int32_t outgoing, const char* caller) {
    if (outgoing == incoming) return;
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(status, &outgoing, sizeof(outgoing));
    errno = savedErrno;
    if (!ok) NotImplemented_nid_no_patch(caller);
}

std::shared_ptr<void> Registered(const UCollator* coll, const char* caller) {
    auto& registry = Collators();
    const std::lock_guard lock(registry.mutex);
    const auto found = registry.collators.find(coll);
    if (found == registry.collators.end()) NotImplemented_nid_no_patch(caller);
    return found->second;
}

void Register(const std::shared_ptr<void>& collator) {
    auto& registry = Collators();
    const std::lock_guard lock(registry.mutex);
    registry.collators.emplace(collator.get(), collator);
}

void Unregister(const void* collator, const char* caller) {
    auto& registry = Collators();
    decltype(registry.collators)::node_type retired;
    const std::lock_guard lock(registry.mutex);
    retired = registry.collators.extract(collator);
    if (retired.empty()) NotImplemented_nid_no_patch(caller);
}

template<class TUnit>
std::basic_string<TUnit> CopyUnits(const TUnit* text, std::int32_t length, const char* caller) {
    std::basic_string<TUnit> units;
    if (length >= 0) {
        units.resize(static_cast<std::size_t>(length));
        if (length > 0 && !CopyIn(units.data(), text, units.size() * sizeof(TUnit))) NotImplemented_nid_no_patch(caller);
        return units;
    }
    auto address = reinterpret_cast<std::uintptr_t>(text);
    while (true) {
        const std::size_t count = std::max<std::size_t>((PageBytes - address % PageBytes) / sizeof(TUnit), 1);
        const std::size_t offset = units.size();
        units.resize(offset + count);
        if (!CopyIn(units.data() + offset, reinterpret_cast<const void*>(address), count * sizeof(TUnit))) {
            NotImplemented_nid_no_patch(caller);
        }
        const auto end = std::find(units.begin() + static_cast<std::ptrdiff_t>(offset), units.end(), TUnit{});
        if (end != units.end()) {
            units.erase(end, units.end());
            return units;
        }
        Icu::Checked(static_cast<std::int64_t>(units.size()));
        address += count * sizeof(TUnit);
    }
}

template<class TUnit>
struct Operand {
    std::basic_string<TUnit> units;
    bool present;
    std::int32_t length;

    const TUnit* Data() const {
        return present ? units.data() : nullptr;
    }
};

template<class TUnit>
Operand<TUnit> CopyOperand(const TUnit* text, std::int32_t length, const char* caller) {
    if (!text) return {{}, false, length};
    auto units = CopyUnits(text, length, caller);
    const auto size = Icu::Checked(static_cast<std::int64_t>(units.size()));
    return {std::move(units), true, size};
}

void WriteStatus(std::int32_t* status, std::int32_t value, const char* caller) {
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(status, &value, sizeof(value));
    errno = savedErrno;
    if (!ok) NotImplemented_nid_no_patch(caller);
}

bool CollationKeyword(const char* keyword, const char* caller) {
    constexpr char expected[] = "collation";
    for (std::size_t index = 0; index < sizeof(expected); ++index) {
        char unit = 0;
        const int savedErrno = errno;
        const bool ok = GuestCopyIn_nid_no_patch(&unit, keyword + index, sizeof(unit));
        errno = savedErrno;
        if (!ok) NotImplemented_nid_no_patch(caller);
        if (unit != expected[index]) return false;
    }
    return true;
}

} // namespace

extern "C" {

void APS5_VABI ucol_close_nid_postfix(UCollator* coll) {
    if (!coll) return;
    Unregister(coll, __func__);
}

std::int32_t APS5_VABI ucol_getAttribute_nid_postfix(const UCollator* coll, std::int32_t attr, std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0 || !coll) return UcolDefault;
    auto outgoing = incoming;
    const auto value = Icu::CollatorHost::GetAttribute(Registered(coll, __func__).get(), attr, outgoing);
    WriteStatus(status, incoming, outgoing, __func__);
    return value;
}

void APS5_VABI ucol_getContractionsAndExpansions_nid_postfix(const UCollator* coll, USet*, USet*, std::int8_t, std::int32_t* status) {
    if (Failure(status, __func__)) return;
    if (!coll) {
        WriteStatus(status, Icu::IllegalArgument, __func__);
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

UEnumeration* APS5_VABI ucol_getKeywordValues_nid_postfix(const char* keyword, std::int32_t* status) {
    if (Failure(status, __func__)) return nullptr;
    if (!keyword || !CollationKeyword(keyword, __func__)) {
        WriteStatus(status, Icu::IllegalArgument, __func__);
        return nullptr;
    }
    NotImplemented_nid_no_patch(__func__);
}

UEnumeration* APS5_VABI ucol_getKeywordValuesForLocale_nid_postfix(const char*, const char*, std::int8_t, std::int32_t* status) {
    if (Failure(status, __func__)) return nullptr;
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI ucol_getReorderCodes_nid_postfix(const UCollator*, std::int32_t*, std::int32_t, std::int32_t* status) {
    if (Failure(status, __func__)) {
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
}

const char16_t* APS5_VABI ucol_getRules_nid_postfix(const UCollator*, std::int32_t*) {
    NotImplemented_nid_no_patch(__func__);
}

UCollationStrength APS5_VABI ucol_getStrength_nid_postfix(const UCollator* coll) {
    if (!coll) return UCOL_DEFAULT;
    std::int32_t status = 0;
    return static_cast<UCollationStrength>(Icu::CollatorHost::GetAttribute(Registered(coll, __func__).get(), UCOL_STRENGTH, status));
}

USet* APS5_VABI ucol_getTailoredSet_nid_postfix(const UCollator*, std::int32_t* status) {
    if (Failure(status, __func__)) {
        return nullptr;
    }
    NotImplemented_nid_no_patch(__func__);
}

UCollator* APS5_VABI ucol_open_nid_postfix(const char* locale, std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return nullptr;
    std::string name;
    if (locale) {
        const int savedErrno = errno;
        const int copied = CopyGuestString_nid_no_patch(locale, LocaleBytes, name);
        errno = savedErrno;
        if (copied != 0) NotImplemented_nid_no_patch(__func__);
    }
    auto outgoing = incoming;
    void* collator = Icu::CollatorHost::Open(locale ? name.c_str() : nullptr, outgoing);
    if (collator) {
        const std::shared_ptr<void> owner(collator, Icu::CollatorHost::Close);
        WriteStatus(status, incoming, outgoing, __func__);
        Register(owner);
        return static_cast<UCollator*>(collator);
    }
    WriteStatus(status, incoming, outgoing, __func__);
    return nullptr;
}

void APS5_VABI ucol_setAttribute_nid_postfix(UCollator* coll, UColAttribute attr, UColAttributeValue value, std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0 || !coll) return;
    auto outgoing = incoming;
    Icu::CollatorHost::SetAttribute(Registered(coll, __func__).get(), attr, value, outgoing);
    WriteStatus(status, incoming, outgoing, __func__);
}

void APS5_VABI ucol_setStrength_nid_postfix(UCollator* coll, UCollationStrength strength) {
    if (!coll) return;
    std::int32_t status = 0;
    Icu::CollatorHost::SetAttribute(Registered(coll, __func__).get(), UCOL_STRENGTH, strength, status);
}

UCollationResult APS5_VABI ucol_strcoll_nid_postfix(const UCollator* coll, const char16_t* source, std::int32_t sourceLength,
                                                   const char16_t* target, std::int32_t targetLength) {
    const auto handle = Registered(coll, __func__);
    const auto left = CopyOperand(source, sourceLength, __func__);
    const auto right = CopyOperand(target, targetLength, __func__);
    return static_cast<UCollationResult>(Icu::CollatorHost::Compare(handle.get(), left.Data(), left.length, right.Data(), right.length));
}

UCollationResult APS5_VABI ucol_strcollUTF8_nid_postfix(const UCollator* coll, const char* source, std::int32_t sourceLength,
                                                       const char* target, std::int32_t targetLength, std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return UCOL_EQUAL;
    const auto handle = Registered(coll, __func__);
    const auto left = CopyOperand(source, sourceLength, __func__);
    const auto right = CopyOperand(target, targetLength, __func__);
    auto outgoing = incoming;
    const auto result = Icu::CollatorHost::CompareUtf8(handle.get(), left.Data(), left.length, right.Data(), right.length, outgoing);
    WriteStatus(status, incoming, outgoing, __func__);
    return static_cast<UCollationResult>(result);
}

std::int32_t APS5_VABI ucol_strcollIter_nid_postfix(const UCollator* coll, UCharIterator* sIter, UCharIterator* tIter, std::int32_t* status) {
    if (!status || Failure(status, __func__)) return 0;
    if (!sIter || !tIter || !coll) {
        WriteStatus(status, Icu::IllegalArgument, __func__);
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
}

}
