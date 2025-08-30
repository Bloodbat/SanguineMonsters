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
		ENUMS(INPUT_VOLTAGE, medusa::kMaxPorts),
		INPUTS_COUNT
	};

	enum OutputIds {
		ENUMS(OUTPUT_VOLTAGE, medusa::kMaxPorts),
		OUTPUTS_COUNT
	};

	enum LightIds {
		ENUMS(LIGHT_NORMALLED_PORT, medusa::kMaxPorts * 3),
		LIGHTS_COUNT
	};

	static const int kLightsFrequency = 1024;

	dsp::ClockDivider lightDivider;

	bool inputsConnected[medusa::kMaxPorts] = {};
	bool outputsConnected[medusa::kMaxPorts] = {};

	Medusa() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		for (int port = 0; port < medusa::kMaxPorts; ++port) {
			configInput(INPUT_VOLTAGE + port, string::f("Medusa %d", port + 1));
			configOutput(OUTPUT_VOLTAGE + port, string::f("Medusa %d", port + 1));
		}

		lightDivider.setDivision(kLightsFrequency);
	}

	void process(const ProcessArgs& args) override {
		int connectedCount = 0;

		int channelCount = 0;
		int activePort = 0;

		int currentPalette = 0;

		bool bIsLightsTurn = lightDivider.process();

		for (int port = 0; port < medusa::kMaxPorts; ++port) {
			if (inputsConnected[port]) {
				channelCount = inputs[INPUT_VOLTAGE + port].getChannels();
				activePort = port;
				// TODO: use inc.
				currentPalette = (currentPalette + 1);

				if (currentPalette > 4) {
					currentPalette = 0;
				}

				++connectedCount;
			}

			if (outputsConnected[port]) {
				outputs[port].setChannels(channelCount);

				for (int channel = 0; channel < channelCount; channel += 4) {
					float_4 voltages = inputs[activePort].getVoltageSimd<float_4>(channel);
					outputs[OUTPUT_VOLTAGE + port].setVoltageSimd(voltages, channel);
				}
			}

			if (bIsLightsTurn) {
				const float sampleTime = kLightsFrequency * args.sampleTime;

				int currentLight = LIGHT_NORMALLED_PORT + port * 3;
				if (connectedCount > 0) {
					lights[currentLight + 0].setBrightnessSmooth(medusa::paletteLights[currentPalette].red, sampleTime);
					lights[currentLight + 1].setBrightnessSmooth(medusa::paletteLights[currentPalette].green, sampleTime);
					lights[currentLight + 2].setBrightnessSmooth(medusa::paletteLights[currentPalette].blue, sampleTime);
				} else {
					lights[currentLight + 0].setBrightnessSmooth(0.f, sampleTime);
					lights[currentLight + 1].setBrightnessSmooth(0.f, sampleTime);
					lights[currentLight + 2].setBrightnessSmooth(0.f, sampleTime);
				}
			}
		}
	}

	void onPortChange(const PortChangeEvent& e) override {
		if (e.type == Port::INPUT) {
			inputsConnected[e.portId] = e.connecting;
		} else {
			outputsConnected[e.portId] = e.connecting;
		}
	}
};

struct MedusaWidget : SanguineModuleWidget {
	explicit MedusaWidget(Medusa* module) {
		setModule(module);

		moduleName = "medusa";
		panelSize = SIZE_27;
		backplateColor = PLATE_PURPLE;
		bFaceplateSuffix = false;

		makePanel();

		addScrews(SCREW_ALL);

#ifndef METAMODULE
		SanguinePolyInputLight* lightInput1 = new SanguinePolyInputLight(module, 8.119, 22.162);
		addChild(lightInput1);

		SanguinePolyOutputLight* lightOutput1 = new SanguinePolyOutputLight(module, 24.625, 22.162);
		addChild(lightOutput1);

		SanguinePolyInputLight* lightInput2 = new SanguinePolyInputLight(module, 42.912, 22.162);
		addChild(lightInput2);

		SanguinePolyOutputLight* lightOutput2 = new SanguinePolyOutputLight(module, 59.418, 22.162);
		addChild(lightOutput2);

		SanguinePolyInputLight* lightInput3 = new SanguinePolyInputLight(module, 77.742, 22.162);
		addChild(lightInput3);

		SanguinePolyOutputLight* lightOutput3 = new SanguinePolyOutputLight(module, 94.248, 22.162);
		addChild(lightOutput3);

		SanguinePolyInputLight* lightInput4 = new SanguinePolyInputLight(module, 112.535, 22.162);
		addChild(lightInput4);

		SanguinePolyOutputLight* lightOutput4 = new SanguinePolyOutputLight(module, 129.041, 22.162);
		addChild(lightOutput4);

		SanguineBloodLogoLight* bloodLight = new SanguineBloodLogoLight(module, 58.816, 110.201);
		addChild(bloodLight);

		SanguineMonstersLogoLight* monstersLight = new SanguineMonstersLogoLight(module, 71.948, 117.156);
		addChild(monstersLight);
#endif

		// 1st column
		createComponentColumn(10, 8.119, 24.625, 16.378, 0);

		// 2nd column
		createComponentColumn(6, 42.912, 59.418, 51.171, 10);

		// 3rd column
		createComponentColumn(6, 77.742, 94.248, 86.001, 16);

		// 4th column
		createComponentColumn(10, 112.535, 129.041, 120.794, 22);
	}

	void createComponentColumn(const int componentCount, const float inputsX,
		const float outputsX, const float lightsX, const int componentOffset) {
		static const float yDelta = 9.827;

		float currentPortY = 29.326;
		float currentLightY = 29.326;

		for (int component = 0; component < componentCount; ++component) {
			addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(inputsX, currentPortY),
				module, Medusa::INPUT_VOLTAGE + componentOffset + component));
			addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(outputsX, currentPortY),
				module, Medusa::OUTPUT_VOLTAGE + componentOffset + component));

			addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(lightsX, currentLightY),
				module, (Medusa::LIGHT_NORMALLED_PORT + component + componentOffset) * 3));

			currentPortY += yDelta;
			currentLightY += yDelta;
		}
	}
};

Model* modelMedusa = createModel<Medusa, MedusaWidget>("Sanguine-Medusa");