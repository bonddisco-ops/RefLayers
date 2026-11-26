#pragma once

#include <juce_dsp/juce_dsp.h>
#include "Grid.h"

namespace rl
{
// Audio thread pushes samples into a lock-free FIFO; a worker thread runs every hop
// through the FFT, so no frame is skipped and peak capture is a true max-hold.
class SpectrumEngine : private juce::Thread
{
public:
    SpectrumEngine();
    ~SpectrumEngine() override;

    void prepare (double sampleRate);
    void push (const float* const* channels, int numChannels, int numSamples) noexcept;

    // Message thread. Peak = max of all frames since previous call.
    void getLive (Curve& avgDb, Curve& peakDb);

    void startCapture();
    void stopCapture();
    bool isCapturing() const noexcept { return capturing.load(); }

    struct CaptureResult
    {
        Curve peakDb {}, avgDb {};
        int totalFrames = 0, gatedFrames = 0;
        double seconds = 0.0;
    };
    CaptureResult getCapture();

private:
    void run() override;
    void processFrame();
    void rebuildMapping();

    struct BandMap { int k0 = 0, k1 = 0; float pos = 0.0f; bool interp = false; };

    // Long window resolves the low end, short window keeps transients (kick/clap attacks) sharp.
    struct Resolution
    {
        explicit Resolution (int order);
        void computeBins (const float* newestSamples);
        void map (int gridIndex, float& outPow) const;
        void rebuild (double sampleRate);

        int size;
        juce::dsp::FFT fft;
        std::vector<float> window, buf, binPow;
        std::array<BandMap, kGridSize> bands;
    };

    static constexpr int longOrder = 13;
    static constexpr int longSize = 1 << longOrder;
    static constexpr int shortOrder = 11;
    static constexpr int hopSize = 512;
    static constexpr int fifoSize = 1 << 17;
    static constexpr float gateDb = -70.0f;
    static constexpr float liveAvgTau = 0.3f;
    static constexpr float blendLoHz = 250.0f, blendHiHz = 500.0f;

    Resolution longRes { longOrder }, shortRes { shortOrder };
    std::vector<float> timeBuf, hopBuf;
    std::array<float, kGridSize> shortWeight {};

    juce::AbstractFifo fifo { fifoSize };
    std::vector<float> fifoBuf;

    std::atomic<double> sampleRate { 48000.0 };
    std::atomic<bool> srChanged { true };
    float liveAlpha = 0.05f;

    Curve gridPow {};

    juce::SpinLock lock;
    Curve liveAvgPow {}, livePeakPow {};
    std::atomic<bool> capturing { false };
    Curve capPeakPow {}, capSumPow {};
    int capTotal = 0, capGated = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumEngine)
};
}
