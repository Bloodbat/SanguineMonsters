#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"
#include "oubliette.hpp"

using namespace sanguineCommonCode;

struct Oubliette : SanguineModule {
	enum ParamIds {
		PARAMS_COUNT
	};

	enum InputIds {
		ENUMS(INPUT_NULL, oubliette::kMaxSectionPorts),
		INPUTS_COUNT
	};

	enum OutputIds {
		ENUMS(OUTPUT_NULL, oubliette::kMaxSectionPorts),
		OUTPUTS_COUNT
	};

	enum LightIds {
		LIGHTS_COUNT
	};

	Oubliette() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		for (int port = 0; port < oubliette::kMaxSectionPorts; ++port) {
			int currentPort = port + 1;
			configInput(INPUT_NULL + port, string::f("Sink %d", currentPort));
			configOutput(OUTPUT_NULL + port, string::f("0V %d", currentPort));
		}
	}
};

struct OublietteWidget : SanguineModuleWidget {
	explicit OublietteWidget(Oubliette* module) {
		setModule(module);

		moduleName = "oubliette";
		panelSize = sanguineThemes::SIZE_8;
		backplateColor = sanguineThemes::PLATE_PURPLE;
		bFaceplateSuffix = false;

		makePanel();

		addScrews(SCREW_ALL);

		bShowSanguineOptions = false;

		static const float xBase = 6.012;
		static const float xDelta = 9.539;

		static const float yInputsBase = 19.648;
		static const float yOutputsBase = 85.961;
		static const float yDelta = 9.568;

		float currentX = xBase;
		float currentInputsY = yInputsBase;
		float currentOutputsY = yOutputsBase;

		static const int kMaxPortsPerRow = 4;
		static const int kMaxPortsPerColumn = 4;

		for (int y = 0; y < kMaxPortsPerColumn; ++y) {
			for (int x = 0; x < kMaxPortsPerRow; ++x) {
				addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(currentX, currentInputsY),
					module, Oubliette::INPUT_NULL + x + (y * kMaxPortsPerColumn)));
				addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(currentX, currentOutputsY),
					module, Oubliette::OUTPUT_NULL + x + (y * kMaxPortsPerColumn)));
				currentX += xDelta;
			}
			currentX = xBase;
			currentInputsY += yDelta;
			currentOutputsY += yDelta;
		}
	}
};

Model* modelOubliette = createModel<Oubliette, OublietteWidget>("Sanguine-Oubliette");