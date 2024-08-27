#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "seqcomponents.hpp"
#include "sanguinehelpers.hpp"

enum ModuleStages {
	MODULE_STAGE_INIT,
	MODULE_STAGE_ROUND_1,
	MODULE_STAGE_ROUND_2,
	MODULE_STAGE_ONE_SHOT_END,
	MODULE_STAGE_STATES_COUNT
};

enum ModuleStates {
	MODULE_STATE_READY,
	MODULE_STATE_ROUND_1_START,
	MODULE_STATE_ROUND_1_STEP_A,
	MODULE_STATE_ROUND_1_STEP_B,
	MODULE_STATE_ROUND_1_STEP_C,
	MODULE_STATE_ROUND_1_END,
	MODULE_STATE_ROUND_2_START,
	MODULE_STATE_ROUND_2_STEP_C,
	MODULE_STATE_ROUND_2_STEP_B,
	MODULE_STATE_ROUND_2_STEP_A,
	MODULE_STATE_ROUND_2_END,
	MODULE_STATE_WAIT_FOR_RESET
};

enum StepStates {
	STEP_STATE_READY,
	STEP_STATE_TRIGGER_SENT,
	STEP_STATE_TRIGGER_DONE
};

enum StepDirections {
	DIRECTION_BIDIRECTIONAL,
	DIRECTION_FORWARD,
	DIRECTION_BACKWARD,
	DIRECTIONS_COUNT
};

struct Brainz : Module {
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
		LIGHTS_COUNT
	};

	const RGBLightColor moduleStagesLightColors[MODULE_STAGE_STATES_COUNT]{
		{0.f, 1.f, 0.f},
		{1.f, 1.f, 0.f},
		{0.f, 0.f, 1.f},
		{1.f, 0.0f, 0.f},
	};

	const RGBLightColor stepDirectionsLightColors[DIRECTIONS_COUNT]{
		{1.f, 0.f, 1.f},
		{1.f, 0.f, 0.f},
		{0.f, 0.f, 1.f},
	};

	bool bEnteredMetronome = false;
	bool bInMetronome = false;
	bool bResetSent;
	bool bRunSent;
	bool bStepEnabled[3] = { true, true, true };
	bool bStepStarted = false;
	bool bStepTrigger = false;
	bool bTriggersDone[4];
	bool bTriggersSent = false;

	StepDirections moduleDirection = DIRECTION_BIDIRECTIONAL;
	StepDirections stepDirections[2] = { DIRECTION_BIDIRECTIONAL, DIRECTION_BIDIRECTIONAL };
	ModuleStages lastModuleStage = MODULE_STAGE_INIT;
	ModuleStates lastModuleState = MODULE_STATE_READY;
	ModuleStages moduleStage = MODULE_STAGE_INIT;
	ModuleStates moduleState = MODULE_STATE_READY;
	StepStates stepState = STEP_STATE_READY;

	std::chrono::time_point<std::chrono::steady_clock> startTime;

	int* metronomeCounterPtr;

	int currentDelayTime;
	int currentCounters[3] = { 0,0,0 };
	int maxCounters[3] = { 1,1,1 };
	int metronomeSpeed = 60;
	int metronomeSteps = 0;
	int metronomeStepsDone = 0;
	static const int kClockDivider = 64;

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
	dsp::PulseGenerator pgOutTriggers[4];

	dsp::ClockDivider clockDivider;

	Brainz() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		configSwitch(PARAM_MODULE_DIRECTION, 0.f, 2.f, 0.f, "Module cycle", { "Bidirectional", "Forward", "Backward" });
		configSwitch(PARAM_LOGIC_ENABLED, 0.f, 1.f, 1.f, "Toggle logic", { "Disabled", "Enabled" });

		configParam(PARAM_A_DELAY, 1.f, 99.f, 1.f, "Trigger A delay", " seconds");
		paramQuantities[PARAM_A_DELAY]->snapEnabled = true;
		configParam(PARAM_B_DELAY, 1.f, 99.f, 1.f, "Trigger B delay", " seconds");
		paramQuantities[PARAM_B_DELAY]->snapEnabled = true;
		configParam(PARAM_C_DELAY, 1.f, 99.f, 1.f, "Trigger C delay", " seconds");
		paramQuantities[PARAM_C_DELAY]->snapEnabled = true;
		configParam(PARAM_METRONOME_SPEED, 15.f, 99.f, 60.f, "BPM", " beats");
		paramQuantities[PARAM_METRONOME_SPEED]->snapEnabled = true;
		configParam(PARAM_METRONOME_STEPS, 5.f, 99.f, 10.f, "Steps", "");
		paramQuantities[PARAM_METRONOME_STEPS]->snapEnabled = true;

		configButton(PARAM_A_ENABLED, "Step A enabled");
		configButton(PARAM_B_ENABLED, "Step B enabled");
		configButton(PARAM_C_ENABLED, "Step C enabled");

		configButton(PARAM_START_TRIGGERS, "Toggle global triggers on start");
		params[PARAM_START_TRIGGERS].setValue(1);
		configButton(PARAM_END_TRIGGERS, "Toggle global triggers on end");
		params[PARAM_END_TRIGGERS].setValue(1);

		configButton(PARAM_A_DO_TRIGGERS, "Send global triggers at the end of coundown A");
		configButton(PARAM_B_DO_TRIGGERS, "Send global triggers at the end of coundown B");
		configButton(PARAM_C_DO_TRIGGERS, "Send global triggers at the end of coundown C");

		configButton(PARAM_A_IS_METRONOME, "Step A is metronome");
		configButton(PARAM_B_IS_METRONOME, "Step B is metronome");
		configButton(PARAM_C_IS_METRONOME, "Step C is metronome");

		configSwitch(PARAM_A_DIRECTION, 0.f, 2.f, 0.f, "Step A direction", { "Bidirectional", "Forward", "Backward" });
		configSwitch(PARAM_B_DIRECTION, 0.f, 2.f, 0.f, "Step A direction", { "Bidirectional", "Forward", "Backward" });

		configInput(INPUT_TRIGGER, "Start/stop trigger");
		configInput(INPUT_RESET, "Reset trigger");

		configOutput(OUTPUT_RUN, "Run pass-through");
		configOutput(OUTPUT_RESET, "Reset");
		configOutput(OUTPUT_METRONOME, "Metronome audio");
		configOutput(OUTPUT_OUT_1, "Global trigger 1");
		configOutput(OUTPUT_OUT_2, "Global trigger 2");
		configOutput(OUTPUT_OUT_3, "Global trigger 3");
		configOutput(OUTPUT_OUT_4, "Global trigger 4");

		configOutput(OUTPUT_STAGE_A, "Stage A end of count");
		configOutput(OUTPUT_STAGE_B, "Stage B end of count");
		configOutput(OUTPUT_STAGE_C, "Stage C end of count");

		configButton(PARAM_ONE_SHOT, "Toggle one-shot");
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
		}
		else {
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
					switch (moduleState)
					{
					case MODULE_STATE_READY: {
						break;
					}

					case MODULE_STATE_ROUND_1_START: {
						resetGlobalTriggers();
						if (params[PARAM_START_TRIGGERS].getValue()) {
							doGlobalTriggers(sampleTime);
						}
						else {
							for (int i = 0; i < 4; i++)
								bTriggersDone[i] = true;
						}

						if (bTriggersDone[0] && bTriggersDone[1] && bTriggersDone[2] && bTriggersDone[3]) {
							resetStep();
							if (bStepEnabled[0]) {
								moduleState = MODULE_STATE_ROUND_1_STEP_A;
							}
							else if (stepDirections[0] < DIRECTION_BACKWARD && bStepEnabled[1]) {
								moduleState = MODULE_STATE_ROUND_1_STEP_B;
							}
							else if (bStepEnabled[2] && stepDirections[1] < DIRECTION_BACKWARD) {
								moduleState = MODULE_STATE_ROUND_1_STEP_C;
							}
							else {
								moduleState = MODULE_STATE_ROUND_1_END;
							}
						}
						break;
					}

					case MODULE_STATE_ROUND_1_STEP_A: {
						resetGlobalTriggers();
						if (params[PARAM_A_IS_METRONOME].getValue()) {
							if (!bEnteredMetronome) {
								setupMetronome(&currentCounters[0]);
							}
							else {
								if (!bStepStarted) {
									setupAfterMetronomeTriggers();
								}
								else {
									handleAfterMetronomeTriggers(OUTPUT_STAGE_A, sampleTime);

									doEndOfStepTriggers(PARAM_A_DO_TRIGGERS, sampleTime);
								}
							}
						}
						else {
							if (!bStepStarted) {
								setupStep(maxCounters[0]);
							}
							else {
								doStepTrigger(OUTPUT_STAGE_A, &currentCounters[0], sampleTime);
								doEndOfStepTriggers(PARAM_A_DO_TRIGGERS, sampleTime);
							}
						}

						if (bTriggersDone[0] && bTriggersDone[1] && bTriggersDone[2] && bTriggersDone[3]) {
							if (bStepEnabled[1] && stepDirections[0] < DIRECTION_BACKWARD) {
								moduleState = MODULE_STATE_ROUND_1_STEP_B;
							}
							else if (bStepEnabled[2] && stepDirections[1] < DIRECTION_BACKWARD) {
								moduleState = MODULE_STATE_ROUND_1_STEP_C;
							}
							else {
								moduleState = MODULE_STATE_ROUND_1_END;
							}
							resetStep();
						}
						break;
					}

					case MODULE_STATE_ROUND_1_STEP_B: {
						resetGlobalTriggers();
						if (params[PARAM_B_IS_METRONOME].getValue()) {
							if (!bEnteredMetronome) {
								setupMetronome(&currentCounters[1]);
							}
							else {
								if (!bStepStarted) {
									setupAfterMetronomeTriggers();
								}
								else {
									handleAfterMetronomeTriggers(OUTPUT_STAGE_B, sampleTime);

									doEndOfStepTriggers(PARAM_B_DO_TRIGGERS, sampleTime);
								}
							}
						}
						else {
							if (!bStepStarted) {
								setupStep(maxCounters[1]);
							}
							else {
								doStepTrigger(OUTPUT_STAGE_B, &currentCounters[1], sampleTime);
								doEndOfStepTriggers(PARAM_B_DO_TRIGGERS, sampleTime);
							}
						}

						if (bTriggersDone[0] && bTriggersDone[1] && bTriggersDone[2] && bTriggersDone[3]) {
							if (bStepEnabled[2] && stepDirections[1] < DIRECTION_BACKWARD) {
								moduleState = MODULE_STATE_ROUND_1_STEP_C;
							}
							else {
								moduleState = MODULE_STATE_ROUND_1_END;
							}
							resetStep();
						}
						break;
					}

					case MODULE_STATE_ROUND_1_STEP_C: {
						resetGlobalTriggers();
						if (params[PARAM_C_IS_METRONOME].getValue()) {
							if (!bEnteredMetronome) {
								setupMetronome(&currentCounters[2]);
							}
							else {
								if (!bStepStarted) {
									setupAfterMetronomeTriggers();
								}
								else {
									handleAfterMetronomeTriggers(OUTPUT_STAGE_C, sampleTime);

									doEndOfStepTriggers(PARAM_C_DO_TRIGGERS, sampleTime);
								}
							}
						}
						else {
							if (!bStepStarted) {
								setupStep(maxCounters[2]);
							}
							else {
								doStepTrigger(OUTPUT_STAGE_C, &currentCounters[2], sampleTime);
								doEndOfStepTriggers(PARAM_C_DO_TRIGGERS, sampleTime);
							}

							if (bTriggersDone[0] && bTriggersDone[1] && bTriggersDone[2] && bTriggersDone[3]) {
								if (moduleDirection == DIRECTION_BIDIRECTIONAL) {
									moduleState = MODULE_STATE_ROUND_1_END;
									stepState = STEP_STATE_READY;
								}
								else {
									if (!params[PARAM_ONE_SHOT].getValue()) {
										moduleState = MODULE_STATE_READY;
										moduleStage = MODULE_STAGE_INIT;
									}
									else {
										moduleState = MODULE_STATE_WAIT_FOR_RESET;
										moduleStage = MODULE_STAGE_ONE_SHOT_END;
									}
								}
								resetStep();
							}
						}
						break;
					}

					case MODULE_STATE_ROUND_1_END: {
						break;
					}

					case MODULE_STATE_ROUND_2_START: {
						resetStep();
						if (bStepEnabled[2]) {
							moduleState = MODULE_STATE_ROUND_2_STEP_C;
						}
						else if ((stepDirections[1] == DIRECTION_BIDIRECTIONAL || stepDirections[1] == DIRECTION_BACKWARD) && bStepEnabled[1]) {
							moduleState = MODULE_STATE_ROUND_2_STEP_B;
						}
						else if (bStepEnabled[0] && (stepDirections[0] == DIRECTION_BIDIRECTIONAL || stepDirections[0] == DIRECTION_BACKWARD)) {
							moduleState = MODULE_STATE_ROUND_2_STEP_A;
						}
						else {
							moduleState = MODULE_STATE_ROUND_2_END;
						}
						break;
					}

					case MODULE_STATE_ROUND_2_STEP_C: {
						resetGlobalTriggers();
						if (params[PARAM_C_IS_METRONOME].getValue()) {
							if (!bEnteredMetronome) {
								setupMetronome(&currentCounters[2]);
							}
							else {
								if (!bStepStarted) {
									setupAfterMetronomeTriggers();
								}
								else {
									handleAfterMetronomeTriggers(OUTPUT_STAGE_C, sampleTime);

									doEndOfStepTriggers(PARAM_C_DO_TRIGGERS, sampleTime);
								}
							}
						}
						else {
							if (!bStepStarted) {
								setupStep(maxCounters[2]);
							}
							else {
								doStepTrigger(OUTPUT_STAGE_C, &currentCounters[2], sampleTime);
								doEndOfStepTriggers(PARAM_C_DO_TRIGGERS, sampleTime);
							}
						}

						if (bTriggersDone[0] && bTriggersDone[1] && bTriggersDone[2] && bTriggersDone[3]) {
							if (bStepEnabled[1] && (stepDirections[1] == DIRECTION_BACKWARD || stepDirections[1] == DIRECTION_BIDIRECTIONAL)) {
								moduleState = MODULE_STATE_ROUND_2_STEP_B;
							}
							else if (bStepEnabled[0] && (stepDirections[0] == DIRECTION_BACKWARD || stepDirections[0] == DIRECTION_BIDIRECTIONAL)) {
								moduleState = MODULE_STATE_ROUND_2_STEP_A;
							}
							else {
								moduleState = MODULE_STATE_ROUND_2_END;
							}
							resetStep();
						}
						break;
					}

					case MODULE_STATE_ROUND_2_STEP_B: {
						resetGlobalTriggers();
						if (params[PARAM_B_IS_METRONOME].getValue()) {
							if (!bEnteredMetronome) {
								setupMetronome(&currentCounters[1]);
							}
							else {
								if (!bStepStarted) {
									setupAfterMetronomeTriggers();
								}
								else {
									handleAfterMetronomeTriggers(OUTPUT_STAGE_B, sampleTime);

									doEndOfStepTriggers(PARAM_B_DO_TRIGGERS, sampleTime);
								}
							}
						}
						else {
							if (!bStepStarted) {
								setupStep(maxCounters[1]);
							}
							else {
								doStepTrigger(OUTPUT_STAGE_B, &currentCounters[1], sampleTime);
								doEndOfStepTriggers(PARAM_B_DO_TRIGGERS, sampleTime);
							}
						}

						if (bTriggersDone[0] && bTriggersDone[1] && bTriggersDone[2] && bTriggersDone[3]) {
							if (bStepEnabled[0] && (stepDirections[0] == DIRECTION_BACKWARD || stepDirections[0] == DIRECTION_BIDIRECTIONAL)) {
								moduleState = MODULE_STATE_ROUND_2_STEP_A;
							}
							else {
								moduleState = MODULE_STATE_ROUND_2_END;
							}
							resetStep();
						}
						break;
					}

					case MODULE_STATE_ROUND_2_STEP_A: {
						resetGlobalTriggers();
						if (params[PARAM_A_IS_METRONOME].getValue()) {
							if (!bEnteredMetronome) {
								setupMetronome(&currentCounters[0]);
							}
							else {
								if (!bStepStarted) {
									setupAfterMetronomeTriggers();
								}
								else {
									handleAfterMetronomeTriggers(OUTPUT_STAGE_A, sampleTime);

									doEndOfStepTriggers(PARAM_A_DO_TRIGGERS, sampleTime);
								}
							}
						}
						else {
							if (!bStepStarted) {
								setupStep(maxCounters[0]);
							}
							else {
								doStepTrigger(OUTPUT_STAGE_A, &currentCounters[0], sampleTime);
								doEndOfStepTriggers(PARAM_A_DO_TRIGGERS, sampleTime);
							}
						}

						if (bTriggersDone[0] && bTriggersDone[1] && bTriggersDone[2] && bTriggersDone[3]) {
							moduleState = MODULE_STATE_ROUND_2_END;
							resetStep();
						}
						break;
					}

					case MODULE_STATE_ROUND_2_END: {
						resetGlobalTriggers();
						if (params[PARAM_END_TRIGGERS].getValue()) {
							doGlobalTriggers(sampleTime);
						}
						else {
							for (int i = 0; i < 4; i++) {
								bTriggersDone[i] = true;
							}
						}

						if (bTriggersDone[0] && bTriggersDone[1] && bTriggersDone[2] && bTriggersDone[3]) {
							if (!params[PARAM_ONE_SHOT].getValue()) {
								moduleState = MODULE_STATE_READY;
								moduleStage = MODULE_STAGE_INIT;
							}
							else {
								moduleState = MODULE_STATE_WAIT_FOR_RESET;
								moduleStage = MODULE_STAGE_ONE_SHOT_END;
							}
						}
						break;
					}

					case MODULE_STATE_WAIT_FOR_RESET: {
						break;
					}
					}
				}

				metronomeSpeed = params[PARAM_METRONOME_SPEED].getValue();
				metronomeSteps = params[PARAM_METRONOME_STEPS].getValue();

				for (int i = 0; i < 3; i++) {
					bStepEnabled[i] = params[PARAM_A_ENABLED + i].getValue();

					lights[LIGHT_STEP_A_ENABLED + i].setBrightnessSmooth(params[PARAM_A_ENABLED + i].getValue(), sampleTime);
					lights[LIGHT_STEP_A_TRIGGERS + i].setBrightnessSmooth(params[PARAM_A_DO_TRIGGERS + i].getValue(), sampleTime);
					lights[LIGHT_STEP_A_METRONOME + i].setBrightnessSmooth(params[PARAM_A_IS_METRONOME + i].getValue(), sampleTime);

					if (i < 2) {
						stepDirections[i] = StepDirections(params[PARAM_A_DIRECTION + i].getValue());

						int currentLight = LIGHT_STEP_A_DIRECTION + i * 3;
						lights[currentLight + 0].setBrightness(stepDirectionsLightColors[stepDirections[i]].red);
						lights[currentLight + 1].setBrightness(stepDirectionsLightColors[stepDirections[i]].green);
						lights[currentLight + 2].setBrightness(stepDirectionsLightColors[stepDirections[i]].blue);
					}

					if (!params[PARAM_A_IS_METRONOME + i].getValue()) {
						maxCounters[i] = params[PARAM_A_DELAY + i].getValue();
					}
					else {
						maxCounters[i] = 0;
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
				}
				else
				{
					lights[LIGHT_MODULE_STAGE + 0].setBrightnessSmooth(0.f, sampleTime);
					lights[LIGHT_MODULE_STAGE + 1].setBrightnessSmooth(0.f, sampleTime);
					lights[LIGHT_MODULE_STAGE + 2].setBrightnessSmooth(0.f, sampleTime);
				}



				lights[LIGHT_START_TRIGGERS].setBrightnessSmooth(params[PARAM_START_TRIGGERS].getValue(), sampleTime);
				lights[LIGHT_END_TRIGGERS].setBrightnessSmooth(params[PARAM_END_TRIGGERS].getValue(), sampleTime);

				lights[LIGHT_OUT_ENABLED].setBrightnessSmooth(params[PARAM_LOGIC_ENABLED].getValue() ? 0.f : 1.f, sampleTime);
				lights[LIGHT_OUT_ENABLED + 1].setBrightnessSmooth(1.f, sampleTime);
				for (int i = 2; i < 7; i++) {
					lights[LIGHT_OUT_ENABLED + i].setBrightnessSmooth(params[PARAM_LOGIC_ENABLED].getValue() ? 1.f : 0.f, sampleTime);
				}

				handleStepLights(sampleTime);

				if (moduleState == MODULE_STATE_READY) {
					moduleDirection = StepDirections(params[PARAM_MODULE_DIRECTION].getValue());
					if (moduleState < MODULE_STATE_WAIT_FOR_RESET) {
						moduleStage = MODULE_STAGE_INIT;
					}
					else {
						moduleStage = MODULE_STAGE_ONE_SHOT_END;
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
		for (int i = 0; i < 3; i++) {
			params[PARAM_A_ENABLED + i].setValue(1);
			if (i < 2) {
				params[PARAM_START_TRIGGERS + i].setValue(1);
			}
		}

	}

	void resetGlobalTriggers() {
		memset(bTriggersDone, 0, sizeof(bool) * 4);
	}

	void doGlobalTriggers(const float sampleTime) {
		if (!bTriggersSent) {
			for (int i = 0; i < 4; i++) {
				if (outputs[OUTPUT_OUT_1 + i].isConnected()) {
					pgOutTriggers[i].trigger();
					outputs[OUTPUT_OUT_1 + i].setVoltage(pgOutTriggers[i].process(1.0f / sampleTime) ? 10.f : 0.f);
				}
			}
			bTriggersSent = true;
		}
		else {
			for (int i = 0; i < 4; i++) {
				bTriggersDone[i] = !pgOutTriggers[i].process(1.0f / sampleTime);
				if (outputs[OUTPUT_OUT_1 + i].isConnected()) {
					outputs[OUTPUT_OUT_1 + i].setVoltage(bTriggersDone[i] ? 0.f : 10.f);
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
			metronomeStepsDone++;
			*metronomeCounterPtr = metronomeStepsDone;
		}
		metronomeCounter++;

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
		if (stepState < STEP_STATE_TRIGGER_SENT) {
			if (outputs[output].isConnected()) {
				pgTrigger.trigger();
				outputs[output].setVoltage(pgTrigger.process(1.0f / sampleTime) ? 10.f : 0.f);
			}
			stepState = STEP_STATE_TRIGGER_SENT;
		}
		else {
			bStepTrigger = pgTrigger.process(1.0f / sampleTime);
			if (outputs[output].isConnected()) {
				outputs[output].setVoltage(bStepTrigger ? 10.f : 0.f);
			}
			if (!bStepTrigger) {
				stepState = STEP_STATE_TRIGGER_DONE;
			}
		}
	}

	void handleRunTriggers() {
		if (params[PARAM_LOGIC_ENABLED].getValue()) {
			if (bInMetronome) {
				bInMetronome = false;
				killVoltages();
				moduleState = MODULE_STATE_READY;
				moduleStage = MODULE_STAGE_INIT;
			}
			else {
				switch (moduleState)
				{
				case MODULE_STATE_READY: {
					bTriggersSent = false;
					if (moduleDirection < DIRECTION_BACKWARD) {
						moduleStage = MODULE_STAGE_ROUND_1;
						moduleState = MODULE_STATE_ROUND_1_START;
					}
					else if (moduleDirection == DIRECTION_BACKWARD) {
						moduleState = MODULE_STATE_ROUND_2_START;
						moduleStage = MODULE_STAGE_ROUND_2;
					}
					break;
				}
				case MODULE_STATE_ROUND_1_START:
				case MODULE_STATE_ROUND_1_STEP_A:
				case MODULE_STATE_ROUND_1_STEP_B:
				case MODULE_STATE_ROUND_1_STEP_C: {
					killVoltages();
					moduleState = MODULE_STATE_READY;
					moduleStage = MODULE_STAGE_INIT;
					break;
				}
				case MODULE_STATE_ROUND_1_END: {
					if (moduleDirection == DIRECTION_BACKWARD || moduleDirection == DIRECTION_BIDIRECTIONAL) {
						memset(currentCounters, 0, sizeof(int) * 3);
						moduleStage = MODULE_STAGE_ROUND_2;
						moduleState = MODULE_STATE_ROUND_2_START;
					}
					break;
				}
				case MODULE_STATE_ROUND_2_START:
				case MODULE_STATE_ROUND_2_STEP_A:
				case MODULE_STATE_ROUND_2_STEP_B:
				case MODULE_STATE_ROUND_2_STEP_C:
				case MODULE_STATE_ROUND_2_END: {
					killVoltages();
					moduleState = MODULE_STATE_READY;
					moduleStage = MODULE_STAGE_INIT;
					break;
				}
				case MODULE_STATE_WAIT_FOR_RESET: {
					break;
				}
				}
			}
		}
		else {
			if (outputs[OUTPUT_RUN].isConnected()) {
				bRunSent = true;
				pgRun.trigger();
			}
		}
	}

	void handleResetTriggers() {
		bInMetronome = false;
		killVoltages();
		memset(currentCounters, 0, sizeof(int) * 3);
		metronomeStepsDone = 0;
		moduleState = MODULE_STATE_READY;
		moduleStage = MODULE_STAGE_INIT;
		if (outputs[OUTPUT_RESET].isConnected()) {
			bResetSent = true;
			pgReset.trigger();
		}
	}

	void killVoltages() {
		for (int i = 0; i < INPUTS_COUNT; i++) {
			outputs[OUTPUT_METRONOME + i].setVoltage(0);
		}
	}

	void setupStep(int delayTime) {
		startTime = std::chrono::steady_clock::now();
		bStepStarted = true;
		bTriggersSent = false;
		currentDelayTime = delayTime * 1000;
	}

	void doStepTrigger(OutputIds output, int* counter, const float sampleTime) {
		if (stepState < STEP_STATE_TRIGGER_SENT) {
			std::chrono::time_point<std::chrono::steady_clock> currentTime = std::chrono::steady_clock::now();
			int elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();
			*counter = elapsedTime / 1000;
			if (elapsedTime >= currentDelayTime) {
				if (outputs[output].isConnected()) {
					pgTrigger.trigger();
					outputs[output].setVoltage(pgTrigger.process(1.0f / sampleTime) ? 10.f : 0.f);
				}
				stepState = STEP_STATE_TRIGGER_SENT;
			}
		}
		else {
			bStepTrigger = pgTrigger.process(1.0f / sampleTime);
			if (outputs[output].isConnected()) {
				outputs[output].setVoltage(bStepTrigger ? 10.f : 0.f);
			}
			if (!bStepTrigger) {
				stepState = STEP_STATE_TRIGGER_DONE;
			}
		}
	}

	void doEndOfStepTriggers(ParamIds checkParam, const float sampleTime) {
		if (bStepStarted && stepState == STEP_STATE_TRIGGER_DONE) {
			if (!params[checkParam].getValue()) {
				for (int i = 0; i < 4; i++) {
					bTriggersDone[i] = true;
				}
			}
			else {
				doGlobalTriggers(sampleTime);
			}
		}
	}

	void resetStep() {
		bStepStarted = false;
		bStepTrigger = false;
		bEnteredMetronome = false;
		stepState = STEP_STATE_READY;
	}

	void handleStepLights(float sampleTime) {
		lights[LIGHT_STEP_A].setBrightnessSmooth((moduleState == MODULE_STATE_ROUND_1_STEP_A || moduleState == MODULE_STATE_ROUND_2_STEP_A) ? 1.0f : 0.f, sampleTime);
		lights[LIGHT_STEP_B].setBrightnessSmooth((moduleState == MODULE_STATE_ROUND_1_STEP_B || moduleState == MODULE_STATE_ROUND_2_STEP_B) ? 1.0f : 0.f, sampleTime);
		lights[LIGHT_STEP_C].setBrightnessSmooth((moduleState == MODULE_STATE_ROUND_1_STEP_C || moduleState == MODULE_STATE_ROUND_2_STEP_C) ? 1.0f : 0.f, sampleTime);
		lights[LIGHT_METRONOME].setBrightnessSmooth(bInMetronome ? 1.0f : 0.f, sampleTime);
	}
};

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

struct BrainzWidget : ModuleWidget {
	BrainzWidget(Brainz* module) {
		setModule(module);

		SanguinePanel* panel = new SanguinePanel("res/backplate_25hp_purple.svg", "res/brainz.svg");
		setPanel(panel);

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<SeqButtonPlay>(millimetersToPixelsVec(97.39, 11.87), module, Brainz::PARAM_PLAY_BUTTON));

		addParam(createParamCentered<SeqButtonReset>(millimetersToPixelsVec(109.556, 11.87), module, Brainz::PARAM_RESET_BUTTON));

		CKD6* btnModuleDirection = createParamCentered<CKD6>(millimetersToPixelsVec(81.319, 59.888), module, Brainz::PARAM_MODULE_DIRECTION);
		btnModuleDirection->momentary = false;
		btnModuleDirection->latch = true;
		addParam(btnModuleDirection);
		addChild(createLightCentered<CKD6Light<RedGreenBlueLight>>(millimetersToPixelsVec(81.319, 59.888), module, Brainz::LIGHT_MODULE_DIRECTION));

		addChild(createLightCentered<LargeLight<RedGreenBlueLight>>(millimetersToPixelsVec(119.637, 11.906), module, Brainz::LIGHT_MODULE_STAGE));

		Befaco2StepSwitch* switchLogicEnabled = createParamCentered<Befaco2StepSwitch>(millimetersToPixelsVec(63.5, 28.771), module, Brainz::PARAM_LOGIC_ENABLED);
		switchLogicEnabled->momentary = false;
		addParam(switchLogicEnabled);
		addChild(createLightCentered<LargeLight<RedLight>>(millimetersToPixelsVec(63.5, 38.373), module, Brainz::LIGHT_LOGIC_ENABLED));

		addParam(createParamCentered<BefacoTinyKnobRed>(millimetersToPixelsVec(27.568, 25.114), module, Brainz::PARAM_A_DELAY));
		addParam(createParamCentered<BefacoTinyKnobRed>(millimetersToPixelsVec(27.568, 76.144), module, Brainz::PARAM_B_DELAY));
		addParam(createParamCentered<BefacoTinyKnobRed>(millimetersToPixelsVec(99.539, 76.144), module, Brainz::PARAM_C_DELAY));
		addParam(createParamCentered<BefacoTinyKnobRed>(millimetersToPixelsVec(95.045, 28.914), module, Brainz::PARAM_METRONOME_SPEED));
		addParam(createParamCentered<BefacoTinyKnobBlack>(millimetersToPixelsVec(81.488, 40.647), module, Brainz::PARAM_METRONOME_STEPS));

		addParam(createLightParamCentered<VCVLightBezelLatch<OrangeLight>>(millimetersToPixelsVec(47.535, 25.114), module, Brainz::PARAM_A_ENABLED, Brainz::LIGHT_STEP_A_ENABLED));
		addParam(createLightParamCentered<VCVLightBezelLatch<OrangeLight>>(millimetersToPixelsVec(47.535, 76.144), module, Brainz::PARAM_B_ENABLED, Brainz::LIGHT_STEP_B_ENABLED));
		addParam(createLightParamCentered<VCVLightBezelLatch<OrangeLight>>(millimetersToPixelsVec(119.321, 76.144), module, Brainz::PARAM_C_ENABLED, Brainz::LIGHT_STEP_C_ENABLED));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<BlueLight>>>(millimetersToPixelsVec(7.301, 43.647),
			module, Brainz::PARAM_A_DO_TRIGGERS, Brainz::LIGHT_STEP_A_TRIGGERS));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(millimetersToPixelsVec(25.177, 43.647),
			module, Brainz::PARAM_A_IS_METRONOME, Brainz::LIGHT_STEP_A_METRONOME));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<BlueLight>>>(millimetersToPixelsVec(7.301, 94.677),
			module, Brainz::PARAM_B_DO_TRIGGERS, Brainz::LIGHT_STEP_B_TRIGGERS));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(millimetersToPixelsVec(25.177, 94.677),
			module, Brainz::PARAM_B_IS_METRONOME, Brainz::LIGHT_STEP_B_METRONOME));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<BlueLight>>>(millimetersToPixelsVec(79.288, 94.677),
			module, Brainz::PARAM_C_DO_TRIGGERS, Brainz::LIGHT_STEP_C_TRIGGERS));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(millimetersToPixelsVec(97.148, 94.677),
			module, Brainz::PARAM_C_IS_METRONOME, Brainz::LIGHT_STEP_C_METRONOME));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedLight>>>(millimetersToPixelsVec(108.308, 54.211),
			module, Brainz::PARAM_START_TRIGGERS, Brainz::LIGHT_START_TRIGGERS));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedLight>>>(millimetersToPixelsVec(108.308, 65.543),
			module, Brainz::PARAM_END_TRIGGERS, Brainz::LIGHT_END_TRIGGERS));

		addOutput(createOutputCentered<BananutBlack>(millimetersToPixelsVec(46.835, 43.647), module, Brainz::OUTPUT_STAGE_A));
		addOutput(createOutputCentered<BananutBlack>(millimetersToPixelsVec(46.835, 94.677), module, Brainz::OUTPUT_STAGE_B));
		addOutput(createOutputCentered<BananutBlack>(millimetersToPixelsVec(118.821, 94.677), module, Brainz::OUTPUT_STAGE_C));

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

		addChild(createLightCentered<SmallLight<RedLight>>(millimetersToPixelsVec(13.628, 25.114), module, Brainz::LIGHT_STEP_A));
		addChild(createLightCentered<SmallLight<RedLight>>(millimetersToPixelsVec(13.628, 76.144), module, Brainz::LIGHT_STEP_B));
		addChild(createLightCentered<SmallLight<RedLight>>(millimetersToPixelsVec(85.615, 76.144), module, Brainz::LIGHT_STEP_C));
		addChild(createLightCentered<SmallLight<RedLight>>(millimetersToPixelsVec(85.615, 25.114), module, Brainz::LIGHT_METRONOME));

		float currentX = 63.456;
		float deltaX = 9.74;
		for (int i = 0; i < 7; i++) {
			addChild(createLightCentered<TinyLight<YellowLight>>(millimetersToPixelsVec(currentX, 109.601), module, Brainz::LIGHT_OUT_ENABLED + i));
			currentX += deltaX;
		}

		addParam(createParam<SeqButtonOneShotSmall>(millimetersToPixelsVec(91.231, 57.888), module, Brainz::PARAM_ONE_SHOT));

		FramebufferWidget* brainzFrameBuffer = new FramebufferWidget();
		addChild(brainzFrameBuffer);

		SanguineTinyNumericDisplay* displayStepsACurrent = new SanguineTinyNumericDisplay(2, module, 18.929, 34.908);
		brainzFrameBuffer->addChild(displayStepsACurrent);

		if (module)
			displayStepsACurrent->values.numberValue = &module->currentCounters[0];

		SanguineTinyNumericDisplay* displayStepsATotal = new SanguineTinyNumericDisplay(2, module, 36.207, 34.908);
		brainzFrameBuffer->addChild(displayStepsATotal);
		displayStepsATotal->fallbackNumber = 1;

		if (module)
			displayStepsATotal->values.numberValue = &module->maxCounters[0];

		SanguineTinyNumericDisplay* displayStepsBCurrent = new SanguineTinyNumericDisplay(2, module, 18.929, 85.938);
		brainzFrameBuffer->addChild(displayStepsBCurrent);

		if (module)
			displayStepsBCurrent->values.numberValue = &module->currentCounters[1];

		SanguineTinyNumericDisplay* displayStepsBTotal = new SanguineTinyNumericDisplay(2, module, 36.207, 85.938);
		brainzFrameBuffer->addChild(displayStepsBTotal);
		displayStepsBTotal->fallbackNumber = 1;

		if (module)
			displayStepsBTotal->values.numberValue = &module->maxCounters[1];

		SanguineTinyNumericDisplay* displayStepsCCurrent = new SanguineTinyNumericDisplay(2, module, 90.891, 85.938);
		brainzFrameBuffer->addChild(displayStepsCCurrent);

		if (module)
			displayStepsCCurrent->values.numberValue = &module->currentCounters[2];

		SanguineTinyNumericDisplay* displayStepsCTotal = new SanguineTinyNumericDisplay(2, module, 108.181, 85.938);
		brainzFrameBuffer->addChild(displayStepsCTotal);
		displayStepsCTotal->fallbackNumber = 1;

		if (module)
			displayStepsCTotal->values.numberValue = &module->maxCounters[2];

		SanguineTinyNumericDisplay* displayMetronomeSpeed = new SanguineTinyNumericDisplay(2, module, 110.857, 28.914);
		brainzFrameBuffer->addChild(displayMetronomeSpeed);
		displayMetronomeSpeed->fallbackNumber = 60;

		if (module)
			displayMetronomeSpeed->values.numberValue = &module->metronomeSpeed;

		SanguineTinyNumericDisplay* displayMetronomeCurrentStep = new SanguineTinyNumericDisplay(2, module, 95.045, 40.647);
		brainzFrameBuffer->addChild(displayMetronomeCurrentStep);

		if (module)
			displayMetronomeCurrentStep->values.numberValue = &module->metronomeStepsDone;

		SanguineTinyNumericDisplay* displayMetronomeTotalSteps = new SanguineTinyNumericDisplay(2, module, 110.857, 40.647);
		brainzFrameBuffer->addChild(displayMetronomeTotalSteps);
		displayMetronomeTotalSteps->fallbackNumber = 10;

		if (module)
			displayMetronomeTotalSteps->values.numberValue = &module->metronomeSteps;

		SanguineStaticRGBLight* inPlayLight = new SanguineStaticRGBLight(module, "res/play_lit.svg", 7.402, 109.601, true, kSanguineYellowLight);
		addChild(inPlayLight);

		SanguineStaticRGBLight* resetLight = new SanguineStaticRGBLight(module, "res/reset_lit.svg", 20.367, 109.601, true, kSanguineYellowLight);
		addChild(resetLight);

		SanguineStaticRGBLight* metronomeStepsLight = new SanguineStaticRGBLight(module, "res/numpad_lit.svg", 120.921, 40.647, true, kSanguineBlueLight);
		addChild(metronomeStepsLight);

		YellowGateLight* gateLightA = new YellowGateLight(module, 39.454, 43.647);
		addChild(gateLightA);

		YellowGateLight* gateLightB = new YellowGateLight(module, 39.454, 94.677);
		addChild(gateLightB);

		YellowGateLight* gateLightC = new YellowGateLight(module, 111.441, 94.677);
		addChild(gateLightC);

		BluePowerLight* powerLightA = new BluePowerLight(module, 40.675, 25.114);
		addChild(powerLightA);

		BluePowerLight* powerLightB = new BluePowerLight(module, 40.675, 76.144);
		addChild(powerLightB);

		BluePowerLight* powerLightC = new BluePowerLight(module, 112.461, 76.144);
		addChild(powerLightC);

		BlueAdvancedClockLight* steppedClockLightA = new BlueAdvancedClockLight(module, 6.201, 34.743);
		addChild(steppedClockLightA);

		BlueAdvancedClockLight* steppedClockLightB = new BlueAdvancedClockLight(module, 6.201, 85.773);
		addChild(steppedClockLightB);

		BlueAdvancedClockLight* steppedClockLightC = new BlueAdvancedClockLight(module, 78.151, 85.773);
		addChild(steppedClockLightC);

		BlueInitialClockLight* initialClockLightA = new BlueInitialClockLight(module, 48.935, 34.908);
		addChild(initialClockLightA);

		BlueInitialClockLight* initialClockLightB = new BlueInitialClockLight(module, 48.935, 85.938);
		addChild(initialClockLightB);

		BlueInitialClockLight* initialClockLightC = new BlueInitialClockLight(module, 120.921, 85.938);
		addChild(initialClockLightC);

		BlueRightArrowLight* arrowLightA = new BlueRightArrowLight(module, 13.471, 43.647);
		addChild(arrowLightA);

		BlueRightArrowLight* arrowLightB = new BlueRightArrowLight(module, 13.471, 94.677);
		addChild(arrowLightB);

		BlueRightArrowLight* arrowLightC = new BlueRightArrowLight(module, 85.426, 94.677);
		addChild(arrowLightC);

		BlueQuarterNoteLight* quarterNoteLightA = new BlueQuarterNoteLight(module, 30.167, 43.647);
		addChild(quarterNoteLightA);

		BlueQuarterNoteLight* quarterNoteLightB = new BlueQuarterNoteLight(module, 30.167, 94.677);
		addChild(quarterNoteLightB);

		BlueQuarterNoteLight* quarterNoteLightC = new BlueQuarterNoteLight(module, 102.138, 94.677);
		addChild(quarterNoteLightC);

		BlueQuarterNoteLight* quarterNoteLightMetronome = new BlueQuarterNoteLight(module, 120.921, 28.914);
		addChild(quarterNoteLightMetronome);

		SanguineStaticRGBLight* outPlayLight = new SanguineStaticRGBLight(module, "res/play_lit.svg", 59.739, 109.601, true, kSanguineYellowLight);
		addChild(outPlayLight);

		SanguineStaticRGBLight* outResetLight = new SanguineStaticRGBLight(module, "res/reset_lit.svg", 69.799, 109.601, true, kSanguineYellowLight);
		addChild(outResetLight);

		SanguineStaticRGBLight* outMetronomeLight = new SanguineStaticRGBLight(module, "res/clock_lit.svg", 79.541, 109.601, true, kSanguineYellowLight);
		addChild(outMetronomeLight);

		SanguineStaticRGBLight* outLight1 = new SanguineStaticRGBLight(module, "res/number_1_lit.svg", 88.888, 109.601, true, kSanguineYellowLight);
		addChild(outLight1);

		SanguineStaticRGBLight* outLight2 = new SanguineStaticRGBLight(module, "res/number_2_lit.svg", 98.652, 109.601, true, kSanguineYellowLight);
		addChild(outLight2);

		SanguineStaticRGBLight* outLight3 = new SanguineStaticRGBLight(module, "res/number_3_lit.svg", 108.321, 109.601, true, kSanguineYellowLight);
		addChild(outLight3);

		SanguineStaticRGBLight* outLight4 = new SanguineStaticRGBLight(module, "res/number_4_lit.svg", 117.898, 109.601, true, kSanguineYellowLight);
		addChild(outLight4);

		SanguineBloodLogoLight* bloodLight = new SanguineBloodLogoLight(module, 31.116, 109.911);
		addChild(bloodLight);

		SanguineMonstersLogoLight* monstersLight = new SanguineMonstersLogoLight(module, 44.248, 116.867);
		addChild(monstersLight);
	}
};

Model* modelBrainz = createModel<Brainz, BrainzWidget>("Sanguine-Monsters-Brainz");