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
}