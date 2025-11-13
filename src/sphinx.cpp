#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-parameter"
#include "pcg_random.hpp"
#pragma GCC diagnostic pop

#include "bjorklund.hpp"
#include <array>

#include "sphinx.hpp"

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

	sanguinebjorklund::Bjorklund euclid;
	sanguinebjorklund::Bjorklund euclid2;

	// Calculated sequence/accents.
	std::vector<bool> calculatedSequence;
	std::vector<bool> calculatedAccents;

	// Padded + rotated + distributed.
	std::array<bool, sphinx::kMaxLength * 2> finalSequence;
	std::array<bool, sphinx::kMaxLength * 2> finalAccents;

	bool bAccentOn = false;
	bool bCalculate;
	bool bGateOn = false;
	bool bCycleReset = true;
	bool bHaveReset = false;
	bool bHaveClock = false;

	int patternFill = 4;
	int patternLength = 16;
	int patternRotation = 0;
	int patternPadding = 0;
	int patternAccentRotation = 0;
	int patternAccents = 0;

	int patternChecksum = 0;

	int lastPatternFill = 0;
	int lastPatternLength = 0;
	int lastPatternAccents = -1;

	int currentStep = 0;
	int turing = 0;

	sphinx::PatternStyle lastPatternStyle = sphinx::RANDOM_PATTERN;
	sphinx::PatternStyle patternStyle = sphinx::EUCLIDEAN_PATTERN;

	static const int kClockDivider = 16;

	dsp::SchmittTrigger stClockInput;
	dsp::SchmittTrigger stResetInput;

	dsp::PulseGenerator pgGate;
	dsp::PulseGenerator pgAccent;
	dsp::PulseGenerator pgEoc;

	dsp::ClockDivider clockDivider;

	pcg32 pcgRng;

	sphinx::GateMode gateMode = sphinx::GM_TRIGGER;

	Sphinx() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		configParam(PARAM_LENGTH, 1.f, 32.f, 16.f, "Length", "", 0.f, 1.f);
		configParam(PARAM_STEPS, 0.f, 1.f, 0.25f, "Steps", "%", 0.f, 100.f);
		paramQuantities[PARAM_LENGTH]->snapEnabled = true;
		configParam(PARAM_ROTATION, 0.f, 1.f, 0.f, "Rotation", "%", 0.f, 100.f);
		configParam(PARAM_PADDING, 0.f, 1.f, 0.f, "Padding", "%", 0.f, 100.f);
		configParam(PARAM_ACCENT, 0.f, 1.f, 0.f, "Accents", "%", 0.f, 100.f);
		configParam(PARAM_SHIFT, 0.f, 1.f, 0.f, "Accents rotation", "%", 0.f, 100.f);

		configSwitch(PARAM_PATTERN_STYLE, 0.f, 3.f, 0.f, "Pattern style", sphinx::patternStyleLabels);

		configSwitch(PARAM_GATE_MODE, 0.f, 2.f, 0.f, "Gate mode", sphinx::gateModeLabels);

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

		finalSequence.fill(0);
		finalAccents.fill(0);
		init();

		clockDivider.setDivision(kClockDivider);

		pcgRng = pcg32(static_cast<int>(std::round(system::getUnixTime())));
	}

	void process(const ProcessArgs& args) override {
		using simd::float_4;

		if (bCalculate) {
			init();
			lastPatternFill = patternFill;
			lastPatternLength = patternLength;
			lastPatternAccents = patternAccents;
			lastPatternStyle = patternStyle;
		}

		bool bNextStep = false;

		// Reset sequence.
		if (bHaveReset) {
			if (stResetInput.process(inputs[INPUT_RESET].getVoltage())) {
				if (!params[PARAM_REVERSE].getValue()) {
					currentStep = patternLength + patternPadding;
				} else {
					currentStep = 0;
				}
				bCycleReset = true;
			}
		}

		if (bHaveClock) {
			if (stClockInput.process(inputs[INPUT_CLOCK].getVoltage())) {
				bNextStep = true;
			}
		}

		if (bNextStep) {
			if (!params[PARAM_REVERSE].getValue()) {
				++currentStep;
				if (currentStep >= patternLength + patternPadding) {
					currentStep = 0;
					if (!bCycleReset) {
						pgEoc.trigger();
					}
				}
			} else {
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

			if (gateMode == sphinx::GM_TURING) {
				turing = 0;
				for (int step = 0; step < patternLength; ++step) {
					turing |= finalSequence[(currentStep + step) % patternLength + patternPadding];
					turing <<= 1;
				}
			} else {
				bGateOn = false;
				if (finalSequence[currentStep]) {
					pgGate.trigger();
					if (gateMode == sphinx::GM_GATE) {
						bGateOn = true;
					}
				}
			}

			bAccentOn = false;
			if (patternAccents && finalAccents[currentStep]) {
				pgAccent.trigger();
				if (gateMode == sphinx::GM_GATE) {
					bAccentOn = true;
				}
			}
		}

		bool bGatePulse = pgGate.process(args.sampleTime);
		bool bAccentPulse = pgAccent.process(args.sampleTime);

		float gateVoltage = 0.f;

		float accentVoltage = (bAccentOn | bAccentPulse);

		switch (gateMode) {
		case sphinx::GM_TRIGGER:
		case sphinx::GM_GATE: {
			gateVoltage = bGateOn | bGatePulse;
			break;
		}
		case sphinx::GM_TURING: {
			gateVoltage = turing / powf(2.f, patternLength) - 1.f;
			break;
		}
		}

		outputs[OUTPUT_GATE].setVoltage(gateVoltage * 10.f);

		outputs[OUTPUT_ACCENT].setVoltage(accentVoltage * 10.f);

		outputs[OUTPUT_EOC].setVoltage(pgEoc.process(args.sampleTime) * 10.f);

		if (clockDivider.process()) {
			float_4 parameterVoltages;

			parameterVoltages[0] = inputs[INPUT_PADDING].getVoltage();
			parameterVoltages[1] = inputs[INPUT_ROTATION].getVoltage();
			parameterVoltages[2] = inputs[INPUT_STEPS].getVoltage();
			parameterVoltages[3] = inputs[INPUT_ACCENT].getVoltage();

			parameterVoltages /= 9.f;

			parameterVoltages[0] += params[PARAM_PADDING].getValue();
			parameterVoltages[1] += params[PARAM_ROTATION].getValue();
			parameterVoltages[2] += params[PARAM_STEPS].getValue();
			parameterVoltages[3] += params[PARAM_ACCENT].getValue();

			parameterVoltages = simd::clamp(parameterVoltages, 0.f, 1.f);

			patternLength = clamp(params[PARAM_LENGTH].getValue() +
				math::rescale(inputs[INPUT_LENGTH].getVoltage(), -10.f, 0.f, -31.f, 0.f), 1.f, 32.f);

			patternPadding = abs((32.f - patternLength) * parameterVoltages[0]);
			patternRotation = abs((patternLength + patternPadding - 1.f) * parameterVoltages[1]);
			patternFill = abs(1.f + (patternLength - 1.f) * parameterVoltages[2]);
			patternAccents = abs(patternFill * parameterVoltages[3]);

			if (patternAccents == 0) {
				patternAccentRotation = 0;
			} else {
				patternAccentRotation = abs((patternFill - 1.f) * clamp(params[PARAM_SHIFT].getValue() +
					inputs[INPUT_SHIFT].getVoltage() / 9.f, 0.f, 1.f));
			}

			// New sequence in case of parameter change.
			if (patternLength + patternRotation + patternAccents + patternFill + patternPadding +
				patternAccentRotation != patternChecksum) {
				patternChecksum = patternLength + patternRotation + patternAccents + patternFill + patternPadding + patternAccentRotation;
				bCalculate = true;
			}

			patternStyle = sphinx::PatternStyle(params[PARAM_PATTERN_STYLE].getValue());
			if (patternStyle != lastPatternStyle) {
				bCalculate = true;
			}

			gateMode = static_cast<sphinx::GateMode>(params[PARAM_GATE_MODE].getValue());

			const float sampleTime = args.sampleTime * kClockDivider;

			// Update lights.
			float lightVoltage1;

			lightVoltage1 = outputs[OUTPUT_EOC].getVoltage();
			lights[LIGHT_EOC].setBrightnessSmooth(lightVoltage1, sampleTime);

			lights[LIGHT_PATTERN_STYLE].setBrightness(sphinx::patternLightColorTable[patternStyle].red *
				kSanguineButtonLightValue);
			lights[LIGHT_PATTERN_STYLE + 1].setBrightness(sphinx::patternLightColorTable[patternStyle].green *
				kSanguineButtonLightValue);
			lights[LIGHT_PATTERN_STYLE + 2].setBrightness(sphinx::patternLightColorTable[patternStyle].blue *
				kSanguineButtonLightValue);

			lights[LIGHT_REVERSE].setBrightnessSmooth(params[PARAM_REVERSE].getValue() *
				kSanguineButtonLightValue, sampleTime);

			lights[LIGHT_GATE_MODE].setBrightness(sphinx::gateModeLightColorTable[gateMode].red
				* kSanguineButtonLightValue);
			lights[LIGHT_GATE_MODE + 1].setBrightness(sphinx::gateModeLightColorTable[gateMode].green
				* kSanguineButtonLightValue);
			lights[LIGHT_GATE_MODE + 2].setBrightness(sphinx::gateModeLightColorTable[gateMode].blue
				* kSanguineButtonLightValue);

			lights[LIGHT_OUTPUT].setBrightnessSmooth(-gateVoltage, sampleTime);
			lights[LIGHT_OUTPUT + 1].setBrightnessSmooth(gateVoltage, sampleTime);
			lights[LIGHT_OUTPUT + 2].setBrightnessSmooth(accentVoltage, sampleTime);
		}
	}

	int getFibonacci(int n) {
		return (n < 2) ? n : getFibonacci(n - 1) + getFibonacci(n - 2);
	}

	void init() {
		int patternSize = patternLength + patternPadding;

		if (lastPatternLength != patternLength || lastPatternFill != patternFill ||
			lastPatternStyle != patternStyle || lastPatternAccents != patternAccents) {
			if (lastPatternLength != patternLength || lastPatternStyle != patternStyle) {
				calculatedSequence.resize(patternSize);
				std::fill(calculatedSequence.begin(), calculatedSequence.end(), 0);
			}
			if (lastPatternFill != patternFill || lastPatternStyle != patternStyle) {
				calculatedAccents.resize(patternFill);
				std::fill(calculatedAccents.begin(), calculatedAccents.end(), 0);
			}

			switch (patternStyle) {
			case sphinx::EUCLIDEAN_PATTERN: {
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

			case sphinx::RANDOM_PATTERN: {
				if (lastPatternLength != patternLength || lastPatternFill != patternFill ||
					lastPatternStyle != patternStyle) {
					int num = 0;
					calculatedSequence.resize(patternLength);
					std::fill(calculatedSequence.begin(), calculatedSequence.end(), 0);
					int fill = 0;
					while (fill < patternFill) {
						if (ldexpf(pcgRng(), -32) < static_cast<float>(patternFill) / static_cast<float>(patternLength)) {
							calculatedSequence[num % patternLength] = 1;
							++fill;
						}
						++num;
					}
				}
				if (patternAccents && (lastPatternAccents != patternAccents || lastPatternFill != patternFill ||
					patternStyle != lastPatternStyle)) {
					int num = 0;
					calculatedAccents.resize(patternFill);
					std::fill(calculatedAccents.begin(), calculatedAccents.end(), 0);
					int accentNum = 0;
					while (accentNum < patternAccents) {
						if (ldexpf(pcgRng(), -32) < static_cast<float>(patternAccents) / static_cast<float>(patternFill)) {
							calculatedAccents[num % patternFill] = 1;
							++accentNum;
						}
						++num;
					}
				}
				break;
			}

			case sphinx::FIBONACCI_PATTERN: {
				calculatedSequence.resize(patternLength);
				std::fill(calculatedSequence.begin(), calculatedSequence.end(), 0);
				for (int num = 0; num < patternFill; ++num) {
					calculatedSequence[getFibonacci(num) % patternLength] = 1;
				}

				calculatedAccents.resize(patternFill);
				std::fill(calculatedAccents.begin(), calculatedAccents.end(), 0);
				for (int accent = 0; accent < patternAccents; ++accent) {
					calculatedAccents[getFibonacci(accent) % patternFill] = 1;
				}
				break;
			}

			case sphinx::LINEAR_PATTERN: {
				calculatedSequence.resize(patternLength);
				std::fill(calculatedSequence.begin(), calculatedSequence.end(), 0);
				for (int num = 0; num < patternFill; ++num) {
					calculatedSequence[patternLength * num / patternFill] = 1;
				}

				calculatedAccents.resize(patternFill);
				std::fill(calculatedAccents.begin(), calculatedAccents.end(), 0);
				for (int accent = 0; accent < patternAccents; ++accent) {
					calculatedAccents[patternFill * accent / patternAccents] = 1;
				}
				break;
			}
			}
		}

		// Distribute accents on sequence.
		finalSequence.fill(0);
		finalAccents.fill(0);
		int accent = patternFill - patternAccentRotation;
		for (size_t step = 0; step != calculatedSequence.size(); ++step) {
			int index = (step + patternRotation) % patternSize;
			finalSequence[index] = calculatedSequence[step];
			finalAccents[index] = 0;
			if (patternAccents && calculatedSequence[step]) {
				finalAccents[index] = calculatedAccents[accent % patternFill];
				++accent;
			}
		}

		bCalculate = false;
	}

	void onPortChange(const PortChangeEvent& e) override {
		if (e.type == Port::INPUT) {
			switch (e.portId) {
			case INPUT_RESET:
				bHaveReset = e.connecting;
				break;

			case INPUT_CLOCK:
				bHaveClock = e.connecting;
				break;

			default:
				break;
			}
		}
	}

	void onReset() override {
		init();
	}
};

struct SphinxDisplay : TransparentWidget {
	Sphinx* module = nullptr;
	std::array<bool, sphinx::kMaxLength * 2>* sequence = nullptr;
	std::array<bool, sphinx::kMaxLength * 2>* accents = nullptr;
	int* currentStep = nullptr;
	int* patternFill = nullptr;
	int* patternLength = nullptr;
	int* patternPadding = nullptr;
	sphinx::PatternStyle* patternStyle = nullptr;

	void draw(const DrawArgs& args) override {
		// Display border.
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
				drawDisplayBackground(args.vg, *patternStyle);

				// Shape.
				if (accents && currentStep && patternFill && patternLength && patternPadding && patternStyle) {
					Rect polyBoxSize = Rect(Vec(2, 2), box.size.minus(Vec(2, 2)));

					float circleX = 0.5f * polyBoxSize.size.x + 1;
					float circleY = 0.5f * polyBoxSize.size.y + 1;
					const float radius1 = 0.45f * polyBoxSize.size.x;
					const float radius2 = 0.35f * polyBoxSize.size.x;

					unsigned length = *patternLength + *patternPadding;

					drawCircles(args.vg, *patternStyle, circleX, circleY, radius1, radius2);
					drawInactiveSteps(args.vg, *patternStyle, circleX, circleY, radius1, radius2, length,
						sequence->data(), accents->data());
					drawPath(args.vg, *patternStyle, circleX, circleY, radius1, radius2, length, sequence->data(),
						accents->data(), patternFill);
					drawActiveSteps(args.vg, *patternStyle, circleX, circleY, radius1, radius2, length, sequence->data(),
						accents->data());
					drawCurrentStep(args.vg, *patternStyle, circleX, circleY, radius1, radius2, length, sequence->data(),
						accents->data(), *currentStep);
					drawRectHalo(args, box.size, sphinx::displayColors[*patternStyle].activeColor, 55, 0.f);
				}
			} else if (!module) {
				drawDisplayBackground(args.vg, 0);

				// Shape.
				Rect polyBoxSize = Rect(Vec(2, 2), box.size.minus(Vec(2, 2)));

				float circleX = 0.5f * polyBoxSize.size.x + 1;
				float circleY = 0.5f * polyBoxSize.size.y + 1;
				const float radius1 = 0.45f * polyBoxSize.size.x;
				const float radius2 = 0.35f * polyBoxSize.size.x;

				unsigned length = 0;

				length = 16;

				drawCircles(args.vg, 0, circleX, circleY, radius1, radius2);
				drawInactiveSteps(args.vg, 0, circleX, circleY, radius1, radius2, length,
					sphinx::browserSequence.data(), nullptr);
				drawPath(args.vg, 0, circleX, circleY, radius1, radius2, length, sphinx::browserSequence.data(),
					nullptr, nullptr);
				drawActiveSteps(args.vg, 0, circleX, circleY, radius1, radius2, length, sphinx::browserSequence.data(),
					nullptr);
				drawCurrentStep(args.vg, 0, circleX, circleY, radius1, radius2, length, sphinx::browserSequence.data(),
					nullptr, 0);
			}
		}
		Widget::drawLayer(args, layer);
	}

	void drawCircles(NVGcontext* vg, const int patternStyle, const float& circleX,
		const float& circleY, const float& radius1, const float& radius2) {
		// Circles.
		nvgBeginPath(vg);

		nvgStrokeColor(vg, sphinx::displayColors[patternStyle].inactiveColor);
		nvgFillColor(vg, sphinx::displayColors[patternStyle].activeColor);

		nvgStrokeWidth(vg, 1.f);
		nvgCircle(vg, circleX, circleY, radius1);
		nvgCircle(vg, circleX, circleY, radius2);
		nvgStroke(vg);

		nvgStrokeColor(vg, sphinx::displayColors[patternStyle].activeColor);
	}

	void drawInactiveSteps(NVGcontext* vg, const int patternStyle, const float& circleX,
		const float& circleY, const float& radius1, const float& radius2, const unsigned length,
		const bool* sequence, const bool* accents) {
		nvgBeginPath(vg);

		for (unsigned step = 0; step < length; ++step) {
			if (!sequence[step]) {
				float r = (accents && accents[step]) ? radius1 : radius2;
				float x = circleX + r * cosf(sphinx::kDoublePi * step / length - sphinx::kHalfPi);
				float y = circleY + r * sinf(sphinx::kDoublePi * step / length - sphinx::kHalfPi);

				nvgBeginPath(vg);
				nvgFillColor(vg, sphinx::displayColors[patternStyle].backgroundColor);
				nvgStrokeColor(vg, sphinx::displayColors[patternStyle].inactiveColor);
				nvgStrokeWidth(vg, 1.f);
				nvgCircle(vg, x, y, 2.f);
				nvgFill(vg);
				nvgStroke(vg);
			}
		}
	}

	void drawPath(NVGcontext* vg, const int patternStyle, const float& circleX,
		const float& circleY, const float& radius1, const float& radius2, const unsigned length,
		const bool* sequence, const bool* accents, const int* patternFill) {
		bool bFirst = true;
		nvgBeginPath(vg);
		nvgStrokeColor(vg, sphinx::displayColors[patternStyle].activeColor);
		nvgStrokeWidth(vg, 1.f);

		for (unsigned int step = 0; step < length; ++step) {
			if (sequence[step]) {
				float a = step / static_cast<float>(length);
				float r = (accents && accents[step]) ? radius1 : radius2;
				float x = circleX + r * cosf(sphinx::kDoublePi * a - sphinx::kHalfPi);
				float y = circleY + r * sinf(sphinx::kDoublePi * a - sphinx::kHalfPi);

				Vec p(x, y);
				if (patternFill && *patternFill == 1) {
					nvgCircle(vg, x, y, 2.f);
				}
				if (bFirst) {
					nvgMoveTo(vg, p.x, p.y);
					bFirst = false;
				} else {
					nvgLineTo(vg, p.x, p.y);
				}
			}
		}
		nvgClosePath(vg);
		nvgStroke(vg);
	}

	void drawActiveSteps(NVGcontext* vg, const int patternStyle, const float& circleX,
		const float& circleY, const float& radius1, const float& radius2, const unsigned length,
		const bool* sequence, const bool* accents) {
		for (unsigned step = 0; step < length; ++step) {
			if (sequence[step]) {
				float r = (accents && accents[step]) ? radius1 : radius2;
				float x = circleX + r * cosf(sphinx::kDoublePi * step / length - sphinx::kHalfPi);
				float y = circleY + r * sinf(sphinx::kDoublePi * step / length - sphinx::kHalfPi);

				nvgBeginPath(vg);
				nvgFillColor(vg, sphinx::displayColors[patternStyle].backgroundColor);
				nvgStrokeWidth(vg, 1.f);
				nvgStrokeColor(vg, sphinx::displayColors[patternStyle].activeColor);
				nvgCircle(vg, x, y, 2.f);
				nvgFill(vg);
				nvgStroke(vg);
			}
		}
	}

	void drawCurrentStep(NVGcontext* vg, const int patternStyle, const float& circleX,
		const float& circleY, const float& radius1, const float& radius2, const unsigned length,
		const bool* sequence, const bool* accents, const int currentStep) {
		float r = (accents && accents[currentStep]) ? radius1 : radius2;
		float x = circleX + r * cosf(sphinx::kDoublePi * currentStep / length - sphinx::kHalfPi);
		float y = circleY + r * sinf(sphinx::kDoublePi * currentStep / length - sphinx::kHalfPi);
		nvgBeginPath(vg);
		nvgStrokeColor(vg, sphinx::displayColors[patternStyle].activeColor);
		nvgFillColor(vg, sphinx::displayColors[patternStyle].activeColor);
		nvgCircle(vg, x, y, 2.);
		nvgStrokeWidth(vg, 1.5f);
		nvgFill(vg);
		nvgStroke(vg);
	}

	void drawDisplayBackground(NVGcontext* vg, const int patternStyle) {
		nvgBeginPath(vg);
		nvgRoundedRect(vg, 0.f, 0.f, box.size.x, box.size.y, 5.f);
		nvgFillColor(vg, sphinx::displayColors[patternStyle].backgroundColor);
		nvgFill(vg);
	}
};

struct TL1105Latch : TL1105 {
	TL1105Latch() {
		momentary = false;
		latch = true;
	}
};

struct SphinxWidget : SanguineModuleWidget {
	explicit SphinxWidget(Sphinx* module) {
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

		SanguineTinyNumericDisplay* displayFill = new SanguineTinyNumericDisplay(2, module, 27.82, 50.499);
		sphinxFrameBuffer->addChild(displayFill);
		displayFill->fallbackNumber = 4;

		SanguineTinyNumericDisplay* displayRotation = new SanguineTinyNumericDisplay(2, module, 45.414, 50.499);
		sphinxFrameBuffer->addChild(displayRotation);

		SanguineTinyNumericDisplay* displayPadding = new SanguineTinyNumericDisplay(2, module, 10.386, 86.77);
		sphinxFrameBuffer->addChild(displayPadding);

		SanguineTinyNumericDisplay* displayAccent = new SanguineTinyNumericDisplay(2, module, 27.82, 86.77);
		sphinxFrameBuffer->addChild(displayAccent);

		SanguineTinyNumericDisplay* displayAccentRotation = new SanguineTinyNumericDisplay(2, module, 45.414, 86.77);
		sphinxFrameBuffer->addChild(displayAccentRotation);

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

		SanguineBloodLogoLight* bloodLight = new SanguineBloodLogoLight(module, 27.9, 113.47);
		addChild(bloodLight);

		if (module) {
			sphinxDisplay->sequence = &module->finalSequence;
			sphinxDisplay->accents = &module->finalAccents;
			sphinxDisplay->patternLength = &module->patternLength;
			sphinxDisplay->patternPadding = &module->patternPadding;
			sphinxDisplay->patternFill = &module->patternFill;
			sphinxDisplay->currentStep = &module->currentStep;
			sphinxDisplay->patternStyle = &module->patternStyle;

			displayAccentRotation->values.numberValue = &module->patternAccentRotation;
			displayLength->values.numberValue = &module->patternLength;
			displayFill->values.numberValue = &module->patternFill;
			displayRotation->values.numberValue = &module->patternRotation;
			displayPadding->values.numberValue = &module->patternPadding;
			displayAccent->values.numberValue = &module->patternAccents;
		}
	}
};

Model* modelSphinx = createModel<Sphinx, SphinxWidget>("Sanguine-Monsters-Sphinx");