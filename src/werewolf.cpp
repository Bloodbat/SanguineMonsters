#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"

struct Werewolf : SanguineModule {

	enum ParamIds {
		PARAM_GAIN,
		PARAM_FOLD,
		PARAMS_COUNT
	};

	enum InputIds {
		INPUT_GAIN,
		INPUT_FOLD,
		INPUT_LEFT,
		INPUT_RIGHT,
		INPUTS_COUNT
	};

	enum OutputIds {
		OUTPUT_LEFT,
		OUTPUT_RIGHT,
		OUTPUTS_COUNT
	};

	enum LightIds {
		ENUMS(LIGHT_EYE_1, 3),
		ENUMS(LIGHT_EYE_2, 3),
		ENUMS(LIGHT_GAIN, 3),
		ENUMS(LIGHT_FOLD, 3),
		LIGHTS_COUNT
	};

	dsp::ClockDivider lightDivider;

	const int kLightFrequency = 64;

	Werewolf() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		configParam(PARAM_GAIN, 0.f, 10.f, 1.f, "Gain");
		configParam(PARAM_FOLD, 0.f, 10.f, 0.f, "Fold");
		configInput(INPUT_LEFT, "Left");
		configInput(INPUT_RIGHT, "Right");
		configInput(INPUT_GAIN, "Gain");
		configInput(INPUT_FOLD, "Fold");
		configOutput(OUTPUT_LEFT, "Left");
		configOutput(OUTPUT_RIGHT, "Right");

		configBypass(INPUT_LEFT, OUTPUT_LEFT);
		configBypass(INPUT_RIGHT, OUTPUT_RIGHT);

		lightDivider.setDivision(kLightFrequency);
	}

	void process(const ProcessArgs& args) override {
		bool bLeftInConnected = inputs[INPUT_LEFT].isConnected();
		bool bRightInConnected = inputs[INPUT_RIGHT].isConnected();
		bool bIsNormalled = true;

		float voltageSumLeft = 0.f;
		float voltageSumRight = 0.f;
		float foldSum = 0.f;
		float gainSum = 0.f;

		bool bIsLightsTurn = lightDivider.process();

		int channelCount = std::max(inputs[INPUT_LEFT].getChannels(), inputs[INPUT_RIGHT].getChannels());

		// Logical XOR
		bIsNormalled = !bLeftInConnected != !bRightInConnected;
		bool bOutputsNormalled = !outputs[OUTPUT_LEFT].isConnected() != !outputs[OUTPUT_RIGHT].isConnected();

		if (channelCount > 0) {
			float fold = params[PARAM_FOLD].getValue();
			float gain = params[PARAM_GAIN].getValue();

			for (int channel = 0; channel < channelCount; ++channel) {
				float voltageInLeft = 0.f;
				float voltageInRight = 0.f;
				float voltageOutLeft = 0.f;
				float voltageOutRight = 0.f;

				float channelFold = clamp(fold + inputs[INPUT_FOLD].getVoltage(channel), 0.f, 10.f);
				float channelGain = clamp(gain + inputs[INPUT_GAIN].getVoltage(channel), 0.f, 20.f);

				gainSum += channelGain;
				foldSum += channelFold;

				outputs[OUTPUT_LEFT].setChannels(channelCount);
				outputs[OUTPUT_RIGHT].setChannels(channelCount);

				if (bLeftInConnected) {
					voltageInLeft = inputs[INPUT_LEFT].getVoltage(channel) * channelGain;
					if (bIsNormalled) {
						voltageInRight = voltageInLeft;
					}
				}
				if (bRightInConnected) {
					voltageInRight = inputs[INPUT_RIGHT].getVoltage(channel) * channelGain;
					if (bIsNormalled) {
						voltageInLeft = voltageInRight;
					}
				}

				// Distortion
				if (bLeftInConnected) {
					doDistortion(voltageInLeft, voltageOutLeft, channelFold);
					if (bIsNormalled) {
						voltageOutRight = voltageOutLeft;
					}
				}

				if (bRightInConnected) {
					doDistortion(voltageInRight, voltageOutRight, channelFold);
					if (bIsNormalled) {
						voltageOutLeft = voltageOutRight;
					}
				}

				float voltageMix = 0.f;

				if (bOutputsNormalled) {
					if (!bIsNormalled) {
						voltageMix = voltageOutLeft + voltageOutRight;
					} else {
						voltageMix = voltageOutLeft;
					}
				}

				if (!bOutputsNormalled) {
					voltageSumLeft += voltageOutLeft;

					if (outputs[OUTPUT_LEFT].isConnected()) {
						outputs[OUTPUT_LEFT].setVoltage(voltageOutLeft, channel);
					}

					if (bIsNormalled) {
						voltageOutRight = voltageOutLeft;
						voltageSumRight += voltageOutLeft;

					}
					if (outputs[OUTPUT_RIGHT].isConnected()) {
						outputs[OUTPUT_RIGHT].setVoltage(voltageOutRight, channel);
					}
					voltageSumRight += voltageOutRight;
				} else {
					if (outputs[OUTPUT_LEFT].isConnected()) {
						outputs[OUTPUT_LEFT].setVoltage(voltageMix, channel);
					}
					if (outputs[OUTPUT_RIGHT].isConnected()) {
						outputs[OUTPUT_RIGHT].setVoltage(voltageMix, channel);
					}
					voltageSumLeft += voltageMix;
					voltageSumRight += voltageMix;
				}
			}
		}

		if (bIsLightsTurn) {
			const float sampleTime = args.sampleTime * kLightFrequency;

			if (channelCount < 2) {
				lights[LIGHT_EYE_1 + 0].setBrightnessSmooth(math::rescale(voltageSumLeft, 0.f, 5.f, 0.f, 1.f), sampleTime);
				lights[LIGHT_EYE_1 + 1].setBrightnessSmooth(0.f, sampleTime);
				lights[LIGHT_EYE_1 + 2].setBrightnessSmooth(0.f, sampleTime);
				if (bIsNormalled) {
					lights[LIGHT_EYE_2].setBrightnessSmooth(math::rescale(voltageSumLeft, 0.f, 5.f, 0.f, 1.f), sampleTime);
					lights[LIGHT_EYE_2 + 1].setBrightnessSmooth(0.f, sampleTime);
					lights[LIGHT_EYE_2 + 2].setBrightnessSmooth(0.f, sampleTime);
				} else {
					lights[LIGHT_EYE_2].setBrightnessSmooth(math::rescale(voltageSumRight, 0.f, 5.f, 0.f, 1.f), sampleTime);
					lights[LIGHT_EYE_2 + 1].setBrightnessSmooth(0.f, sampleTime);
					lights[LIGHT_EYE_2 + 2].setBrightnessSmooth(0.f, sampleTime);
				}

				lights[LIGHT_GAIN + 0].setBrightnessSmooth(0.f, sampleTime);
				lights[LIGHT_GAIN + 1].setBrightnessSmooth(math::rescale(gainSum, 0.f, 20.f, 0.f, 1.f), sampleTime);
				lights[LIGHT_GAIN + 2].setBrightnessSmooth(0.f, sampleTime);

				float rescaledLight = math::rescale(foldSum, 0.f, 10.f, 0.f, 1.f);
				lights[LIGHT_FOLD + 0].setBrightnessSmooth(rescaledLight, sampleTime);
				lights[LIGHT_FOLD + 1].setBrightnessSmooth(rescaledLight, sampleTime);
				lights[LIGHT_FOLD + 2].setBrightnessSmooth(0.f, sampleTime);
			} else {
				float rescaledLight = math::rescale(voltageSumLeft / channelCount, 0.f, 5.f, 0.f, 1.f);
				lights[LIGHT_EYE_1 + 0].setBrightnessSmooth(0.f, sampleTime);
				lights[LIGHT_EYE_1 + 1].setBrightnessSmooth(0.f, sampleTime);
				lights[LIGHT_EYE_1 + 2].setBrightnessSmooth(rescaledLight, sampleTime);
				if (bIsNormalled) {
					lights[LIGHT_EYE_2].setBrightnessSmooth(0.f, sampleTime);
					lights[LIGHT_EYE_2 + 1].setBrightnessSmooth(0.f, sampleTime);
					lights[LIGHT_EYE_2 + 2].setBrightnessSmooth(rescaledLight, sampleTime);
				} else {
					rescaledLight = math::rescale(voltageSumRight / channelCount, 0.f, 5.f, 0.f, 1.f);
					lights[LIGHT_EYE_2].setBrightnessSmooth(0.f, sampleTime);
					lights[LIGHT_EYE_2 + 1].setBrightnessSmooth(0.f, sampleTime);
					lights[LIGHT_EYE_2 + 2].setBrightnessSmooth(rescaledLight, sampleTime);
				}
				rescaledLight = math::rescale(gainSum / channelCount, 0.f, 20.f, 0.f, 1.f);
				lights[LIGHT_GAIN + 0].setBrightnessSmooth(0.f, sampleTime);
				lights[LIGHT_GAIN + 1].setBrightnessSmooth(rescaledLight, sampleTime);
				lights[LIGHT_GAIN + 2].setBrightnessSmooth(rescaledLight, sampleTime);

				rescaledLight = math::rescale(foldSum / channelCount, 0.f, 10.f, 0.f, 1.f);
				lights[LIGHT_FOLD + 0].setBrightnessSmooth(rescaledLight, sampleTime);
				lights[LIGHT_FOLD + 1].setBrightnessSmooth(0.f, sampleTime);
				lights[LIGHT_FOLD + 2].setBrightnessSmooth(rescaledLight, sampleTime);
			}
		}
	}

	inline void doDistortion(float inVoltage, float& outVoltage, const float fold) {
		for (int i = 0; i < 100; ++i) {
			if (inVoltage < -5.f) {
				inVoltage = -5.f + (-inVoltage - 5.f) * fold / 5.f;
			}
			if (inVoltage > 5.f) {
				inVoltage = 5.f - (inVoltage - 5.f) * fold / 5.f;
			}

			if (inVoltage >= -5.f && inVoltage <= 5.f) {
				break;
			}

			if (i == 99) {
				inVoltage = 0.f;
			}
		}
		outVoltage = inVoltage;
	}
};

struct WerewolfWidget : SanguineModuleWidget {
	explicit WerewolfWidget(Werewolf* module) {
		setModule(module);

		moduleName = "werewolf";
		panelSize = SIZE_12;
		backplateColor = PLATE_PURPLE;
		bFaceplateSuffix = false;

		makePanel();

		addScrews(SCREW_ALL);

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(millimetersToPixelsVec(22.879, 39.583), module, Werewolf::LIGHT_EYE_1));
		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(millimetersToPixelsVec(38.602, 39.583), module, Werewolf::LIGHT_EYE_2));

		addParam(createParamCentered<BefacoTinyKnobRed>(millimetersToPixelsVec(11.947, 88.26), module, Werewolf::PARAM_GAIN));

		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(20.026, 83.56), module, Werewolf::LIGHT_GAIN));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(40.948, 83.56), module, Werewolf::LIGHT_FOLD));

		addParam(createParamCentered<BefacoTinyKnobBlack>(millimetersToPixelsVec(49.008, 88.26), module, Werewolf::PARAM_FOLD));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(23.04, 98.047), module, Werewolf::INPUT_GAIN));
		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(37.814, 98.047), module, Werewolf::INPUT_FOLD));

		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(9.866, 112.894), module, Werewolf::INPUT_LEFT));
		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(21.771, 112.894), module, Werewolf::INPUT_RIGHT));

		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(39.083, 112.894), module, Werewolf::OUTPUT_LEFT));
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(50.988, 112.894), module, Werewolf::OUTPUT_RIGHT));

#ifndef METAMODULE
		SanguinePolyInputLight* inLight = new SanguinePolyInputLight(module, 15.819, 106.451);
		addChild(inLight);

		SanguinePolyOutputLight* outLight = new SanguinePolyOutputLight(module, 45.036, 106.051);
		addChild(outLight);

		SanguineBloodLogoLight* bloodLight = new SanguineBloodLogoLight(module, 30.48, 90.451);
		addChild(bloodLight);
#endif
	}
};

Model* modelWerewolf = createModel<Werewolf, WerewolfWidget>("Sanguine-Werewolf");