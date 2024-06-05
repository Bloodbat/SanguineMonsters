﻿#include "sanguinecomponents.hpp"
#include <color.hpp>

using namespace rack;

extern Plugin* pluginInstance;

// Ports

BananutBlack::BananutBlack() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/BananutBlack.svg")));
}

BananutGreen::BananutGreen() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/BananutGreen.svg")));
}

BananutPurple::BananutPurple() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/BananutPurple.svg")));
}

BananutRed::BananutRed() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/BananutRed.svg")));
}

// Knobs

BefacoTinyKnobRed::BefacoTinyKnobRed() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/BefacoTinyPointWhite.svg")));
	bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/BefacoTinyKnobRed_bg.svg")));
}

BefacoTinyKnobBlack::BefacoTinyKnobBlack() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/BefacoTinyPointWhite.svg")));
	bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/BefacoTinyKnobBlack_bg.svg")));
}

Sanguine1PBlue::Sanguine1PBlue() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PBlue.svg")));
	bg->setSvg(Svg::load(asset::system("res/ComponentLibrary/Rogan1P_bg.svg")));
	fg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PBlue_fg.svg")));
}

Sanguine1PGrayCap::Sanguine1PGrayCap() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PWhite.svg")));
	bg->setSvg(Svg::load(asset::system("res/ComponentLibrary/Rogan1P_bg.svg")));
	fg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PGrayCap_fg.svg")));
}

Sanguine1PGreen::Sanguine1PGreen() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PGreen.svg")));
	bg->setSvg(Svg::load(asset::system("res/ComponentLibrary/Rogan1P_bg.svg")));
	fg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PGreen_fg.svg")));
}

Sanguine1PPurple::Sanguine1PPurple() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PPurple.svg")));
	bg->setSvg(Svg::load(asset::system("res/ComponentLibrary/Rogan1P_bg.svg")));
	fg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PPurple_fg.svg")));
}

Sanguine1PRed::Sanguine1PRed() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PRed.svg")));
	bg->setSvg(Svg::load(asset::system("res/ComponentLibrary/Rogan1P_bg.svg")));
	fg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PRed_fg.svg")));
}

Sanguine1PYellow::Sanguine1PYellow() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PYellow.svg")));
	bg->setSvg(Svg::load(asset::system("res/ComponentLibrary/Rogan1P_bg.svg")));
	fg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PYellow_fg.svg")));
}

Sanguine1PSBlue::Sanguine1PSBlue() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PSBlue.svg")));
	bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PS_bg.svg")));
	fg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PSBlue_fg.svg")));
}

Sanguine1PSGreen::Sanguine1PSGreen() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PSGreen.svg")));
	bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PS_bg.svg")));
	fg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PSGreen_fg.svg")));
}

Sanguine1PSOrange::Sanguine1PSOrange() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PSOrange.svg")));
	bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PS_bg.svg")));
	fg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PSOrange_fg.svg")));
}

Sanguine1PSPurple::Sanguine1PSPurple() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PSPurple.svg")));
	bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PS_bg.svg")));
	fg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PSPurple_fg.svg")));
}

Sanguine1PSRed::Sanguine1PSRed() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PSRed.svg")));
	bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PS_bg.svg")));
	fg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PSRed_fg.svg")));
}

Sanguine1PSYellow::Sanguine1PSYellow() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PSYellow.svg")));
	bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PS_bg.svg")));
	fg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine1PSYellow_fg.svg")));
}

Sanguine2PSBlue::Sanguine2PSBlue() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine2PSBlue.svg")));
	bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine2PS_bg.svg")));
	fg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine2PSBlue_fg.svg")));
}

Sanguine2PSRed::Sanguine2PSRed() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine2PSRed.svg")));
	bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine2PS_bg.svg")));
	fg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine2PSRed_fg.svg")));
}

Sanguine3PSGreen::Sanguine3PSGreen() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine3PSGreen.svg")));
	bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine3PS_bg.svg")));
	fg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine3PSGreen_fg.svg")));
}

Sanguine3PSRed::Sanguine3PSRed() {
	setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine3PSRed.svg")));
	bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine3PS_bg.svg")));
	fg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/Sanguine3PSRed_fg.svg")));
}

// Displays

SanguineBaseSegmentDisplay::SanguineBaseSegmentDisplay(uint32_t newCharacterCount) {
	characterCount = newCharacterCount;
}

void SanguineBaseSegmentDisplay::draw(const DrawArgs& args) {
	// Background		
	nvgBeginPath(args.vg);
	nvgRoundedRect(args.vg, 0.0, 0.0, box.size.x, box.size.y, 5.0);
	nvgFillColor(args.vg, backgroundColor);
	nvgFill(args.vg);
	nvgStrokeWidth(args.vg, 1.0);
	nvgStrokeColor(args.vg, borderColor);
	nvgStroke(args.vg);

	Widget::draw(args);
}

SanguineAlphaDisplay::SanguineAlphaDisplay(uint32_t newCharacterCount) : SanguineBaseSegmentDisplay(newCharacterCount) {
	font = APP->window->loadFont(asset::plugin(pluginInstance, "res/components/Segment14.ttf"));
	box.size = mm2px(Vec(newCharacterCount * 12.6, 21.2));
	fontSize = 40;
}

void SanguineAlphaDisplay::drawLayer(const DrawArgs& args, int layer) {
	if (layer == 1) {
		if (module && !module->isBypassed()) {
			if (font) {
				// Text					
				nvgFontSize(args.vg, fontSize);
				nvgFontFaceId(args.vg, font->handle);
				nvgTextLetterSpacing(args.vg, 2.5);

				Vec textPos = Vec(9, 52);
				nvgFillColor(args.vg, nvgTransRGBA(textColor, 16));
				// Background of all segments
				std::string backgroundText = "";
				for (uint32_t i = 0; i < characterCount; i++)
					backgroundText += "~";
				nvgText(args.vg, textPos.x, textPos.y, backgroundText.c_str(), NULL);
				nvgFillColor(args.vg, textColor);
				if (values.displayText && !(values.displayText->empty()))
				{
					// TODO: Make sure we only display max. display chars.
					nvgText(args.vg, textPos.x, textPos.y, values.displayText->c_str(), NULL);
				}
				drawRectHalo(args, box.size, textColor, haloOpacity, 0.f);
			}
		}
	}
	Widget::drawLayer(args, layer);
}

SanguineLedNumberDisplay::SanguineLedNumberDisplay(uint32_t newCharacterCount) : SanguineBaseSegmentDisplay(newCharacterCount) {
	font = APP->window->loadFont(asset::plugin(pluginInstance, "res/components/Segment7Standard.otf"));
	box.size = mm2px(Vec(newCharacterCount * 7.75, 15));
	fontSize = 33.95;
}

void SanguineLedNumberDisplay::drawLayer(const DrawArgs& args, int layer) {
	if (layer == 1) {
		if (module && !module->isBypassed()) {
			if (font) {
				// Text					
				nvgFontSize(args.vg, fontSize);
				nvgFontFaceId(args.vg, font->handle);
				nvgTextLetterSpacing(args.vg, 2.5);

				Vec textPos = Vec(2, 36);
				nvgFillColor(args.vg, nvgTransRGBA(textColor, 16));
				// Background of all segments
				std::string backgroundText = "";
				for (uint32_t i = 0; i < characterCount; i++)
					backgroundText += "8";
				nvgText(args.vg, textPos.x, textPos.y, backgroundText.c_str(), NULL);
				nvgFillColor(args.vg, textColor);

				std::string displayValue = "";

				if (values.numberValue) {
					displayValue = std::to_string(*values.numberValue);
					if (*values.numberValue < 10)
						displayValue.insert(0, 1, '0');
				}

				nvgText(args.vg, textPos.x, textPos.y, displayValue.c_str(), NULL);
				drawRectHalo(args, box.size, textColor, haloOpacity, 0.f);
			}
		}
	}
	Widget::drawLayer(args, layer);
}

SanguineTinyNumericDisplay::SanguineTinyNumericDisplay(uint32_t newCharacterCount) : SanguineLedNumberDisplay(newCharacterCount) {
	box.size = mm2px(Vec(newCharacterCount * 6.45, 8.f));
	fontSize = 21.4;
};

void SanguineTinyNumericDisplay::drawLayer(const DrawArgs& args, int layer) {
	if (layer == 1) {
		if (module && !module->isBypassed()) {
			if (font) {
				// Text				
				nvgFontSize(args.vg, fontSize);
				nvgFontFaceId(args.vg, font->handle);
				nvgTextLetterSpacing(args.vg, 2.5);

				Vec textPos = Vec(5, 20);
				nvgFillColor(args.vg, nvgTransRGBA(textColor, 16));
				// Background of all segments
				std::string backgroundText = "";
				for (uint32_t i = 0; i < characterCount; i++)
					backgroundText += "8";
				nvgText(args.vg, textPos.x, textPos.y, backgroundText.c_str(), NULL);
				nvgFillColor(args.vg, textColor);

				std::string displayValue = "";

				if (values.numberValue) {
					displayValue = std::to_string(*values.numberValue);
					if (*values.numberValue < 10)
						displayValue.insert(0, 1, '0');
				}

				nvgText(args.vg, textPos.x, textPos.y, displayValue.c_str(), NULL);

				drawRectHalo(args, box.size, textColor, haloOpacity, 0.f);
			}
		}
	}
	Widget::drawLayer(args, layer);
}

Sanguine96x32OLEDDisplay::Sanguine96x32OLEDDisplay() {
	font = APP->window->loadFont(asset::plugin(pluginInstance, "res/components/sanguinematrix.ttf"));
	box.size = mm2px(Vec(16.298, 5.418));
}

void Sanguine96x32OLEDDisplay::draw(const DrawArgs& args) {
	// Background
	NVGcolor backgroundColor = nvgRGB(10, 10, 10);
	NVGcolor borderColor = nvgRGB(100, 100, 100);
	nvgBeginPath(args.vg);
	nvgRect(args.vg, 0.0, 0.0, box.size.x, box.size.y);
	nvgFillColor(args.vg, backgroundColor);
	nvgFill(args.vg);
	nvgStrokeWidth(args.vg, 1.0);
	nvgStrokeColor(args.vg, borderColor);
	nvgStroke(args.vg);

	Widget::draw(args);
}

void Sanguine96x32OLEDDisplay::drawLayer(const DrawArgs& args, int layer) {
	if (layer == 1) {
		if (module && !module->isBypassed()) {
			if (font) {
				if (oledText && !(oledText->empty())) {
					// Text					
					nvgFontSize(args.vg, 6);
					nvgFontFaceId(args.vg, font->handle);

					nvgFillColor(args.vg, textColor);

					Vec textPos = Vec(3, 7.5);
					std::string textCopy;
					textCopy.assign(oledText->data());
					bool multiLine = oledText->size() > 8;
					if (multiLine) {
						std::string displayText = "";
						for (uint32_t i = 0; i < 8; i++)
							displayText += textCopy[i];
						textCopy.erase(0, 8);
						nvgText(args.vg, textPos.x, textPos.y, displayText.c_str(), NULL);
						textPos = Vec(3, 14.5);
						displayText = "";
						for (uint32_t i = 0; (i < 8 || i < textCopy.length()); i++)
							displayText += textCopy[i];
						nvgText(args.vg, textPos.x, textPos.y, displayText.c_str(), NULL);
					}
					else {
						nvgText(args.vg, textPos.x, textPos.y, oledText->c_str(), NULL);
					}
					//drawRectHalo(args, box.size, textColor, 55, 0.f);					
				}
			}
		}
	}
	Widget::drawLayer(args, layer);
}

SanguineMatrixDisplay::SanguineMatrixDisplay(uint32_t newCharacterCount) : SanguineBaseSegmentDisplay(newCharacterCount)
{
	font = APP->window->loadFont(asset::plugin(pluginInstance, "res/components/sanguinematrix.ttf"));
	box.size = mm2px(Vec(newCharacterCount * 5.70275, 10.16));
	fontSize = 16.45;
}

void SanguineMatrixDisplay::drawLayer(const DrawArgs& args, int layer) {
	if (layer == 1) {
		if (module && !module->isBypassed()) {
			if (font) {
				// Text					
				nvgFontSize(args.vg, fontSize);
				nvgFontFaceId(args.vg, font->handle);
				nvgTextLetterSpacing(args.vg, 2);

				// Verify this!
				Vec textPos = Vec(5, 24);
				nvgFillColor(args.vg, nvgTransRGBA(textColor, 16));
				// Background of all segments
				std::string backgroundText = "";
				for (uint32_t i = 0; i < characterCount; i++)
					backgroundText += "█";
				nvgText(args.vg, textPos.x, textPos.y, backgroundText.c_str(), NULL);
				nvgFillColor(args.vg, textColor);
				if (values.displayText && !(values.displayText->empty()))
				{
					// TODO make sure we only display max. display chars					
					nvgText(args.vg, textPos.x, textPos.y, values.displayText->c_str(), NULL);
				}
				drawRectHalo(args, box.size, textColor, haloOpacity, 0.f);
			}
		}
	}
	Widget::drawLayer(args, layer);
}

// Switches

SanguineLightUpSwitch::SanguineLightUpSwitch() {
	momentary = true;
	shadow->opacity = 0.0;
	sw->wrap();
	box.size = sw->box.size;
}

void SanguineLightUpSwitch::addHalo(NVGcolor haloColor) {
	halos.push_back(haloColor);
}

void SanguineLightUpSwitch::drawLayer(const DrawArgs& args, int layer) {
	if (layer == 1) {
		if (module && !module->isBypassed()) {
			uint32_t frameNum = getParamQuantity()->getValue();
			std::shared_ptr<window::Svg> svg = frames[static_cast<int>(frameNum)];
			if (!svg)
				return;
			nvgGlobalCompositeBlendFunc(args.vg, NVG_ONE_MINUS_DST_COLOR, NVG_ONE);
			rack::window::svgDraw(args.vg, svg->handle);
			if (frameNum < halos.size()) {
				drawCircularHalo(args, box.size, halos[frameNum], 175, 8.f);
			}
		}
	}
	Widget::drawLayer(args, layer);
}

Befaco2StepSwitch::Befaco2StepSwitch() {
	addFrame(Svg::load(asset::system("res/ComponentLibrary/BefacoSwitch_0.svg")));
	addFrame(Svg::load(asset::system("res/ComponentLibrary/BefacoSwitch_2.svg")));
}

// Lights

SanguineMultiColoredShapedLight::SanguineMultiColoredShapedLight() {
}

/** Returns the parameterized value of the line p2--p3 where it intersects with p0--p1 */
float SanguineMultiColoredShapedLight::getLineCrossing(math::Vec p0, math::Vec p1, math::Vec p2, math::Vec p3) {
	math::Vec b = p2.minus(p0);
	math::Vec d = p1.minus(p0);
	math::Vec e = p3.minus(p2);
	float m = d.x * e.y - d.y * e.x;
	// Check if lines are parallel, or if either pair of points are equal
	if (std::abs(m) < 1e-6)
		return NAN;
	return -(d.x * b.y - d.y * b.x) / m;
}

NVGcolor SanguineMultiColoredShapedLight::getNVGColor(uint32_t color) {
	return nvgRGBA((color >> 0) & 0xff, (color >> 8) & 0xff, (color >> 16) & 0xff, (color >> 24) & 0xff);
}

NVGpaint SanguineMultiColoredShapedLight::getPaint(NVGcontext* vg, NSVGpaint* p, NVGcolor innerColor, NVGcolor outerColor) {
	assert(p->type == NSVG_PAINT_LINEAR_GRADIENT || p->type == NSVG_PAINT_RADIAL_GRADIENT);
	NSVGgradient* g = p->gradient;
	assert(g->nstops >= 1);

	float inverse[6];
	nvgTransformInverse(inverse, g->xform);
	math::Vec s, e;
	// Is it always the case that the gradient should be transformed from (0, 0) to (0, 1)?
	nvgTransformPoint(&s.x, &s.y, inverse, 0, 0);
	nvgTransformPoint(&e.x, &e.y, inverse, 0, 1);

	NVGpaint paint;
	if (p->type == NSVG_PAINT_LINEAR_GRADIENT)
		paint = nvgLinearGradient(vg, s.x, s.y, e.x, e.y, innerColor, outerColor);
	else
		paint = nvgRadialGradient(vg, s.x, s.y, 0.0, 160, innerColor, outerColor);
	return paint;
};

void SanguineMultiColoredShapedLight::drawLayer(const DrawArgs& args, int layer) {
	if (innerColor) {
		if (layer == 1) {
			if (module && !module->isBypassed()) {
				int shapeIndex = 0;
				NSVGimage* mySvg = svg->handle;
				NSVGimage* myGradient = svgGradient->handle;

				// Iterate shape linked list
				for (NSVGshape* shape = mySvg->shapes; shape; shape = shape->next, shapeIndex++) {

					// Visibility
					if (!(shape->flags & NSVG_FLAGS_VISIBLE))
						continue;

					nvgSave(args.vg);

					// Opacity
					if (shape->opacity < 1.0)
						nvgAlpha(args.vg, shape->opacity);

					// Build path
					nvgBeginPath(args.vg);

					// Iterate path linked list
					for (NSVGpath* path = shape->paths; path; path = path->next) {

						nvgMoveTo(args.vg, path->pts[0], path->pts[1]);
						for (int i = 1; i < path->npts; i += 3) {
							float* p = &path->pts[2 * i];
							nvgBezierTo(args.vg, p[0], p[1], p[2], p[3], p[4], p[5]);
						}

						// Close path
						if (path->closed)
							nvgClosePath(args.vg);

						// Compute whether this is a hole or a solid.
						// Assume that no paths are crossing (usually true for normal SVG graphics).
						// Also assume that the topology is the same if we use straight lines rather than Beziers (not always the case but usually true).
						// Using the even-odd fill rule, if we draw a line from a point on the path to a point outside the boundary (e.g. top left) and count the number of times it crosses another path, the parity of this count determines whether the path is a hole (odd) or solid (even).
						int crossings = 0;
						math::Vec p0 = math::Vec(path->pts[0], path->pts[1]);
						math::Vec p1 = math::Vec(path->bounds[0] - 1.0, path->bounds[1] - 1.0);
						// Iterate all other paths
						for (NSVGpath* path2 = shape->paths; path2; path2 = path2->next) {
							if (path2 == path)
								continue;

							// Iterate all lines on the path
							if (path2->npts < 4)
								continue;
							for (int i = 1; i < path2->npts + 3; i += 3) {
								float* p = &path2->pts[2 * i];
								// The previous point
								math::Vec p2 = math::Vec(p[-2], p[-1]);
								// The current point
								math::Vec p3 = (i < path2->npts) ? math::Vec(p[4], p[5]) : math::Vec(path2->pts[0], path2->pts[1]);
								float crossing = getLineCrossing(p0, p1, p2, p3);
								float crossing2 = getLineCrossing(p2, p3, p0, p1);
								if (0.0 <= crossing && crossing < 1.0 && 0.0 <= crossing2) {
									crossings++;
								}
							}
						}

						if (crossings % 2 == 0)
							nvgPathWinding(args.vg, NVG_SOLID);
						else
							nvgPathWinding(args.vg, NVG_HOLE);
					}

					// Fill shape with external gradient
					if (svgGradient) {
						if (myGradient->shapes->fill.type) {
							switch (myGradient->shapes->fill.type) {
							case NSVG_PAINT_COLOR: {
								//NVGcolor color = nvgRGB(0, 250, 0);
								nvgFillColor(args.vg, *innerColor);
								break;
							}
							case NSVG_PAINT_LINEAR_GRADIENT:
							case NSVG_PAINT_RADIAL_GRADIENT: {
								if (innerColor && outerColor) {
									nvgFillPaint(args.vg, getPaint(args.vg, &myGradient->shapes->fill, *innerColor, *outerColor));
								}
								else {
									nvgFillColor(args.vg, *innerColor);
								}
								break;
							}
							}
							nvgFill(args.vg);
						}
					}
					else {
						NVGcolor color = nvgRGB(0, 250, 0);
						nvgFillColor(args.vg, color);
						nvgFill(args.vg);
					}

					// Stroke shape
					if (shape->stroke.type) {
						nvgStrokeWidth(args.vg, shape->strokeWidth);
						nvgLineCap(args.vg, (NVGlineCap)shape->strokeLineCap);
						nvgLineJoin(args.vg, (int)shape->strokeLineJoin);

						switch (shape->stroke.type) {
						case NSVG_PAINT_COLOR: {
							NVGcolor color = getNVGColor(shape->stroke.color);
							nvgStrokeColor(args.vg, color);
						} break;
						case NSVG_PAINT_LINEAR_GRADIENT: {
							break;
						}
						}
						nvgStroke(args.vg);
					}

					nvgRestore(args.vg);
				}
			}
		}
	}
}

// Decorations

void SanguineShapedLight::draw(const DrawArgs& args) {
	// Do not call Widget::draw: it draws on the wrong layer.	
}

void SanguineShapedLight::drawLayer(const DrawArgs& args, int layer) {
	if (layer == 1) {
		//From SvgWidget::draw()
		if (!sw->svg)
			return;
		if (module && !module->isBypassed()) {
			nvgGlobalCompositeBlendFunc(args.vg, NVG_ONE_MINUS_DST_COLOR, NVG_ONE);
			rack::window::svgDraw(args.vg, sw->svg->handle);
		}
	}
	Widget::drawLayer(args, layer);
}

// Drawing utils

void drawCircularHalo(const Widget::DrawArgs& args, Vec boxSize, NVGcolor haloColor, unsigned char haloOpacity, float radiusFactor)
{
	// Adapted from LightWidget
	// Don't draw halo if rendering in a framebuffer, e.g. screenshots or Module Browser
	if (args.fb)
		return;

	const float halo = settings::haloBrightness;
	if (halo == 0.f)
		return;

	nvgGlobalCompositeBlendFunc(args.vg, NVG_ONE_MINUS_DST_COLOR, NVG_ONE);

	math::Vec c = boxSize.div(2);
	float radius = std::min(boxSize.x, boxSize.y) / radiusFactor;
	float oradius = radius + std::min(radius * 4.f, 15.f);

	nvgBeginPath(args.vg);
	nvgRect(args.vg, c.x - oradius, c.y - oradius, 2 * oradius, 2 * oradius);

	NVGcolor icol = color::mult(nvgTransRGBA(haloColor, haloOpacity), halo);

	NVGcolor ocol = nvgRGBA(0, 0, 0, 0);
	NVGpaint paint = nvgRadialGradient(args.vg, c.x, c.y, radius, oradius, icol, ocol);
	nvgFillPaint(args.vg, paint);
	nvgFill(args.vg);
}

void drawRectHalo(const Widget::DrawArgs& args, Vec boxSize, NVGcolor haloColor, unsigned char haloOpacity, float positionX)
{
	// Adapted from MindMeld & LightWidget.
	if (args.fb)
		return;

	const float halo = settings::haloBrightness;
	if (halo == 0.f)
		return;

	nvgGlobalCompositeBlendFunc(args.vg, NVG_ONE_MINUS_DST_COLOR, NVG_ONE);

	nvgBeginPath(args.vg);
	nvgRect(args.vg, -9 + positionX, -9, boxSize.x + 9, boxSize.y + 9);

	NVGcolor icol = color::mult(nvgTransRGBA(haloColor, haloOpacity), halo);

	NVGcolor ocol = nvgRGBA(0, 0, 0, 0);
	NVGpaint paint = nvgBoxGradient(args.vg, 4.5f + positionX, 4.5f, boxSize.x - 4.5, boxSize.y - 4.5, 5, 8, icol, ocol);
	nvgFillPaint(args.vg, paint);
	nvgFill(args.vg);

	nvgFillPaint(args.vg, paint);
	nvgFill(args.vg);
	nvgGlobalCompositeOperation(args.vg, NVG_SOURCE_OVER);
}