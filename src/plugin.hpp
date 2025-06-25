#pragma once
#include <rack.hpp>
#include "themes.hpp"

using namespace rack;

/* TODO: expanders have been disabled for MetaModule: it doesn't support them.
   If MetaModule ever supports them, re-enable them here and add them to the MetaModule json manifest. */

extern Plugin* pluginInstance;

extern Model* modelSuperSwitch81;
extern Model* modelSuperSwitch18;
extern Model* modelMonstersBlank;
extern Model* modelDollyX;
extern Model* modelOraculus;
extern Model* modelRaiju;
extern Model* modelSphinx;
extern Model* modelBrainz;
extern Model* modelBukavac;
extern Model* modelDungeon;
extern Model* modelKitsune;
extern Model* modelOubliette;
extern Model* modelMedusa;
extern Model* modelAion;
extern Model* modelWerewolf;
extern Model* modelAlchemist;
extern Model* modelChronos;
extern Model* modelFortuna;

// MetaModule disabled modules go here!

#ifndef METAMODULE
extern Model* modelAlembic;
extern Model* modelDenki;
extern Model* modelManus;
extern Model* modelCrucible;
#endif