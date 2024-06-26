#include "plugin.hpp"
#include "sanguinecomponents.hpp"

struct GradientColors {
	NVGcolor innerColor;
	NVGcolor outerColor;
};

static const std::vector<GradientColors> moonColors{
	{ nvgRGB(247, 187, 187), nvgRGB(223, 33, 33) },
	{ nvgRGB(217, 217, 217), nvgRGB(128, 128, 128) },
	{ nvgRGB(187, 214, 247), nvgRGB(22, 117, 234) }
};

static const std::vector<std::string> dungeonModeLabels{
	"SH ",
	"TH",
	"HT"
};

struct SlewFilter {
	float value = 0.f;

	float process(float in, float slew) {
		value += math::clamp(in - value, -slew, slew);
		return value;
	}
	float getValue() {
		return value;
	}
};

struct Dungeon : Module {

	enum ParamIds {
		PARAM_MODE,
		PARAM_TRIGGER,
		PARAM_SLEW,
		PARAMS_COUNT
	};

	enum InputIds {
		INPUT_CLOCK,
		INPUT_VOLTAGE,
		INPUT_SLEW,
		INPUTS_COUNT
	};

	enum OutputIds {
		OUTPUT_NOISE,
		OUTPUT_VOLTAGE,
		OUTPUTS_COUNT
	};

	enum LightIds {
		LIGHT_TRIGGER,
		ENUMS(LIGHT_SLEW, 2),
		LIGHTS_COUNT
	};

	struct Engine {
		bool state = false;
		float voltage = 0.f;
		SlewFilter sampleFilter;
	} engine;

	enum ModuleModes {
		MODE_SAMPLE_AND_HOLD,
		MODE_TRACK_AND_HOLD,
		MODE_HOLD_AND_TRACK
	} moduleMode = MODE_SAMPLE_AND_HOLD;

	NVGcolor innerMoon = moonColors[1].innerColor;
	NVGcolor outerMoon = moonColors[1].outerColor;

	const int kClockDividerFrequency = 512;

	float minSlew = std::log2(1e-3f);
	float maxSlew = std::log2(10.f);
	float inVoltage = 0;
	float whiteNoise;

	bool bStoreVoltageInPatch = true;

	std::string modeLabel = dungeonModeLabels[0];

	dsp::ClockDivider clockDivider;

	Dungeon() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);
		configSwitch(PARAM_MODE, 0.f, 2.f, 0.f, "Mode", { "Sample and hold", "Track and hold", "Hold and track" });
		paramQuantities[PARAM_MODE]->snapEnabled = true;

		configButton(PARAM_TRIGGER, "Trigger");

		struct SlewQuantity : ParamQuantity {
			float getDisplayValue() override {
				if (getValue() <= getMinValue())
					return 0.f;
				return ParamQuantity::getDisplayValue();
			}
		};
		configParam<SlewQuantity>(PARAM_SLEW, minSlew, maxSlew, minSlew, "Slew", " ms/V", 2, 1000);
		configInput(INPUT_SLEW, "Slew CV");

		configInput(INPUT_CLOCK, "Clock");
		configInput(INPUT_VOLTAGE, "Voltage");

		configOutput(OUTPUT_NOISE, "Noise");
		configOutput(OUTPUT_VOLTAGE, "Voltage");

		clockDivider.division = kClockDividerFrequency;
	}

	inline void getNewVoltage(bool bHaveWhiteNoise, bool bHaveInVoltage) {
		if (bHaveInVoltage) {
			inVoltage = inputs[INPUT_VOLTAGE].getVoltage();
		}
		else {
			if (!bHaveWhiteNoise) {
				whiteNoise = random::normal();
			}
			inVoltage = whiteNoise;
		}
	}

	void process(const ProcessArgs& args) override {
		float slewParam = params[PARAM_SLEW].getValue();
		bool bGateButton = params[PARAM_TRIGGER].getValue() > 0.f;
		bool bHaveWhiteNoise = outputs[OUTPUT_NOISE].isConnected();
		bool bHaveInVoltage = inputs[INPUT_VOLTAGE].isConnected();

		if (bHaveWhiteNoise) {
			whiteNoise = random::normal();
			if (outputs[OUTPUT_NOISE].isConnected()) {
				outputs[OUTPUT_NOISE].setVoltage(whiteNoise);
			}
		}

		switch (moduleMode)
		{
		case Dungeon::MODE_SAMPLE_AND_HOLD: {
			// Gate trigger/untrigger
			if (!engine.state) {
				if (inputs[INPUT_CLOCK].getVoltage() >= 2.f || bGateButton) {
					// Triggered
					engine.state = true;
					getNewVoltage(bHaveWhiteNoise, bHaveInVoltage);
					engine.voltage = inVoltage;
				}
			}
			else {
				if (inputs[INPUT_CLOCK].getVoltage() <= 0.1f && !bGateButton) {
					// Untriggered
					engine.state = false;
				}
			}

			inVoltage = engine.voltage;
			break;
		}
		case Dungeon::MODE_TRACK_AND_HOLD: {
			getNewVoltage(bHaveWhiteNoise, bHaveInVoltage);

			// Gate trigger/untrigger
			if (!engine.state) {
				if (inputs[INPUT_CLOCK].getVoltage() >= 2.f || bGateButton) {
					// Triggered
					engine.state = true;
				}
			}
			else {
				if (inputs[INPUT_CLOCK].getVoltage() <= 0.1f && !bGateButton) {
					// Untriggered
					engine.state = false;
					// Track and hold
					engine.voltage = inVoltage;
				}
			}
			if (engine.state) {
				inVoltage = engine.voltage;
			}
			break;
		}
		case Dungeon::MODE_HOLD_AND_TRACK: {
			getNewVoltage(bHaveWhiteNoise, bHaveInVoltage);

			// Gate trigger/untrigger
			if (!engine.state) {
				if (inputs[INPUT_CLOCK].getVoltage() >= 2.f || bGateButton) {
					// Triggered
					engine.state = true;
				}
			}
			else {
				if (inputs[INPUT_CLOCK].getVoltage() <= 0.1f && !bGateButton) {
					// Untriggered
					engine.state = false;
					// Track and hold
					engine.voltage = inVoltage;
				}
			}
			if (!engine.state) {
				inVoltage = engine.voltage;
			}
			break;
		}
		}

		if (outputs[OUTPUT_VOLTAGE].isConnected()) {
			// Slider bottom means infinite slew
			if (slewParam <= std::log2(1e-3f)) {
				slewParam = -INFINITY;
			}

			// Slew rate in V/s
			float slew = INFINITY;
			if (std::isfinite(slewParam)) {
				float slewPitch = slewParam + inputs[INPUT_SLEW].getVoltage();
				slew = dsp::exp2_taylor5(-slewPitch + 30.f) / std::exp2(30.f);
			}
			float slewDelta = slew * args.sampleTime;

			outputs[OUTPUT_VOLTAGE].setVoltage(engine.sampleFilter.process(inVoltage, slewDelta));
		}

		lights[LIGHT_TRIGGER].setBrightnessSmooth(engine.state, args.sampleTime);

		if (clockDivider.process()) {
			moduleMode = ModuleModes(params[PARAM_MODE].getValue());
			modeLabel = dungeonModeLabels[moduleMode];

			const float sampleTime = args.sampleTime * kClockDividerFrequency;

			if (!inputs[INPUT_SLEW].isConnected()) {
				lights[LIGHT_SLEW + 0].setBrightnessSmooth(0.f, sampleTime);
				lights[LIGHT_SLEW + 1].setBrightnessSmooth(math::rescale(params[PARAM_SLEW].getValue(), minSlew, maxSlew, 0.f, 1.f), sampleTime);
			}
			else {
				lights[LIGHT_SLEW + 0].setBrightnessSmooth(math::rescale(inputs[INPUT_SLEW].getVoltage(), 0.f, 5.f, 0.f, 1.f), sampleTime);
				lights[LIGHT_SLEW + 1].setBrightnessSmooth(math::rescale(-inputs[INPUT_SLEW].getVoltage(), 0.f, 5.f, 0.f, 1.f), sampleTime);
			}

			if (inVoltage >= 1.f) {
				outerMoon = moonColors[2].outerColor;
				innerMoon.r = math::rescale(inVoltage, 1.f, 5.f, moonColors[2].outerColor.r, moonColors[2].innerColor.r);
				innerMoon.g = math::rescale(inVoltage, 1.f, 5.f, moonColors[2].outerColor.g, moonColors[2].innerColor.g);
				innerMoon.b = math::rescale(inVoltage, 1.f, 5.f, moonColors[2].outerColor.b, moonColors[2].innerColor.b);
			}
			else if (inVoltage >= -0.99f && inVoltage <= 99.f) {
				outerMoon = moonColors[1].outerColor;
				innerMoon.r = math::rescale(inVoltage, -0.99f, 0.99f, moonColors[1].outerColor.r, moonColors[1].innerColor.r);
				innerMoon.g = math::rescale(inVoltage, -0.99f, 0.99f, moonColors[1].outerColor.g, moonColors[1].innerColor.g);
				innerMoon.b = math::rescale(inVoltage, -0.99f, 0.99f, moonColors[1].outerColor.b, moonColors[1].innerColor.b);
			}
			else {
				outerMoon = moonColors[0].outerColor;
				innerMoon.r = math::rescale(inVoltage, -1.f, -5.f, moonColors[0].outerColor.r, moonColors[0].innerColor.r);
				innerMoon.g = math::rescale(inVoltage, -1.f, -5.f, moonColors[0].outerColor.g, moonColors[0].innerColor.g);
				innerMoon.b = math::rescale(inVoltage, -1.f, -5.f, moonColors[0].outerColor.b, moonColors[0].innerColor.b);
			}
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();

		if (bStoreVoltageInPatch) {
			json_object_set_new(rootJ, "storeVoltageInPatch", json_boolean(bStoreVoltageInPatch));
			json_object_set_new(rootJ, "heldVoltage", json_real(engine.voltage));
		}

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* storeVoltageInPatchJ = json_object_get(rootJ, "storeVoltageInPatch");
		if (storeVoltageInPatchJ) {
			bStoreVoltageInPatch = json_boolean_value(storeVoltageInPatchJ);
			if (bStoreVoltageInPatch) {
				json_t* heldVoltageJ = json_object_get(rootJ, "heldVoltage");
				if (heldVoltageJ) {
					engine.voltage = json_number_value(heldVoltageJ);
					outputs[OUTPUT_VOLTAGE].setChannels(1);
					outputs[OUTPUT_VOLTAGE].setVoltage(engine.voltage);
				}
			}
		}
	}
};

struct DungeonWidget : ModuleWidget {
	DungeonWidget(Dungeon* module) {
		setModule(module);

		SanguinePanel* panel = new SanguinePanel(pluginInstance, "res/backplate_14hp_purple.svg", "res/dungeon.svg");
		setPanel(panel);

		FramebufferWidget* dungeonFrameBuffer = new FramebufferWidget();
		addChild(dungeonFrameBuffer);

		SanguineMultiColoredShapedLight* moonLight = new SanguineMultiColoredShapedLight();
		moonLight->box.pos = mm2px(Vec(3.361, 27.351));
		moonLight->module = module;
		moonLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/dungeon_moon_light.svg")));
		moonLight->svgGradient = Svg::load(asset::plugin(pluginInstance, "res/dungeon_moon_gradient.svg"));
		dungeonFrameBuffer->addChild(moonLight);

		if (module) {
			moonLight->innerColor = &module->innerMoon;
			moonLight->outerColor = &module->outerMoon;
		}

		addParam(createParamCentered<CKD6>(mm2px(Vec(62.386, 12.334)), module, Dungeon::PARAM_TRIGGER));
		addChild(createLightCentered<CKD6Light<RedLight>>(mm2px(Vec(62.386, 12.334)), module, Dungeon::LIGHT_TRIGGER));

		addParam(createParamCentered<BefacoTinyKnobRed>(mm2px(Vec(10.027, 12.334)), module, Dungeon::PARAM_MODE));

		SanguineTinyNumericDisplay* displayMode = new SanguineTinyNumericDisplay(2);
		displayMode->box.pos = mm2px(Vec(29.11, 12.934));
		displayMode->module = module;
		displayMode->displayType = DISPLAY_STRING;
		dungeonFrameBuffer->addChild(displayMode);

		if (module)
			displayMode->values.displayText = &module->modeLabel;

		addParam(createLightParamCentered<VCVLightSlider<GreenRedLight>>(mm2px(Vec(36.76, 73.316)), module, Dungeon::PARAM_SLEW, Dungeon::LIGHT_SLEW));
		addInput(createInputCentered<BananutPurple>(mm2px(Vec(36.76, 95.874)), module, Dungeon::INPUT_SLEW));

		addInput(createInputCentered<BananutGreen>(mm2px(Vec(8.762, 100.733)), module, Dungeon::INPUT_CLOCK));
		addInput(createInputCentered<BananutGreen>(mm2px(Vec(8.762, 116.011)), module, Dungeon::INPUT_VOLTAGE));

		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(62.386, 100.733)), module, Dungeon::OUTPUT_NOISE));
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(62.386, 116.011)), module, Dungeon::OUTPUT_VOLTAGE));

		SanguineShapedLight* clockLight = new SanguineShapedLight();
		clockLight->box.pos = mm2px(Vec(7.078, 91.346));
		clockLight->module = module;
		clockLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/clock_lit_mono.svg")));
		addChild(clockLight);

		SanguineMonoInputLight* inLight = new SanguineMonoInputLight();
		inLight->box.pos = mm2px(Vec(5.468, 106.547));
		inLight->module = module;
		addChild(inLight);

		SanguineShapedLight* noiseLight = new SanguineShapedLight();
		noiseLight->box.pos = mm2px(Vec(59.092, 91.72));
		noiseLight->module = module;
		noiseLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/noise_lit.svg")));
		addChild(noiseLight);

		SanguineMonoOutputLight* outLight = new SanguineMonoOutputLight();
		outLight->box.pos = mm2px(Vec(59.092, 106.247));
		outLight->module = module;
		addChild(outLight);

		SanguineShapedLight* bloodLight = new SanguineShapedLight();
		bloodLight->box.pos = mm2px(Vec(23.899, 105.766));
		bloodLight->module = module;
		bloodLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/blood_glowy.svg")));
		addChild(bloodLight);

		SanguineShapedLight* monstersLight = new SanguineShapedLight();
		monstersLight->box.pos = mm2px(Vec(31.221, 113.733));
		monstersLight->module = module;
		monstersLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/monsters_lit.svg")));
		addChild(monstersLight);
	}

	void appendContextMenu(Menu* menu) override {
		Dungeon* module = dynamic_cast<Dungeon*>(this->module);

		menu->addChild(new MenuSeparator);

		menu->addChild(createCheckMenuItem("Store held voltage in patch", "",
			[=]() {return module->bStoreVoltageInPatch; },
			[=]() {module->bStoreVoltageInPatch = !module->bStoreVoltageInPatch; }));
	}
};

Model* modelDungeon = createModel<Dungeon, DungeonWidget>("Sanguine-Monsters-Dungeon");