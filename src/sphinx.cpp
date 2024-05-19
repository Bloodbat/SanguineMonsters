#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "bjorklund.hpp"
#include <array>

#define MAXLEN 32

enum PatternStyle {
	EUCLIDEAN_PATTERN,
	RANDOM_PATTERN,
	FIBONACCI_PATTERN,
	LINEAR_PATTERN
};

struct Sphinx : Module {
	enum ParamIds {
		PARAM_LENGTH,
		PARAM_STEPS,
		PARAM_ROTATION,
		PARAM_SHIFT,
		PARAM_PADDING,
		PARAM_ACCENT,
		PARAM_PATTERN_STYLE,
		PARAM_GATE_MODE,
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
	std::array<bool, MAXLEN * 2> sequence;
	std::array<bool, MAXLEN * 2> accents;

	bool bAccentOn;
	bool bCalculate;
	bool bFromReset;
	bool bGateOn;

	int patternFill = 4;
	int patternLength = 16;
	int patternRotation = 1;
	int patternPadding = 1;
	int patternShift = 1;
	int patternAccent = 3;

	int patternChecksum;

	int lastPatternFill;
	int lastPatternLength;
	int lastPatternAccent;

	int currentStep = 0;
	int turing = 0;

	PatternStyle lastPatternStyle = EUCLIDEAN_PATTERN;
	PatternStyle patternStyle = EUCLIDEAN_PATTERN;

	static const int kClockUpdateFrequency = 16;

	dsp::SchmittTrigger stClock;
	dsp::SchmittTrigger stReset;

	dsp::PulseGenerator pgGate;
	dsp::PulseGenerator pgAccent;
	dsp::PulseGenerator pgEoc;

	dsp::ClockDivider clockDivider;

	enum GateMode {
		GM_TRIGGER,
		GM_GATE,
		GM_TURING
	} gateMode = GM_TRIGGER;

	Sphinx() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		configParam(PARAM_LENGTH, 1.f, 32.f, 16.f, "Length", "", 0.f, 1.f);
		configParam(PARAM_STEPS, 0.f, 1.f, .25f, "Steps", "%", 0.f, 100.f);
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

		sequence.fill(0);
		accents.fill(0);
		onReset();

		clockDivider.setDivision(kClockUpdateFrequency);
	}

	int getFibonacci(int n) {
		return (n < 2) ? n : getFibonacci(n - 1) + getFibonacci(n - 2);
	}

	void onReset() override {
		if (lastPatternLength != patternLength) {
			std::fill(calculatedSequence.begin(), calculatedSequence.end(), 0);
			calculatedSequence.resize(patternLength + patternPadding);
		}
		if (lastPatternFill != patternFill) {
			std::fill(calculatedAccents.begin(), calculatedAccents.end(), 0);
			calculatedAccents.resize(patternFill);
		}

		switch (patternStyle) {
		case EUCLIDEAN_PATTERN: {
			euclid.reset();
			euclid.init(patternLength, patternFill);
			euclid.iter();

			euclid2.reset();
			if (patternAccent > 0) {
				euclid2.init(patternFill, patternAccent);
				euclid2.iter();
			}
			calculatedSequence = euclid.sequence;
			calculatedAccents = euclid2.sequence;
			break;
		}

		case RANDOM_PATTERN: {
			if (lastPatternLength != patternLength || lastPatternFill != patternFill) {
				int n = 0;
				calculatedSequence.resize(patternLength);
				std::fill(calculatedSequence.begin(), calculatedSequence.end(), 0);
				int f = 0;
				while (f < patternFill) {
					if (random::uniform() < (float)patternFill / (float)patternLength) {
						calculatedSequence.at(n % patternLength) = 1;
						f++;
					}
					n++;
				}
			}
			if (patternAccent && (lastPatternAccent != patternAccent || lastPatternFill != patternFill)) {
				int n = 0;
				calculatedAccents.resize(patternFill);
				std::fill(calculatedAccents.begin(), calculatedAccents.end(), 0);
				int nacc = 0;
				while (nacc < patternAccent) {
					if (random::uniform() < (float)patternAccent / (float)patternFill) {
						calculatedAccents.at(n % patternFill) = 1;
						nacc++;
					}
					n++;
				}
			}
			break;
		}

		case FIBONACCI_PATTERN: {
			calculatedSequence.resize(patternLength);
			std::fill(calculatedSequence.begin(), calculatedSequence.end(), 0);
			for (int k = 0; k < patternFill; k++) {
				calculatedSequence.at(getFibonacci(k) % patternLength) = 1;
			}

			calculatedAccents.resize(patternFill);
			std::fill(calculatedAccents.begin(), calculatedAccents.end(), 0);
			for (int a = 0; a < patternAccent; a++) {
				calculatedAccents.at(getFibonacci(a) % patternFill) = 1;
			}
			break;
		}

		case LINEAR_PATTERN: {
			calculatedSequence.resize(patternLength);
			std::fill(calculatedSequence.begin(), calculatedSequence.end(), 0);
			for (int k = 0; k < patternFill; k++) {
				calculatedSequence.at(patternLength * k / patternFill) = 1;
			}

			calculatedAccents.resize(patternFill);
			std::fill(calculatedAccents.begin(), calculatedAccents.end(), 0);
			for (int a = 0; a < patternAccent; a++) {
				calculatedAccents.at(patternFill * a / patternAccent) = 1;
			}
			break;
		}
		}

		// Distribute accents on sequence
		int j = patternFill - patternShift;
		for (int i = 0; i != (int)calculatedSequence.size(); i++) {
			int idx = (i + patternRotation) % (patternLength + patternPadding);
			sequence[idx] = calculatedSequence.at(i);
			accents[idx] = 0;
			if (patternAccent && calculatedSequence.at(i)) {
				accents[idx] = calculatedAccents.at(j % patternFill);
				j++;
			}
		}

		bCalculate = false;
		bFromReset = true;
	}

	void process(const ProcessArgs& args) override {
		if (clockDivider.process()) {
			// Updated only every N samples, so make sure setBrightnessSmooth accounts for this.
			const float sampleTime = APP->engine->getSampleTime() * kClockUpdateFrequency;

			if (bCalculate) {
				onReset();
				lastPatternFill = patternFill;
				lastPatternLength = patternLength;
				lastPatternAccent = patternAccent;
			}

			bool nextStep = false;

			// Reset sequence.
			if (inputs[INPUT_RESET].isConnected()) {
				if (stReset.process(inputs[INPUT_RESET].getVoltage())) {
					currentStep = patternLength + patternPadding;
				}
			}

			if (inputs[INPUT_CLOCK].isConnected()) {
				if (stClock.process(inputs[INPUT_CLOCK].getVoltage())) {
					nextStep = true;
				}
			}

			if (nextStep) {
				currentStep++;
				if (currentStep >= patternLength + patternPadding) {
					currentStep = 0;
					pgEoc.trigger();
				}

				if (gateMode == GM_TURING) {
					turing = 0;
					for (int i = 0; i < patternLength; i++) {
						turing |= sequence[(currentStep + i) % patternLength + patternPadding];
						turing <<= 1;
					}
				}
				else {
					bGateOn = false;
					if (sequence[currentStep]) {
						pgGate.trigger();
						if (gateMode == GM_GATE) {
							bGateOn = true;
						}
					}
				}

				bAccentOn = false;
				if (patternAccent && accents.at(currentStep)) {
					pgAccent.trigger();
					if (gateMode == GM_GATE) {
						bAccentOn = true;
					}
				}
			}

			bool bGatePulse = pgGate.process(1.0 / args.sampleRate);
			bool bAccentPulse = pgAccent.process(1.0 / args.sampleRate);

			if (gateMode == GM_TURING) {
				outputs[OUTPUT_GATE].setVoltage(10.0 * (turing / pow(2., patternLength) - 1.));
			}
			else {
				outputs[OUTPUT_GATE].setVoltage(bGateOn | bGatePulse ? 10.0 : 0.0);
			}
			outputs[OUTPUT_ACCENT].setVoltage(bAccentOn | bAccentPulse ? 10.0 : 0.0);

			outputs[OUTPUT_EOC].setVoltage(pgEoc.process(1.0 / args.sampleRate) ? 10.0f : 0.f);

			patternLength = clamp(params[PARAM_LENGTH].getValue() +
				rescale(inputs[INPUT_LENGTH].getNormalVoltage(0.), -10.f, 0.f, -31.0f, 0.f), 1.f, 32.f);

			patternPadding = abs((32. - patternLength) * clamp(params[PARAM_PADDING].getValue() +
				inputs[INPUT_PADDING].getNormalVoltage(0.) / 9., 0.0f, 1.0f));

			patternRotation = abs((patternLength + patternPadding - 1.) * clamp(params[PARAM_ROTATION].getValue() +
				inputs[INPUT_ROTATION].getNormalVoltage(0.) / 9., 0.0f, 1.0f));

			patternFill = abs((1. + (patternLength - 1.) * clamp(params[PARAM_STEPS].getValue() +
				inputs[INPUT_STEPS].getNormalVoltage(0.) / 9., 0.0f, 1.0f)));

			patternAccent = abs((patternFill)*clamp(params[PARAM_ACCENT].getValue() +
				inputs[INPUT_ACCENT].getNormalVoltage(0.) / 9., 0.0f, 1.0f));

			if (patternAccent == 0) {
				patternShift = 0;
			}
			else {
				patternShift = abs((patternFill - 1.) * clamp(params[PARAM_SHIFT].getValue() +
					inputs[INPUT_SHIFT].getNormalVoltage(0.) / 9., 0.0f, 1.0f));
			}

			// New sequence in case of parameter change.
			if (patternLength + patternRotation + patternAccent + patternFill + patternPadding + patternShift != patternChecksum) {
				patternChecksum = patternLength + patternRotation + patternAccent + patternFill + patternPadding + patternShift;
				bCalculate = true;
			}

			patternStyle = PatternStyle(params[PARAM_PATTERN_STYLE].getValue());
			if (patternStyle != lastPatternStyle) {
				bCalculate = true;
			}

			gateMode = GateMode(params[PARAM_GATE_MODE].getValue());

			// Update lights			
			lights[LIGHT_PATTERN_STYLE + 0].setBrightnessSmooth(patternLightColorTable[patternStyle].red, sampleTime);
			lights[LIGHT_PATTERN_STYLE + 1].setBrightnessSmooth(patternLightColorTable[patternStyle].green, sampleTime);
			lights[LIGHT_PATTERN_STYLE + 2].setBrightnessSmooth(patternLightColorTable[patternStyle].blue, sampleTime);

			lights[LIGHT_GATE_MODE + 0].setBrightnessSmooth(gateModeLightColorTable[gateMode].red, sampleTime);
			lights[LIGHT_GATE_MODE + 1].setBrightnessSmooth(gateModeLightColorTable[gateMode].green, sampleTime);
			lights[LIGHT_GATE_MODE + 2].setBrightnessSmooth(gateModeLightColorTable[gateMode].blue, sampleTime);
		}
	}
};

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
		Rect b = Rect(Vec(2, 2), box.size.minus(Vec(2, 2)));

		float circleX = 0.5 * b.size.x + 1;
		float circleY = 0.5 * b.size.y + 1;
		const float radius1 = .45 * b.size.x;
		const float radius2 = .35 * b.size.x;

		// Circles
		nvgBeginPath(vg);
		nvgStrokeColor(vg, arrayDisplayColors[*patternStyle].inactiveColor);
		nvgFillColor(vg, arrayDisplayColors[*patternStyle].activeColor);
		nvgStrokeWidth(vg, 1.);
		nvgCircle(vg, circleX, circleY, radius1);
		nvgCircle(vg, circleX, circleY, radius2);
		nvgStroke(vg);

		unsigned len = *patternLength + *patternPadding;

		nvgStrokeColor(vg, arrayDisplayColors[*patternStyle].activeColor);
		nvgBeginPath(vg);
		bool first = true;

		// inactive Step Rings
		for (unsigned i = 0; i < len; i++) {

			if (!sequence->at(i)) {
				float r = accents->at(i) ? radius1 : radius2;
				float x = circleX + r * cosf(2. * M_PI * i / len - .5 * M_PI);
				float y = circleY + r * sinf(2. * M_PI * i / len - .5 * M_PI);

				nvgBeginPath(vg);
				nvgFillColor(vg, arrayDisplayColors[*patternStyle].backgroundColor);
				nvgStrokeWidth(vg, 1.);
				nvgStrokeColor(vg, arrayDisplayColors[*patternStyle].inactiveColor);
				nvgCircle(vg, x, y, 2.);
				nvgFill(vg);
				nvgStroke(vg);
			}
		}

		// Path
		nvgBeginPath(vg);
		nvgStrokeColor(vg, arrayDisplayColors[*patternStyle].activeColor);
		nvgStrokeWidth(vg, 1.);
		for (unsigned int i = 0; i < len; i++) {
			if (sequence->at(i)) {
				float a = i / float(len);
				float r = accents->at(i) ? radius1 : radius2;
				float x = circleX + r * cosf(2. * M_PI * a - .5 * M_PI);
				float y = circleY + r * sinf(2. * M_PI * a - .5 * M_PI);

				Vec p(x, y);
				if (*patternFill == 1)
					nvgCircle(vg, x, y, 2.);
				if (first) {
					nvgMoveTo(vg, p.x, p.y);
					first = false;
				}
				else {
					nvgLineTo(vg, p.x, p.y);
				}
			}
		}
		nvgClosePath(vg);
		nvgStroke(vg);

		// Active Step Rings
		for (unsigned i = 0; i < len; i++) {
			if (sequence->at(i)) {
				float r = accents->at(i) ? radius1 : radius2;
				float x = circleX + r * cosf(2. * M_PI * i / len - .5 * M_PI);
				float y = circleY + r * sinf(2. * M_PI * i / len - .5 * M_PI);

				nvgBeginPath(vg);
				nvgFillColor(vg, arrayDisplayColors[*patternStyle].backgroundColor);
				nvgStrokeWidth(vg, 1.);
				nvgStrokeColor(vg, arrayDisplayColors[*patternStyle].activeColor);
				nvgCircle(vg, x, y, 2.f);
				nvgFill(vg);
				nvgStroke(vg);
			}
		}

		float r = accents->at(*currentStep) ? radius1 : radius2;
		float x = circleX + r * cosf(2. * M_PI * *currentStep / len - .5 * M_PI);
		float y = circleY + r * sinf(2. * M_PI * *currentStep / len - .5 * M_PI);
		nvgBeginPath(vg);
		nvgStrokeColor(vg, arrayDisplayColors[*patternStyle].activeColor);
		if (sequence->at(*currentStep)) {
			nvgFillColor(vg, arrayDisplayColors[*patternStyle].activeColor);
		}
		else {
			nvgFillColor(vg, arrayDisplayColors[*patternStyle].backgroundColor);
		}
		nvgCircle(vg, x, y, 2.);
		nvgStrokeWidth(vg, 1.5);
		nvgFill(vg);
		nvgStroke(vg);
	}

	void draw(const DrawArgs& args) override {

		if (!module) {
			return;
		}

		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.0, 0.0, box.size.x, box.size.y, 5.0);
		nvgStrokeWidth(args.vg, 1.5);
		nvgStrokeColor(args.vg, nvgRGB(0x10, 0x10, 0x10));
		nvgStroke(args.vg);

		Widget::draw(args);
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			if (module && !module->isBypassed()) {
				nvgBeginPath(args.vg);
				nvgRoundedRect(args.vg, 0.0, 0.0, box.size.x, box.size.y, 5.0);
				nvgFillColor(args.vg, arrayDisplayColors[*patternStyle].backgroundColor);
				nvgFill(args.vg);

				// Shape
				if (accents && currentStep && patternFill && patternLength && patternPadding && patternStyle) {
					drawPolygon(args.vg);
					drawRectHalo(args, box.size, arrayDisplayColors[*patternStyle].activeColor, 55, 0.f);
				}
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

struct SphinxWidget : ModuleWidget {
	SphinxWidget(Sphinx* module) {
		setModule(module);
		setPanel(Svg::load(asset::plugin(pluginInstance, "res/sphinx.svg")));

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		FramebufferWidget* sphinxFrameBuffer = new FramebufferWidget();
		addChild(sphinxFrameBuffer);

		SphinxDisplay* sphinxDisplay = new SphinxDisplay();
		sphinxDisplay->module = module;
		sphinxDisplay->box.pos = mm2px(Vec(19.726, 8.271));
		sphinxDisplay->box.size = mm2px(Vec(22.14, 22.14));
		sphinxFrameBuffer->addChild(sphinxDisplay);

		if (module) {
			sphinxDisplay->sequence = &module->sequence;
			sphinxDisplay->accents = &module->accents;
			sphinxDisplay->patternLength = &module->patternLength;
			sphinxDisplay->patternPadding = &module->patternPadding;
			sphinxDisplay->patternFill = &module->patternFill;
			sphinxDisplay->currentStep = &module->currentStep;
			sphinxDisplay->patternStyle = &module->patternStyle;
		}

		addChild(createParamCentered<TL1105Latch>(mm2px(Vec(49.822, 13.947)), module, Sphinx::PARAM_PATTERN_STYLE));

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(44.519, 13.947)), module, Sphinx::LIGHT_PATTERN_STYLE));

		addChild(createOutputCentered<BananutBlack>(mm2px(Vec(48.472, 26.411)), module, Sphinx::OUTPUT_EOC));

		addChild(createParamCentered<BefacoTinyKnobRed>(mm2px(Vec(10.386, 40.197)), module, Sphinx::PARAM_LENGTH));
		addChild(createParamCentered<BefacoTinyKnobBlack>(mm2px(Vec(27.82, 40.197)), module, Sphinx::PARAM_STEPS));
		addChild(createParamCentered<BefacoTinyKnobRed>(mm2px(Vec(45.414, 40.197)), module, Sphinx::PARAM_ROTATION));
		addChild(createParamCentered<BefacoTinyKnobBlack>(mm2px(Vec(10.386, 97.059)), module, Sphinx::PARAM_PADDING));
		addChild(createParamCentered<BefacoTinyKnobRed>(mm2px(Vec(27.82, 97.059)), module, Sphinx::PARAM_ACCENT));
		addChild(createParamCentered<BefacoTinyKnobBlack>(mm2px(Vec(45.414, 97.059)), module, Sphinx::PARAM_SHIFT));

		addChild(createInputCentered<BananutPurple>(mm2px(Vec(10.386, 63.519)), module, Sphinx::INPUT_LENGTH));
		addChild(createInputCentered<BananutPurple>(mm2px(Vec(27.82, 63.519)), module, Sphinx::INPUT_STEPS));
		addChild(createInputCentered<BananutPurple>(mm2px(Vec(45.414, 63.519)), module, Sphinx::INPUT_ROTATION));
		addChild(createInputCentered<BananutPurple>(mm2px(Vec(10.386, 73.871)), module, Sphinx::INPUT_PADDING));
		addChild(createInputCentered<BananutPurple>(mm2px(Vec(27.82, 73.871)), module, Sphinx::INPUT_ACCENT));
		addChild(createInputCentered<BananutPurple>(mm2px(Vec(45.414, 73.871)), module, Sphinx::INPUT_SHIFT));

		SanguineTinyNumericDisplay* displayLength = new SanguineTinyNumericDisplay(2);
		displayLength->box.pos = mm2px(Vec(3.936, 46.499));
		displayLength->module = module;
		sphinxFrameBuffer->addChild(displayLength);

		if (module)
			displayLength->values.numberValue = &module->patternLength;

		SanguineTinyNumericDisplay* displayFill = new SanguineTinyNumericDisplay(2);
		displayFill->box.pos = mm2px(Vec(21.369, 46.499));
		displayFill->module = module;
		sphinxFrameBuffer->addChild(displayFill);

		if (module)
			displayFill->values.numberValue = &module->patternFill;

		SanguineTinyNumericDisplay* displayRotation = new SanguineTinyNumericDisplay(2);
		displayRotation->box.pos = mm2px(Vec(38.964, 46.499));
		displayRotation->module = module;
		sphinxFrameBuffer->addChild(displayRotation);

		if (module)
			displayRotation->values.numberValue = &module->patternRotation;

		SanguineTinyNumericDisplay* displayPadding = new SanguineTinyNumericDisplay(2);
		displayPadding->box.pos = mm2px(Vec(3.936, 82.77));
		displayPadding->module = module;
		sphinxFrameBuffer->addChild(displayPadding);

		if (module)
			displayPadding->values.numberValue = &module->patternPadding;

		SanguineTinyNumericDisplay* displayAccent = new SanguineTinyNumericDisplay(2);
		displayAccent->box.pos = mm2px(Vec(21.369, 82.77));
		displayAccent->module = module;
		sphinxFrameBuffer->addChild(displayAccent);

		if (module)
			displayAccent->values.numberValue = &module->patternAccent;

		SanguineTinyNumericDisplay* displayShift = new SanguineTinyNumericDisplay(2);
		displayShift->box.pos = mm2px(Vec(38.964, 82.77));
		displayShift->module = module;
		sphinxFrameBuffer->addChild(displayShift);

		if (module)
			displayShift->values.numberValue = &module->patternShift;


		addChild(createInputCentered<BananutGreen>(mm2px(Vec(7.326, 112.894)), module, Sphinx::INPUT_CLOCK));
		addChild(createInputCentered<BananutGreen>(mm2px(Vec(19.231, 112.894)), module, Sphinx::INPUT_RESET));

		addChild(createParamCentered<TL1105Latch>(mm2px(Vec(27.9, 110.729)), module, Sphinx::PARAM_GATE_MODE));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(mm2px(Vec(27.9, 104.625)), module, Sphinx::LIGHT_GATE_MODE));

		addChild(createOutputCentered<BananutRed>(mm2px(Vec(36.543, 112.894)), module, Sphinx::OUTPUT_GATE));
		addChild(createOutputCentered<BananutRed>(mm2px(Vec(48.448, 112.894)), module, Sphinx::OUTPUT_ACCENT));

		SanguineShapedLight* clockLight = new SanguineShapedLight();
		clockLight->box.pos = mm2px(Vec(5.642, 104.058));
		clockLight->wrap();
		clockLight->module = module;
		clockLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/clock_lit_mono.svg")));
		sphinxFrameBuffer->addChild(clockLight);

		SanguineShapedLight* resetLight = new SanguineShapedLight();
		resetLight->box.pos = mm2px(Vec(17.331, 104.514));
		resetLight->wrap();
		resetLight->module = module;
		resetLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/reset_buttonless_lit_mono.svg")));
		sphinxFrameBuffer->addChild(resetLight);

		SanguineShapedLight* gateLight = new SanguineShapedLight();
		gateLight->box.pos = mm2px(Vec(34.643, 104.058));
		gateLight->wrap();
		gateLight->module = module;
		gateLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/gate_lit_mono.svg")));
		sphinxFrameBuffer->addChild(gateLight);

		SanguineShapedLight* accentLight = new SanguineShapedLight();
		accentLight->box.pos = mm2px(Vec(46.548, 104.058));
		accentLight->wrap();
		accentLight->module = module;
		accentLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/accent_lit_mono.svg")));
		sphinxFrameBuffer->addChild(accentLight);
	}
};

Model* modelSphinx = createModel<Sphinx, SphinxWidget>("Sanguine-Monsters-Sphinx");