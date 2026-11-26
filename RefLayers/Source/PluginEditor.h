#pragma once

#include <juce_opengl/juce_opengl.h>
#include "PluginProcessor.h"
#include "LayersPanel.h"
#include "SpectrumView.h"

class RefLayersEditor : public juce::AudioProcessorEditor,
                        private juce::Timer,
                        private juce::ChangeListener
{
public:
    explicit RefLayersEditor (RefLayersProcessor&);
    ~RefLayersEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    void selectLayer (const juce::String& id);
    void toggleRecord();
    void setMode (rl::ViewMode m);
    void applySettings();
    void rebuildRefs();

    RefLayersProcessor& proc;
    juce::SharedResourcePointer<rl::LayerStore> store;
    juce::OpenGLContext gl;
    juce::LookAndFeel_V4 laf;
    juce::TooltipWindow tooltips { this, 600 };

    rl::LayersPanel panel { *store };
    rl::SpectrumView view;

    juce::TextButton recButton { "REC" };
    juce::TextButton peakButton { "PEAK" }, avgButton { "AVG" }, bothButton { "BOTH" };
    juce::TextButton liveButton { "LIVE" };
    juce::ComboBox smoothBox, tiltBox, rangeBox;
    juce::Slider offsetSlider;
    juce::Label offsetLabel, statusLabel;

    juce::String selectedId, recordingId;
    rl::SpectrumEngine::CaptureResult preview;
    rl::Curve livePeakHold {};
    double lastTick = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RefLayersEditor)
};
