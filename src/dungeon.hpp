#pragma once

namespace dungeon {
    struct GradientColors {
        NVGcolor innerColor;
        NVGcolor outerColor;
    };

    static const std::vector<GradientColors> moonColors{
        { nvgRGB(251, 157, 157), nvgRGB(192, 124, 124) },
        { nvgRGB(228, 228, 228), nvgRGB(161, 161, 161) },
        { nvgRGB(181, 211, 237), nvgRGB(117, 172, 215) }
    };

    static const std::vector<std::string> modeLabels{
        "SH ",
        "TH",
        "HT"
    };
}

struct SlewFilter {
    float value = 0.f;

    float process(float in, float slew) {
        value += math::clamp(in - value, -slew, slew);
        return value;
    }
    float getValue() {
        return value;
    }
};