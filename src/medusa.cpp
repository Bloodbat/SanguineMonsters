#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"
#include "medusa.hpp"

using simd::float_4;

struct Medusa : SanguineModule {

	enum ParamIds {
		PARAMS_COUNT
	};

	enum InputIds {
		ENUMS(INPUT_VOLTAGE, kMedusaMaxPorts),
		INPUTS_COUNT
	};

	enum OutputIds {
		ENUMS(OUTPUT_VOLTAGE, kMedusaMaxPorts),
		OUTPUTS_COUNT
	};

	enum LightIds {
		ENUMS(LIGHT_NORMALLED_PORT, kMedusaMaxPorts * 3),
		LIGHTS_COUNT
	};

	const int kLightFrequency = 1024;

	dsp::ClockDivider lightDivider;

	Medusa() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		for (int port = 0; port < kMedusaMaxPorts; ++port) {
			configInput(INPUT_VOLTAGE + port, string::f("Medusa %d", port + 1));
			configOutput(OUTPUT_VOLTAGE + port, string::f("Medusa %d", port + 1));
		}

		lightDivider.setDivision(kLightFrequency);
	}

	void process(const ProcessArgs& args) override {
		int connectedCount = 0;

		int channelCount = 0;
		int activePort = 0;

		int currentPalette = 0;

		bool bIsLightsTurn = lightDivider.process();

		for (int port = 0; port < kMedusaMaxPorts; ++port) {
			if (inputs[INPUT_VOLTAGE + port].isConnected()) {
				channelCount = inputs[INPUT_VOLTAGE + port].getChannels();
				activePort = port;
				currentPalette = (currentPalette + 1);

				if (currentPalette > 4) {
					currentPalette = 0;
				}

				++connectedCount;
			}

			if (outputs[OUTPUT_VOLTAGE + port].isConnected()) {
				outputs[port].setChannels(channelCount);

				for (int channel = 0; channel < channelCount; channel += 4) {
					float_4 voltages = inputs[activePort].getVoltageSimd<float_4>(channel);
					outputs[OUTPUT_VOLTAGE + port].setVoltageSimd(voltages, channel);
				}
			}

			if (bIsLightsTurn) {
				const float sampleTime = kLightFrequency * args.sampleTime;

				int currentLight = LIGHT_NORMALLED_PORT + port * 3;
				if (connectedCount > 0) {
					lights[currentLight + 0].setBrightnessSmooth(paletteMedusaLights[currentPalette].red, sampleTime);
					lights[currentLight + 1].setBrightnessSmooth(paletteMedusaLights[currentPalette].green, sampleTime);
					lights[currentLight + 2].setBrightnessSmooth(paletteMedusaLights[currentPalette].blue, sampleTime);
				} else {
					lights[currentLight + 0].setBrightnessSmooth(0.f, sampleTime);
					lights[currentLight + 1].setBrightnessSmooth(0.f, sampleTime);
					lights[currentLight + 2].setBrightnessSmooth(0.f, sampleTime);
				}
			}
		}
	}
};

struct MedusaWidget : SanguineModuleWidget {
	MedusaWidget(Medusa* module) {
		setModule(module);

		moduleName = "medusa";
		panelSize = SIZE_27;
		backplateColor = PLATE_PURPLE;
		bFaceplateSuffix = false;

		makePanel();

		addScrews(SCREW_ALL);

		SanguinePolyInputLight* lightInput1 = new SanguinePolyInputLight(module, 8.119, 22.162);
		addChild(lightInput1);

		SanguinePolyOutputLight* lightOutput1 = new SanguinePolyOutputLight(module, 24.625, 22.162);
		addChild(lightOutput1);

		float xInputs = 8.119;
		float xOutputs = 24.625;
		float xLights = 16.378;

		float yPortBase = 29.326;
		const float yDelta = 9.827;

		float yLightBase = 29.326;

		float currentPortY = yPortBase;
		float currentLightY = yLightBase;

		int portOffset = 0;

		for (int component = 0; component < 10; ++component) {
			addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(xInputs, currentPortY), module,
				Medusa::INPUT_VOLTAGE + portOffset + component));
			addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(xOutputs, currentPortY), module,
				Medusa::OUTPUT_VOLTAGE + portOffset + component));

			addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(xLights, currentLightY), module,
				(Medusa::LIGHT_NORMALLED_PORT + component + portOffset) * 3));

			currentPortY += yDelta;
			currentLightY += yDelta;
		}

		SanguinePolyInputLight* lightInput2 = new SanguinePolyInputLight(module, 42.912, 22.162);
		addChild(lightInput2);

		SanguinePolyOutputLight* lightOutput2 = new SanguinePolyOutputLight(module, 59.418, 22.162);
		addChild(lightOutput2);

		xInputs = 42.912;
		xOutputs = 59.418;
		xLights = 51.171;

		currentPortY = yPortBase;
		currentLightY = yLightBase;

		portOffset = 10;

		for (int component = 0; component < 6; ++component) {
			addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(xInputs, currentPortY),
				module, Medusa::INPUT_VOLTAGE + portOffset + component));
			addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(xOutputs, currentPortY),
				module, Medusa::OUTPUT_VOLTAGE + portOffset + component));

			addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(xLights, currentLightY),
				module, (Medusa::LIGHT_NORMALLED_PORT + component + portOffset) * 3));

			currentPortY += yDelta;
			currentLightY += yDelta;
		}

		SanguinePolyInputLight* lightInput3 = new SanguinePolyInputLight(module, 77.742, 22.162);
		addChild(lightInput3);

		SanguinePolyOutputLight* lightOutput3 = new SanguinePolyOutputLight(module, 94.248, 22.162);
		addChild(lightOutput3);

		xInputs = 77.742;
		xOutputs = 94.248;
		xLights = 86.001;

		currentPortY = yPortBase;
		currentLightY = yLightBase;

		portOffset = 16;

		for (int component = 0; component < 6; ++component) {
			addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(xInputs, currentPortY),
				module, Medusa::INPUT_VOLTAGE + portOffset + component));
			addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(xOutputs, currentPortY),
				module, Medusa::OUTPUT_VOLTAGE + portOffset + component));

			addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(xLights, currentLightY),
				module, (Medusa::LIGHT_NORMALLED_PORT + component + portOffset) * 3));

			currentPortY += yDelta;
			currentLightY += yDelta;
		}

		SanguinePolyInputLight* lightInput4 = new SanguinePolyInputLight(module, 112.535, 22.162);
		addChild(lightInput4);

		SanguinePolyOutputLight* lightOutput4 = new SanguinePolyOutputLight(module, 129.041, 22.162);
		addChild(lightOutput4);

		xInputs = 112.535;
		xOutputs = 129.041;
		xLights = 120.794;

		currentPortY = yPortBase;
		currentLightY = yLightBase;

		portOffset = 22;

		for (int component = 0; component < 10; ++component) {
			addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(xInputs, currentPortY),
				module, Medusa::INPUT_VOLTAGE + portOffset + component));
			addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(xOutputs, currentPortY),
				module, Medusa::OUTPUT_VOLTAGE + portOffset + component));

			addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(xLights, currentLightY),
				module, (Medusa::LIGHT_NORMALLED_PORT + component + portOffset) * 3));

			currentPortY += yDelta;
			currentLightY += yDelta;
		}

		SanguineBloodLogoLight* bloodLight = new SanguineBloodLogoLight(module, 58.816, 110.201);
		addChild(bloodLight);

		SanguineMonstersLogoLight* monstersLight = new SanguineMonstersLogoLight(module, 71.948, 117.156);
		addChild(monstersLight);
	}
};

Model* modelMedusa = createModel<Medusa, MedusaWidget>("Sanguine-Medusa");