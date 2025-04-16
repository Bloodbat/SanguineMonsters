#pragma once

#include "sanguinecomponents.hpp"

struct SeqSwitchSquare : SanguineLightUpRGBSwitch {
	SeqSwitchSquare() {
		setBackground("res/seqs/square_button_bg.svg");
		addHalo(nvgRGB(22, 167, 252));
		addHalo(nvgRGB(238, 249, 113));
	}
};

struct SeqButtonPlay : SeqSwitchSquare {
	SeqButtonPlay() {
		setGlyph("res/seqs/glyph_play_button.svg", 3.165, 2.015);
		addColor(kSanguineBlueLight);
		addColor(kSanguineYellowLight);
	}
};

struct SeqButtonClock : SeqSwitchSquare {
	SeqButtonClock() {
		setGlyph("res/seqs/glyph_clock_button.svg", 1.358, 1.03);
		addColor(kSanguineBlueLight);
		addColor(kSanguineYellowLight);
	}
};

struct SeqButtonDown : SeqSwitchSquare {
	SeqButtonDown() {
		setGlyph("res/seqs/glyph_down_button.svg", 1.263, 3.06);
		addColor(kSanguineBlueLight);
		addColor(kSanguineYellowLight);
	}
};

struct SeqButtonRandom : SeqSwitchSquare {
	SeqButtonRandom() {
		setGlyph("res/seqs/glyph_random_button.svg", 0.848, 0.848);
		addColor(kSanguineBlueLight);
		addColor(kSanguineYellowLight);
	}
};

struct SeqButtonReset : SeqSwitchSquare {
	SeqButtonReset() {
		setGlyph("res/seqs/glyph_reset_button.svg", 0.119, 1.03);
		addColor(kSanguineBlueLight);
		addColor(kSanguineYellowLight);
	}
};

struct SeqButtonUp : SeqSwitchSquare {
	SeqButtonUp() {
		setGlyph("res/seqs/glyph_up_button.svg", 1.041, 0.956);
		addColor(kSanguineBlueLight);
		addColor(kSanguineYellowLight);
	}
};

struct SeqButtonRoundSmall : SanguineLightUpRGBSwitch {
	SeqButtonRoundSmall() {
		setBackground("res/seqs/round_button_small_bg.svg");
	}
};

struct SeqButtonRoundBig : SanguineLightUpRGBSwitch {
	SeqButtonRoundBig() {
		setBackground("res/seqs/round_button_big_bg.svg");
	}
};

struct SeqButtonNoRepeatsSmall : SeqButtonRoundSmall {
	SeqButtonNoRepeatsSmall() {
		setGlyph("res/seqs/no_repeats_glyph.svg", 0.782f, 0.876f);
		addColor(39, 0, 52);
		addColor(206, 61, 255);
		addHalo(nvgRGB(0, 0, 0));
		addHalo(nvgRGB(206, 61, 255));
		momentary = false;
	}
};

struct SeqButtonOneShotSmall : SeqButtonRoundSmall {
	SeqButtonOneShotSmall() {
		setGlyph("res/seqs/one_shot_glyph.svg", 0.356f, 0.356f);
		addColor(52, 0, 0);
		addColor(255, 11, 11);
		addHalo(nvgRGB(0, 0, 0));
		addHalo(nvgRGB(255, 11, 11));
		momentary = false;
	}
};

struct SeqButtonResetToOne : SeqButtonRoundSmall {
	SeqButtonResetToOne() {
		setGlyph("res/seqs/reset_to_one_glyph.svg", 0.554f, 0.356f);
		addColor(26, 26, 26);
		addColor(230, 230, 230);
		addHalo(nvgRGB(0, 0, 0));
		addHalo(nvgRGB(230, 230, 230));
		momentary = false;
	}
};

struct SeqButtonRestartSmall : SeqButtonRoundSmall {
	SeqButtonRestartSmall() {
		setGlyph("res/seqs/restart_glyph.svg", 0.714f, 0.356f);
		addColor(0, 46, 0);
		addColor(0, 255, 0);
		addHalo(nvgRGB(0, 0, 0));
		addHalo(nvgRGB(0, 255, 0));
		momentary = false;
		latch = true;
	}
};

struct SeqStepButtonBig : SeqButtonRoundBig {
	SeqStepButtonBig() {
		addColor(0, 206, 79);
		addColor(207, 38, 0);
		addColor(51, 51, 51);
		addHalo(nvgRGB(0, 206, 79));
		addHalo(nvgRGB(207, 38, 0));
		addHalo(nvgRGB(0, 0, 0));
		momentary = false;
	}
};

struct SeqStep1Big : SeqStepButtonBig {
	SeqStep1Big() {
		setGlyph("res/seqs/step_1_glyph.svg", 2.157f, 1.304f);
	}
};

struct SeqStep2Big : SeqStepButtonBig {
	SeqStep2Big() {
		setGlyph("res/seqs/step_2_glyph.svg", 2.058f, 1.277f);
	}
};

struct SeqStep3Big : SeqStepButtonBig {
	SeqStep3Big() {
		setGlyph("res/seqs/step_3_glyph.svg", 2.276f, 1.241f);
	}
};

struct SeqStep4Big : SeqStepButtonBig {
	SeqStep4Big() {
		setGlyph("res/seqs/step_4_glyph.svg", 1.685f, 1.305f);
	}
};

struct SeqStep5Big : SeqStepButtonBig {
	SeqStep5Big() {
		setGlyph("res/seqs/step_5_glyph.svg", 2.335f, 1.277f);
	}
};

struct SeqStep6Big : SeqStepButtonBig {
	SeqStep6Big() {
		setGlyph("res/seqs/step_6_glyph.svg", 1.894f, 1.246f);
	}
};

struct SeqStep7Big : SeqStepButtonBig {
	SeqStep7Big() {
		setGlyph("res/seqs/step_7_glyph.svg", 2.058f, 1.309f);
	}
};

struct SeqStep8Big : SeqStepButtonBig {
	SeqStep8Big() {
		setGlyph("res/seqs/step_8_glyph.svg", 2.121f, 1.246f);
	}
};