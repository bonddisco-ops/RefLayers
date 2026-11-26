#include "LayersPanel.h"

namespace rl
{
static const juce::Colour kPanelBg (0xff161a20);
static const juce::Colour kRowSel (0xff242b35);
static const juce::Colour kRowText (0xffd8dee6);
static const juce::Colour kDim (0xff6f7a88);

static juce::Rectangle<int> swatchArea (juce::Rectangle<int> row) { return row.withWidth (16).reduced (4, 8).translated (4, 0); }
static juce::Rectangle<int> deleteArea (juce::Rectangle<int> row) { return row.removeFromRight (24); }
static juce::Rectangle<int> eyeArea (juce::Rectangle<int> row) { return row.withTrimmedRight (24).removeFromRight (26); }
static juce::Rectangle<int> nameArea (juce::Rectangle<int> row) { return row.withTrimmedLeft (26).withTrimmedRight (52).withY (row.getY() + 4).withHeight (18); }
static juce::Rectangle<int> statusArea (juce::Rectangle<int> row) { return nameArea (row).translated (0, 17).withHeight (13); }

LayersPanel::LayersPanel (LayerStore& s) : store (s)
{
    addButton.setTooltip ("Add layer");
    addButton.onClick = [this] { addLayerAndRename(); };

    exportButton.setTooltip ("Save all layers to a .reflayers file");
    exportButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Export layers", juce::File(), "*.reflayers");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
                              {
                                  auto f = fc.getResult();
                                  if (f != juce::File())
                                      store.exportTo (f.withFileExtension ("reflayers"));
                              });
    };

    importButton.setTooltip ("Add layers from a .reflayers file");
    importButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Import layers", juce::File(), "*.reflayers;*.json");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  auto f = fc.getResult();
                                  if (f.existsAsFile())
                                      store.importFrom (f);
                              });
    };

    for (auto* b : { &addButton, &exportButton, &importButton })
        addAndMakeVisible (b);

    viewport.setViewedComponent (&rows, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (6);
    addAndMakeVisible (viewport);
}

void LayersPanel::paint (juce::Graphics& g)
{
    g.fillAll (kPanelBg);
    g.setColour (kRowText);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText ("LAYERS", getLocalBounds().removeFromTop (34).withTrimmedLeft (12), juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xff232932));
    g.drawVerticalLine (getWidth() - 1, 0.0f, (float) getHeight());
}

void LayersPanel::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (34);
    addButton.setBounds (top.removeFromRight (40).reduced (6, 5));
    auto bottom = r.removeFromBottom (36).reduced (8, 6);
    exportButton.setBounds (bottom.removeFromLeft (bottom.getWidth() / 2).withTrimmedRight (3));
    importButton.setBounds (bottom.withTrimmedLeft (3));
    viewport.setBounds (r);
    refresh();
}

void LayersPanel::refresh()
{
    rows.setSize (viewport.getMaximumVisibleWidth(),
                  std::max (viewport.getHeight(), (int) store.layers.size() * Rows::rowH));
    rows.repaint();
}

void LayersPanel::setSelected (const juce::String& id)
{
    selectedId = id;
    rows.repaint();
}

void LayersPanel::setRecordingId (const juce::String& id)
{
    recordingId = id;
    rows.repaint();
}

void LayersPanel::addLayerAndRename()
{
    const auto id = store.addLayer ("Layer " + juce::String ((int) store.layers.size() + 1));
    refresh();
    if (onSelect)
        onSelect (id);
    viewport.setViewPosition (0, rows.getHeight());
    rows.startRename (id);
}

LayersPanel::Rows::Rows (LayersPanel& o) : owner (o)
{
    nameEditor.setFont (juce::FontOptions (13.0f));
    nameEditor.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0e1115));
    nameEditor.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff42a5ff));
    nameEditor.setColour (juce::TextEditor::textColourId, kRowText);
    nameEditor.onReturnKey = [this] { commitRename(); };
    nameEditor.onFocusLost = [this] { commitRename(); };
    nameEditor.onEscapeKey = [this] { renamingId.clear(); nameEditor.setVisible (false); };
    addChildComponent (nameEditor);
}

int LayersPanel::Rows::rowAt (int y) const
{
    const int i = y / rowH;
    return (i >= 0 && i < (int) owner.store.layers.size()) ? i : -1;
}

void LayersPanel::Rows::paint (juce::Graphics& g)
{
    const auto& layers = owner.store.layers;
    const bool blink = (juce::Time::getMillisecondCounter() / 400) % 2 == 0;

    for (int i = 0; i < (int) layers.size(); ++i)
    {
        const auto& l = layers[(size_t) i];
        auto row = juce::Rectangle<int> (0, i * rowH, getWidth(), rowH);

        if (l.id == owner.selectedId)
        {
            g.setColour (kRowSel);
            g.fillRect (row.reduced (4, 2));
        }

        g.setColour (l.visible ? l.colour : l.colour.withAlpha (0.3f));
        g.fillRoundedRectangle (swatchArea (row).toFloat(), 3.0f);

        g.setColour (l.visible ? kRowText : kDim);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (l.name, nameArea (row), juce::Justification::centredLeft, true);

        juce::String status;
        if (l.id == owner.recordingId)
            status = "recording...";
        else if (l.hasData)
            status = "peak + avg   " + juce::String (l.seconds, 1) + " s";
        else
            status = "empty - press REC";

        g.setFont (juce::FontOptions (10.5f));
        g.setColour (l.id == owner.recordingId ? juce::Colour (0xffff5c5c) : kDim);
        g.drawText (status, statusArea (row), juce::Justification::centredLeft, true);

        if (l.id == owner.recordingId && blink)
        {
            g.setColour (juce::Colour (0xffff3b3b));
            g.fillEllipse (eyeArea (row).toFloat().withSizeKeepingCentre (8.0f, 8.0f).translated (-26.0f, 0.0f));
        }

        auto eye = eyeArea (row).toFloat().withSizeKeepingCentre (16.0f, 10.0f);
        g.setColour (l.visible ? kRowText : kDim);
        g.drawEllipse (eye, 1.3f);
        if (l.visible)
            g.fillEllipse (eye.withSizeKeepingCentre (4.0f, 4.0f));
        else
            g.drawLine (eye.getX(), eye.getBottom() + 1.0f, eye.getRight(), eye.getY() - 1.0f, 1.3f);

        auto del = deleteArea (row).toFloat().withSizeKeepingCentre (8.0f, 8.0f);
        g.setColour (kDim);
        g.drawLine (del.getX(), del.getY(), del.getRight(), del.getBottom(), 1.3f);
        g.drawLine (del.getRight(), del.getY(), del.getX(), del.getBottom(), 1.3f);
    }

    if (layers.empty())
    {
        g.setColour (kDim);
        g.setFont (juce::FontOptions (12.0f));
        g.drawFittedText ("Press + to add a layer,\nthen REC to capture it", getLocalBounds().reduced (12).removeFromTop (60),
                          juce::Justification::centred, 3);
    }
}

void LayersPanel::Rows::mouseDown (const juce::MouseEvent& e)
{
    const int i = rowAt (e.y);
    if (i < 0)
        return;

    auto& l = owner.store.layers[(size_t) i];
    const auto row = juce::Rectangle<int> (0, i * rowH, getWidth(), rowH);
    const auto id = l.id;

    if (deleteArea (row).contains (e.getPosition()))
    {
        if (id == owner.recordingId)
            return;

        auto opts = juce::MessageBoxOptions()
                        .withIconType (juce::MessageBoxIconType::QuestionIcon)
                        .withTitle ("Delete layer")
                        .withMessage ("Delete \"" + l.name + "\"?")
                        .withButton ("Delete")
                        .withButton ("Cancel")
                        .withAssociatedComponent (this);
        juce::Component::SafePointer<Rows> safe (this);
        juce::AlertWindow::showAsync (opts, [safe, id] (int result)
        {
            if (result != 1 || safe == nullptr)
                return;
            safe->owner.store.removeLayer (id);
            safe->owner.refresh();
        });
        return;
    }

    if (eyeArea (row).contains (e.getPosition()))
    {
        l.visible = ! l.visible;
        owner.store.changed();
        return;
    }

    if (swatchArea (row).expanded (4, 8).contains (e.getPosition()))
    {
        int idx = 0;
        for (int k = 0; k < 10; ++k)
            if (LayerStore::paletteColour (k) == l.colour)
                idx = k + 1;
        l.colour = LayerStore::paletteColour (idx);
        owner.store.changed();
    }

    if (owner.onSelect)
        owner.onSelect (id);
}

void LayersPanel::Rows::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int i = rowAt (e.y);
    if (i < 0)
        return;
    const auto row = juce::Rectangle<int> (0, i * rowH, getWidth(), rowH);
    if (row.withTrimmedLeft (22).withTrimmedRight (52).contains (e.getPosition()))
        startRename (owner.store.layers[(size_t) i].id);
}

void LayersPanel::Rows::startRename (const juce::String& id)
{
    for (int i = 0; i < (int) owner.store.layers.size(); ++i)
    {
        if (owner.store.layers[(size_t) i].id != id)
            continue;

        renamingId = id;
        const auto row = juce::Rectangle<int> (0, i * rowH, getWidth(), rowH);
        nameEditor.setBounds (nameArea (row).expanded (0, 2).translated (-4, 0));
        nameEditor.setText (owner.store.layers[(size_t) i].name, false);
        nameEditor.setVisible (true);
        nameEditor.grabKeyboardFocus();
        nameEditor.selectAll();
        return;
    }
}

void LayersPanel::Rows::commitRename()
{
    if (renamingId.isEmpty())
        return;

    const auto id = renamingId;
    renamingId.clear();
    nameEditor.setVisible (false);

    const auto text = nameEditor.getText().trim();
    if (auto* l = owner.store.find (id); l != nullptr && text.isNotEmpty() && text != l->name)
    {
        l->name = text;
        owner.store.changed();
    }
}
}
