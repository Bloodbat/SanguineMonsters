#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "bjorklund.hpp"
#include <array>
#include "sanguinehelpers.hpp"
#include "pcg_variants.h"

#define MAXLEN 32

enum PatternStyle {
	EUCLIDEAN_PATTERN,
	RANDOM_PATTERN,
	FIBONACCI_PATTERN,
	LINEAR_PATTERN
};

struct Sphinx : SanguineModule {
	enum ParamIds {
		PARAM_LENGTH,
		PARAM_STEPS,
		PARAM_ROTATION,
		PARAM_SHIFT,
		PARAM_PADDING,
		PARAM_ACCENT,
		PARAM_PATTERN_STYLE,
		PARAM_GATE_MODE,
		PARAM_REVERSE,
		PARAMS_COUNT
	};
	enum InputIds {
		INPUT_STEPS,
		INPUT_LENGTH,
		INPUT_ROTATION,
		INPUT_SHIFT,
		INPUT_ACCENT,
		INPUT_PADDING,
		INPUT_CLOCK,
		INPUT_RESET,
		INPUTS_COUNT
	};
	enum OutputIds {
		OUTPUT_GATE,
		OUTPUT_ACCENT,
		OUTPUT_EOC,
		OUTPUTS_COUNT
	};

	enum LightIds {
		ENUMS(LIGHT_GATE_MODE, 1 * 3),
		ENUMS(LIGHT_PATTERN_STYLE, 1 * 3),
		LIGHT_EOC,
		ENUMS(LIGHT_OUTPUT, 1 * 3),
		LIGHT_REVERSE,
		LIGHTS_COUNT
	};

	struct RGBBits {
		bool red;
		bool green;
		bool blue;
	};

	RGBBits patternLightColorTable[4];
	RGBBits gateModeLightColorTable[3];

	Bjorklund euclid;
	Bjorklund euclid2;

	// Calculated sequence/accents
	std::vector<bool> calculatedSequence;
	std::vector<bool> calculatedAccents;

	// Padded + rotated + distributed
	std::array<bool, MAXLEN * 2> finalSequence;
	std::array<bool, MAXLEN * 2> finalAccents;

	bool bAccentOn = false;
	bool bCalculate;
	bool bGateOn = false;
	bool bCycleReset = true;

	int patternFill = 4;
	int patternLength = 16;
	int patternRotation = 0;
	int patternPadding = 0;
	int patternAccentRotation = 0;
	int patternAccents = 0;

	int patternChecksum;

	int lastPatternFill;
	int lastPatternLength;
	int lastPatternAccents;

	int currentStep = 0;
	int turing = 0;

	PatternStyle lastPatternStyle = EUCLIDEAN_PATTERN;
	PatternStyle patternStyle = EUCLIDEAN_PATTERN;

	static const int kClockDivider = 16;

	dsp::SchmittTrigger stClockInput;
	dsp::SchmittTrigger stResetInput;

	dsp::PulseGenerator pgGate;
	dsp::PulseGenerator pgAccent;
	dsp::PulseGenerator pgEoc;

	dsp::ClockDivider clockDivider;

	pcg32_random_t pcgRng;

	enum GateMode {
		GM_TRIGGER,
		GM_GATE,
		GM_TURING
	} gateMode = GM_TRIGGER;

	Sphinx() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		configParam(PARAM_LENGTH, 1.f, 32.f, 16.f, "Length", "", 0.f, 1.f);
		configParam(PARAM_STEPS, 0.f, 1.f, 0.25f, "Steps", "%", 0.f, 100.f);
		paramQuantities[PARAM_LENGTH]->snapEnabled = true;
		configParam(PARAM_ROTATION, 0.f, 1.f, 0.f, "Rotation", "%", 0.f, 100.f);
		configParam(PARAM_PADDING, 0.f, 1.f, 0.f, "Padding", "%", 0.f, 100.f);
		configParam(PARAM_ACCENT, 0.f, 1.f, 0.f, "Accents", "%", 0.f, 100.f);
		configParam(PARAM_SHIFT, 0.f, 1.f, 0.f, "Accents rotation", "%", 0.f, 100.f);

		configSwitch(PARAM_PATTERN_STYLE, 0.f, 3.f, 0.f, "Pattern style", { "Euclidean", "Random", "Fibonacci", "Linear" });

		configSwitch(PARAM_GATE_MODE, 0.f, 2.f, 0.f, "Gate mode", { "Trigger", "Gate", "Turing" });

		configInput(INPUT_ACCENT, "Accents CV");
		configInput(INPUT_CLOCK, "Clock");
		configInput(INPUT_STEPS, "Steps CV");
		configInput(INPUT_LENGTH, "Length CV");
		configInput(INPUT_PADDING, "Padding CV");
		configInput(INPUT_RESET, "Reset");
		configInput(INPUT_ROTATION, "Rotation CV");
		configInput(INPUT_SHIFT, "Accents rotation");

		configOutput(OUTPUT_GATE, "Gate");
		configOutput(OUTPUT_ACCENT, "Accent");
		configOutput(OUTPUT_EOC, "End of cycle");

		patternLightColorTable[0].red = true;
		patternLightColorTable[0].green = false;
		patternLightColorTable[0].blue = false;
		patternLightColorTable[1].red = true;
		patternLightColorTable[1].green = false;
		patternLightColorTable[1].blue = true;
		patternLightColorTable[2].red = false;
		patternLightColorTable[2].green = true;
		patternLightColorTable[2].blue = false;
		patternLightColorTable[3].red = false;
		patternLightColorTable[3].green = false;
		patternLightColorTable[3].blue = true;

		gateModeLightColorTable[0].red = false;
		gateModeLightColorTable[0].green = false;
		gateModeLightColorTable[0].blue = true;
		gateModeLightColorTable[1].red = false;
		gateModeLightColorTable[1].green = true;
		gateModeLightColorTable[1].blue = false;
		gateModeLightColorTable[2].red = true;
		gateModeLightColorTable[2].green = false;
		gateModeLightColorTable[2].blue = false;

		finalSequence.fill(0);
		finalAccents.fill(0);
		onReset();

		clockDivider.setDivision(kClockDivider);

		pcg32_srandom_r(&pcgRng, std::round(system::getUnixTime()), (intptr_t)&pcgRng);
	}

	int getFibonacci(int n) {
		return (n < 2) ? n : getFibonacci(n - 1) + getFibonacci(n - 2);
	}

	void onReset() override {
		int patternSize = patternLength + patternPadding;

		if (lastPatternLength != patternLength || lastPatternFill != patternFill || lastPatternStyle != patternStyle || lastPatternAccents != patternAccents) {
			if (lastPatternLength != patternLength || lastPatternStyle != patternStyle) {
				calculatedSequence.resize(patternSize);
				std::fill(calculatedSequence.begin(), calculatedSequence.end(), 0);
			}
			if (lastPatternFill != patternFill || lastPatternStyle != patternStyle) {
				calculatedAccents.resize(patternFill);
				std::fill(calculatedAccents.begin(), calculatedAccents.end(), 0);
			}

			switch (patternStyle) {
			case EUCLIDEAN_PATTERN: {
				euclid.reset();
				euclid.init(patternLength, patternFill);
				euclid.iter();

				euclid2.reset();
				if (patternAccents > 0) {
					euclid2.init(patternFill, patternAccents);
					euclid2.iter();
				}
				calculatedSequence = euclid.sequence;
				calculatedAccents = euclid2.sequence;
				break;
			}

			case RANDOM_PATTERN: {
				if (lastPatternLength != patternLength || lastPatternFill != patternFill || lastPatternStyle != patternStyle) {
					int num = 0;
					calculatedSequence.resize(patternLength);
					std::fill(calculatedSequence.begin(), calculatedSequence.end(), 0);
					int fill = 0;
					while (fill < patternFill) {
						if (ldexpf(pcg32_random_r(&pcgRng), -32) < (float)patternFill / (float)patternLength) {
							calculatedSequence[num % patternLength] = 1;
							fill++;
						}
						num++;
					}
				}
				if (patternAccents && (lastPatternAccents != patternAccents || lastPatternFill != patternFill || patternStyle != lastPatternStyle)) {
					int num = 0;
					calculatedAccents.resize(patternFill);
					std::fill(calculatedAccents.begin(), calculatedAccents.end(), 0);
					int accentNum = 0;
					while (accentNum < patternAccents) {
						if (ldexpf(pcg32_random_r(&pcgRng), -32) < (float)patternAccents / (float)patternFill) {
							calculatedAccents[num % patternFill] = 1;
							accentNum++;
						}
						num++;
					}
				}
				break;
			}

			case FIBONACCI_PATTERN: {
				calculatedSequence.resize(patternLength);
				std::fill(calculatedSequence.begin(), calculatedSequence.end(), 0);
				for (int num = 0; num < patternFill; num++) {
					calculatedSequence[getFibonacci(num) % patternLength] = 1;
				}

				calculatedAccents.resize(patternFill);
				std::fill(calculatedAccents.begin(), calculatedAccents.end(), 0);
				for (int accent = 0; accent < patternAccents; accent++) {
					calculatedAccents[getFibonacci(accent) % patternFill] = 1;
				}
				break;
			}

			case LINEAR_PATTERN: {
				calculatedSequence.resize(patternLength);
				std::fill(calculatedSequence.begin(), calculatedSequence.end(), 0);
				for (int num = 0; num < patternFill; num++) {
					calculatedSequence[patternLength * num / patternFill] = 1;
				}

				calculatedAccents.resize(patternFill);
				std::fill(calculatedAccents.begin(), calculatedAccents.end(), 0);
				for (int accent = 0; accent < patternAccents; accent++) {
					calculatedAccents[patternFill * accent / patternAccents] = 1;
				}
				break;
			}
			}
		}

		// Distribute accents on sequence
		finalSequence.fill(0);
		finalAccents.fill(0);
		int j = patternFill - patternAccentRotation;
		for (int i = 0; i != int(calculatedSequence.size()); i++) {
			int index = (i + patternRotation) % patternSize;
			finalSequence[index] = calculatedSequence[i];
			finalAccents[index] = 0;
			if (patternAccents && calculatedSequence[i]) {
				finalAccents[index] = calculatedAccents[j % patternFill];
				j++;
			}
		}

		bCalculate = false;
	}

	void process(const ProcessArgs& args) override {
		if (bCalculate) {
			onReset();
			lastPatternFill = patternFill;
			lastPatternLength = patternLength;
			lastPatternAccents = patternAccents;
			lastPatternStyle = patternStyle;
		}

		bool nextStep = false;

		// Reset sequence.
		if (inputs[INPUT_RESET].isConnected()) {
			if (stResetInput.process(inputs[INPUT_RESET].getVoltage())) {
				if (!params[PARAM_REVERSE].getValue()) {
					currentStep = patternLength + patternPadding;
				}
				else {
					currentStep = 0;
				}
				bCycleReset = true;
			}
		}

		if (inputs[INPUT_CLOCK].isConnected()) {
			if (stClockInput.process(inputs[INPUT_CLOCK].getVoltage())) {
				nextStep = true;
			}
		}

		if (nextStep) {
			if (!params[PARAM_REVERSE].getValue()) {
				currentStep++;
				if (currentStep >= patternLength + patternPadding) {
					currentStep = 0;
					if (!bCycleReset) {
						pgEoc.trigger();
					}
				}
			}
			else {
				currentStep--;
				if (currentStep < 0) {
					currentStep = patternLength + patternPadding - 1;
					if (!bCycleReset) {
						pgEoc.trigger();
					}
				}
			}

			if (bCycleReset && currentStep != 0) {
				bCycleReset = false;
			}

			if (gateMode == GM_TURING) {
				turing = 0;
				for (int i = 0; i < patternLength; i++) {
					turing |= finalSequence[(currentStep + i) % patternLength + patternPadding];
					turing <<= 1;
				}
			}
			else {
				bGateOn = false;
				if (finalSequence[currentStep]) {
					pgGate.trigger();
					if (gateMode == GM_GATE) {
						bGateOn = true;
					}
				}
			}

			bAccentOn = false;
			if (patternAccents && finalAccents[currentStep]) {
				pgAccent.trigger();
				if (gateMode == GM_GATE) {
					bAccentOn = true;
				}
			}
		}

		bool bGatePulse = pgGate.process(args.sampleTime);
		bool bAccentPulse = pgAccent.process(args.sampleTime);

		switch (gateMode)
		{
		case GM_TRIGGER:
		case GM_GATE: {
			outputs[OUTPUT_GATE].setVoltage(bGateOn || bGatePulse ? 10.0f : 0.f);
			break;
		}
		case GM_TURING: {
			outputs[OUTPUT_GATE].setVoltage(10.f * (turing / powf(2.f, patternLength) - 1.f));
			break;
		}
		}

		outputs[OUTPUT_ACCENT].setVoltage(bAccentOn || bAccentPulse ? 10.0f : 0.f);

		outputs[OUTPUT_EOC].setVoltage(pgEoc.process(args.sampleTime) ? 10.0f : 0.f);

		if (clockDivider.process()) {
			patternLength = clamp(params[PARAM_LENGTH].getValue() +
				math::rescale(inputs[INPUT_LENGTH].getNormalVoltage(0.f), -10.f, 0.f, -31.f, 0.f), 1.f, 32.f);

			patternPadding = abs((32.f - patternLength) * clamp(params[PARAM_PADDING].getValue() +
				inputs[INPUT_PADDING].getNormalVoltage(0.f) / 9.f, 0.f, 1.f));

			patternRotation = abs((patternLength + patternPadding - 1.f) * clamp(params[PARAM_ROTATION].getValue() +
				inputs[INPUT_ROTATION].getNormalVoltage(0.f) / 9.f, 0.f, 1.f));

			patternFill = abs((1.f + (patternLength - 1.f) * clamp(params[PARAM_STEPS].getValue() +
				inputs[INPUT_STEPS].getNormalVoltage(0.f) / 9.f, 0.f, 1.f)));

			patternAccents = abs((patternFill)*clamp(params[PARAM_ACCENT].getValue() +
				inputs[INPUT_ACCENT].getNormalVoltage(0.f) / 9.f, 0.f, 1.f));

			if (patternAccents == 0) {
				patternAccentRotation = 0;
			}
			else {
				patternAccentRotation = abs((patternFill - 1.f) * clamp(params[PARAM_SHIFT].getValue() +
					inputs[INPUT_SHIFT].getNormalVoltage(0.f) / 9.f, 0.f, 1.f));
			}

			// New sequence in case of parameter change.
			if (patternLength + patternRotation + patternAccents + patternFill + patternPadding + patternAccentRotation != patternChecksum) {
				patternChecksum = patternLength + patternRotation + patternAccents + patternFill + patternPadding + patternAccentRotation;
				bCalculate = true;
			}

			patternStyle = PatternStyle(params[PARAM_PATTERN_STYLE].getValue());
			if (patternStyle != lastPatternStyle) {
				bCalculate = true;
			}

			gateMode = GateMode(params[PARAM_GATE_MODE].getValue());

			const float sampleTime = args.sampleTime * kClockDivider;

			// Update lights			
			float lightVoltage1;

			lightVoltage1 = outputs[OUTPUT_EOC].getVoltage();
			lights[LIGHT_EOC].setBrightnessSmooth(lightVoltage1, sampleTime);

			lights[LIGHT_PATTERN_STYLE + 0].setBrightness(patternLightColorTable[patternStyle].red ? 0.75f : 0.f);
			lights[LIGHT_PATTERN_STYLE + 1].setBrightness(patternLightColorTable[patternStyle].green ? 0.75f : 0.f);
			lights[LIGHT_PATTERN_STYLE + 2].setBrightness(patternLightColorTable[patternStyle].blue ? 0.75f : 0.f);

			lights[LIGHT_REVERSE].setBrightnessSmooth(params[PARAM_REVERSE].getValue() ? 0.75f : 0.f, sampleTime);

			lights[LIGHT_GATE_MODE + 0].setBrightness(gateModeLightColorTable[gateMode].red ? 0.75f : 0.f);
			lights[LIGHT_GATE_MODE + 1].setBrightness(gateModeLightColorTable[gateMode].green ? 0.75f : 0.f);
			lights[LIGHT_GATE_MODE + 2].setBrightness(gateModeLightColorTable[gateMode].blue ? 0.75f : 0.f);

			lightVoltage1 = outputs[OUTPUT_GATE].getVoltage() / 10.f;
			float lightVoltage2 = outputs[OUTPUT_ACCENT].getVoltage() / 10.f;
			lights[LIGHT_OUTPUT + 0].setBrightnessSmooth(-lightVoltage1, sampleTime);
			lights[LIGHT_OUTPUT + 1].setBrightnessSmooth(lightVoltage1, sampleTime);
			lights[LIGHT_OUTPUT + 2].setBrightnessSmooth(lightVoltage2, sampleTime);
		}
	}
};

static const std::array<bool, MAXLEN / 2> browserSequence = { true, false, false, false, true, false, false, false, true, false, false, false, true, false, false, false };

struct SphinxDisplay : TransparentWidget {
	Sphinx* module;
	std::array<bool, MAXLEN * 2>* sequence = nullptr;
	std::array<bool, MAXLEN * 2>* accents = nullptr;
	int* currentStep = nullptr;
	int* patternFill = nullptr;
	int* patternLength = nullptr;
	int* patternPadding = nullptr;
	PatternStyle* patternStyle = nullptr;
	struct DisplayColors {
		NVGcolor backgroundColor;
		NVGcolor inactiveColor;
		NVGcolor activeColor;
	};
	DisplayColors arrayDisplayColors[4];

	SphinxDisplay() {
		arrayDisplayColors[0].backgroundColor = nvgRGB(0x30, 0x10, 0x10);
		arrayDisplayColors[0].inactiveColor = nvgRGB(0x7f, 0x00, 0x00);
		arrayDisplayColors[0].activeColor = nvgRGB(0xff, 0x00, 0x00);

		arrayDisplayColors[1].backgroundColor = nvgRGB(0x30, 0x10, 0x30);
		arrayDisplayColors[1].inactiveColor = nvgRGB(0x7f, 0x00, 0x7f);
		arrayDisplayColors[1].activeColor = nvgRGB(0xff, 0x00, 0xff);

		arrayDisplayColors[2].backgroundColor = nvgRGB(0x10, 0x30, 0x10);
		arrayDisplayColors[2].inactiveColor = nvgRGB(0x00, 0x7f, 0x00);
		arrayDisplayColors[2].activeColor = nvgRGB(0x00, 0xff, 0x00);

		arrayDisplayColors[3].backgroundColor = nvgRGB(0x10, 0x10, 0x30);
		arrayDisplayColors[3].inactiveColor = nvgRGB(0x00, 0x00, 0x7f);
		arrayDisplayColors[3].activeColor = nvgRGB(0x00, 0x00, 0xff);
	}

	void drawPolygon(NVGcontext* vg) {
		Rect polyBoxSize = Rect(Vec(2, 2), box.size.minus(Vec(2, 2)));

		float circleX = 0.5f * polyBoxSize.size.x + 1;
		float circleY = 0.5f * polyBoxSize.size.y + 1;
		const float radius1 = 0.45f * polyBoxSize.size.x;
		const float radius2 = 0.35f * polyBoxSize.size.x;

		// Circles
		nvgBeginPath(vg);
		if (module && !module->isBypassed()) {
			nvgStrokeColor(vg, arrayDisplayColors[*patternStyle].inactiveColor);
			nvgFillColor(vg, arrayDisplayColors[*patternStyle].activeColor);
		}
		else if (!module) {
			nvgStrokeColor(vg, arrayDisplayColors[0].inactiveColor);
			nvgFillColor(vg, arrayDisplayColors[0].activeColor);
		}
		nvgStrokeWidth(vg, 1.f);
		nvgCircle(vg, circleX, circleY, radius1);
		nvgCircle(vg, circleX, circleY, radius2);
		nvgStroke(vg);

		unsigned len = 0;

		if (module && !module->isBypassed()) {
			len = *patternLength + *patternPadding;
		}
		else if (!module) {
			len = 16;
		}

		if (module && !module->isBypassed()) {
			nvgStrokeColor(vg, arrayDisplayColors[*patternStyle].activeColor);
		}
		else if (!module) {
			nvgStrokeColor(vg, arrayDisplayColors[0].activeColor);
		}
		nvgBeginPath(vg);
		bool first = true;

		// inactive Step Rings
		for (unsigned i = 0; i < len; i++) {
			if (module && !module->isBypassed()) {
				if (!sequence->at(i)) {
					float r = accents->at(i) ? radius1 : radius2;
					float x = circleX + r * cosf(2.f * M_PI * i / len - 0.5f * M_PI);
					float y = circleY + r * sinf(2.f * M_PI * i / len - 0.5f * M_PI);

					nvgBeginPath(vg);
					nvgFillColor(vg, arrayDisplayColors[*patternStyle].backgroundColor);
					nvgStrokeColor(vg, arrayDisplayColors[*patternStyle].inactiveColor);
					nvgStrokeWidth(vg, 1.f);
					nvgCircle(vg, x, y, 2.f);
					nvgFill(vg);
					nvgStroke(vg);
				}
			}
			else if (!module) {
				if (!browserSequence[i]) {
					float r = radius2;
					float x = circleX + r * cosf(2.f * M_PI * i / len - 0.5f * M_PI);
					float y = circleY + r * sinf(2.f * M_PI * i / len - 0.5f * M_PI);

					nvgBeginPath(vg);
					nvgFillColor(vg, arrayDisplayColors[0].backgroundColor);
					nvgStrokeColor(vg, arrayDisplayColors[0].inactiveColor);
					nvgStrokeWidth(vg, 1.f);
					nvgCircle(vg, x, y, 2.f);
					nvgFill(vg);
					nvgStroke(vg);
				}
			}
		}

		// Path
		nvgBeginPath(vg);
		if (module && !module->isBypassed()) {
			nvgStrokeColor(vg, arrayDisplayColors[*patternStyle].activeColor);
		}
		else if (!module) {
			nvgStrokeColor(vg, arrayDisplayColors[0].activeColor);
		}
		nvgStrokeWidth(vg, 1.f);
		for (unsigned int i = 0; i < len; i++) {
			if (module && !module->isBypassed()) {
				if (sequence->at(i)) {
					float a = i / float(len);
					float r = accents->at(i) ? radius1 : radius2;
					float x = circleX + r * cosf(2.f * M_PI * a - 0.5f * M_PI);
					float y = circleY + r * sinf(2.f * M_PI * a - 0.5f * M_PI);

					Vec p(x, y);
					if (*patternFill == 1)
						nvgCircle(vg, x, y, 2.f);
					if (first) {
						nvgMoveTo(vg, p.x, p.y);
						first = false;
					}
					else {
						nvgLineTo(vg, p.x, p.y);
					}
				}
			}
			else if (!module) {
				if (browserSequence[i]) {
					float a = i / float(len);
					float r = radius2;
					float x = circleX + r * cosf(2.f * M_PI * a - 0.5f * M_PI);
					float y = circleY + r * sinf(2.f * M_PI * a - 0.5f * M_PI);

					Vec p(x, y);
					if (first) {
						nvgMoveTo(vg, p.x, p.y);
						first = false;
					}
					else {
						nvgLineTo(vg, p.x, p.y);
					}
				}
			}
		}
		nvgClosePath(vg);
		nvgStroke(vg);

		// Active Step Rings
		for (unsigned i = 0; i < len; i++) {
			if (module && !module->isBypassed()) {
				if (sequence->at(i)) {
					float r = accents->at(i) ? radius1 : radius2;
					float x = circleX + r * cosf(2.f * M_PI * i / len - 0.5f * M_PI);
					float y = circleY + r * sinf(2.f * M_PI * i / len - 0.5f * M_PI);

					nvgBeginPath(vg);
					nvgFillColor(vg, arrayDisplayColors[*patternStyle].backgroundColor);
					nvgStrokeWidth(vg, 1.f);
					nvgStrokeColor(vg, arrayDisplayColors[*patternStyle].activeColor);
					nvgCircle(vg, x, y, 2.f);
					nvgFill(vg);
					nvgStroke(vg);
				}
			}
			else if (!module) {
				if (browserSequence[i]) {
					float r = radius2;
					float x = circleX + r * cosf(2.f * M_PI * i / len - 0.5f * M_PI);
					float y = circleY + r * sinf(2.f * M_PI * i / len - 0.5f * M_PI);

					nvgBeginPath(vg);
					nvgFillColor(vg, arrayDisplayColors[0].backgroundColor);
					nvgStrokeWidth(vg, 1.f);
					nvgStrokeColor(vg, arrayDisplayColors[0].activeColor);
					nvgCircle(vg, x, y, 2.f);
					nvgFill(vg);
					nvgStroke(vg);
				}
			}
		}

		if (module && !module->isBypassed()) {
			float r = accents->at(*currentStep) ? radius1 : radius2;
			float x = circleX + r * cosf(2.f * M_PI * *currentStep / len - 0.5f * M_PI);
			float y = circleY + r * sinf(2.f * M_PI * *currentStep / len - 0.5f * M_PI);
			nvgBeginPath(vg);
			nvgStrokeColor(vg, arrayDisplayColors[*patternStyle].activeColor);
			if (sequence->at(*currentStep)) {
				nvgFillColor(vg, arrayDisplayColors[*patternStyle].activeColor);
			}
			else {
				nvgFillColor(vg, arrayDisplayColors[*patternStyle].backgroundColor);
			}
			nvgCircle(vg, x, y, 2.);
			nvgStrokeWidth(vg, 1.5f);
			nvgFill(vg);
			nvgStroke(vg);
		}
		else if (!module) {
			float r = radius2;
			float x = circleX + r * cosf(2.f * M_PI * 0 / len - 0.5f * M_PI);
			float y = circleY + r * sinf(2.f * M_PI * 0 / len - 0.5f * M_PI);
			nvgBeginPath(vg);
			nvgStrokeColor(vg, arrayDisplayColors[0].activeColor);
			if (browserSequence[0]) {
				nvgFillColor(vg, arrayDisplayColors[0].activeColor);
			}
			else {
				nvgFillColor(vg, arrayDisplayColors[0].backgroundColor);
			}
			nvgCircle(vg, x, y, 2.);
			nvgStrokeWidth(vg, 1.5f);
			nvgFill(vg);
			nvgStroke(vg);
		}
	}

	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, 5.f);
		nvgStrokeWidth(args.vg, 1.5f);
		nvgStrokeColor(args.vg, nvgRGB(100, 100, 100));
		nvgStroke(args.vg);
		nvgFillColor(args.vg, nvgRGB(10, 10, 10));
		nvgFill(args.vg);

		Widget::draw(args);
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			if (module && !module->isBypassed()) {
				nvgBeginPath(args.vg);
				nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, 5.f);
				nvgFillColor(args.vg, arrayDisplayColors[*patternStyle].backgroundColor);
				nvgFill(args.vg);

				// Shape
				if (accents && currentStep && patternFill && patternLength && patternPadding && patternStyle) {
					drawPolygon(args.vg);
					drawRectHalo(args, box.size, arrayDisplayColors[*patternStyle].activeColor, 55, 0.f);
				}
			}
			else if (!module) {
				nvgBeginPath(args.vg);
				nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, 5.f);
				nvgFillColor(args.vg, arrayDisplayColors[0].backgroundColor);
				nvgFill(args.vg);

				// Shape				
				drawPolygon(args.vg);
			}
		}
		Widget::drawLayer(args, layer);
	}
};

struct TL1105Latch : TL1105 {
	TL1105Latch() {
		momentary = false;
		latch = true;
	}
};

struct SphinxWidget : SanguineModuleWidget {
	SphinxWidget(Sphinx* module) {
		setModule(module);

		moduleName = "sphinx";
		panelSize = SIZE_11;
		backplateColor = PLATE_PURPLE;
		bFaceplateSuffix = false;

		makePanel();

		addScrews(SCREW_ALL);

		FramebufferWidget* sphinxFrameBuffer = new FramebufferWidget();
		addChild(sphinxFrameBuffer);

		SphinxDisplay* sphinxDisplay = new SphinxDisplay();
		sphinxDisplay->module = module;
		sphinxDisplay->box.pos = millimetersToPixelsVec(17.126, 8.271);
		sphinxDisplay->box.size = millimetersToPixelsVec(22.14, 22.14);
		sphinxFrameBuffer->addChild(sphinxDisplay);

		if (module) {
			sphinxDisplay->sequence = &module->finalSequence;
			sphinxDisplay->accents = &module->finalAccents;
			sphinxDisplay->patternLength = &module->patternLength;
			sphinxDisplay->patternPadding = &module->patternPadding;
			sphinxDisplay->patternFill = &module->patternFill;
			sphinxDisplay->currentStep = &module->currentStep;
			sphinxDisplay->patternStyle = &module->patternStyle;
		}

		addChild(createLightParamCentered<VCVLightBezelLatch<RedGreenBlueLight>>(millimetersToPixelsVec(48.472, 13.947),
			module, Sphinx::PARAM_PATTERN_STYLE, Sphinx::LIGHT_PATTERN_STYLE));

		addChild(createLightCentered<SmallLight<RedLight>>(millimetersToPixelsVec(41.862, 26.411), module, Sphinx::LIGHT_EOC));
		addChild(createOutputCentered<BananutBlack>(millimetersToPixelsVec(48.472, 26.411), module, Sphinx::OUTPUT_EOC));

		addChild(createParamCentered<BefacoTinyKnobRed>(millimetersToPixelsVec(10.386, 40.197), module, Sphinx::PARAM_LENGTH));
		addChild(createParamCentered<BefacoTinyKnobBlack>(millimetersToPixelsVec(27.82, 40.197), module, Sphinx::PARAM_STEPS));
		addChild(createParamCentered<BefacoTinyKnobRed>(millimetersToPixelsVec(45.414, 40.197), module, Sphinx::PARAM_ROTATION));
		addChild(createParamCentered<BefacoTinyKnobBlack>(millimetersToPixelsVec(10.386, 97.059), module, Sphinx::PARAM_PADDING));
		addChild(createParamCentered<BefacoTinyKnobRed>(millimetersToPixelsVec(27.82, 97.059), module, Sphinx::PARAM_ACCENT));
		addChild(createParamCentered<BefacoTinyKnobBlack>(millimetersToPixelsVec(45.414, 97.059), module, Sphinx::PARAM_SHIFT));

		addChild(createInputCentered<BananutPurple>(millimetersToPixelsVec(10.386, 63.519), module, Sphinx::INPUT_LENGTH));
		addChild(createInputCentered<BananutPurple>(millimetersToPixelsVec(27.82, 63.519), module, Sphinx::INPUT_STEPS));
		addChild(createInputCentered<BananutPurple>(millimetersToPixelsVec(45.414, 63.519), module, Sphinx::INPUT_ROTATION));
		addChild(createInputCentered<BananutPurple>(millimetersToPixelsVec(10.386, 73.871), module, Sphinx::INPUT_PADDING));
		addChild(createInputCentered<BananutPurple>(millimetersToPixelsVec(27.82, 73.871), module, Sphinx::INPUT_ACCENT));
		addChild(createInputCentered<BananutPurple>(millimetersToPixelsVec(45.414, 73.871), module, Sphinx::INPUT_SHIFT));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(millimetersToPixelsVec(19.103, 68.695),
			module, Sphinx::PARAM_REVERSE, Sphinx::LIGHT_REVERSE));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedGreenBlueLight>>>(millimetersToPixelsVec(36.617, 68.695),
			module, Sphinx::PARAM_GATE_MODE, Sphinx::LIGHT_GATE_MODE));

		SanguineTinyNumericDisplay* displayLength = new SanguineTinyNumericDisplay(2, module, 10.386, 50.499);
		sphinxFrameBuffer->addChild(displayLength);
		displayLength->fallbackNumber = 16;

		if (module)
			displayLength->values.numberValue = &module->patternLength;

		SanguineTinyNumericDisplay* displayFill = new SanguineTinyNumericDisplay(2, module, 27.82, 50.499);
		sphinxFrameBuffer->addChild(displayFill);
		displayFill->fallbackNumber = 4;

		if (module)
			displayFill->values.numberValue = &module->patternFill;

		SanguineTinyNumericDisplay* displayRotation = new SanguineTinyNumericDisplay(2, module, 45.414, 50.499);
		sphinxFrameBuffer->addChild(displayRotation);

		if (module)
			displayRotation->values.numberValue = &module->patternRotation;

		SanguineTinyNumericDisplay* displayPadding = new SanguineTinyNumericDisplay(2, module, 10.386, 86.77);
		sphinxFrameBuffer->addChild(displayPadding);

		if (module)
			displayPadding->values.numberValue = &module->patternPadding;

		SanguineTinyNumericDisplay* displayAccent = new SanguineTinyNumericDisplay(2, module, 27.82, 86.77);
		sphinxFrameBuffer->addChild(displayAccent);

		if (module)
			displayAccent->values.numberValue = &module->patternAccents;

		SanguineTinyNumericDisplay* displayAccentRotation = new SanguineTinyNumericDisplay(2, module, 45.414, 86.77);
		sphinxFrameBuffer->addChild(displayAccentRotation);

		if (module)
			displayAccentRotation->values.numberValue = &module->patternAccentRotation;


		addChild(createInputCentered<BananutGreen>(millimetersToPixelsVec(7.326, 112.894), module, Sphinx::INPUT_CLOCK));
		addChild(createInputCentered<BananutGreen>(millimetersToPixelsVec(19.231, 112.894), module, Sphinx::INPUT_RESET));

		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(42.496, 105.958), module, Sphinx::LIGHT_OUTPUT));
		addChild(createOutputCentered<BananutRed>(millimetersToPixelsVec(36.543, 112.894), module, Sphinx::OUTPUT_GATE));
		addChild(createOutputCentered<BananutRed>(millimetersToPixelsVec(48.448, 112.894), module, Sphinx::OUTPUT_ACCENT));

		SanguineStaticRGBLight* clockLight = new SanguineStaticRGBLight(module, "res/clock_lit.svg", 7.326, 105.958, true, kSanguineYellowLight);
		addChild(clockLight);

		SanguineStaticRGBLight* resetLight = new SanguineStaticRGBLight(module, "res/reset_lit.svg", 19.231, 105.958, true, kSanguineYellowLight);
		addChild(resetLight);

		SanguineStaticRGBLight* gateLight = new SanguineStaticRGBLight(module, "res/gate_lit.svg", 36.543, 105.958, true, kSanguineYellowLight);
		addChild(gateLight);

		SanguineStaticRGBLight* accentLight = new SanguineStaticRGBLight(module, "res/accent_lit.svg", 48.448, 105.958, true, kSanguineYellowLight);
		addChild(accentLight);
	}
};

Model* modelSphinx = createModel<Sphinx, SphinxWidget>("Sanguine-Monsters-Sphinx");