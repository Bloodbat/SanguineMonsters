#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include <iomanip>
#include "seqcomponents.hpp"
#include "sanguinehelpers.hpp"

struct Raiju : Module {
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
				}
				else
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
		json_t* rootJ = json_object();

		json_t* channelCountsJ = json_array();
		for (int channelCount : channelCounts) {
			json_t* cJ = json_integer(channelCount);
			json_array_append_new(channelCountsJ, cJ);
		}
		json_object_set_new(rootJ, "channelCounts", channelCountsJ);

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* channelCountsJ = json_object_get(rootJ, "channelCounts");
		size_t idx;
		json_t* cJ;
		json_array_foreach(channelCountsJ, idx, cJ) {
			channelCounts[idx] = json_integer_value(cJ);
		}
	}
};

struct RaijuWidget : ModuleWidget {
	RaijuWidget(Raiju* module) {
		setModule(module);

		SanguinePanel* panel = new SanguinePanel(pluginInstance, "res/backplate_27hp_purple.svg", "res/raiju.svg");
		setPanel(panel);

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

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

		addChild(createOutputCentered<BananutRed>(millimetersToPixelsVec(82.75, 118.393), module, Raiju::OUTPUT_EIGHT_CHANNELS));

		FramebufferWidget* raijuFrameBuffer = new FramebufferWidget();
		addChild(raijuFrameBuffer);

		currentY = 29.182f;
		yDistance = 19.689f;

		SeqStepSwitch* step1 = createParam<SeqStepSwitch>(millimetersToPixelsVec(4.012, currentY),
			module, Raiju::PARAM_VOLTAGE_SELECTOR + 0);
		step1->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_1_off.svg")));
		step1->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_1_on.svg")));
		addParam(step1);
		currentY += yDistance;
		SeqStepSwitch* step2 = createParam<SeqStepSwitch>(millimetersToPixelsVec(4.012, currentY),
			module, Raiju::PARAM_VOLTAGE_SELECTOR + 1);
		step2->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_2_off.svg")));
		step2->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_2_on.svg")));
		addParam(step2);
		currentY += yDistance;
		SeqStepSwitch* step3 = createParam<SeqStepSwitch>(millimetersToPixelsVec(4.012, currentY),
			module, Raiju::PARAM_VOLTAGE_SELECTOR + 2);
		step3->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_3_off.svg")));
		step3->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_3_on.svg")));
		addParam(step3);
		currentY += yDistance;
		SeqStepSwitch* step4 = createParam<SeqStepSwitch>(millimetersToPixelsVec(4.012, currentY),
			module, Raiju::PARAM_VOLTAGE_SELECTOR + 3);
		step4->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_4_off.svg")));
		step4->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_4_on.svg")));
		addParam(step4);

		currentY = 29.182f;
		yDistance = 19.689f;
		SeqStepSwitch* step5 = createParam<SeqStepSwitch>(millimetersToPixelsVec(125.548, currentY),
			module, Raiju::PARAM_VOLTAGE_SELECTOR + 4);
		step5->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_5_off.svg")));
		step5->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_5_on.svg")));
		addParam(step5);
		currentY += yDistance;
		SeqStepSwitch* step6 = createParam<SeqStepSwitch>(millimetersToPixelsVec(125.548, currentY),
			module, Raiju::PARAM_VOLTAGE_SELECTOR + 5);
		step6->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_6_off.svg")));
		step6->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_6_on.svg")));
		addParam(step6);
		currentY += yDistance;
		SeqStepSwitch* step7 = createParam<SeqStepSwitch>(millimetersToPixelsVec(125.548, currentY),
			module, Raiju::PARAM_VOLTAGE_SELECTOR + 6);
		step7->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_7_off.svg")));
		step7->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_7_on.svg")));
		addParam(step7);
		currentY += yDistance;
		SeqStepSwitch* step8 = createParam<SeqStepSwitch>(millimetersToPixelsVec(125.548, currentY),
			module, Raiju::PARAM_VOLTAGE_SELECTOR + 7);
		step8->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_8_off.svg")));
		step8->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/step_8_on.svg")));
		addParam(step8);

		SanguineLedNumberDisplay* displayChannelCount = new SanguineLedNumberDisplay(2, module, 112.331, 13.997);
		raijuFrameBuffer->addChild(displayChannelCount);

		if (module)
			displayChannelCount->values.numberValue = (&module->currentChannelCount);

		SanguineMatrixDisplay* displayVoltage1 = new SanguineMatrixDisplay(7, module, 45.663, 32.982);
		raijuFrameBuffer->addChild(displayVoltage1);

		if (module)
			displayVoltage1->values.displayText = &module->strVoltages[0];

		SanguineMatrixDisplay* displayVoltage2 = new SanguineMatrixDisplay(7, module, 45.663, 52.67);
		raijuFrameBuffer->addChild(displayVoltage2);

		if (module)
			displayVoltage2->values.displayText = &module->strVoltages[1];

		SanguineMatrixDisplay* displayVoltage3 = new SanguineMatrixDisplay(7, module, 45.663, 72.359);
		raijuFrameBuffer->addChild(displayVoltage3);

		if (module)
			displayVoltage3->values.displayText = &module->strVoltages[2];

		SanguineMatrixDisplay* displayVoltage4 = new SanguineMatrixDisplay(7, module, 45.663, 92.048);
		raijuFrameBuffer->addChild(displayVoltage4);

		if (module)
			displayVoltage4->values.displayText = &module->strVoltages[3];

		SanguineMatrixDisplay* displayVoltage5 = new SanguineMatrixDisplay(7, module, 91.495, 32.982);
		raijuFrameBuffer->addChild(displayVoltage5);

		if (module)
			displayVoltage5->values.displayText = &module->strVoltages[4];

		SanguineMatrixDisplay* displayVoltage6 = new SanguineMatrixDisplay(7, module, 91.495, 52.67);
		raijuFrameBuffer->addChild(displayVoltage6);

		if (module)
			displayVoltage6->values.displayText = &module->strVoltages[5];

		SanguineMatrixDisplay* displayVoltage7 = new SanguineMatrixDisplay(7, module, 91.495, 72.359);
		raijuFrameBuffer->addChild(displayVoltage7);

		if (module)
			displayVoltage7->values.displayText = &module->strVoltages[6];

		SanguineMatrixDisplay* displayVoltage8 = new SanguineMatrixDisplay(7, module, 91.495, 92.048);
		raijuFrameBuffer->addChild(displayVoltage8);

		if (module)
			displayVoltage8->values.displayText = &module->strVoltages[7];

		SanguineShapedLight* channelsLight = new SanguineShapedLight();
		channelsLight->box.pos = millimetersToPixelsVec(125.219, 16.15);
		channelsLight->module = module;
		channelsLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/channels_lit.svg")));
		addChild(channelsLight);

		SanguinePolyOutputLight* outLight = new SanguinePolyOutputLight();
		outLight->box.pos = millimetersToPixelsVec(79.456, 102.594);
		outLight->module = module;
		addChild(outLight);

		SanguineShapedLight* bloodLight = new SanguineShapedLight();
		bloodLight->box.pos = millimetersToPixelsVec(4.718, 105.882);
		bloodLight->module = module;
		bloodLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/blood_glowy.svg")));
		addChild(bloodLight);

		SanguineShapedLight* monstersLight = new SanguineShapedLight();
		monstersLight->box.pos = millimetersToPixelsVec(12.04, 113.849);
		monstersLight->module = module;
		monstersLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/monsters_lit.svg")));
		addChild(monstersLight);
	}
};

Model* modelRaiju = createModel<Raiju, RaijuWidget>("Sanguine-Monsters-Raiju");
