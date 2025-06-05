#include "plugin.hpp"

/* TODO: expanders have been disabled for MetaModule: it doesn't support them.
   If MetaModule ever supports them, re-enable them here and add them to the MetaModule json manifest. */

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
#ifndef METAMODULE
	p->addModel(modelAlembic);
	p->addModel(modelDenki);
#endif
	p->addModel(modelChronos);
	p->addModel(modelFortuna);
#ifndef METAMODULE
	p->addModel(modelManus);
	p->addModel(modelCrucible);
#endif

	getDefaultTheme();
}
