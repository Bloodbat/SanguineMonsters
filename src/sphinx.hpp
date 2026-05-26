#pragma once

#include "rack.hpp"

namespace sphinx {
    static const int kMaxLength = 32;

    static const int kLightsFrequency = 16;

    enum PatternStyle {
        EUCLIDEAN_PATTERN,
        RANDOM_PATTERN,
        FIBONACCI_PATTERN,
        LINEAR_PATTERN
    };

    static const std::array<bool, kMaxLength * 2> browserSequence = {
        true,
        false,
        false,
        false,
        true,
        false,
        false,
        false,
        true,
        false,
        false,
        false,
        true,
        false,
        false,
        false
    };

    enum GateMode {
        GM_TRIGGER,
        GM_GATE,
        GM_TURING
    };

    struct RGBBits {
        bool red;
        bool green;
        bool blue;
    };

    static const RGBBits patternLightColorTable[4] = {
        {true, false, false},
        {true, false, true},
        {false, true, false},
        {false, false, true}
    };

    static const RGBBits gateModeLightColorTable[3] = {
        {false, false, true},
        {false, true, false},
        {true, false, false}
    };

    struct DisplayColors {
        NVGcolor backgroundColor;
        NVGcolor inactiveColor;
        NVGcolor activeColor;
    };

    static const DisplayColors displayColors[4] = {
        {nvgRGB(0x30, 0x10, 0x10), nvgRGB(0x7f, 0x00, 0x00), nvgRGB(0xff, 0x00, 0x00)},
        {nvgRGB(0x30, 0x10, 0x30), nvgRGB(0x7f, 0x00, 0x7f), nvgRGB(0xff, 0x00, 0xff)},
        {nvgRGB(0x10, 0x30, 0x10), nvgRGB(0x00, 0x7f, 0x00), nvgRGB(0x00, 0xff, 0x00)},
        {nvgRGB(0x10, 0x10, 0x30), nvgRGB(0x00, 0x00, 0x7f), nvgRGB(0x00, 0x00, 0xff)}
    };

    static constexpr float kDoublePi = 2.f * M_PI;
    static constexpr float kHalfPi = 0.5f * M_PI;

    static const std::vector<std::string> patternStyleLabels = {
        "Euclidean",
        "Random",
        "Fibonacci",
        "Linear"
    };

    static const std::vector<std::string> gateModeLabels = {
        "Trigger",
        "Gate",
        "Turing"
    };

    static const std::vector<std::string> labelsLength = {
        "1 steps",
        "2 steps",
        "3 steps",
        "4 steps",
        "5 steps",
        "6 steps",
        "7 steps",
        "8 steps",
        "9 steps",
        "10 steps",
        "11 steps",
        "12 steps",
        "13 steps",
        "14 steps",
        "15 steps",
        "16 steps",
        "17 steps",
        "18 steps",
        "19 steps",
        "20 steps",
        "21 steps",
        "22 steps",
        "23 steps",
        "24 steps",
        "25 steps",
        "26 steps",
        "27 steps",
        "28 steps",
        "29 steps",
        "30 steps",
        "31 steps",
        "32 steps"
    };
}