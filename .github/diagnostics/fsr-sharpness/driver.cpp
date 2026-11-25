#include "UpscaleMode.hpp"
#include <array>
#include <bit>
#include <cfenv>
#include <clocale>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <locale>
#include <string>

struct CommaDecimal : std::numpunct<char> {
    char do_decimal_point() const override { return ','; }
};

int main(int argc, char** argv) {
    if (argc != 3) return 2;
    if (std::string_view(argv[2]) == "comma") {
        std::locale::global(std::locale(std::locale::classic(), new CommaDecimal));
#if defined(__APPLE__)
        if (!std::setlocale(LC_NUMERIC, "fr_FR.UTF-8")) return 3;
#endif
    }
    constexpr std::array rounding{FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};
    const auto selected = rounding.at(std::atoi(argv[1]));
    if (std::fesetround(selected) != 0) return 4;
    std::string text;
    while (std::getline(std::cin, text)) {
        try {
            const auto value = AgcDriver::SelectUpscaleMode("fsr", text.c_str()).sharpness;
            std::cout << std::hex << std::bit_cast<std::uint32_t>(value) << '\n';
        } catch (const std::runtime_error&) {
            std::cout << "invalid\n";
        }
        if (std::fegetround() != selected) return 5;
    }
}
