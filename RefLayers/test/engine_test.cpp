#include "../Source/SpectrumEngine.h"
#include <cstdio>

using namespace rl;

static int nearestIndex (float hz)
{
    int best = 0;
    for (int i = 0; i < kGridSize; ++i)
        if (std::abs (gridFreq (i) - hz) < std::abs (gridFreq (best) - hz))
            best = i;
    return best;
}

static float bandMax (const Curve& c, float hz)
{
    const int i = nearestIndex (hz);
    float m = -999.0f;
    for (int k = std::max (0, i - 3); k <= std::min (kGridSize - 1, i + 3); ++k)
        m = std::max (m, c[(size_t) k]);
    return m;
}

int main()
{
    const double sr = 48000.0;
    SpectrumEngine e;
    e.prepare (sr);
    juce::Thread::sleep (50);

    e.startCapture();

    // 4 s of 1 kHz sine at -6 dBFS, plus one 50 ms 5 kHz burst at -3 dBFS at t=2 s
    const int block = 256;
    const int total = (int) (4.0 * sr);
    std::vector<float> L (block), R (block);
    for (int pos = 0; pos < total; pos += block)
    {
        for (int i = 0; i < block; ++i)
        {
            const double t = (pos + i) / sr;
            float s = 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * t)
                    + 0.2512f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 60.0 * t);
            if (t >= 2.0 && t < 2.05)
                s += 0.7079f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 5000.0 * t);
            L[(size_t) i] = R[(size_t) i] = s;
        }
        const float* ch[] = { L.data(), R.data() };
        e.push (ch, 2, block);
        if ((pos / block) % 16 == 0)
            juce::Thread::sleep (1);
    }
    juce::Thread::sleep (300);
    e.stopCapture();
    const auto r = e.getCapture();

    std::printf ("frames=%d gated=%d seconds=%.2f (expected ~%.2f)\n", r.totalFrames, r.gatedFrames, r.seconds, 4.0);
    std::printf ("1 kHz : peak %.2f dB  avg %.2f dB   (sine -6.02 dBFS)\n", bandMax (r.peakDb, 1000), bandMax (r.avgDb, 1000));
    std::printf ("5 kHz : peak %.2f dB  avg %.2f dB   (50 ms burst -3 dBFS: peak must catch it, avg must be much lower)\n",
                 bandMax (r.peakDb, 5000), bandMax (r.avgDb, 5000));
    std::printf ("60 Hz : peak %.2f dB  avg %.2f dB   (sine -12.0 dBFS)\n", bandMax (r.peakDb, 60), bandMax (r.avgDb, 60));
    std::printf ("200 Hz: peak %.2f dB  avg %.2f dB   (should be very low)\n", bandMax (r.peakDb, 200), bandMax (r.avgDb, 200));
    return 0;
}
