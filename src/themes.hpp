#pragma once

#include <rack.hpp>

enum PanelSizes {
	SIZE_6,
	SIZE_10,
	SIZE_14,
	SIZE_21,
	SIZE_22,
	SIZE_27,
	SIZE_28,
	SIZE_34
};

static const std::vector<std::string> panelSizeStrings = {
	"6hp",
	"10hp",
	"14hp",
	"21hp",
	"22hp",
	"27hp",
	"28hp",
	"34hp"
};

enum BackplateColors {
	PLATE_PURPLE,
	PLATE_RED,
	PLATE_GREEN,
	PLATE_BLACK
};

static const std::vector<std::string> backplateColorStrings = {
	"_purple",
	"_red",
	"_green",
	"_black"
};

enum FaceplateThemes {
	THEME_NONE = -1,
	THEME_VITRIOL,
	THEME_PLUMBAGO
};

static const std::vector<std::string> faceplateThemeStrings = {
	"",
	"_plumbago"
};

static const std::vector<std::string> faceplateMenuLabels = {
	"Vitriol",
	"Plumbago"
};

void getDefaultTheme();
void setDefaultTheme(int themeNum);

extern FaceplateThemes defaultTheme;