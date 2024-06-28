#pragma once
#include "rack.hpp"

math::Vec millimetersToPixelsVec(const float x, const float y) {
	return mm2px(Vec(x, y));
}

math::Vec centerWidgetInMillimeters(Widget* theWidget, const float x, const float y) {
	math::Vec widgetVec;
	widgetVec = millimetersToPixelsVec(x, y);
	widgetVec = widgetVec.minus(theWidget->box.size.div(2));
	return widgetVec;
}