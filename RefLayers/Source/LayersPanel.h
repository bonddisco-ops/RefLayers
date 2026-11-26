#pragma once

#include "LayerStore.h"

namespace rl
{
class LayersPanel : public juce::Component
{
public:
    explicit LayersPanel (LayerStore& store);

    std::function<void (const juce::String& id)> onSelect;

    void setSelected (const juce::String& id);
    void setRecordingId (const juce::String& id);
    void refresh();
    void addLayerAndRename();

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    class Rows : public juce::Component
    {
    public:
        explicit Rows (LayersPanel& o);
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDoubleClick (const juce::MouseEvent& e) override;
        void startRename (const juce::String& id);

        static constexpr int rowH = 38;

    private:
        int rowAt (int y) const;
        void commitRename();

        LayersPanel& owner;
        juce::TextEditor nameEditor;
        juce::String renamingId;
    };

    LayerStore& store;
    juce::String selectedId, recordingId;

    juce::TextButton addButton { "+" }, exportButton { "Export" }, importButton { "Import" };
    juce::Viewport viewport;
    Rows rows { *this };
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LayersPanel)
};
}
