#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"

struct MonstersBlank : SanguineModule {

};

struct MonstersBlankWidget : SanguineModuleWidget {
	explicit MonstersBlankWidget(MonstersBlank* module) {
		setModule(module);

		moduleName = "monsters_blank";
		panelSize = SIZE_10;
		backplateColor = PLATE_PURPLE;
		bFaceplateSuffix = false;
		bHasCommon = false;

		makePanel();

		addScrews(SCREW_ALL);

		SanguineShapedLight* monstersLight = new SanguineShapedLight(module, "res/monsters_lit_blank.svg", 25.4, 51.62);
		addChild(monstersLight);

		SanguineShapedLight* bloodLight = new SanguineShapedLight(module, "res/blood_glowy_blank.svg", 7.907, 114.709);
		addChild(bloodLight);

		SanguineShapedLight* sanguineLogo = new SanguineShapedLight(module, "res/sanguine_lit_blank.svg", 29.204, 113.209);
		addChild(sanguineLogo);
	}
};

Model* modelMonstersBlank = createModel<MonstersBlank, MonstersBlankWidget>("Sanguine-Monsters-Blank");
