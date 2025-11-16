#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"

using namespace sanguineCommonCode;

struct MonstersBlank : SanguineModule {

};

struct MonstersBlankWidget : SanguineModuleWidget {
	explicit MonstersBlankWidget(MonstersBlank* module) {
		setModule(module);

		moduleName = "monsters_blank";
		panelSize = sanguineThemes::SIZE_10;
		backplateColor = sanguineThemes::PLATE_PURPLE;
		bFaceplateSuffix = false;
		bHasCommon = false;

		makePanel();

		addScrews(SCREW_ALL);

#ifndef METAMODULE
		SanguineShapedLight* monstersLight = new SanguineShapedLight(module, "res/monsters_lit_blank.svg", 25.4, 51.62);
		addChild(monstersLight);

		SanguineShapedLight* bloodLight = new SanguineShapedLight(module, "res/blood_glowy_blank.svg", 7.907, 114.709);
		addChild(bloodLight);

		SanguineShapedLight* sanguineLogo = new SanguineShapedLight(module, "res/sanguine_lit_blank.svg", 29.204, 113.209);
		addChild(sanguineLogo);
#endif
	}
};

Model* modelMonstersBlank = createModel<MonstersBlank, MonstersBlankWidget>("Sanguine-Monsters-Blank");
