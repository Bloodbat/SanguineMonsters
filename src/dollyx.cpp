#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"

using simd::float_4;

struct DollyX : SanguineModule {

	enum ParamIds {
		PARAM_CHANNELS1,
		PARAM_CHANNELS2,
		PARAMS_COUNT
	};

	enum InputIds {
		INPUT_MONO_IN1,
		INPUT_MONO_IN2,
		INPUT_CHANNELS1_CV,
		INPUT_CHANNELS2_CV,
		INPUTS_COUNT
	};

	enum OutputIds {
		OUTPUT_POLYOUT_1,
		OUTPUT_POLYOUT_2,
		OUTPUTS_COUNT
	};

	static const int kSubmodules = 2;
	static const int kClockDivision = 64;

	int cloneCounts[kSubmodules];

	bool cvsConnected[kSubmodules] = {};
	bool inputsConnected[kSubmodules] = {};
	bool outputsConnected[kSubmodules] = {};

	dsp::ClockDivider clockDivider;

	DollyX() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, 0);

		for (int submodule = 0; submodule < kSubmodules; ++submodule) {
			int componentOffset = submodule + 1;

			configParam(PARAM_CHANNELS1 + submodule, 1.f, PORT_MAX_CHANNELS, PORT_MAX_CHANNELS, string::f("Clone count %d", componentOffset));
			paramQuantities[PARAM_CHANNELS1]->snapEnabled = true;

			configOutput(OUTPUT_POLYOUT_1 + submodule, string::f("Cloned %d", componentOffset));

			configInput(INPUT_MONO_IN1 + submodule, string::f("Mono %d", componentOffset));

			configInput(INPUT_CHANNELS1_CV + submodule, string::f("Channels CV %d", componentOffset));

			cloneCounts[submodule] = PORT_MAX_CHANNELS;
		}

		clockDivider.setDivision(kClockDivision);
		onReset();
	};

	void process(const ProcessArgs& args) override {
		if (clockDivider.process()) {
			checkConnections();
			updateCloneCounts();
		}

		for (int submodule = 0; submodule < kSubmodules; ++submodule) {
			float_4 voltages[4] = {};

			if (outputsConnected[submodule]) {
				for (int channel = 0; channel < cloneCounts[submodule]; channel += 4) {
					uint8_t currentChannel = channel >> 2;

					if (inputsConnected[submodule]) {
						voltages[currentChannel] = inputs[INPUT_MONO_IN1 + submodule].getVoltage();
					}
					outputs[OUTPUT_POLYOUT_1 + submodule].setVoltageSimd(voltages[currentChannel], channel);
				}
			} else {
				outputs[OUTPUT_POLYOUT_1 + submodule].setChannels(0);
			}
		}
	}

	int getChannelCloneCount(int channel) {
		if (cvsConnected[channel]) {
			float inputValue = math::clamp(inputs[INPUT_CHANNELS1_CV + channel].getVoltage(), 0.f, 10.f);
			int steps = static_cast<int>(rescale(inputValue, 0.f, 10.f, 1.f, 16.f));
			return steps;
		} else {
			return std::floor(params[PARAM_CHANNELS1 + channel].getValue());
		}
	}

	void updateCloneCounts() {
		for (int submodule = 0; submodule < kSubmodules; ++submodule) {
			cloneCounts[submodule] = getChannelCloneCount(submodule);
			outputs[OUTPUT_POLYOUT_1 + submodule].setChannels(cloneCounts[submodule]);
		}
	}

	void onReset() override {
		for (int submodule = 0; submodule < kSubmodules; ++submodule) {
			cloneCounts[submodule] = PORT_MAX_CHANNELS;
		}
	}

	void checkConnections() {
		for (int submodule = 0; submodule < kSubmodules; ++submodule) {
			cvsConnected[submodule] = inputs[INPUT_CHANNELS1_CV + submodule].isConnected();
			inputsConnected[submodule] = inputs[INPUT_MONO_IN1 + submodule].isConnected();
			outputsConnected[submodule] = outputs[OUTPUT_POLYOUT_1 + submodule].isConnected();
		}
	}
};

struct DollyXWidget : SanguineModuleWidget {
	explicit DollyXWidget(DollyX* module) {
		setModule(module);

		moduleName = "dolly-x";
		panelSize = SIZE_8;
		backplateColor = PLATE_PURPLE;
		bFaceplateSuffix = false;

		makePanel();

		addScrews(SCREW_ALL);

		FramebufferWidget* dollyFrameBuffer = new FramebufferWidget();
		addChild(dollyFrameBuffer);

		// Section 1
		SanguineLedNumberDisplay* displayCloner1 = new SanguineLedNumberDisplay(2, module, 13.713, 24.498);
		dollyFrameBuffer->addChild(displayCloner1);
		displayCloner1->fallbackNumber = 16;

		addParam(createParamCentered<BefacoTinyKnobRed>(millimetersToPixelsVec(29.945, 24.543), module, DollyX::PARAM_CHANNELS1));

#ifndef METAMODULE
		SanguineStaticRGBLight* amalgamLight1 = new SanguineStaticRGBLight(module, "res/amalgam_light.svg", 13.713, 36.856, true, kSanguineBlueLight);
		addChild(amalgamLight1);

		SanguineMonoInputLight* inMonoLight1 = new SanguineMonoInputLight(module, 9.871, 49.743);
		addChild(inMonoLight1);

		SanguinePolyOutputLight* outPolyLight1 = new SanguinePolyOutputLight(module, 30.769, 49.743);
		addChild(outPolyLight1);
#endif

		addInput(createInputCentered<BananutBlack>(millimetersToPixelsVec(29.945, 36.856), module, DollyX::INPUT_CHANNELS1_CV));

		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(9.871, 56.666), module, DollyX::INPUT_MONO_IN1));

		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(30.769, 56.666), module, DollyX::OUTPUT_POLYOUT_1));

		// Section 2
		SanguineLedNumberDisplay* displayCloner2 = new SanguineLedNumberDisplay(2, module, 13.713, 79.168);
		dollyFrameBuffer->addChild(displayCloner2);
		displayCloner2->fallbackNumber = 16;

		addParam(createParamCentered<BefacoTinyKnobBlack>(millimetersToPixelsVec(29.945, 79.214), module, DollyX::PARAM_CHANNELS2));

#ifndef METAMODULE
		SanguineStaticRGBLight* amalgamLight2 = new SanguineStaticRGBLight(module, "res/amalgam_light.svg", 13.713, 91.526, true, kSanguineBlueLight);
		addChild(amalgamLight2);

		SanguineMonoInputLight* inMonoLight2 = new SanguineMonoInputLight(module, 9.871, 104.413);
		addChild(inMonoLight2);

		SanguinePolyOutputLight* outPolyLight2 = new SanguinePolyOutputLight(module, 30.769, 104.413);
		addChild(outPolyLight2);
#endif

		addInput(createInputCentered<BananutBlack>(millimetersToPixelsVec(29.945, 91.526), module, DollyX::INPUT_CHANNELS2_CV));

		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(9.871, 111.337), module, DollyX::INPUT_MONO_IN2));

		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(30.769, 111.337), module, DollyX::OUTPUT_POLYOUT_2));

		if (module) {
			displayCloner1->values.numberValue = (&module->cloneCounts[0]);
			displayCloner2->values.numberValue = (&module->cloneCounts[1]);
		}
	}
};

Model* modelDollyX = createModel<DollyX, DollyXWidget>("Sanguine-Monsters-Dolly-X");