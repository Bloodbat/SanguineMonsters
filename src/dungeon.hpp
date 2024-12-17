struct GradientColors {
    NVGcolor innerColor;
    NVGcolor outerColor;
};

static const std::vector<GradientColors> moonColors{
    { nvgRGB(246, 228, 228), nvgRGB(242, 160, 160) },
    { nvgRGB(228, 228, 228), nvgRGB(161, 161, 161) },
    { nvgRGB(244, 244, 250), nvgRGB(140, 195, 238) }
};

static const std::vector<std::string> dungeonModeLabels{
    "SH ",
    "TH",
    "HT"
};

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