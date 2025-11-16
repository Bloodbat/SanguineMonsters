#pragma once

#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"

using namespace sanguineCommonCode;

struct Crucible : SanguineModule {
    enum ParamIds {
        PARAM_MUTE_ALL,
        PARAM_MUTE_EXCLUSIVE,
        PARAM_SOLO_ALL,
        PARAM_SOLO_EXCLUSIVE,
        PARAMS_COUNT
    };

    enum InputIds {
        INPUT_MUTE_ALL,
        INPUT_MUTE_POLY,
        INPUT_SOLO_ALL,
        INPUT_SOLO_POLY,
        INPUTS_COUNT
    };


    enum OutputIds {
        OUTPUTS_COUNT
    };

    enum LightIds {
        LIGHT_MASTER_MODULE,
        LIGHT_MUTE_ALL,
        LIGHT_MUTE_EXCLUSIVE,
        LIGHT_SOLO_ALL,
        LIGHT_SOLO_EXCLUSIVE,
        LIGHTS_COUNT
    };

    Crucible();

    void onExpanderChange(const ExpanderChangeEvent& e) override;

private:
    bool bHadMaster = false;
};