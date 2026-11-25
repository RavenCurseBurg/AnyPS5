#ifndef CORE_LIBS_TESTS_FREETYPEVARIATIONFONTFIXTURE_HPP
#define CORE_LIBS_TESTS_FREETYPEVARIATIONFONTFIXTURE_HPP

#include "FontFixture.hpp"

#include <cstdint>
#include <utility>
#include <vector>

namespace FontFixture {

inline void Put32(std::vector<unsigned char>& bytes, std::uint32_t value) {
    Put16(bytes, static_cast<int>(value >> 16));
    Put16(bytes, static_cast<int>(value & 0xFFFF));
}

inline void Put24(std::vector<unsigned char>& bytes, std::uint32_t value) {
    bytes.push_back(static_cast<unsigned char>(value >> 16));
    Put16(bytes, static_cast<int>(value & 0xFFFF));
}

inline std::vector<unsigned char> VariationFont(bool baseMaps, bool variations) {
    const auto format4 = Words({4, 32, 0, 4, 4, 1, 0, 'A', 0xFFFF, 0, 'A', 0xFFFF, 3 - 'A', 1, 0, 0});
    auto format12 = Words({12, 0, 0, 16 + 12 * 5, 0, 0, 0, 5});
    for (const auto& mapping : {std::pair{0x41u, 1u}, {0x42u, 2u}, {0x43u, 3u}, {0x4E00u, 3u}, {0x1F600u, 1u}}) {
        Put32(format12, mapping.first);
        Put32(format12, mapping.first);
        Put32(format12, mapping.second);
    }
    std::vector<unsigned char> defaultFirst;
    Put32(defaultFirst, 2);
    Put24(defaultFirst, 'A');
    defaultFirst.push_back(1);
    Put24(defaultFirst, 0x1F600);
    defaultFirst.push_back(0);
    std::vector<unsigned char> otherFirst;
    Put32(otherFirst, 2);
    for (const auto& mapping : {std::pair{0x43u, 2u}, {0x4E00u, 3u}}) {
        Put24(otherFirst, mapping.first);
        Put16(otherFirst, static_cast<int>(mapping.second));
    }
    std::vector<unsigned char> defaultSecond;
    Put32(defaultSecond, 1);
    Put24(defaultSecond, 'B');
    defaultSecond.push_back(1);
    std::vector<unsigned char> otherSecond;
    Put32(otherSecond, 2);
    for (const auto& mapping : {std::pair{0x41u, 3u}, {0x1F600u, 2u}}) {
        Put24(otherSecond, mapping.first);
        Put16(otherSecond, static_cast<int>(mapping.second));
    }
    auto format14 = Words({14});
    const std::uint32_t dataStart = 10 + 2 * 11;
    Put32(format14, dataStart + static_cast<std::uint32_t>(defaultFirst.size() + otherFirst.size() + defaultSecond.size() + otherSecond.size()));
    Put32(format14, 2);
    Put24(format14, 0xFE0F);
    Put32(format14, dataStart);
    Put32(format14, dataStart + static_cast<std::uint32_t>(defaultFirst.size()));
    Put24(format14, 0xE0100);
    Put32(format14, dataStart + static_cast<std::uint32_t>(defaultFirst.size() + otherFirst.size()));
    Put32(format14, dataStart + static_cast<std::uint32_t>(defaultFirst.size() + otherFirst.size() + defaultSecond.size()));
    for (const auto* table : {&defaultFirst, &otherFirst, &defaultSecond, &otherSecond}) format14.insert(format14.end(), table->begin(), table->end());
    struct Map {
        int platform;
        int encoding;
        std::vector<unsigned char> bytes;
    };
    std::vector<Map> maps;
    if (baseMaps) maps = {{3, 10, format12}, {3, 1, format4}, {3, 0, format4}};
    if (variations) maps.push_back({0, 5, format14});
    if (maps.empty()) return BuildFont(4, {});
    auto cmap = Words({0, static_cast<int>(maps.size())});
    auto offset = static_cast<std::uint32_t>(4 + 8 * maps.size());
    for (const auto& map : maps) {
        Put16(cmap, map.platform);
        Put16(cmap, map.encoding);
        Put32(cmap, offset);
        offset += static_cast<std::uint32_t>(map.bytes.size());
    }
    for (const auto& map : maps) cmap.insert(cmap.end(), map.bytes.begin(), map.bytes.end());
    return BuildFont(4, {{"cmap", cmap}});
}

}

#endif
