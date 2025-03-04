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
        ENUMS(LIGHT_STEP_1_LEFT, 3),
        ENUMS(LIGHT_STEP_2_LEFT, 3),
        ENUMS(LIGHT_STEP_3_LEFT, 3),
        ENUMS(LIGHT_STEP_4_LEFT, 3),
        ENUMS(LIGHT_STEP_5_LEFT, 3),
        ENUMS(LIGHT_STEP_6_LEFT, 3),
        ENUMS(LIGHT_STEP_7_LEFT, 3),
        ENUMS(LIGHT_STEP_8_LEFT, 3),
        ENUMS(LIGHT_STEP_1_RIGHT, 3),
        ENUMS(LIGHT_STEP_2_RIGHT, 3),
        ENUMS(LIGHT_STEP_3_RIGHT, 3),
        ENUMS(LIGHT_STEP_4_RIGHT, 3),
        ENUMS(LIGHT_STEP_5_RIGHT, 3),
        ENUMS(LIGHT_STEP_6_RIGHT, 3),
        ENUMS(LIGHT_STEP_7_RIGHT, 3),
        ENUMS(LIGHT_STEP_8_RIGHT, 3),
        LIGHTS_COUNT
    };

    Manus();

    void onExpanderChange(const ExpanderChangeEvent& e) override;
};