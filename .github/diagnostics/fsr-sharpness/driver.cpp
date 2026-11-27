#include "UpscaleMode.hpp"
#include <array>
#include <bit>
#include <cerrno>
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
        if (std::feclearexcept(FE_ALL_EXCEPT) || std::feraiseexcept(FE_DIVBYZERO)) return 6;
        const int flags = std::fetestexcept(FE_ALL_EXCEPT);
        errno = EDOM;
        float result = 0.0f;
        bool parsed = true;
        try {
            result = AgcDriver::SelectUpscaleMode("fsr", text.c_str()).sharpness;
        } catch (const std::runtime_error&) {
            parsed = false;
        }
#if defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__) && __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 260000
        if (errno != EDOM || std::fetestexcept(FE_ALL_EXCEPT) != flags) return 7;
#endif
        if (std::fegetround() != selected) return 5;
        if (parsed) std::cout << std::hex << std::bit_cast<std::uint32_t>(result) << '\n';
        else std::cout << "invalid\n";
    }
}
