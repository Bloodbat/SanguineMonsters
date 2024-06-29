#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"
#include "sanguinedsp.hpp"

const float SaturatorFloat::limit = 9.5f;

struct Alchemist : Module {

	enum ParamIds {
		ENUMS(PARAM_GAIN, 16),
		PARAM_MIX,
		PARAMS_COUNT
	};

	enum InputIds {
		INPUT_POLYPHONIC,
		ENUMS(INPUT_GAIN_CV, 16),
		INPUT_MIX_CV,
		INPUTS_COUNT
	};

	enum OutputIds {
		OUTPUT_MONO_MIX,
		OUTPUT_POLYPHONIC_MIX,
		OUTPUTS_COUNT
	};

	enum LightIds {
		ENUMS(LIGHT_GAIN, 2 * 16),
		ENUMS(LIGHT_VU, 4),
		LIGHTS_COUNT
	};

	const int kLightFrequency = 512;

	dsp::ClockDivider lightsDivider;
	dsp::VuMeter2 vuMeterMix;
	dsp::VuMeter2 vuMetersGain[16];

	SaturatorFloat saturatorFloat;

	Alchemist() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		for (int i = 0; i < 16; i++) {
			configParam(PARAM_GAIN + i, 0.f, 1.f, 0.f, string::f("Channel %d gain", i + 1), "%", 0.f, 100.f);
			configInput(INPUT_GAIN_CV + i, string::f("Channel %d gain CV", i + 1));
		}

		configParam(PARAM_MIX, 0.f, 1.f, 1.f, "Master", "%", 0.f, 100.f);
		configInput(INPUT_MIX_CV, "Master CV");

		configInput(INPUT_POLYPHONIC, "Polyphonic");

		configOutput(OUTPUT_POLYPHONIC_MIX, "Polyphonic mix");
		configOutput(OUTPUT_MONO_MIX, "Monophonic mix");

		configBypass(INPUT_POLYPHONIC, OUTPUT_POLYPHONIC_MIX);

		lightsDivider.setDivision(kLightFrequency);
	}

	void process(const ProcessArgs& args) override {
		float monoMix = 0.f;
		float outVoltages[16] = {};
		float masterOutVoltages[16] = {};
		float mixModulation = params[PARAM_MIX].getValue() + clamp(inputs[INPUT_MIX_CV].getVoltage(), -5.f, 5.f) / 5.f;

		int channelCount = inputs[INPUT_POLYPHONIC].getChannels();
		bool bIsLightsTurn = lightsDivider.process();

		for (int i = 0; i < channelCount; i++) {
			outVoltages[i] = inputs[INPUT_POLYPHONIC].getPolyVoltage(i);

			outVoltages[i] = outVoltages[i] * (params[PARAM_GAIN + i].getValue() + clamp(inputs[INPUT_GAIN_CV + i].getVoltage(), -5.f, 5.f) / 5.f);

			if (outVoltages[i] <= -9.5f || outVoltages[i] >= 9.5f) {
				outVoltages[i] = saturatorFloat.next(outVoltages[i]);
			}

			monoMix += outVoltages[i];

			if (outputs[OUTPUT_POLYPHONIC_MIX].isConnected()) {
				masterOutVoltages[i] = outVoltages[i] * mixModulation;
			}
		}

		monoMix = monoMix * mixModulation;

		if (monoMix <= -9.5f || monoMix >= 9.5f) {
			monoMix = saturatorFloat.next(monoMix);
		}

		if (outputs[OUTPUT_MONO_MIX].isConnected()) {
			outputs[OUTPUT_MONO_MIX].setVoltage(monoMix);
		}

		if (outputs[OUTPUT_POLYPHONIC_MIX].isConnected()) {
			outputs[OUTPUT_POLYPHONIC_MIX].setChannels(channelCount);
			outputs[OUTPUT_POLYPHONIC_MIX].writeVoltages(masterOutVoltages);
		}

		if (bIsLightsTurn) {
			const float sampleTime = kLightFrequency * args.sampleTime;

			for (int i = 0; i < PORT_MAX_CHANNELS; i++) {
				vuMetersGain[i].process(sampleTime, outVoltages[i] / 10.f);

				int currentLight = LIGHT_GAIN + i * 2;
				float redValue = vuMetersGain[i].getBrightness(-3.f, 0.f);
				float yellowValue = vuMetersGain[i].getBrightness(-6.f, -3.f);
				float greenValue = vuMetersGain[i].getBrightness(-38.f, -6.f);
				bool isRed = redValue > 0;
				bool isYellow = yellowValue > 0;

				if (isRed) {
					lights[currentLight + 0].setBrightness(0.f);
					lights[currentLight + 1].setBrightness(redValue);
				}
				else if (isYellow) {
					lights[currentLight + 0].setBrightness(yellowValue);
					lights[currentLight + 1].setBrightness(yellowValue);
				}
				else {
					lights[currentLight + 0].setBrightness(greenValue);
					lights[currentLight + 1].setBrightness(0.f);
				}
			}
			vuMeterMix.process(sampleTime, monoMix / 10);
			lights[LIGHT_VU].setBrightness(vuMeterMix.getBrightness(-38.f, -19.f));
			lights[LIGHT_VU + 1].setBrightness(vuMeterMix.getBrightness(-19.f, -6.f));
			lights[LIGHT_VU + 2].setBrightness(vuMeterMix.getBrightness(-6.f, -3.f));
			lights[LIGHT_VU + 3].setBrightness(vuMeterMix.getBrightness(-3.f, 0.f));
		}
	}
};

struct AlchemistWidget : ModuleWidget {
	AlchemistWidget(Alchemist* module) {
		setModule(module);

		SanguinePanel* panel = new SanguinePanel(pluginInstance, "res/backplate_23hp_purple.svg", "res/alchemist.svg");
		setPanel(panel);

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		const float sliderBaseX = 11.002;
		const float cvInputBaseX = 11.003;
		const float deltaX = 13.55;

		float currentSliderX = sliderBaseX;
		float currentCVInputX = cvInputBaseX;

		for (int i = 0; i < 8; i++) {
			addParam(createLightParamCentered<VCVLightSlider<GreenRedLight>>(millimetersToPixelsVec(currentSliderX, 31.545),
				module, Alchemist::PARAM_GAIN + i, Alchemist::LIGHT_GAIN + i * 2));
			addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(currentCVInputX, 51.911), module, Alchemist::INPUT_GAIN_CV + i));
			currentSliderX += deltaX;
			currentCVInputX += deltaX;
		}

		const int offset = 8;

		currentSliderX = sliderBaseX;
		currentCVInputX = cvInputBaseX;

		for (int i = 0; i < 8; i++) {
			addParam(createLightParamCentered<VCVLightSlider<GreenRedLight>>(millimetersToPixelsVec(currentSliderX, 75.078),
				module, Alchemist::PARAM_GAIN + i + offset, (Alchemist::LIGHT_GAIN + i + offset) * 2));
			addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(currentCVInputX, 95.445), module, Alchemist::INPUT_GAIN_CV + i + offset));
			currentSliderX += deltaX;
			currentCVInputX += deltaX;
		}

		SanguinePolyInputLight* inLight = createWidgetCentered<SanguinePolyInputLight>(millimetersToPixelsVec(7.876, 110.216));
		inLight->module = module;
		addChild(inLight);

		SanguineShapedLight* bloodLight = new SanguineShapedLight();
		bloodLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/blood_glowy.svg")));
		bloodLight->module = module;
		bloodLight->box.pos = centerWidgetInMillimeters(bloodLight, 22.43f, 110.15f);
		addChild(bloodLight);

		SanguineShapedLight* monstersLight = new SanguineShapedLight();
		monstersLight->module = module;
		monstersLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/monsters_lit.svg")));
		monstersLight->box.pos = centerWidgetInMillimeters(monstersLight, 35.563f, 117.106f);
		addChild(monstersLight);

		SanguineShapedLight* sumLight = new SanguineShapedLight();
		sumLight->module = module;
		sumLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/sum_light_on.svg")));
		sumLight->box.pos = centerWidgetInMillimeters(sumLight, 54.673, 108.602);
		addChild(sumLight);

		addParam(createParamCentered<BefacoTinyKnobRed>(millimetersToPixelsVec(66.657, 108.602), module, Alchemist::PARAM_MIX));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(79.964, 108.602), module, Alchemist::INPUT_MIX_CV));

		SanguinePolyOutputLight* outPolyLight = createWidgetCentered<SanguinePolyOutputLight>(millimetersToPixelsVec(95.442, 109.673));
		outPolyLight->module = module;
		addChild(outPolyLight);

		SanguineMonoOutputLight* outMonoLight = createWidgetCentered<SanguineMonoOutputLight>(millimetersToPixelsVec(108.963, 109.673));
		outMonoLight->module = module;
		addChild(outMonoLight);

		addChild(createLightCentered<LargeLight<GreenLight>>(millimetersToPixelsVec(53.772, 118.436), module, Alchemist::LIGHT_VU));
		addChild(createLightCentered<LargeLight<GreenLight>>(millimetersToPixelsVec(63.184, 118.436), module, Alchemist::LIGHT_VU + 1));
		addChild(createLightCentered<LargeLight<YellowLight>>(millimetersToPixelsVec(72.596, 118.436), module, Alchemist::LIGHT_VU + 2));
		addChild(createLightCentered<LargeLight<RedLight>>(millimetersToPixelsVec(82.007, 118.436), module, Alchemist::LIGHT_VU + 3));

		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(7.876, 117.569), module, Alchemist::INPUT_POLYPHONIC));

		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(95.442, 117.569), module, Alchemist::OUTPUT_POLYPHONIC_MIX));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(108.963, 117.569), module, Alchemist::OUTPUT_MONO_MIX));
	}
};

Model* modelAlchemist = createModel<Alchemist, AlchemistWidget>("Sanguine-Alchemist");