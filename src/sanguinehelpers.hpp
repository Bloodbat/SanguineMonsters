#pragma once

#include "rack.hpp"
#include <time.h>

rack::math::Vec millimetersToPixelsVec(const float x, const float y);

rack::math::Vec centerWidgetInMillimeters(rack::widget::Widget* theWidget, const float x, const float y);

inline long long getSystemTimeMs() {
	clock_t now = clock();
	return (long long)(now * 1000.0 / CLOCKS_PER_SEC);
}