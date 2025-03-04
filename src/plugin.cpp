#include "plugin.hpp"

Plugin* pluginInstance;

void init(rack::Plugin* p) {
	pluginInstance = p;

	p->addModel(modelSuperSwitch81);
	p->addModel(modelSuperSwitch18);
	p->addModel(modelMonstersBlank);
	p->addModel(modelDollyX);
	p->addModel(modelOraculus);
	p->addModel(modelRaiju);
	p->addModel(modelSphinx);
	p->addModel(modelBrainz);
	p->addModel(modelBukavac);
	p->addModel(modelDungeon);
	p->addModel(modelKitsune);
	p->addModel(modelOubliette);
	p->addModel(modelMedusa);
	p->addModel(modelAion);
	p->addModel(modelWerewolf);
	p->addModel(modelAlchemist);
	p->addModel(modelAlembic);
	p->addModel(modelDenki);
	p->addModel(modelChronos);
	p->addModel(modelFortuna);
	p->addModel(modelManus);

	getDefaultTheme();
}
