#include "LayerStore.h"

namespace rl
{
static const juce::uint32 kPalette[] = {
    0xffff5c5c, 0xffffa53d, 0xfff2d544, 0xff7ddc4a, 0xff3fd6b0,
    0xff42a5ff, 0xff8f7bff, 0xffe06bff, 0xffff6fae, 0xffb0b8c4
};

juce::Colour LayerStore::paletteColour (int index)
{
    const int n = (int) (sizeof (kPalette) / sizeof (kPalette[0]));
    return juce::Colour (kPalette[((index % n) + n) % n]);
}

juce::File LayerStore::libraryFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("RefLayers")
        .getChildFile ("layers.json");
}

LayerStore::LayerStore()
{
    load();
}

juce::String LayerStore::addLayer (const juce::String& name)
{
    Layer l;
    l.id = juce::Uuid().toString();
    l.name = name;
    l.colour = paletteColour ((int) layers.size());
    l.peakDb.fill (kFloorDb);
    l.avgDb.fill (kFloorDb);
    layers.push_back (l);
    changed();
    return l.id;
}

void LayerStore::removeLayer (const juce::String& id)
{
    layers.erase (std::remove_if (layers.begin(), layers.end(),
                                  [&] (const Layer& l) { return l.id == id; }),
                  layers.end());
    changed();
}

Layer* LayerStore::find (const juce::String& id)
{
    for (auto& l : layers)
        if (l.id == id)
            return &l;
    return nullptr;
}

void LayerStore::changed()
{
    save();
    sendChangeMessage();
}

static juce::var curveToVar (const Curve& c)
{
    juce::Array<juce::var> arr;
    arr.ensureStorageAllocated (kGridSize);
    for (auto v : c)
        arr.add (std::round (v * 100.0f) / 100.0f);
    return arr;
}

static Curve curveFromVar (const juce::var& v)
{
    Curve c;
    c.fill (kFloorDb);
    if (auto* arr = v.getArray())
        for (int i = 0; i < std::min (kGridSize, arr->size()); ++i)
            c[(size_t) i] = (float) (double) (*arr)[i];
    return c;
}

juce::var LayerStore::toVar (const std::vector<Layer>& src) const
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("format", "RefLayers");
    root->setProperty ("version", 1);
    root->setProperty ("gridSize", kGridSize);
    root->setProperty ("minHz", kMinHz);
    root->setProperty ("maxHz", kMaxHz);

    juce::Array<juce::var> arr;
    for (const auto& l : src)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("id", l.id);
        o->setProperty ("name", l.name);
        o->setProperty ("colour", l.colour.toString());
        o->setProperty ("visible", l.visible);
        o->setProperty ("hasData", l.hasData);
        o->setProperty ("seconds", l.seconds);
        o->setProperty ("capturedAt", l.capturedAt);
        if (l.hasData)
        {
            o->setProperty ("peakDb", curveToVar (l.peakDb));
            o->setProperty ("avgDb", curveToVar (l.avgDb));
        }
        arr.add (juce::var (o));
    }
    root->setProperty ("layers", arr);
    return juce::var (root);
}

std::vector<Layer> LayerStore::fromVar (const juce::var& v)
{
    std::vector<Layer> out;
    if (auto* arr = v["layers"].getArray())
    {
        for (const auto& o : *arr)
        {
            Layer l;
            l.id = o["id"].toString();
            if (l.id.isEmpty())
                l.id = juce::Uuid().toString();
            l.name = o["name"].toString();
            l.colour = juce::Colour::fromString (o["colour"].toString());
            l.visible = (bool) o.getProperty ("visible", true);
            l.hasData = (bool) o.getProperty ("hasData", false);
            l.seconds = (double) o.getProperty ("seconds", 0.0);
            l.capturedAt = o["capturedAt"].toString();
            l.peakDb = curveFromVar (o["peakDb"]);
            l.avgDb = curveFromVar (o["avgDb"]);
            out.push_back (l);
        }
    }
    return out;
}

void LayerStore::load()
{
    const auto f = libraryFile();
    if (f.existsAsFile())
        layers = fromVar (juce::JSON::parse (f));
}

void LayerStore::save() const
{
    const auto f = libraryFile();
    f.getParentDirectory().createDirectory();
    f.replaceWithText (juce::JSON::toString (toVar (layers), true));
}

bool LayerStore::exportTo (const juce::File& f) const
{
    return f.replaceWithText (juce::JSON::toString (toVar (layers), true));
}

bool LayerStore::importFrom (const juce::File& f)
{
    const auto parsed = juce::JSON::parse (f);
    if (parsed["format"].toString() != "RefLayers")
        return false;

    for (auto& l : fromVar (parsed))
    {
        if (find (l.id) != nullptr)
            l.id = juce::Uuid().toString();
        layers.push_back (l);
    }
    changed();
    return true;
}
}
