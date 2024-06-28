#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "pcg_variants.h"
#include "sanguinehelpers.hpp"

using simd::float_4;

struct DollyX : Module {

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

	static const int kSUBMODULES = 2;
	static const int kDEFAULT_CLONES = 16;

	int cloneCounts[kSUBMODULES];

	bool bCvConnected[kSUBMODULES];
	bool bInputConnected[kSUBMODULES];
	bool bOutputConnected[kSUBMODULES];

	dsp::ClockDivider clockDivider;

	DollyX() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, 0);

		configParam(PARAM_CHANNELS1, 1.0f, 16.0f, 16.0f, "Clone count 1", "", 0.0f, 1.0f, 0.0f);
		paramQuantities[PARAM_CHANNELS1]->snapEnabled = true;

		configParam(PARAM_CHANNELS2, 1.0f, 16.0f, 16.0f, "Clone count 2", "", 0.0f, 1.0f, 0.0f);
		paramQuantities[PARAM_CHANNELS2]->snapEnabled = true;

		configOutput(OUTPUT_POLYOUT_1, "Cloned 1");
		configOutput(OUTPUT_POLYOUT_2, "Cloned 2");

		configInput(INPUT_MONO_IN1, "Mono 1");
		configInput(INPUT_MONO_IN2, "Mono 2");
		configInput(INPUT_CHANNELS1_CV, "Channels CV 1");
		configInput(INPUT_CHANNELS2_CV, "Channels CV 2");

		clockDivider.setDivision(64);
		onReset();
	};

	void process(const ProcessArgs& args) override {
		if (clockDivider.process()) {
			checkConnections();
			updateCloneCounts();
		}

		for (int i = 0; i < kSUBMODULES; i++) {
			float_4 voltages[4] = {};

			if (bOutputConnected[i]) {
				for (int channel = 0; channel < cloneCounts[i]; channel += 4) {
					uint8_t currentChannel = channel >> 2;

					if (bInputConnected[i]) {
						voltages[currentChannel] = inputs[INPUT_MONO_IN1 + i].getVoltage();
					}
					outputs[OUTPUT_POLYOUT_1 + i].setVoltageSimd(voltages[currentChannel], channel);
				}
			}
			else
			{
				outputs[OUTPUT_POLYOUT_1 + i].setChannels(0);
			}
		}
	}

	int getChannelCloneCount(int channel) {
		if (bCvConnected[channel]) {
			float inputValue = math::clamp(inputs[INPUT_CHANNELS1_CV + channel].getVoltage(), 0.f, 10.f);
			int steps = (int)rescale(inputValue, 0.f, 10.f, 1.f, 16.f);
			return steps;
		}
		else {
			return std::floor(params[PARAM_CHANNELS1 + channel].getValue());
		}
	}

	void updateCloneCounts() {
		for (int i = 0; i < kSUBMODULES; i++) {
			cloneCounts[i] = getChannelCloneCount(i);
			outputs[OUTPUT_POLYOUT_1 + i].setChannels(cloneCounts[i]);
		}
	}

	void onReset() override {
		for (int i = 0; i < kSUBMODULES; i++) {
			cloneCounts[i] = kDEFAULT_CLONES;
		}
	}

	void checkConnections() {
		for (int i = 0; i < kSUBMODULES; i++) {
			bCvConnected[i] = inputs[INPUT_CHANNELS1_CV + i].isConnected();
			bInputConnected[i] = inputs[INPUT_MONO_IN1 + i].isConnected();
			bOutputConnected[i] = outputs[OUTPUT_POLYOUT_1 + i].isConnected();
		}
	}
};

struct DollyXWidget : ModuleWidget {
	DollyXWidget(DollyX* module) {
		setModule(module);

		SanguinePanel* panel = new SanguinePanel(pluginInstance, "res/backplate_8hp_purple.svg", "res/dolly-x.svg");
		setPanel(panel);

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<BefacoTinyKnobRed>(millimetersToPixelsVec(29.945, 21.843), module, DollyX::PARAM_CHANNELS1));
		addParam(createParamCentered<BefacoTinyKnobRed>(millimetersToPixelsVec(29.945, 76.514), module, DollyX::PARAM_CHANNELS2));

		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(9.871, 56.666), module, DollyX::INPUT_MONO_IN1));
		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(9.871, 111.337), module, DollyX::INPUT_MONO_IN2));

		addInput(createInputCentered<BananutBlack>(millimetersToPixelsVec(29.945, 36.856), module, DollyX::INPUT_CHANNELS1_CV));
		addInput(createInputCentered<BananutBlack>(millimetersToPixelsVec(29.945, 91.526), module, DollyX::INPUT_CHANNELS2_CV));

		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(30.769, 56.666), module, DollyX::OUTPUT_POLYOUT_1));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(30.769, 111.337), module, DollyX::OUTPUT_POLYOUT_2));

		FramebufferWidget* dollyFrameBuffer = new FramebufferWidget();
		addChild(dollyFrameBuffer);

		SanguineLedNumberDisplay* displayCloner1 = new SanguineLedNumberDisplay(2);
		displayCloner1->box.pos = millimetersToPixelsVec(6.475, 17.493);
		displayCloner1->module = module;
		dollyFrameBuffer->addChild(displayCloner1);

		if (module)
			displayCloner1->values.numberValue = (&module->cloneCounts[0]);

		SanguineLedNumberDisplay* displayCloner2 = new SanguineLedNumberDisplay(2);
		displayCloner2->box.pos = millimetersToPixelsVec(6.475, 72.164);
		displayCloner2->module = module;
		dollyFrameBuffer->addChild(displayCloner2);

		if (module)
			displayCloner2->values.numberValue = (&module->cloneCounts[1]);

		SanguineShapedLight* amalgamLight1 = new SanguineShapedLight();
		amalgamLight1->box.pos = millimetersToPixelsVec(7.337, 33.237);
		amalgamLight1->module = module;
		amalgamLight1->setSvg(Svg::load(asset::plugin(pluginInstance, "res/amalgam_light.svg")));
		addChild(amalgamLight1);

		SanguineShapedLight* amalgamLight2 = new SanguineShapedLight();
		amalgamLight2->box.pos = millimetersToPixelsVec(7.337, 87.908);
		amalgamLight2->module = module;
		amalgamLight2->setSvg(Svg::load(asset::plugin(pluginInstance, "res/amalgam_light.svg")));
		addChild(amalgamLight2);


		SanguineMonoInputLight* inMonoLight1 = new SanguineMonoInputLight();
		inMonoLight1->box.pos = Vec(19.42, 141.0);
		inMonoLight1->module = module;
		addChild(inMonoLight1);

		SanguineMonoInputLight* inMonoLight2 = new SanguineMonoInputLight();
		inMonoLight2->box.pos = Vec(19.42, 302.67);
		inMonoLight2->module = module;
		addChild(inMonoLight2);

		SanguinePolyOutputLight* outPolyLight1 = new SanguinePolyOutputLight();
		outPolyLight1->box.pos = millimetersToPixelsVec(27.475, 47.678);
		outPolyLight1->module = module;
		addChild(outPolyLight1);

		SanguinePolyOutputLight* outPolyLight2 = new SanguinePolyOutputLight();
		outPolyLight2->box.pos = millimetersToPixelsVec(27.475, 102.349);
		outPolyLight2->module = module;
		addChild(outPolyLight2);
	}
};

Model* modelDollyX = createModel<DollyX, DollyXWidget>("Sanguine-Monsters-Dolly-X");