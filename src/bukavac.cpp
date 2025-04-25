#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"
#include "bukavac.hpp"
#include "sanguinerandom.hpp"

struct Bukavac : SanguineModule {
	enum ParamIds {
		PARAM_PERLIN_SPEED,
		PARAM_PERLIN_SPEED_CV,
		PARAM_PERLIN_AMP,
		PARAM_PERLIN_AMP_CV,
		PARAM_PERLIN_WEIGHT0,
		PARAM_PERLIN_WEIGHT1,
		PARAM_PERLIN_WEIGHT2,
		PARAM_PERLIN_WEIGHT3,
		PARAMS_COUNT
	};

	enum InputIds {
		INPUT_PERLIN_SPEED,
		INPUT_PERLIN_AMP,
		INPUTS_COUNT
	};

	enum OutputIds {
		OUTPUT_PERLIN_NOISE_MIX,
		OUTPUT_PERLIN_NOISE0,
		OUTPUT_PERLIN_NOISE1,
		OUTPUT_PERLIN_NOISE2,
		OUTPUT_PERLIN_NOISE3,
		OUTPUT_WHITE,
		OUTPUT_PINK,
		OUTPUT_RED,
		OUTPUT_VIOLET,
		OUTPUT_BLUE,
		OUTPUT_GRAY,
		OUTPUT_PRISM,
		OUTPUTS_COUNT
	};

	enum LightIds {
		LIGHTS_COUNT
	};

	PinkNoiseGenerator<8> pinkNoiseGenerator;
	dsp::IIRFilter<2, 2> redFilter;
	float lastWhite = 0.f;
	float lastPink = 0.f;
	InverseAWeightingFFTFilter grayFilter;

	const int kPerlinOctaves = 4;
	float currentPerlinTime = 0.0;
	float minSpd = 1;
	float maxSpd = 500;
	float oldSpeedVal = 0.f;
	float oldSpeedPctVal = 0.f;
	float noiseOutMix = 0.f;
	float* noise;
	const float maxTime = 511; //FLT_MAX-1000; <-- this needs some more love

	pcg32 pcgRng;

	Bukavac() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);
		configOutput(OUTPUT_WHITE, "White noise");
		outputInfos[OUTPUT_WHITE]->description = "0 dB/octave power density";
		configOutput(OUTPUT_PINK, "Pink noise");
		outputInfos[OUTPUT_PINK]->description = "-3 dB/octave power density";
		configOutput(OUTPUT_RED, "Red noise");
		outputInfos[OUTPUT_RED]->description = "-6 dB/octave power density";
		configOutput(OUTPUT_VIOLET, "Violet noise");
		outputInfos[OUTPUT_VIOLET]->description = "+6 dB/octave power density";
		configOutput(OUTPUT_BLUE, "Blue noise");
		outputInfos[OUTPUT_BLUE]->description = "+3 dB/octave power density";
		configOutput(OUTPUT_GRAY, "Gray noise");
		outputInfos[OUTPUT_GRAY]->description = "Psychoacoustic equal loudness";
		configOutput(OUTPUT_PRISM, "Prism noise");
		outputInfos[OUTPUT_PRISM]->description = "Uniform random numbers";

		configInput(INPUT_PERLIN_SPEED, "Perlin noise speed");
		configParam(PARAM_PERLIN_SPEED, 1.f, 500.f, 250.f, "Perlin noise speed");
		configParam(PARAM_PERLIN_SPEED_CV, -1.f, 1.f, 0.f, "Perlin speed CV");

		configInput(INPUT_PERLIN_AMP, "Perlin noise amplifier");
		configParam(PARAM_PERLIN_AMP, 1.f, 10.f, 1.f, "Perlin noise amplifier");
		configParam(PARAM_PERLIN_AMP_CV, -1.f, 1.f, 0.f, "Perlin amplifier CV");

		configParam(PARAM_PERLIN_WEIGHT0, 0.f, 1.f, 0.25f, "Perlin noise first octave weight", "%", 0.f, 100.f);
		configParam(PARAM_PERLIN_WEIGHT1, 0.f, 1.f, 0.25f, "Perlin noise second octave weight", "%", 0.f, 100.f);
		configParam(PARAM_PERLIN_WEIGHT2, 0.f, 1.f, 0.25f, "Perlin noise third octave weight", "%", 0.f, 100.f);
		configParam(PARAM_PERLIN_WEIGHT3, 0.f, 1.f, 0.25f, "Perlin noise fourth octave weight", "%", 0.f, 100.f);

		configOutput(OUTPUT_PERLIN_NOISE0, "Perlin noise first octave");
		configOutput(OUTPUT_PERLIN_NOISE1, "Perlin noise second octave");
		configOutput(OUTPUT_PERLIN_NOISE2, "Perlin noise third octave");
		configOutput(OUTPUT_PERLIN_NOISE3, "Perlin noise fourth octave");

		configOutput(OUTPUT_PERLIN_NOISE_MIX, "Perlin noise mix");

		// Hard-code coefficients for Butterworth lowpass with cutoff 20 Hz @ 44.1kHz.
		const float b[] = { 0.00425611, 0.00425611 };
		const float a[] = { -0.99148778 };
		redFilter.setCoefficients(b, a);

		noise = new float[kPerlinOctaves];

		pcgRng = pcg32(std::round(system::getUnixTime()));
		pinkNoiseGenerator.init();
	}

	~Bukavac() {
		delete[] noise;
	}

	void process(const ProcessArgs& args) override {
		// All noise from Fundamental Noise is calibrated to 1 RMS.
		// Then they should be scaled to match the RMS of a sine wave with 5V amplitude.
		const float gain = 5.f / std::sqrt(2.f);

		if (outputs[OUTPUT_WHITE].isConnected() || outputs[OUTPUT_RED].isConnected() || outputs[OUTPUT_VIOLET].isConnected() || outputs[OUTPUT_GRAY].isConnected()) {
			// White noise: equal power density
			float white = sanguineRandom::normal();
			outputs[OUTPUT_WHITE].setVoltage(white * gain);

			// Red/Brownian noise: -6dB/oct
			if (outputs[OUTPUT_RED].isConnected()) {
				float red = redFilter.process(white) / 0.0645f;
				outputs[OUTPUT_RED].setVoltage(red * gain);
			}

			// Violet/purple noise: 6dB/oct
			if (outputs[OUTPUT_VIOLET].isConnected()) {
				float violet = (white - lastWhite) / 1.41f;
				lastWhite = white;
				outputs[OUTPUT_VIOLET].setVoltage(violet * gain);
			}

			// Gray noise: psychoacoustic equal loudness curve, specifically inverted A-weighted
			if (outputs[OUTPUT_GRAY].isConnected()) {
				float gray = grayFilter.process(args.sampleTime, white) / 1.67f;
				outputs[OUTPUT_GRAY].setVoltage(gray * gain);
			}
		}

		if (outputs[OUTPUT_PINK].isConnected() || outputs[OUTPUT_BLUE].isConnected()) {
			// Pink noise: -3dB/oct
			float pink = pinkNoiseGenerator.process() / 0.816f;
			outputs[OUTPUT_PINK].setVoltage(pink * gain);

			// Blue noise: 3dB/oct
			if (outputs[OUTPUT_BLUE].isConnected()) {
				float blue = (pink - lastPink) / 0.705f;
				lastPink = pink;
				outputs[OUTPUT_BLUE].setVoltage(blue * gain);
			}
		}

		// Prism noise: uniform noise
		/* Note: Black noise was the original definition, made up by VCV.
		   Amended by me to be Prism(for light ring convenience)... also completely made up. */
		if (outputs[OUTPUT_PRISM].isConnected()) {
			float uniformNoise = ldexpf(pcgRng(), -32);
			outputs[OUTPUT_PRISM].setVoltage(uniformNoise * 10.f - 5.f);
		}

		if (outputs[OUTPUT_PERLIN_NOISE_MIX].isConnected() || outputs[OUTPUT_PERLIN_NOISE0].isConnected() || outputs[OUTPUT_PERLIN_NOISE1].isConnected()
			|| outputs[OUTPUT_PERLIN_NOISE2].isConnected() || outputs[OUTPUT_PERLIN_NOISE3].isConnected()) {

			if (currentPerlinTime > maxTime) {
				currentPerlinTime = 0;
			}

			float deltaTime = 1.0 / args.sampleRate;
			currentPerlinTime += deltaTime;

			float perlinSpeed = params[PARAM_PERLIN_SPEED].getValue();
			if (inputs[INPUT_PERLIN_SPEED].isConnected()) {
				float perlinSpeedVoltage = inputs[INPUT_PERLIN_SPEED].getVoltage() / 5.f;
				float perlinSpeedVoltagePercent = params[PARAM_PERLIN_SPEED_CV].getValue();
				perlinSpeed = getPerlinEffectiveValue(perlinSpeedVoltage, perlinSpeed, perlinSpeedVoltagePercent, 1.f, 500.f);
			}

			float perlinAmplifier = params[PARAM_PERLIN_AMP].getValue();
			if (inputs[INPUT_PERLIN_AMP].isConnected()) {
				float perlinAmplifierVoltage = inputs[INPUT_PERLIN_AMP].getVoltage() / 5.f;
				float perlinAmplifierVoltagePercent = params[PARAM_PERLIN_AMP_CV].getValue();
				perlinAmplifier = getPerlinEffectiveValue(perlinAmplifierVoltage, perlinAmplifier, perlinAmplifierVoltagePercent, 1.f, 10.f);
			}

			float octaveMult = 1.0;
			for (int octave = 0; octave < kPerlinOctaves; ++octave) {
				noise[octave] = perlinAmplifier * getPerlinNoise(currentPerlinTime * perlinSpeed * octaveMult);
				if (outputs[OUTPUT_PERLIN_NOISE0 + octave].isConnected()) {
					outputs[OUTPUT_PERLIN_NOISE0 + octave].setVoltage(noise[octave]);
				}
				octaveMult *= 2;
			}

			if (outputs[OUTPUT_PERLIN_NOISE_MIX].isConnected()) {
				mixPerlinOctaves(noise);
			}
		}
	}

	float makePerlinGradient(int hash, float x) {
		int hashBitmask = hash & 15;
		float gradient = 1.0 + (hashBitmask & 7);
		if (hashBitmask & 8) {
			gradient = -gradient;
		}
		return (gradient * x);
	}

	float getPerlinNoise(float x) {
		int i0 = FASTFLOOR(x);
		int i1 = i0 + 1;
		float x0 = x - i0;
		float x1 = x0 - 1.0;
		float t1 = 1.0 - x1 * x1;
		float n0;
		float n1;
		float t0 = 1.0 - x0 * x0;
		t0 *= t0;
		n0 = t0 * t0 * makePerlinGradient(bukavac::permutations[i0 & 0xff], x0);
		t1 *= t1;
		n1 = t1 * t1 * makePerlinGradient(bukavac::permutations[i1 & 0xff], x1);
		return (0.25f * (n0 + n1));
	}

	void mixPerlinOctaves(float* noise) {
		float totalWeight = 0;
		noiseOutMix = 0.f;
		for (int octave = 0; octave < kPerlinOctaves; ++octave) {
			float currentWeight = params[PARAM_PERLIN_WEIGHT0 + octave].getValue();
			noiseOutMix += noise[octave] * currentWeight;
			totalWeight += currentWeight;
		}
		if (totalWeight == 0) {
			totalWeight = 1.0;
		}
		noiseOutMix /= totalWeight;
		outputs[OUTPUT_PERLIN_NOISE_MIX].setVoltage(noiseOutMix);
	}

	float getPerlinEffectiveValue(const float& inputVoltage, const float& baseValue, const float& attenuverterValue,
		const float& minvalue, const float& maxValue) {
		return clamp(baseValue + ((inputVoltage * maxValue) * attenuverterValue), minvalue, maxValue);
	}
};

struct SanguineCirclePortLight : SanguineStaticRGBLight {
	SvgWidget* blackBorder;
	SanguineCirclePortLight(Module* theModule, const float X, const float Y, bool createCentered, unsigned int newLightColor) :
		SanguineStaticRGBLight(theModule, "res/port_circle_light.svg", X, Y, createCentered, newLightColor) {
		blackBorder = new SvgWidget();
		blackBorder->setSvg(Svg::load(asset::plugin(pluginInstance, "res/port_circle_light_border.svg")));
		fb->addChildBelow(blackBorder, sw);
	};
};

struct BukavacWidget : SanguineModuleWidget {
	explicit BukavacWidget(Bukavac* module) {
		setModule(module);

		moduleName = "bukavac";
		panelSize = SIZE_9;
		backplateColor = PLATE_PURPLE;
		bFaceplateSuffix = false;

		makePanel();

		addScrews(SCREW_ALL);

		SanguineStaticRGBLight* speedLight = new SanguineStaticRGBLight(module, "res/speed_lit.svg", 4.199, 23.402, true, kSanguineBlueLight);
		addChild(speedLight);

		addInput(createInputCentered<BananutBlack>(millimetersToPixelsVec(10.941, 23.402), module, Bukavac::INPUT_PERLIN_SPEED));
		addParam(createParamCentered<BefacoTinyKnobRed>(millimetersToPixelsVec(22.484, 23.402), module, Bukavac::PARAM_PERLIN_SPEED_CV));
		addParam(createParamCentered<Davies1900hRedKnob>(millimetersToPixelsVec(36.827, 23.402), module, Bukavac::PARAM_PERLIN_SPEED));

		SanguineStaticRGBLight* amplifierLight = new SanguineStaticRGBLight(module, "res/amplifier_lit.svg", 4.199, 42.098, true, kSanguineBlueLight);
		addChild(amplifierLight);

		addInput(createInputCentered<BananutBlack>(millimetersToPixelsVec(10.941, 42.098), module, Bukavac::INPUT_PERLIN_AMP));
		addParam(createParamCentered<BefacoTinyKnobBlack>(millimetersToPixelsVec(22.484, 42.098), module, Bukavac::PARAM_PERLIN_AMP_CV));
		addParam(createParamCentered<Davies1900hBlackKnob>(millimetersToPixelsVec(36.827, 42.098), module, Bukavac::PARAM_PERLIN_AMP));

		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(5.376, 57.323), module, Bukavac::PARAM_PERLIN_WEIGHT0));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(17.588, 57.323), module, Bukavac::OUTPUT_PERLIN_NOISE0));
		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(5.376, 70.387), module, Bukavac::PARAM_PERLIN_WEIGHT1));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(17.588, 70.387), module, Bukavac::OUTPUT_PERLIN_NOISE1));
		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(40.33, 57.323), module, Bukavac::PARAM_PERLIN_WEIGHT2));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(28.132, 57.323), module, Bukavac::OUTPUT_PERLIN_NOISE2));
		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(40.33, 70.387), module, Bukavac::PARAM_PERLIN_WEIGHT3));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(28.132, 70.387), module, Bukavac::OUTPUT_PERLIN_NOISE3));

		SanguineBloodLogoLight* bloodLight = new SanguineBloodLogoLight(module, 13.096, 86.429);
		addChild(bloodLight);

		SanguineMonstersLogoLight* monstersLight = new SanguineMonstersLogoLight(module, 26.228, 93.384);
		addChild(monstersLight);

		SanguineCirclePortLight* whiteLight = new SanguineCirclePortLight(module, 7.04, 106.724, true, rgbColorToInt(204, 204, 204));
		addChild(whiteLight);

		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(7.04, 106.724), module, Bukavac::OUTPUT_WHITE));

		SanguineCirclePortLight* pinkLight = new SanguineCirclePortLight(module, 17.583, 106.724, true, rgbColorToInt(255, 153, 203));
		addChild(pinkLight);

		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(17.583, 106.724), module, Bukavac::OUTPUT_PINK));

		SanguineCirclePortLight* redLight = new SanguineCirclePortLight(module, 28.127, 106.724, true, rgbColorToInt(255, 0, 0));
		addChild(redLight);

		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(28.127, 106.724), module, Bukavac::OUTPUT_RED));

		SanguineCirclePortLight* purpleLight = new SanguineCirclePortLight(module, 38.67, 106.724, true, rgbColorToInt(180, 71, 216));
		addChild(purpleLight);

		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(38.67, 106.724), module, Bukavac::OUTPUT_VIOLET));

		SanguineCirclePortLight* blueLight = new SanguineCirclePortLight(module, 7.04, 117.456, true, kSanguineBlueLight);
		addChild(blueLight);

		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(7.04, 117.456), module, Bukavac::OUTPUT_BLUE));

		SanguineCirclePortLight* grayLight = new SanguineCirclePortLight(module, 17.583, 117.456, true, rgbColorToInt(101, 101, 101));
		addChild(grayLight);

		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(17.583, 117.456), module, Bukavac::OUTPUT_GRAY));

		SanguineShapedLight* randomLight = new SanguineShapedLight(module, "res/random_ring_lit.svg", 28.127, 117.456);
		addChild(randomLight);

		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(28.127, 117.456), module, Bukavac::OUTPUT_PRISM));

		SanguineShapedLight* perlinLight = new SanguineShapedLight(module, "res/perlin_ring_lit.svg", 38.67, 117.456);
		addChild(perlinLight);

		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(38.67, 117.456), module, Bukavac::OUTPUT_PERLIN_NOISE_MIX));
	}
};

Model* modelBukavac = createModel<Bukavac, BukavacWidget>("Sanguine-Monsters-Bukavac");