#pragma once

#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"

#include "switches.hpp"

struct Manus : SanguineModule {

    enum ParamIds {
        PARAMS_COUNT
    };

    enum InputIds {
        INPUT_STEP_1,
        INPUT_STEP_2,
        INPUT_STEP_3,
        INPUT_STEP_4,
        INPUT_STEP_5,
        INPUT_STEP_6,
        INPUT_STEP_7,
        INPUT_STEP_8,
        INPUTS_COUNT
    };


    enum OutputIds {
        OUTPUTS_COUNT
    };

    enum LightIds {
        LIGHT_MASTER_MODULE_LEFT,
        LIGHT_MASTER_MODULE_RIGHT,
        LIGHT_STEP_1_LEFT,
        LIGHT_STEP_2_LEFT,
        LIGHT_STEP_3_LEFT,
        LIGHT_STEP_4_LEFT,
        LIGHT_STEP_5_LEFT,
        LIGHT_STEP_6_LEFT,
        LIGHT_STEP_7_LEFT,
        LIGHT_STEP_8_LEFT,
        LIGHT_STEP_1_RIGHT,
        LIGHT_STEP_2_RIGHT,
        LIGHT_STEP_3_RIGHT,
        LIGHT_STEP_4_RIGHT,
        LIGHT_STEP_5_RIGHT,
        LIGHT_STEP_6_RIGHT,
        LIGHT_STEP_7_RIGHT,
        LIGHT_STEP_8_RIGHT,
        LIGHTS_COUNT
    };

    Manus();

    void onExpanderChange(const ExpanderChangeEvent& e) override;
};