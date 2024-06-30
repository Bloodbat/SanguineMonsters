#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"

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

		for (int i = 0; i < 4; i++) {
			int channelCount = inputs[INPUT_VOLTAGE1 + i].getChannels() > 0 ? inputs[INPUT_VOLTAGE1 + i].getChannels() : 1;

			for (int channel = 0; channel < channelCount; channel += 4) {
				float_4 voltages = {};

				voltages = clamp(inputs[INPUT_VOLTAGE1 + i].getVoltageSimd<float_4>(channel) * params[PARAM_ATTENUATOR1 + i].getValue() +
					params[PARAM_OFFSET1 + i].getValue(), -10.f, 10.f);

				outputs[OUTPUT_VOLTAGE1 + i].setChannels(channelCount);
				outputs[OUTPUT_VOLTAGE1 + i].setVoltageSimd(voltages, channel);
			}

			int currentLight;

			float lightValue = outputs[OUTPUT_VOLTAGE1 + i].getVoltageSum() / channelCount;

			currentLight = LIGHT_VOLTAGE1 + i * 3;

			if (channelCount == 1) {
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

		SanguinePanel* panel = new SanguinePanel("res/backplate_10hp_purple.svg", "res/kitsune.svg");
		setPanel(panel);

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Section 1
		addParam(createParamCentered<Davies1900hBlackKnob>(millimetersToPixelsVec(12.7, 11.198), module, Kitsune::PARAM_ATTENUATOR1));
		addParam(createParamCentered<Davies1900hRedKnob>(millimetersToPixelsVec(12.7, 30.085), module, Kitsune::PARAM_OFFSET1));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(12.701, 41.9), module, Kitsune::LIGHT_VOLTAGE1));

		SanguinePolyInputLight* lightInput1 = new SanguinePolyInputLight(module, 5.488, 48.4);		
		addChild(lightInput1);

		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(5.488, 55.888), module, Kitsune::INPUT_VOLTAGE1));

		SanguinePolyOutputLight* lightOutput1 = new SanguinePolyOutputLight(module, 19.599, 47.999);		
		addChild(lightOutput1);

		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(19.599, 55.888), module, Kitsune::OUTPUT_VOLTAGE1));

		// Section 2
		addParam(createParamCentered<Davies1900hWhiteKnob>(millimetersToPixelsVec(38.099, 11.198), module, Kitsune::PARAM_ATTENUATOR2));
		addParam(createParamCentered<Davies1900hRedKnob>(millimetersToPixelsVec(38.099, 30.085), module, Kitsune::PARAM_OFFSET2));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(38.1, 41.9), module, Kitsune::LIGHT_VOLTAGE2));

		SanguinePolyInputLight* lightInput2 = new SanguinePolyInputLight(module, 30.887, 48.4);
		addChild(lightInput2);

		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(30.887, 55.888), module, Kitsune::INPUT_VOLTAGE2));


		SanguinePolyOutputLight* lightOutput2 = new SanguinePolyOutputLight(module, 44.998, 47.999);
		addChild(lightOutput2);

		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(44.998, 55.888), module, Kitsune::OUTPUT_VOLTAGE2));

		// Section 3
		addParam(createParamCentered<Davies1900hWhiteKnob>(millimetersToPixelsVec(12.7, 73.732), module, Kitsune::PARAM_ATTENUATOR3));
		addParam(createParamCentered<Davies1900hRedKnob>(millimetersToPixelsVec(12.7, 92.618), module, Kitsune::PARAM_OFFSET3));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(12.701, 104.433), module, Kitsune::LIGHT_VOLTAGE3));

		SanguinePolyInputLight* lightInput3 = new SanguinePolyInputLight(module, 5.488, 110.933);
		addChild(lightInput3);

		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(5.488, 118.422), module, Kitsune::INPUT_VOLTAGE3));

		SanguinePolyOutputLight* lightOutput3 = new SanguinePolyOutputLight(module, 19.599, 110.533);
		addChild(lightOutput3);

		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(19.599, 118.422), module, Kitsune::OUTPUT_VOLTAGE3));

		// Section 4
		addParam(createParamCentered<Davies1900hBlackKnob>(millimetersToPixelsVec(38.099, 73.732), module, Kitsune::PARAM_ATTENUATOR4));
		addParam(createParamCentered<Davies1900hRedKnob>(millimetersToPixelsVec(38.099, 92.618), module, Kitsune::PARAM_OFFSET4));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(38.1, 104.433), module, Kitsune::LIGHT_VOLTAGE4));

		SanguinePolyInputLight* lightInput4 = new SanguinePolyInputLight(module, 30.887, 110.933);
		addChild(lightInput4);

		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(30.887, 118.422), module, Kitsune::INPUT_VOLTAGE4));

		SanguinePolyOutputLight* lightOutput4 = new SanguinePolyOutputLight(module, 44.998, 110.533);
		addChild(lightOutput4);

		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(44.998, 118.422), module, Kitsune::OUTPUT_VOLTAGE4));
	}
};

Model* modelKitsune = createModel<Kitsune, KitsuneWidget>("Sanguine-Kitsune");