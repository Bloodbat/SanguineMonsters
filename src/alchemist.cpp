#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"
#include "sanguinedsp.hpp"
#include "sanguinejson.hpp"

#include "alchemist.hpp"
#ifndef METAMODULE
#include "alembic.hpp"
#include "crucible.hpp"
#endif

struct Alchemist : SanguineModule {

	enum ParamIds {
		ENUMS(PARAM_GAIN, PORT_MAX_CHANNELS),
		PARAM_MIX,
		ENUMS(PARAM_MUTE, PORT_MAX_CHANNELS),
		ENUMS(PARAM_SOLO, PORT_MAX_CHANNELS),
		PARAM_MASTER_MUTE,
		PARAMS_COUNT
	};

	enum InputIds {
		INPUT_POLYPHONIC,
		INPUT_MIX_CV,
		INPUT_MASTER_MUTE,
		INPUTS_COUNT
	};

	enum OutputIds {
		OUTPUT_MONO_MIX,
		OUTPUT_POLYPHONIC_MIX,
		OUTPUTS_COUNT
	};

	enum LightIds {
		ENUMS(LIGHT_GAIN, 2 * PORT_MAX_CHANNELS),
		ENUMS(LIGHT_MUTE, PORT_MAX_CHANNELS),
		ENUMS(LIGHT_SOLO, PORT_MAX_CHANNELS),
		ENUMS(LIGHT_VU, 4),
#ifndef METAMODULE
		LIGHT_EXPANDER_RIGHT,
		LIGHT_EXPANDER_LEFT,
#endif
		LIGHT_MASTER_MUTE,
		LIGHTS_COUNT
	};

	const int kLightFrequency = 512;

	int soloCount = 0;

	int exclusiveMuteChannel = -1;
	int exclusiveSoloChannel = -1;

#ifndef METAMODULE
	int expanderMuteCount = 0;
	int expanderSoloCount = 0;
#endif

	bool mutedChannels[PORT_MAX_CHANNELS] = {};
	bool bLastAllMuted = false;

	bool soloedChannels[PORT_MAX_CHANNELS] = {};
	bool bLastAllSoloed = false;

#ifndef METAMODULE
	bool bLastHaveExpanderMuteCv = false;
	bool bLastHaveExpanderSoloCv = false;

	bool bHasRightExpander = false;
	bool bHasLeftExpander = false;
#endif

	bool bMuteAllEnabled = false;
	bool bSoloAllEnabled = false;

	bool bMuteExclusiveEnabled = false;
	bool bSoloExclusiveEnabled = false;

	dsp::ClockDivider lightsDivider;
	dsp::VuMeter2 vuMeterMix;
	dsp::VuMeter2 vuMetersGains[PORT_MAX_CHANNELS];
	dsp::BooleanTrigger btMuteButtons[PORT_MAX_CHANNELS];
	dsp::BooleanTrigger btSoloButtons[PORT_MAX_CHANNELS];

	float muteVoltages[PORT_MAX_CHANNELS] = {};
	float soloVoltages[PORT_MAX_CHANNELS] = {};

	SaturatorFloat saturatorFloat;

	Alchemist() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
			int channelNumber = channel + 1;
			configParam(PARAM_GAIN + channel, 0.f, 1.f, 0.f, string::f("Channel %d gain", channelNumber), "%", 0.f, 100.f);
			configButton(PARAM_MUTE + channel, string::f("Channel %d mute", channelNumber));
			configButton(PARAM_SOLO + channel, string::f("Channel %d solo", channelNumber));
		}

		configInput(INPUT_MASTER_MUTE, "Master mute CV");
		configButton(PARAM_MASTER_MUTE, "Master mute");

		configInput(INPUT_MIX_CV, "Master mix CV");
		configParam(PARAM_MIX, 0.f, 1.f, 1.f, "Master mix", "%", 0.f, 100.f);


		configInput(INPUT_POLYPHONIC, "Polyphonic");

		configOutput(OUTPUT_POLYPHONIC_MIX, "Polyphonic mix");
		configOutput(OUTPUT_MONO_MIX, "Monophonic mix");

		configBypass(INPUT_POLYPHONIC, OUTPUT_POLYPHONIC_MIX);

		lightsDivider.setDivision(kLightFrequency);
	}

	void process(const ProcessArgs& args) override {
		float monoMix = 0.f;
		float outVoltages[PORT_MAX_CHANNELS] = {};
		float masterOutVoltages[PORT_MAX_CHANNELS] = {};
		bool bMasterMuted = static_cast<bool>(params[PARAM_MASTER_MUTE].getValue()) || inputs[INPUT_MASTER_MUTE].getVoltage() >= 1.f;
		float mixModulation = clamp(params[PARAM_MIX].getValue() + inputs[INPUT_MIX_CV].getVoltage() / 5.f, 0.f, 2.f);

#ifndef METAMODULE
		Module* alembicExpander = getRightExpander().module;
		Module* crucibleExpander = getLeftExpander().module;
#endif

		int channelCount = inputs[INPUT_POLYPHONIC].getChannels();

		bool bIsLightsTurn = lightsDivider.process();

#ifndef METAMODULE
		bHasRightExpander = (alembicExpander && alembicExpander->getModel() == modelAlembic && !alembicExpander->isBypassed());
		bHasLeftExpander = (crucibleExpander && crucibleExpander->getModel() == modelCrucible && !crucibleExpander->isBypassed());

		bool bHaveExpanderMuteCv = false;
		bool bHaveExpanderSoloCv = false;
#endif

#ifndef METAMODULE
		if (bHasLeftExpander) {
			bMuteExclusiveEnabled = static_cast<bool>(crucibleExpander->getParam(Crucible::PARAM_MUTE_EXCLUSIVE).getValue());
			bSoloExclusiveEnabled = static_cast<bool>(crucibleExpander->getParam(Crucible::PARAM_SOLO_EXCLUSIVE).getValue());

			bMuteAllEnabled = !bMuteExclusiveEnabled && (crucibleExpander->getParam(Crucible::PARAM_MUTE_ALL).getValue() ||
				crucibleExpander->getInput(Crucible::INPUT_MUTE_ALL).getVoltage() >= 1.f);
			bSoloAllEnabled = bSoloExclusiveEnabled == false && (crucibleExpander->getParam(Crucible::PARAM_SOLO_ALL).getValue() ||
				crucibleExpander->getInput(Crucible::INPUT_SOLO_ALL).getVoltage() >= 1.f);

			if (bMuteExclusiveEnabled) {
				crucibleExpander->getParam(Crucible::PARAM_MUTE_ALL).setValue(0.f);
			}

			if (bSoloExclusiveEnabled) {
				crucibleExpander->getParam(Crucible::PARAM_SOLO_ALL).setValue(0.f);
			}

			expanderMuteCount = crucibleExpander->getInput(Crucible::INPUT_MUTE_POLY).getChannels();
			expanderSoloCount = crucibleExpander->getInput(Crucible::INPUT_SOLO_POLY).getChannels();

			bHaveExpanderMuteCv = expanderMuteCount > 0;
			bHaveExpanderSoloCv = expanderSoloCount > 0;

			if (expanderMuteCount > 0) {
				crucibleExpander->getInput(Crucible::INPUT_MUTE_POLY).readVoltages(muteVoltages);
			}

			if (expanderSoloCount > 0) {
				crucibleExpander->getInput(Crucible::INPUT_SOLO_POLY).readVoltages(soloVoltages);
			}
		}
#endif

		if (bIsLightsTurn) {
			bool bIgnoreMuteAll = false;
			bool bIgnoreSoloAll = false;

			for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
				if (btMuteButtons[channel].process(params[PARAM_MUTE + channel].getValue())) {
					mutedChannels[channel] = !mutedChannels[channel];

					if (soloedChannels[channel]) {
						soloedChannels[channel] = false;
					}
#ifndef METAMODULE
					if (bHasLeftExpander) {
						if ((bMuteAllEnabled && bLastAllMuted) && !mutedChannels[channel]) {
							bMuteAllEnabled = false;
							crucibleExpander->getParam(Crucible::PARAM_MUTE_ALL).setValue(0.f);
							bIgnoreMuteAll = true;
						}

						if ((bSoloAllEnabled && bLastAllSoloed)) {
							bSoloAllEnabled = false;
							crucibleExpander->getParam(Crucible::PARAM_SOLO_ALL).setValue(0.f);
							bIgnoreSoloAll = true;
						}

						if (exclusiveMuteChannel != channel) {
							exclusiveMuteChannel = channel;
						} else {
							exclusiveMuteChannel = -1;
						}
					}
#endif
				}
#ifndef METAMODULE
				if (expanderMuteCount > 0 && channel < channelCount) {
					mutedChannels[channel] = muteVoltages[channel] >= 1.f;
					exclusiveMuteChannel = -1;
				}
#endif

				if (btSoloButtons[channel].process(params[PARAM_SOLO + channel].getValue())) {
					soloedChannels[channel] = !soloedChannels[channel];

					if (mutedChannels[channel]) {
						mutedChannels[channel] = false;
					}

#ifndef METAMODULE
					if (bHasLeftExpander) {
						if ((bMuteAllEnabled && bLastAllMuted)) {
							bMuteAllEnabled = false;
							crucibleExpander->getParam(Crucible::PARAM_MUTE_ALL).setValue(0.f);
							bIgnoreMuteAll = true;
						}

						if ((bSoloAllEnabled && bLastAllSoloed) && !soloedChannels[channel]) {
							bSoloAllEnabled = false;
							crucibleExpander->getParam(Crucible::PARAM_SOLO_ALL).setValue(0.f);
							bIgnoreSoloAll = true;
						}

						if (exclusiveSoloChannel != channel) {
							exclusiveSoloChannel = channel;
						} else {
							exclusiveSoloChannel = -1;
						}
					}
#endif
				}
#ifndef METAMODULE
				if (expanderSoloCount > 0 && channel < channelCount) {
					soloedChannels[channel] = soloVoltages[channel] >= 1.f;
					exclusiveSoloChannel = -1;
				}
#endif
			}

			soloCount = 0;
			for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
#ifndef METAMODULE
				if (bHasLeftExpander) {
					if (!bMuteExclusiveEnabled && !bIgnoreMuteAll && ((bLastAllMuted != bMuteAllEnabled) ||
						(bLastHaveExpanderMuteCv != bHaveExpanderMuteCv))) {
						mutedChannels[channel] = bMuteAllEnabled;
					}

					if (!bSoloExclusiveEnabled && !bIgnoreSoloAll && ((bLastAllSoloed != bSoloAllEnabled) ||
						(bLastHaveExpanderSoloCv != bHaveExpanderSoloCv))) {
						soloedChannels[channel] = bSoloAllEnabled;
					}

					if (bMuteExclusiveEnabled) {
						if (channel != exclusiveMuteChannel && exclusiveMuteChannel >= 0) {
							mutedChannels[channel] = false;
						}
					}

					if (bSoloExclusiveEnabled) {
						if (channel != exclusiveSoloChannel && exclusiveSoloChannel >= 0) {
							soloedChannels[channel] = false;
						}
					}
				}
#endif

				if (soloedChannels[channel]) {
					++soloCount;
				}

				if (soloedChannels[channel]) {
					if (mutedChannels[channel]) {
						mutedChannels[channel] = false;
					}
				}
			}

#ifndef METAMODULE
			if (bHasLeftExpander) {
				if (bLastAllMuted != bMuteAllEnabled) {
					bLastAllMuted = bMuteAllEnabled;

					if (bMuteAllEnabled) {
						memset(mutedChannels, true, sizeof(bool) * PORT_MAX_CHANNELS);
						memset(soloedChannels, false, sizeof(bool) * PORT_MAX_CHANNELS);
					}

					if (bSoloAllEnabled) {
						bSoloAllEnabled = false;
						bLastAllSoloed = false;
						crucibleExpander->getParam(Crucible::PARAM_SOLO_ALL).setValue(0.f);
					}
				}

				if (bLastAllSoloed != bSoloAllEnabled) {
					bLastAllSoloed = bSoloAllEnabled;

					if (bSoloAllEnabled) {
						memset(soloedChannels, true, sizeof(bool) * PORT_MAX_CHANNELS);
						memset(mutedChannels, false, sizeof(bool) * PORT_MAX_CHANNELS);
					}

					if (bMuteAllEnabled) {
						bMuteAllEnabled = false;
						bLastAllMuted = false;
						crucibleExpander->getParam(Crucible::PARAM_MUTE_ALL).setValue(0.f);
					}
				}
			}

			bLastHaveExpanderMuteCv = bHaveExpanderMuteCv;
			bLastHaveExpanderSoloCv = bHaveExpanderSoloCv;
#endif
		}

		inputs[INPUT_POLYPHONIC].readVoltages(outVoltages);
		for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
			if (channel < channelCount) {
#ifndef METAMODULE
				if (!bHasRightExpander) {
					outVoltages[channel] = outVoltages[channel] * params[PARAM_GAIN + channel].getValue();
				} else {
					outVoltages[channel] = outVoltages[channel] * clamp(params[PARAM_GAIN + channel].getValue() +
						alembicExpander->getInput(Alembic::INPUT_GAIN_CV + channel).getVoltage() / 5.f, 0.f, 2.f);
				}
#else
				outVoltages[channel] = outVoltages[channel] * params[PARAM_GAIN + channel].getValue();
#endif

				if (std::fabs(outVoltages[channel]) >= 10.f) {
					outVoltages[channel] = saturatorFloat.next(outVoltages[channel]);
				}

				if (!mutedChannels[channel] && (soloCount == 0 || soloedChannels[channel]) && !bMasterMuted) {
					monoMix += outVoltages[channel];
					masterOutVoltages[channel] = outVoltages[channel] * mixModulation;
				} else {
					masterOutVoltages[channel] = 0.f;
				}

#ifndef METAMODULE
				if (bHasRightExpander) {
					Output& output = alembicExpander->getOutput(Alembic::OUTPUT_CHANNEL + channel);
					if (output.isConnected()) {
						output.setVoltage(masterOutVoltages[channel]);
					}
				}
#endif
			} else {
				outVoltages[channel] = 0.f;
#ifndef METAMODULE
				if (bHasRightExpander) {
					Output& output = alembicExpander->getOutput(Alembic::OUTPUT_CHANNEL + channel);
					if (output.isConnected()) {
						output.setVoltage(0.f);
					}
				}
#endif
			}
		}

		monoMix = monoMix * mixModulation;

		if (std::fabs(monoMix) >= 10.1f) {
			monoMix = saturatorFloat.next(monoMix);
		}

		if (outputs[OUTPUT_MONO_MIX].isConnected()) {
			outputs[OUTPUT_MONO_MIX].setVoltage(monoMix);
		}

		if (outputs[OUTPUT_POLYPHONIC_MIX].isConnected()) {
			outputs[OUTPUT_POLYPHONIC_MIX].setChannels(channelCount);
			outputs[OUTPUT_POLYPHONIC_MIX].writeVoltages(masterOutVoltages);
		}

		if (bIsLightsTurn) {
			const float sampleTime = kLightFrequency * args.sampleTime;

			for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
				vuMetersGains[channel].process(sampleTime, outVoltages[channel] / 10.f);

				int currentLight = LIGHT_GAIN + channel * 2;
				float redValue = vuMetersGains[channel].getBrightness(0.f, 0.f);
				float yellowValue = vuMetersGains[channel].getBrightness(-3.f, -1.f);
				float greenValue = vuMetersGains[channel].getBrightness(-38.f, -1.f);
				bool bLightIsRed = redValue > 0;

				if (bLightIsRed) {
					lights[currentLight + 0].setBrightness(0.f);
					lights[currentLight + 1].setBrightness(redValue);
				} else {
					lights[currentLight + 0].setBrightness(greenValue);
					lights[currentLight + 1].setBrightness(yellowValue);
				}

				lights[LIGHT_MUTE + channel].setBrightnessSmooth(mutedChannels[channel] ?
					kSanguineButtonLightValue : 0.f, sampleTime);
				lights[LIGHT_SOLO + channel].setBrightnessSmooth(soloedChannels[channel] ?
					kSanguineButtonLightValue : 0.f, sampleTime);
			}
			vuMeterMix.process(sampleTime, monoMix / 10);
			lights[LIGHT_VU].setBrightness(vuMeterMix.getBrightness(-38.f, -19.f));
			lights[LIGHT_VU + 1].setBrightness(vuMeterMix.getBrightness(-19.f, -3.f));
			lights[LIGHT_VU + 2].setBrightness(vuMeterMix.getBrightness(-3.f, -1.f));
			lights[LIGHT_VU + 3].setBrightness(vuMeterMix.getBrightness(0.f, 0.f));

#ifndef METAMODULE
			lights[LIGHT_EXPANDER_RIGHT].setBrightnessSmooth(bHasRightExpander ? kSanguineButtonLightValue : 0.f, sampleTime);
			lights[LIGHT_EXPANDER_LEFT].setBrightnessSmooth(bHasLeftExpander ? kSanguineButtonLightValue : 0.f, sampleTime);
#endif

			lights[LIGHT_MASTER_MUTE].setBrightnessSmooth(bMasterMuted ? kSanguineButtonLightValue : 0.f, sampleTime);

#ifndef METAMODULE
			if (bHasLeftExpander) {
				crucibleExpander->getLight(Crucible::LIGHT_MUTE_ALL).setBrightnessSmooth(bMuteAllEnabled ?
					kSanguineButtonLightValue : 0.f, sampleTime);
				crucibleExpander->getLight(Crucible::LIGHT_SOLO_ALL).setBrightnessSmooth(bSoloAllEnabled ?
					kSanguineButtonLightValue : 0.f, sampleTime);
				crucibleExpander->getLight(Crucible::LIGHT_MUTE_EXCLUSIVE).setBrightnessSmooth(bMuteExclusiveEnabled ?
					kSanguineButtonLightValue : 0.f, sampleTime);
				crucibleExpander->getLight(Crucible::LIGHT_SOLO_EXCLUSIVE).setBrightnessSmooth(bSoloExclusiveEnabled ?
					kSanguineButtonLightValue : 0.f, sampleTime);
			}
#endif
		}
	}

#ifndef METAMODULE
	void onBypass(const BypassEvent& e) override {
		if (bHasRightExpander) {
			Module* alembicExpander = getRightExpander().module;
			alembicExpander->getLight(Alembic::LIGHT_MASTER_MODULE).setBrightness(0.f);
		}

		if (bHasLeftExpander) {
			Module* crucibleExpander = getLeftExpander().module;
			crucibleExpander->getLight(Crucible::LIGHT_MASTER_MODULE).setBrightness(0.f);
		}
		Module::onBypass(e);
	}

	void onUnBypass(const UnBypassEvent& e) override {
		if (bHasRightExpander) {
			Module* alembicExpander = getRightExpander().module;
			alembicExpander->getLight(Alembic::LIGHT_MASTER_MODULE).setBrightness(kSanguineButtonLightValue);
		}

		if (bHasLeftExpander) {
			Module* crucibleExpander = getLeftExpander().module;
			crucibleExpander->getLight(Crucible::LIGHT_MASTER_MODULE).setBrightness(kSanguineButtonLightValue);
		}
		Module::onUnBypass(e);
	}

	void onExpanderChange(const ExpanderChangeEvent& e) override {
		Module* crucibleExpander = getLeftExpander().module;
		bool bHasLeftExpander = crucibleExpander && crucibleExpander->getModel() == modelCrucible;

		if (!bHasLeftExpander) {
			exclusiveMuteChannel = -1;
			exclusiveSoloChannel = -1;

			bMuteAllEnabled = false;
			bSoloAllEnabled = false;
			bMuteExclusiveEnabled = false;
			bSoloExclusiveEnabled = false;

			for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
				muteVoltages[channel] = 0.f;
				soloVoltages[channel] = 0.f;
			}

			expanderMuteCount = 0;
			expanderSoloCount = 0;
		}
	}
#endif

	json_t* dataToJson() override {
		json_t* rootJ = SanguineModule::dataToJson();

		json_t* mutedChannelsJ = json_array();
		json_t* soloedChannelsJ = json_array();

		for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
			json_array_insert_new(mutedChannelsJ, channel, json_boolean(mutedChannels[channel]));
			json_array_insert_new(soloedChannelsJ, channel, json_boolean(soloedChannels[channel]));
		}

		json_object_set_new(rootJ, "mutedChannels", mutedChannelsJ);
		json_object_set_new(rootJ, "soloedChannels", soloedChannelsJ);

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		SanguineModule::dataFromJson(rootJ);

		json_t* mutedChannelsJ = json_object_get(rootJ, "mutedChannels");
		json_t* soloedChannelsJ = json_object_get(rootJ, "soloedChannels");

		if (mutedChannelsJ || soloedChannelsJ) {
			for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
				if (mutedChannelsJ) {
					json_t* mutedJ = json_array_get(mutedChannelsJ, channel);

					if (mutedJ) {
						mutedChannels[channel] = json_boolean_value(mutedJ);
					}
				}

				if (soloedChannelsJ) {
					json_t* soloedJ = json_array_get(soloedChannelsJ, channel);

					if (soloedJ) {
						soloedChannels[channel] = json_boolean_value(soloedJ);
					}
				}
			}
		}
	}
};

struct AlchemistWidget : SanguineModuleWidget {
	explicit AlchemistWidget(Alchemist* module) {
		setModule(module);

		moduleName = "alchemist";
		panelSize = SIZE_23;
		backplateColor = PLATE_PURPLE;
		bFaceplateSuffix = false;

		makePanel();

		addScrews(SCREW_ALL);

#ifndef METAMODULE
		addChild(createLightCentered<SmallLight<OrangeLight>>(millimetersToPixelsVec(2.6, 5.573), module,
			Alchemist::LIGHT_EXPANDER_LEFT));
		addChild(createLightCentered<SmallLight<OrangeLight>>(millimetersToPixelsVec(114.24, 5.573), module,
			Alchemist::LIGHT_EXPANDER_RIGHT));
#endif

		static const int componentIdOffset = 8;

		static const float componentBaseX = 8.329;
		static const float componentDeltaX = 14.307;

		float componentX = componentBaseX;

		for (int component = 0; component < 8; ++component) {
			addParam(createLightParamCentered<VCVLightSlider<GreenRedLight>>(millimetersToPixelsVec(componentX, 27.245),
				module, Alchemist::PARAM_GAIN + component, Alchemist::LIGHT_GAIN + component * 2));
			addParam(createLightParamCentered<VCVLightSlider<GreenRedLight>>(millimetersToPixelsVec(componentX, 72.578),
				module, Alchemist::PARAM_GAIN + component + componentIdOffset, (Alchemist::LIGHT_GAIN + component + componentIdOffset) * 2));

			addParam(createLightParamCentered<VCVLightBezel<RedLight>>(millimetersToPixelsVec(componentX, 45.411),
				module, Alchemist::PARAM_MUTE + component, Alchemist::LIGHT_MUTE + component));
			addParam(createLightParamCentered<VCVLightBezel<RedLight>>(millimetersToPixelsVec(componentX, 90.749),
				module, Alchemist::PARAM_MUTE + component + componentIdOffset, Alchemist::LIGHT_MUTE + component + componentIdOffset));

			addParam(createLightParamCentered<VCVLightBezel<GreenLight>>(millimetersToPixelsVec(componentX, 54.249),
				module, Alchemist::PARAM_SOLO + component, Alchemist::LIGHT_SOLO + component));
			addParam(createLightParamCentered<VCVLightBezel<GreenLight>>(millimetersToPixelsVec(componentX, 99.6),
				module, Alchemist::PARAM_SOLO + component + componentIdOffset, Alchemist::LIGHT_SOLO + component + componentIdOffset));

			componentX += componentDeltaX;
		}

#ifndef METAMODULE
		SanguinePolyInputLight* inLight = new SanguinePolyInputLight(module, 7.876, 108.973);
		addChild(inLight);

		SanguineShapedLight* bloodLogo = new SanguineShapedLight(module, "res/blood_glowy_small.svg", 16.419, 112.181);
		addChild(bloodLogo);

		SanguineStaticRGBLight* sumLight = new SanguineStaticRGBLight(module, "res/sum_light_on.svg", 56.096, 107.473, true, kSanguineBlueLight);
		addChild(sumLight);

		SanguinePolyOutputLight* outPolyLight = new SanguinePolyOutputLight(module, 95.442, 108.973);
		addChild(outPolyLight);

		SanguineMonoOutputLight* outMonoLight = new SanguineMonoOutputLight(module, 108.963, 108.973);
		addChild(outMonoLight);
#endif

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(31.615, 114.094), module, Alchemist::INPUT_MASTER_MUTE));

		addParam(createParamCentered<SanguineBezel115>(millimetersToPixelsVec(44.332, 114.094), module,
			Alchemist::PARAM_MASTER_MUTE));
		addChild(createLightCentered<SanguineBezelLight115<RedLight>>(millimetersToPixelsVec(44.332, 114.094), module,
			Alchemist::LIGHT_MASTER_MUTE));

		addInput(createInputCentered<BananutBlack>(millimetersToPixelsVec(57.048, 114.094), module, Alchemist::INPUT_MIX_CV));

		addParam(createParamCentered<Davies1900hRedKnob>(millimetersToPixelsVec(71.373, 114.094), module, Alchemist::PARAM_MIX));

		addChild(createLightCentered<MediumLight<GreenLight>>(millimetersToPixelsVec(84.329, 120.785), module, Alchemist::LIGHT_VU));
		addChild(createLightCentered<MediumLight<GreenLight>>(millimetersToPixelsVec(84.329, 116.729), module, Alchemist::LIGHT_VU + 1));
		addChild(createLightCentered<MediumLight<YellowLight>>(millimetersToPixelsVec(84.329, 112.652), module, Alchemist::LIGHT_VU + 2));
		addChild(createLightCentered<MediumLight<RedLight>>(millimetersToPixelsVec(84.329, 108.55), module, Alchemist::LIGHT_VU + 3));

		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(7.876, 116.769), module, Alchemist::INPUT_POLYPHONIC));

		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(95.442, 116.769), module, Alchemist::OUTPUT_POLYPHONIC_MIX));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(108.963, 116.769), module, Alchemist::OUTPUT_MONO_MIX));
	}

#ifndef METAMODULE
	void appendContextMenu(Menu* menu) override {
		SanguineModuleWidget::appendContextMenu(menu);

		Alchemist* alchemist = dynamic_cast<Alchemist*>(this->module);

		menu->addChild(new MenuSeparator());
		const Module* leftExpander = alchemist->leftExpander.module;
		if (leftExpander && leftExpander->model == modelCrucible) {
			menu->addChild(createMenuLabel("Crucible expander already connected"));
		} else {
			menu->addChild(createMenuItem("Add Crucible expander", "", [=]() {
				alchemist->addExpander(modelCrucible, this, SanguineModule::EXPANDER_LEFT);
				}));
		}

		menu->addChild(new MenuSeparator());
		const Module* rightExpander = alchemist->rightExpander.module;
		if (rightExpander && rightExpander->model == modelAlembic) {
			menu->addChild(createMenuLabel("Alembic expander already connected"));
		} else {
			menu->addChild(createMenuItem("Add Alembic expander", "", [=]() {
				alchemist->addExpander(modelAlembic, this);
				}));
		}
	}
#endif
};

Model* modelAlchemist = createModel<Alchemist, AlchemistWidget>("Sanguine-Alchemist");