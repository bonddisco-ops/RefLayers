#include "SpectrumEngine.h"

namespace rl
{
SpectrumEngine::Resolution::Resolution (int order)
    : size (1 << order), fft (order)
{
    window.resize ((size_t) size);
    juce::dsp::WindowingFunction<float>::fillWindowingTables (window.data(), (size_t) size,
                                                              juce::dsp::WindowingFunction<float>::hann, false);
    buf.assign ((size_t) size * 2, 0.0f);
    binPow.assign ((size_t) size / 2 + 1, 0.0f);
}

void SpectrumEngine::Resolution::rebuild (double sr)
{
    const double binHz = sr / size;
    const int maxBin = size / 2;
    const double halfBand = std::pow (2.0, 1.0 / 48.0); // 1/24 octave band

    for (int i = 0; i < kGridSize; ++i)
    {
        const double f = gridFreq (i);
        auto& b = bands[(size_t) i];
        const int k0 = (int) std::ceil (f / halfBand / binHz);
        const int k1 = (int) std::floor (f * halfBand / binHz);

        if (k1 >= k0 && k0 >= 1)
        {
            b.interp = false;
            b.k0 = juce::jlimit (1, maxBin, k0);
            b.k1 = juce::jlimit (1, maxBin, k1);
        }
        else
        {
            b.interp = true;
            b.pos = (float) juce::jlimit (1.0, (double) maxBin - 1.0, f / binHz);
        }
    }
}

void SpectrumEngine::Resolution::computeBins (const float* newest)
{
    for (int i = 0; i < size; ++i)
        buf[(size_t) i] = newest[i] * window[(size_t) i];
    std::fill (buf.begin() + size, buf.end(), 0.0f);

    fft.performFrequencyOnlyForwardTransform (buf.data(), true);

    // Hann coherent gain 0.5 -> a full-scale sine reads 0 dBFS at its bin
    const float scale = 4.0f / (float) size;
    for (size_t k = 0; k < binPow.size(); ++k)
    {
        const float m = buf[k] * scale;
        binPow[k] = m * m;
    }
}

void SpectrumEngine::Resolution::map (int i, float& out) const
{
    const auto& b = bands[(size_t) i];
    if (b.interp)
    {
        const int k = (int) b.pos;
        const float t = b.pos - (float) k;
        out = binPow[(size_t) k] * (1.0f - t) + binPow[(size_t) k + 1] * t;
        return;
    }
    float s = 0.0f;
    for (int k = b.k0; k <= b.k1; ++k)
        s += binPow[(size_t) k];
    out = s / (float) (b.k1 - b.k0 + 1);
}

SpectrumEngine::SpectrumEngine() : juce::Thread ("RefLayers FFT")
{
    timeBuf.assign (longSize, 0.0f);
    hopBuf.assign (hopSize, 0.0f);
    fifoBuf.assign (fifoSize, 0.0f);

    const float lo = std::log2 (blendLoHz), hi = std::log2 (blendHiHz);
    for (int i = 0; i < kGridSize; ++i)
        shortWeight[(size_t) i] = juce::jlimit (0.0f, 1.0f, (std::log2 (gridFreq (i)) - lo) / (hi - lo));

    startThread (juce::Thread::Priority::normal);
}

SpectrumEngine::~SpectrumEngine()
{
    stopThread (2000);
}

void SpectrumEngine::prepare (double sr)
{
    if (sr > 0.0 && sr != sampleRate.load())
    {
        sampleRate.store (sr);
        srChanged.store (true);
    }
}

void SpectrumEngine::push (const float* const* channels, int numChannels, int numSamples) noexcept
{
    if (numChannels <= 0 || numSamples <= 0)
        return;

    const auto scope = fifo.write (numSamples);
    const float gain = 1.0f / (float) numChannels;

    auto writeBlock = [&] (int dest, int count, int srcOffset)
    {
        for (int i = 0; i < count; ++i)
        {
            float s = 0.0f;
            for (int c = 0; c < numChannels; ++c)
                s += channels[c][srcOffset + i];
            fifoBuf[(size_t) (dest + i)] = s * gain;
        }
    };

    writeBlock (scope.startIndex1, scope.blockSize1, 0);
    writeBlock (scope.startIndex2, scope.blockSize2, scope.blockSize1);
}

void SpectrumEngine::rebuildMapping()
{
    const double sr = sampleRate.load();
    longRes.rebuild (sr);
    shortRes.rebuild (sr);
    liveAlpha = 1.0f - std::exp (-(float) hopSize / ((float) sr * liveAvgTau));
}

void SpectrumEngine::run()
{
    while (! threadShouldExit())
    {
        if (srChanged.exchange (false))
            rebuildMapping();

        bool didWork = false;
        while (fifo.getNumReady() >= hopSize)
        {
            {
                const auto scope = fifo.read (hopSize);
                std::copy_n (fifoBuf.begin() + scope.startIndex1, scope.blockSize1, hopBuf.begin());
                std::copy_n (fifoBuf.begin() + scope.startIndex2, scope.blockSize2, hopBuf.begin() + scope.blockSize1);
            }

            std::move (timeBuf.begin() + hopSize, timeBuf.end(), timeBuf.begin());
            std::copy (hopBuf.begin(), hopBuf.end(), timeBuf.end() - hopSize);

            processFrame();
            didWork = true;
        }

        if (! didWork)
            wait (3);
    }
}

void SpectrumEngine::processFrame()
{
    double energy = 0.0;
    for (auto v : timeBuf)
        energy += (double) v * v;
    const bool aboveGate = powToDb ((float) (energy / longSize)) > gateDb;

    longRes.computeBins (timeBuf.data());
    shortRes.computeBins (timeBuf.data() + (longSize - shortRes.size));

    for (int i = 0; i < kGridSize; ++i)
    {
        const float w = shortWeight[(size_t) i];
        float pl = 0.0f, ps = 0.0f;
        if (w < 1.0f) longRes.map (i, pl);
        if (w > 0.0f) shortRes.map (i, ps);
        gridPow[(size_t) i] = pl * (1.0f - w) + ps * w;
    }

    const juce::SpinLock::ScopedLockType sl (lock);

    for (int i = 0; i < kGridSize; ++i)
    {
        const float p = gridPow[(size_t) i];
        liveAvgPow[(size_t) i] += liveAlpha * (p - liveAvgPow[(size_t) i]);
        livePeakPow[(size_t) i] = std::max (livePeakPow[(size_t) i], p);
    }

    if (capturing.load())
    {
        ++capTotal;
        for (int i = 0; i < kGridSize; ++i)
            capPeakPow[(size_t) i] = std::max (capPeakPow[(size_t) i], gridPow[(size_t) i]);

        if (aboveGate)
        {
            ++capGated;
            for (int i = 0; i < kGridSize; ++i)
                capSumPow[(size_t) i] += gridPow[(size_t) i];
        }
    }
}

void SpectrumEngine::getLive (Curve& avgDb, Curve& peakDb)
{
    const juce::SpinLock::ScopedLockType sl (lock);
    for (int i = 0; i < kGridSize; ++i)
    {
        avgDb[(size_t) i] = powToDb (liveAvgPow[(size_t) i]);
        peakDb[(size_t) i] = powToDb (livePeakPow[(size_t) i]);
    }
    livePeakPow.fill (0.0f);
}

void SpectrumEngine::startCapture()
{
    const juce::SpinLock::ScopedLockType sl (lock);
    capPeakPow.fill (0.0f);
    capSumPow.fill (0.0f);
    capTotal = capGated = 0;
    capturing.store (true);
}

void SpectrumEngine::stopCapture()
{
    capturing.store (false);
}

SpectrumEngine::CaptureResult SpectrumEngine::getCapture()
{
    CaptureResult r;
    const juce::SpinLock::ScopedLockType sl (lock);
    r.totalFrames = capTotal;
    r.gatedFrames = capGated;
    r.seconds = capTotal * (double) hopSize / sampleRate.load();
    for (int i = 0; i < kGridSize; ++i)
    {
        r.peakDb[(size_t) i] = powToDb (capPeakPow[(size_t) i]);
        r.avgDb[(size_t) i] = capGated > 0 ? powToDb (capSumPow[(size_t) i] / (float) capGated) : kFloorDb;
    }
    return r;
}
}
