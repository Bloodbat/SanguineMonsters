#pragma once
using simd::float_4;

struct SaturatorFloat_4 {
	static const float_4 limit;

	// Zavalishin 2018, "The Art of VA Filter Design", http://www.native-instruments.com/fileadmin/ni_media/downloads/pdf/VAFilterDesign_2.0.0a.pdf via Bogaudio
	static inline float_4 saturation(float_4 x) {
		const float_4 y1 = 0.98765f; // (2*x - 1)/x**2 where x is 0.9.
		const float_4 offset = 0.075f / SaturatorFloat_4::limit; // Magic.
		float_4 x1 = (x + 1.0f) * 0.5f;
		return limit * (offset + x1 - simd::sqrt(x1 * x1 - y1 * x) * (1.f / y1));
	}

	float_4 next(float_4 sample) {
		float_4 x = sample * (1.f / limit);
		float_4 negativeVoltage = sample < 0.f;
		float_4 saturated = simd::ifelse(negativeVoltage, -saturation(-x), saturation(x));
		return saturated;
	}
};

const float_4 SaturatorFloat_4::limit = 12.f;