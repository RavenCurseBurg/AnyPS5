#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_UPSCALEMODE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_UPSCALEMODE_HPP

#include <charconv>
#include <cmath>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace AgcDriver {

struct UpscaleMode {
    bool fsr = false;
    float sharpness = 0.2f;
};

inline UpscaleMode SelectUpscaleMode(const char* upscale, const char* sharpness) {
    UpscaleMode mode;
    if (upscale != nullptr && *upscale != '\0') {
        if (std::string_view(upscale) != "fsr") throw std::runtime_error("Vulkan presentation: ANYPS5_UPSCALE must be fsr or unset");
        mode.fsr = true;
    }
    if (sharpness != nullptr && *sharpness != '\0') {
        const std::string_view text(sharpness);
        float value = 0.0f;
#if defined(__APPLE__)
        std::istringstream stream(sharpness);
        stream.imbue(std::locale::classic());
        stream >> std::noskipws >> value;
        const auto mantissa = text.substr(0, text.find_first_of("eE"));
        const bool underflowed = value == 0.0f && mantissa.find_first_of("123456789") != std::string_view::npos;
        const bool decimal = text.front() != '+' && text.find_first_not_of("0123456789eE+-.") == std::string_view::npos;
        const bool parsed = decimal && stream.eof() && (!stream.fail() || std::fpclassify(value) == FP_SUBNORMAL) && !underflowed;
#else
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        const bool parsed = error == std::errc{} && end == text.data() + text.size();
#endif
        if (!parsed || !std::isfinite(value) || value < 0.0f || value > 2.0f) throw std::runtime_error("Vulkan presentation: ANYPS5_FSR_SHARPNESS must be a number from 0.0 to 2.0");
        mode.sharpness = value;
    }
    return mode;
}

}

#endif
