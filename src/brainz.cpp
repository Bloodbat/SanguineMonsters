#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#ifndef METAMODULE
#include "seqcomponents.hpp"
#endif
#include "sanguinehelpers.hpp"
#include "sanguinejson.hpp"

#include "brainz.hpp"

struct Brainz : SanguineModule {
	enum ParamIds {
		PARAM_MODULE_DIRECTION,
		PARAM_LOGIC_ENABLED,
		PARAM_METRONOME_SPEED,
		PARAM_METRONOME_STEPS,
		PARAM_A_DELAY,
		PARAM_B_DELAY,
		PARAM_C_DELAY,
		PARAM_A_ENABLED,
		PARAM_B_ENABLED,
		PARAM_C_ENABLED,
		PARAM_A_DO_TRIGGERS,
		PARAM_B_DO_TRIGGERS,
		PARAM_C_DO_TRIGGERS,
		PARAM_A_IS_METRONOME,
		PARAM_B_IS_METRONOME,
		PARAM_C_IS_METRONOME,
		PARAM_A_DIRECTION,
		PARAM_B_DIRECTION,
		PARAM_PLAY_BUTTON,
		PARAM_RESET_BUTTON,
		PARAM_ONE_SHOT,
		PARAM_START_TRIGGERS,
		PARAM_END_TRIGGERS,
		PARAMS_COUNT
	};

	enum InputIds {
		INPUT_TRIGGER,
		INPUT_RESET,
		INPUTS_COUNT
	};

	enum OutputIds {
		OUTPUT_RUN,
		OUTPUT_RESET,
		OUTPUT_METRONOME,
		OUTPUT_OUT_1,
		OUTPUT_OUT_2,
		OUTPUT_OUT_3,
		OUTPUT_OUT_4,
		OUTPUT_STAGE_A,
		OUTPUT_STAGE_B,
		OUTPUT_STAGE_C,
		OUTPUTS_COUNT
	};

	enum LightIds {
		ENUMS(LIGHT_MODULE_DIRECTION, 3),
		LIGHT_LOGIC_ENABLED,
		LIGHT_STEP_A,
		LIGHT_STEP_B,
		LIGHT_STEP_C,
		LIGHT_METRONOME,
		LIGHT_STEP_A_TRIGGERS,
		LIGHT_STEP_B_TRIGGERS,
		LIGHT_STEP_C_TRIGGERS,
		LIGHT_STEP_A_METRONOME,
		LIGHT_STEP_B_METRONOME,
		LIGHT_STEP_C_METRONOME,
		LIGHT_STEP_A_ENABLED,
		LIGHT_STEP_B_ENABLED,
		LIGHT_STEP_C_ENABLED,
		ENUMS(LIGHT_STEP_A_DIRECTION, 2 * 3),
		ENUMS(LIGHT_MODULE_STAGE, 3),
		LIGHT_START_TRIGGERS,
		LIGHT_END_TRIGGERS,
		ENUMS(LIGHT_OUT_ENABLED, 7),
#ifdef METAMODULE
		LIGHT_ONE_SHOT,
#endif
		LIGHTS_COUNT
	};

	const RGBLightColor moduleStagesLightColors[brainz::MODULE_STAGE_STATES_COUNT]{
		{0.f, 1.f, 0.f},
		{1.f, 1.f, 0.f},
		{0.f, 0.f, 1.f},
		{1.f, 0.f, 0.f},
	};

	const RGBLightColor stepDirectionsLightColors[brainz::DIRECTIONS_COUNT]{
		{kSanguineButtonLightValue, 0.f, kSanguineButtonLightValue},
		{kSanguineButtonLightValue, 0.f, 0.f},
		{0.f, 0.f, kSanguineButtonLightValue},
	};

	static const int kClockDivider = 64;
	static const int kMaxSteps = 3;
	static const int kMaxOutTriggers = 4;

	bool bEnteredMetronome = false;
	bool bInMetronome = false;
	bool bResetSent;
	bool bRunSent;
	bool stepsEnabled[kMaxSteps] = { true, true, true };
	bool bStepStarted = false;
	bool bStepTrigger = false;
	bool bTriggersDone[kMaxOutTriggers];
	bool bTriggersSent = false;

	bool globalOutputsConnected[kMaxOutTriggers] = {};
	bool bHaveRunCable = false;
	bool bHaveResetCable = false;

	brainz::StepDirections moduleDirection = brainz::DIRECTION_BIDIRECTIONAL;
	brainz::StepDirections stepDirections[2] = { brainz::DIRECTION_BIDIRECTIONAL, brainz::DIRECTION_BIDIRECTIONAL };
	brainz::ModuleStages lastModuleStage = brainz::MODULE_STAGE_INIT;
	brainz::ModuleStates lastModuleState = brainz::MODULE_STATE_READY;
	brainz::ModuleStages moduleStage = brainz::MODULE_STAGE_INIT;
	brainz::ModuleStates moduleState = brainz::MODULE_STATE_READY;
	brainz::StepStates stepState = brainz::STEP_STATE_READY;

	std::chrono::time_point<std::chrono::steady_clock> startTime;

	int* metronomeCounterPtr;

	int currentDelayTime;
	int currentCounters[kMaxSteps] = { 0,0,0 };
	int maxCounters[kMaxSteps] = { 1,1,1 };
	int metronomeSpeed = 60;
	int metronomeSteps = 0;
	int metronomeStepsDone = 0;

	float metronomeCounter = 0.f;
	float metronomePeriod = 0.f;

	dsp::BooleanTrigger btReset;
	dsp::BooleanTrigger btRun;
	dsp::SchmittTrigger stResetInput;
	dsp::SchmittTrigger stRunInput;
	dsp::PulseGenerator pgMetronone;
	dsp::PulseGenerator pgReset;
	dsp::PulseGenerator pgRun;
	dsp::PulseGenerator pgTrigger;
	dsp::PulseGenerator pgOutTriggers[kMaxOutTriggers];

	dsp::ClockDivider clockDivider;

	Brainz() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		configSwitch(PARAM_MODULE_DIRECTION, 0.f, 2.f, 0.f, "Module cycle", brainz::directionToolTips);
		configSwitch(PARAM_LOGIC_ENABLED, 0.f, 1.f, 1.f, "Step logic", brainz::stateToolTips);

		configParam(PARAM_A_DELAY, 1.f, 99.f, 1.f, "A trigger delay", " seconds");
		paramQuantities[PARAM_A_DELAY]->snapEnabled = true;
		configParam(PARAM_B_DELAY, 1.f, 99.f, 1.f, "B trigger delay", " seconds");
		paramQuantities[PARAM_B_DELAY]->snapEnabled = true;
		configParam(PARAM_C_DELAY, 1.f, 99.f, 1.f, "C trigger delay", " seconds");
		paramQuantities[PARAM_C_DELAY]->snapEnabled = true;
		configParam(PARAM_METRONOME_SPEED, 15.f, 99.f, 60.f, "BPM", " beats");
		paramQuantities[PARAM_METRONOME_SPEED]->snapEnabled = true;
		configParam(PARAM_METRONOME_STEPS, 5.f, 99.f, 10.f, "Steps", "");
		paramQuantities[PARAM_METRONOME_STEPS]->snapEnabled = true;

		configButton(PARAM_A_ENABLED, "Step A");
		configButton(PARAM_B_ENABLED, "Step B");
		configButton(PARAM_C_ENABLED, "Step C");

		configButton(PARAM_START_TRIGGERS, "Global triggers on start");
		params[PARAM_START_TRIGGERS].setValue(1);
		configButton(PARAM_END_TRIGGERS, "Global triggers on end");
		params[PARAM_END_TRIGGERS].setValue(1);

		configButton(PARAM_A_DO_TRIGGERS, "A global triggers on end");
		configButton(PARAM_B_DO_TRIGGERS, "B global triggers on end");
		configButton(PARAM_C_DO_TRIGGERS, "C global triggers on end");

		configButton(PARAM_A_IS_METRONOME, "A is metronome");
		configButton(PARAM_B_IS_METRONOME, "B is metronome");
		configButton(PARAM_C_IS_METRONOME, "C is metronome");

		configSwitch(PARAM_A_DIRECTION, 0.f, 2.f, 0.f, "A step direction", brainz::directionToolTips);
		configSwitch(PARAM_B_DIRECTION, 0.f, 2.f, 0.f, "B step direction", brainz::directionToolTips);

		configInput(INPUT_TRIGGER, "Start/stop");
		configInput(INPUT_RESET, "Reset");

		configOutput(OUTPUT_RUN, "Run");
		configOutput(OUTPUT_RESET, "Reset");
		configOutput(OUTPUT_METRONOME, "Metronome signal");
		configOutput(OUTPUT_OUT_1, "Global trigger 1");
		configOutput(OUTPUT_OUT_2, "Global trigger 2");
		configOutput(OUTPUT_OUT_3, "Global trigger 3");
		configOutput(OUTPUT_OUT_4, "Global trigger 4");

		configOutput(OUTPUT_STAGE_A, "Step A end");
		configOutput(OUTPUT_STAGE_B, "Step B end");
		configOutput(OUTPUT_STAGE_C, "Step C end");

		configSwitch(PARAM_ONE_SHOT, 0.f, 1.f, 0.f, "One-shot", brainz::stateToolTips);
		configButton(PARAM_PLAY_BUTTON, "Start/stop");
		configButton(PARAM_RESET_BUTTON, "Reset");

		onReset();

		clockDivider.setDivision(kClockDivider);
	}

	void process(const ProcessArgs& args) override {
		if (bInMetronome) {
			if (btReset.process(params[PARAM_RESET_BUTTON].getValue()) || stResetInput.process(inputs[INPUT_RESET].getNormalVoltage(0))) {
				handleResetTriggers();
			}

			if (btRun.process(params[PARAM_PLAY_BUTTON].getValue()) || stRunInput.process(inputs[INPUT_TRIGGER].getNormalVoltage(0))) {
				handleRunTriggers();
			}
			// Ensure only the correct step is lit.
			handleStepLights(args.sampleTime);

			if (params[PARAM_LOGIC_ENABLED].getValue()) {
				doMetronome(args);
			}
		} else {
			if (btReset.process(params[PARAM_RESET_BUTTON].getValue()) || stResetInput.process(inputs[INPUT_RESET].getVoltage())) {
				handleResetTriggers();
			}

			if (btRun.process(params[PARAM_PLAY_BUTTON].getValue()) || stRunInput.process(inputs[INPUT_TRIGGER].getVoltage())) {
				handleRunTriggers();
			}

			if (clockDivider.process()) {
				// Updated only every N samples, so make sure setBrightnessSmooth accounts for this.
				const float sampleTime = args.sampleTime * kClockDivider;

				if (params[PARAM_LOGIC_ENABLED].getValue()) {
					switch (moduleState) {
					case brainz::MODULE_STATE_READY:
					case brainz::MODULE_STATE_WAIT_FOR_RESET:
					case brainz::MODULE_STATE_ROUND_1_END:
						break;

					case brainz::MODULE_STATE_ROUND_1_START:
						resetGlobalTriggers();
						if (params[PARAM_START_TRIGGERS].getValue()) {
							doGlobalTriggers(sampleTime);
						} else {
							for (int trigger = 0; trigger < kMaxOutTriggers; ++trigger) {
								bTriggersDone[trigger] = true;
							}
						}

						if (bTriggersDone[0] && bTriggersDone[1] && bTriggersDone[2] && bTriggersDone[3]) {
							resetStep();
							if (stepsEnabled[0]) {
								moduleState = brainz::MODULE_STATE_ROUND_1_STEP_A;
							} else if (stepDirections[0] < brainz::DIRECTION_BACKWARD && stepsEnabled[1]) {
								moduleState = brainz::MODULE_STATE_ROUND_1_STEP_B;
							} else if (stepsEnabled[2] && stepDirections[1] < brainz::DIRECTION_BACKWARD) {
								moduleState = brainz::MODULE_STATE_ROUND_1_STEP_C;
							} else {
								moduleState = brainz::MODULE_STATE_ROUND_1_END;
							}
						}
						break;


					case brainz::MODULE_STATE_ROUND_1_STEP_A:
						resetGlobalTriggers();
						if (params[PARAM_A_IS_METRONOME].getValue()) {
							if (!bEnteredMetronome) {
								setupMetronome(&currentCounters[0]);
							} else {
								if (!bStepStarted) {
									setupAfterMetronomeTriggers();
								} else {
									handleAfterMetronomeTriggers(OUTPUT_STAGE_A, sampleTime);

									doEndOfStepTriggers(PARAM_A_DO_TRIGGERS, sampleTime);
								}
							}
						} else {
							if (!bStepStarted) {
								setupStep(maxCounters[0]);
							} else {
								doStepTrigger(OUTPUT_STAGE_A, &currentCounters[0], sampleTime);
								doEndOfStepTriggers(PARAM_A_DO_TRIGGERS, sampleTime);
							}
						}

						if (bTriggersDone[0] && bTriggersDone[1] && bTriggersDone[2] && bTriggersDone[3]) {
							if (stepsEnabled[1] && stepDirections[0] < brainz::DIRECTION_BACKWARD) {
								moduleState = brainz::MODULE_STATE_ROUND_1_STEP_B;
							} else if (stepsEnabled[2] && stepDirections[1] < brainz::DIRECTION_BACKWARD) {
								moduleState = brainz::MODULE_STATE_ROUND_1_STEP_C;
							} else {
								moduleState = brainz::MODULE_STATE_ROUND_1_END;
							}
							resetStep();
						}
						break;


					case brainz::MODULE_STATE_ROUND_1_STEP_B:
						resetGlobalTriggers();
						if (params[PARAM_B_IS_METRONOME].getValue()) {
							if (!bEnteredMetronome) {
								setupMetronome(&currentCounters[1]);
							} else {
								if (!bStepStarted) {
									setupAfterMetronomeTriggers();
								} else {
									handleAfterMetronomeTriggers(OUTPUT_STAGE_B, sampleTime);

									doEndOfStepTriggers(PARAM_B_DO_TRIGGERS, sampleTime);
								}
							}
						} else {
							if (!bStepStarted) {
								setupStep(maxCounters[1]);
							} else {
								doStepTrigger(OUTPUT_STAGE_B, &currentCounters[1], sampleTime);
								doEndOfStepTriggers(PARAM_B_DO_TRIGGERS, sampleTime);
							}
						}

						if (bTriggersDone[0] && bTriggersDone[1] && bTriggersDone[2] && bTriggersDone[3]) {
							if (stepsEnabled[2] && stepDirections[1] < brainz::DIRECTION_BACKWARD) {
								moduleState = brainz::MODULE_STATE_ROUND_1_STEP_C;
							} else {
								moduleState = brainz::MODULE_STATE_ROUND_1_END;
							}
							resetStep();
						}
						break;


					case brainz::MODULE_STATE_ROUND_1_STEP_C:
						resetGlobalTriggers();
						if (params[PARAM_C_IS_METRONOME].getValue()) {
							if (!bEnteredMetronome) {
								setupMetronome(&currentCounters[2]);
							} else {
								if (!bStepStarted) {
									setupAfterMetronomeTriggers();
								} else {
									handleAfterMetronomeTriggers(OUTPUT_STAGE_C, sampleTime);

									doEndOfStepTriggers(PARAM_C_DO_TRIGGERS, sampleTime);
								}
							}
						} else {
							if (!bStepStarted) {
								setupStep(maxCounters[2]);
							} else {
								doStepTrigger(OUTPUT_STAGE_C, &currentCounters[2], sampleTime);
								doEndOfStepTriggers(PARAM_C_DO_TRIGGERS, sampleTime);
							}

							if (bTriggersDone[0] && bTriggersDone[1] && bTriggersDone[2] && bTriggersDone[3]) {
								if (moduleDirection == brainz::DIRECTION_BIDIRECTIONAL) {
									moduleState = brainz::MODULE_STATE_ROUND_1_END;
									stepState = brainz::STEP_STATE_READY;
								} else {
									if (!params[PARAM_ONE_SHOT].getValue()) {
										moduleState = brainz::MODULE_STATE_READY;
										moduleStage = brainz::MODULE_STAGE_INIT;
									} else {
										moduleState = brainz::MODULE_STATE_WAIT_FOR_RESET;
										moduleStage = brainz::MODULE_STAGE_ONE_SHOT_END;
									}
								}
								resetStep();
							}
						}
						break;

					case brainz::MODULE_STATE_ROUND_2_START:
						resetStep();
						if (stepsEnabled[2]) {
							moduleState = brainz::MODULE_STATE_ROUND_2_STEP_C;
						} else if ((stepDirections[1] == brainz::DIRECTION_BIDIRECTIONAL
							|| stepDirections[1] == brainz::DIRECTION_BACKWARD) && stepsEnabled[1]) {
							moduleState = brainz::MODULE_STATE_ROUND_2_STEP_B;
						} else if (stepsEnabled[0] && (stepDirections[0] == brainz::DIRECTION_BIDIRECTIONAL
							|| stepDirections[0] == brainz::DIRECTION_BACKWARD)) {
							moduleState = brainz::MODULE_STATE_ROUND_2_STEP_A;
						} else {
							moduleState = brainz::MODULE_STATE_ROUND_2_END;
						}
						break;

					case brainz::MODULE_STATE_ROUND_2_STEP_C:
						resetGlobalTriggers();
						if (params[PARAM_C_IS_METRONOME].getValue()) {
							if (!bEnteredMetronome) {
								setupMetronome(&currentCounters[2]);
							} else {
								if (!bStepStarted) {
									setupAfterMetronomeTriggers();
								} else {
									handleAfterMetronomeTriggers(OUTPUT_STAGE_C, sampleTime);

									doEndOfStepTriggers(PARAM_C_DO_TRIGGERS, sampleTime);
								}
							}
						} else {
							if (!bStepStarted) {
								setupStep(maxCounters[2]);
							} else {
								doStepTrigger(OUTPUT_STAGE_C, &currentCounters[2], sampleTime);
								doEndOfStepTriggers(PARAM_C_DO_TRIGGERS, sampleTime);
							}
						}

						if (bTriggersDone[0] && bTriggersDone[1] && bTriggersDone[2] && bTriggersDone[3]) {
							if (stepsEnabled[1] && (stepDirections[1] == brainz::DIRECTION_BACKWARD
								|| stepDirections[1] == brainz::DIRECTION_BIDIRECTIONAL)) {
								moduleState = brainz::MODULE_STATE_ROUND_2_STEP_B;
							} else if (stepsEnabled[0] && (stepDirections[0] == brainz::DIRECTION_BACKWARD
								|| stepDirections[0] == brainz::DIRECTION_BIDIRECTIONAL)) {
								moduleState = brainz::MODULE_STATE_ROUND_2_STEP_A;
							} else {
								moduleState = brainz::MODULE_STATE_ROUND_2_END;
							}
							resetStep();
						}
						break;


					case brainz::MODULE_STATE_ROUND_2_STEP_B:
						resetGlobalTriggers();
						if (params[PARAM_B_IS_METRONOME].getValue()) {
							if (!bEnteredMetronome) {
								setupMetronome(&currentCounters[1]);
							} else {
								if (!bStepStarted) {
									setupAfterMetronomeTriggers();
								} else {
									handleAfterMetronomeTriggers(OUTPUT_STAGE_B, sampleTime);

									doEndOfStepTriggers(PARAM_B_DO_TRIGGERS, sampleTime);
								}
							}
						} else {
							if (!bStepStarted) {
								setupStep(maxCounters[1]);
							} else {
								doStepTrigger(OUTPUT_STAGE_B, &currentCounters[1], sampleTime);
								doEndOfStepTriggers(PARAM_B_DO_TRIGGERS, sampleTime);
							}
						}

						if (bTriggersDone[0] && bTriggersDone[1] && bTriggersDone[2] && bTriggersDone[3]) {
							if (stepsEnabled[0] && (stepDirections[0] == brainz::DIRECTION_BACKWARD
								|| stepDirections[0] == brainz::DIRECTION_BIDIRECTIONAL)) {
								moduleState = brainz::MODULE_STATE_ROUND_2_STEP_A;
							} else {
								moduleState = brainz::MODULE_STATE_ROUND_2_END;
							}
							resetStep();
						}
						break;

					case brainz::MODULE_STATE_ROUND_2_STEP_A:
						resetGlobalTriggers();
						if (params[PARAM_A_IS_METRONOME].getValue()) {
							if (!bEnteredMetronome) {
								setupMetronome(&currentCounters[0]);
							} else {
								if (!bStepStarted) {
									setupAfterMetronomeTriggers();
								} else {
									handleAfterMetronomeTriggers(OUTPUT_STAGE_A, sampleTime);

									doEndOfStepTriggers(PARAM_A_DO_TRIGGERS, sampleTime);
								}
							}
						} else {
							if (!bStepStarted) {
								setupStep(maxCounters[0]);
							} else {
								doStepTrigger(OUTPUT_STAGE_A, &currentCounters[0], sampleTime);
								doEndOfStepTriggers(PARAM_A_DO_TRIGGERS, sampleTime);
							}
						}

						if (bTriggersDone[0] && bTriggersDone[1] && bTriggersDone[2] && bTriggersDone[3]) {
							moduleState = brainz::MODULE_STATE_ROUND_2_END;
							resetStep();
						}
						break;

					case brainz::MODULE_STATE_ROUND_2_END:
						resetGlobalTriggers();
						if (params[PARAM_END_TRIGGERS].getValue()) {
							doGlobalTriggers(sampleTime);
						} else {
							for (int trigger = 0; trigger < kMaxOutTriggers; ++trigger) {
								bTriggersDone[trigger] = true;
							}
						}

						if (bTriggersDone[0] && bTriggersDone[1] && bTriggersDone[2] && bTriggersDone[3]) {
							if (!params[PARAM_ONE_SHOT].getValue()) {
								moduleState = brainz::MODULE_STATE_READY;
								moduleStage = brainz::MODULE_STAGE_INIT;
							} else {
								moduleState = brainz::MODULE_STATE_WAIT_FOR_RESET;
								moduleStage = brainz::MODULE_STAGE_ONE_SHOT_END;
							}
						}
						break;
					}
				}

				metronomeSpeed = params[PARAM_METRONOME_SPEED].getValue();
				metronomeSteps = params[PARAM_METRONOME_STEPS].getValue();

				for (int step = 0; step < kMaxSteps; ++step) {
					stepsEnabled[step] = params[PARAM_A_ENABLED + step].getValue();

					lights[LIGHT_STEP_A_ENABLED + step].setBrightnessSmooth(params[PARAM_A_ENABLED + step].getValue() ?
						kSanguineButtonLightValue : 0.f, sampleTime);
					lights[LIGHT_STEP_A_TRIGGERS + step].setBrightnessSmooth(params[PARAM_A_DO_TRIGGERS + step].getValue() ?
						kSanguineButtonLightValue : 0.f, sampleTime);
					lights[LIGHT_STEP_A_METRONOME + step].setBrightnessSmooth(params[PARAM_A_IS_METRONOME + step].getValue() ?
						kSanguineButtonLightValue : 0.f, sampleTime);

					if (step < 2) {
						stepDirections[step] = brainz::StepDirections(params[PARAM_A_DIRECTION + step].getValue());

						int currentLight = LIGHT_STEP_A_DIRECTION + step * 3;
						lights[currentLight + 0].setBrightness(stepDirectionsLightColors[stepDirections[step]].red);
						lights[currentLight + 1].setBrightness(stepDirectionsLightColors[stepDirections[step]].green);
						lights[currentLight + 2].setBrightness(stepDirectionsLightColors[stepDirections[step]].blue);
					}

					if (!params[PARAM_A_IS_METRONOME + step].getValue()) {
						maxCounters[step] = params[PARAM_A_DELAY + step].getValue();
					} else {
						maxCounters[step] = 0;
					}
				}

				lights[LIGHT_LOGIC_ENABLED].setBrightnessSmooth(params[PARAM_LOGIC_ENABLED].getValue() ? 1.f : 0.f, sampleTime);

				lights[LIGHT_MODULE_DIRECTION + 0].setBrightness(stepDirectionsLightColors[moduleDirection].red);
				lights[LIGHT_MODULE_DIRECTION + 1].setBrightness(stepDirectionsLightColors[moduleDirection].green);
				lights[LIGHT_MODULE_DIRECTION + 2].setBrightness(stepDirectionsLightColors[moduleDirection].blue);

				if (params[PARAM_LOGIC_ENABLED].getValue()) {
					lights[LIGHT_MODULE_STAGE + 0].setBrightnessSmooth(moduleStagesLightColors[moduleStage].red, sampleTime);
					lights[LIGHT_MODULE_STAGE + 1].setBrightnessSmooth(moduleStagesLightColors[moduleStage].green, sampleTime);
					lights[LIGHT_MODULE_STAGE + 2].setBrightnessSmooth(moduleStagesLightColors[moduleStage].blue, sampleTime);
				} else
				{
					lights[LIGHT_MODULE_STAGE + 0].setBrightnessSmooth(0.f, sampleTime);
					lights[LIGHT_MODULE_STAGE + 1].setBrightnessSmooth(0.f, sampleTime);
					lights[LIGHT_MODULE_STAGE + 2].setBrightnessSmooth(0.f, sampleTime);
				}

				lights[LIGHT_START_TRIGGERS].setBrightnessSmooth(params[PARAM_START_TRIGGERS].getValue() ? kSanguineButtonLightValue : 0.f, sampleTime);
				lights[LIGHT_END_TRIGGERS].setBrightnessSmooth(params[PARAM_END_TRIGGERS].getValue() ? kSanguineButtonLightValue : 0.f, sampleTime);

				lights[LIGHT_OUT_ENABLED].setBrightnessSmooth(params[PARAM_LOGIC_ENABLED].getValue() ? 0.f : 1.f, sampleTime);
				lights[LIGHT_OUT_ENABLED + 1].setBrightnessSmooth(1.f, sampleTime);
				for (int light = 2; light < 7; ++light) {
					lights[LIGHT_OUT_ENABLED + light].setBrightnessSmooth(params[PARAM_LOGIC_ENABLED].getValue() ? 1.f : 0.f, sampleTime);
				}

#ifdef METAMODULE
				lights[LIGHT_ONE_SHOT].setBrightness(static_cast<bool>(params[PARAM_ONE_SHOT].getValue()) ? kSanguineButtonLightValue : 0.f);
#endif

				handleStepLights(sampleTime);

				if (moduleState == brainz::MODULE_STATE_READY) {
					moduleDirection = brainz::StepDirections(params[PARAM_MODULE_DIRECTION].getValue());
					if (moduleState < brainz::MODULE_STATE_WAIT_FOR_RESET) {
						moduleStage = brainz::MODULE_STAGE_INIT;
					} else {
						moduleStage = brainz::MODULE_STAGE_ONE_SHOT_END;
					}
					moduleState = lastModuleState;
				}
			} // End clock divider.

			if (bRunSent) {
				bRunSent = pgRun.process(args.sampleTime);
				outputs[OUTPUT_RUN].setVoltage(bRunSent ? 10.f : 0.f);
			}

			if (bResetSent) {
				bResetSent = pgReset.process(args.sampleTime);
				outputs[OUTPUT_RESET].setVoltage(bResetSent ? 10.f : 0.f);
			}
		}
	}

	void onReset() override {
		for (int step = 0; step < kMaxSteps; ++step) {
			params[PARAM_A_ENABLED + step].setValue(1);
			if (step < 2) {
				params[PARAM_START_TRIGGERS + step].setValue(1);
			}
		}

	}

	void resetGlobalTriggers() {
		memset(bTriggersDone, 0, sizeof(bool) * kMaxOutTriggers);
	}

	void doGlobalTriggers(const float sampleTime) {
		if (!bTriggersSent) {
			for (int trigger = 0; trigger < kMaxOutTriggers; ++trigger) {
				if (globalOutputsConnected[trigger]) {
					pgOutTriggers[trigger].trigger();
					outputs[OUTPUT_OUT_1 + trigger].setVoltage(pgOutTriggers[trigger].process(1.f / sampleTime) ? 10.f : 0.f);
				}
			}
			bTriggersSent = true;
		} else {
			for (int trigger = 0; trigger < kMaxOutTriggers; ++trigger) {
				bTriggersDone[trigger] = !pgOutTriggers[trigger].process(1.f / sampleTime);
				if (globalOutputsConnected[trigger]) {
					outputs[OUTPUT_OUT_1 + trigger].setVoltage(bTriggersDone[trigger] ? 0.f : 10.f);
				}
			}
		}
	}

	void doMetronome(const ProcessArgs& args) {
		bEnteredMetronome = true;

		metronomePeriod = 60.f * args.sampleRate / metronomeSpeed;

		if (metronomeCounter > metronomePeriod) {
			pgMetronone.trigger();
			metronomeCounter -= metronomePeriod;
			++metronomeStepsDone;
			*metronomeCounterPtr = metronomeStepsDone;
		}
		++metronomeCounter;

		outputs[OUTPUT_METRONOME].setVoltage(pgMetronone.process(args.sampleTime) ? 10.f : 0.f);

		if (metronomeStepsDone >= metronomeSteps && !pgMetronone.process(args.sampleTime)) {
			bInMetronome = false;
		}
	}

	void setupMetronome(int* counterNumber) {
		metronomeCounter = 0.f;
		metronomePeriod = 0.f;
		metronomeStepsDone = 0;
		metronomeCounterPtr = counterNumber;
		bInMetronome = true;
	}

	void setupAfterMetronomeTriggers() {
		bStepStarted = true;
		bTriggersSent = false;
	}

	void handleAfterMetronomeTriggers(OutputIds output, const float sampleTime) {
		if (stepState < brainz::STEP_STATE_TRIGGER_SENT) {
			if (outputs[output].isConnected()) {
				pgTrigger.trigger();
				outputs[output].setVoltage(pgTrigger.process(1.f / sampleTime) ? 10.f : 0.f);
			}
			stepState = brainz::STEP_STATE_TRIGGER_SENT;
		} else {
			bStepTrigger = pgTrigger.process(1.f / sampleTime);
			if (outputs[output].isConnected()) {
				outputs[output].setVoltage(bStepTrigger ? 10.f : 0.f);
			}
			if (!bStepTrigger) {
				stepState = brainz::STEP_STATE_TRIGGER_DONE;
			}
		}
	}

	void handleRunTriggers() {
		if (params[PARAM_LOGIC_ENABLED].getValue()) {
			if (bInMetronome) {
				bInMetronome = false;
				killVoltages();
				moduleState = brainz::MODULE_STATE_READY;
				moduleStage = brainz::MODULE_STAGE_INIT;
			} else {
				switch (moduleState) {
				case brainz::MODULE_STATE_READY:
					bTriggersSent = false;
					if (moduleDirection < brainz::DIRECTION_BACKWARD) {
						moduleStage = brainz::MODULE_STAGE_ROUND_1;
						moduleState = brainz::MODULE_STATE_ROUND_1_START;
					} else if (moduleDirection == brainz::DIRECTION_BACKWARD) {
						moduleState = brainz::MODULE_STATE_ROUND_2_START;
						moduleStage = brainz::MODULE_STAGE_ROUND_2;
					}
					break;

				case brainz::MODULE_STATE_ROUND_1_START:
				case brainz::MODULE_STATE_ROUND_1_STEP_A:
				case brainz::MODULE_STATE_ROUND_1_STEP_B:
				case brainz::MODULE_STATE_ROUND_1_STEP_C:
					killVoltages();
					moduleState = brainz::MODULE_STATE_READY;
					moduleStage = brainz::MODULE_STAGE_INIT;
					break;


				case brainz::MODULE_STATE_ROUND_1_END:
					if (moduleDirection == brainz::DIRECTION_BACKWARD
						|| moduleDirection == brainz::DIRECTION_BIDIRECTIONAL) {
						memset(currentCounters, 0, sizeof(int) * kMaxSteps);
						moduleStage = brainz::MODULE_STAGE_ROUND_2;
						moduleState = brainz::MODULE_STATE_ROUND_2_START;
					}
					break;

				case brainz::MODULE_STATE_ROUND_2_START:
				case brainz::MODULE_STATE_ROUND_2_STEP_A:
				case brainz::MODULE_STATE_ROUND_2_STEP_B:
				case brainz::MODULE_STATE_ROUND_2_STEP_C:
				case brainz::MODULE_STATE_ROUND_2_END:
					killVoltages();
					moduleState = brainz::MODULE_STATE_READY;
					moduleStage = brainz::MODULE_STAGE_INIT;
					break;

				case brainz::MODULE_STATE_WAIT_FOR_RESET:
					break;
				}
			}
		} else {
			if (bHaveRunCable) {
				bRunSent = true;
				pgRun.trigger();
			}
		}
	}

	void handleResetTriggers() {
		bInMetronome = false;
		killVoltages();
		memset(currentCounters, 0, sizeof(int) * kMaxSteps);
		metronomeStepsDone = 0;
		moduleState = brainz::MODULE_STATE_READY;
		moduleStage = brainz::MODULE_STAGE_INIT;
		if (bHaveResetCable) {
			bResetSent = true;
			pgReset.trigger();
		}
	}

	void killVoltages() {
		for (int output = OUTPUT_METRONOME; output < OUTPUTS_COUNT; ++output) {
			outputs[output].setVoltage(0);
		}
	}

	void setupStep(int delayTime) {
		startTime = std::chrono::steady_clock::now();
		bStepStarted = true;
		bTriggersSent = false;
		currentDelayTime = delayTime * 1000;
	}

	void doStepTrigger(OutputIds output, int* counter, const float sampleTime) {
		if (stepState < brainz::STEP_STATE_TRIGGER_SENT) {
			std::chrono::time_point<std::chrono::steady_clock> currentTime = std::chrono::steady_clock::now();
			int elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();
			*counter = elapsedTime / 1000;
			if (elapsedTime >= currentDelayTime) {
				if (outputs[output].isConnected()) {
					pgTrigger.trigger();
					outputs[output].setVoltage(pgTrigger.process(1.f / sampleTime) ? 10.f : 0.f);
				}
				stepState = brainz::STEP_STATE_TRIGGER_SENT;
			}
		} else {
			bStepTrigger = pgTrigger.process(1.f / sampleTime);
			if (outputs[output].isConnected()) {
				outputs[output].setVoltage(bStepTrigger ? 10.f : 0.f);
			}
			if (!bStepTrigger) {
				stepState = brainz::STEP_STATE_TRIGGER_DONE;
			}
		}
	}

	void doEndOfStepTriggers(ParamIds checkParam, const float sampleTime) {
		if (bStepStarted && stepState == brainz::STEP_STATE_TRIGGER_DONE) {
			if (!params[checkParam].getValue()) {
				for (int trigger = 0; trigger < kMaxOutTriggers; ++trigger) {
					bTriggersDone[trigger] = true;
				}
			} else {
				doGlobalTriggers(sampleTime);
			}
		}
	}

	void resetStep() {
		bStepStarted = false;
		bStepTrigger = false;
		bEnteredMetronome = false;
		stepState = brainz::STEP_STATE_READY;
	}

	void handleStepLights(float sampleTime) {
		lights[LIGHT_STEP_A].setBrightnessSmooth((moduleState == brainz::MODULE_STATE_ROUND_1_STEP_A
			|| moduleState == brainz::MODULE_STATE_ROUND_2_STEP_A) ? 1.f : 0.f, sampleTime);
		lights[LIGHT_STEP_B].setBrightnessSmooth((moduleState == brainz::MODULE_STATE_ROUND_1_STEP_B
			|| moduleState == brainz::MODULE_STATE_ROUND_2_STEP_B) ? 1.f : 0.f, sampleTime);
		lights[LIGHT_STEP_C].setBrightnessSmooth((moduleState == brainz::MODULE_STATE_ROUND_1_STEP_C
			|| moduleState == brainz::MODULE_STATE_ROUND_2_STEP_C) ? 1.f : 0.f, sampleTime);
		lights[LIGHT_METRONOME].setBrightnessSmooth(bInMetronome ? 1.f : 0.f, sampleTime);
	}

	void onPortChange(const PortChangeEvent& e) override {
		if (e.type == Port::OUTPUT) {
			switch (e.portId) {
			case OUTPUT_OUT_1:
				globalOutputsConnected[0] = e.connecting;
				break;
			case OUTPUT_OUT_2:
				globalOutputsConnected[1] = e.connecting;
				break;
			case OUTPUT_OUT_3:
				globalOutputsConnected[2] = e.connecting;
				break;
			case OUTPUT_OUT_4:
				globalOutputsConnected[2] = e.connecting;
				break;

			case OUTPUT_RUN:
				bHaveRunCable = e.connecting;
				break;

			case OUTPUT_RESET:
				bHaveResetCable = e.connecting;
				break;

			default:
				break;
			}
		}
	}
};

#ifndef METAMODULE
struct YellowGateLight : SanguineStaticRGBLight {
	YellowGateLight(Module* theModule, const float X, const float Y, bool createCentered = true) :
		SanguineStaticRGBLight(theModule, "res/gate_lit.svg", X, Y, createCentered, kSanguineYellowLight) {
	}
};

struct BluePowerLight : SanguineStaticRGBLight {
	BluePowerLight(Module* theModule, const float X, const float Y, bool createCentered = true) :
		SanguineStaticRGBLight(theModule, "res/power_lit.svg", X, Y, createCentered, kSanguineBlueLight) {
	}
};

struct BlueAdvancedClockLight : SanguineStaticRGBLight {
	BlueAdvancedClockLight(Module* theModule, const float X, const float Y, bool createCentered = true) :
		SanguineStaticRGBLight(theModule, "res/clock_move_lit.svg", X, Y, createCentered, kSanguineBlueLight) {
	}
};

struct BlueInitialClockLight : SanguineStaticRGBLight {
	BlueInitialClockLight(Module* theModule, const float X, const float Y, bool createCentered = true) :
		SanguineStaticRGBLight(theModule, "res/clock_still_lit.svg", X, Y, createCentered, kSanguineBlueLight) {
	}
};

struct BlueRightArrowLight : SanguineStaticRGBLight {
	BlueRightArrowLight(Module* theModule, const float X, const float Y, bool createCentered = true) :
		SanguineStaticRGBLight(theModule, "res/arrow_right_lit.svg", X, Y, createCentered, kSanguineBlueLight) {
	}
};

struct BlueQuarterNoteLight : SanguineStaticRGBLight {
	BlueQuarterNoteLight(Module* theModule, const float X, const float Y, bool createCentered = true) :
		SanguineStaticRGBLight(theModule, "res/quarter_note_lit.svg", X, Y, createCentered, kSanguineBlueLight) {
	}
};
#endif

struct BrainzWidget : SanguineModuleWidget {
	explicit BrainzWidget(Brainz* module) {
		setModule(module);

		moduleName = "brainz";
		panelSize = SIZE_25;
		backplateColor = PLATE_PURPLE;
		bFaceplateSuffix = false;

		makePanel();

		addScrews(SCREW_ALL);

#ifndef METAMODULE
		addParam(createParamCentered<SeqButtonPlay>(millimetersToPixelsVec(97.39, 11.87), module,
			Brainz::PARAM_PLAY_BUTTON));

		addParam(createParamCentered<SeqButtonReset>(millimetersToPixelsVec(109.556, 11.87), module,
			Brainz::PARAM_RESET_BUTTON));
#else
		addParam(createParamCentered<VCVButton>(millimetersToPixelsVec(97.39, 11.87), module,
			Brainz::PARAM_PLAY_BUTTON));

		addParam(createParamCentered<VCVButton>(millimetersToPixelsVec(109.556, 11.87), module,
			Brainz::PARAM_RESET_BUTTON));
#endif

		CKD6* btnModuleDirection = createParamCentered<CKD6>(millimetersToPixelsVec(81.319, 59.888), module, Brainz::PARAM_MODULE_DIRECTION);
		btnModuleDirection->momentary = false;
		btnModuleDirection->latch = true;
		addParam(btnModuleDirection);
		addChild(createLightCentered<CKD6Light<RedGreenBlueLight>>(millimetersToPixelsVec(81.319, 59.888), module,
			Brainz::LIGHT_MODULE_DIRECTION));

		addChild(createLightCentered<LargeLight<RedGreenBlueLight>>(millimetersToPixelsVec(119.637, 11.906), module,
			Brainz::LIGHT_MODULE_STAGE));

		Befaco2StepSwitch* switchLogicEnabled = createParamCentered<Befaco2StepSwitch>(millimetersToPixelsVec(63.5, 28.771), module,
			Brainz::PARAM_LOGIC_ENABLED);
		switchLogicEnabled->momentary = false;
		addParam(switchLogicEnabled);
		addChild(createLightCentered<LargeLight<RedLight>>(millimetersToPixelsVec(63.5, 38.373), module, Brainz::LIGHT_LOGIC_ENABLED));

		addParam(createParamCentered<BefacoTinyKnobRed>(millimetersToPixelsVec(27.568, 25.914), module, Brainz::PARAM_A_DELAY));
		addParam(createParamCentered<BefacoTinyKnobRed>(millimetersToPixelsVec(27.568, 76.944), module, Brainz::PARAM_B_DELAY));
		addParam(createParamCentered<BefacoTinyKnobRed>(millimetersToPixelsVec(99.539, 76.944), module, Brainz::PARAM_C_DELAY));
		addParam(createParamCentered<BefacoTinyKnobRed>(millimetersToPixelsVec(95.045, 28.914), module, Brainz::PARAM_METRONOME_SPEED));
		addParam(createParamCentered<BefacoTinyKnobBlack>(millimetersToPixelsVec(81.488, 40.647), module, Brainz::PARAM_METRONOME_STEPS));

		addParam(createLightParamCentered<VCVLightBezelLatch<OrangeLight>>(millimetersToPixelsVec(47.535, 25.914), module,
			Brainz::PARAM_A_ENABLED, Brainz::LIGHT_STEP_A_ENABLED));
		addParam(createLightParamCentered<VCVLightBezelLatch<OrangeLight>>(millimetersToPixelsVec(47.535, 76.944), module,
			Brainz::PARAM_B_ENABLED, Brainz::LIGHT_STEP_B_ENABLED));
		addParam(createLightParamCentered<VCVLightBezelLatch<OrangeLight>>(millimetersToPixelsVec(119.321, 76.944), module,
			Brainz::PARAM_C_ENABLED, Brainz::LIGHT_STEP_C_ENABLED));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<BlueLight>>>(millimetersToPixelsVec(7.301, 44.247),
			module, Brainz::PARAM_A_DO_TRIGGERS, Brainz::LIGHT_STEP_A_TRIGGERS));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(millimetersToPixelsVec(25.177, 44.247),
			module, Brainz::PARAM_A_IS_METRONOME, Brainz::LIGHT_STEP_A_METRONOME));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<BlueLight>>>(millimetersToPixelsVec(7.301, 95.277),
			module, Brainz::PARAM_B_DO_TRIGGERS, Brainz::LIGHT_STEP_B_TRIGGERS));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(millimetersToPixelsVec(25.177, 95.277),
			module, Brainz::PARAM_B_IS_METRONOME, Brainz::LIGHT_STEP_B_METRONOME));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<BlueLight>>>(millimetersToPixelsVec(79.288, 95.277),
			module, Brainz::PARAM_C_DO_TRIGGERS, Brainz::LIGHT_STEP_C_TRIGGERS));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(millimetersToPixelsVec(97.148, 95.277),
			module, Brainz::PARAM_C_IS_METRONOME, Brainz::LIGHT_STEP_C_METRONOME));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedLight>>>(millimetersToPixelsVec(108.308, 54.211),
			module, Brainz::PARAM_START_TRIGGERS, Brainz::LIGHT_START_TRIGGERS));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedLight>>>(millimetersToPixelsVec(108.308, 65.543),
			module, Brainz::PARAM_END_TRIGGERS, Brainz::LIGHT_END_TRIGGERS));

		addOutput(createOutputCentered<BananutBlack>(millimetersToPixelsVec(46.835, 44.247), module, Brainz::OUTPUT_STAGE_A));
		addOutput(createOutputCentered<BananutBlack>(millimetersToPixelsVec(46.835, 95.277), module, Brainz::OUTPUT_STAGE_B));
		addOutput(createOutputCentered<BananutBlack>(millimetersToPixelsVec(118.821, 95.277), module, Brainz::OUTPUT_STAGE_C));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedGreenBlueLight>>>(millimetersToPixelsVec(27.522, 59.903),
			module, Brainz::PARAM_A_DIRECTION, Brainz::LIGHT_STEP_A_DIRECTION + 0 * 3));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedGreenBlueLight>>>(millimetersToPixelsVec(63.5, 85.403),
			module, Brainz::PARAM_B_DIRECTION, Brainz::LIGHT_STEP_A_DIRECTION + 1 * 3));

		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(7.402, 116.807), module, Brainz::INPUT_TRIGGER));
		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(20.367, 116.807), module, Brainz::INPUT_RESET));

		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(61.229, 116.807), module, Brainz::OUTPUT_RUN));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(70.964, 116.807), module, Brainz::OUTPUT_RESET));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(80.698, 116.807), module, Brainz::OUTPUT_METRONOME));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(90.433, 116.807), module, Brainz::OUTPUT_OUT_1));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(100.168, 116.807), module, Brainz::OUTPUT_OUT_2));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(109.903, 116.807), module, Brainz::OUTPUT_OUT_3));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(119.637, 116.807), module, Brainz::OUTPUT_OUT_4));

		addChild(createLightCentered<SmallLight<RedLight>>(millimetersToPixelsVec(13.628, 25.914), module, Brainz::LIGHT_STEP_A));
		addChild(createLightCentered<SmallLight<RedLight>>(millimetersToPixelsVec(13.628, 76.944), module, Brainz::LIGHT_STEP_B));
		addChild(createLightCentered<SmallLight<RedLight>>(millimetersToPixelsVec(85.615, 76.944), module, Brainz::LIGHT_STEP_C));
		addChild(createLightCentered<SmallLight<RedLight>>(millimetersToPixelsVec(85.615, 25.114), module, Brainz::LIGHT_METRONOME));

		float currentX = 63.456;
		float deltaX = 9.74;
		for (int light = 0; light < 7; ++light) {
			addChild(createLightCentered<TinyLight<YellowLight>>(millimetersToPixelsVec(currentX, 109.601), module,
				Brainz::LIGHT_OUT_ENABLED + light));
			currentX += deltaX;
		}

#ifndef METAMODULE
		addParam(createParam<SeqButtonOneShotSmall>(millimetersToPixelsVec(91.231, 57.888), module, Brainz::PARAM_ONE_SHOT));
#else
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedLight>>>(millimetersToPixelsVec(91.231, 59.888),
			module, Brainz::PARAM_ONE_SHOT, Brainz::LIGHT_ONE_SHOT));
#endif

		FramebufferWidget* brainzFrameBuffer = new FramebufferWidget();
		addChild(brainzFrameBuffer);

		SanguineTinyNumericDisplay* displayStepsACurrent = new SanguineTinyNumericDisplay(2, module, 18.929, 35.908);
		brainzFrameBuffer->addChild(displayStepsACurrent);

		SanguineTinyNumericDisplay* displayStepsATotal = new SanguineTinyNumericDisplay(2, module, 36.207, 35.908);
		brainzFrameBuffer->addChild(displayStepsATotal);
		displayStepsATotal->fallbackNumber = 1;

		SanguineTinyNumericDisplay* displayStepsBCurrent = new SanguineTinyNumericDisplay(2, module, 18.929, 86.938);
		brainzFrameBuffer->addChild(displayStepsBCurrent);

		SanguineTinyNumericDisplay* displayStepsBTotal = new SanguineTinyNumericDisplay(2, module, 36.207, 86.938);
		brainzFrameBuffer->addChild(displayStepsBTotal);
		displayStepsBTotal->fallbackNumber = 1;

		SanguineTinyNumericDisplay* displayStepsCCurrent = new SanguineTinyNumericDisplay(2, module, 90.891, 86.938);
		brainzFrameBuffer->addChild(displayStepsCCurrent);

		SanguineTinyNumericDisplay* displayStepsCTotal = new SanguineTinyNumericDisplay(2, module, 108.181, 86.938);
		brainzFrameBuffer->addChild(displayStepsCTotal);
		displayStepsCTotal->fallbackNumber = 1;

		SanguineTinyNumericDisplay* displayMetronomeSpeed = new SanguineTinyNumericDisplay(2, module, 110.857, 28.914);
		brainzFrameBuffer->addChild(displayMetronomeSpeed);
		displayMetronomeSpeed->fallbackNumber = 60;

		SanguineTinyNumericDisplay* displayMetronomeCurrentStep = new SanguineTinyNumericDisplay(2, module, 95.045, 40.647);
		brainzFrameBuffer->addChild(displayMetronomeCurrentStep);

		SanguineTinyNumericDisplay* displayMetronomeTotalSteps = new SanguineTinyNumericDisplay(2, module, 110.857, 40.647);
		brainzFrameBuffer->addChild(displayMetronomeTotalSteps);
		displayMetronomeTotalSteps->fallbackNumber = 10;

#ifndef METAMODULE
		SanguineStaticRGBLight* inPlayLight = new SanguineStaticRGBLight(module, "res/play_lit.svg", 7.402, 109.601,
			true, kSanguineYellowLight);
		addChild(inPlayLight);

		SanguineStaticRGBLight* resetLight = new SanguineStaticRGBLight(module, "res/reset_lit.svg", 20.367, 109.601,
			true, kSanguineYellowLight);
		addChild(resetLight);

		SanguineStaticRGBLight* metronomeStepsLight = new SanguineStaticRGBLight(module, "res/numpad_lit.svg", 120.921, 40.647,
			true, kSanguineBlueLight);
		addChild(metronomeStepsLight);

		YellowGateLight* gateLightA = new YellowGateLight(module, 39.454, 44.247);
		addChild(gateLightA);

		YellowGateLight* gateLightB = new YellowGateLight(module, 39.454, 95.277);
		addChild(gateLightB);

		YellowGateLight* gateLightC = new YellowGateLight(module, 111.441, 95.277);
		addChild(gateLightC);

		BluePowerLight* powerLightA = new BluePowerLight(module, 40.675, 25.914);
		addChild(powerLightA);

		BluePowerLight* powerLightB = new BluePowerLight(module, 40.675, 76.944);
		addChild(powerLightB);

		BluePowerLight* powerLightC = new BluePowerLight(module, 112.461, 76.944);
		addChild(powerLightC);

		BlueAdvancedClockLight* steppedClockLightA = new BlueAdvancedClockLight(module, 6.201, 35.743);
		addChild(steppedClockLightA);

		BlueAdvancedClockLight* steppedClockLightB = new BlueAdvancedClockLight(module, 6.201, 86.772);
		addChild(steppedClockLightB);

		BlueAdvancedClockLight* steppedClockLightC = new BlueAdvancedClockLight(module, 78.151, 86.772);
		addChild(steppedClockLightC);

		BlueInitialClockLight* initialClockLightA = new BlueInitialClockLight(module, 48.935, 35.908);
		addChild(initialClockLightA);

		BlueInitialClockLight* initialClockLightB = new BlueInitialClockLight(module, 48.935, 86.938);
		addChild(initialClockLightB);

		BlueInitialClockLight* initialClockLightC = new BlueInitialClockLight(module, 120.921, 86.938);
		addChild(initialClockLightC);

		BlueRightArrowLight* arrowLightA = new BlueRightArrowLight(module, 13.471, 44.247);
		addChild(arrowLightA);

		BlueRightArrowLight* arrowLightB = new BlueRightArrowLight(module, 13.471, 95.277);
		addChild(arrowLightB);

		BlueRightArrowLight* arrowLightC = new BlueRightArrowLight(module, 85.426, 95.277);
		addChild(arrowLightC);

		BlueQuarterNoteLight* quarterNoteLightA = new BlueQuarterNoteLight(module, 30.167, 44.247);
		addChild(quarterNoteLightA);

		BlueQuarterNoteLight* quarterNoteLightB = new BlueQuarterNoteLight(module, 30.167, 95.277);
		addChild(quarterNoteLightB);

		BlueQuarterNoteLight* quarterNoteLightC = new BlueQuarterNoteLight(module, 102.138, 95.277);
		addChild(quarterNoteLightC);

		BlueQuarterNoteLight* quarterNoteLightMetronome = new BlueQuarterNoteLight(module, 120.921, 28.914);
		addChild(quarterNoteLightMetronome);

		SanguineStaticRGBLight* outPlayLight = new SanguineStaticRGBLight(module, "res/play_lit.svg", 59.739, 109.601,
			true, kSanguineYellowLight);
		addChild(outPlayLight);

		SanguineStaticRGBLight* outResetLight = new SanguineStaticRGBLight(module, "res/reset_lit.svg", 69.799, 109.601,
			true, kSanguineYellowLight);
		addChild(outResetLight);

		SanguineStaticRGBLight* outMetronomeLight = new SanguineStaticRGBLight(module, "res/clock_lit.svg", 79.541, 109.601,
			true, kSanguineYellowLight);
		addChild(outMetronomeLight);

		SanguineStaticRGBLight* outLight1 = new SanguineStaticRGBLight(module, "res/number_1_lit.svg", 88.888, 109.601,
			true, kSanguineYellowLight);
		addChild(outLight1);

		SanguineStaticRGBLight* outLight2 = new SanguineStaticRGBLight(module, "res/number_2_lit.svg", 98.652, 109.601,
			true, kSanguineYellowLight);
		addChild(outLight2);

		SanguineStaticRGBLight* outLight3 = new SanguineStaticRGBLight(module, "res/number_3_lit.svg", 108.321, 109.601,
			true, kSanguineYellowLight);
		addChild(outLight3);

		SanguineStaticRGBLight* outLight4 = new SanguineStaticRGBLight(module, "res/number_4_lit.svg", 117.898, 109.601,
			true, kSanguineYellowLight);
		addChild(outLight4);

		SanguineBloodLogoLight* bloodLight = new SanguineBloodLogoLight(module, 31.116, 109.911);
		addChild(bloodLight);

		SanguineMonstersLogoLight* monstersLight = new SanguineMonstersLogoLight(module, 44.248, 116.867);
		addChild(monstersLight);
#endif

		if (module) {
			displayStepsACurrent->values.numberValue = &module->currentCounters[0];
			displayStepsATotal->values.numberValue = &module->maxCounters[0];
			displayStepsBCurrent->values.numberValue = &module->currentCounters[1];
			displayStepsBTotal->values.numberValue = &module->maxCounters[1];
			displayStepsCCurrent->values.numberValue = &module->currentCounters[2];
			displayStepsCTotal->values.numberValue = &module->maxCounters[2];
			displayMetronomeSpeed->values.numberValue = &module->metronomeSpeed;
			displayMetronomeCurrentStep->values.numberValue = &module->metronomeStepsDone;

			displayMetronomeTotalSteps->values.numberValue = &module->metronomeSteps;
		}
	}
};

Model* modelBrainz = createModel<Brainz, BrainzWidget>("Sanguine-Monsters-Brainz");