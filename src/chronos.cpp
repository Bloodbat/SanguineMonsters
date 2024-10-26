#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"
#include "sanguinechannels.hpp"
#include "chronos.hpp"

using simd::float_4;

struct Chronos : SanguineModule {

    enum ParamIds {
        PARAM_FREQUENCY_1,
        PARAM_FREQUENCY_2,
        PARAM_FREQUENCY_3,
        PARAM_FREQUENCY_4,
        PARAM_PULSEWIDTH_1,
        PARAM_PULSEWIDTH_2,
        PARAM_PULSEWIDTH_3,
        PARAM_PULSEWIDTH_4,
        PARAM_INVERT_1,
        PARAM_INVERT_2,
        PARAM_INVERT_3,
        PARAM_INVERT_4,
        PARAM_BIPOLAR_1,
        PARAM_BIPOLAR_2,
        PARAM_BIPOLAR_3,
        PARAM_BIPOLAR_4,
        PARAM_FM_1,
        PARAM_FM_2,
        PARAM_FM_3,
        PARAM_FM_4,
        PARAM_PWM_1,
        PARAM_PWM_2,
        PARAM_PWM_3,
        PARAM_PWM_4,
        PARAMS_COUNT
    };

    enum InputIds {
        INPUT_CLOCK_1,
        INPUT_CLOCK_2,
        INPUT_CLOCK_3,
        INPUT_CLOCK_4,
        INPUT_RESET_1,
        INPUT_RESET_2,
        INPUT_RESET_3,
        INPUT_RESET_4,
        INPUT_FM_1,
        INPUT_FM_2,
        INPUT_FM_3,
        INPUT_FM_4,
        INPUT_PWM_1,
        INPUT_PWM_2,
        INPUT_PWM_3,
        INPUT_PWM_4,
        INPUTS_COUNT
    };

    enum OutputIds {
        OUTPUT_SINE_1,
        OUTPUT_SINE_2,
        OUTPUT_SINE_3,
        OUTPUT_SINE_4,
        OUTPUT_TRIANGLE_1,
        OUTPUT_TRIANGLE_2,
        OUTPUT_TRIANGLE_3,
        OUTPUT_TRIANGLE_4,
        OUTPUT_SAW_1,
        OUTPUT_SAW_2,
        OUTPUT_SAW_3,
        OUTPUT_SAW_4,
        OUTPUT_SQUARE_1,
        OUTPUT_SQUARE_2,
        OUTPUT_SQUARE_3,
        OUTPUT_SQUARE_4,
        OUTPUTS_COUNT
    };

    enum LightIds {
        ENUMS(LIGHT_PHASE_1, 3),
        ENUMS(LIGHT_PHASE_2, 3),
        ENUMS(LIGHT_PHASE_3, 3),
        ENUMS(LIGHT_PHASE_4, 3),
        LIGHT_INVERT_1,
        LIGHT_INVERT_2,
        LIGHT_INVERT_3,
        LIGHT_INVERT_4,
        LIGHT_BIPOLAR_1,
        LIGHT_BIPOLAR_2,
        LIGHT_BIPOLAR_3,
        LIGHT_BIPOLAR_4,
        LIGHTS_COUNT
    };

    static const int kLightsDivision = 64;

    int channelCounts[kMaxSections] = {};
    int ledsChannel[kMaxSections] = {};

    float clockFrequencies[kMaxSections] = {};

    float_4 phases[kMaxSections][4];
    float_4 sineVoltages[kMaxSections][4];

    dsp::ClockDivider lightsDivider;
    dsp::Timer clockTimers[kMaxSections];
    dsp::TSchmittTrigger<float_4> stResetTriggers[kMaxSections][4];
    dsp::SchmittTrigger stClockTriggers[kMaxSections];

    struct FrequencyParam : ParamQuantity {
        float getDisplayValue() override {
            Chronos* moduleChronos = dynamic_cast<Chronos*>(module);
            if (paramId >= PARAM_FREQUENCY_1 && paramId <= PARAM_FREQUENCY_4) {
                switch (paramId)
                {
                case PARAM_FREQUENCY_1:
                    if (moduleChronos->clockFrequencies[PARAM_FREQUENCY_1] == 2.f) {
                        unit = " Hz";
                        displayMultiplier = 1.f;
                    } else {
                        unit = "x";
                        displayMultiplier = 1 / 2.f;
                    }
                    break;
                case PARAM_FREQUENCY_2:
                    if (moduleChronos->clockFrequencies[PARAM_FREQUENCY_2] == 2.f) {
                        unit = " Hz";
                        displayMultiplier = 1.f;
                    } else {
                        unit = "x";
                        displayMultiplier = 1 / 2.f;
                    }
                    break;
                case PARAM_FREQUENCY_3:
                    if (moduleChronos->clockFrequencies[PARAM_FREQUENCY_3] == 2.f) {
                        unit = " Hz";
                        displayMultiplier = 1.f;
                    } else {
                        unit = "x";
                        displayMultiplier = 1 / 2.f;
                    }
                    break;
                case PARAM_FREQUENCY_4:
                    if (moduleChronos->clockFrequencies[PARAM_FREQUENCY_4] == 2.f) {
                        unit = " Hz";
                        displayMultiplier = 1.f;
                    } else {
                        unit = "x";
                        displayMultiplier = 1 / 2.f;
                    }
                    break;
                }
            }
            return ParamQuantity::getDisplayValue();
        }
    };

    Chronos() {
        config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

        for (int section = 0; section < kMaxSections; ++section) {
            int lfoNumber = section + 1;
            configParam<FrequencyParam>(PARAM_FREQUENCY_1 + section, -8.f, 10.f, 1.f,
                string::f("LFO %d frequency", lfoNumber), " Hz", 2, 1);

            configInput(INPUT_CLOCK_1 + section, string::f("LFO %d clock", lfoNumber));
            configButton(PARAM_INVERT_1 + section, string::f("LFO %d invert", lfoNumber));
            configButton(PARAM_BIPOLAR_1 + section, string::f("LFO %d bipolar", lfoNumber));

            configLight(LIGHT_PHASE_1 + section * 3, string::f("LFO %d phase", lfoNumber));
            configParam(PARAM_FM_1 + section, -1.f, 1.f, 0.f,
                string::f("LFO %d frequency modulation", lfoNumber), "%", 0.f, 100.f);
            getParamQuantity(PARAM_FM_1 + section)->randomizeEnabled = false;
            configParam(PARAM_PWM_1 + section, -1.f, 1.f, 0.f,
                string::f("LFO %d Pulse width modulation", lfoNumber), "%", 0.f, 100.f);

            configParam(PARAM_PULSEWIDTH_1 + section, 0.01f, 0.99f, 0.5f,
                string::f("LFO %d Pulse width", lfoNumber), "%", 0.f, 100.f);
            configInput(INPUT_RESET_1 + section, string::f("LFO %d reset", lfoNumber));
            configInput(INPUT_FM_1 + section, string::f("LFO %d FM", lfoNumber));
            configInput(INPUT_PWM_1 + section, string::f("LFO %d pulse width modulation", lfoNumber));

            configOutput(OUTPUT_SINE_1 + section, string::f("LFO %d sine", lfoNumber));
            configOutput(OUTPUT_TRIANGLE_1 + section, string::f("LFO %d triangle", lfoNumber));
            configOutput(OUTPUT_SAW_1 + section, string::f("LFO %d sawtooth", lfoNumber));
            configOutput(OUTPUT_SQUARE_1 + section, string::f("LFO %d square", lfoNumber));
        }

        onReset();
        lightsDivider.setDivision(kLightsDivision);
    };

    void process(const ProcessArgs& args) override {
        bool bIsLightsTurn = lightsDivider.process();

        for (int section = 0; section < kMaxSections; ++section) {
            float paramFrequency = params[PARAM_FREQUENCY_1 + section].getValue();
            float paramFm = params[PARAM_FM_1 + section].getValue();
            float paramPulsewidth = params[PARAM_PULSEWIDTH_1 + section].getValue();
            float paramPwm = params[PARAM_PWM_1 + section].getValue();
            bool bHasOffset = !(static_cast<bool>(params[PARAM_BIPOLAR_1 + section].getValue()));
            bool bIsInverted = static_cast<bool>(params[PARAM_INVERT_1 + section].getValue());

            // Clocks
            if (inputs[INPUT_CLOCK_1 + section].isConnected()) {
                clockTimers[section].process(args.sampleTime);

                if (stClockTriggers[section].process(inputs[INPUT_CLOCK_1 + section].getVoltage(), 0.1f, 2.f)) {
                    float clockFrequency = 1.f / clockTimers[section].getTime();
                    clockTimers[section].reset();
                    if (0.001f <= clockFrequency && clockFrequency <= 1000.f) {
                        clockFrequencies[section] = clockFrequency;
                    }
                }
            } else {
                clockFrequencies[section] = 2.f;
            }

            channelCounts[section] = std::max(inputs[INPUT_FM_1 + section].getChannels(), 1);

            for (uint8_t channel = 0; channel < channelCounts[section]; channel += 4) {
                uint8_t currentChannel = channel >> 2;

                // Pitch and frequency
                float_4 pitch = paramFrequency;
                pitch += inputs[INPUT_FM_1 + section].getVoltageSimd<float_4>(channel) * paramFm;
                float_4 frequency = clockFrequencies[section] / 2.f * dsp::exp2_taylor5(pitch);

                // Pulse width
                float_4 pulseWidth = paramPulsewidth;
                pulseWidth += inputs[INPUT_PWM_1 + section].getPolyVoltageSimd<float_4>(channel) / 10.f * paramPwm;
                pulseWidth = clamp(pulseWidth, 0.01f, 0.99f);

                // Advance phase
                float_4 deltaPhase = simd::fmin(frequency * args.sampleTime, 0.5f);
                phases[section][currentChannel] += deltaPhase;
                phases[section][currentChannel] -= simd::trunc(phases[section][currentChannel]);

                // Reset
                float_4 reset = inputs[INPUT_RESET_1 + section].getPolyVoltageSimd<float_4>(channel);
                float_4 resetTriggered = stResetTriggers[section][currentChannel].process(reset, 0.1f, 2.f);
                phases[section][currentChannel] = simd::ifelse(resetTriggered, 0.f, phases[section][currentChannel]);

                // Sine
                if (outputs[OUTPUT_SINE_1 + section].isConnected() || bIsLightsTurn) {
                    float_4 phase = phases[section][currentChannel];
                    if (bHasOffset) {
                        phase -= 0.25f;
                    }
                    sineVoltages[section][currentChannel] = simd::sin(2.f * M_PI * phase);
                    if (outputs[OUTPUT_SINE_1 + section].isConnected()) {
                        float_4 voltage = sineVoltages[section][currentChannel];
                        if (bIsInverted) {
                            voltage = -voltage;
                        }
                        if (bHasOffset) {
                            voltage += 1.f;
                        }
                        outputs[OUTPUT_SINE_1 + section].setVoltageSimd(5.f * voltage, channel);
                    }
                }

                // Triangle
                if (outputs[OUTPUT_TRIANGLE_1 + section].isConnected()) {
                    float_4 phase = phases[section][currentChannel];
                    if (!bHasOffset) {
                        phase += 0.25f;
                    }
                    float_4 voltage = 4.f * simd::fabs(phase - simd::round(phase)) - 1.f;
                    if (bIsInverted) {
                        voltage = -voltage;
                    }
                    if (bHasOffset) {
                        voltage += 1.f;
                    }
                    outputs[OUTPUT_TRIANGLE_1 + section].setVoltageSimd(5.f * voltage, channel);
                }

                // Sawtooth
                if (outputs[OUTPUT_SAW_1 + section].isConnected()) {
                    float_4 phase = phases[section][currentChannel];
                    if (bHasOffset) {
                        phase -= 0.5f;
                    }
                    float_4 voltage = 2.f * (phase - simd::round(phase));
                    if (bIsInverted) {
                        voltage = -voltage;
                    }
                    if (bHasOffset) {
                        voltage += 1.f;
                    }
                    outputs[OUTPUT_SAW_1 + section].setVoltageSimd(5.f * voltage, channel);
                }

                // Square
                if (outputs[OUTPUT_SQUARE_1 + section].isConnected()) {
                    float_4 voltage = simd::ifelse(phases[section][currentChannel] < pulseWidth, 1.f, -1.f);
                    if (bIsInverted) {
                        voltage = -voltage;
                    }
                    if (bHasOffset) {
                        voltage += 1.f;
                    }
                    outputs[OUTPUT_SQUARE_1 + section].setVoltageSimd(5.f * voltage, channel);
                }
            }

            outputs[OUTPUT_SINE_1 + section].setChannels(channelCounts[section]);
            outputs[OUTPUT_TRIANGLE_1 + section].setChannels(channelCounts[section]);
            outputs[OUTPUT_SAW_1 + section].setChannels(channelCounts[section]);
            outputs[OUTPUT_SQUARE_1 + section].setChannels(channelCounts[section]);

            if (bIsLightsTurn) {
                if (ledsChannel[section] >= channelCounts[section]) {
                    ledsChannel[section] = channelCounts[section] - 1;
                }

                const float sampleTime = args.sampleTime * kLightsDivision;
                int currentLight = LIGHT_PHASE_1 + section * 3;
                if (channelCounts[section] == 1) {
                    lights[currentLight + 0].setBrightnessSmooth(-sineVoltages[section][0][0], sampleTime);
                    lights[currentLight + 1].setBrightnessSmooth(sineVoltages[section][0][0], sampleTime);
                    lights[currentLight + 2].setBrightnessSmooth(0.f, sampleTime);
                } else {
                    float brightness = sineVoltages[section][ledsChannel[section] >> 2][ledsChannel[section] % 4];
                    lights[currentLight + 0].setBrightnessSmooth(-brightness, sampleTime);
                    lights[currentLight + 1].setBrightnessSmooth(brightness, sampleTime);
                    lights[currentLight + 2].setBrightnessSmooth(fabsf(brightness), sampleTime);
                }
                lights[LIGHT_BIPOLAR_1 + section].setBrightnessSmooth(!bHasOffset ? 0.75f : 0.f, sampleTime);
                lights[LIGHT_INVERT_1 + section].setBrightnessSmooth(bIsInverted ? 0.75f : 0.f, sampleTime);
            }
        }
    }

    void onReset() override {
        float newPhase = 0.f;
        // Offset each phase by 90º
        const float phaseOffset = 0.25f;
        for (int section = 0; section < kMaxSections; ++section) {
            for (int channel = 0; channel < 16; channel += 4) {
                phases[section][channel >> 2] = newPhase;
            }
            newPhase += phaseOffset;
            clockFrequencies[section] = 1.f;
            clockTimers[section].reset();
        }
    }

    json_t* dataToJson() override {
        json_t* rootJ = SanguineModule::dataToJson();

        for (int section = 0; section < kMaxSections; ++section) {
            json_object_set_new(rootJ, string::f("ledsChannel%d", section).c_str(), json_integer(ledsChannel[section]));
        }
        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
        SanguineModule::dataFromJson(rootJ);

        for (int section = 0; section < kMaxSections; ++section) {
            json_t* ledsChannelJ = json_object_get(rootJ, string::f("ledsChannel%d", section).c_str());
            if (ledsChannelJ) {
                ledsChannel[section] = json_integer_value(ledsChannelJ);
            }
        }
    }
};

struct ChronosWidget : SanguineModuleWidget {
    ChronosWidget(Chronos* module) {
        setModule(module);

        moduleName = "chronos";
        panelSize = SIZE_22;
        backplateColor = PLATE_PURPLE;
        bFaceplateSuffix = false;

        makePanel();

        addScrews(SCREW_ALL);

        // LFO 1
        SanguineStaticRGBLight* lightClock1 = new SanguineStaticRGBLight(module, "res/clock_lit.svg", 22.771, 13.18, true, kSanguineBlueLight);
        addChild(lightClock1);

        addParam(createParamCentered<Davies1900hRedKnob>(millimetersToPixelsVec(9.645, 24.53), module, Chronos::PARAM_FREQUENCY_1));

        addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(22.771, 20.33), module, Chronos::INPUT_CLOCK_1));
        addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedLight>>>(millimetersToPixelsVec(34.212, 20.33),
            module, Chronos::PARAM_INVERT_1, Chronos::LIGHT_INVERT_1));
        addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(millimetersToPixelsVec(45.652, 20.33),
            module, Chronos::PARAM_BIPOLAR_1, Chronos::LIGHT_BIPOLAR_1));

        addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(22.771, 30.093), module, Chronos::LIGHT_PHASE_1));
        addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(34.212, 29.993), module, Chronos::PARAM_FM_1));
        addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(45.652, 29.993), module, Chronos::PARAM_PWM_1));

        addParam(createParamCentered<BefacoTinyKnobBlack>(millimetersToPixelsVec(9.645, 39.856), module, Chronos::PARAM_PULSEWIDTH_1));
        addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(22.771, 39.856), module, Chronos::INPUT_RESET_1));
        addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(34.212, 39.856), module, Chronos::INPUT_FM_1));
        addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(45.652, 39.856), module, Chronos::INPUT_PWM_1));

        SanguineStaticRGBLight* lightReset1 = new SanguineStaticRGBLight(module, "res/reset_lit.svg", 22.771, 46.545, true, kSanguineBlueLight);
        addChild(lightReset1);

        SanguineStaticRGBLight* lightSine1 = new SanguineStaticRGBLight(module, "res/wave_sine.svg", 7.078, 52.518, true, kSanguineBlueLight);
        addChild(lightSine1);
        SanguineStaticRGBLight* lightTriangle1 = new SanguineStaticRGBLight(module, "res/wave_triangle.svg", 18.519, 52.518, true, kSanguineBlueLight);
        addChild(lightTriangle1);
        SanguineStaticRGBLight* lightSaw1 = new SanguineStaticRGBLight(module, "res/wave_saw.svg", 29.959, 52.518, true, kSanguineBlueLight);
        addChild(lightSaw1);
        SanguineStaticRGBLight* lightSquare1 = new SanguineStaticRGBLight(module, "res/wave_square.svg", 41.4, 52.518, true, kSanguineBlueLight);
        addChild(lightSquare1);

        addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(7.078, 59.167), module, Chronos::OUTPUT_SINE_1));
        addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(18.519, 59.167), module, Chronos::OUTPUT_TRIANGLE_1));
        addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(29.959, 59.167), module, Chronos::OUTPUT_SAW_1));
        addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(41.4, 59.167), module, Chronos::OUTPUT_SQUARE_1));

        // LFO 2
        SanguineStaticRGBLight* lightClock2 = new SanguineStaticRGBLight(module, "res/clock_lit.svg", 77.917, 13.18, true, kSanguineBlueLight);
        addChild(lightClock2);

        addParam(createParamCentered<Davies1900hBlackKnob>(millimetersToPixelsVec(64.791, 24.53), module, Chronos::PARAM_FREQUENCY_2));

        addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(77.917, 20.33), module, Chronos::INPUT_CLOCK_2));
        addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedLight>>>(millimetersToPixelsVec(89.357, 20.33),
            module, Chronos::PARAM_INVERT_2, Chronos::LIGHT_INVERT_2));
        addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(millimetersToPixelsVec(100.798, 20.33),
            module, Chronos::PARAM_BIPOLAR_2, Chronos::LIGHT_BIPOLAR_2));

        addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(77.917, 30.093), module, Chronos::LIGHT_PHASE_2));
        addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(89.357, 29.993), module, Chronos::PARAM_FM_2));
        addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(100.798, 29.993), module, Chronos::PARAM_PWM_2));

        addParam(createParamCentered<BefacoTinyKnobRed>(millimetersToPixelsVec(64.791, 39.856), module, Chronos::PARAM_PULSEWIDTH_2));
        addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(77.917, 39.856), module, Chronos::INPUT_RESET_2));
        addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(89.357, 39.856), module, Chronos::INPUT_FM_2));
        addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(100.798, 39.856), module, Chronos::INPUT_PWM_2));

        SanguineStaticRGBLight* lightReset2 = new SanguineStaticRGBLight(module, "res/reset_lit.svg", 77.917, 46.545, true, kSanguineBlueLight);
        addChild(lightReset2);

        SanguineStaticRGBLight* lightSine2 = new SanguineStaticRGBLight(module, "res/wave_sine.svg", 70.279, 52.518, true, kSanguineBlueLight);
        addChild(lightSine2);
        SanguineStaticRGBLight* lightTriangle2 = new SanguineStaticRGBLight(module, "res/wave_triangle.svg", 81.72, 52.518, true, kSanguineBlueLight);
        addChild(lightTriangle2);
        SanguineStaticRGBLight* lightSaw2 = new SanguineStaticRGBLight(module, "res/wave_saw.svg", 93.161, 52.518, true, kSanguineBlueLight);
        addChild(lightSaw2);
        SanguineStaticRGBLight* lightSquare2 = new SanguineStaticRGBLight(module, "res/wave_square.svg", 104.601, 52.518, true, kSanguineBlueLight);
        addChild(lightSquare2);

        addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(70.279, 59.167), module, Chronos::OUTPUT_SINE_2));
        addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(81.72, 59.167), module, Chronos::OUTPUT_TRIANGLE_2));
        addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(93.161, 59.167), module, Chronos::OUTPUT_SAW_2));
        addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(104.601, 59.167), module, Chronos::OUTPUT_SQUARE_2));

        // LFO 3
        SanguineStaticRGBLight* lightClock3 = new SanguineStaticRGBLight(module, "res/clock_lit.svg", 22.771, 72.663, true, kSanguineBlueLight);
        addChild(lightClock3);

        addParam(createParamCentered<Davies1900hBlackKnob>(millimetersToPixelsVec(9.645, 84.013), module, Chronos::PARAM_FREQUENCY_3));

        addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(22.771, 79.813), module, Chronos::INPUT_CLOCK_3));
        addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedLight>>>(millimetersToPixelsVec(34.212, 79.813),
            module, Chronos::PARAM_INVERT_3, Chronos::LIGHT_INVERT_3));
        addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(millimetersToPixelsVec(45.652, 79.813),
            module, Chronos::PARAM_BIPOLAR_3, Chronos::LIGHT_BIPOLAR_3));

        addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(22.771, 89.576), module, Chronos::LIGHT_PHASE_3));
        addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(34.212, 89.476), module, Chronos::PARAM_FM_3));
        addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(45.652, 89.476), module, Chronos::PARAM_PWM_3));

        addParam(createParamCentered<BefacoTinyKnobRed>(millimetersToPixelsVec(9.645, 99.34), module, Chronos::PARAM_PULSEWIDTH_3));
        addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(22.771, 99.34), module, Chronos::INPUT_RESET_3));
        addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(34.212, 99.34), module, Chronos::INPUT_FM_3));
        addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(45.652, 99.34), module, Chronos::INPUT_PWM_3));

        SanguineStaticRGBLight* lightReset3 = new SanguineStaticRGBLight(module, "res/reset_lit.svg", 22.771, 106.028, true, kSanguineBlueLight);
        addChild(lightReset3);

        SanguineStaticRGBLight* lightSine3 = new SanguineStaticRGBLight(module, "res/wave_sine.svg", 7.078, 112.001, true, kSanguineBlueLight);
        addChild(lightSine3);
        SanguineStaticRGBLight* lightTriangle3 = new SanguineStaticRGBLight(module, "res/wave_triangle.svg", 18.519, 112.001, true, kSanguineBlueLight);
        addChild(lightTriangle3);
        SanguineStaticRGBLight* lightSaw3 = new SanguineStaticRGBLight(module, "res/wave_saw.svg", 29.959, 112.001, true, kSanguineBlueLight);
        addChild(lightSaw3);
        SanguineStaticRGBLight* lightSquare3 = new SanguineStaticRGBLight(module, "res/wave_square.svg", 41.4, 112.001, true, kSanguineBlueLight);
        addChild(lightSquare3);

        addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(7.078, 118.651), module, Chronos::OUTPUT_SINE_3));
        addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(18.519, 118.651), module, Chronos::OUTPUT_TRIANGLE_3));
        addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(29.959, 118.651), module, Chronos::OUTPUT_SAW_3));
        addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(41.4, 118.651), module, Chronos::OUTPUT_SQUARE_3));

        // LFO 4
        SanguineStaticRGBLight* lightClock4 = new SanguineStaticRGBLight(module, "res/clock_lit.svg", 77.917, 72.663, true, kSanguineBlueLight);
        addChild(lightClock4);

        addParam(createParamCentered<Davies1900hRedKnob>(millimetersToPixelsVec(64.791, 84.013), module, Chronos::PARAM_FREQUENCY_4));

        addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(77.917, 79.813), module, Chronos::INPUT_CLOCK_4));
        addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedLight>>>(millimetersToPixelsVec(89.357, 79.813),
            module, Chronos::PARAM_INVERT_4, Chronos::LIGHT_INVERT_4));
        addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(millimetersToPixelsVec(100.798, 79.813),
            module, Chronos::PARAM_BIPOLAR_4, Chronos::LIGHT_BIPOLAR_4));

        addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(77.917, 89.576), module, Chronos::LIGHT_PHASE_4));
        addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(89.357, 89.476), module, Chronos::PARAM_FM_4));
        addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(100.798, 89.476), module, Chronos::PARAM_PWM_4));

        addParam(createParamCentered<BefacoTinyKnobBlack>(millimetersToPixelsVec(64.791, 99.34), module, Chronos::PARAM_PULSEWIDTH_4));
        addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(77.917, 99.34), module, Chronos::INPUT_RESET_4));
        addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(89.357, 99.34), module, Chronos::INPUT_FM_4));
        addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(100.798, 99.34), module, Chronos::INPUT_PWM_4));

        SanguineStaticRGBLight* lightReset4 = new SanguineStaticRGBLight(module, "res/reset_lit.svg", 77.917, 106.028, true, kSanguineBlueLight);
        addChild(lightReset4);

        SanguineStaticRGBLight* lightSine4 = new SanguineStaticRGBLight(module, "res/wave_sine.svg", 70.279, 112.001, true, kSanguineBlueLight);
        addChild(lightSine4);
        SanguineStaticRGBLight* lightTriangle4 = new SanguineStaticRGBLight(module, "res/wave_triangle.svg", 81.72, 112.001, true, kSanguineBlueLight);
        addChild(lightTriangle4);
        SanguineStaticRGBLight* lightSaw4 = new SanguineStaticRGBLight(module, "res/wave_saw.svg", 93.161, 112.001, true, kSanguineBlueLight);
        addChild(lightSaw4);
        SanguineStaticRGBLight* lightSquare4 = new SanguineStaticRGBLight(module, "res/wave_square.svg", 104.601, 112.001, true, kSanguineBlueLight);
        addChild(lightSquare4);

        addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(70.279, 118.651), module, Chronos::OUTPUT_SINE_4));
        addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(81.72, 118.651), module, Chronos::OUTPUT_TRIANGLE_4));
        addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(93.161, 118.651), module, Chronos::OUTPUT_SAW_4));
        addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(104.601, 118.651), module, Chronos::OUTPUT_SQUARE_4));
    }

    void appendContextMenu(Menu* menu) override {
        SanguineModuleWidget::appendContextMenu(menu);

        Chronos* module = dynamic_cast<Chronos*>(this->module);

        menu->addChild(new MenuSeparator);

        std::vector<std::string> availableChannels = {};

        for (int section = 0; section < kMaxSections; ++section) {
            availableChannels.clear();
            for (int channel = 0; channel < module->channelCounts[section]; ++channel) {
                availableChannels.push_back(channelNumbers[channel]);
            }
            menu->addChild(createIndexSubmenuItem(string::f("Section %d LEDs channel", section + 1), availableChannels,
                [=]() {return module->ledsChannel[section]; },
                [=](int i) {module->ledsChannel[section] = i; }
            ));
        }
    }
};

Model* modelChronos = createModel<Chronos, ChronosWidget>("Sanguine-Monsters-Chronos");