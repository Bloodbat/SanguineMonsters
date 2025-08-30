#pragma once

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
}