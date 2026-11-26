#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Grid.h"

namespace rl
{
struct Layer
{
    juce::String id;
    juce::String name;
    juce::Colour colour;
    bool visible = true;
    bool hasData = false;
    Curve peakDb {}, avgDb {};
    double seconds = 0.0;
    juce::String capturedAt;
};

// Shared by all plugin instances in the DAW process and persisted to
// %APPDATA%/RefLayers/layers.json, so layers survive moving the plugin between tracks.
class LayerStore : public juce::ChangeBroadcaster
{
public:
    LayerStore();

    std::vector<Layer> layers;

    juce::String addLayer (const juce::String& name);
    void removeLayer (const juce::String& id);
    Layer* find (const juce::String& id);
    void changed();

    bool exportTo (const juce::File& f) const;
    bool importFrom (const juce::File& f);

    static juce::Colour paletteColour (int index);
    static juce::File libraryFile();

private:
    juce::var toVar (const std::vector<Layer>& src) const;
    static std::vector<Layer> fromVar (const juce::var& v);
    void load();
    void save() const;
};
}
