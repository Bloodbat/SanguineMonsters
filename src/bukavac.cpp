#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "pcg_variants.h"

/** Based on "The Voss algorithm"
http://www.firstpr.com.au/dsp/pink-noise/
*/
template <int QUALITY = 8>
struct PinkNoiseGenerator {
private:
	pcg32_random_t pinkRng;
public:
	void init() {
		pcg32_srandom_r(&pinkRng, std::round(system::getUnixTime()), (intptr_t)&pinkRng);
	}

	int frame = -1;
	float values[QUALITY] = {};

	float process() {
		int lastFrame = frame;
		frame++;
		if (frame >= (1 << QUALITY))
			frame = 0;
		int diff = lastFrame ^ frame;

		float sum = 0.f;
		for (int i = 0; i < QUALITY; i++) {
			if (diff & (1 << i)) {
				values[i] = (ldexpf(pcg32_random_r(&pinkRng), -32)) - 0.5f;
			}
			sum += values[i];
		}
		return sum;
	}
};


struct InverseAWeightingFFTFilter {
	static constexpr int BUFFER_LEN = 1024;

	alignas(16) float inputBuffer[BUFFER_LEN] = {};
	alignas(16) float outputBuffer[BUFFER_LEN] = {};
	int frame = 0;
	dsp::RealFFT fft;

	InverseAWeightingFFTFilter() : fft(BUFFER_LEN) {}

	float process(float deltaTime, float x) {
		inputBuffer[frame] = x;
		if (++frame >= BUFFER_LEN) {
			frame = 0;
			alignas(16) float freqBuffer[BUFFER_LEN * 2];
			fft.rfft(inputBuffer, freqBuffer);

			for (int i = 0; i < BUFFER_LEN; i++) {
				float f = 1 / deltaTime / 2 / BUFFER_LEN * i;
				float amp = 0.f;
				if (80.f <= f && f <= 20000.f) {
					float f2 = f * f;
					// Inverse A-weighted curve
					amp = ((424.36f + f2) * std::sqrt((11599.3f + f2) * (544496.f + f2)) * (148693636.f + f2)) / (148693636.f * f2 * f2);
				}
				freqBuffer[2 * i + 0] *= amp / BUFFER_LEN;
				freqBuffer[2 * i + 1] *= amp / BUFFER_LEN;
			}

			fft.irfft(freqBuffer, outputBuffer);
		}
		return outputBuffer[frame];
	}
};

static const unsigned char perm[512] = { 151,160,137,91,90,15,
  131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
  190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
  88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
  77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
  102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
  135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
  5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
  223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
  129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
  251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
  49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
  138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180,
  151,160,137,91,90,15,
  131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
  190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
  88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
  77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
  102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
  135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
  5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
  223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
  129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
  251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
  49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
  138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
};

#define FASTFLOOR(x) ( ((x)>0) ? ((int)x) : (((int)x)-1) )

struct Bukavac : Module {

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
	float oldSpeedVal;
	float oldSpeedPctVal;
	float noiseOutMix;
	float* noise;
	const float maxTime = 511; //FLT_MAX-1000; <-- this needs some more love

	pcg32_random_t pcgRng;

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
		configParam(PARAM_PERLIN_SPEED_CV, 0.f, 1.f, 0.5f, "Perlin speed CV", "%", 0.f, 100.f);

		configInput(INPUT_PERLIN_AMP, "Perlin noise amplifier");
		configParam(PARAM_PERLIN_AMP, 1.f, 10.f, 1.f, "Perlin noise amplifier");
		configParam(PARAM_PERLIN_AMP_CV, 0.f, 1.f, 0.5f, "Perlin amplifier CV", "%", 0.f, 100.f);

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

		pcg32_srandom_r(&pcgRng, std::round(system::getUnixTime()), (intptr_t)&pcgRng);
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
			float white = random::normal();
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
		   Ammended by me to be Prism(for light ring convenience)... also completely made up. */
		if (outputs[OUTPUT_PRISM].isConnected()) {
			float uniformNoise = ldexpf(pcg32_random_r(&pcgRng), -32);
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
				float perlinSpeedVoltage = inputs[INPUT_PERLIN_SPEED].getVoltage() / 5.0;
				float perlinSpeedVoltagePercent = params[PARAM_PERLIN_SPEED_CV].getValue();
				perlinSpeed = getPerlinEffectiveValue(perlinSpeedVoltage, perlinSpeed, perlinSpeedVoltagePercent);
			}

			float perlinAmplifier = params[PARAM_PERLIN_AMP].getValue();
			if (inputs[INPUT_PERLIN_AMP].isConnected()) {
				float perlinAmplifierVoltage = inputs[INPUT_PERLIN_AMP].getVoltage();
				float perlinAmplifierVoltagePercent = params[PARAM_PERLIN_AMP_CV].getValue();
				perlinAmplifier = getPerlinEffectiveValue(perlinAmplifierVoltage, perlinAmplifier, perlinAmplifierVoltagePercent);
			}

			float octaveMult = 1.0;
			for (int i = 0; i < kPerlinOctaves; i++) {
				noise[i] = perlinAmplifier * getPerlinNoise(currentPerlinTime * perlinSpeed * octaveMult);
				if (outputs[OUTPUT_PERLIN_NOISE0 + i].isConnected()) {
					outputs[OUTPUT_PERLIN_NOISE0 + i].setVoltage(noise[i]);
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
		n0 = t0 * t0 * makePerlinGradient(perm[i0 & 0xff], x0);
		t1 *= t1;
		n1 = t1 * t1 * makePerlinGradient(perm[i1 & 0xff], x1);
		return (0.25f * (n0 + n1));
	}

	void mixPerlinOctaves(float* noise) {
		float totalWeight = 0;
		noiseOutMix = 0.f;
		for (int i = 0; i < kPerlinOctaves; i++) {
			float currentWeight = params[PARAM_PERLIN_WEIGHT0 + i].getValue();
			noiseOutMix += noise[i] * currentWeight;
			totalWeight += currentWeight;
		}
		if (totalWeight == 0) {
			totalWeight = 1.0;
		}
		noiseOutMix /= totalWeight;
		outputs[OUTPUT_PERLIN_NOISE_MIX].setVoltage(noiseOutMix);
	}

	float getPerlinEffectiveValue(float& inputVoltage, float& baseValue, float& voltagePercent) {
		return (inputVoltage * voltagePercent) + (baseValue * (1.0 - voltagePercent));
	}
};

struct BukavacWidget : ModuleWidget {
	BukavacWidget(Bukavac* module) {
		setModule(module);

		SanguinePanel* panel = new SanguinePanel(pluginInstance, "res/backplate_9hp_purple.svg", "res/bukavac.svg");
		setPanel(panel);

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		SanguineShapedLight* speedLight = new SanguineShapedLight();
		speedLight->box.pos = mm2px(Vec(2.299, 25.581));
		speedLight->module = module;
		speedLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/speed_lit.svg")));
		addChild(speedLight);

		addInput(createInputCentered<BananutBlack>(mm2px(Vec(10.941, 26.802)), module, Bukavac::INPUT_PERLIN_SPEED));
		addParam(createParamCentered<BefacoTinyKnobRed>(mm2px(Vec(23.184, 26.802)), module, Bukavac::PARAM_PERLIN_SPEED_CV));
		addParam(createParamCentered<Davies1900hRedKnob>(mm2px(Vec(37.527, 26.802)), module, Bukavac::PARAM_PERLIN_SPEED));

		SanguineShapedLight* amplifierLight = new SanguineShapedLight();
		amplifierLight->box.pos = mm2px(Vec(2.299, 43.862));
		amplifierLight->module = module;
		amplifierLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/amplifier_lit.svg")));
		addChild(amplifierLight);

		addInput(createInputCentered<BananutBlack>(mm2px(Vec(10.941, 44.898)), module, Bukavac::INPUT_PERLIN_AMP));
		addParam(createParamCentered<BefacoTinyKnobBlack>(mm2px(Vec(23.184, 44.898)), module, Bukavac::PARAM_PERLIN_AMP_CV));
		addParam(createParamCentered<Davies1900hBlackKnob>(mm2px(Vec(37.527, 44.898)), module, Bukavac::PARAM_PERLIN_AMP));

		addParam(createParamCentered<Trimpot>(mm2px(Vec(5.976, 59.623)), module, Bukavac::PARAM_PERLIN_WEIGHT0));
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(17.588, 59.623)), module, Bukavac::OUTPUT_PERLIN_NOISE0));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(5.976, 71.787)), module, Bukavac::PARAM_PERLIN_WEIGHT1));
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(17.588, 71.787)), module, Bukavac::OUTPUT_PERLIN_NOISE1));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(39.73, 59.623)), module, Bukavac::PARAM_PERLIN_WEIGHT2));
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(28.132, 59.623)), module, Bukavac::OUTPUT_PERLIN_NOISE2));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(39.73, 71.787)), module, Bukavac::PARAM_PERLIN_WEIGHT3));
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(28.132, 71.787)), module, Bukavac::OUTPUT_PERLIN_NOISE3));

		SanguineShapedLight* bloodLight = new SanguineShapedLight();
		bloodLight->box.pos = mm2px(Vec(11.199, 82.493));
		bloodLight->module = module;
		bloodLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/blood_glowy.svg")));
		addChild(bloodLight);

		SanguineShapedLight* monstersLight = new SanguineShapedLight();
		monstersLight->box.pos = mm2px(Vec(18.52, 90.459));
		monstersLight->module = module;
		monstersLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/monsters_lit.svg")));
		addChild(monstersLight);

		SanguineShapedLight* whiteLight = new SanguineShapedLight();
		whiteLight->box.pos = mm2px(Vec(2.04, 101.724));
		whiteLight->module = module;
		whiteLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/white_ring_lit.svg")));
		addChild(whiteLight);

		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(7.04, 106.724)), module, Bukavac::OUTPUT_WHITE));

		SanguineShapedLight* pinkLight = new SanguineShapedLight();
		pinkLight->box.pos = mm2px(Vec(12.584, 101.724));
		pinkLight->module = module;
		pinkLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/pink_ring_lit.svg")));
		addChild(pinkLight);

		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(17.583, 106.724)), module, Bukavac::OUTPUT_PINK));

		SanguineShapedLight* redLight = new SanguineShapedLight();
		redLight->box.pos = mm2px(Vec(23.127, 101.724));
		redLight->module = module;
		redLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/red_ring_lit.svg")));
		addChild(redLight);

		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(28.127, 106.724)), module, Bukavac::OUTPUT_RED));

		SanguineShapedLight* purpleLight = new SanguineShapedLight();
		purpleLight->box.pos = mm2px(Vec(33.67, 101.724));
		purpleLight->module = module;
		purpleLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/purple_ring_lit.svg")));
		addChild(purpleLight);

		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(38.67, 106.724)), module, Bukavac::OUTPUT_VIOLET));

		SanguineShapedLight* blueLight = new SanguineShapedLight();
		blueLight->box.pos = mm2px(Vec(2.04, 112.456));
		blueLight->module = module;
		blueLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/blue_ring_lit.svg")));
		addChild(blueLight);

		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(7.04, 117.456)), module, Bukavac::OUTPUT_BLUE));

		SanguineShapedLight* grayLight = new SanguineShapedLight();
		grayLight->box.pos = mm2px(Vec(12.584, 112.456));
		grayLight->module = module;
		grayLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/gray_ring_lit.svg")));
		addChild(grayLight);

		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(17.583, 117.456)), module, Bukavac::OUTPUT_GRAY));

		SanguineShapedLight* randomLight = new SanguineShapedLight();
		randomLight->box.pos = mm2px(Vec(23.127, 112.456));
		randomLight->module = module;
		randomLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/random_ring_lit.svg")));
		addChild(randomLight);

		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(28.127, 117.456)), module, Bukavac::OUTPUT_PRISM));

		SanguineShapedLight* perlinLight = new SanguineShapedLight();
		perlinLight->box.pos = mm2px(Vec(33.67, 112.456));
		perlinLight->module = module;
		perlinLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/perlin_ring_lit.svg")));
		addChild(perlinLight);

		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(38.67, 117.456)), module, Bukavac::OUTPUT_PERLIN_NOISE_MIX));
	}
};

Model* modelBukavac = createModel<Bukavac, BukavacWidget>("Sanguine-Monsters-Bukavac");