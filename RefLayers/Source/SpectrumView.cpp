#include "SpectrumView.h"

namespace rl
{
static const juce::Colour kBg (0xff101317);
static const juce::Colour kGridMajor (0xff2a313a);
static const juce::Colour kGridMinor (0xff1b2027);
static const juce::Colour kText (0xff8a96a5);

void SpectrumView::resized()
{
    plot = getLocalBounds().toFloat().withTrimmedLeft (36.0f).withTrimmedBottom (18.0f).reduced (4.0f, 6.0f);
}

float SpectrumView::xForIndex (int i) const
{
    return plot.getX() + plot.getWidth() * (float) i / (float) (kGridSize - 1);
}

float SpectrumView::yForDb (float db) const
{
    const float t = (settings.topDb - db) / settings.rangeDb;
    return plot.getY() + plot.getHeight() * t;
}

void SpectrumView::prepare (const Curve& in, float offsetDb, Curve& out) const
{
    const int w = settings.smoothHalfWidth;
    auto& pre = prefix;

    if (w > 0)
    {
        pre[0] = 0.0;
        for (int i = 0; i < kGridSize; ++i)
            pre[(size_t) i + 1] = pre[(size_t) i] + (double) dbToPow (in[(size_t) i]);
    }

    for (int i = 0; i < kGridSize; ++i)
    {
        float db = in[(size_t) i];
        if (w > 0)
        {
            const int a = std::max (0, i - w), b = std::min (kGridSize - 1, i + w);
            db = powToDb ((float) ((pre[(size_t) b + 1] - pre[(size_t) a]) / (double) (b - a + 1)));
        }
        const float oct = std::log2 (gridFreq (i) / 1000.0f);
        out[(size_t) i] = db + settings.tiltDbPerOct * oct + offsetDb;
    }
}

juce::Path SpectrumView::makePath (const Curve& c) const
{
    juce::Path p;
    p.preallocateSpace (kGridSize * 3 + 8);
    const float bottom = plot.getBottom() + 2.0f;
    for (int i = 0; i < kGridSize; ++i)
    {
        const float x = xForIndex (i);
        const float y = std::min (bottom, yForDb (c[(size_t) i]));
        if (i == 0)
            p.startNewSubPath (x, y);
        else
            p.lineTo (x, y);
    }
    return p;
}

float SpectrumView::dbAtX (const Curve& c, float x) const
{
    const float pos = juce::jlimit (0.0f, (float) (kGridSize - 1),
                                    (x - plot.getX()) / plot.getWidth() * (float) (kGridSize - 1));
    const int i = std::min ((int) pos, kGridSize - 2);
    const float t = pos - (float) i;
    return c[(size_t) i] * (1.0f - t) + c[(size_t) i + 1] * t;
}

void SpectrumView::drawGrid (juce::Graphics& g) const
{
    g.setFont (juce::FontOptions (11.0f));

    for (int decade = 10; decade <= 10000; decade *= 10)
        for (int m = 1; m <= 9; ++m)
        {
            const float f = (float) (decade * m);
            if (f < kMinHz || f > kMaxHz)
                continue;
            const float x = plot.getX() + plot.getWidth() * freqToNorm (f);
            const bool major = (m == 1 || m == 2 || m == 5);
            g.setColour (major ? kGridMajor : kGridMinor);
            g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());

            if (major)
            {
                const auto label = f >= 1000.0f ? juce::String (f / 1000.0f, 0) + "k" : juce::String ((int) f);
                g.setColour (kText);
                g.drawText (label, juce::Rectangle<float> (x - 20.0f, plot.getBottom() + 3.0f, 40.0f, 14.0f),
                            juce::Justification::centred);
            }
        }

    const float step = settings.rangeDb > 90.0f ? 12.0f : 6.0f;
    const float first = std::floor (settings.topDb / step) * step;
    for (float db = first; db >= settings.topDb - settings.rangeDb; db -= step)
    {
        const float y = yForDb (db);
        g.setColour (std::fmod (std::abs (db), step * 2.0f) < 0.01f ? kGridMajor : kGridMinor);
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        g.setColour (kText);
        g.drawText (juce::String ((int) db), juce::Rectangle<float> (0.0f, y - 7.0f, 32.0f, 14.0f),
                    juce::Justification::centredRight);
    }
}

void SpectrumView::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
    drawGrid (g);

    g.saveState();
    g.reduceClipRegion (plot.toNearestInt());

    const bool showPeak = settings.mode != ViewMode::avg;
    const bool showAvg = settings.mode != ViewMode::peak;
    const juce::PathStrokeType thin (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    const float floorY = plot.getBottom() + 2.0f;

    prepPeak.resize (refs.size());
    prepAvg.resize (refs.size());

    for (size_t r = 0; r < refs.size(); ++r)
    {
        const auto& ref = refs[r];
        prepare (ref.peak, settings.refOffsetDb, prepPeak[r]);
        prepare (ref.avg, settings.refOffsetDb, prepAvg[r]);

        const float wAvg = ref.selected ? 2.6f : 1.8f;
        const float alpha = (ref.selected || ref.recording) ? 1.0f : 0.8f;
        const auto avgPath = makePath (prepAvg[r]);
        const auto peakPath = makePath (prepPeak[r]);

        if (settings.mode == ViewMode::both)
        {
            juce::Path band (peakPath);
            for (int i = kGridSize - 1; i >= 0; --i)
                band.lineTo (xForIndex (i), std::min (floorY, yForDb (prepAvg[r][(size_t) i])));
            band.closeSubPath();
            g.setColour (ref.colour.withAlpha (ref.selected ? 0.16f : 0.07f));
            g.fillPath (band);
        }
        else if (showAvg && ref.selected)
        {
            juce::Path fillP (avgPath);
            fillP.lineTo (plot.getRight(), floorY);
            fillP.lineTo (plot.getX(), floorY);
            fillP.closeSubPath();
            g.setColour (ref.colour.withAlpha (0.10f));
            g.fillPath (fillP);
        }

        if (showPeak)
        {
            g.setColour (ref.colour.withAlpha (alpha * 0.75f));
            g.strokePath (peakPath, thin);
        }
        if (showAvg)
        {
            g.setColour (ref.colour.withAlpha (alpha));
            g.strokePath (avgPath, juce::PathStrokeType (wAvg, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    if (settings.showLive)
    {
        prepare (liveAvg, 0.0f, prepLiveAvg);
        prepare (livePeak, 0.0f, prepLivePeak);

        if (showPeak)
        {
            g.setColour (juce::Colours::white.withAlpha (showAvg ? 0.40f : 0.90f));
            g.strokePath (makePath (prepLivePeak), thin);
        }
        if (showAvg)
        {
            const auto p = makePath (prepLiveAvg);
            juce::Path fillP (p);
            fillP.lineTo (plot.getRight(), floorY);
            fillP.lineTo (plot.getX(), floorY);
            fillP.closeSubPath();
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.fillPath (fillP);
            g.setColour (juce::Colours::white.withAlpha (0.95f));
            g.strokePath (p, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    g.restoreState();

    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    float ly = plot.getY() + 6.0f;
    for (const auto& ref : refs)
    {
        const auto row = juce::Rectangle<float> (plot.getRight() - 150.0f, ly, 144.0f, 16.0f);
        g.setColour (ref.colour);
        g.fillRoundedRectangle (row.getX(), row.getCentreY() - 1.5f, 10.0f, 3.0f, 1.5f);
        g.drawText ((ref.recording ? "* " : "") + ref.name, row.withTrimmedLeft (16.0f), juce::Justification::centredLeft);
        ly += 16.0f;
    }

    if (hovering && plot.contains (mouse))
        drawReadout (g);
}

void SpectrumView::drawReadout (juce::Graphics& g)
{
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.drawVerticalLine ((int) mouse.x, plot.getY(), plot.getBottom());
    g.drawHorizontalLine ((int) mouse.y, plot.getX(), plot.getRight());

    const float norm = (mouse.x - plot.getX()) / plot.getWidth();
    const float freq = kMinHz * std::pow (kMaxHz / kMinHz, norm);
    const float cursorDb = settings.topDb - (mouse.y - plot.getY()) / plot.getHeight() * settings.rangeDb;

    juce::StringArray lines;
    lines.add ((freq >= 1000.0f ? juce::String (freq / 1000.0f, 2) + " kHz" : juce::String ((int) freq) + " Hz")
               + "   " + juce::String (cursorDb, 1) + " dB");

    const bool showPeak = settings.mode != ViewMode::avg;
    const bool showAvg = settings.mode != ViewMode::peak;

    float liveA = 0.0f, liveP = 0.0f;
    if (settings.showLive)
    {
        liveA = dbAtX (prepLiveAvg, mouse.x);
        liveP = dbAtX (prepLivePeak, mouse.x);
        juce::String s = "Live";
        if (showAvg) s << "   avg " << juce::String (liveA, 1);
        if (showPeak) s << "   pk " << juce::String (liveP, 1);
        lines.add (s);
    }

    for (size_t r = 0; r < refs.size(); ++r)
    {
        if (! refs[r].selected)
            continue;
        const float a = dbAtX (prepAvg[r], mouse.x);
        const float p = dbAtX (prepPeak[r], mouse.x);
        juce::String s = refs[r].name;
        if (showAvg) s << "   avg " << juce::String (a, 1);
        if (showPeak) s << "   pk " << juce::String (p, 1);
        lines.add (s);

        if (settings.showLive)
        {
            juce::String d = "Diff";
            if (showAvg) d << "   avg " << (liveA - a >= 0 ? "+" : "") << juce::String (liveA - a, 1);
            if (showPeak) d << "   pk " << (liveP - p >= 0 ? "+" : "") << juce::String (liveP - p, 1);
            lines.add (d);
        }
    }

    const float w = 230.0f, h = 8.0f + 16.0f * (float) lines.size();
    auto box = juce::Rectangle<float> (mouse.x + 14.0f, mouse.y + 14.0f, w, h);
    if (box.getRight() > plot.getRight()) box.setX (mouse.x - 14.0f - w);
    if (box.getBottom() > plot.getBottom()) box.setY (mouse.y - 14.0f - h);

    g.setColour (juce::Colour (0xe0181d24));
    g.fillRoundedRectangle (box, 5.0f);
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.setFont (juce::FontOptions (12.0f));
    for (int i = 0; i < lines.size(); ++i)
        g.drawText (lines[i], box.reduced (8.0f, 4.0f).withHeight (16.0f).translated (0.0f, 16.0f * (float) i),
                    juce::Justification::centredLeft);
}

void SpectrumView::mouseMove (const juce::MouseEvent& e)
{
    mouse = e.position;
    hovering = true;
}

void SpectrumView::mouseExit (const juce::MouseEvent&)
{
    hovering = false;
}

void SpectrumView::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    if (onWheelShift)
        onWheelShift (w.deltaY > 0 ? -3.0f : 3.0f);
}
}
