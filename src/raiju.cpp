#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#ifndef METAMODULE
#include "seqcomponents.hpp"
#endif
#include "sanguinehelpers.hpp"
#include "sanguinejson.hpp"

#include <iomanip>

#include "raiju.hpp"

struct Raiju : SanguineModule {
	static const int kVoltagesCount = 8;

	enum ParamIds {
		ENUMS(PARAM_VOLTAGE_SELECTOR, kVoltagesCount),
		PARAM_CHANNEL_COUNT,
		ENUMS(PARAM_VOLTAGE, kVoltagesCount),
		PARAMS_COUNT
	};

	enum InputIds {
		INPUTS_COUNT
	};

	enum OutputIds {
		ENUMS(OUTPUT_VOLTAGE, kVoltagesCount),
		OUTPUT_EIGHT_CHANNELS,
		OUTPUTS_COUNT
	};

	enum LightIds {
#ifdef METAMODULE
		ENUMS(LIGHT_SELECTED_CHANNEL_1, 2),
		ENUMS(LIGHT_SELECTED_CHANNEL_2, 2),
		ENUMS(LIGHT_SELECTED_CHANNEL_3, 2),
		ENUMS(LIGHT_SELECTED_CHANNEL_4, 2),
		ENUMS(LIGHT_SELECTED_CHANNEL_5, 2),
		ENUMS(LIGHT_SELECTED_CHANNEL_6, 2),
		ENUMS(LIGHT_SELECTED_CHANNEL_7, 2),
		ENUMS(LIGHT_SELECTED_CHANNEL_8, 2),
#endif
		LIGHTS_COUNT
	};

	bool bOutputConnected = false;
	bool bPolyOutConnected = false;

	int currentChannelCount = 1;
	int channelCounts[kVoltagesCount] = { 1,1,1,1,1,1,1,1 };
	int lastSelectedVoltage = -1;
	int selectedVoltage = 0;

	static const int kClockDivision = 1024;

	float voltages[kVoltagesCount];

	std::string strVoltages[kVoltagesCount] = { "0.000" ,"0.000" ,"0.000" ,"0.000" ,"0.000" ,"0.000" ,"0.000" ,"0.000" };

	dsp::BooleanTrigger btButtons[kVoltagesCount];

	dsp::ClockDivider clockDivider;

	Raiju() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		configParam(PARAM_CHANNEL_COUNT, 1.f, 16.f, 1.f, "Polyphonic output channels", "", 0.f, 1.f, 0.f);
		paramQuantities[PARAM_CHANNEL_COUNT]->snapEnabled = true;

		for (int component = 0; component < kVoltagesCount; ++component) {
			configOutput(OUTPUT_VOLTAGE + component, "Voltage " + std::to_string(component + 1));
			configParam(PARAM_VOLTAGE + component, -10.f, 10.f, 0.f, "Voltage selector " + std::to_string(component + 1));
		}

		configOutput(OUTPUT_EIGHT_CHANNELS, "Voltage series polyphonic");

		memset(voltages, 0, sizeof(float) * kVoltagesCount);

		clockDivider.setDivision(kClockDivision);
	}

	void process(const ProcessArgs& args) override {
		if (clockDivider.process()) {
			bPolyOutConnected = outputs[OUTPUT_EIGHT_CHANNELS].isConnected();

			pollSwitches();
			currentChannelCount = channelCounts[selectedVoltage];
			if (selectedVoltage != lastSelectedVoltage) {
				params[PARAM_CHANNEL_COUNT].setValue(currentChannelCount);
				lastSelectedVoltage = selectedVoltage;
			}

			int selectedChannelCount = params[PARAM_CHANNEL_COUNT].getValue();
			if (currentChannelCount != selectedChannelCount) {
				channelCounts[selectedVoltage] = selectedChannelCount;
				currentChannelCount = selectedChannelCount;
			}

			for (uint8_t voltage = 0; voltage < kVoltagesCount; ++voltage) {
				params[PARAM_VOLTAGE_SELECTOR + voltage].setValue(voltage == selectedVoltage ? 1 : 0);

				// Get channel voltages and update strings for displays
				voltages[voltage] = params[PARAM_VOLTAGE + voltage].getValue();
				std::stringstream stringStream;
				stringStream << std::fixed << std::setprecision(3) << std::setfill('0') << std::setw(6) << voltages[voltage];
				if (voltages[voltage] < 0 && voltages[voltage] > -10) {
					std::string tmpStr = stringStream.str();
					tmpStr.replace(0, 1, "0");
					tmpStr.insert(0, "-");
					strVoltages[voltage] = tmpStr;
				} else
					strVoltages[voltage] = stringStream.str();

				if (outputs[OUTPUT_VOLTAGE + voltage].isConnected()) {
					outputs[OUTPUT_VOLTAGE + voltage].setChannels(channelCounts[voltage]);
					float outputVoltages[PORT_MAX_CHANNELS];
					std::fill(outputVoltages, outputVoltages + PORT_MAX_CHANNELS, voltages[voltage]);
					outputs[OUTPUT_VOLTAGE + voltage].writeVoltages(outputVoltages);
				}

#ifdef METAMODULE
				int currentLight = LIGHT_SELECTED_CHANNEL_1 + voltage * 2;
				bool bIsSelectedVoltage = selectedVoltage == voltage;
				lights[currentLight + 0].setBrightness(!bIsSelectedVoltage ? kSanguineButtonLightValue : 0.f);
				lights[currentLight + 1].setBrightness(bIsSelectedVoltage ? kSanguineButtonLightValue : 0.f);
#endif
			}

			if (bPolyOutConnected) {
				outputs[OUTPUT_EIGHT_CHANNELS].setChannels(kVoltagesCount);
				outputs[OUTPUT_EIGHT_CHANNELS].writeVoltages(voltages);
			}
		}
	}

	void pollSwitches() {
		for (uint8_t button = 0; button < kVoltagesCount; ++button) {
			if (btButtons[button].process(params[PARAM_VOLTAGE_SELECTOR + button].getValue())) {
				selectedVoltage = button;
			}
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = SanguineModule::dataToJson();

		json_t* channelCountsJ = json_array();
		for (int channelCount : channelCounts) {
			json_t* cloneCountJ = json_integer(channelCount);
			json_array_append_new(channelCountsJ, cloneCountJ);
		}
		json_object_set_new(rootJ, "channelCounts", channelCountsJ);

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		SanguineModule::dataFromJson(rootJ);

		json_t* channelCountsJ = json_object_get(rootJ, "channelCounts");
		size_t idx;
		json_t* cloneCountJ;
		json_array_foreach(channelCountsJ, idx, cloneCountJ) {
			channelCounts[idx] = json_integer_value(cloneCountJ);
		}
	}
};

struct RaijuWidget : SanguineModuleWidget {
	explicit RaijuWidget(Raiju* module) {
		setModule(module);

		moduleName = "raiju";
		panelSize = SIZE_27;
		backplateColor = PLATE_PURPLE;
		bFaceplateSuffix = false;

		makePanel();

		addScrews(SCREW_ALL);

		addChild(createParamCentered<BefacoTinyKnobRed>(millimetersToPixelsVec(127.365, 12.197), module, Raiju::PARAM_CHANNEL_COUNT));

		float yDistance = 19.688;
		float currentY = 32.982;

		for (int component = 0; component < 4; ++component) {
			addChild(createParamCentered<BefacoTinyKnobRed>(millimetersToPixelsVec(19.21, currentY),
				module, Raiju::PARAM_VOLTAGE + component));
			addChild(createParamCentered<BefacoTinyKnobBlack>(millimetersToPixelsVec(117.942, currentY),
				module, Raiju::PARAM_VOLTAGE + component + 4));
			currentY += yDistance;
		}

		float currentX = 37.073;
		float xDistance = 12.136;
		for (int component = 0; component < 4; ++component) {
			addChild(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(currentX, 111.758),
				module, Raiju::OUTPUT_VOLTAGE + component));
			currentX += xDistance;
		}
		currentX = 92.018;
		for (int component = 4; component < 8; ++component) {
			addChild(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(currentX, 111.758),
				module, Raiju::OUTPUT_VOLTAGE + component));
			currentX += xDistance;
		}

		addChild(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(82.75, 118.393),
			module, Raiju::OUTPUT_EIGHT_CHANNELS));

		FramebufferWidget* raijuFrameBuffer = new FramebufferWidget();
		addChild(raijuFrameBuffer);

#ifndef METAMODULE
		currentY = 29.182f;
		yDistance = 19.689f;

		addParam(createParam<SeqStep1Big>(millimetersToPixelsVec(4.012, currentY),
			module, Raiju::PARAM_VOLTAGE_SELECTOR + 0));
		currentY += yDistance;

		addParam(createParam<SeqStep2Big>(millimetersToPixelsVec(4.012, currentY),
			module, Raiju::PARAM_VOLTAGE_SELECTOR + 1));
		currentY += yDistance;

		addParam(createParam<SeqStep3Big>(millimetersToPixelsVec(4.012, currentY),
			module, Raiju::PARAM_VOLTAGE_SELECTOR + 2));
		currentY += yDistance;

		addParam(createParam<SeqStep4Big>(millimetersToPixelsVec(4.012, currentY),
			module, Raiju::PARAM_VOLTAGE_SELECTOR + 3));

		currentY = 29.182f;
		yDistance = 19.689f;

		addParam(createParam<SeqStep5Big>(millimetersToPixelsVec(125.548, currentY),
			module, Raiju::PARAM_VOLTAGE_SELECTOR + 4));
		currentY += yDistance;

		addParam(createParam<SeqStep6Big>(millimetersToPixelsVec(125.548, currentY),
			module, Raiju::PARAM_VOLTAGE_SELECTOR + 5));
		currentY += yDistance;

		addParam(createParam<SeqStep7Big>(millimetersToPixelsVec(125.548, currentY),
			module, Raiju::PARAM_VOLTAGE_SELECTOR + 6));
		currentY += yDistance;

		addParam(createParam<SeqStep8Big>(millimetersToPixelsVec(125.548, currentY),
			module, Raiju::PARAM_VOLTAGE_SELECTOR + 7));
#else
		currentY = 32.982f;
		yDistance = 19.688f;

		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(9.412, currentY),
			module, Raiju::PARAM_VOLTAGE_SELECTOR + 0, Raiju::LIGHT_SELECTED_CHANNEL_1));
		currentY += yDistance;

		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(9.412, currentY),
			module, Raiju::PARAM_VOLTAGE_SELECTOR + 1, Raiju::LIGHT_SELECTED_CHANNEL_2));
		currentY += yDistance;

		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(9.412, currentY),
			module, Raiju::PARAM_VOLTAGE_SELECTOR + 2, Raiju::LIGHT_SELECTED_CHANNEL_3));
		currentY += yDistance;

		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(9.412, currentY),
			module, Raiju::PARAM_VOLTAGE_SELECTOR + 3, Raiju::LIGHT_SELECTED_CHANNEL_4));

		currentY = 32.982f;
		yDistance = 19.688f;

		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(127.748, currentY),
			module, Raiju::PARAM_VOLTAGE_SELECTOR + 4, Raiju::LIGHT_SELECTED_CHANNEL_5));
		currentY += yDistance;

		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(127.748, currentY),
			module, Raiju::PARAM_VOLTAGE_SELECTOR + 5, Raiju::LIGHT_SELECTED_CHANNEL_6));
		currentY += yDistance;

		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(127.748, currentY),
			module, Raiju::PARAM_VOLTAGE_SELECTOR + 6, Raiju::LIGHT_SELECTED_CHANNEL_7));
		currentY += yDistance;

		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(127.748, currentY),
			module, Raiju::PARAM_VOLTAGE_SELECTOR + 7, Raiju::LIGHT_SELECTED_CHANNEL_8));
#endif

		SanguineLedNumberDisplay* displayChannelCount = new SanguineLedNumberDisplay(2, module, 112.331, 15.197);
		raijuFrameBuffer->addChild(displayChannelCount);
		displayChannelCount->fallbackNumber = 1;

		SanguineMatrixDisplay* displayVoltage1 = new SanguineMatrixDisplay(7, module, 45.663, 32.982);
		raijuFrameBuffer->addChild(displayVoltage1);
		displayVoltage1->fallbackString = raiju::kBrowserDisplayText;

		SanguineMatrixDisplay* displayVoltage2 = new SanguineMatrixDisplay(7, module, 45.663, 52.67);
		raijuFrameBuffer->addChild(displayVoltage2);
		displayVoltage2->fallbackString = raiju::kBrowserDisplayText;

		SanguineMatrixDisplay* displayVoltage3 = new SanguineMatrixDisplay(7, module, 45.663, 72.359);
		raijuFrameBuffer->addChild(displayVoltage3);
		displayVoltage3->fallbackString = raiju::kBrowserDisplayText;

		SanguineMatrixDisplay* displayVoltage4 = new SanguineMatrixDisplay(7, module, 45.663, 92.048);
		raijuFrameBuffer->addChild(displayVoltage4);
		displayVoltage4->fallbackString = raiju::kBrowserDisplayText;

		SanguineMatrixDisplay* displayVoltage5 = new SanguineMatrixDisplay(7, module, 91.495, 32.982);
		raijuFrameBuffer->addChild(displayVoltage5);
		displayVoltage5->fallbackString = raiju::kBrowserDisplayText;

		SanguineMatrixDisplay* displayVoltage6 = new SanguineMatrixDisplay(7, module, 91.495, 52.67);
		raijuFrameBuffer->addChild(displayVoltage6);
		displayVoltage6->fallbackString = raiju::kBrowserDisplayText;

		SanguineMatrixDisplay* displayVoltage7 = new SanguineMatrixDisplay(7, module, 91.495, 72.359);
		raijuFrameBuffer->addChild(displayVoltage7);
		displayVoltage7->fallbackString = raiju::kBrowserDisplayText;

		SanguineMatrixDisplay* displayVoltage8 = new SanguineMatrixDisplay(7, module, 91.495, 92.048);
		raijuFrameBuffer->addChild(displayVoltage8);
		displayVoltage8->fallbackString = raiju::kBrowserDisplayText;

#ifndef METAMODULE
		SanguineStaticRGBLight* channelsLight = new SanguineStaticRGBLight(module, "res/channels_lit.svg",
			127.365, 20.199, true, kSanguineBlueLight);
		addChild(channelsLight);

		SanguinePolyOutputLight* outLight = new SanguinePolyOutputLight(module, 82.75, 104.658);
		addChild(outLight);

		SanguineBloodLogoLight* bloodLight = new SanguineBloodLogoLight(module, 6.615, 109.819);
		addChild(bloodLight);

		SanguineMonstersLogoLight* monstersLight = new SanguineMonstersLogoLight(module, 19.747, 116.774);
		addChild(monstersLight);
#endif

		if (module) {
			displayChannelCount->values.numberValue = (&module->currentChannelCount);

			displayVoltage1->values.displayText = &module->strVoltages[0];
			displayVoltage2->values.displayText = &module->strVoltages[1];
			displayVoltage3->values.displayText = &module->strVoltages[2];
			displayVoltage4->values.displayText = &module->strVoltages[3];
			displayVoltage5->values.displayText = &module->strVoltages[4];
			displayVoltage6->values.displayText = &module->strVoltages[5];
			displayVoltage7->values.displayText = &module->strVoltages[6];
			displayVoltage8->values.displayText = &module->strVoltages[7];
		}
	}
};

Model* modelRaiju = createModel<Raiju, RaijuWidget>("Sanguine-Monsters-Raiju");
