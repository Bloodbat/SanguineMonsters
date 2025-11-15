#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"
#include "sanguinejson.hpp"

struct AionKairos : SanguineModule {

    enum ParamIds {
        PARAM_TIMER_1,
        PARAM_TIMER_2,
        PARAM_RESTART_1,
        PARAM_RESTART_2,
        PARAM_START_1,
        PARAM_START_2,
        PARAM_TRIGGER_1,
        PARAM_TRIGGER_2,
        PARAM_RESET_1,
        PARAM_RESET_2,
        PARAMS_COUNT
    };

    enum InputIds {
        INPUT_TRIGGER_1,
        INPUT_TRIGGER_2,
        INPUT_RESET_1,
        INPUT_RESET_2,
        INPUT_RUN_1,
        INPUT_RUN_2,
        INPUTS_COUNT
    };

    enum OutputIds {
        OUTPUT_TRIGGER_1,
        OUTPUT_TRIGGER_2,
        OUTPUTS_COUNT
    };

    enum LightIds {
        LIGHT_TIMER_1,
        LIGHT_TIMER_2,
        LIGHT_START_1,
        LIGHT_START_2,
        LIGHT_TRIGGER_1,
        LIGHT_TRIGGER_2,
        LIGHT_RESET_1,
        LIGHT_RESET_2,
        LIGHT_RESTART_1,
        LIGHT_RESTART_2,
        LIGHTS_COUNT
    };

    static const int kKnobsFrequency = 64;
    static const int kModuleSections = 2;
    int jitteredKnobsFrequency;

    bool timersStarted[kModuleSections] = {};

    bool lastTimerEdges[kModuleSections] = {};
    bool triggerCables[kModuleSections] = {};
    bool outputCables[kModuleSections] = {};

    int setTimerValues[kModuleSections] = {};
    int currentTimerValues[kModuleSections] = {};

    float currentTime = 0.f;

    dsp::ClockDivider knobsDivider;

    dsp::BooleanTrigger btResetButtons[kModuleSections];
    dsp::BooleanTrigger btRunButtons[kModuleSections];
    dsp::BooleanTrigger btTriggerButtons[kModuleSections];
    dsp::SchmittTrigger stResetInputs[kModuleSections];
    dsp::SchmittTrigger stRunInputs[kModuleSections];
    dsp::SchmittTrigger stTriggerInputs[kModuleSections];
    dsp::PulseGenerator pgTimerLights[kModuleSections];
    dsp::PulseGenerator pgTriggerLights[kModuleSections];
    dsp::PulseGenerator pgResetLights[kModuleSections];
    dsp::PulseGenerator pgRunLights[kModuleSections];
    dsp::PulseGenerator pgTriggerOutputs[kModuleSections];

    AionKairos() {
        config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

        for (int section = 0; section < kModuleSections; ++section) {
            int currentNumber = section + 1;

            configParam(PARAM_TIMER_1 + section, 1.f, 99.f, 1.f, string::f("Timer %d", currentNumber));
            paramQuantities[PARAM_TIMER_1 + section]->snapEnabled = true;

            configButton(PARAM_RESTART_1 + section, string::f("Auto restart timer %d on timer end", currentNumber));
            configButton(PARAM_START_1 + section, string::f("Start / stop timer %d", currentNumber));
            configButton(PARAM_TRIGGER_1 + section, string::f("Advance timer %d", currentNumber));
            configButton(PARAM_RESET_1 + section, string::f("Reset timer %d", currentNumber));

            configInput(INPUT_TRIGGER_1 + section, string::f("Advance timer %d", currentNumber));
            configInput(INPUT_RESET_1 + section, string::f("Reset timer %d", currentNumber));
            configInput(INPUT_RUN_1 + section, string::f("Start / stop timer %d", currentNumber));

            configOutput(OUTPUT_TRIGGER_1 + section, string::f("Timer end %d", currentNumber));

            setTimerValues[section] = 1;
            currentTimerValues[section] = 1;
        }
    }

    void process(const ProcessArgs& args) override {

        bool bIsControlsTurn = knobsDivider.process();

        bool bInternalTimerSecond = false;
        currentTime += args.sampleTime;
        if (currentTime > 1.f) {
            currentTime -= 1.f;
            bInternalTimerSecond = true;
        } else if (currentTime < 0.5f) {
            bInternalTimerSecond = true;
        }

        for (int section = 0; section < kModuleSections; ++section) {
            if (bInternalTimerSecond) {
                if (!triggerCables[section]) {
                    if (lastTimerEdges[section] != bInternalTimerSecond) {
                        if (timersStarted[section]) {
                            decreaseTimer(section);
                        }
                        lastTimerEdges[section] = bInternalTimerSecond;
                    }

                    lights[LIGHT_TIMER_1 + section].setBrightnessSmooth(timersStarted[section], args.sampleTime);
                }
            } else {
                if (!triggerCables[section]) {
                    lights[LIGHT_TIMER_1 + section].setBrightnessSmooth(0.f, args.sampleTime);
                }
                lastTimerEdges[section] = bInternalTimerSecond;
            }

            if (stResetInputs[section].process(inputs[INPUT_RESET_1 + section].getVoltage())) {
                pgResetLights[section].trigger(0.05f);
                currentTimerValues[section] = setTimerValues[section];
            }

            if (stRunInputs[section].process(inputs[INPUT_RUN_1 + section].getVoltage()) && currentTimerValues[section] > 0) {
                pgRunLights[section].trigger(0.05f);
                timersStarted[section] = !timersStarted[section];
            }

            if (stTriggerInputs[section].process(inputs[INPUT_TRIGGER_1 + section].getVoltage()) && timersStarted[section]) {
                pgTriggerLights[section].trigger(0.05f);
                pgTimerLights[section].trigger(0.05f);
                decreaseTimer(section);
            }

            if (bIsControlsTurn) {
                const float sampleTime = args.sampleTime * jitteredKnobsFrequency;

                if (btResetButtons[section].process(params[PARAM_RESET_1 + section].getValue())) {
                    pgResetLights[section].trigger(0.05f);
                    currentTimerValues[section] = setTimerValues[section];
                }

                if (btRunButtons[section].process(params[PARAM_START_1 + section].getValue()) && currentTimerValues[section] > 0) {
                    pgRunLights[section].trigger(0.05f);
                    timersStarted[section] = !timersStarted[section];
                }

                if (btTriggerButtons[section].process(params[PARAM_TRIGGER_1 + section].getValue()) && timersStarted[section]) {
                    pgTriggerLights[section].trigger(0.05f);
                    pgTimerLights[section].trigger(0.05f);
                    decreaseTimer(section);
                }

                if (setTimerValues[section] != static_cast<int>(params[PARAM_TIMER_1 + section].getValue())) {
                    setTimerValues[section] = params[PARAM_TIMER_1 + section].getValue();
                    currentTimerValues[section] = params[PARAM_TIMER_1 + section].getValue();
                }

                lights[LIGHT_RESTART_1 + section].setBrightness(params[PARAM_RESTART_1 + section].getValue() *
                    kSanguineButtonLightValue);

                lights[LIGHT_START_1 + section].setBrightnessSmooth(pgRunLights[section].process(sampleTime),
                    sampleTime);

                lights[LIGHT_TRIGGER_1 + section].setBrightnessSmooth(pgTriggerLights[section].process(sampleTime),
                    sampleTime);

                lights[LIGHT_RESET_1 + section].setBrightnessSmooth(pgResetLights[section].process(sampleTime),
                    sampleTime);
            }

            if (outputCables[section]) {
                outputs[OUTPUT_TRIGGER_1 + section].setVoltage(pgTriggerOutputs[section].process(args.sampleTime) * 10.f);
            }

            if (triggerCables[section]) {
                lights[LIGHT_TIMER_1 + section].setBrightnessSmooth(pgTimerLights[section].process(args.sampleTime), args.sampleTime);
            }
        }
    }

    inline void decreaseTimer(int timerNum) {
        --currentTimerValues[timerNum];

        if (currentTimerValues[timerNum] <= 0) {
            if (outputCables[timerNum]) {
                pgTriggerOutputs[timerNum].trigger();
            }
            if (!params[PARAM_RESTART_1 + timerNum].getValue()) {
                timersStarted[timerNum] = false;
                currentTimerValues[timerNum] = 0;
            } else {
                currentTimerValues[timerNum] = setTimerValues[timerNum];
            }
        }
    }

    void onPortChange(const PortChangeEvent& e) override {
        switch (e.type) {
        case Port::INPUT:
            switch (e.portId) {
            case INPUT_TRIGGER_1:
                triggerCables[0] = e.connecting;
                break;
            case INPUT_TRIGGER_2:
                triggerCables[1] = e.connecting;
                break;
            default:
                break;
            }
            break;

        case Port::OUTPUT:
            outputCables[e.portId] = e.connecting;
            break;
        }
    }

    void onAdd(const AddEvent& e) override {
        jitteredKnobsFrequency = kKnobsFrequency + (getId() % kKnobsFrequency);
        knobsDivider.setDivision(jitteredKnobsFrequency);
    }

    json_t* dataToJson() override {
        json_t* rootJ = SanguineModule::dataToJson();

        json_t* timersStartedJ = json_array();
        for (int section = 0; section < kModuleSections; ++section) {
            json_t* timerJ = json_boolean(timersStarted[section]);
            json_array_append_new(timersStartedJ, timerJ);
        }
        json_object_set_new(rootJ, "timersStarted", timersStartedJ);

        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
        SanguineModule::dataFromJson(rootJ);

        json_t* timersStartedJ = json_object_get(rootJ, "timersStarted");
        size_t idx;
        json_t* timerJ;
        json_array_foreach(timersStartedJ, idx, timerJ) {
            timersStarted[idx] = json_boolean_value(timerJ);
        }
    }
};

struct AionKairosWidget : SanguineModuleWidget {
    explicit AionKairosWidget(AionKairos* module) {
        setModule(module);

        moduleName = "aionkairos";
        panelSize = sanguineThemes::SIZE_6;
        backplateColor = sanguineThemes::PLATE_PURPLE;
        bFaceplateSuffix = false;

        makePanel();

        addScrews(SCREW_TOP_LEFT | SCREW_BOTTOM_RIGHT);

        FramebufferWidget* aionKairosFramebuffer = new FramebufferWidget();
        addChild(aionKairosFramebuffer);

        // Timer 1
        SanguineTinyNumericDisplay* displayTimer1 = new SanguineTinyNumericDisplay(2, module, 7.703, 19.173);
        aionKairosFramebuffer->addChild(displayTimer1);
        displayTimer1->fallbackNumber = 1;

        addChild(createLightCentered<SmallLight<RedLight>>(millimetersToPixelsVec(16.603, 19.173), module, AionKairos::LIGHT_TIMER_1));

        addParam(createParamCentered<BefacoTinyKnobRed>(millimetersToPixelsVec(24.481, 19.118), module, AionKairos::PARAM_TIMER_1));

        addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(5.253, 30.516), module, AionKairos::INPUT_RUN_1));
        addParam(createLightParamCentered<VCVLightBezel<PurpleLight>>(millimetersToPixelsVec(24.481, 30.516), module,
            AionKairos::PARAM_START_1, AionKairos::LIGHT_START_1));

        addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(5.253, 40.395), module, AionKairos::INPUT_TRIGGER_1));
        addParam(createLightParamCentered<VCVLightBezel<BlueLight>>(millimetersToPixelsVec(24.481, 40.395), module,
            AionKairos::PARAM_TRIGGER_1, AionKairos::LIGHT_TRIGGER_1));

        addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(5.253, 50.274), module, AionKairos::INPUT_RESET_1));
        addParam(createLightParamCentered<VCVLightBezel<YellowLight>>(millimetersToPixelsVec(24.481, 50.274), module,
            AionKairos::PARAM_RESET_1, AionKairos::LIGHT_RESET_1));

        addParam(createLightParamCentered<VCVLightBezelLatch<GreenLight>>(millimetersToPixelsVec(5.253, 60.257),
            module, AionKairos::PARAM_RESTART_1, AionKairos::LIGHT_RESTART_1));

#ifndef METAMODULE
        SanguineStaticRGBLight* lightOutput1 = new SanguineStaticRGBLight(module, "res/gate_lit.svg",
            17.53, 60.257, true, kSanguineYellowLight);
        addChild(lightOutput1);
#endif

        addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(24.481, 60.257), module, AionKairos::OUTPUT_TRIGGER_1));

        // Timer 2
        SanguineTinyNumericDisplay* displayTimer2 = new SanguineTinyNumericDisplay(2, module, 7.703, 77.351);
        aionKairosFramebuffer->addChild(displayTimer2);
        displayTimer2->fallbackNumber = 1;

        addChild(createLightCentered<SmallLight<RedLight>>(millimetersToPixelsVec(16.603, 77.351), module, AionKairos::LIGHT_TIMER_2));

        addParam(createParamCentered<BefacoTinyKnobBlack>(millimetersToPixelsVec(24.481, 77.185), module, AionKairos::PARAM_TIMER_2));

        addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(5.253, 88.693), module, AionKairos::INPUT_RUN_2));
        addParam(createLightParamCentered<VCVLightBezel<PurpleLight>>(millimetersToPixelsVec(24.481, 88.693), module,
            AionKairos::PARAM_START_2, AionKairos::LIGHT_START_2));

        addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(5.253, 98.573), module, AionKairos::INPUT_TRIGGER_2));
        addParam(createLightParamCentered<VCVLightBezel<BlueLight>>(millimetersToPixelsVec(24.481, 98.573), module,
            AionKairos::PARAM_TRIGGER_2, AionKairos::LIGHT_TRIGGER_2));

        addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(5.253, 108.452), module, AionKairos::INPUT_RESET_2));
        addParam(createLightParamCentered<VCVLightBezel<YellowLight>>(millimetersToPixelsVec(24.481, 108.452), module,
            AionKairos::PARAM_RESET_2, AionKairos::LIGHT_RESET_2));

        addParam(createLightParamCentered<VCVLightBezelLatch<GreenLight>>(millimetersToPixelsVec(5.253, 118.435),
            module, AionKairos::PARAM_RESTART_2, AionKairos::LIGHT_RESTART_2));

#ifndef METAMODULE
        SanguineStaticRGBLight* lightOutput2 = new SanguineStaticRGBLight(module, "res/gate_lit.svg",
            17.53, 118.435, true, kSanguineYellowLight);
        addChild(lightOutput2);
#endif

        addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(24.481, 118.435), module, AionKairos::OUTPUT_TRIGGER_2));

        if (module) {
            displayTimer1->values.numberValue = &module->currentTimerValues[0];
            displayTimer2->values.numberValue = &module->currentTimerValues[1];
        }
    }
};

Model* modelAionKairos = createModel<AionKairos, AionKairosWidget>("Sanguine-Aion-Kairos");