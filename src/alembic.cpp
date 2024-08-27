#include "alembic.hpp"

Alembic::Alembic() {
	config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

	for (int i = 0; i < 16; i++) {
		int channelNumber = i + 1;
		configInput(INPUT_GAIN_CV + i, string::f("Channel %d gain CV", channelNumber));
		configOutput(OUTPUT_CHANNEL + i, string::f("Channel %d", channelNumber));
	}
}

void Alembic::onExpanderChange(const ExpanderChangeEvent& e) {
	Module* alchemistMaster = getLeftExpander().module;
	bool hasMaster = (alchemistMaster && alchemistMaster->getModel() == modelAlchemist);
	if (hasMaster) {
		lights[LIGHT_MASTER_MODULE].setBrightness(1.f);
	}
	else {
		lights[LIGHT_MASTER_MODULE].setBrightness(0.f);
		for (int i = 0; i < PORT_MAX_CHANNELS; i++) {
			outputs[OUTPUT_CHANNEL + i].setVoltage(0.f);
		}
	}
}

struct AlembicWidget : ModuleWidget {
	AlembicWidget(Alembic* module) {
		setModule(module);

		SanguinePanel* panel = new SanguinePanel("res/backplate_10hp_purple.svg", "res/alembic.svg");
		setPanel(panel);

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addChild(createLightCentered<MediumLight<OrangeLight>>(millimetersToPixelsVec(4.899, 9.672), module, Alembic::LIGHT_MASTER_MODULE));

		SanguineMonoOutputLight* outMonoLight1 = new SanguineMonoOutputLight(module, 6.466, 17.494);
		addChild(outMonoLight1);

		SanguineMonoOutputLight* outMonoLight2 = new SanguineMonoOutputLight(module, 31.404, 17.494);
		addChild(outMonoLight2);

		SanguineStaticRGBLight* cvLight1 = new SanguineStaticRGBLight(module, "res/light_cv_lit.svg", 19.397, 17.494, true, kSanguineYellowLight);
		addChild(cvLight1);

		SanguineStaticRGBLight* cvLight2 = new SanguineStaticRGBLight(module, "res/light_cv_lit.svg", 44.334, 17.494, true, kSanguineYellowLight);
		addChild(cvLight2);

		const float portBaseY = 25.703;
		const float deltaY = 13.01;

		float currentPortY = portBaseY;

		for (int i = 0; i < 8; i++) {
			addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(6.466, currentPortY), module, Alembic::OUTPUT_CHANNEL + i));
			addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(19.397, currentPortY), module, Alembic::INPUT_GAIN_CV + i));
			currentPortY += deltaY;
		}

		const int offset = 8;

		currentPortY = portBaseY;

		for (int i = 0; i < 8; i++) {
			addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(31.403, currentPortY), module, Alembic::OUTPUT_CHANNEL + i + offset));
			addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(44.334, currentPortY), module, Alembic::INPUT_GAIN_CV + i + offset));
			currentPortY += deltaY;
		}
	}
};

Model* modelAlembic = createModel<Alembic, AlembicWidget>("Sanguine-Alembic");