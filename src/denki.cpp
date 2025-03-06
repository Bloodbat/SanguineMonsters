#include "denki.hpp"

Denki::Denki() {
	config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

	for (int section = 0; section < kMaxSections; ++section) {
		int channelNumber = section + 1;
		configInput(INPUT_GAIN_CV + section, string::f("Channel %d gain CV", channelNumber));
		configInput(INPUT_OFFSET_CV + section, string::f("Channel %d offset CV", channelNumber));
	}
}

void Denki::onExpanderChange(const ExpanderChangeEvent& e) {
	Module* kitsuneMaster = getLeftExpander().module;
	bool bHasMaster = (kitsuneMaster && kitsuneMaster->getModel() == modelKitsune);
	if (bHasMaster) {
		lights[LIGHT_MASTER_MODULE].setBrightness(kSanguineButtonLightValue);
	} else {
		lights[LIGHT_MASTER_MODULE].setBrightness(0.f);
	}
}

struct DenkiWidget : SanguineModuleWidget {
	explicit DenkiWidget(Denki* module) {
		setModule(module);

		moduleName = "denki";
		panelSize = SIZE_6;
		backplateColor = PLATE_PURPLE;
		bFaceplateSuffix = false;

		makePanel();

		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 1 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 1 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addChild(createLightCentered<SmallLight<OrangeLight>>(millimetersToPixelsVec(2.769, 5.573), module, Denki::LIGHT_MASTER_MODULE));

		// Channel 1
		SanguineStaticRGBLight* cvGainLight1 = new SanguineStaticRGBLight(module, "res/light_cv_lit.svg", 6.76, 11.636, true, kSanguineBlueLight);
		addChild(cvGainLight1);

		SanguineStaticRGBLight* cvOffsetLight1 = new SanguineStaticRGBLight(module, "res/arrow_left_right.svg", 23.72, 11.636, true, kSanguineBlueLight);
		addChild(cvOffsetLight1);

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(6.76, 20.671), module, Denki::INPUT_GAIN_CV + 0));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(23.72, 20.671), module, Denki::INPUT_OFFSET_CV + 0));

		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(6.76, 30.769), module, Denki::LIGHT_GAIN_CV + 0 * 3));

		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(23.72, 30.769), module, Denki::LIGHT_OFFSET_CV + 0 * 3));

		// Channel 2
		SanguineStaticRGBLight* cvGainLight2 = new SanguineStaticRGBLight(module, "res/light_cv_lit.svg", 6.76, 40.419, true, kSanguineBlueLight);
		addChild(cvGainLight2);

		SanguineStaticRGBLight* cvOffsetLight2 = new SanguineStaticRGBLight(module, "res/arrow_left_right.svg", 23.72, 40.419, true, kSanguineBlueLight);
		addChild(cvOffsetLight2);

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(6.76, 49.454), module, Denki::INPUT_GAIN_CV + 1));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(23.72, 49.454), module, Denki::INPUT_OFFSET_CV + 1));

		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(6.76, 59.552), module, Denki::LIGHT_GAIN_CV + 1 * 3));

		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(23.72, 59.552), module, Denki::LIGHT_OFFSET_CV + 1 * 3));

		// Channel 3
		SanguineStaticRGBLight* cvGainLight3 = new SanguineStaticRGBLight(module, "res/light_cv_lit.svg", 6.76, 69.213, true, kSanguineBlueLight);
		addChild(cvGainLight3);

		SanguineStaticRGBLight* cvOffsetLight3 = new SanguineStaticRGBLight(module, "res/arrow_left_right.svg", 23.72, 69.213, true, kSanguineBlueLight);
		addChild(cvOffsetLight3);

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(6.76, 78.248), module, Denki::INPUT_GAIN_CV + 2));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(23.72, 78.248), module, Denki::INPUT_OFFSET_CV + 2));

		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(6.76, 88.347), module, Denki::LIGHT_GAIN_CV + 2 * 3));

		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(23.72, 88.347), module, Denki::LIGHT_OFFSET_CV + 2 * 3));

		// Channel 4
		SanguineStaticRGBLight* cvGainLight4 = new SanguineStaticRGBLight(module, "res/light_cv_lit.svg", 6.76, 97.384, true, kSanguineBlueLight);
		addChild(cvGainLight4);

		SanguineStaticRGBLight* cvOffsetLight4 = new SanguineStaticRGBLight(module, "res/arrow_left_right.svg", 23.72, 97.384, true, kSanguineBlueLight);
		addChild(cvOffsetLight4);

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(6.76, 106.419), module, Denki::INPUT_GAIN_CV + 3));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(23.72, 106.419), module, Denki::INPUT_OFFSET_CV + 3));

		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(6.76, 116.517), module, Denki::LIGHT_GAIN_CV + 3 * 3));

		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(23.72, 116.517), module, Denki::LIGHT_OFFSET_CV + 3 * 3));
	}
};

Model* modelDenki = createModel<Denki, DenkiWidget>("Sanguine-Denki");