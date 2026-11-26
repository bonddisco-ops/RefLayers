#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Grid.h"

namespace rl
{
enum class ViewMode { peak = 0, avg = 1, both = 2 };

struct ViewSettings
{
    ViewMode mode = ViewMode::both;
    int smoothHalfWidth = 4; // grid points; 48 points = 1 octave
    float tiltDbPerOct = 3.0f;
    float topDb = 0.0f;
    float rangeDb = 80.0f;
    float refOffsetDb = 0.0f;
    bool showLive = true;
};

class SpectrumView : public juce::Component
{
public:
    struct RefCurve
    {
        juce::String name;
        juce::Colour colour;
        Curve peak {}, avg {};
        bool selected = false;
        bool recording = false;
    };

    ViewSettings settings;
    std::vector<RefCurve> refs;
    Curve liveAvg {}, livePeak {};

    std::function<void (float deltaDb)> onWheelShift;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;

private:
    void prepare (const Curve& in, float offsetDb, Curve& out) const;
    juce::Path makePath (const Curve& c) const;
    float xForIndex (int i) const;
    float yForDb (float db) const;
    float dbAtX (const Curve& c, float x) const;
    void drawGrid (juce::Graphics& g) const;
    void drawReadout (juce::Graphics& g);

    juce::Rectangle<float> plot;
    juce::Point<float> mouse;
    bool hovering = false;

    Curve prepLiveAvg {}, prepLivePeak {};
    mutable std::array<double, kGridSize + 1> prefix {};
    std::vector<Curve> prepPeak, prepAvg;
};
}
