#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include <iomanip>

struct Raiju : Module {
	static const int kVoltagesCount = 8;

	enum ParamIds {
		PARAM_CHANNEL_COUNT,
		ENUMS(PARAM_VOLTAGE, kVoltagesCount),
		PARAMS_COUNT
	};

	enum OutputIds {
		ENUMS(OUTPUT_VOLTAGE, kVoltagesCount),
		OUTPUT_EIGHT_CHANNELS,
		OUTPUTS_COUNT
	};

	enum LightIds {
		ENUMS(LIGHT_VOLTAGE, kVoltagesCount * 2),
		LIGHTS_COUNT
	};
	
	bool bOutputConnected = false;
	bool bPolyOutConnected = false;

	int channelCount = 1;

	static const int kClockDivision = 64;

	float voltages[kVoltagesCount];

	std::string strVoltages[kVoltagesCount] = { "0.000" ,"0.000" ,"0.000" ,"0.000" ,"0.000" ,"0.000" ,"0.000" ,"0.000" };

	dsp::ClockDivider clockDivider;

	Raiju() {
		config(PARAMS_COUNT, 0, OUTPUTS_COUNT, LIGHTS_COUNT);

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

	void process(const ProcessArgs& args) override {
		if (clockDivider.process()) {	
			const float sampleTime = APP->engine->getSampleTime() * kClockDivision;

			float lightValue;

			bPolyOutConnected = outputs[OUTPUT_EIGHT_CHANNELS].isConnected();
			channelCount = params[PARAM_CHANNEL_COUNT].getValue();

			for (int i = 0; i < kVoltagesCount; i++) {
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
					outputs[OUTPUT_VOLTAGE + i].setChannels(channelCount);
					for (int j = 0; j < channelCount; j++) {
						outputs[OUTPUT_VOLTAGE + i].setVoltage(voltages[i], j);
					}
				}
				else {
					outputs[OUTPUT_VOLTAGE + i].setChannels(0);
				}

				// Adjust lights
				int currentLight = LIGHT_VOLTAGE + i * 2;
				if (voltages[i] > 0) {
					lightValue = rescale(voltages[i], 0.f, 10.f, 0.f, 1.0f);
					lights[currentLight + 0].setBrightnessSmooth(lightValue, sampleTime);
					lights[currentLight + 1].setBrightnessSmooth(0, sampleTime);
				}
				else if (voltages[i] < 0) {
					lightValue = rescale(voltages[i], -10.f, 0.f, -1.f, 0.f);
					lights[currentLight + 0].setBrightnessSmooth(0, sampleTime);
					lights[currentLight + 1].setBrightnessSmooth(-lightValue, sampleTime);
				}
				else
				{
					lights[currentLight + 0].setBrightnessSmooth(0, sampleTime);
					lights[currentLight + 1].setBrightnessSmooth(0, sampleTime);
				}
			}

			if (bPolyOutConnected) {
				outputs[OUTPUT_EIGHT_CHANNELS].setChannels(kVoltagesCount);
				outputs[OUTPUT_EIGHT_CHANNELS].writeVoltages(voltages);
			}
			else
				outputs[OUTPUT_EIGHT_CHANNELS].setChannels(0);			
		}
	}
};

struct RaijuWidget : ModuleWidget {
	RaijuWidget(Raiju* module) {
		setModule(module);
		setPanel(Svg::load(asset::plugin(pluginInstance, "res/raiju.svg")));

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addChild(createParamCentered<BefacoTinyKnobRed>(mm2px(Vec(127.365, 10.997)), module, Raiju::PARAM_CHANNEL_COUNT));

		float yDistance = 19.688;
		float currentY = 32.982;

		for (int i = 0; i < 4; i++) {
			addChild(createParamCentered<BefacoTinyKnobRed>(mm2px(Vec(11.011, currentY)), module, Raiju::PARAM_VOLTAGE + i));
			addChild(createLightCentered<SmallLight<GreenRedLight>>(mm2px(Vec(66.069, currentY)), module, Raiju::LIGHT_VOLTAGE + i * 2));
			currentY += yDistance;
		}

		currentY = 32.982;
		for (int i = 4; i < 8; i++) {
			addChild(createParamCentered<BefacoTinyKnobBlack>(mm2px(Vec(125.773, currentY)), module, Raiju::PARAM_VOLTAGE + i));
			addChild(createLightCentered<SmallLight<GreenRedLight>>(mm2px(Vec(70.727, currentY)), module, Raiju::LIGHT_VOLTAGE + i * 2));
			currentY += yDistance;
		}

		float currentX = 37.073;
		float xDistance = 12.136;
		for (int i = 0; i < 4; i++) {
			addChild(createOutputCentered<BananutRed>(mm2px(Vec(currentX, 111.758)), module, Raiju::OUTPUT_VOLTAGE + i));
			currentX += xDistance;
		}
		currentX = 92.018;
		for (int i = 4; i < 8; i++) {
			addChild(createOutputCentered<BananutRed>(mm2px(Vec(currentX, 111.758)), module, Raiju::OUTPUT_VOLTAGE + i));
			currentX += xDistance;
		}

		addChild(createOutputCentered<BananutRed>(mm2px(Vec(82.75, 118.393)), module, Raiju::OUTPUT_EIGHT_CHANNELS));

		FramebufferWidget* raijuFrameBuffer = new FramebufferWidget();
		addChild(raijuFrameBuffer);

		SanguineLedNumberDisplay* displayChannelCount = new SanguineLedNumberDisplay(2);
		displayChannelCount->box.pos = mm2px(Vec(104.581, 6.497));
		displayChannelCount->module = module;
		raijuFrameBuffer->addChild(displayChannelCount);

		if (module)
			displayChannelCount->value = (&module->channelCount);

		SanguineMatrixDisplay* displayVoltage1 = new SanguineMatrixDisplay(8);
		displayVoltage1->box.pos = mm2px(Vec(17.504, 27.902));
		displayVoltage1->module = module;
		raijuFrameBuffer->addChild(displayVoltage1);

		if (module)
			displayVoltage1->displayText = &module->strVoltages[0];

		SanguineMatrixDisplay* displayVoltage2 = new SanguineMatrixDisplay(8);
		displayVoltage2->box.pos = mm2px(Vec(17.504, 47.591));
		displayVoltage2->module = module;
		raijuFrameBuffer->addChild(displayVoltage2);

		if (module)
			displayVoltage2->displayText = &module->strVoltages[1];

		SanguineMatrixDisplay* displayVoltage3 = new SanguineMatrixDisplay(8);
		displayVoltage3->box.pos = mm2px(Vec(17.504, 67.279));
		displayVoltage3->module = module;
		raijuFrameBuffer->addChild(displayVoltage3);

		if (module)
			displayVoltage3->displayText = &module->strVoltages[2];

		SanguineMatrixDisplay* displayVoltage4 = new SanguineMatrixDisplay(8);
		displayVoltage4->box.pos = mm2px(Vec(17.504, 86.968));
		displayVoltage4->module = module;
		raijuFrameBuffer->addChild(displayVoltage4);

		if (module)
			displayVoltage4->displayText = &module->strVoltages[3];

		SanguineMatrixDisplay* displayVoltage5 = new SanguineMatrixDisplay(8);
		displayVoltage5->box.pos = mm2px(Vec(73.664, 27.902));
		displayVoltage5->module = module;
		raijuFrameBuffer->addChild(displayVoltage5);

		if (module)
			displayVoltage5->displayText = &module->strVoltages[4];

		SanguineMatrixDisplay* displayVoltage6 = new SanguineMatrixDisplay(8);
		displayVoltage6->box.pos = mm2px(Vec(73.664, 47.591));
		displayVoltage6->module = module;
		raijuFrameBuffer->addChild(displayVoltage6);

		if (module)
			displayVoltage6->displayText = &module->strVoltages[5];

		SanguineMatrixDisplay* displayVoltage7 = new SanguineMatrixDisplay(8);
		displayVoltage7->box.pos = mm2px(Vec(73.664, 67.279));
		displayVoltage7->module = module;
		raijuFrameBuffer->addChild(displayVoltage7);

		if (module)
			displayVoltage7->displayText = &module->strVoltages[6];

		SanguineMatrixDisplay* displayVoltage8 = new SanguineMatrixDisplay(8);
		displayVoltage8->box.pos = mm2px(Vec(73.664, 86.968));
		displayVoltage8->module = module;
		raijuFrameBuffer->addChild(displayVoltage8);

		if (module)
			displayVoltage8->displayText = &module->strVoltages[7];

		SanguineShapedLight* channelsLight = new SanguineShapedLight();
		channelsLight->box.pos = mm2px(Vec(125.219, 16.15));
		channelsLight->wrap();
		channelsLight->module = module;
		channelsLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/channels_lit.svg")));
		raijuFrameBuffer->addChild(channelsLight);

		SanguineShapedLight* outLight = new SanguineShapedLight();
		outLight->box.pos = mm2px(Vec(79.456, 102.594));
		outLight->wrap();
		outLight->module = module;
		outLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/out_light.svg")));
		raijuFrameBuffer->addChild(outLight);

		SanguineShapedLight* bloodLight = new SanguineShapedLight();
		bloodLight->box.pos = mm2px(Vec(4.718, 105.882));
		bloodLight->wrap();
		bloodLight->module = module;
		bloodLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/blood_glowy.svg")));
		raijuFrameBuffer->addChild(bloodLight);

		SanguineShapedLight* monstersLight = new SanguineShapedLight();
		monstersLight->box.pos = mm2px(Vec(12.04, 113.849));
		monstersLight->wrap();
		monstersLight->module = module;
		monstersLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/monsters_lit.svg")));
		raijuFrameBuffer->addChild(monstersLight);
	}
};

Model* modelRaiju = createModel<Raiju, RaijuWidget>("Sanguine-Monsters-Raiju");
