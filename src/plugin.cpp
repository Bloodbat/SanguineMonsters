#include "plugin.hpp"

Plugin* pluginInstance;

void init(rack::Plugin* p) {
	pluginInstance = p;

	p->addModel(modelSuperSwitch81);
	p->addModel(modelSuperSwitch18);
	p->addModel(modelMonstersBlank);
	p->addModel(modelDollyX);
	p->addModel(modelOraculus);
}
