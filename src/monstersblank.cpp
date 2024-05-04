#include "plugin.hpp"
#include "sanguinecomponents.hpp"

struct MonstersBlank : Module {

};

struct MonstersBlankWidget : ModuleWidget {
	MonstersBlankWidget(MonstersBlank* module) {
		setModule(module);
		setPanel(Svg::load(asset::plugin(pluginInstance, "res/monsters_blank.svg")));

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		FramebufferWidget* blankFrameBuffer = new FramebufferWidget();
		addChild(blankFrameBuffer);

		SanguineShapedLight* vampLight = new SanguineShapedLight();
		vampLight->box.pos = mm2px(Vec(13.007, 20.804));
		vampLight->wrap();
		vampLight->module = module;
		vampLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/vamp_lit_blank.svg")));
		blankFrameBuffer->addChild(vampLight);

		SanguineShapedLight* monstersLight = new SanguineShapedLight();
		monstersLight->box.pos = mm2px(Vec(3.253, 43.216));
		monstersLight->wrap();
		monstersLight->module = module;
		monstersLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/monsters_lit_blank.svg")));
		blankFrameBuffer->addChild(monstersLight);

		SanguineShapedLight* bloodLight = new SanguineShapedLight();
		bloodLight->box.pos = mm2px(Vec(4.468, 107.571));
		bloodLight->wrap();
		bloodLight->module = module;
		bloodLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/blood_glowy_blank.svg")));
		blankFrameBuffer->addChild(bloodLight);

		SanguineShapedLight* sanguineLogo = new SanguineShapedLight();
		sanguineLogo->box.pos = mm2px(Vec(11.597, 104.861));
		sanguineLogo->wrap();
		sanguineLogo->module = module;
		sanguineLogo->setSvg(Svg::load(asset::plugin(pluginInstance, "res/sanguine_lit_blank.svg")));
		blankFrameBuffer->addChild(sanguineLogo);
	}
};

Model* modelMonstersBlank = createModel<MonstersBlank, MonstersBlankWidget>("Sanguine-Monsters-Blank");
