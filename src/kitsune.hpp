#pragma once

#include "plugin.hpp"
#include "sanguinecomponents.hpp"

namespace kitsune {
    enum NormalledModes {
        NORMAL_NONE,
        NORMAL_SMART
    };

    static const std::vector<RGBLightColor> lightColors = {
        { 1.f, 0.f, 0.f },
        { 0.f, 1.f, 0.f },
        { 0.f, 0.f, 1.f },
        { 1.f, 1.f, 1.f }
    };

    static const std::vector<std::string> normallingModes = {
        "Off",
        "Smart"
    };
}