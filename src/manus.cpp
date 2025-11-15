#include "manus.hpp"

Manus::Manus() {
    config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

    for (int input = 0; input < superSwitches::kMaxSteps; ++input) {
        configInput(INPUT_STEP_1 + input, string::f("Step %d", input + 1));
    }
}

void Manus::onExpanderChange(const ExpanderChangeEvent& e) {
    if (e.side == 0) {
        Module* gegeenesMaster = getLeftExpander().module;

        bool bHasLeftMaster = gegeenesMaster && gegeenesMaster->getModel() == modelSuperSwitch18;

        if (bHasLeftMaster) {
            lights[LIGHT_MASTER_MODULE_LEFT].setBrightness(kSanguineButtonLightValue);
        } else {
            lights[LIGHT_MASTER_MODULE_LEFT].setBrightness(0.f);
            int currentLight;
            for (int light = 0; light < superSwitches::kMaxSteps; ++light) {
                currentLight = LIGHT_STEP_1_LEFT + light;
                lights[currentLight].setBrightness(0.f);
            }
        }
    } else {
        Module* hydraMaster = getRightExpander().module;

        bool bHasRightMaster = hydraMaster && hydraMaster->getModel() == modelSuperSwitch81;

        if (bHasRightMaster) {
            lights[LIGHT_MASTER_MODULE_RIGHT].setBrightness(kSanguineButtonLightValue);
        } else {
            lights[LIGHT_MASTER_MODULE_RIGHT].setBrightness(0.f);
            int currentLight;
            for (int light = 0; light < superSwitches::kMaxSteps; ++light) {
                currentLight = LIGHT_STEP_1_RIGHT + light;
                lights[currentLight].setBrightness(0.f);
            }
        }
    }
}

void Manus::onPortChange(const PortChangeEvent& e) {
    setInputConnected(e.portId, e.connecting);
}

struct AcrylicLeftTriangle : SanguineShapedAcrylicLed<RedLight> {
    AcrylicLeftTriangle() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/light_triangle_left.svg")));
    }
};

struct AcrylicRightTriangle : SanguineShapedAcrylicLed<RedLight> {
    AcrylicRightTriangle() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/light_triangle_right.svg")));
    }
};

struct ManusWidget : SanguineModuleWidget {
    explicit ManusWidget(Manus* module) {
        setModule(module);

        moduleName = "manus";
        panelSize = sanguineThemes::SIZE_5;
        backplateColor = sanguineThemes::PLATE_PURPLE;
        bFaceplateSuffix = false;

        makePanel();

        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH * 2, 0)));

        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH * 2, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addChild(createLightCentered<SmallLight<OrangeLight>>(millimetersToPixelsVec(2.6, 5.573),
            module, Manus::LIGHT_MASTER_MODULE_LEFT));

        addChild(createLightCentered<SmallLight<OrangeLight>>(millimetersToPixelsVec(22.8, 5.573),
            module, Manus::LIGHT_MASTER_MODULE_RIGHT));

        addInput(createInput<BananutPurple>(millimetersToPixelsVec(8.7, 19.904), module, Manus::INPUT_STEP_1));

        addInput(createInput<BananutPurple>(millimetersToPixelsVec(8.7, 33.033), module, Manus::INPUT_STEP_2));

        addInput(createInput<BananutPurple>(millimetersToPixelsVec(8.7, 46.162), module, Manus::INPUT_STEP_3));

        addInput(createInput<BananutPurple>(millimetersToPixelsVec(8.7, 59.292), module, Manus::INPUT_STEP_4));

        addInput(createInput<BananutPurple>(millimetersToPixelsVec(8.7, 72.421), module, Manus::INPUT_STEP_5));

        addInput(createInput<BananutPurple>(millimetersToPixelsVec(8.7, 85.55), module, Manus::INPUT_STEP_6));

        addInput(createInput<BananutPurple>(millimetersToPixelsVec(8.7, 98.679), module, Manus::INPUT_STEP_7));

        addInput(createInput<BananutPurple>(millimetersToPixelsVec(8.7, 111.808), module, Manus::INPUT_STEP_8));

        addChild(createLightCentered<AcrylicLeftTriangle>(millimetersToPixelsVec(5.194, 23.905), module, Manus::LIGHT_STEP_1_LEFT));
        addChild(createLightCentered<AcrylicLeftTriangle>(millimetersToPixelsVec(5.194, 37.034), module, Manus::LIGHT_STEP_2_LEFT));
        addChild(createLightCentered<AcrylicLeftTriangle>(millimetersToPixelsVec(5.194, 50.163), module, Manus::LIGHT_STEP_3_LEFT));
        addChild(createLightCentered<AcrylicLeftTriangle>(millimetersToPixelsVec(5.194, 63.292), module, Manus::LIGHT_STEP_4_LEFT));
        addChild(createLightCentered<AcrylicLeftTriangle>(millimetersToPixelsVec(5.194, 76.421), module, Manus::LIGHT_STEP_5_LEFT));
        addChild(createLightCentered<AcrylicLeftTriangle>(millimetersToPixelsVec(5.194, 89.55), module, Manus::LIGHT_STEP_6_LEFT));
        addChild(createLightCentered<AcrylicLeftTriangle>(millimetersToPixelsVec(5.194, 102.679), module, Manus::LIGHT_STEP_7_LEFT));
        addChild(createLightCentered<AcrylicLeftTriangle>(millimetersToPixelsVec(5.194, 115.808), module, Manus::LIGHT_STEP_8_LEFT));

        addChild(createLightCentered<AcrylicRightTriangle>(millimetersToPixelsVec(20.209, 23.905), module, Manus::LIGHT_STEP_1_RIGHT));
        addChild(createLightCentered<AcrylicRightTriangle>(millimetersToPixelsVec(20.209, 37.034), module, Manus::LIGHT_STEP_2_RIGHT));
        addChild(createLightCentered<AcrylicRightTriangle>(millimetersToPixelsVec(20.209, 50.163), module, Manus::LIGHT_STEP_3_RIGHT));
        addChild(createLightCentered<AcrylicRightTriangle>(millimetersToPixelsVec(20.209, 63.292), module, Manus::LIGHT_STEP_4_RIGHT));
        addChild(createLightCentered<AcrylicRightTriangle>(millimetersToPixelsVec(20.209, 76.421), module, Manus::LIGHT_STEP_5_RIGHT));
        addChild(createLightCentered<AcrylicRightTriangle>(millimetersToPixelsVec(20.209, 89.55), module, Manus::LIGHT_STEP_6_RIGHT));
        addChild(createLightCentered<AcrylicRightTriangle>(millimetersToPixelsVec(20.209, 102.679), module, Manus::LIGHT_STEP_7_RIGHT));
        addChild(createLightCentered<AcrylicRightTriangle>(millimetersToPixelsVec(20.209, 115.808), module, Manus::LIGHT_STEP_8_RIGHT));
    }
};

Model* modelManus = createModel<Manus, ManusWidget>("Sanguine-Manus");