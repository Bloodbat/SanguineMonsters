#include "manus.hpp"

Manus::Manus() {
    config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);
}

void Manus::onExpanderChange(const ExpanderChangeEvent& e) {
    Module* gegeenesMaster = getLeftExpander().module;
    Module* hydraMaster = getRightExpander().module;
    bool bHasLeftMaster = gegeenesMaster && gegeenesMaster->getModel() == modelSuperSwitch18;
    bool bHasRightMaster = hydraMaster && hydraMaster->getModel() == modelSuperSwitch81;
    if (bHasLeftMaster) {
        lights[LIGHT_MASTER_MODULE_LEFT].setBrightness(0.75f);
    } else {
        lights[LIGHT_MASTER_MODULE_LEFT].setBrightness(0.f);
        for (int light = 0; light < kMaxSteps; ++light) {
            const int currentLight = LIGHT_STEP_1_LEFT + light;
            lights[currentLight].setBrightness(0.f);
        }
    }
    if (bHasRightMaster) {
        lights[LIGHT_MASTER_MODULE_RIGHT].setBrightness(0.75f);
    } else {
        lights[LIGHT_MASTER_MODULE_RIGHT].setBrightness(0.f);
        for (int light = 0; light < kMaxSteps; ++light) {
            const int currentLight = LIGHT_STEP_1_RIGHT + light;
            lights[currentLight].setBrightness(0.f);
        }
    }
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
        panelSize = SIZE_4;
        backplateColor = PLATE_PURPLE;
        bFaceplateSuffix = false;

        makePanel();

        addChild(createWidget<ScrewBlack>(Vec(0.f, 0)));

        addChild(createWidget<ScrewBlack>(Vec(0.f, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addInput(createInput<BananutPurple>(millimetersToPixelsVec(6.160, 19.916), module, Manus::INPUT_STEP_1));

        addInput(createInput<BananutPurple>(millimetersToPixelsVec(6.160, 33.045), module, Manus::INPUT_STEP_2));

        addInput(createInput<BananutPurple>(millimetersToPixelsVec(6.160, 46.174), module, Manus::INPUT_STEP_3));

        addInput(createInput<BananutPurple>(millimetersToPixelsVec(6.160, 59.304), module, Manus::INPUT_STEP_4));

        addInput(createInput<BananutPurple>(millimetersToPixelsVec(6.160, 72.433), module, Manus::INPUT_STEP_5));

        addInput(createInput<BananutPurple>(millimetersToPixelsVec(6.160, 85.562), module, Manus::INPUT_STEP_6));

        addInput(createInput<BananutPurple>(millimetersToPixelsVec(6.160, 98.691), module, Manus::INPUT_STEP_7));

        addInput(createInput<BananutPurple>(millimetersToPixelsVec(6.160, 111.820), module, Manus::INPUT_STEP_8));

        addChild(createLight<SmallLight<OrangeLight>>(millimetersToPixelsVec(1.562, 8.646), module, Manus::LIGHT_MASTER_MODULE_LEFT));

        addChild(createLight<SmallLight<OrangeLight>>(millimetersToPixelsVec(16.682, 8.646), module, Manus::LIGHT_MASTER_MODULE_RIGHT));

        addChild(createLightCentered<AcrylicLeftTriangle>(millimetersToPixelsVec(3.338, 23.905), module, Manus::LIGHT_STEP_1_LEFT));
        addChild(createLightCentered<AcrylicLeftTriangle>(millimetersToPixelsVec(3.338, 37.034), module, Manus::LIGHT_STEP_2_LEFT));
        addChild(createLightCentered<AcrylicLeftTriangle>(millimetersToPixelsVec(3.338, 50.163), module, Manus::LIGHT_STEP_3_LEFT));
        addChild(createLightCentered<AcrylicLeftTriangle>(millimetersToPixelsVec(3.338, 63.292), module, Manus::LIGHT_STEP_4_LEFT));
        addChild(createLightCentered<AcrylicLeftTriangle>(millimetersToPixelsVec(3.338, 76.421), module, Manus::LIGHT_STEP_5_LEFT));
        addChild(createLightCentered<AcrylicLeftTriangle>(millimetersToPixelsVec(3.338, 89.55), module, Manus::LIGHT_STEP_6_LEFT));
        addChild(createLightCentered<AcrylicLeftTriangle>(millimetersToPixelsVec(3.338, 102.679), module, Manus::LIGHT_STEP_7_LEFT));
        addChild(createLightCentered<AcrylicLeftTriangle>(millimetersToPixelsVec(3.338, 115.808), module, Manus::LIGHT_STEP_8_LEFT));

        addChild(createLightCentered<AcrylicRightTriangle>(millimetersToPixelsVec(16.994, 23.905), module, Manus::LIGHT_STEP_1_RIGHT));
        addChild(createLightCentered<AcrylicRightTriangle>(millimetersToPixelsVec(16.994, 37.034), module, Manus::LIGHT_STEP_2_RIGHT));
        addChild(createLightCentered<AcrylicRightTriangle>(millimetersToPixelsVec(16.994, 50.163), module, Manus::LIGHT_STEP_3_RIGHT));
        addChild(createLightCentered<AcrylicRightTriangle>(millimetersToPixelsVec(16.994, 63.292), module, Manus::LIGHT_STEP_4_RIGHT));
        addChild(createLightCentered<AcrylicRightTriangle>(millimetersToPixelsVec(16.994, 76.421), module, Manus::LIGHT_STEP_5_RIGHT));
        addChild(createLightCentered<AcrylicRightTriangle>(millimetersToPixelsVec(16.994, 89.55), module, Manus::LIGHT_STEP_6_RIGHT));
        addChild(createLightCentered<AcrylicRightTriangle>(millimetersToPixelsVec(16.994, 102.679), module, Manus::LIGHT_STEP_7_RIGHT));
        addChild(createLightCentered<AcrylicRightTriangle>(millimetersToPixelsVec(16.994, 115.808), module, Manus::LIGHT_STEP_8_RIGHT));
    }
};

Model* modelManus = createModel<Manus, ManusWidget>("Sanguine-Manus");