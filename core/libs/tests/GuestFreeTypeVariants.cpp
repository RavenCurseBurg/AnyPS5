#include "FreeTypeVariationFontFixture.hpp"
#include "prx/libSceFreeType/libSceFreeType.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

using namespace FreeType;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType variant check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

struct FacePair {
    std::vector<unsigned char> bytes;
    GuestFaceRec* guest = nullptr;
    FT_Face native = nullptr;

    FacePair(GuestLibrary* library, FT_Library nativeLibrary, std::vector<unsigned char> font) : bytes(std::move(font)) {
        Require(FT_New_Memory_Face_nid_postfix(library, bytes.data(), static_cast<std::int64_t>(bytes.size()), 0, &guest) == 0);
        Require(FT_New_Memory_Face(nativeLibrary, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &native) == 0);
    }

    ~FacePair() {
        Require(FT_Done_Face_nid_postfix(guest) == 0 && FT_Done_Face(native) == 0);
    }
};

static std::vector<std::uint32_t> CopyList(const std::uint32_t* values) {
    std::vector<std::uint32_t> copy;
    if (!values) return copy;
    for (unsigned index = 0; index < 64; ++index) {
        if (values[index] == 0) return copy;
        copy.push_back(values[index]);
    }
    Require(false);
    return copy;
}

static void ListMatches(const std::uint32_t* guest, const FT_UInt32* native, int& count) {
    Require((guest == nullptr) == (native == nullptr));
    Require(CopyList(guest) == CopyList(native));
    ++count;
}

static void CompareQueries(GuestFaceRec* guest, FT_Face native, int& scalars, int& lists) {
    constexpr std::array<std::uint64_t, 12> characters{'A', 'B', 'C', 'D', 0, 0x4E00, 0x1F600, 0x1F601, 0xFFFFFFFF,
                                                     0x100000041ull, 0x10001F600ull, std::numeric_limits<std::uint64_t>::max()};
    constexpr std::array<std::uint64_t, 8> selectors{0xFE0E, 0xFE0F, 0xE0100, 0xE0101, 0, 0xFFFFFFFF,
                                                   0x10000FE0Full, std::numeric_limits<std::uint64_t>::max()};
    const auto selected = guest ? guest->charmap : nullptr;
    const auto nativeSelected = native ? native->charmap : nullptr;
    std::array<unsigned char, sizeof(GuestFaceRec)> savedFace{};
    std::array<unsigned char, sizeof(GuestGlyphSlotRec)> savedSlot{};
    if (guest) {
        std::memcpy(savedFace.data(), guest, savedFace.size());
        std::memcpy(savedSlot.data(), guest->glyph, savedSlot.size());
    }
    ListMatches(FT_Face_GetVariantSelectors_nid_postfix(guest), FT_Face_GetVariantSelectors(native), lists);
    for (const auto character : characters) {
        ListMatches(FT_Face_GetVariantsOfChar_nid_postfix(guest, character), FT_Face_GetVariantsOfChar(native, static_cast<FT_ULong>(character)), lists);
        for (const auto selector : selectors) {
            Require(FT_Face_GetCharVariantIndex_nid_postfix(guest, character, selector) ==
                    FT_Face_GetCharVariantIndex(native, static_cast<FT_ULong>(character), static_cast<FT_ULong>(selector)));
            Require(FT_Face_GetCharVariantIsDefault_nid_postfix(guest, character, selector) ==
                    FT_Face_GetCharVariantIsDefault(native, static_cast<FT_ULong>(character), static_cast<FT_ULong>(selector)));
            scalars += 2;
        }
    }
    for (const auto selector : selectors) {
        ListMatches(FT_Face_GetCharsOfVariant_nid_postfix(guest, selector), FT_Face_GetCharsOfVariant(native, static_cast<FT_ULong>(selector)), lists);
    }
    if (guest) {
        Require(guest->charmap == selected && native->charmap == nativeSelected);
        Require(std::memcmp(savedFace.data(), guest, savedFace.size()) == 0 && std::memcmp(savedSlot.data(), guest->glyph, savedSlot.size()) == 0);
    }
}

int main() {
    GuestLibrary* library = nullptr;
    FT_Library nativeLibrary = nullptr;
    Require(FT_Init_FreeType_nid_postfix(&library) == 0 && FT_Init_FreeType(&nativeLibrary) == 0);
    int scalars = 0;
    int lists = 0;
    CompareQueries(nullptr, nullptr, scalars, lists);
    {
        FacePair regular(library, nativeLibrary, FontFixture::VariationFont(true, true));
        FacePair noVariants(library, nativeLibrary, FontFixture::VariationFont(true, false));
        FacePair noBase(library, nativeLibrary, FontFixture::VariationFont(false, true));
        FacePair noMap(library, nativeLibrary, FontFixture::VariationFont(false, false));
        Require(regular.guest->num_charmaps == 4 && noBase.guest->num_charmaps == 1 && noMap.guest->num_charmaps == 0);
        Require(noBase.guest->charmap == nullptr && noBase.native->charmap == nullptr);
        Require(noBase.guest->charmaps[0]->platform_id == 0 && noBase.guest->charmaps[0]->encoding_id == 5);
        Require(regular.guest->charmap->encoding_id == 10);
        Require(CopyList(FT_Face_GetVariantSelectors_nid_postfix(regular.guest)) == std::vector<std::uint32_t>({0xFE0F, 0xE0100}));
        Require(CopyList(FT_Face_GetCharsOfVariant_nid_postfix(regular.guest, 0xFE0F)) == std::vector<std::uint32_t>({'A', 'B', 'C', 0x4E00, 0x1F600}));
        Require(CopyList(FT_Face_GetVariantsOfChar_nid_postfix(regular.guest, 'D')).empty());
        Require(FT_Face_GetCharVariantIndex_nid_postfix(regular.guest, 'A', 0xFE0F) == 1);
        Require(FT_Face_GetCharVariantIndex_nid_postfix(regular.guest, 'A', 0xE0100) == 3);
        Require(FT_Face_GetCharVariantIndex_nid_postfix(regular.guest, 0x100000041ull, 0x10000FE0Full) == 1);
        Require(FT_Face_GetCharVariantIsDefault_nid_postfix(regular.guest, 'A', 0xFE0F) == 1);
        Require(FT_Face_GetCharVariantIsDefault_nid_postfix(regular.guest, 'C', 0xFE0F) == 0);
        Require(FT_Face_GetCharVariantIsDefault_nid_postfix(regular.guest, 'D', 0xFE0F) == -1);
        regular.guest->bbox.xMin = -0x123456789ABCll;
        CompareQueries(regular.guest, regular.native, scalars, lists);
        for (int index = 0; index < regular.guest->num_charmaps; ++index) {
            const auto* map = regular.guest->charmaps[index];
            if (map->platform_id != 3 || map->encoding_id == 10) continue;
            Require(FT_Set_Charmap_nid_postfix(regular.guest, regular.guest->charmaps[index]) == 0);
            Require(FT_Set_Charmap(regular.native, regular.native->charmaps[index]) == 0);
            Require(FT_Face_GetCharVariantIndex_nid_postfix(regular.guest, 'A', 0xFE0F) == (map->encoding_id == 1 ? 3u : 0u));
            CompareQueries(regular.guest, regular.native, scalars, lists);
        }
        Require(FT_Face_GetCharVariantIndex_nid_postfix(noBase.guest, 'A', 0xE0100) == 0);
        Require(FT_Face_GetCharVariantIndex_nid_postfix(noBase.guest, 'A', 0xFE0F) == 0);
        Require(FT_Face_GetCharVariantIsDefault_nid_postfix(noBase.guest, 'A', 0xFE0F) == 1);
        CompareQueries(noVariants.guest, noVariants.native, scalars, lists);
        CompareQueries(noBase.guest, noBase.native, scalars, lists);
        CompareQueries(noMap.guest, noMap.native, scalars, lists);
    }
    Require(scalars == 1344 && lists == 147);
    std::printf("FreeType variation selectors: %d scalar comparisons, %d terminated-list comparisons\n", scalars, lists);
    Require(FT_Done_FreeType_nid_postfix(library) == 0 && FT_Done_FreeType(nativeLibrary) == 0);
}
