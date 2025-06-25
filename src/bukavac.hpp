#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-parameter"
#include "pcg_random.hpp"
#pragma GCC diagnostic pop

/** Based on "The Voss algorithm"
http://www.firstpr.com.au/dsp/pink-noise/
*/
template <int QUALITY = 8>
struct PinkNoiseGenerator {
private:
    pcg32 pinkRng;
public:
    void init() {
        pinkRng = pcg32(std::round(system::getUnixTime()));
    }

    int frame = -1;
    float values[QUALITY] = {};

    float process() {
        int lastFrame = frame;
        ++frame;
        if (frame >= (1 << QUALITY))
            frame = 0;
        int diff = lastFrame ^ frame;

        float sum = 0.f;
        for (int value = 0; value < QUALITY; ++value) {
            if (diff & (1 << value)) {
                values[value] = (ldexpf(pinkRng(), -32)) - 0.5f;
            }
            sum += values[value];
        }
        return sum;
    }
};

struct InverseAWeightingFFTFilter {
    static constexpr int BUFFER_LEN = 1024;

    alignas(16) float inputBuffer[BUFFER_LEN] = {};
    alignas(16) float outputBuffer[BUFFER_LEN] = {};
    int frame = 0;
    dsp::RealFFT fft;

    InverseAWeightingFFTFilter() : fft(BUFFER_LEN) {}

    float process(float deltaTime, float x) {
        inputBuffer[frame] = x;
        if (++frame >= BUFFER_LEN) {
            frame = 0;
            alignas(16) float freqBuffer[BUFFER_LEN * 2];
            fft.rfft(inputBuffer, freqBuffer);

            for (int frequency = 0; frequency < BUFFER_LEN; ++frequency) {
                float f = 1 / deltaTime / 2 / BUFFER_LEN * frequency;
                float amp = 0.f;
                if (80.f <= f && f <= 20000.f) {
                    float f2 = f * f;
                    // Inverse A-weighted curve
                    amp = ((424.36f + f2) * std::sqrt((11599.3f + f2) * (544496.f + f2)) * (148693636.f + f2)) / (148693636.f * f2 * f2);
                }
                freqBuffer[2 * frequency + 0] *= amp / BUFFER_LEN;
                freqBuffer[2 * frequency + 1] *= amp / BUFFER_LEN;
            }

            fft.irfft(freqBuffer, outputBuffer);
        }
        return outputBuffer[frame];
    }
};

namespace bukavac {
    static const unsigned char permutations[512] = { 151,160,137,91,90,15,
      131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
      190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
      88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
      77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
      102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
      135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
      5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
      223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
      129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
      251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
      49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
      138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180,
      151,160,137,91,90,15,
      131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
      190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
      88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
      77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
      102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
      135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
      5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
      223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
      129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
      251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
      49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
      138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
    };
}

#define FASTFLOOR(x) ( ((x)>0) ? ((int)x) : (((int)x)-1) )