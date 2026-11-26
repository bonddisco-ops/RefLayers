#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace rl
{
constexpr int kGridSize = 480; // ~48 points per octave, 20 Hz .. 20 kHz
constexpr float kMinHz = 20.0f;
constexpr float kMaxHz = 20000.0f;
constexpr float kFloorDb = -140.0f;

using Curve = std::array<float, kGridSize>;

inline float gridFreq (int i)
{
    return kMinHz * std::pow (kMaxHz / kMinHz, (float) i / (float) (kGridSize - 1));
}

inline float freqToNorm (float f)
{
    return std::log (f / kMinHz) / std::log (kMaxHz / kMinHz);
}

inline float powToDb (float p) { return 10.0f * std::log10 (std::max (p, 1.0e-14f)); }
inline float dbToPow (float d) { return std::pow (10.0f, d / 10.0f); }
}
