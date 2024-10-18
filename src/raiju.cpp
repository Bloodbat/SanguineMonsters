#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include <iomanip>
#include "seqcomponents.hpp"
#include "sanguinehelpers.hpp"
#include "raiju.hpp"

struct Raiju : SanguineModule {
	static const int kVoltagesCount = 8;

	enum ParamIds {
		ENUMS(PARAM_VOLTAGE_SELECTOR, kVoltagesCount),
		PARAM_CHANNEL_COUNT,
		ENUMS(PARAM_VOLTAGE, kVoltagesCount),
		PARAMS_COUNT
	};

	enum OutputIds {
		ENUMS(OUTPUT_VOLTAGE, kVoltagesCount),
		OUTPUT_EIGHT_CHANNELS,
		OUTPUTS_COUNT
	};

	bool bOutputConnected = false;
	bool bPolyOutConnected = false;

	int currentChannelCount = 1;
	int channelCounts[kVoltagesCount] = { 1,1,1,1,1,1,1,1 };
	int lastSelectedVoltage = -1;
	int selectedVoltage = 0;

	static const int kClockDivision = 1024;

	float voltages[kVoltagesCount];
	float outputVoltages[16];

	std::string strVoltages[kVoltagesCount] = { "0.000" ,"0.000" ,"0.000" ,"0.000" ,"0.000" ,"0.000" ,"0.000" ,"0.000" };

	dsp::BooleanTrigger btButtons[kVoltagesCount];

	dsp::ClockDivider clockDivider;

	Raiju() {
		config(PARAMS_COUNT, 0, OUTPUTS_COUNT, 0);

		configParam(PARAM_CHANNEL_COUNT, 1.0f, 16.0f, 1.0f, "Polyphonic output channels", "", 0.0f, 1.0f, 0.0f);
		paramQuantities[PARAM_CHANNEL_COUNT]->snapEnabled = true;

		for (int i = 0; i < kVoltagesCount; i++) {
			configOutput(OUTPUT_VOLTAGE + i, "Voltage " + std::to_string(i + 1));
			configParam(PARAM_VOLTAGE + i, -10.f, 10.f, 0.f, "Voltage selector " + std::to_string(i + 1));
		}

		configOutput(OUTPUT_EIGHT_CHANNELS, "Voltage series polyphonic");

		memset(voltages, 0, sizeof(float) * kVoltagesCount);

		clockDivider.setDivision(kClockDivision);
	}

	void pollSwitches() {
		for (uint8_t i = 0; i < kVoltagesCount; i++) {
			if (btButtons[i].process(params[PARAM_VOLTAGE_SELECTOR + i].getValue())) {
				selectedVoltage = i;
			}
		}
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

			for (uint8_t i = 0; i < kVoltagesCount; i++) {
				params[PARAM_VOLTAGE_SELECTOR + i].setValue(i == selectedVoltage ? 1 : 0);

				// Get channel voltages and update strings for displays
				voltages[i] = params[PARAM_VOLTAGE + i].getValue();
				std::stringstream stringStream;
				stringStream << std::fixed << std::setprecision(3) << std::setfill('0') << std::setw(6) << voltages[i];
				if (voltages[i] < 0 && voltages[i] > -10)
				{
					std::string tmpStr = stringStream.str();
					tmpStr.replace(0, 1, "0");
					tmpStr.insert(0, "-");
					strVoltages[i] = tmpStr;
				} else
					strVoltages[i] = stringStream.str();

				if (outputs[OUTPUT_VOLTAGE + i].isConnected()) {
					outputs[OUTPUT_VOLTAGE + i].setChannels(channelCounts[i]);
					std::fill(outputVoltages, outputVoltages + 16, voltages[i]);
					outputs[OUTPUT_VOLTAGE + i].writeVoltages(outputVoltages);
				}
			}

			if (bPolyOutConnected) {
				outputs[OUTPUT_EIGHT_CHANNELS].setChannels(kVoltagesCount);
				outputs[OUTPUT_EIGHT_CHANNELS].writeVoltages(voltages);
			}
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = SanguineModule::dataToJson();

		json_t* channelCountsJ = json_array();
		for (int channelCount : channelCounts) {
			json_t* cJ = json_integer(channelCount);
			json_array_append_new(channelCountsJ, cJ);
		}
		json_object_set_new(rootJ, "channelCounts", channelCountsJ);

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		SanguineModule::dataFromJson(rootJ);

		json_t* channelCountsJ = json_object_get(rootJ, "channelCounts");
		size_t idx;
		json_t* cJ;
		json_array_foreach(channelCountsJ, idx, cJ) {
			channelCounts[idx] = json_integer_value(cJ);
		}
	}
};

struct RaijuWidget : SanguineModuleWidget {
	RaijuWidget(Raiju* module) {
		setModule(module);

		moduleName = "raiju";
		panelSize = SIZE_27;
		backplateColor = PLATE_PURPLE;
		bFaceplateSuffix = false;

		makePanel();

		addScrews(SCREW_ALL);

		addChild(createParamCentered<BefacoTinyKnobRed>(millimetersToPixelsVec(127.365, 10.997), module, Raiju::PARAM_CHANNEL_COUNT));

		float yDistance = 19.688;
		float currentY = 32.982;

		for (int i = 0; i < 4; i++) {
			addChild(createParamCentered<BefacoTinyKnobRed>(millimetersToPixelsVec(19.21, currentY), module, Raiju::PARAM_VOLTAGE + i));
			currentY += yDistance;
		}

		currentY = 32.982;
		for (int i = 4; i < 8; i++) {
			addChild(createParamCentered<BefacoTinyKnobBlack>(millimetersToPixelsVec(117.942, currentY), module, Raiju::PARAM_VOLTAGE + i));
			currentY += yDistance;
		}

		float currentX = 37.073;
		float xDistance = 12.136;
		for (int i = 0; i < 4; i++) {
			addChild(createOutputCentered<BananutRed>(millimetersToPixelsVec(currentX, 111.758), module, Raiju::OUTPUT_VOLTAGE + i));
			currentX += xDistance;
		}
		currentX = 92.018;
		for (int i = 4; i < 8; i++) {
			addChild(createOutputCentered<BananutRed>(millimetersToPixelsVec(currentX, 111.758), module, Raiju::OUTPUT_VOLTAGE + i));
			currentX += xDistance;
		}

		addChild(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(82.75, 118.393), module, Raiju::OUTPUT_EIGHT_CHANNELS));

		FramebufferWidget* raijuFrameBuffer = new FramebufferWidget();
		addChild(raijuFrameBuffer);

		currentY = 29.182f;
		yDistance = 19.689f;

		addParam(createParam<SeqStep1Big>(millimetersToPixelsVec(4.012, currentY), module, Raiju::PARAM_VOLTAGE_SELECTOR + 0));
		currentY += yDistance;

		addParam(createParam<SeqStep2Big>(millimetersToPixelsVec(4.012, currentY), module, Raiju::PARAM_VOLTAGE_SELECTOR + 1));
		currentY += yDistance;

		addParam(createParam<SeqStep3Big>(millimetersToPixelsVec(4.012, currentY), module, Raiju::PARAM_VOLTAGE_SELECTOR + 2));
		currentY += yDistance;

		addParam(createParam<SeqStep4Big>(millimetersToPixelsVec(4.012, currentY), module, Raiju::PARAM_VOLTAGE_SELECTOR + 3));

		currentY = 29.182f;
		yDistance = 19.689f;

		addParam(createParam<SeqStep5Big>(millimetersToPixelsVec(125.548, currentY), module, Raiju::PARAM_VOLTAGE_SELECTOR + 4));
		currentY += yDistance;

		addParam(createParam<SeqStep6Big>(millimetersToPixelsVec(125.548, currentY), module, Raiju::PARAM_VOLTAGE_SELECTOR + 5));
		currentY += yDistance;

		addParam(createParam<SeqStep7Big>(millimetersToPixelsVec(125.548, currentY), module, Raiju::PARAM_VOLTAGE_SELECTOR + 6));
		currentY += yDistance;

		addParam(createParam<SeqStep8Big>(millimetersToPixelsVec(125.548, currentY), module, Raiju::PARAM_VOLTAGE_SELECTOR + 7));

		SanguineLedNumberDisplay* displayChannelCount = new SanguineLedNumberDisplay(2, module, 112.331, 13.997);
		raijuFrameBuffer->addChild(displayChannelCount);
		displayChannelCount->fallbackNumber = 1;

		if (module)
			displayChannelCount->values.numberValue = (&module->currentChannelCount);

		SanguineMatrixDisplay* displayVoltage1 = new SanguineMatrixDisplay(7, module, 45.663, 32.982);
		raijuFrameBuffer->addChild(displayVoltage1);
		displayVoltage1->fallbackString = kBrowserDisplayText;

		if (module)
			displayVoltage1->values.displayText = &module->strVoltages[0];

		SanguineMatrixDisplay* displayVoltage2 = new SanguineMatrixDisplay(7, module, 45.663, 52.67);
		raijuFrameBuffer->addChild(displayVoltage2);
		displayVoltage2->fallbackString = kBrowserDisplayText;

		if (module)
			displayVoltage2->values.displayText = &module->strVoltages[1];

		SanguineMatrixDisplay* displayVoltage3 = new SanguineMatrixDisplay(7, module, 45.663, 72.359);
		raijuFrameBuffer->addChild(displayVoltage3);
		displayVoltage3->fallbackString = kBrowserDisplayText;

		if (module)
			displayVoltage3->values.displayText = &module->strVoltages[2];

		SanguineMatrixDisplay* displayVoltage4 = new SanguineMatrixDisplay(7, module, 45.663, 92.048);
		raijuFrameBuffer->addChild(displayVoltage4);
		displayVoltage4->fallbackString = kBrowserDisplayText;

		if (module)
			displayVoltage4->values.displayText = &module->strVoltages[3];

		SanguineMatrixDisplay* displayVoltage5 = new SanguineMatrixDisplay(7, module, 91.495, 32.982);
		raijuFrameBuffer->addChild(displayVoltage5);
		displayVoltage5->fallbackString = kBrowserDisplayText;

		if (module)
			displayVoltage5->values.displayText = &module->strVoltages[4];

		SanguineMatrixDisplay* displayVoltage6 = new SanguineMatrixDisplay(7, module, 91.495, 52.67);
		raijuFrameBuffer->addChild(displayVoltage6);
		displayVoltage6->fallbackString = kBrowserDisplayText;

		if (module)
			displayVoltage6->values.displayText = &module->strVoltages[5];

		SanguineMatrixDisplay* displayVoltage7 = new SanguineMatrixDisplay(7, module, 91.495, 72.359);
		raijuFrameBuffer->addChild(displayVoltage7);
		displayVoltage7->fallbackString = kBrowserDisplayText;

		if (module)
			displayVoltage7->values.displayText = &module->strVoltages[6];

		SanguineMatrixDisplay* displayVoltage8 = new SanguineMatrixDisplay(7, module, 91.495, 92.048);
		raijuFrameBuffer->addChild(displayVoltage8);
		displayVoltage8->fallbackString = kBrowserDisplayText;

		if (module)
			displayVoltage8->values.displayText = &module->strVoltages[7];

		SanguineStaticRGBLight* channelsLight = new SanguineStaticRGBLight(module, "res/channels_lit.svg", 127.365, 18.999, true, kSanguineBlueLight);
		addChild(channelsLight);

		SanguinePolyOutputLight* outLight = new SanguinePolyOutputLight(module, 82.75, 104.658);
		addChild(outLight);

		SanguineBloodLogoLight* bloodLight = new SanguineBloodLogoLight(module, 6.615, 109.819);
		addChild(bloodLight);

		SanguineMonstersLogoLight* monstersLight = new SanguineMonstersLogoLight(module, 19.747, 116.774);
		addChild(monstersLight);
	}
};

Model* modelRaiju = createModel<Raiju, RaijuWidget>("Sanguine-Monsters-Raiju");
