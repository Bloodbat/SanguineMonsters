#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"
#include "sanguinerandom.hpp"
#include "sanguinejson.hpp"

#include "dungeon.hpp"

struct Dungeon : SanguineModule {

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
#ifdef METAMODULE
		ENUMS(LIGHT_VOLTAGE, 3),
#endif
		LIGHTS_COUNT
	};

	struct Engine {
		bool isTriggered = false;
		float voltage = 0.f;
		SlewFilter sampleFilter;
	} engine;

	enum ModuleModes {
		MODE_SAMPLE_AND_HOLD,
		MODE_TRACK_AND_HOLD,
		MODE_HOLD_AND_TRACK
	} moduleMode = MODE_SAMPLE_AND_HOLD;

#ifndef METAMODULE
	NVGcolor innerMoon = dungeon::moonColors[1].innerColor;
	NVGcolor outerMoon = dungeon::moonColors[1].outerColor;
#endif

	static const int kClockDividerFrequency = 512;

	static constexpr float kMinSlew = -9.965784285; // std::log2(1e-3f)
	static constexpr float kMaxSlew = 3.321928095; // std::log2(10.f)
	float inVoltage = 0.f;
	float whiteNoise = 0.f;

	bool bStoreVoltageInPatch = true;
	bool bOutNoiseConnected = false;
	bool bOutVoltageConnected = false;
	bool bInVoltageConnected = false;
	bool bInSlewConnected = false;

#ifndef METAMODULE
	HaloTypes haloType = HALO_CIRCULAR;
#endif

	std::string modeLabel = dungeon::modeLabels[0];

	dsp::ClockDivider clockDivider;

	Dungeon() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);
		configSwitch(PARAM_MODE, 0.f, 2.f, 0.f, "Mode", { "Sample and hold", "Track and hold", "Hold and track" });

		configButton(PARAM_TRIGGER, "Trigger");

		struct SlewQuantity : ParamQuantity {
			float getDisplayValue() override {
				if (getValue() <= getMinValue())
					return 0.f;
				return ParamQuantity::getDisplayValue();
			}
		};
		configParam<SlewQuantity>(PARAM_SLEW, kMinSlew, kMaxSlew, kMinSlew, "Slew", " ms/V", 2, 1000);
		configInput(INPUT_SLEW, "Slew CV");

		configInput(INPUT_CLOCK, "Clock");
		configInput(INPUT_VOLTAGE, "Voltage");

		configOutput(OUTPUT_NOISE, "Noise");
		configOutput(OUTPUT_VOLTAGE, "Voltage");

		clockDivider.division = kClockDividerFrequency;
	}

	void process(const ProcessArgs& args) override {
		float slewParam = params[PARAM_SLEW].getValue();
		bool bGateButton = params[PARAM_TRIGGER].getValue() > 0.f;

		if (bOutNoiseConnected) {
			whiteNoise = 2.f * sanguineRandom::normal();
			outputs[OUTPUT_NOISE].setVoltage(whiteNoise);
		}

		switch (moduleMode) {
		case Dungeon::MODE_SAMPLE_AND_HOLD: {
			// Gate trigger/untrigger
			if (!engine.isTriggered) {
				if (inputs[INPUT_CLOCK].getVoltage() >= 2.f || bGateButton) {
					// Triggered
					engine.isTriggered = true;
					getNewVoltage();
					engine.voltage = inVoltage;
				}
			} else {
				if (inputs[INPUT_CLOCK].getVoltage() <= 0.1f && !bGateButton) {
					// Untriggered
					engine.isTriggered = false;
				}
			}

			inVoltage = engine.voltage;
			break;
		}
		case Dungeon::MODE_TRACK_AND_HOLD: {
			getNewVoltage();

			// Gate trigger/untrigger
			if (!engine.isTriggered) {
				if (inputs[INPUT_CLOCK].getVoltage() >= 2.f || bGateButton) {
					// Triggered
					engine.isTriggered = true;
				}
			} else {
				if (inputs[INPUT_CLOCK].getVoltage() <= 0.1f && !bGateButton) {
					// Untriggered
					engine.isTriggered = false;
					// Track and hold
					engine.voltage = inVoltage;
				}
			}
			if (engine.isTriggered) {
				inVoltage = engine.voltage;
			}
			break;
		}
		case Dungeon::MODE_HOLD_AND_TRACK: {
			getNewVoltage();

			// Gate trigger/untrigger
			if (!engine.isTriggered) {
				if (inputs[INPUT_CLOCK].getVoltage() >= 2.f || bGateButton) {
					// Triggered
					engine.isTriggered = true;
				}
			} else {
				if (inputs[INPUT_CLOCK].getVoltage() <= 0.1f && !bGateButton) {
					// Untriggered
					engine.isTriggered = false;
					// Track and hold
					engine.voltage = inVoltage;
				}
			}
			if (!engine.isTriggered) {
				inVoltage = engine.voltage;
			}
			break;
		}
		}

		if (bOutVoltageConnected) {
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

		lights[LIGHT_TRIGGER].setBrightnessSmooth(engine.isTriggered * kSanguineButtonLightValue, args.sampleTime);

		if (clockDivider.process()) {
			moduleMode = ModuleModes(params[PARAM_MODE].getValue());
			modeLabel = dungeon::modeLabels[moduleMode];

			const float sampleTime = args.sampleTime * kClockDividerFrequency;

			if (!bInSlewConnected) {
				lights[LIGHT_SLEW].setBrightnessSmooth(0.f, sampleTime);
				lights[LIGHT_SLEW + 1].setBrightnessSmooth(math::rescale(params[PARAM_SLEW].getValue(), kMinSlew, kMaxSlew, 0.f, 1.f), sampleTime);
			} else {
				float rescaledLight = math::rescale(inputs[INPUT_SLEW].getVoltage(), 0.f, 5.f, 0.f, 1.f);
				lights[LIGHT_SLEW].setBrightnessSmooth(rescaledLight, sampleTime);
				lights[LIGHT_SLEW + 1].setBrightnessSmooth(-rescaledLight, sampleTime);
			}

#ifndef METAMODULE
			if (inVoltage >= 1.f) {
				outerMoon = dungeon::moonColors[2].outerColor;
				innerMoon.r = math::rescale(inVoltage, 1.f, 5.f, dungeon::moonColors[2].outerColor.r,
					dungeon::moonColors[2].innerColor.r);
				innerMoon.g = math::rescale(inVoltage, 1.f, 5.f, dungeon::moonColors[2].outerColor.g,
					dungeon::moonColors[2].innerColor.g);
				innerMoon.b = math::rescale(inVoltage, 1.f, 5.f, dungeon::moonColors[2].outerColor.b,
					dungeon::moonColors[2].innerColor.b);
			} else if (inVoltage >= -0.99f && inVoltage <= 0.99f) {
				outerMoon = dungeon::moonColors[1].outerColor;
				innerMoon.r = math::rescale(inVoltage, -0.99f, 0.99f, dungeon::moonColors[1].outerColor.r,
					dungeon::moonColors[1].innerColor.r);
				innerMoon.g = math::rescale(inVoltage, -0.99f, 0.99f, dungeon::moonColors[1].outerColor.g,
					dungeon::moonColors[1].innerColor.g);
				innerMoon.b = math::rescale(inVoltage, -0.99f, 0.99f, dungeon::moonColors[1].outerColor.b,
					dungeon::moonColors[1].innerColor.b);
			} else {
				outerMoon = dungeon::moonColors[0].outerColor;
				innerMoon.r = math::rescale(inVoltage, -1.f, -5.f, dungeon::moonColors[0].outerColor.r,
					dungeon::moonColors[0].innerColor.r);
				innerMoon.g = math::rescale(inVoltage, -1.f, -5.f, dungeon::moonColors[0].outerColor.g,
					dungeon::moonColors[0].innerColor.g);
				innerMoon.b = math::rescale(inVoltage, -1.f, -5.f, dungeon::moonColors[0].outerColor.b,
					dungeon::moonColors[0].innerColor.b);
			}
#else
			if (inVoltage >= 1.f) {
				lights[LIGHT_VOLTAGE].setBrightness(0.f);
				lights[LIGHT_VOLTAGE + 1].setBrightness(0.f);
				lights[LIGHT_VOLTAGE + 2].setBrightness(rescale(inVoltage, 1.f, 10.f, 0.f, 1.f));
			} else if (inVoltage >= -0.99f && inVoltage <= 0.99f) {
				float rescaledVoltage = rescale(inVoltage, -0.99f, 0.99f, 0.f, 1.f);
				lights[LIGHT_VOLTAGE].setBrightness(rescaledVoltage);
				lights[LIGHT_VOLTAGE + 1].setBrightness(rescaledVoltage);
				lights[LIGHT_VOLTAGE + 2].setBrightness(rescaledVoltage);
			} else {
				lights[LIGHT_VOLTAGE].setBrightness(rescale(inVoltage, -10.f, -1.f, 0.f, 1.f));
				lights[LIGHT_VOLTAGE + 1].setBrightness(0.f);
				lights[LIGHT_VOLTAGE + 2].setBrightness(0.f);
			}
#endif
		}
	}

	inline void getNewVoltage() {
		if (bInVoltageConnected) {
			inVoltage = inputs[INPUT_VOLTAGE].getVoltage();
		} else {
			if (!bOutNoiseConnected) {
				whiteNoise = 2.f * sanguineRandom::normal();
			}
			inVoltage = whiteNoise;
		}
	}

	void onPortChange(const PortChangeEvent& e) override {
		switch (e.type) {
		case Port::INPUT:
			switch (e.portId) {
			case INPUT_VOLTAGE:
				bInVoltageConnected = e.connecting;
				break;

			case INPUT_SLEW:
				bInSlewConnected = e.connecting;
			default:
				break;
			}
			break;
		case Port::OUTPUT:
			switch (e.portId) {
			case OUTPUT_NOISE:
				bOutNoiseConnected = e.connecting;
				break;

			case OUTPUT_VOLTAGE:
				bOutVoltageConnected = e.connecting;
				break;
			}
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = SanguineModule::dataToJson();

		setJsonBoolean(rootJ, "storeVoltageInPatch", bStoreVoltageInPatch);

		if (bStoreVoltageInPatch) {
			setJsonFloat(rootJ, "heldVoltage", engine.voltage);
		}
#ifndef METAMODULE
		setJsonInt(rootJ, "haloType", static_cast<int>(haloType));
#endif

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		SanguineModule::dataFromJson(rootJ);

		getJsonBoolean(rootJ, "storeVoltageInPatch", bStoreVoltageInPatch);
		if (bStoreVoltageInPatch) {
			if (getJsonFloat(rootJ, "heldVoltage", engine.voltage)) {
				outputs[OUTPUT_VOLTAGE].setChannels(1);
				outputs[OUTPUT_VOLTAGE].setVoltage(engine.voltage);
			}
		}

		json_int_t intValue;

#ifndef METAMODULE
		if (getJsonInt(rootJ, "haloType", intValue)) {
			haloType = static_cast<HaloTypes>(intValue);
		}
#endif
	}
};

struct DungeonWidget : SanguineModuleWidget {
	explicit DungeonWidget(Dungeon* module) {
		setModule(module);

		moduleName = "dungeon";
		panelSize = SIZE_14;
		backplateColor = PLATE_PURPLE;
		bFaceplateSuffix = false;

		makePanel();

		addScrews(SCREW_ALL);

		FramebufferWidget* dungeonFrameBuffer = new FramebufferWidget();
		addChild(dungeonFrameBuffer);

#ifndef METAMODULE
		SanguineMultiColoredShapedLight* moonLight = new SanguineMultiColoredShapedLight();
		moonLight->box.pos = millimetersToPixelsVec(3.361, 27.351);
		moonLight->module = module;
		moonLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/dungeon_moon_light.svg")));
		moonLight->svgGradient = Svg::load(asset::plugin(pluginInstance, "res/dungeon_moon_gradient.svg"));
		moonLight->haloRadiusFactor = 2.1f;
		dungeonFrameBuffer->addChild(moonLight);
#else
		addChild(createLightCentered<LargeLight<RedGreenBlueLight>>(millimetersToPixelsVec(36.76, 55.256), module,
			Dungeon::LIGHT_VOLTAGE));
#endif

		addParam(createParamCentered<CKD6>(millimetersToPixelsVec(62.386, 12.334), module, Dungeon::PARAM_TRIGGER));
		addChild(createLightCentered<CKD6Light<RedLight>>(millimetersToPixelsVec(62.386, 12.334), module, Dungeon::LIGHT_TRIGGER));

		addParam(createParamCentered<BefacoTinyKnobRed>(millimetersToPixelsVec(10.027, 12.334), module, Dungeon::PARAM_MODE));

		SanguineTinyNumericDisplay* displayMode = new SanguineTinyNumericDisplay(2, module, 35.56, 16.934);
		displayMode->displayType = DISPLAY_STRING;
		dungeonFrameBuffer->addChild(displayMode);
		displayMode->fallbackString = dungeon::modeLabels[0];

		addParam(createLightParamCentered<VCVLightSlider<GreenRedLight>>(millimetersToPixelsVec(36.76, 73.316), module,
			Dungeon::PARAM_SLEW, Dungeon::LIGHT_SLEW));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(36.76, 95.874), module, Dungeon::INPUT_SLEW));

		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(8.762, 100.733), module, Dungeon::INPUT_CLOCK));
		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(8.762, 116.011), module, Dungeon::INPUT_VOLTAGE));

		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(62.386, 100.733), module, Dungeon::OUTPUT_NOISE));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(62.386, 116.011), module, Dungeon::OUTPUT_VOLTAGE));

#ifndef METAMODULE
		SanguineStaticRGBLight* clockLight = new SanguineStaticRGBLight(module, "res/clock_lit.svg", 8.762, 93.246, true, kSanguineYellowLight);
		addChild(clockLight);

		SanguineMonoInputLight* inLight = new SanguineMonoInputLight(module, 8.762, 108.611);
		addChild(inLight);

		SanguineStaticRGBLight* noiseLight = new SanguineStaticRGBLight(module, "res/noise_lit.svg", 62.386, 93.246, true, kSanguineYellowLight);
		addChild(noiseLight);

		SanguineMonoOutputLight* outLight = new SanguineMonoOutputLight(module, 62.386, 108.311);
		addChild(outLight);

		SanguineBloodLogoLight* bloodLight = new SanguineBloodLogoLight(module, 25.796, 109.702);
		addChild(bloodLight);

		SanguineMonstersLogoLight* monstersLight = new SanguineMonstersLogoLight(module, 38.928, 116.658);
		addChild(monstersLight);
#endif

		if (module) {
#ifndef METAMODULE
			moonLight->innerColor = &module->innerMoon;
			moonLight->outerColor = &module->outerMoon;
			moonLight->haloType = &module->haloType;
#endif

			displayMode->values.displayText = &module->modeLabel;
		}
	}

	void appendContextMenu(Menu* menu) override {
		SanguineModuleWidget::appendContextMenu(menu);

		Dungeon* module = dynamic_cast<Dungeon*>(this->module);

		menu->addChild(new MenuSeparator);

		menu->addChild(createSubmenuItem("Options", "",
			[=](Menu* menu) {
				menu->addChild(createCheckMenuItem("Store held voltage in patch", "",
					[=]() {return module->bStoreVoltageInPatch; },
					[=]() {module->bStoreVoltageInPatch = !module->bStoreVoltageInPatch; }));

#ifndef METAMODULE
				menu->addChild(new MenuSeparator);

				menu->addChild(createCheckMenuItem("Draw moon halo", "",
					[=]() { return module->haloType == HALO_CIRCULAR ? true : false; },
					[=]() { module->haloType == HALO_CIRCULAR ? module->haloType = HALO_NONE : module->haloType = HALO_CIRCULAR; }));
#endif
			}
		));
	}
};

Model* modelDungeon = createModel<Dungeon, DungeonWidget>("Sanguine-Monsters-Dungeon");