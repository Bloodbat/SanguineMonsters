#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "seqcomponents.hpp"

enum ModuleStages {
	MODULE_STAGE_INIT,
	MODULE_STAGE_ROUND_1,
	MODULE_STAGE_ROUND_2,
	MODULE_STAGE_ONE_SHOT_END,
	MODULE_STAGE_DISABLED,
	MODULE_STAGE_STATES_COUNT
};

enum ModuleStates {
	MODULE_STATE_READY,
	MODULE_STATE_START_ROUND_1,
	MODULE_STATE_ROUND_1_TRIGGERS_SENT,
	MODULE_STATE_ROUND_1_TRIGGERS_DONE,
	MODULE_STATE_ROUND_1_STEPS,
	MODULE_STATE_ROUND_1_STEP_A,
	MODULE_STATE_ROUND_1_STEP_B,
	MODULE_STATE_ROUND_1_STEP_C,
	MODULE_STATE_END_ROUND_1,
	MODULE_STATE_START_ROUND_2,
	MODULE_STATE_ROUND_2_STEP_C,
	MODULE_STATE_ROUND_2_STEP_B,
	MODULE_STATE_ROUND_2_STEP_A,
	MODULE_STATE_END_ROUND_2,	
	MODULE_STATE_WAIT_FOR_RESET,
	MODULE_STATE_DISABLED
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
	DIRECTION_DISABLED,
	DIRECTIONS_COUNT
};

struct Brainz : Module {
	enum ParamIds {
		PARAM_MODULE_DIRECTION,
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
		ENUMS(LIGHT_MODULE_DIRECTION, 1 * 3),
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
		ENUMS(LIGHT_MODULE_STAGE, 1 * 3),
		LIGHT_START_TRIGGERS,
		LIGHT_END_TRIGGERS,
		LIGHTS_COUNT
	};

	struct RGBColor {
		float red;
		float green;
		float blue;
	};

	const RGBColor moduleStagesLightColors[MODULE_STAGE_STATES_COUNT]{
		{0.f, 1.f, 0.f},
		{1.f, 1.f, 0.f},
		{0.f, 0.f, 1.f},
		{1.f, 0.0f, 0.f},
		{0.f, 0.f, 0.f},
	};

	const RGBColor stepDirectionsLightColors[DIRECTIONS_COUNT]{
		{1.f, 0.f, 1.f},
		{1.f, 0.f, 0.f},
		{0.f, 0.f, 1.f},
		{0.f, 0.f, 0.f},
	};

	bool bEnteredMetronome = false;
	bool bInMetronome = false;
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

	dsp::SchmittTrigger stReset;
	dsp::SchmittTrigger stResetInput;
	dsp::SchmittTrigger stRun;
	dsp::SchmittTrigger stRunInput;
	dsp::PulseGenerator pgMetronone;
	dsp::PulseGenerator pgTrigger;
	dsp::PulseGenerator pgOutTriggers[4];

	dsp::ClockDivider clockDivider;

	Brainz() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		configSwitch(PARAM_MODULE_DIRECTION, 0.f, 3.f, 0.f, "Module cycle", { "Bidirectional", "Forward", "Backward", "Disabled" });

		configParam(PARAM_A_DELAY, 1.f, 99.f, 1.f, "Delay to trigger A", " seconds");
		paramQuantities[PARAM_A_DELAY]->snapEnabled = true;
		configParam(PARAM_B_DELAY, 1.f, 99.f, 1.f, "Delay to trigger B", " seconds");
		paramQuantities[PARAM_B_DELAY]->snapEnabled = true;
		configParam(PARAM_C_DELAY, 1.f, 99.f, 1.f, "Delay to trigger C", " seconds");
		paramQuantities[PARAM_C_DELAY]->snapEnabled = true;
		configParam(PARAM_METRONOME_SPEED, 15.f, 99.f, 60.f, "BPM", " quarter notes");
		paramQuantities[PARAM_METRONOME_SPEED]->snapEnabled = true;
		configParam(PARAM_METRONOME_STEPS, 5.f, 99.f, 10.f, "Steps", "");
		paramQuantities[PARAM_METRONOME_STEPS]->snapEnabled = true;

		configButton(PARAM_A_ENABLED, "Step A enabled");
		configButton(PARAM_B_ENABLED, "Step B enabled");
		configButton(PARAM_C_ENABLED, "Step C enabled");

		configButton(PARAM_START_TRIGGERS, "Triggers on start");
		params[PARAM_START_TRIGGERS].setValue(1);
		configButton(PARAM_END_TRIGGERS, "Triggers on end");
		params[PARAM_END_TRIGGERS].setValue(1);

		configButton(PARAM_A_DO_TRIGGERS, "Send triggers at the end of coundown A");
		configButton(PARAM_B_DO_TRIGGERS, "Send triggers at the end of coundown B");
		configButton(PARAM_C_DO_TRIGGERS, "Send triggers at the end of coundown C");

		configButton(PARAM_A_IS_METRONOME, "Step A is metronome");
		configButton(PARAM_B_IS_METRONOME, "Step B is metronome");
		configButton(PARAM_C_IS_METRONOME, "Step C is metronome");

		configSwitch(PARAM_A_DIRECTION, 0.f, 2.f, 0.f, "Step A direction", { "Bidirectional", "Forward", "Backward" });
		configSwitch(PARAM_B_DIRECTION, 0.f, 2.f, 0.f, "Step A direction", { "Bidirectional", "Forward", "Backward" });

		configInput(INPUT_TRIGGER, "Start/stop trigger");
		configInput(INPUT_RESET, "Reset trigger");

		configOutput(OUTPUT_METRONOME, "Metronome audio");
		configOutput(OUTPUT_OUT_1, "Trigger 1");
		configOutput(OUTPUT_OUT_2, "Trigger 2");
		configOutput(OUTPUT_OUT_3, "Trigger 3");
		configOutput(OUTPUT_OUT_4, "Trigger 4");

		configOutput(OUTPUT_STAGE_A, "Stage A end of count");
		configOutput(OUTPUT_STAGE_B, "Stage B end of count");
		configOutput(OUTPUT_STAGE_C, "Stage C end of count");

		configButton(PARAM_ONE_SHOT, "One shot");
		configButton(PARAM_PLAY_BUTTON, "Start/stop");
		configButton(PARAM_RESET_BUTTON, "Reset");

		for (int i = 0; i < 4; i++) {
			params[PARAM_A_ENABLED + i].setValue(1);
		}

		clockDivider.setDivision(kClockDivider);
	}

	void process(const ProcessArgs& args) override {
		if (bInMetronome) {
			// Ensure only the correct step is lit.
			lights[LIGHT_STEP_A].setBrightnessSmooth((moduleState == MODULE_STATE_ROUND_1_STEP_A || moduleState == MODULE_STATE_ROUND_2_STEP_A) ? 1.0f : 0.f, args.sampleTime);
			lights[LIGHT_STEP_B].setBrightnessSmooth((moduleState == MODULE_STATE_ROUND_1_STEP_B || moduleState == MODULE_STATE_ROUND_2_STEP_B) ? 1.0f : 0.f, args.sampleTime);
			lights[LIGHT_STEP_C].setBrightnessSmooth((moduleState == MODULE_STATE_ROUND_1_STEP_C || moduleState == MODULE_STATE_ROUND_2_STEP_C) ? 1.0f : 0.f, args.sampleTime);
			doMetronome(args);
		}
		else {
			if (clockDivider.process()) {
				// Updated only every N samples, so make sure setBrightnessSmooth accounts for this.
				const float sampleTime = APP->engine->getSampleTime() * kClockDivider;

				if (stReset.process(params[PARAM_RESET_BUTTON].getValue())|| stResetInput.process(inputs[INPUT_RESET].getVoltage())) {
					handleResetTriggers();
				}

				if ((stRun.process(params[PARAM_PLAY_BUTTON].getValue()) || stRunInput.process(inputs[INPUT_TRIGGER].getVoltage()))
					&& moduleState != MODULE_STATE_DISABLED) {
					handleRunTriggers();
				}

				if (moduleState > MODULE_STATE_READY && moduleState < MODULE_STATE_ROUND_1_TRIGGERS_DONE && params[PARAM_START_TRIGGERS].getValue()) {
					setupGlobalVoltages(MODULE_STATE_READY, MODULE_STATE_ROUND_1_TRIGGERS_SENT, PARAM_START_TRIGGERS, args);

					processGlobalVoltages(MODULE_STATE_ROUND_1_TRIGGERS_SENT, MODULE_STATE_ROUND_1_TRIGGERS_DONE, args);
				}
				else if (moduleState > MODULE_STATE_READY && moduleState < MODULE_STATE_ROUND_1_TRIGGERS_DONE) {
					moduleState = MODULE_STATE_ROUND_1_TRIGGERS_DONE;
				}

				if (moduleState == MODULE_STATE_ROUND_1_TRIGGERS_DONE) {
					moduleState = MODULE_STATE_ROUND_1_STEPS;

					for (int i = 0; i < 3; i++) {
						if (bStepEnabled[i]) {
							moduleState = ModuleStates(MODULE_STATE_ROUND_1_STEP_A + i);
							break;
						}
					}

					if (moduleState > MODULE_STATE_ROUND_1_STEPS) {
						bStepStarted = false;
						bStepTrigger = false;
						bEnteredMetronome = false;
						stepState = STEP_STATE_READY;
					}
					else {
						moduleState = MODULE_STATE_END_ROUND_1;
					}
				}

				if (moduleState == MODULE_STATE_ROUND_1_STEP_A) {
					if (params[PARAM_A_IS_METRONOME].getValue()) {
						if (!bEnteredMetronome) {
							setupMetronome(&currentCounters[0]);
						}
						else {
							if (!bStepStarted) {
								setupAfterMetronomeTriggers();
							}
							else {
								handleAfterMetronomeTriggers(OUTPUT_STAGE_A, args);

								doEndOfStepTriggers(PARAM_A_DO_TRIGGERS, args);
							}
						}
					}
					else {
						if (!bStepStarted) {
							setupStep(maxCounters[0]);
						}
						else {
							doStepTrigger(OUTPUT_STAGE_A, &currentCounters[0], args);
							doEndOfStepTriggers(PARAM_A_DO_TRIGGERS, args);
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
							moduleState = MODULE_STATE_END_ROUND_1;
						}
						bStepStarted = false;
						bStepTrigger = false;
						bEnteredMetronome = false;
						stepState = STEP_STATE_READY;
					}
				}

				if (moduleState == MODULE_STATE_ROUND_1_STEP_B) {
					if (params[PARAM_B_IS_METRONOME].getValue()) {
						if (!bEnteredMetronome) {
							setupMetronome(&currentCounters[1]);
						}
						else {
							if (!bStepStarted) {
								setupAfterMetronomeTriggers();
							}
							else {
								handleAfterMetronomeTriggers(OUTPUT_STAGE_B, args);

								doEndOfStepTriggers(PARAM_B_DO_TRIGGERS, args);
							}
						}
					}
					else {
						if (!bStepStarted) {
							setupStep(maxCounters[1]);
						}
						else {
							doStepTrigger(OUTPUT_STAGE_B, &currentCounters[1], args);
							doEndOfStepTriggers(PARAM_B_DO_TRIGGERS, args);
						}
					}

					if (bTriggersDone[0] && bTriggersDone[1] && bTriggersDone[2] && bTriggersDone[3]) {
						if (bStepEnabled[2] && stepDirections[1] < DIRECTION_BACKWARD) {
							moduleState = MODULE_STATE_ROUND_1_STEP_C;
						}
						else {
							moduleState = MODULE_STATE_END_ROUND_1;
						}
						bStepStarted = false;
						bStepTrigger = false;
						bEnteredMetronome = false;
						stepState = STEP_STATE_READY;
					}
				}

				if (moduleState == MODULE_STATE_ROUND_1_STEP_C) {
					if (params[PARAM_C_IS_METRONOME].getValue()) {
						if (!bEnteredMetronome) {
							setupMetronome(&currentCounters[2]);
						}
						else {
							if (!bStepStarted) {
								setupAfterMetronomeTriggers();
							}
							else {
								handleAfterMetronomeTriggers(OUTPUT_STAGE_C, args);

								doEndOfStepTriggers(PARAM_C_DO_TRIGGERS, args);
							}
						}
					}
					else {
						if (!bStepStarted) {
							setupStep(maxCounters[2]);
						}
						else {
							doStepTrigger(OUTPUT_STAGE_C, &currentCounters[2], args);
							doEndOfStepTriggers(PARAM_C_DO_TRIGGERS, args);
						}

						if (bTriggersDone[0] && bTriggersDone[1] && bTriggersDone[2] && bTriggersDone[3]) {
							if (moduleDirection == DIRECTION_BIDIRECTIONAL) {
								moduleState = MODULE_STATE_END_ROUND_1;
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
							bStepStarted = false;
							bStepTrigger = false;
							bEnteredMetronome = false;
							stepState = STEP_STATE_READY;
						}
					}
				}

				if (moduleState == MODULE_STATE_START_ROUND_2) {
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
						moduleState = MODULE_STATE_END_ROUND_2;
					}
				}

				if (moduleState == MODULE_STATE_ROUND_2_STEP_C) {
					if (params[PARAM_C_IS_METRONOME].getValue()) {
						if (!bEnteredMetronome) {
							setupMetronome(&currentCounters[2]);
						}
						else {
							if (!bStepStarted) {
								setupAfterMetronomeTriggers();
							}
							else {
								handleAfterMetronomeTriggers(OUTPUT_STAGE_C, args);

								doEndOfStepTriggers(PARAM_C_DO_TRIGGERS, args);
							}
						}
					}
					else {
						if (!bStepStarted) {
							setupStep(maxCounters[2]);
						}
						else {
							doStepTrigger(OUTPUT_STAGE_C, &currentCounters[2], args);
							doEndOfStepTriggers(PARAM_C_DO_TRIGGERS, args);
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
							moduleState = MODULE_STATE_END_ROUND_2;
						}
						bStepStarted = false;
						bStepTrigger = false;
						bEnteredMetronome = false;
						stepState = STEP_STATE_READY;
					}
				}

				if (moduleState == MODULE_STATE_ROUND_2_STEP_B) {
					if (params[PARAM_B_IS_METRONOME].getValue()) {
						if (!bEnteredMetronome) {
							setupMetronome(&currentCounters[1]);
						}
						else {
							if (!bStepStarted) {
								setupAfterMetronomeTriggers();
							}
							else {
								handleAfterMetronomeTriggers(OUTPUT_STAGE_B, args);

								doEndOfStepTriggers(PARAM_B_DO_TRIGGERS, args);
							}
						}
					}
					else {
						if (!bStepStarted) {
							setupStep(maxCounters[1]);
						}
						else {
							doStepTrigger(OUTPUT_STAGE_B, &currentCounters[1], args);
							doEndOfStepTriggers(PARAM_B_DO_TRIGGERS, args);
						}
					}

					if (bTriggersDone[0] && bTriggersDone[1] && bTriggersDone[2] && bTriggersDone[3]) {
						if (bStepEnabled[0] && (stepDirections[0] == DIRECTION_BACKWARD || stepDirections[0] == DIRECTION_BIDIRECTIONAL)) {
							moduleState = MODULE_STATE_ROUND_2_STEP_A;
						}
						else {
							moduleState = MODULE_STATE_END_ROUND_2;
						}
						bStepStarted = false;
						bStepTrigger = false;
						bEnteredMetronome = false;
						stepState = STEP_STATE_READY;
					}
				}

				if (moduleState == MODULE_STATE_ROUND_2_STEP_A) {
					if (params[PARAM_A_IS_METRONOME].getValue()) {
						if (!bEnteredMetronome) {
							setupMetronome(&currentCounters[0]);
						}
						else {
							if (!bStepStarted) {
								setupAfterMetronomeTriggers();
							}
							else {
								handleAfterMetronomeTriggers(OUTPUT_STAGE_A, args);

								doEndOfStepTriggers(PARAM_A_DO_TRIGGERS, args);
							}
						}
					}
					else {
						if (!bStepStarted) {
							setupStep(maxCounters[0]);
						}
						else {
							doStepTrigger(OUTPUT_STAGE_A, &currentCounters[0], args);
							doEndOfStepTriggers(PARAM_A_DO_TRIGGERS, args);
						}
					}

					if (bTriggersDone[0] && bTriggersDone[1] && bTriggersDone[2] && bTriggersDone[3]) {
						moduleState = MODULE_STATE_END_ROUND_2;
						bStepStarted = false;
						bStepTrigger = false;
						bEnteredMetronome = false;
						stepState = STEP_STATE_READY;
					}
				}

				if (moduleState == MODULE_STATE_END_ROUND_2) {
					if (params[PARAM_END_TRIGGERS].getValue()) {
						if (!bTriggersSent) {
							memset(bTriggersDone, 0, sizeof(bool) * 4);
							for (int i = 0; i < 4; i++) {
								if (outputs[OUTPUT_OUT_1 + i].isConnected()) {
									pgOutTriggers[i].trigger();
									outputs[OUTPUT_OUT_1 + i].setVoltage(pgOutTriggers[i].process(args.sampleTime) ? 10.f : 0.f);
								}
							}
							bTriggersSent = true;
						}
						else {
							for (int i = 0; i < 4; i++) {
								bTriggersDone[i] = !pgOutTriggers[i].process(args.sampleTime);
								if (outputs[OUTPUT_OUT_1 + i].isConnected()) {
									outputs[OUTPUT_OUT_1 + i].setVoltage(bTriggersDone[i] ? 0.f : 10.f);
								}
							}
						}
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
						lights[currentLight + 0].setBrightnessSmooth(stepDirectionsLightColors[stepDirections[i]].red, sampleTime);
						lights[currentLight + 1].setBrightnessSmooth(stepDirectionsLightColors[stepDirections[i]].green, sampleTime);
						lights[currentLight + 2].setBrightnessSmooth(stepDirectionsLightColors[stepDirections[i]].blue, sampleTime);
					}
					if (!params[PARAM_A_IS_METRONOME + i].getValue()) {
						maxCounters[i] = params[PARAM_A_DELAY + i].getValue();
					}
					else {
						maxCounters[i] = 0;
					}
				}

				lights[LIGHT_MODULE_DIRECTION + 0].setBrightnessSmooth(stepDirectionsLightColors[moduleDirection].red, sampleTime);
				lights[LIGHT_MODULE_DIRECTION + 1].setBrightnessSmooth(stepDirectionsLightColors[moduleDirection].green, sampleTime);
				lights[LIGHT_MODULE_DIRECTION + 2].setBrightnessSmooth(stepDirectionsLightColors[moduleDirection].blue, sampleTime);

				lights[LIGHT_MODULE_STAGE + 0].setBrightnessSmooth(moduleStagesLightColors[moduleStage].red, sampleTime);
				lights[LIGHT_MODULE_STAGE + 1].setBrightnessSmooth(moduleStagesLightColors[moduleStage].green, sampleTime);
				lights[LIGHT_MODULE_STAGE + 2].setBrightnessSmooth(moduleStagesLightColors[moduleStage].blue, sampleTime);

				lights[LIGHT_START_TRIGGERS].setBrightnessSmooth(params[PARAM_START_TRIGGERS].getValue(), sampleTime);
				lights[LIGHT_END_TRIGGERS].setBrightnessSmooth(params[PARAM_END_TRIGGERS].getValue(), sampleTime);

				lights[LIGHT_STEP_A].setBrightnessSmooth((moduleState == MODULE_STATE_ROUND_1_STEP_A || moduleState == MODULE_STATE_ROUND_2_STEP_A) ? 1.0f : 0.f, sampleTime);
				lights[LIGHT_STEP_B].setBrightnessSmooth((moduleState == MODULE_STATE_ROUND_1_STEP_B || moduleState == MODULE_STATE_ROUND_2_STEP_B) ? 1.0f : 0.f, sampleTime);
				lights[LIGHT_STEP_C].setBrightnessSmooth((moduleState == MODULE_STATE_ROUND_1_STEP_C || moduleState == MODULE_STATE_ROUND_2_STEP_C) ? 1.0f : 0.f, sampleTime);
				lights[LIGHT_METRONOME].setBrightnessSmooth(bInMetronome ? 1.0f : 0.f, sampleTime);

				// TODO!!! Call reset when this is done?
				if (moduleState == MODULE_STATE_READY || moduleState == MODULE_STATE_DISABLED) {
					moduleDirection = StepDirections(params[PARAM_MODULE_DIRECTION].getValue());

					if (moduleDirection < DIRECTION_DISABLED) {
						if (moduleState < MODULE_STATE_WAIT_FOR_RESET) {
							moduleStage = MODULE_STAGE_INIT;
						}
						else {
							moduleStage = MODULE_STAGE_ONE_SHOT_END;
						}
						moduleState = lastModuleState;
					}
					else {
						moduleState = MODULE_STATE_DISABLED;
						moduleStage = MODULE_STAGE_DISABLED;
					}
				}
			} // Clock divider
		}
	}

	inline void doMetronome(const ProcessArgs& args) {
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

	inline void setupMetronome(int* counterNumber) {
		memset(bTriggersDone, 0, sizeof(bool) * 4);
		metronomeCounter = 0.f;
		metronomePeriod = 0.f;
		metronomeStepsDone = 0;
		metronomeCounterPtr = counterNumber;
		bInMetronome = true;
	}

	inline void setupAfterMetronomeTriggers() {
		bStepStarted = true;
		bTriggersSent = false;
	}

	inline void handleAfterMetronomeTriggers(OutputIds output, const ProcessArgs& args) {
		if (stepState < STEP_STATE_TRIGGER_SENT) {
			if (outputs[output].isConnected()) {
				pgTrigger.trigger();
				outputs[output].setVoltage(pgTrigger.process(args.sampleTime) ? 10.f : 0.f);
				stepState = STEP_STATE_TRIGGER_SENT;
			}
		}
		else {
			bStepTrigger = pgTrigger.process(args.sampleTime);
			outputs[output].setVoltage(bStepTrigger ? 10.f : 0.f);
			if (!bStepTrigger) {
				stepState = STEP_STATE_TRIGGER_DONE;
			}
		}
	}

	inline void handleRunTriggers() {
		// TODO Kill metronome also?
		switch (moduleState)
		{
		case MODULE_STATE_READY: {
			if (moduleDirection < DIRECTION_BACKWARD) {
				moduleStage = MODULE_STAGE_ROUND_1;
				moduleState = MODULE_STATE_START_ROUND_1;
			}
			else if (moduleDirection == DIRECTION_BACKWARD) {
				moduleState = MODULE_STATE_START_ROUND_2;
				moduleStage = MODULE_STAGE_ROUND_2;
			}
			break;
		}
		case MODULE_STATE_START_ROUND_1:
		case MODULE_STATE_ROUND_1_TRIGGERS_SENT:
		case MODULE_STATE_ROUND_1_TRIGGERS_DONE:
		case MODULE_STATE_ROUND_1_STEPS:
		case MODULE_STATE_ROUND_1_STEP_A:
		case MODULE_STATE_ROUND_1_STEP_B:
		case MODULE_STATE_ROUND_1_STEP_C: {
			killVoltages();
			moduleState = MODULE_STATE_READY;
			moduleStage = MODULE_STAGE_INIT;
			break;
		}
		case MODULE_STATE_END_ROUND_1: {
			if (moduleDirection == DIRECTION_BACKWARD || moduleDirection == DIRECTION_BIDIRECTIONAL) {
				memset(currentCounters, 0, sizeof(int) * 3);
				moduleStage = MODULE_STAGE_ROUND_2;
				moduleState = MODULE_STATE_START_ROUND_2;
			}
			break;
		}
		case MODULE_STATE_START_ROUND_2:		
		case MODULE_STATE_ROUND_2_STEP_A:
		case MODULE_STATE_ROUND_2_STEP_B:
		case MODULE_STATE_ROUND_2_STEP_C:
		case MODULE_STATE_END_ROUND_2: {
			killVoltages();
			moduleState = MODULE_STATE_READY;
			moduleStage = MODULE_STAGE_INIT;
			break;
		}
		case MODULE_STATE_WAIT_FOR_RESET: {
			break;
		}
		case MODULE_STATE_DISABLED: {
			break;
		}
		}
	}

	inline void handleResetTriggers() {
		killVoltages();
		memset(currentCounters, 0, sizeof(int) * 3);
		metronomeStepsDone = 0;
		if (moduleState != MODULE_STATE_DISABLED) {
			moduleState = MODULE_STATE_READY;
			moduleStage = MODULE_STAGE_INIT;
		}
	}

	inline void killVoltages() {
		for (int i = 0; i < INPUTS_COUNT; i++) {
			outputs[OUTPUT_METRONOME + i].setVoltage(0);
		}
	}

	inline void setupGlobalVoltages(ModuleStates initialState, ModuleStates finalState, ParamIds checkParam, const ProcessArgs& args) {
		if (params[checkParam].getValue() == 1 && moduleState > initialState && moduleState < finalState) {
			for (int i = 0; i < 4; i++) {
				if (outputs[OUTPUT_OUT_1 + i].isConnected()) {
					pgOutTriggers[i].trigger();
					outputs[OUTPUT_OUT_1 + i].setVoltage(pgOutTriggers[i].process(args.sampleTime) ? 10.f : 0.f);
				}
			}
			memset(bTriggersDone, 0, sizeof(bool) * 4);
			moduleState = finalState;
		}
	}

	inline void processGlobalVoltages(ModuleStates initialState, ModuleStates finalState, const ProcessArgs& args) {
		if (moduleState == initialState) {
			for (int i = 0; i < 4; i++) {
				bTriggersDone[i] = !pgOutTriggers[i].process(args.sampleTime);
				if (outputs[OUTPUT_OUT_1 + i].isConnected()) {
					outputs[OUTPUT_OUT_1 + i].setVoltage(bTriggersDone[i] ? 0.f : 10.f);
				}
				if (bTriggersDone[0] && bTriggersDone[1] && bTriggersDone[2] && bTriggersDone[3]) {
					moduleState = finalState;
				}
			}
		}
	}

	inline void setupStep(int delayTime) {
		startTime = std::chrono::steady_clock::now();
		bStepStarted = true;
		memset(bTriggersDone, 0, sizeof(bool) * 4);
		bTriggersSent = false;
		currentDelayTime = delayTime * 1000;
	}

	inline void doStepTrigger(OutputIds output, int* counter, const ProcessArgs& args) {
		if (stepState < STEP_STATE_TRIGGER_SENT) {
			std::chrono::time_point<std::chrono::steady_clock> currentTime = std::chrono::steady_clock::now();
			int elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();
			*counter = elapsedTime / 1000;
			if (elapsedTime >= currentDelayTime) {
				if (outputs[output].isConnected()) {
					pgTrigger.trigger();
					outputs[output].setVoltage(pgTrigger.process(args.sampleTime) ? 10.f : 0.f);
					stepState = STEP_STATE_TRIGGER_SENT;
				}
			}
		}
		else {
			bStepTrigger = pgTrigger.process(args.sampleTime);
			outputs[output].setVoltage(bStepTrigger ? 10.f : 0.f);
			if (!bStepTrigger) {
				stepState = STEP_STATE_TRIGGER_DONE;
			}
		}
	}

	inline void doEndOfStepTriggers(ParamIds checkParam, const ProcessArgs& args) {
		if (bStepStarted && stepState == STEP_STATE_TRIGGER_DONE) {
			if (!params[checkParam].getValue()) {
				for (int i = 0; i < 4; i++) {
					bTriggersDone[i] = true;
				}
			}
			else {
				if (!bTriggersSent) {
					for (int i = 0; i < 4; i++) {
						if (outputs[OUTPUT_OUT_1 + i].isConnected()) {
							pgOutTriggers[i].trigger();
							outputs[OUTPUT_OUT_1 + i].setVoltage(pgOutTriggers[i].process(args.sampleTime) ? 10.f : 0.f);
						}
					}
					bTriggersSent = true;
				}
				else {
					for (int i = 0; i < 4; i++) {
						bTriggersDone[i] = !pgOutTriggers[i].process(args.sampleTime);
						if (outputs[OUTPUT_OUT_1 + i].isConnected()) {
							outputs[OUTPUT_OUT_1 + i].setVoltage(bTriggersDone[i] ? 0.f : 10.f);
						}
					}
				}
			}
		}
	}
};

struct YellowGateLight : SanguineShapedLight {
	YellowGateLight(Module* newModule) {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/gate_lit_mono.svg")));
		module = newModule;
		wrap();
	}
};

struct BluePowerLight : SanguineShapedLight {
	BluePowerLight(Module* newModule) {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/power_blue_lit.svg")));
		module = newModule;
		wrap();
	}
};

struct BlueAdvancedClockLight : SanguineShapedLight {
	BlueAdvancedClockLight(Module* newModule) {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/clock_move_blue_lit.svg")));
		module = newModule;
		wrap();
	}
};

struct BlueInitialClockLight : SanguineShapedLight {
	BlueInitialClockLight(Module* newModule) {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/clock_still_blue_lit.svg")));
		module = newModule;
		wrap();
	}
};

struct BlueRightArrowLight : SanguineShapedLight {
	BlueRightArrowLight(Module* newModule) {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/arrow_right_blue_lit.svg")));
		module = newModule;
		wrap();
	}
};

struct BlueQuarterNoteLight : SanguineShapedLight {
	BlueQuarterNoteLight(Module* newModule) {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/quarter_note_blue_lit.svg")));
		module = newModule;
		wrap();
	}
};

struct BrainzWidget : ModuleWidget {
	BrainzWidget(Brainz* module) {
		setModule(module);
		setPanel(Svg::load(asset::plugin(pluginInstance, "res/brainz.svg")));

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		SeqControlSwitch* btnPlay = createParamCentered<SeqControlSwitch>(mm2px(Vec(69.954, 7.964)),
			module, Brainz::PARAM_PLAY_BUTTON);
		btnPlay->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/play_off.svg")));
		btnPlay->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/play_on.svg")));
		addParam(btnPlay);

		SeqControlSwitch* btnReset = createParamCentered<SeqControlSwitch>(mm2px(Vec(82.121, 7.964)),
			module, Brainz::PARAM_RESET_BUTTON);
		btnReset->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/reset_off.svg")));
		btnReset->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/reset_on.svg")));
		addParam(btnReset);

		SanguineBezel8mm* btnModuleDirection = createLightParamCentered<LightButton<SanguineBezel8mm, SanguineBezelLight8mm<RedGreenBlueLight>>>(mm2px(Vec(97.453, 11.87)),
			module, Brainz::PARAM_MODULE_DIRECTION, Brainz::LIGHT_MODULE_DIRECTION);
		btnModuleDirection->momentary = false;
		btnModuleDirection->latch = true;
		addParam(btnModuleDirection);

		addChild(createLightCentered<LargeLight<RedGreenBlueLight>>(mm2px(Vec(106.469, 11.906)), module, Brainz::LIGHT_MODULE_STAGE));

		addParam(createParamCentered<BefacoTinyKnobRed>(mm2px(Vec(24.498, 25.114)), module, Brainz::PARAM_A_DELAY));
		addParam(createParamCentered<BefacoTinyKnobRed>(mm2px(Vec(24.498, 76.144)), module, Brainz::PARAM_B_DELAY));
		addParam(createParamCentered<BefacoTinyKnobRed>(mm2px(Vec(87.262, 76.144)), module, Brainz::PARAM_C_DELAY));
		addParam(createParamCentered<BefacoTinyKnobRed>(mm2px(Vec(75.493, 33.208)), module, Brainz::PARAM_METRONOME_SPEED));
		addParam(createParamCentered<BefacoTinyKnobBlack>(mm2px(Vec(87.207, 43.414)), module, Brainz::PARAM_METRONOME_STEPS));

		addParam(createLightParamCentered<VCVLightBezelLatch<OrangeLight>>(mm2px(Vec(40.815, 25.114)), module, Brainz::PARAM_A_ENABLED, Brainz::LIGHT_STEP_A_ENABLED));
		addParam(createLightParamCentered<VCVLightBezelLatch<OrangeLight>>(mm2px(Vec(40.815, 76.144)), module, Brainz::PARAM_B_ENABLED, Brainz::LIGHT_STEP_B_ENABLED));
		addParam(createLightParamCentered<VCVLightBezelLatch<OrangeLight>>(mm2px(Vec(103.578, 76.144)), module, Brainz::PARAM_C_ENABLED, Brainz::LIGHT_STEP_C_ENABLED));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<BlueLight>>>(mm2px(Vec(7.781, 43.647)),
			module, Brainz::PARAM_A_DO_TRIGGERS, Brainz::LIGHT_STEP_A_TRIGGERS));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(mm2px(Vec(20.996, 43.647)),
			module, Brainz::PARAM_A_IS_METRONOME, Brainz::LIGHT_STEP_A_METRONOME));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<BlueLight>>>(mm2px(Vec(7.781, 94.677)),
			module, Brainz::PARAM_B_DO_TRIGGERS, Brainz::LIGHT_STEP_B_TRIGGERS));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(mm2px(Vec(20.996, 94.677)),
			module, Brainz::PARAM_B_IS_METRONOME, Brainz::LIGHT_STEP_B_METRONOME));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<BlueLight>>>(mm2px(Vec(70.544, 94.677)),
			module, Brainz::PARAM_C_DO_TRIGGERS, Brainz::LIGHT_STEP_C_TRIGGERS));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(mm2px(Vec(83.775, 94.677)),
			module, Brainz::PARAM_C_IS_METRONOME, Brainz::LIGHT_STEP_C_METRONOME));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedLight>>>(mm2px(Vec(49.85, 26.969)),
			module, Brainz::PARAM_START_TRIGGERS, Brainz::LIGHT_START_TRIGGERS));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedLight>>>(mm2px(Vec(61.938, 26.969)),
			module, Brainz::PARAM_END_TRIGGERS, Brainz::LIGHT_END_TRIGGERS));

		addOutput(createOutputCentered<BananutBlack>(mm2px(Vec(40.315, 43.647)), module, Brainz::OUTPUT_STAGE_A));
		addOutput(createOutputCentered<BananutBlack>(mm2px(Vec(40.315, 94.677)), module, Brainz::OUTPUT_STAGE_B));
		addOutput(createOutputCentered<BananutBlack>(mm2px(Vec(103.078, 94.677)), module, Brainz::OUTPUT_STAGE_C));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedGreenBlueLight>>>(mm2px(Vec(24.498, 59.903)),
			module, Brainz::PARAM_A_DIRECTION, Brainz::LIGHT_STEP_A_DIRECTION + 0 * 3));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedGreenBlueLight>>>(mm2px(Vec(55.905, 85.403)),
			module, Brainz::PARAM_B_DIRECTION, Brainz::LIGHT_STEP_A_DIRECTION + 1 * 3));

		addInput(createInputCentered<BananutGreen>(mm2px(Vec(7.882, 116.807)), module, Brainz::INPUT_TRIGGER));
		addInput(createInputCentered<BananutGreen>(mm2px(Vec(20.847, 116.807)), module, Brainz::INPUT_RESET));

		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(60.091, 116.807)), module, Brainz::OUTPUT_METRONOME));
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(71.042, 116.807)), module, Brainz::OUTPUT_OUT_1));
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(81.993, 116.807)), module, Brainz::OUTPUT_OUT_2));
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(92.943, 116.807)), module, Brainz::OUTPUT_OUT_3));
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(103.894, 116.807)), module, Brainz::OUTPUT_OUT_4));

		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(14.108, 25.114)), module, Brainz::LIGHT_STEP_A));
		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(14.108, 76.144)), module, Brainz::LIGHT_STEP_B));
		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(76.871, 76.144)), module, Brainz::LIGHT_STEP_C));
		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(76.871, 25.114)), module, Brainz::LIGHT_METRONOME));

		SanguineLightUpSwitch* switchOneShot = createParam<SanguineLightUpSwitch>(mm2px(Vec(53.88, 32.373)),
			module, Brainz::PARAM_ONE_SHOT);
		switchOneShot->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/one_shot_off.svg")));
		switchOneShot->addFrame(Svg::load(asset::plugin(pluginInstance, "res/seqs/one_shot_on.svg")));
		switchOneShot->haloColorOff = nvgRGB(0, 0, 0);
		switchOneShot->haloColorOn = nvgRGB(255, 11, 11);
		switchOneShot->momentary = false;
		addParam(switchOneShot);

		FramebufferWidget* brainzFrameBuffer = new FramebufferWidget();
		addChild(brainzFrameBuffer);

		SanguineTinyNumericDisplay* displayStepsACurrent = new SanguineTinyNumericDisplay(2);
		displayStepsACurrent->box.pos = mm2px(Vec(10.088, 30.908));
		displayStepsACurrent->module = module;
		brainzFrameBuffer->addChild(displayStepsACurrent);

		if (module)
			displayStepsACurrent->values.numberValue = &module->currentCounters[0];

		SanguineTinyNumericDisplay* displayStepsATotal = new SanguineTinyNumericDisplay(2);
		displayStepsATotal->box.pos = mm2px(Vec(25.901, 30.908));
		displayStepsATotal->module = module;
		brainzFrameBuffer->addChild(displayStepsATotal);

		if (module)
			displayStepsATotal->values.numberValue = &module->maxCounters[0];

		SanguineTinyNumericDisplay* displayStepsBCurrent = new SanguineTinyNumericDisplay(2);
		displayStepsBCurrent->box.pos = mm2px(Vec(10.088, 81.938));
		displayStepsBCurrent->module = module;
		brainzFrameBuffer->addChild(displayStepsBCurrent);

		if (module)
			displayStepsBCurrent->values.numberValue = &module->currentCounters[1];

		SanguineTinyNumericDisplay* displayStepsBTotal = new SanguineTinyNumericDisplay(2);
		displayStepsBTotal->box.pos = mm2px(Vec(25.901, 81.938));
		displayStepsBTotal->module = module;
		brainzFrameBuffer->addChild(displayStepsBTotal);

		if (module)
			displayStepsBTotal->values.numberValue = &module->maxCounters[1];

		SanguineTinyNumericDisplay* displayStepsCCurrent = new SanguineTinyNumericDisplay(2);
		displayStepsCCurrent->box.pos = mm2px(Vec(72.851, 81.938));
		displayStepsCCurrent->module = module;
		brainzFrameBuffer->addChild(displayStepsCCurrent);

		if (module)
			displayStepsCCurrent->values.numberValue = &module->currentCounters[2];

		SanguineTinyNumericDisplay* displayStepsCTotal = new SanguineTinyNumericDisplay(2);
		displayStepsCTotal->box.pos = mm2px(Vec(88.664, 81.938));
		displayStepsCTotal->module = module;
		brainzFrameBuffer->addChild(displayStepsCTotal);

		if (module)
			displayStepsCTotal->values.numberValue = &module->maxCounters[2];

		SanguineTinyNumericDisplay* displayMetronomeSpeed = new SanguineTinyNumericDisplay(2);
		displayMetronomeSpeed->box.pos = mm2px(Vec(88.664, 29.208));
		displayMetronomeSpeed->module = module;
		brainzFrameBuffer->addChild(displayMetronomeSpeed);

		if (module)
			displayMetronomeSpeed->values.numberValue = &module->metronomeSpeed;

		SanguineTinyNumericDisplay* displayMetronomeSteps = new SanguineTinyNumericDisplay(2);
		displayMetronomeSteps->box.pos = mm2px(Vec(88.664, 49.456));
		displayMetronomeSteps->module = module;
		brainzFrameBuffer->addChild(displayMetronomeSteps);

		if (module)
			displayMetronomeSteps->values.numberValue = &module->metronomeSteps;

		SanguineTinyNumericDisplay* displayMetronomeCurrentStep = new SanguineTinyNumericDisplay(2);
		displayMetronomeCurrentStep->box.pos = mm2px(Vec(72.851, 49.456));
		displayMetronomeCurrentStep->module = module;
		brainzFrameBuffer->addChild(displayMetronomeCurrentStep);

		if (module)
			displayMetronomeCurrentStep->values.numberValue = &module->metronomeStepsDone;

		SanguineShapedLight* inPlayLight = new SanguineShapedLight();
		inPlayLight->box.pos = mm2px(Vec(6.932, 107.701));
		inPlayLight->wrap();
		inPlayLight->module = module;
		inPlayLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/yellow_play_lit.svg")));
		brainzFrameBuffer->addChild(inPlayLight);

		SanguineShapedLight* resetLight = new SanguineShapedLight();
		resetLight->box.pos = mm2px(Vec(18.947, 108.157));
		resetLight->wrap();
		resetLight->module = module;
		resetLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/reset_buttonless_lit_mono.svg")));
		brainzFrameBuffer->addChild(resetLight);

		SanguineShapedLight* clockLight = new SanguineShapedLight();
		clockLight->box.pos = mm2px(Vec(58.407, 107.701));
		clockLight->wrap();
		clockLight->module = module;
		clockLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/clock_lit_mono.svg")));
		brainzFrameBuffer->addChild(clockLight);

		SanguineShapedLight* metronomeStepsLight = new SanguineShapedLight();
		metronomeStepsLight->box.pos = mm2px(Vec(104.262, 52.306));
		metronomeStepsLight->wrap();
		metronomeStepsLight->module = module;
		metronomeStepsLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/numpad_lit_blue.svg")));
		brainzFrameBuffer->addChild(metronomeStepsLight);

		YellowGateLight* gateLightA = new YellowGateLight(module);
		gateLightA->box.pos = mm2px(Vec(31.034, 41.747));
		brainzFrameBuffer->addChild(gateLightA);

		YellowGateLight* gateLightB = new YellowGateLight(module);
		gateLightB->box.pos = mm2px(Vec(31.034, 92.777));
		brainzFrameBuffer->addChild(gateLightB);

		YellowGateLight* gateLightC = new YellowGateLight(module);
		gateLightC->box.pos = mm2px(Vec(93.798, 92.777));
		brainzFrameBuffer->addChild(gateLightC);

		BluePowerLight* powerLightA = new BluePowerLight(module);
		powerLightA->box.pos = mm2px(Vec(32.145, 23.214));
		brainzFrameBuffer->addChild(powerLightA);

		BluePowerLight* powerLightB = new BluePowerLight(module);
		powerLightB->box.pos = mm2px(Vec(32.145, 74.244));
		brainzFrameBuffer->addChild(powerLightB);

		BluePowerLight* powerLightC = new BluePowerLight(module);
		powerLightC->box.pos = mm2px(Vec(94.908, 74.244));
		brainzFrameBuffer->addChild(powerLightC);

		BlueAdvancedClockLight* advancedClockLightA = new BlueAdvancedClockLight(module);
		advancedClockLightA->box.pos = mm2px(Vec(4.781, 32.845));
		brainzFrameBuffer->addChild(advancedClockLightA);

		BlueAdvancedClockLight* advancedClockLightB = new BlueAdvancedClockLight(module);
		advancedClockLightB->box.pos = mm2px(Vec(4.781, 83.875));
		brainzFrameBuffer->addChild(advancedClockLightB);

		BlueAdvancedClockLight* advancedClockLightC = new BlueAdvancedClockLight(module);
		advancedClockLightC->box.pos = mm2px(Vec(67.544, 83.875));
		brainzFrameBuffer->addChild(advancedClockLightC);

		BlueInitialClockLight* initialClockLightA = new BlueInitialClockLight(module);
		initialClockLightA->box.pos = mm2px(Vec(40.515, 33.01));
		brainzFrameBuffer->addChild(initialClockLightA);

		BlueInitialClockLight* initialClockLightB = new BlueInitialClockLight(module);
		initialClockLightB->box.pos = mm2px(Vec(40.515, 84.04));
		brainzFrameBuffer->addChild(initialClockLightB);

		BlueInitialClockLight* initialClockLightC = new BlueInitialClockLight(module);
		initialClockLightC->box.pos = mm2px(Vec(103.278, 84.04));
		brainzFrameBuffer->addChild(initialClockLightC);

		BlueRightArrowLight* arrowLightA = new BlueRightArrowLight(module);
		arrowLightA->box.pos = mm2px(Vec(12.051, 42.643));
		brainzFrameBuffer->addChild(arrowLightA);

		BlueRightArrowLight* arrowLightB = new BlueRightArrowLight(module);
		arrowLightB->box.pos = mm2px(Vec(12.051, 93.673));
		brainzFrameBuffer->addChild(arrowLightB);

		BlueRightArrowLight* arrowLightC = new BlueRightArrowLight(module);
		arrowLightC->box.pos = mm2px(Vec(74.783, 93.673));
		brainzFrameBuffer->addChild(arrowLightC);

		BlueQuarterNoteLight* quarterNoteLightA = new BlueQuarterNoteLight(module);
		quarterNoteLightA->box.pos = mm2px(Vec(25.405, 41.747));
		brainzFrameBuffer->addChild(quarterNoteLightA);

		BlueQuarterNoteLight* quarterNoteLightB = new BlueQuarterNoteLight(module);
		quarterNoteLightB->box.pos = mm2px(Vec(25.405, 92.777));
		brainzFrameBuffer->addChild(quarterNoteLightB);

		BlueQuarterNoteLight* quarterNoteLightC = new BlueQuarterNoteLight(module);
		quarterNoteLightC->box.pos = mm2px(Vec(88.183, 92.777));
		brainzFrameBuffer->addChild(quarterNoteLightC);

		BlueQuarterNoteLight* quarterNoteLightMetronome = new BlueQuarterNoteLight(module);
		quarterNoteLightMetronome->box.pos = mm2px(Vec(104.597, 31.308));
		brainzFrameBuffer->addChild(quarterNoteLightMetronome);

		SanguineShapedLight* outLight1 = new SanguineShapedLight();
		outLight1->box.pos = mm2px(Vec(70.449, 108.589));
		outLight1->wrap();
		outLight1->module = module;
		outLight1->setSvg(Svg::load(asset::plugin(pluginInstance, "res/number_1_yellow_lit.svg")));
		brainzFrameBuffer->addChild(outLight1);

		SanguineShapedLight* outLight2 = new SanguineShapedLight();
		outLight2->box.pos = mm2px(Vec(81.175, 108.567));
		outLight2->wrap();
		outLight2->module = module;
		outLight2->setSvg(Svg::load(asset::plugin(pluginInstance, "res/number_2_yellow_lit.svg")));
		brainzFrameBuffer->addChild(outLight2);

		SanguineShapedLight* outLight3 = new SanguineShapedLight();
		outLight3->box.pos = mm2px(Vec(92.126, 108.567));
		outLight3->wrap();
		outLight3->module = module;
		outLight3->setSvg(Svg::load(asset::plugin(pluginInstance, "res/number_3_yellow_lit.svg")));
		brainzFrameBuffer->addChild(outLight3);

		SanguineShapedLight* outLight4 = new SanguineShapedLight();
		outLight4->box.pos = mm2px(Vec(103.012, 108.589));
		outLight4->wrap();
		outLight4->module = module;
		outLight4->setSvg(Svg::load(asset::plugin(pluginInstance, "res/number_4_yellow_lit.svg")));
		brainzFrameBuffer->addChild(outLight4);

		SanguineShapedLight* bloodLight = new SanguineShapedLight();
		bloodLight->box.pos = mm2px(Vec(28.799, 105.975));
		bloodLight->wrap();
		bloodLight->module = module;
		bloodLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/blood_glowy.svg")));
		brainzFrameBuffer->addChild(bloodLight);

		SanguineShapedLight* monstersLight = new SanguineShapedLight();
		monstersLight->box.pos = mm2px(Vec(36.121, 113.941));
		monstersLight->wrap();
		monstersLight->module = module;
		monstersLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/monsters_lit.svg")));
		brainzFrameBuffer->addChild(monstersLight);
	}
};

Model* modelBrainz = createModel<Brainz, BrainzWidget>("Sanguine-Monsters-Brainz");