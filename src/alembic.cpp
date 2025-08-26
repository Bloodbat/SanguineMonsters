#include "alembic.hpp"

Alembic::Alembic() {
	config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

	for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
		int channelNumber = channel + 1;
		configInput(INPUT_GAIN_CV + channel, string::f("Channel %d gain CV", channelNumber));
		configOutput(OUTPUT_CHANNEL + channel, string::f("Channel %d", channelNumber));
	}
}

bool Alembic::getOutputConnected(const int channel) const {
	return outputsConnected[channel];
}
bool Alembic::getInputConnected(const int channel) const {
	return inputsConnected[channel];
}

void Alembic::setOutputConnected(const int channel, const bool value) {
	outputsConnected[channel] = value;
}
void Alembic::setInputConnected(const int channel, const bool value) {
	inputsConnected[channel] = value;
}

void Alembic::onExpanderChange(const ExpanderChangeEvent& e) {
	if (e.side == 0) {
		Module* alchemistMaster = getLeftExpander().module;
		bool bHasLeftMaster = (alchemistMaster && alchemistMaster->getModel() == modelAlchemist);
		if (bHasLeftMaster) {
			lights[LIGHT_MASTER_MODULE].setBrightness(kSanguineButtonLightValue);
			bHadMaster = true;
		} else if (bHadMaster && !bHasLeftMaster) {
			lights[LIGHT_MASTER_MODULE].setBrightness(0.f);
			for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
				outputs[OUTPUT_CHANNEL + channel].setVoltage(0.f);
			}
		}
	}
}

void Alembic::onPortChange(const PortChangeEvent& e) {
	switch (e.type) {
	case Port::OUTPUT:
		outputsConnected[e.portId] = e.connecting;
		break;

	case Port::INPUT:
		inputsConnected[e.portId] = e.connecting;
		break;
	}
}

struct AlembicWidget : SanguineModuleWidget {
	explicit AlembicWidget(Alembic* module) {
		setModule(module);

		moduleName = "alembic";
		panelSize = SIZE_10;
		backplateColor = PLATE_PURPLE;
		bFaceplateSuffix = false;

		makePanel();

		addScrews(SCREW_ALL);

		addChild(createLightCentered<SmallLight<OrangeLight>>(millimetersToPixelsVec(2.6, 5.573), module, Alembic::LIGHT_MASTER_MODULE));

		SanguineMonoOutputLight* outMonoLight1 = new SanguineMonoOutputLight(module, 6.466, 17.494);
		addChild(outMonoLight1);

		SanguineMonoOutputLight* outMonoLight2 = new SanguineMonoOutputLight(module, 31.404, 17.494);
		addChild(outMonoLight2);

		SanguineStaticRGBLight* cvLight1 = new SanguineStaticRGBLight(module, "res/light_cv_lit.svg", 19.397, 17.494, true, kSanguineYellowLight);
		addChild(cvLight1);

		SanguineStaticRGBLight* cvLight2 = new SanguineStaticRGBLight(module, "res/light_cv_lit.svg", 44.334, 17.494, true, kSanguineYellowLight);
		addChild(cvLight2);

		static const int offset = 8;

		static const float portBaseY = 25.703;
		static const float deltaY = 13.01;

		float currentPortY = portBaseY;

		for (int component = 0; component < 8; ++component) {
			addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(6.466, currentPortY), module, Alembic::OUTPUT_CHANNEL + component));
			addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(31.403, currentPortY), module, Alembic::OUTPUT_CHANNEL + component + offset));

			addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(19.397, currentPortY), module, Alembic::INPUT_GAIN_CV + component));
			addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(44.334, currentPortY), module, Alembic::INPUT_GAIN_CV + component + offset));

			currentPortY += deltaY;
		}
	}
};

Model* modelAlembic = createModel<Alembic, AlembicWidget>("Sanguine-Alembic");