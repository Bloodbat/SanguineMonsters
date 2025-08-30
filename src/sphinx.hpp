#pragma once

#include "rack.hpp"

namespace sphinx {
    static const int kMaxLength = 32;

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
}