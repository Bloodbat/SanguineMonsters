#include "crucible.hpp"

Crucible::Crucible() {
    config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

    configButton(PARAM_MUTE_ALL, "Mute all");
    configButton(PARAM_MUTE_EXCLUSIVE, "Mute exclusive");
    configButton(PARAM_SOLO_ALL, "Solo all");
    configButton(PARAM_SOLO_EXCLUSIVE, "Solo exclusive");
    configInput(INPUT_MUTE_ALL, "Mute all");
    configInput(INPUT_MUTE_POLY, "Mute channels");
    configInput(INPUT_SOLO_ALL, "Solo all");
    configInput(INPUT_SOLO_POLY, "Solo channels");
}

void Crucible::onExpanderChange(const ExpanderChangeEvent& e) {
    Module* alchemistMaster = getRightExpander().module;
    bool bHasRightMaster = alchemistMaster && alchemistMaster->getModel() == modelAlchemist;

    if (bHasRightMaster) {
        lights[LIGHT_MASTER_MODULE].setBrightness(kSanguineButtonLightValue);
    } else {
        lights[LIGHT_MASTER_MODULE].setBrightness(0.f);

        lights[LIGHT_MUTE_ALL].setBrightness(0.f);
        lights[LIGHT_MUTE_EXCLUSIVE].setBrightness(0.f);

        lights[LIGHT_SOLO_ALL].setBrightness(0.f);
        lights[LIGHT_SOLO_EXCLUSIVE].setBrightness(0.f);
    }
}

struct CrucibleWidget : SanguineModuleWidget {
    explicit CrucibleWidget(Crucible* module) {
        setModule(module);

        moduleName = "crucible";
        panelSize = SIZE_5;
        backplateColor = PLATE_PURPLE;
        bFaceplateSuffix = false;

        makePanel();

        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH * 2, 0)));

        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH * 2, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addChild(createLightCentered<SmallLight<OrangeLight>>(millimetersToPixelsVec(22.8, 5.573), module,
            Crucible::LIGHT_MASTER_MODULE));

        // Mute section
        addParam(createParamCentered<SanguineBezel115>(millimetersToPixelsVec(12.7, 25.243), module,
            Crucible::PARAM_MUTE_ALL));
        addChild(createLightCentered<SanguineBezelLight115<RedLight>>(millimetersToPixelsVec(12.7, 25.243), module,
            Crucible::LIGHT_MUTE_ALL));
        addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(12.7, 38.979), module, Crucible::INPUT_MUTE_ALL));
        addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(5.664, 51.499), module, Crucible::INPUT_MUTE_POLY));
        addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedLight>>>(millimetersToPixelsVec(20.259, 51.499),
            module, Crucible::PARAM_MUTE_EXCLUSIVE, Crucible::LIGHT_MUTE_EXCLUSIVE));

        // Solo section
        addParam(createParamCentered<SanguineBezel115>(millimetersToPixelsVec(12.7, 86.616), module,
            Crucible::PARAM_SOLO_ALL));
        addChild(createLightCentered<SanguineBezelLight115<GreenLight>>(millimetersToPixelsVec(12.7, 86.616), module,
            Crucible::LIGHT_SOLO_ALL));
        addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(12.7, 100.352), module, Crucible::INPUT_SOLO_ALL));
        addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(5.664, 112.872), module, Crucible::INPUT_SOLO_POLY));
        addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(millimetersToPixelsVec(20.259, 112.872),
            module, Crucible::PARAM_SOLO_EXCLUSIVE, Crucible::LIGHT_SOLO_EXCLUSIVE));
    }
};

Model* modelCrucible = createModel<Crucible, CrucibleWidget>("Sanguine-Crucible");