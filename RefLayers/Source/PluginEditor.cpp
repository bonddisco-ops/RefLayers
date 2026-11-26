#include "PluginEditor.h"

namespace
{
const juce::Colour kTopBg (0xff13171c);
const juce::Colour kAccent (0xff42a5ff);
const juce::Colour kRec (0xffe23b3b);

const int kSmoothWidths[] = { 0, 2, 4, 8 };
const float kTilts[] = { 0.0f, 3.0f, 4.5f };
const float kRanges[] = { 60.0f, 80.0f, 100.0f, 120.0f };
}

RefLayersEditor::RefLayersEditor (RefLayersProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    laf.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1f252d));
    laf.setColour (juce::TextButton::buttonOnColourId, kAccent.darker (0.3f));
    laf.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffc8d0da));
    laf.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    laf.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff1f252d));
    laf.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff2c343e));
    laf.setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1a1f26));
    setLookAndFeel (&laf);
    livePeakHold.fill (rl::kFloorDb);

    addAndMakeVisible (panel);
    addAndMakeVisible (view);
    panel.onSelect = [this] (const juce::String& id) { selectLayer (id); };

    recButton.setColour (juce::TextButton::buttonOnColourId, kRec);
    recButton.setTooltip ("Start/stop capturing the selected layer. Peak = max-hold over the whole capture, Avg = energy average of non-silent frames");
    recButton.onClick = [this] { toggleRecord(); };
    addAndMakeVisible (recButton);

    int idx = 0;
    for (auto* b : { &peakButton, &avgButton, &bothButton })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (1001);
        b->setConnectedEdges ((idx > 0 ? juce::Button::ConnectedOnLeft : 0) | (idx < 2 ? juce::Button::ConnectedOnRight : 0));
        const auto mode = (rl::ViewMode) idx++;
        b->onClick = [this, b, mode] { if (b->getToggleState()) setMode (mode); };
        addAndMakeVisible (b);
    }

    liveButton.setClickingTogglesState (true);
    liveButton.setTooltip ("Show the live spectrum of this track");
    liveButton.onClick = [this] { applySettings(); };
    addAndMakeVisible (liveButton);

    smoothBox.addItemList ({ "1/24 oct", "1/12 oct", "1/6 oct", "1/3 oct" }, 1);
    tiltBox.addItemList ({ "Tilt 0", "Tilt 3 dB", "Tilt 4.5 dB" }, 1);
    rangeBox.addItemList ({ "60 dB", "80 dB", "100 dB", "120 dB" }, 1);
    smoothBox.setTooltip ("Display smoothing (applied to live and reference curves equally)");
    tiltBox.setTooltip ("Slope in dB/octave around 1 kHz");
    for (auto* c : { &smoothBox, &tiltBox, &rangeBox })
    {
        c->onChange = [this] { applySettings(); };
        addAndMakeVisible (c);
    }

    offsetSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    offsetSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
    offsetSlider.setRange (-30.0, 30.0, 0.1);
    offsetSlider.setDoubleClickReturnValue (true, 0.0);
    offsetSlider.setTextValueSuffix (" dB");
    offsetSlider.setTooltip ("Shift all reference curves up/down (e.g. to compensate a louder reference master). Double-click = 0");
    offsetSlider.onValueChange = [this] { applySettings(); };
    addAndMakeVisible (offsetSlider);

    offsetLabel.setText ("Ref offset", juce::dontSendNotification);
    offsetLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8a96a5));
    addAndMakeVisible (offsetLabel);

    statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8a96a5));
    statusLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (statusLabel);

    view.onWheelShift = [this] (float d)
    {
        proc.uiState.setProperty ("top", juce::jlimit (-60.0f, 30.0f, view.settings.topDb + d), nullptr);
        applySettings();
    };

    auto& s = proc.uiState;
    const int mode = juce::jlimit (0, 2, (int) s.getProperty ("mode", 2));
    (mode == 0 ? peakButton : mode == 1 ? avgButton : bothButton).setToggleState (true, juce::dontSendNotification);
    liveButton.setToggleState ((bool) s.getProperty ("live", true), juce::dontSendNotification);
    smoothBox.setSelectedId ((int) s.getProperty ("smooth", 3), juce::dontSendNotification);
    tiltBox.setSelectedId ((int) s.getProperty ("tilt", 2), juce::dontSendNotification);
    rangeBox.setSelectedId ((int) s.getProperty ("range", 2), juce::dontSendNotification);
    offsetSlider.setValue ((double) s.getProperty ("offset", 0.0), juce::dontSendNotification);

    store->addChangeListener (this);

    selectedId = s.getProperty ("selected").toString();
    if (store->find (selectedId) == nullptr)
        selectedId = store->layers.empty() ? juce::String() : store->layers.front().id;
    panel.setSelected (selectedId);

    applySettings();
    rebuildRefs();

    gl.setComponentPaintingEnabled (true);
    gl.attachTo (*this);

    setResizable (true, true);
    setResizeLimits (900, 420, 2400, 1400);
    setSize ((int) s.getProperty ("w", 1160), (int) s.getProperty ("h", 600));

    lastTick = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (60);
}

RefLayersEditor::~RefLayersEditor()
{
    stopTimer();
    if (recordingId.isNotEmpty())
        toggleRecord();
    store->removeChangeListener (this);
    gl.detach();
    setLookAndFeel (nullptr);
}

void RefLayersEditor::paint (juce::Graphics& g)
{
    g.fillAll (kTopBg);
}

void RefLayersEditor::resized()
{
    auto r = getLocalBounds();
    panel.setBounds (r.removeFromLeft (240));

    auto top = r.removeFromTop (40).reduced (8, 7);
    recButton.setBounds (top.removeFromLeft (64));
    top.removeFromLeft (12);
    peakButton.setBounds (top.removeFromLeft (52));
    avgButton.setBounds (top.removeFromLeft (52));
    bothButton.setBounds (top.removeFromLeft (52));
    top.removeFromLeft (8);
    liveButton.setBounds (top.removeFromLeft (52));
    top.removeFromLeft (12);
    smoothBox.setBounds (top.removeFromLeft (86));
    top.removeFromLeft (6);
    tiltBox.setBounds (top.removeFromLeft (96));
    top.removeFromLeft (6);
    rangeBox.setBounds (top.removeFromLeft (76));
    top.removeFromLeft (10);
    offsetLabel.setBounds (top.removeFromLeft (64));
    offsetSlider.setBounds (top.removeFromLeft (170));
    statusLabel.setBounds (top);

    view.setBounds (r);

    proc.uiState.setProperty ("w", getWidth(), nullptr);
    proc.uiState.setProperty ("h", getHeight(), nullptr);
}

void RefLayersEditor::setMode (rl::ViewMode m)
{
    proc.uiState.setProperty ("mode", (int) m, nullptr);
    applySettings();
}

void RefLayersEditor::applySettings()
{
    auto& s = proc.uiState;
    s.setProperty ("live", liveButton.getToggleState(), nullptr);
    s.setProperty ("smooth", smoothBox.getSelectedId(), nullptr);
    s.setProperty ("tilt", tiltBox.getSelectedId(), nullptr);
    s.setProperty ("range", rangeBox.getSelectedId(), nullptr);
    s.setProperty ("offset", offsetSlider.getValue(), nullptr);

    auto& vs = view.settings;
    vs.mode = (rl::ViewMode) juce::jlimit (0, 2, (int) s.getProperty ("mode", 2));
    vs.showLive = liveButton.getToggleState();
    vs.smoothHalfWidth = kSmoothWidths[juce::jlimit (0, 3, smoothBox.getSelectedId() - 1)];
    vs.tiltDbPerOct = kTilts[juce::jlimit (0, 2, tiltBox.getSelectedId() - 1)];
    vs.rangeDb = kRanges[juce::jlimit (0, 3, rangeBox.getSelectedId() - 1)];
    vs.topDb = (float) s.getProperty ("top", 0.0f);
    vs.refOffsetDb = (float) offsetSlider.getValue();
    view.repaint();
}

void RefLayersEditor::selectLayer (const juce::String& id)
{
    selectedId = id;
    proc.uiState.setProperty ("selected", id, nullptr);
    panel.setSelected (id);
    rebuildRefs();
}

void RefLayersEditor::toggleRecord()
{
    auto& engine = proc.engine;

    if (recordingId.isEmpty())
    {
        if (store->find (selectedId) == nullptr)
            panel.addLayerAndRename();
        if (store->find (selectedId) == nullptr)
            return;

        recordingId = selectedId;
        engine.startCapture();
        preview = engine.getCapture();
        recButton.setToggleState (true, juce::dontSendNotification);
        panel.setRecordingId (recordingId);
        rebuildRefs();
        return;
    }

    engine.stopCapture();
    const auto result = engine.getCapture();
    const auto id = recordingId;
    recordingId.clear();
    recButton.setToggleState (false, juce::dontSendNotification);
    panel.setRecordingId ({});

    auto* layer = store->find (id);
    if (layer == nullptr)
        return;

    if (result.totalFrames < 4)
    {
        statusLabel.setText ("Capture too short - no audio reached the plugin", juce::dontSendNotification);
        rebuildRefs();
        return;
    }

    layer->peakDb = result.peakDb;
    layer->avgDb = result.avgDb;
    layer->seconds = result.seconds;
    layer->hasData = true;
    layer->capturedAt = juce::Time::getCurrentTime().toISO8601 (true);
    store->changed();

    statusLabel.setText (result.gatedFrames == 0 ? "Saved, but signal was below -70 dBFS: avg is empty"
                                                 : "Saved \"" + layer->name + "\"  " + juce::String (result.seconds, 1) + " s",
                         juce::dontSendNotification);
}

void RefLayersEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (store->find (selectedId) == nullptr)
        selectedId = store->layers.empty() ? juce::String() : store->layers.back().id;
    panel.setSelected (selectedId);
    panel.refresh();
    rebuildRefs();
}

void RefLayersEditor::rebuildRefs()
{
    view.refs.clear();
    for (const auto& l : store->layers)
    {
        const bool rec = l.id == recordingId;
        if (! l.visible || (! l.hasData && ! rec))
            continue;

        rl::SpectrumView::RefCurve rc;
        rc.name = l.name;
        rc.colour = l.colour;
        rc.selected = l.id == selectedId;
        rc.recording = rec;
        rc.peak = rec ? preview.peakDb : l.peakDb;
        rc.avg = rec ? preview.avgDb : l.avgDb;
        view.refs.push_back (rc);
    }
}

void RefLayersEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = (float) juce::jlimit (0.0, 0.1, (now - lastTick) * 0.001);
    lastTick = now;

    rl::Curve peakNow;
    proc.engine.getLive (view.liveAvg, peakNow);

    constexpr float peakDecayDbPerSec = 18.0f;
    for (int i = 0; i < rl::kGridSize; ++i)
        livePeakHold[(size_t) i] = std::max (peakNow[(size_t) i], livePeakHold[(size_t) i] - peakDecayDbPerSec * dt);
    view.livePeak = livePeakHold;

    if (recordingId.isNotEmpty())
    {
        preview = proc.engine.getCapture();
        rebuildRefs();
        statusLabel.setText ("REC  " + juce::String (preview.seconds, 1) + " s", juce::dontSendNotification);
        panel.repaint();
    }

    view.repaint();
}
