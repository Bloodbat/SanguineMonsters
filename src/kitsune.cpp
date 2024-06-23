#include "plugin.hpp"
#include "sanguinecomponents.hpp"

struct Kitsune : Module {

	enum ParamIds {
		PARAM_ATTENUATOR1,
		PARAM_ATTENUATOR2,
		PARAM_ATTENUATOR3,
		PARAM_ATTENUATOR4,
		PARAM_OFFSET1,
		PARAM_OFFSET2,
		PARAM_OFFSET3,
		PARAM_OFFSET4,
		PARAMS_COUNT
	};

	enum InputIds {
		INPUT_VOLTAGE1,
		INPUT_VOLTAGE2,
		INPUT_VOLTAGE3,
		INPUT_VOLTAGE4,
		INPUTS_COUNT
	};

	enum OutputIds {
		OUTPUT_VOLTAGE1,
		OUTPUT_VOLTAGE2,
		OUTPUT_VOLTAGE3,
		OUTPUT_VOLTAGE4,
		OUTPUTS_COUNT
	};

	enum LightIds {
		ENUMS(LIGHT_VOLTAGE1, 3),
		ENUMS(LIGHT_VOLTAGE2, 3),
		ENUMS(LIGHT_VOLTAGE3, 3),
		ENUMS(LIGHT_VOLTAGE4, 3),
		LIGHTS_COUNT
	};

	Kitsune() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		for (int i = 0; i < 4; i++) {
			configParam(PARAM_ATTENUATOR1 + i, -1.f, 1.f, 0.f, string::f("Channel %d gain", i + 1), "%", 0, 100);
			configParam(PARAM_OFFSET1 + i, -10.f, 10.f, 0.f, string::f("Channel %d offset", i + 1), " V");
			configOutput(OUTPUT_VOLTAGE1 + i, string::f("Channel %d", i + 1));
			configInput(INPUT_VOLTAGE1 + i, string::f("Channel %d", i + 1));

			configBypass(INPUT_VOLTAGE1 + i, OUTPUT_VOLTAGE1 + i);
		}
	}

	void process(const ProcessArgs& args) override {
		using simd::float_4;

		float_4 voltages1[4] = {};
		float_4 voltages2[4] = {};
		float_4 voltages3[4] = {};
		float_4 voltages4[4] = {};

		int channelCounts[4];

		for (int i = 0; i < 4; i++) {
			channelCounts[i] = inputs[INPUT_VOLTAGE1 + i].getChannels();
			channelCounts[i] = channelCounts[i] > 0 ? channelCounts[i] : 1;
		}

		for (int channel = 0; channel < channelCounts[0]; channel += 4) {
			voltages1[channel / 4] = clamp(inputs[INPUT_VOLTAGE1].getVoltageSimd<float_4>(channel) * params[PARAM_ATTENUATOR1].getValue() +
				params[PARAM_OFFSET1].getValue(), -10.f, 10.f);
		}

		for (int channel = 0; channel < channelCounts[1]; channel += 4) {
			voltages2[channel / 4] = clamp(inputs[INPUT_VOLTAGE2].getVoltageSimd<float_4>(channel) * params[PARAM_ATTENUATOR2].getValue() +
				params[PARAM_OFFSET2].getValue(), -10.f, 10.f);
		}

		for (int channel = 0; channel < channelCounts[2]; channel += 4) {
			voltages3[channel / 4] = clamp(inputs[INPUT_VOLTAGE3].getVoltageSimd<float_4>(channel) * params[PARAM_ATTENUATOR3].getValue() +
				params[PARAM_OFFSET3].getValue(), -10.f, 10.f);
		}

		for (int channel = 0; channel < channelCounts[3]; channel += 4) {
			voltages4[channel / 4] = clamp(inputs[INPUT_VOLTAGE4].getVoltageSimd<float_4>(channel) * params[PARAM_ATTENUATOR4].getValue() +
				params[PARAM_OFFSET4].getValue(), -10.f, 10.f);
		}

		for (int i = 0; i < 4; i++) {
			outputs[OUTPUT_VOLTAGE1 + i].setChannels(channelCounts[i]);
		}

		for (int channel = 0; channel < channelCounts[0]; channel += 4) {
			outputs[OUTPUT_VOLTAGE1].setVoltageSimd(voltages1[channel / 4], channel);
		}

		for (int channel = 0; channel < channelCounts[1]; channel += 4) {
			outputs[OUTPUT_VOLTAGE2].setVoltageSimd(voltages2[channel / 4], channel);
		}

		for (int channel = 0; channel < channelCounts[2]; channel += 4) {
			outputs[OUTPUT_VOLTAGE3].setVoltageSimd(voltages3[channel / 4], channel);
		}

		for (int channel = 0; channel < channelCounts[3]; channel += 4) {
			outputs[OUTPUT_VOLTAGE4].setVoltageSimd(voltages4[channel / 4], channel);
		}

		int currentLight;

		for (int i = 0; i < 4; i++) {
			float lightValue = outputs[OUTPUT_VOLTAGE1 + i].getVoltageSum() / channelCounts[i];

			currentLight = LIGHT_VOLTAGE1 + i * 3;

			if (channelCounts[i] == 1) {
				lights[currentLight + 0].setBrightnessSmooth(math::rescale(-lightValue, 0.f, 5.f, 0.f, 1.f), args.sampleTime);
				lights[currentLight + 1].setBrightnessSmooth(math::rescale(lightValue, 0.f, 5.f, 0.f, 1.f), args.sampleTime);
				lights[currentLight + 2].setBrightnessSmooth(0.0f, args.sampleTime);
			}
			else {
				float redValue = math::rescale(-lightValue, 0.f, 10.f, 0.f, 1.f);
				float greenValue = math::rescale(lightValue, 0.f, 10.f, 0.f, 1.f);
				lights[currentLight + 0].setBrightnessSmooth(redValue, args.sampleTime);
				lights[currentLight + 1].setBrightnessSmooth(greenValue, args.sampleTime);
				lights[currentLight + 2].setBrightnessSmooth(lightValue < 0 ? redValue : greenValue, args.sampleTime);
			}
		}
	}
};

struct KitsuneWidget : ModuleWidget {
	KitsuneWidget(Kitsune* module) {
		setModule(module);

		SanguinePanel* panel = new SanguinePanel(pluginInstance, "res/backplate_10hp_purple.svg", "res/kitsune.svg");
		setPanel(panel);

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Section 1
		addParam(createParamCentered<Davies1900hBlackKnob>(mm2px(Vec(12.7, 11.198)), module, Kitsune::PARAM_ATTENUATOR1));
		addParam(createParamCentered<Davies1900hRedKnob>(mm2px(Vec(12.7, 30.085)), module, Kitsune::PARAM_OFFSET1));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(mm2px(Vec(12.701, 41.9)), module, Kitsune::LIGHT_VOLTAGE1));

		SanguinePolyInputLight* lightInput1 = new SanguinePolyInputLight;
		lightInput1->box.pos = mm2px(Vec(2.194, 46.335));
		lightInput1->module = module;
		addChild(lightInput1);

		addInput(createInputCentered<BananutGreen>(mm2px(Vec(5.488, 55.888)), module, Kitsune::INPUT_VOLTAGE1));

		SanguinePolyOutputLight* lightOutput1 = new SanguinePolyOutputLight;
		lightOutput1->box.pos = mm2px(Vec(16.305, 45.935));
		lightOutput1->module = module;
		addChild(lightOutput1);

		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(19.599, 55.888)), module, Kitsune::OUTPUT_VOLTAGE1));

		// Section 2
		addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(38.099, 11.198)), module, Kitsune::PARAM_ATTENUATOR2));
		addParam(createParamCentered<Davies1900hRedKnob>(mm2px(Vec(38.099, 30.085)), module, Kitsune::PARAM_OFFSET2));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(mm2px(Vec(38.1, 41.9)), module, Kitsune::LIGHT_VOLTAGE2));

		SanguinePolyInputLight* lightInput2 = new SanguinePolyInputLight;
		lightInput2->box.pos = mm2px(Vec(2.194, 108.869));
		lightInput2->module = module;
		addChild(lightInput2);

		addInput(createInputCentered<BananutGreen>(mm2px(Vec(30.887, 55.888)), module, Kitsune::INPUT_VOLTAGE2));


		SanguinePolyOutputLight* lightOutput2 = new SanguinePolyOutputLight;
		lightOutput2->box.pos = mm2px(Vec(16.305, 108.469));
		lightOutput2->module = module;
		addChild(lightOutput2);

		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(44.998, 55.888)), module, Kitsune::OUTPUT_VOLTAGE2));

		// Section 3
		addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(12.7, 73.732)), module, Kitsune::PARAM_ATTENUATOR3));
		addParam(createParamCentered<Davies1900hRedKnob>(mm2px(Vec(12.7, 92.618)), module, Kitsune::PARAM_OFFSET3));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(mm2px(Vec(12.701, 104.433)), module, Kitsune::LIGHT_VOLTAGE3));

		SanguinePolyInputLight* lightInput3 = new SanguinePolyInputLight;
		lightInput3->box.pos = mm2px(Vec(27.593, 46.335));
		lightInput3->module = module;
		addChild(lightInput3);

		addInput(createInputCentered<BananutGreen>(mm2px(Vec(5.488, 118.422)), module, Kitsune::INPUT_VOLTAGE3));

		SanguinePolyOutputLight* lightOutput3 = new SanguinePolyOutputLight;
		lightOutput3->box.pos = mm2px(Vec(41.704, 45.935));
		lightOutput3->module = module;
		addChild(lightOutput3);

		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(19.599, 118.422)), module, Kitsune::OUTPUT_VOLTAGE3));

		// Section 4
		addParam(createParamCentered<Davies1900hBlackKnob>(mm2px(Vec(38.099, 73.732)), module, Kitsune::PARAM_ATTENUATOR4));
		addParam(createParamCentered<Davies1900hRedKnob>(mm2px(Vec(38.099, 92.618)), module, Kitsune::PARAM_OFFSET4));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(mm2px(Vec(38.1, 104.433)), module, Kitsune::LIGHT_VOLTAGE4));

		SanguinePolyInputLight* lightInput4 = new SanguinePolyInputLight;
		lightInput4->box.pos = mm2px(Vec(27.593, 108.869));
		lightInput4->module = module;
		addChild(lightInput4);

		addInput(createInputCentered<BananutGreen>(mm2px(Vec(30.887, 118.422)), module, Kitsune::INPUT_VOLTAGE4));

		SanguinePolyOutputLight* lightOutput4 = new SanguinePolyOutputLight;
		lightOutput4->box.pos = mm2px(Vec(41.704, 108.469));
		lightOutput4->module = module;
		addChild(lightOutput4);

		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(44.998, 118.422)), module, Kitsune::OUTPUT_VOLTAGE4));
	}
};

Model* modelKitsune = createModel<Kitsune, KitsuneWidget>("Sanguine-Kitsune");