#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"
#include "sanguinechannels.hpp"
#include "sanguinedsp.hpp"
#include "sanguinejson.hpp"

#include "fortuna.hpp"

struct Fortuna : SanguineModule {
    enum ParamIds {
        PARAM_THRESHOLD_1,
        PARAM_THRESHOLD_2,
        PARAM_ROLL_MODE_1,
        PARAM_ROLL_MODE_2,
        PARAM_CROSSFADE_A,
        PARAM_CROSSFADE_B,
        PARAMS_COUNT
    };
    enum InputIds {
        INPUT_IN_1,
        INPUT_IN_2,
        INPUT_P_1,
        INPUT_P_2,
        INPUT_TRIGGER_1,
        INPUT_TRIGGER_2,
        INPUTS_COUNT
    };
    enum OutputIds {
        OUTPUT_OUT_1A,
        OUTPUT_OUT_2A,
        OUTPUT_OUT_1B,
        OUTPUT_OUT_2B,
        OUTPUTS_COUNT
    };
    enum LightIds {
        LIGHT_GATE_STATE_1_A,
        LIGHT_GATE_STATE_1_B,
        LIGHT_GATE_STATE_2_A,
        LIGHT_GATE_STATE_2_B,
        ENUMS(LIGHTS_ROLL_MODE, 2 * 2),
        ENUMS(LIGHTS_PROBABILITY, 2 * 2),
        LIGHTS_COUNT
    };

    static const int kLightFrequency = 16;
    static const int kMaxModuleSections = 2;
    int ledsChannel = 0;
    int channelCount = 0;

    dsp::BooleanTrigger btGateTriggers[kMaxModuleSections][PORT_MAX_CHANNELS];
    dsp::ClockDivider lightsDivider;
    RampGenerator rampGenerators[kMaxModuleSections][PORT_MAX_CHANNELS];

    fortuna::RollResults rollResults[kMaxModuleSections][PORT_MAX_CHANNELS] = {};
    fortuna::RollResults lastRollResults[kMaxModuleSections][PORT_MAX_CHANNELS] = {};

    fortuna::RollModes rollModes[kMaxModuleSections] = { fortuna::ROLL_DIRECT, fortuna::ROLL_DIRECT };
    bool outputsConnected[OUTPUTS_COUNT] = {};

    Fortuna() {
        config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);
        for (int section = 0; section < kMaxModuleSections; ++section) {
            configParam(PARAM_THRESHOLD_1 + section, 1.f, 0.f, 0.5f, string::f("Channel %d probability", section + 1),
                "%", 0, 100);
            configParam(PARAM_CROSSFADE_A + section, 0.f, 10.f, 0.f, string::f("Crossfade %d time", section + 1), " s");
            configSwitch(PARAM_ROLL_MODE_1 + section, 0.f, 1.f, 0.f, string::f("Channel %d coin mode", section + 1), { "Direct", "Toggle" });
            configInput(INPUT_IN_1 + section, string::f("Channel %d signal", section + 1));
            configInput(INPUT_TRIGGER_1 + section, string::f("Channel %d trigger", section + 1));
            configInput(INPUT_P_1 + section, string::f("Channel %d probability", section + 1));
            configOutput(OUTPUT_OUT_1A + section, string::f("Channel %d A", section + 1));
            configOutput(OUTPUT_OUT_1B + section, string::f("Channel %d B", section + 1));
            for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
                lastRollResults[section][channel] = fortuna::ROLL_HEADS;
            }
            lightsDivider.setDivision(kLightFrequency);
        }
    }

    void process(const ProcessArgs& args) override {
        outputsConnected[0] = outputs[OUTPUT_OUT_1A].isConnected();
        outputsConnected[1] = outputs[OUTPUT_OUT_2A].isConnected();
        outputsConnected[2] = outputs[OUTPUT_OUT_1B].isConnected();
        outputsConnected[3] = outputs[OUTPUT_OUT_2B].isConnected();

        float inVoltages[kMaxModuleSections][PORT_MAX_CHANNELS] = {};
        float cvVoltages[kMaxModuleSections][PORT_MAX_CHANNELS] = {};
        bool bLightsTurn = lightsDivider.process();

        for (int section = 0; section < kMaxModuleSections; ++section) {
            // Set input & trigger ports.
            Input* input = &inputs[INPUT_IN_1 + section];
            Input* trigger = &inputs[INPUT_TRIGGER_1 + section];

            // 2nd input and 2 trigger are normalized to 1st.
            if (section == 1 && !input->isConnected()) {
                input = &inputs[INPUT_IN_1 + 0];
            }

            if (section == 1 && !trigger->isConnected()) {
                trigger = &inputs[INPUT_TRIGGER_1 + 0];
            }

            channelCount = std::max(std::max(input->getChannels(), trigger->getChannels()), 1);

            rollModes[section] = static_cast<fortuna::RollModes>(params[PARAM_ROLL_MODE_1 + section].getValue());

            float rampDuration = params[PARAM_CROSSFADE_A + section].getValue();

            // Process triggers.
            for (int channel = 0; channel < channelCount; ++channel) {
                cvVoltages[section][channel] = inputs[INPUT_P_1 + section].getVoltage(channel);

                bool bGatePresent = trigger->getVoltage(channel) >= 2.f;
                if (btGateTriggers[section][channel].process(bGatePresent)) {
                    // Trigger.
                    float threshold = clamp(params[PARAM_THRESHOLD_1 + section].getValue() + cvVoltages[section][channel] / 5.f, 0.f, 1.f);
                    rollResults[section][channel] = (random::uniform() >= threshold) ? fortuna::ROLL_HEADS : fortuna::ROLL_TAILS;
                    if (rollModes[section] == fortuna::ROLL_TOGGLE) {
                        rollResults[section][channel] =
                            static_cast<fortuna::RollResults>(lastRollResults[section][channel] ^ rollResults[section][channel]);
                    }
                    if (lastRollResults[section][channel] != rollResults[section][channel]) {
                        rampGenerators[section][channel].trigger(rampDuration);
                    }
                    lastRollResults[section][channel] = rollResults[section][channel];
                }

                rampGenerators[section][channel].process(args.sampleTime);

                // Set output signals
                inVoltages[section][channel] = input->getVoltage(channel);

                float fadingOutVoltage = inVoltages[section][channel] * (clamp(1.f - rampGenerators[section][channel].rampVoltage, 0.f, 1.f));
                float fadingInVoltage = inVoltages[section][channel] * rampGenerators[section][channel].rampVoltage;

                outputs[OUTPUT_OUT_1A + section].setVoltage(rollResults[section][channel] != fortuna::ROLL_TAILS ?
                    fadingInVoltage : fadingOutVoltage, channel);

                outputs[OUTPUT_OUT_1B + section].setVoltage(rollResults[section][channel] == fortuna::ROLL_TAILS ?
                    fadingInVoltage : fadingOutVoltage, channel);
            }

            if (outputsConnected[0 + section]) {
                outputs[OUTPUT_OUT_1A + section].setChannels(channelCount);
            }
            if (outputsConnected[2 + section]) {
                outputs[OUTPUT_OUT_1B + section].setChannels(channelCount);
            }

            if (bLightsTurn) {
                if (ledsChannel >= channelCount) {
                    ledsChannel = channelCount - 1;
                }

                const float sampleTime = args.sampleTime * kLightFrequency;
                int currentLight = LIGHT_GATE_STATE_1_A + section * 2;
                float lightValueA = outputs[OUTPUT_OUT_1A + section].getVoltage(ledsChannel);
                lightValueA = rescale(lightValueA, 0.f, 5.f, 0.f, 1.f);
                lights[currentLight + 0].setSmoothBrightness(lightValueA, sampleTime);
                float lightValueB = outputs[OUTPUT_OUT_1B + section].getVoltage(ledsChannel);
                lightValueB = rescale(lightValueB, 0.f, 5.f, 0.f, 1.f);
                lights[currentLight + 1].setSmoothBrightness(lightValueB, sampleTime);

                currentLight = LIGHTS_PROBABILITY + section * 2;
                float rescaledLight = rescale(cvVoltages[section][ledsChannel], 0.f, 5.f, 0.f, 1.f);
                lights[currentLight + 1].setSmoothBrightness(-rescaledLight, sampleTime);
                lights[currentLight + 0].setSmoothBrightness(rescaledLight, sampleTime);

                currentLight = LIGHTS_ROLL_MODE + section * 2;
                lights[currentLight + 0].setBrightnessSmooth(rollModes[section] == fortuna::ROLL_DIRECT ?
                    kSanguineButtonLightValue : 0.f, sampleTime);
                lights[currentLight + 1].setBrightnessSmooth(rollModes[section] == fortuna::ROLL_DIRECT ?
                    0.f : kSanguineButtonLightValue, sampleTime);
            }
        }
    }

    void onReset() override {
        for (int section = 0; section < kMaxModuleSections; ++section) {
            params[PARAM_ROLL_MODE_1 + section].setValue(0);
            for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
                lastRollResults[section][channel] = fortuna::ROLL_HEADS;
            }
        }
    }

    json_t* dataToJson() override {
        json_t* rootJ = SanguineModule::dataToJson();

        setJsonInt(rootJ, "ledsChannel", ledsChannel);

        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
        SanguineModule::dataFromJson(rootJ);

        json_int_t intValue;

        if (getJsonInt(rootJ, "ledsChannel", intValue)) {
            ledsChannel = intValue;
        }
    }
};

struct FortunaWidget : SanguineModuleWidget {
    explicit FortunaWidget(Fortuna* module) {
        setModule(module);

        moduleName = "fortuna";
        panelSize = SIZE_8;
        backplateColor = PLATE_PURPLE;
        bFaceplateSuffix = false;

        makePanel();


        addScrews(SCREW_ALL);

        // Section 1
#ifndef METAMODULE
        SanguinePolyInputLight* inLight1 = new SanguinePolyInputLight(module, 6.413, 19.362);
        addChild(inLight1);

        SanguineStaticRGBLight* triggerLight1 = new SanguineStaticRGBLight(module,
            "res/trigger_in_lit.svg", 6.413, 51.36, true, kSanguineBlueLight);
        addChild(triggerLight1);
#endif

        addChild(createLightCentered<SmallLight<RedLight>>(millimetersToPixelsVec(34.212, 20.521), module,
            Fortuna::LIGHT_GATE_STATE_1_A));

        addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(6.413, 26.411), module, Fortuna::INPUT_IN_1));
        addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(34.212, 26.411), module, Fortuna::OUTPUT_OUT_1A));
        addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(20.32, 14.368), module,
            Fortuna::PARAM_ROLL_MODE_1, Fortuna::LIGHTS_ROLL_MODE + 0 * 2));
        addParam(createLightParamCentered<VCVLightSlider<GreenRedLight>>(millimetersToPixelsVec(20.32, 35.367), module,
            Fortuna::Fortuna::PARAM_THRESHOLD_1, Fortuna::LIGHTS_PROBABILITY + 0 * 2));
        addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(34.212, 35.014), module, Fortuna::PARAM_CROSSFADE_A));
        addInput(createInputCentered<BananutBlackPoly>(millimetersToPixelsVec(6.413, 44.323), module, Fortuna::INPUT_TRIGGER_1));
        addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(20.32, 53.515), module, Fortuna::INPUT_P_1));
        addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(34.212, 44.323), module, Fortuna::OUTPUT_OUT_1B));

        addChild(createLightCentered<SmallLight<GreenLight>>(millimetersToPixelsVec(34.212, 50.01), module,
            Fortuna::LIGHT_GATE_STATE_1_B));

        // Section 2
#ifndef METAMODULE
        SanguinePolyInputLight* inLight2 = new SanguinePolyInputLight(module, 6.413, 78.759);
        addChild(inLight2);

        SanguineStaticRGBLight* triggerLight2 = new SanguineStaticRGBLight(module,
            "res/trigger_in_lit.svg", 6.413, 110.757, true, kSanguineBlueLight);
        addChild(triggerLight2);
#endif

        addChild(createLightCentered<SmallLight<RedLight>>(millimetersToPixelsVec(34.212, 79.916), module,
            Fortuna::LIGHT_GATE_STATE_2_A));

        addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(6.413, 85.796), module, Fortuna::INPUT_IN_2));
        addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(20.32, 76.605), module, Fortuna::INPUT_P_2));
        addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(34.212, 85.796), module, Fortuna::OUTPUT_OUT_2A));
        addParam(createLightParamCentered<VCVLightSlider<GreenRedLight>>(millimetersToPixelsVec(20.32, 94.753), module,
            Fortuna::Fortuna::PARAM_THRESHOLD_2, Fortuna::LIGHTS_PROBABILITY + 1 * 2));
        addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(34.212, 94.399), module, Fortuna::PARAM_CROSSFADE_B));
        addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(20.288, 115.718), module,
            Fortuna::PARAM_ROLL_MODE_2, Fortuna::LIGHTS_ROLL_MODE + 1 * 2));
        addInput(createInputCentered<BananutBlackPoly>(millimetersToPixelsVec(6.413, 103.709), module, Fortuna::INPUT_TRIGGER_2));
        addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(34.212, 103.709), module, Fortuna::OUTPUT_OUT_2B));

        addChild(createLightCentered<SmallLight<GreenLight>>(millimetersToPixelsVec(34.212, 109.408), module,
            Fortuna::LIGHT_GATE_STATE_2_B));
    }

    void appendContextMenu(Menu* menu) override {
        SanguineModuleWidget::appendContextMenu(menu);

        Fortuna* module = dynamic_cast<Fortuna*>(this->module);

        menu->addChild(new MenuSeparator);

        std::vector<std::string> availableChannels;
        for (int i = 0; i < module->channelCount; ++i) {
            availableChannels.push_back(channelNumbers[i]);
        }
        menu->addChild(createIndexSubmenuItem("LEDs channel", availableChannels,
            [=]() {return module->ledsChannel; },
            [=](int i) {module->ledsChannel = i; }
        ));
    }
};

Model* modelFortuna = createModel<Fortuna, FortunaWidget>("Sanguine-Fortuna");