#pragma once

namespace dungeon {
#ifndef METAMODULE
    struct GradientColors {
        NVGcolor innerColor;
        NVGcolor outerColor;
    };

    static const std::vector<GradientColors> moonColors{
        { nvgRGB(251, 157, 157), nvgRGB(192, 124, 124) },
        { nvgRGB(228, 228, 228), nvgRGB(161, 161, 161) },
        { nvgRGB(181, 211, 237), nvgRGB(117, 172, 215) }
    };
#endif

    static const std::vector<std::string> modeLabels{
        "SH ",
        "TH",
        "HT"
    };

    static const int kLightsFrequency = 512;

    static const float kMinSlew = -9.965784285; // std::log2(1e-3f)
    static const float kMaxSlew = 3.321928095; // std::log2(10.f)
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