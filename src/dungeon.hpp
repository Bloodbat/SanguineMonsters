struct GradientColors {
    NVGcolor innerColor;
    NVGcolor outerColor;
};

static const std::vector<GradientColors> moonColors{
    { nvgRGB(247, 187, 187), nvgRGB(223, 33, 33) },
    { nvgRGB(217, 217, 217), nvgRGB(128, 128, 128) },
    { nvgRGB(187, 214, 247), nvgRGB(22, 117, 234) }
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