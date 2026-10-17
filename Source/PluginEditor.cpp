#include "PluginEditor.h"

using namespace juce;

namespace ui
{
//==============================================================================
Colour modColour (Mod m)
{
    switch (m)
    {
        case Mod::EQ:        return Colour (0xffffb52e);
        case Mod::Rebalance: return Colour (0xff5ff0b4);
        case Mod::Dynamics:  return Colour (0xff47b4ff);
        case Mod::Exciter:   return Colour (0xffff7a45);
        case Mod::Imager:    return Colour (0xffb08cff);
        case Mod::Loudness:  return Colour (0xff7ee36b);
        default: break;
    }
    return Colours::white;
}
Colour chanColour (int c)
{
    static const Colour cc[] = { Colour (0xfff1e6c8), Colour (0xffffb52e), Colour (0xffb08cff), Colour (0xff5ff0b4), Colour (0xffff6b8a) };
    return cc[jlimit (0, 4, c)];
}
Font font (float size, bool bold) { return Font (FontOptions (size, bold ? Font::bold : Font::plain)); }

static const char* bandName (int k) { static const char* n[] = { "LOW", "LOW MID", "HIGH MID", "HIGH" }; return n[k]; }
static String hzText (float f) { return f >= 1000.0f ? String (f / 1000.0f, f >= 10000.0f ? 1 : 2) + "k" : String (roundToInt (f)); }

//==============================================================================
LNF::LNF()
{
    setColour (PopupMenu::backgroundColourId, Colour (0xff1b1e23));
    setColour (PopupMenu::textColourId, col::text);
    setColour (PopupMenu::highlightedBackgroundColourId, Colour (0xff3a4048));
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (ComboBox::textColourId, col::text);
    setColour (TextButton::textColourOffId, col::textDim);
    setColour (TextButton::textColourOnId, Colours::black);
}

void LNF::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float sa, float ea, Slider& s)
{
    const auto accent = s.findColour (Slider::rotarySliderFillColourId);
    const auto area = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float size = jmin (area.getWidth(), area.getHeight());
    const auto c = area.getCentre();
    const float ringR = size * 0.5f - 4.0f, bodyR = size * 0.34f;
    const float angle = sa + pos * (ea - sa);
    const float ringW = jmax (3.0f, size * 0.05f);

    Path track; track.addCentredArc (c.x, c.y, ringR, ringR, 0, sa, ea, true);
    g.setColour (Colour (0xff07080a));
    g.strokePath (track, PathStrokeType (ringW + 2, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (Colour (0xff2a2f36));
    g.strokePath (track, PathStrokeType (ringW, PathStrokeType::curved, PathStrokeType::rounded));
    const bool bipolar = s.getMinimum() < 0 && s.getMaximum() > 0;
    const float from = bipolar ? sa + (float) s.valueToProportionOfLength (0.0) * (ea - sa) : sa;
    if (std::abs (angle - from) > 0.005f)
    {
        Path arc; arc.addCentredArc (c.x, c.y, ringR, ringR, 0, jmin (from, angle), jmax (from, angle), true);
        g.setColour (accent.withAlpha (0.22f));
        g.strokePath (arc, PathStrokeType (ringW + 5, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (accent);
        g.strokePath (arc, PathStrokeType (ringW, PathStrokeType::curved, PathStrokeType::rounded));
    }
    Path body; body.addEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2, bodyR * 2);
    DropShadow (Colours::black.withAlpha (0.85f), (int) (size * 0.10f), { 0, (int) (size * 0.04f) }).drawForPath (g, body);
    g.setGradientFill (ColourGradient (Colour (0xff4a5059), c.x, c.y - bodyR, Colour (0xff17191d), c.x, c.y + bodyR, false));
    g.fillPath (body);
    g.setColour (Colours::white.withAlpha (0.12f));
    g.drawEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2, bodyR * 2, 1.0f);
    const auto p1 = c.getPointOnCircumference (bodyR * 0.25f, angle), p2 = c.getPointOnCircumference (bodyR * 0.92f, angle);
    g.setColour (accent.brighter (0.3f));
    g.drawLine ({ p1, p2 }, jmax (2.2f, size * 0.035f));
}

void LNF::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool hi, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const auto on = b.findColour (TextButton::buttonOnColourId);
    if (b.getToggleState())
    {
        g.setGradientFill (ColourGradient (on.brighter (0.25f), 0, r.getY(), on.darker (0.15f), 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (on.withAlpha (0.45f));
        g.drawRoundedRectangle (r.expanded (0.5f), 4.0f, 1.0f);
    }
    else
    {
        g.setGradientFill (ColourGradient (Colour (down ? 0xff1a1d21 : (hi ? 0xff353a42 : 0xff2a2e34)), 0, r.getY(), Colour (0xff1c1f23), 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (hi ? on.withAlpha (0.6f) : col::edge);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
    }
}

void LNF::drawButtonText (Graphics& g, TextButton& b, bool hi, bool)
{
    g.setFont (font (jmin (13.0f, (float) b.getHeight() * 0.48f)));
    g.setColour (b.getToggleState() ? Colour (0xff101214) : (hi ? col::text : col::textDim));
    g.drawText (b.getButtonText(), b.getLocalBounds().reduced (3, 0), Justification::centred, false);
}

void LNF::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (col::screen); g.fillRoundedRectangle (r, 4.0f);
    g.setColour (box.isMouseOver (true) ? col::textDim : col::edge); g.drawRoundedRectangle (r, 4.0f, 1.0f);
    Path a; const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    a.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (col::textDim); g.fillPath (a);
}
void LNF::positionComboBoxText (ComboBox& b, Label& l) { l.setBounds (8, 1, b.getWidth() - 26, b.getHeight() - 2); l.setFont (getComboBoxFont (b)); }

//==============================================================================
Knob::Knob (APVTS& s, Colour a, const String& t, const String& id, bool sm) : state (s), title (t), accent (a), small (sm)
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    slider.setColour (Slider::rotarySliderFillColourId, accent);
    slider.setMouseDragSensitivity (220);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
    if (id.isNotEmpty()) bind (id);
}
void Knob::bind (const String& id, const String& t)
{
    if (t.isNotEmpty()) title = t;
    if (id == pid && att) return;
    att.reset(); pid = id;
    att = std::make_unique<APVTS::SliderAttachment> (state, id, slider);
    if (auto* p = state.getParameter (id)) slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    repaint();
}
void Knob::resized()
{
    auto r = getLocalBounds(); r.removeFromTop (16); r.removeFromBottom (small ? 18 : 20);
    const int s = jmin (r.getWidth(), r.getHeight());
    slider.setBounds (r.withSizeKeepingCentre (s, s));
}
void Knob::paint (Graphics& g)
{
    auto r = getLocalBounds();
    g.setColour (col::textDim); g.setFont (font (small ? 11.0f : 12.0f));
    g.drawText (title, r.removeFromTop (16), Justification::centred, false);
    auto v = r.removeFromBottom (small ? 18 : 20).withSizeKeepingCentre (jmin (r.getWidth() - 4, 84), small ? 16 : 18).toFloat();
    g.setColour (col::screen); g.fillRoundedRectangle (v, 3.0f);
    g.setColour (accent.brighter (0.2f)); g.setFont (font (small ? 11.5f : 12.5f));
    if (auto* p = state.getParameter (pid)) g.drawText (p->getCurrentValueAsText(), v, Justification::centred, false);
}

//==============================================================================
Seg::Seg (APVTS& s, const StringArray& labels, const String& id, bool cc) : state (s), chanCol (cc)
{
    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = buttons.add (new TextButton (labels[i]));
        b->setColour (TextButton::buttonOnColourId, chanCol ? chanColour (i) : Colour (0xffe8e2d4));
        int e = 0; if (i > 0) e |= Button::ConnectedOnLeft; if (i < labels.size() - 1) e |= Button::ConnectedOnRight;
        b->setConnectedEdges (e);
        b->onClick = [this, i] { if (att) att->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }
    if (id.isNotEmpty()) bind (id);
}
void Seg::bind (const String& id)
{
    if (id == pid && att) return;
    pid = id; att.reset();
    if (auto* p = state.getParameter (id))
    {
        att = std::make_unique<ParameterAttachment> (*p, [this] (float v) { update (roundToInt (v)); });
        att->sendInitialUpdate();
    }
}
void Seg::update (int v)
{
    current = v;
    for (int i = 0; i < buttons.size(); ++i) buttons[i]->setToggleState (i == v, dontSendNotification);
    if (onChange) onChange (v);
}
void Seg::resized()
{
    const float w = (float) getWidth() / (float) jmax (1, buttons.size());
    for (int i = 0; i < buttons.size(); ++i) buttons[i]->setBounds (roundToInt (w * (float) i), 0, roundToInt (w * (float) (i + 1)) - roundToInt (w * (float) i), getHeight());
}

Toggle::Toggle (APVTS& s, const String& label, Colour on, const String& id) : TextButton (label), state (s)
{
    setClickingTogglesState (true);
    setColour (TextButton::buttonOnColourId, on);
    if (id.isNotEmpty()) bind (id);
}
void Toggle::bind (const String& id)
{
    att.reset();
    att = std::make_unique<APVTS::ButtonAttachment> (state, id, *this);
}

//==============================================================================
Analyzer::Analyzer()
{
    for (auto* v : { &rPreM, &rPreS, &rPostM, &rPostS }) v->assign ((size_t) N, 0.0f);
    work.assign ((size_t) N * 2, 0.0f); tA.assign (8192, 0.0f); tB.assign (8192, 0.0f);
    preMid.fill (-120); postMid.fill (-120); postSide.fill (-120);
}
void Analyzer::update (MasterSuiteProcessor& p, double fs)
{
    auto feed = [this] (ms::ScopeFifo<32768>& f, std::vector<float>& ra, std::vector<float>& rb, int& wp)
    {
        int n;
        while ((n = f.pull (tA.data(), tB.data(), 8192)) > 0)
            for (int i = 0; i < n; ++i) { ra[(size_t) wp] = tA[(size_t) i]; rb[(size_t) wp] = tB[(size_t) i]; wp = (wp + 1) % N; }
    };
    int p1 = pos; feed (p.preFifo, rPreM, rPreS, p1);
    int p2 = pos; feed (p.postFifo, rPostM, rPostS, p2);
    pos = p2;
    compute (rPreM, p1, preMid, fs);
    compute (rPostM, p2, postMid, fs);
    compute (rPostS, p2, postSide, fs);
}
void Analyzer::compute (std::vector<float>& ring, int wp, std::array<float, P>& out, double fs)
{
    for (int i = 0; i < N; ++i) work[(size_t) i] = ring[(size_t) ((wp + i) % N)];
    std::fill (work.begin() + N, work.end(), 0.0f);
    win.multiplyWithWindowingTable (work.data(), (size_t) N);
    fft.performFrequencyOnlyForwardTransform (work.data());
    const float binHz = (float) fs / (float) N, norm = 4.0f / (float) N;
    for (int i = 0; i < P; ++i)
    {
        const float f = freqAt (i), fl = f * 0.966f, fh = f * 1.035f;
        const int k0 = jmax (1, (int) std::floor (fl / binHz)), k1 = jmin (N / 2 - 1, (int) std::ceil (fh / binHz));
        float pw = 0;
        if (k1 - k0 <= 1)
        {
            const float kf = f / binHz; const int k = jlimit (1, N / 2 - 2, (int) kf); const float t = kf - (float) k;
            const float m = work[(size_t) k] * (1 - t) + work[(size_t) k + 1] * t; pw = m * m;
        }
        else { for (int k = k0; k <= k1; ++k) pw += work[(size_t) k] * work[(size_t) k]; pw /= (float) (k1 - k0 + 1); }
        float db = 10.0f * std::log10 (pw * norm * norm + 1.0e-14f) + 4.5f * std::log2 (f / 1000.0f);
        out[(size_t) i] = db > out[(size_t) i] ? db : jmax (db, out[(size_t) i] - 1.6f);
    }
}

void drawFreqGrid (Graphics& g, const FreqAxis& ax, float top, float bottom, float step, bool labels)
{
    g.setColour (col::grid);
    for (float f : { 30.f, 40.f, 50.f, 60.f, 70.f, 80.f, 90.f, 200.f, 300.f, 400.f, 500.f, 600.f, 700.f, 800.f, 900.f, 2000.f, 3000.f, 4000.f, 5000.f, 6000.f, 7000.f, 8000.f, 9000.f })
        g.drawVerticalLine (roundToInt (ax.x (f)), ax.r.getY(), ax.r.getBottom());
    g.setColour (col::grid.brighter (0.35f));
    for (float f : { 100.f, 1000.f, 10000.f })
        g.drawVerticalLine (roundToInt (ax.x (f)), ax.r.getY(), ax.r.getBottom());
    for (float d = top; d >= bottom - 0.01f; d -= step)
    {
        const float y = jmap (d, top, bottom, ax.r.getY(), ax.r.getBottom());
        g.setColour (std::abs (d) < 0.01f ? col::grid.brighter (0.5f) : col::grid);
        g.drawHorizontalLine (roundToInt (y), ax.r.getX(), ax.r.getRight());
        if (labels)
        {
            g.setColour (col::textDim.withAlpha (0.6f)); g.setFont (font (10.0f, false));
            g.drawText ((d > 0 ? "+" : "") + String (roundToInt (d)), Rectangle<float> (ax.r.getRight() - 30, y - 7, 26, 14), Justification::centredRight, false);
        }
    }
    if (labels)
    {
        g.setColour (col::textDim.withAlpha (0.7f)); g.setFont (font (10.0f, false));
        for (float f : { 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f })
            g.drawText (hzText (f), Rectangle<float> (ax.x (f) + 3, ax.r.getBottom() - 14, 40, 12), Justification::left, false);
    }
}

void drawSpectrum (Graphics& g, const FreqAxis& ax, const std::array<float, Analyzer::P>& s, const Analyzer& an, Colour c, float top, float bottom, bool fill)
{
    Path p;
    for (int i = 0; i < Analyzer::P; ++i)
    {
        const float x = ax.x (an.freqAt (i)), y = jmap (jlimit (bottom, top, s[(size_t) i]), top, bottom, ax.r.getY(), ax.r.getBottom());
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    if (fill)
    {
        Path f (p); f.lineTo (ax.r.getRight(), ax.r.getBottom()); f.lineTo (ax.r.getX(), ax.r.getBottom()); f.closeSubPath();
        g.setGradientFill (ColourGradient (c.withAlpha (0.30f), 0, ax.r.getY(), c.withAlpha (0.05f), 0, ax.r.getBottom(), false));
        g.fillPath (f);
    }
    g.setColour (c.withAlpha (fill ? 0.7f : 0.85f));
    g.strokePath (p, PathStrokeType (1.2f));
}

//==============================================================================
EqGraph::EqGraph (MasterSuiteProcessor& p, Analyzer& a) : proc (p), an (a) {}

float EqGraph::yOf (float db) const { return jmap (db, range, -range, ax.r.getY(), ax.r.getBottom()); }
float EqGraph::dbOf (float y) const { return jmap (y, ax.r.getY(), ax.r.getBottom(), range, -range); }

Point<float> EqGraph::nodePos (int b) const
{
    const auto t = (ms::EqType) (int) proc.get (ids::b ("eq", b + 1, "type"));
    const float f = proc.get (ids::b ("eq", b + 1, "freq"));
    const float gdb = (t == ms::EqType::LowCut || t == ms::EqType::HighCut) ? 0.0f : proc.get (ids::b ("eq", b + 1, "gain"));
    return { ax.x (f), yOf (gdb) };
}
int EqGraph::hit (Point<float> p) const
{
    int best = -1; float bd = 14.0f;
    for (int b = 0; b < 6; ++b) { const float d = nodePos (b).getDistanceFrom (p); if (d < bd) { bd = d; best = b; } }
    return best;
}

void EqGraph::paint (Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (col::screen); g.fillRoundedRectangle (bounds, 6.0f);
    ax.r = bounds.reduced (8, 6);
    drawFreqGrid (g, ax, range, -range, 6.0f, true);

    // spectra (analyzer scale: 0 dB at top .. -90 at bottom)
    const float sTop = 6.0f, sBot = -96.0f;
    drawSpectrum (g, ax, an.preMid, an, Colour (0xff6c7480), sTop, sBot, false);
    drawSpectrum (g, ax, an.postMid, an, Colour (0xff7fc8ff), sTop, sBot, true);
    drawSpectrum (g, ax, an.postSide, an, chanColour (2).withAlpha (0.8f), sTop, sBot, false);

    // response curves per channel group
    const double fs = proc.getFs();
    ms::Biquad q[6]; bool on[6]; int ch[6];
    bool used[5] {};
    for (int b = 0; b < 6; ++b)
    {
        on[b] = proc.get (ids::b ("eq", b + 1, "on")) > 0.5f;
        ch[b] = (int) proc.get (ids::b ("eq", b + 1, "chan"));
        ms::designEq (q[b], (ms::EqType) (int) proc.get (ids::b ("eq", b + 1, "type")), fs, proc.get (ids::b ("eq", b + 1, "freq")),
                      proc.get (ids::b ("eq", b + 1, "gain")), proc.get (ids::b ("eq", b + 1, "q")));
        if (on[b]) used[ch[b]] = true;
    }
    auto curve = [&] (int group, Colour c, float width, bool fillIt)
    {
        Path p;
        for (int i = 0; i <= 300; ++i)
        {
            const float x = ax.r.getX() + ax.r.getWidth() * (float) i / 300.0f, f = ax.f (x);
            double db = 0;
            for (int b = 0; b < 6; ++b)
                if (on[b] && (ch[b] == 0 || ch[b] == group)) db += q[b].magnitudeDb (f, fs);
            const float y = yOf (jlimit (-range - 4, range + 4, (float) db));
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        if (fillIt)
        {
            Path f (p); f.lineTo (ax.r.getRight(), yOf (0)); f.lineTo (ax.r.getX(), yOf (0)); f.closeSubPath();
            g.setColour (c.withAlpha (0.10f)); g.fillPath (f);
        }
        g.setColour (c.withAlpha (0.35f)); g.strokePath (p, PathStrokeType (width + 3.0f));
        g.setColour (c); g.strokePath (p, PathStrokeType (width));
    };
    curve (1, chanColour (used[1] ? 1 : 0), 2.2f, true);                // stereo + mid
    if (used[2]) curve (2, chanColour (2), 2.0f, false);
    if (used[3]) curve (3, chanColour (3), 1.6f, false);
    if (used[4]) curve (4, chanColour (4), 1.6f, false);

    // nodes
    for (int b = 0; b < 6; ++b)
    {
        const auto p = nodePos (b);
        const auto c = chanColour (ch[b]);
        const float r = b == selected ? 10.0f : 8.0f;
        if (b == selected)
        {
            g.setColour (c.withAlpha (0.25f)); g.fillEllipse (p.x - 16, p.y - 16, 32, 32);
        }
        g.setColour (on[b] ? c : Colour (0xff3b4047)); g.fillEllipse (p.x - r, p.y - r, r * 2, r * 2);
        g.setColour (Colours::black.withAlpha (0.7f)); g.drawEllipse (p.x - r, p.y - r, r * 2, r * 2, 1.2f);
        g.setColour (on[b] ? Colour (0xff101214) : col::textDim); g.setFont (font (11.5f));
        g.drawText (String (b + 1), Rectangle<float> (p.x - r, p.y - r, r * 2, r * 2), Justification::centred, false);
        if (on[b] && ch[b] != 0)
        {
            static const char* tag[] = { "", "M", "S", "L", "R" };
            g.setColour (c); g.setFont (font (10.0f));
            g.drawText (tag[ch[b]], Rectangle<float> (p.x + r, p.y - r - 10, 14, 12), Justification::left, false);
        }
    }
    // legend
    g.setFont (font (11.0f));
    float lx = ax.r.getX() + 8;
    auto leg = [&] (const String& t, Colour c) { g.setColour (c); g.fillRect (lx, ax.r.getY() + 9, 14.0f, 3.0f); g.setColour (col::textDim);
                                                 g.drawText (t, Rectangle<float> (lx + 18, ax.r.getY() + 3, 90, 14), Justification::left, false); lx += 24 + (float) t.length() * 7.0f; };
    leg ("MID (post)", Colour (0xff7fc8ff)); leg ("SIDE (post)", chanColour (2)); leg ("INPUT", Colour (0xff6c7480));
    g.setColour (col::textDim.withAlpha (0.6f)); g.setFont (font (10.5f, false));
    g.drawText ("drag = freq / gain   wheel = Q   double-click = band on / off", ax.r.reduced (8, 4), Justification::topRight, false);
}

void EqGraph::mouseDown (const MouseEvent& e)
{
    const int h = hit (e.position);
    if (h >= 0)
    {
        selected = h; dragging = h;
        if (onSelect) onSelect (h);
        for (auto* w : { "freq", "gain" }) if (auto* p = proc.apvts.getParameter (ids::b ("eq", h + 1, w))) p->beginChangeGesture();
        repaint();
    }
}
void EqGraph::mouseDrag (const MouseEvent& e)
{
    if (dragging < 0) return;
    const int b = dragging + 1;
    if (auto* p = proc.apvts.getParameter (ids::b ("eq", b, "freq"))) p->setValueNotifyingHost (p->convertTo0to1 (jlimit (20.0f, 20000.0f, ax.f (e.position.x))));
    const auto t = (ms::EqType) (int) proc.get (ids::b ("eq", b, "type"));
    if (t != ms::EqType::LowCut && t != ms::EqType::HighCut)
        if (auto* p = proc.apvts.getParameter (ids::b ("eq", b, "gain"))) p->setValueNotifyingHost (p->convertTo0to1 (jlimit (-range, range, dbOf (e.position.y))));
    repaint();
}
void EqGraph::mouseUp (const MouseEvent&)
{
    if (dragging >= 0)
        for (auto* w : { "freq", "gain" }) if (auto* p = proc.apvts.getParameter (ids::b ("eq", dragging + 1, w))) p->endChangeGesture();
    dragging = -1;
}
void EqGraph::mouseDoubleClick (const MouseEvent& e)
{
    const int h = hit (e.position);
    if (h >= 0) { proc.setParam (ids::b ("eq", h + 1, "on"), proc.get (ids::b ("eq", h + 1, "on")) > 0.5f ? 0.0f : 1.0f); return; }
    for (int b = 1; b <= 6; ++b)
        if (proc.get (ids::b ("eq", b, "on")) < 0.5f)
        {
            proc.setParam (ids::b ("eq", b, "on"), 1); proc.setParam (ids::b ("eq", b, "freq"), ax.f (e.position.x));
            proc.setParam (ids::b ("eq", b, "gain"), jlimit (-range, range, dbOf (e.position.y)));
            selected = b - 1; if (onSelect) onSelect (selected);
            return;
        }
}
void EqGraph::mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w)
{
    if (auto* p = proc.apvts.getParameter (ids::b ("eq", selected + 1, "q")))
    {
        const float q = proc.get (ids::b ("eq", selected + 1, "q")) * std::pow (1.15f, w.deltaY * 4.0f);
        p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (jlimit (0.1f, 18.0f, q))); p->endChangeGesture();
    }
}

//==============================================================================
XoverGraph::XoverGraph (MasterSuiteProcessor& p, Analyzer& a, Colour c) : proc (p), an (a), accent (c) {}
float XoverGraph::xo (int k) const { return proc.get ("xo" + String (k + 1)); }

void XoverGraph::paint (Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (col::screen); g.fillRoundedRectangle (bounds, 6.0f);
    ax.r = bounds.reduced (8, 6);
    drawFreqGrid (g, ax, 0, -90, 15, false);
    const float edges[5] = { 20.0f, xo (0), xo (1), xo (2), 20000.0f };
    for (int k = 0; k < 4; ++k)
    {
        const float x0 = ax.x (edges[k]), x1 = ax.x (edges[k + 1]);
        g.setColour (accent.withAlpha (k % 2 == 0 ? 0.07f : 0.03f));
        g.fillRect (x0, ax.r.getY(), x1 - x0, ax.r.getHeight());
        g.setColour (col::textDim); g.setFont (font (11.0f));
        g.drawText (bandName (k), Rectangle<float> (x0, ax.r.getY() + 4, x1 - x0, 14), Justification::centred, false);
        if (bandText)
        {
            g.setColour (accent.brighter (0.2f)); g.setFont (font (13.0f));
            g.drawText (bandText (k), Rectangle<float> (x0, ax.r.getY() + 20, x1 - x0, 16), Justification::centred, false);
        }
    }
    drawSpectrum (g, ax, an.postMid, an, Colour (0xff7fc8ff), 6, -96, true);
    drawSpectrum (g, ax, an.postSide, an, chanColour (2).withAlpha (0.75f), 6, -96, false);
    if (overlay) overlay (g, ax, ax.r);
    for (int k = 0; k < 3; ++k)
    {
        const float x = ax.x (xo (k));
        const bool hot = k == hover || k == dragging;
        g.setColour (accent.withAlpha (hot ? 1.0f : 0.7f));
        g.fillRect (x - (hot ? 1.5f : 1.0f), ax.r.getY(), hot ? 3.0f : 2.0f, ax.r.getHeight());
        Rectangle<float> tag (x - 26, ax.r.getBottom() - 20, 52, 16);
        g.setColour (Colour (0xff101214)); g.fillRoundedRectangle (tag, 3);
        g.setColour (accent); g.drawRoundedRectangle (tag, 3, 1);
        g.setFont (font (11.0f)); g.drawText (hzText (xo (k)) + " Hz", tag, Justification::centred, false);
    }
}
void XoverGraph::mouseMove (const MouseEvent& e)
{
    int h = -1; for (int k = 0; k < 3; ++k) if (std::abs (ax.x (xo (k)) - e.position.x) < 8) h = k;
    if (h != hover) { hover = h; repaint(); }
    setMouseCursor (h >= 0 ? MouseCursor::LeftRightResizeCursor : MouseCursor::NormalCursor);
}
void XoverGraph::mouseDown (const MouseEvent& e)
{
    for (int k = 0; k < 3; ++k) if (std::abs (ax.x (xo (k)) - e.position.x) < 8) dragging = k;
    if (dragging >= 0) if (auto* p = proc.apvts.getParameter ("xo" + String (dragging + 1))) p->beginChangeGesture();
}
void XoverGraph::mouseDrag (const MouseEvent& e)
{
    if (dragging < 0) return;
    if (auto* p = proc.apvts.getParameter ("xo" + String (dragging + 1)))
    {
        float f = ax.f (e.position.x);
        if (dragging > 0) f = jmax (f, xo (dragging - 1) * 1.6f);
        if (dragging < 2) f = jmin (f, xo (dragging + 1) / 1.6f);
        p->setValueNotifyingHost (p->convertTo0to1 (f));
    }
    repaint();
}
void XoverGraph::mouseUp (const MouseEvent&)
{
    if (dragging >= 0) if (auto* p = proc.apvts.getParameter ("xo" + String (dragging + 1))) p->endChangeGesture();
    dragging = -1;
}

//==============================================================================
Page::Page (MasterSuiteProcessor& p, Mod m) : proc (p), mod (m), power (p.apvts, "ON", modColour (m), String (modKey (m)) + "_on")
{
    addAndMakeVisible (power);
}
void Page::panel (Graphics& g, Rectangle<int> r, const String& title) const
{
    auto rf = r.toFloat();
    g.setColour (col::panel); g.fillRoundedRectangle (rf, 6.0f);
    g.setColour (col::edge); g.drawRoundedRectangle (rf.reduced (0.5f), 6.0f, 1.0f);
    if (title.isNotEmpty())
    {
        g.setColour (modColour (mod)); g.setFont (font (11.5f));
        g.drawText (title, r.reduced (10, 4).removeFromTop (14), Justification::left, false);
    }
}
void Page::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colour (0xff16191d)); g.fillRoundedRectangle (r, 8.0f);
    g.setColour (col::edge); g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
    const auto c = modColour (mod);
    g.setColour (c); g.setFont (font (18.0f));
    g.drawText (modName (mod), 16, 10, 220, 24, Justification::left, false);
    static const char* sub[] = { "6-band parametric  |  every band: Stereo / Mid / Side / Left / Right  |  band LISTEN",
                                 "move whole frequency regions up or down  |  per band channel mode  |  solo",
                                 "4-band clean compressor + GLUE  |  flat, no colour  |  per band channel mode  |  solo",
                                 "multiband harmonics with analog emulations  |  per band channel mode  |  solo",
                                 "4-band stereo width  |  correlation + phase check  |  vectorscope",
                                 "LUFS meter + true-peak maximizer  |  targets for every platform" };
    g.setColour (col::textDim); g.setFont (font (12.0f, false));
    g.drawText (sub[(int) mod], 140, 12, 700, 20, Justification::left, false);
    power.setBounds (getWidth() - 74, 10, 60, 24);
}

//==============================================================================
EqPage::EqPage (MasterSuiteProcessor& p, Analyzer& a)
    : Page (p, Mod::EQ), graph (p, a), chan (p.apvts, StringArray { "STEREO", "MID", "SIDE", "LEFT", "RIGHT" }, {}, true),
      freq (p.apvts, modColour (Mod::EQ), "FREQ"), gain (p.apvts, modColour (Mod::EQ), "GAIN"), q (p.apvts, modColour (Mod::EQ), "Q"),
      bandOn (p.apvts, "BAND ON", modColour (Mod::EQ))
{
    addAndMakeVisible (graph);
    graph.onSelect = [this] (int b) { select (b); };
    for (int i = 0; i < 6; ++i)
    {
        auto* b = bandBtns.add (new TextButton ("BAND " + String (i + 1)));
        b->setColour (TextButton::buttonOnColourId, modColour (Mod::EQ));
        b->onClick = [this, i] { select (i); };
        addAndMakeVisible (b);
    }
    type.addItemList (ms::eqTypeNames(), 1);
    for (auto* c : std::initializer_list<Component*> { &type, &chan, &freq, &gain, &q, &bandOn, &listenBtn }) addAndMakeVisible (c);
    listenBtn.setClickingTogglesState (false);
    listenBtn.setColour (TextButton::buttonOnColourId, Colour (0xffffe08a));
    listenBtn.onClick = [this]
    {
        const int cur = (int) proc.get ("eq_listen");
        proc.setParam ("eq_listen", cur == sel + 1 ? 0.0f : (float) (sel + 1));
    };
    select (2);
}
void EqPage::select (int b)
{
    sel = b; graph.selected = b;
    for (int i = 0; i < 6; ++i) bandBtns[i]->setToggleState (i == b, dontSendNotification);
    typeAtt.reset();
    typeAtt = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, ids::b ("eq", b + 1, "type"), type);
    chan.bind (ids::b ("eq", b + 1, "chan"));
    freq.bind (ids::b ("eq", b + 1, "freq")); gain.bind (ids::b ("eq", b + 1, "gain")); q.bind (ids::b ("eq", b + 1, "q"));
    bandOn.bind (ids::b ("eq", b + 1, "on"));
    graph.repaint(); repaint();
}
void EqPage::resized()
{
    auto r = content();
    graph.setBounds (r.removeFromTop (378));
    r.removeFromTop (8);
    auto row = r.removeFromTop (30);
    const int bw = (row.getWidth() - 5 * 6) / 6;
    for (int i = 0; i < 6; ++i) bandBtns[i]->setBounds (row.getX() + i * (bw + 6), row.getY(), bw, 30);
    r.removeFromTop (8);
    auto ctl = r.reduced (12, 6);
    ctl.removeFromTop (18);
    auto left = ctl.removeFromLeft (370);
    auto tr = left.removeFromTop (28);
    lblType = tr.removeFromLeft (70); type.setBounds (tr.removeFromLeft (170));
    left.removeFromTop (8);
    auto cr = left.removeFromTop (28);
    lblChan = cr.removeFromLeft (70); chan.setBounds (cr);
    left.removeFromTop (8);
    auto bb = left.removeFromTop (30);
    bb.removeFromLeft (70);
    bandOn.setBounds (bb.removeFromLeft (120)); bb.removeFromLeft (10); listenBtn.setBounds (bb);
    ctl.removeFromLeft (24);
    const int kw = 116;
    freq.setBounds (ctl.removeFromLeft (kw)); gain.setBounds (ctl.removeFromLeft (kw)); q.setBounds (ctl.removeFromLeft (kw));
    ctl.removeFromLeft (14);
    hintArea = ctl;
}
void EqPage::paint (Graphics& g)
{
    Page::paint (g);
    auto r = content(); r.removeFromTop (378 + 8 + 30 + 8);
    panel (g, r, "SELECTED BAND " + String (sel + 1));
    g.setColour (col::textDim); g.setFont (font (11.0f));
    g.drawText ("TYPE", lblType, Justification::centredLeft, false);
    g.drawText ("WORKS ON", lblChan, Justification::centredLeft, false);
    const int li = (int) proc.get ("eq_listen");
    auto h = hintArea;
    if (li > 0)
    {
        g.setColour (Colour (0xffffe08a)); g.setFont (font (12.5f));
        g.drawFittedText ("LISTENING: BAND " + String (li) + "\n(" + ms::chanNames()[(int) proc.get (ids::b ("eq", li, "chan"))] + " only)",
                          h.removeFromTop (40), Justification::topLeft, 2);
    }
    g.setColour (col::textDim.withAlpha (0.75f)); g.setFont (font (11.0f, false));
    g.drawFittedText ("LISTEN = hear only what this band touches, in its own channel (Mid / Side / Left / Right).\nPick SIDE to EQ only the stereo edges, MID for vocal / kick / bass.",
                      h, Justification::bottomLeft, 6);
}
void EqPage::tick()
{
    graph.repaint();
    const bool l = (int) proc.get ("eq_listen") == sel + 1;
    if (listenBtn.getToggleState() != l) { listenBtn.setToggleState (l, dontSendNotification); repaint(); }
    for (int i = 0; i < 6; ++i)
    {
        const bool on = proc.get (ids::b ("eq", i + 1, "on")) > 0.5f;
        bandBtns[i]->setButtonText ((on ? "BAND " : "band ") + String (i + 1) + ((int) proc.get ("eq_listen") == i + 1 ? "  ))" : ""));
    }
}

//==============================================================================
BandStrip::BandStrip (MasterSuiteProcessor& p, Mod m, int b, Colour c) : band (b), accent (c), proc (p)
{
    const String k = modKey (m);
    const int n = b + 1;
    if (m != Mod::Imager)
    {
        on = std::make_unique<Toggle> (p.apvts, "ON", c, ids::b (k.toRawUTF8(), n, "on"));
        chan = std::make_unique<Seg> (p.apvts, StringArray { "ST", "M", "S", "L", "R" }, ids::b (k.toRawUTF8(), n, "chan"), true);
        addAndMakeVisible (*on); addAndMakeVisible (*chan);
    }
    solo = std::make_unique<Toggle> (p.apvts, "SOLO", Colour (0xffffe08a), ids::b (k.toRawUTF8(), n, "solo"));
    addAndMakeVisible (*solo);
    auto add = [&] (const char* what, const char* title) { addAndMakeVisible (knobs.add (new Knob (p.apvts, c, title, ids::b (k.toRawUTF8(), n, what), true))); };
    switch (m)
    {
        case Mod::Rebalance: add ("gain", "LEVEL"); break;
        case Mod::Dynamics:  add ("thr", "THRESHOLD"); add ("ratio", "RATIO"); add ("att", "ATTACK"); add ("rel", "RELEASE"); add ("mk", "MAKEUP"); break;
        case Mod::Exciter:
            combo = std::make_unique<ComboBox>(); combo->addItemList (ms::exTypeNames(), 1); addAndMakeVisible (*combo);
            comboAtt = std::make_unique<APVTS::ComboBoxAttachment> (p.apvts, ids::b ("ex", n, "type"), *combo);
            add ("drive", "DRIVE"); break;
        case Mod::Imager: add ("width", "WIDTH"); break;
        default: break;
    }
}
void BandStrip::resized()
{
    auto r = getLocalBounds().reduced (8);
    r.removeFromTop (36);
    auto row = r.removeFromTop (24);
    if (on) { on->setBounds (row.removeFromLeft (row.getWidth() / 2 - 3)); row.removeFromLeft (6); }
    solo->setBounds (row);
    r.removeFromTop (6);
    if (chan) { chan->setBounds (r.removeFromTop (24)); r.removeFromTop (8); }
    if (combo) { combo->setBounds (r.removeFromTop (26)); r.removeFromTop (8); }
    if (meter) r.removeFromBottom (26);
    if (knobs.size() == 1) knobs[0]->setBounds (r.withSizeKeepingCentre (jmin (r.getWidth(), 130), jmin (r.getHeight(), 150)));
    else
    {
        const int cols = 2, rows = (knobs.size() + 1) / 2;
        const int cw = r.getWidth() / cols, ch = r.getHeight() / rows;
        for (int i = 0; i < knobs.size(); ++i)
        {
            Rectangle<int> cell (r.getX() + (i % cols) * cw, r.getY() + (i / cols) * ch, cw, ch);
            if (i == knobs.size() - 1 && knobs.size() % 2 == 1) cell = Rectangle<int> (r.getX(), cell.getY(), r.getWidth(), ch).withSizeKeepingCentre (cw, ch);
            knobs[i]->setBounds (cell);
        }
    }
}
void BandStrip::paint (Graphics& g)
{
    auto rf = getLocalBounds().toFloat();
    g.setColour (col::panel); g.fillRoundedRectangle (rf, 6.0f);
    g.setColour (col::edge); g.drawRoundedRectangle (rf.reduced (0.5f), 6.0f, 1.0f);
    g.setColour (accent); g.setFont (font (13.0f));
    g.drawText (bandName (band), 10, 6, getWidth() - 20, 16, Justification::left, false);
    const float lo = band == 0 ? 20.0f : proc.get ("xo" + String (band)), hi = band == 3 ? 20000.0f : proc.get ("xo" + String (band + 1));
    g.setColour (col::textDim); g.setFont (font (11.0f, false));
    g.drawText (hzText (lo) + " - " + hzText (hi) + " Hz", 10, 22, getWidth() - 20, 14, Justification::left, false);
    if (meter) meter (g, getLocalBounds().reduced (10, 8).removeFromBottom (20));
}

//==============================================================================
MultiPage::MultiPage (MasterSuiteProcessor& p, Analyzer& a, Mod m) : Page (p, m), graph (p, a, modColour (m))
{
    addAndMakeVisible (graph);
    for (int k = 0; k < 4; ++k)
    {
        auto* s = strips.add (new BandStrip (p, m, k, modColour (m)));
        addAndMakeVisible (s);
        if (m == Mod::Dynamics)
            s->meter = [this, k] (Graphics& g, Rectangle<int> r)
            {
                g.setColour (col::screen); g.fillRoundedRectangle (r.toFloat(), 3);
                const float frac = jlimit (0.0f, 1.0f, gr[k] / 12.0f);
                auto f = r.toFloat().reduced (2); f = f.withWidth (f.getWidth() * frac);
                g.setColour (modColour (Mod::Dynamics)); g.fillRoundedRectangle (f, 2);
                g.setColour (col::text); g.setFont (font (11.0f));
                g.drawText ("GR -" + String (gr[k], 1) + " dB", r, Justification::centred, false);
            };
    }
    if (m == Mod::Dynamics)
    {
        const auto c = modColour (m);
        for (auto kv : { std::pair<const char*, const char*> { "glue_thr", "THRESHOLD" }, { "glue_ratio", "RATIO" }, { "glue_att", "ATTACK" }, { "glue_rel", "RELEASE" },
                         { "glue_mk", "MAKEUP" }, { "glue_mix", "MIX" }, { "glue_hpf", "SC HPF" } })
            addAndMakeVisible (extraKnobs.add (new Knob (p.apvts, c, kv.second, kv.first, true)));
        glueChan = std::make_unique<Seg> (p.apvts, StringArray { "ST", "M", "S", "L", "R" }, "glue_chan", true);
        glueOn = std::make_unique<Toggle> (p.apvts, "GLUE ON", c, "glue_on");
        addAndMakeVisible (*glueChan); addAndMakeVisible (*glueOn);
        graph.overlay = [this] (Graphics& g, const FreqAxis& ax, Rectangle<float> r)
        {
            const float edges[5] = { 20.0f, proc.get ("xo1"), proc.get ("xo2"), proc.get ("xo3"), 20000.0f };
            for (int k = 0; k < 4; ++k)
            {
                const float x0 = ax.x (edges[k]) + 6, x1 = ax.x (edges[k + 1]) - 6;
                const float h = jlimit (0.0f, 1.0f, gr[k] / 12.0f) * r.getHeight() * 0.5f;
                g.setColour (modColour (Mod::Dynamics).withAlpha (0.35f));
                g.fillRect (x0, r.getY() + 40, x1 - x0, h);
            }
        };
        graph.bandText = [this] (int k) { return gr[k] > 0.05f ? "-" + String (gr[k], 1) + " dB" : String(); };
    }
    else if (m == Mod::Exciter)
    {
        addAndMakeVisible (extraKnobs.add (new Knob (p.apvts, modColour (m), "MIX", "ex_mix")));
        graph.bandText = [this] (int k) { return proc.get (ids::b ("ex", k + 1, "drive")) > 0.5f ? ms::exTypeNames()[(int) proc.get (ids::b ("ex", k + 1, "type"))] + " " + String (roundToInt (proc.get (ids::b ("ex", k + 1, "drive")))) + "%" : String(); };
    }
    else if (m == Mod::Rebalance)
    {
        graph.bandText = [this] (int k) { const float v = proc.get (ids::b ("rb", k + 1, "gain")); return std::abs (v) > 0.04f ? (v > 0 ? "+" : "") + String (v, 1) + " dB" : String(); };
    }
}
void MultiPage::resized()
{
    auto r = content();
    graph.setBounds (r.removeFromTop (mod == Mod::Rebalance ? 300 : 220));
    r.removeFromTop (10);
    Rectangle<int> extra;
    if (mod == Mod::Dynamics) extra = r.removeFromRight (262);
    if (mod == Mod::Exciter) extra = r.removeFromRight (150);
    if (! extra.isEmpty()) r.removeFromRight (10);
    const int w = (r.getWidth() - 3 * 10) / 4;
    for (int k = 0; k < 4; ++k) strips[k]->setBounds (r.getX() + k * (w + 10), r.getY(), w, r.getHeight());
    if (mod == Mod::Dynamics)
    {
        auto e = extra.reduced (10); e.removeFromTop (26);
        auto row = e.removeFromTop (24);
        glueOn->setBounds (row.removeFromLeft (90)); row.removeFromLeft (8); glueChan->setBounds (row);
        e.removeFromTop (6);
        const int cw = e.getWidth() / 2, ch = e.getHeight() / 4;
        for (int i = 0; i < extraKnobs.size(); ++i)
        {
            Rectangle<int> cell (e.getX() + (i % 2) * cw, e.getY() + (i / 2) * ch, cw, ch);
            if (i == 6) cell = cell.withX (e.getX() + e.getWidth() / 4);
            extraKnobs[i]->setBounds (cell);
        }
    }
    if (mod == Mod::Exciter) extraKnobs[0]->setBounds (extra.reduced (10).withTrimmedTop (40).removeFromTop (150));
}
void MultiPage::paint (Graphics& g)
{
    Page::paint (g);
    auto r = content(); r.removeFromTop ((mod == Mod::Rebalance ? 300 : 220) + 10);
    if (mod == Mod::Dynamics)
    {
        auto e = r.removeFromRight (262);
        panel (g, e, "GLUE  (bus compressor)");
        g.setColour (col::textDim); g.setFont (font (11.0f));
        g.drawText ("GR -" + String (glue, 1) + " dB", e.reduced (10, 6).removeFromTop (14), Justification::right, false);
    }
    if (mod == Mod::Exciter)
    {
        auto e = r.removeFromRight (150);
        panel (g, e, "EXCITER");
        g.setColour (col::textDim); g.setFont (font (11.0f, false));
        g.drawFittedText ("Tube: warm even\nTape: soft, smooth\nTransformer: thick lows\nConsole: punch / glue\nCrystal: air & shine",
                          e.reduced (10).withTrimmedTop (200), Justification::topLeft, 6);
    }
}
void MultiPage::tick()
{
    if (mod == Mod::Dynamics)
    {
        for (int k = 0; k < 4; ++k) { const float v = proc.dynGr[k].exchange (0.0f); gr[k] = v > gr[k] ? v : gr[k] * 0.85f; }
        const float gv = proc.glueGr.exchange (0.0f); glue = gv > glue ? gv : glue * 0.85f;
        for (auto* s : strips) s->repaint();
        repaint (content().withTrimmedTop (230).removeFromRight (262));
    }
    graph.repaint();
}

//==============================================================================
void Vectorscope::pull()
{
    float a[4096], b[4096]; int n;
    while ((n = proc.scopeFifo.pull (a, b, 4096)) > 0)
        for (int i = 0; i < n; ++i) { L[(size_t) wpos] = a[i]; R[(size_t) wpos] = b[i]; wpos = (wpos + 1) % (int) L.size(); }
}
void Vectorscope::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (col::screen); g.fillRoundedRectangle (r, 6.0f);
    const auto c = r.getCentre(); const float rad = jmin (r.getWidth(), r.getHeight()) * 0.46f;
    g.setColour (col::grid.brighter (0.2f));
    g.drawEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2, 1.0f);
    g.drawLine (c.x, c.y - rad, c.x, c.y + rad, 1.0f); g.drawLine (c.x - rad, c.y, c.x + rad, c.y, 1.0f);
    g.drawLine (c.x - rad * 0.707f, c.y - rad * 0.707f, c.x + rad * 0.707f, c.y + rad * 0.707f, 0.6f);
    g.drawLine (c.x + rad * 0.707f, c.y - rad * 0.707f, c.x - rad * 0.707f, c.y + rad * 0.707f, 0.6f);
    g.setColour (col::textDim); g.setFont (font (11.0f));
    g.drawText ("M", Rectangle<float> (c.x - 10, r.getY() + 4, 20, 14), Justification::centred, false);
    g.drawText ("L", Rectangle<float> (c.x - rad * 0.72f - 18, c.y - rad * 0.72f - 14, 16, 14), Justification::centred, false);
    g.drawText ("R", Rectangle<float> (c.x + rad * 0.72f + 2, c.y - rad * 0.72f - 14, 16, 14), Justification::centred, false);
    g.drawText ("+S", Rectangle<float> (r.getX() + 4, c.y - 16, 24, 14), Justification::left, false);
    g.drawText ("-S", Rectangle<float> (r.getRight() - 28, c.y - 16, 24, 14), Justification::right, false);
    float pk = 1.0e-4f;
    for (size_t i = 0; i < L.size(); ++i) pk = jmax (pk, std::abs (L[i]), std::abs (R[i]));
    const float sc = rad / jmax (0.05f, pk) * 0.7f;
    const auto dot = modColour (Mod::Imager);
    for (size_t i = 0; i < L.size(); i += 1)
    {
        const float m = (L[i] + R[i]) * 0.7071f, s = (L[i] - R[i]) * 0.7071f;
        const float x = c.x - s * sc, y = c.y - m * sc;
        g.setColour (dot.withAlpha (0.35f)); g.fillRect (x, y, 1.6f, 1.6f);
    }
}

ImagerPage::ImagerPage (MasterSuiteProcessor& p, Analyzer& a) : Page (p, Mod::Imager), graph (p, a, modColour (Mod::Imager)), scope (p), an (a)
{
    addAndMakeVisible (graph); addAndMakeVisible (scope);
    for (int k = 0; k < 4; ++k)
    {
        auto* s = strips.add (new BandStrip (p, Mod::Imager, k, modColour (Mod::Imager)));
        addAndMakeVisible (s);
        s->meter = [this, k] (Graphics& g, Rectangle<int> r)
        {
            const float v = bandCorr[k];
            g.setColour (col::screen); g.fillRoundedRectangle (r.toFloat(), 3);
            const float cx = (float) r.getCentreX(), x = jmap (v, -1.0f, 1.0f, (float) r.getX() + 2, (float) r.getRight() - 2);
            g.setColour (v < 0 ? col::warn : (v < 0.3f ? Colour (0xffffb52e) : col::ok));
            g.fillRect (Rectangle<float> (jmin (cx, x), (float) r.getY() + 3, std::abs (x - cx), (float) r.getHeight() - 6));
            g.setColour (col::text); g.setFont (font (10.5f));
            g.drawText ("CORR " + String (v, 2), r, Justification::centred, false);
        };
    }
    graph.overlay = [this] (Graphics& g, const FreqAxis& ax, Rectangle<float> r)
    {
        // stereo width per frequency: side energy share (0 = mono, 1 = all side)
        Path p; const float top = r.getBottom() - r.getHeight() * 0.42f, bot = r.getBottom() - 2;
        for (int i = 0; i < Analyzer::P; ++i)
        {
            const float m = std::pow (10.0f, an.postMid[(size_t) i] / 10.0f), s = std::pow (10.0f, an.postSide[(size_t) i] / 10.0f);
            const float w = s / (m + s + 1.0e-12f);
            const float x = ax.x (an.freqAt (i)), y = jmap (w, 0.0f, 0.75f, bot, top);
            if (i == 0) p.startNewSubPath (x, jmax (top, y)); else p.lineTo (x, jmax (top, y));
        }
        Path f (p); f.lineTo (r.getRight(), bot); f.lineTo (r.getX(), bot); f.closeSubPath();
        g.setColour (modColour (Mod::Imager).withAlpha (0.25f)); g.fillPath (f);
        g.setColour (Colours::white.withAlpha (0.85f)); g.strokePath (p, PathStrokeType (1.6f));
        g.setColour (col::textDim); g.setFont (font (10.5f));
        g.drawText ("WIDTH BY FREQUENCY  (bottom = mono, top = wide)", Rectangle<float> (r.getX() + 6, top - 14, 300, 12), Justification::left, false);
        g.setColour (col::grid.brighter (0.4f)); g.drawHorizontalLine (roundToInt (top), r.getX(), r.getRight());
    };
    graph.bandText = [this] (int k) { return String (roundToInt (proc.get (ids::b ("im", k + 1, "width")))) + " %"; };
}
void ImagerPage::resized()
{
    auto r = content();
    auto top = r.removeFromTop (330);
    auto left = top.removeFromLeft (300);
    scope.setBounds (left.removeFromTop (226));
    left.removeFromTop (22);
    corrArea = left.removeFromTop (28);
    left.removeFromTop (6);
    phaseArea = left;
    top.removeFromLeft (10);
    graph.setBounds (top);
    r.removeFromTop (10);
    const int w = (r.getWidth() - 30) / 4;
    for (int k = 0; k < 4; ++k) strips[k]->setBounds (r.getX() + k * (w + 10), r.getY(), w, r.getHeight());
}
void ImagerPage::paint (Graphics& g)
{
    Page::paint (g);
    // overall correlation meter
    auto c = corrArea.toFloat();
    g.setColour (col::screen); g.fillRoundedRectangle (c, 4);
    const float cx = c.getCentreX(), x = jmap (corr, -1.0f, 1.0f, c.getX() + 3, c.getRight() - 3);
    g.setGradientFill (ColourGradient (col::warn, c.getX(), 0, col::ok, c.getRight(), 0, false));
    g.fillRect (Rectangle<float> (jmin (cx, x), c.getY() + 4, std::abs (x - cx), c.getHeight() - 8));
    g.setColour (Colours::white); g.fillRect (x - 1.5f, c.getY() + 2, 3.0f, c.getHeight() - 4);
    g.setColour (col::textDim); g.setFont (font (10.0f));
    g.drawText ("-1", c.reduced (4, 0), Justification::centredLeft, false);
    g.drawText ("+1", c.reduced (4, 0), Justification::centredRight, false);
    g.drawText ("0", c, Justification::centred, false);
    // phase status
    auto pa = phaseArea.toFloat();
    String msg; Colour mc;
    if (corr < 0.0f || worst < -0.2f) { msg = "PHASE PROBLEM\nmono will cancel - check the " + String (worst < -0.2f ? "red band" : "stereo source"); mc = col::warn; }
    else if (corr < 0.3f || worst < 0.15f) { msg = "VERY WIDE\ncheck it in mono"; mc = Colour (0xffffb52e); }
    else { msg = "PHASE OK\nmono compatible"; mc = col::ok; }
    g.setColour (mc.withAlpha (0.12f)); g.fillRoundedRectangle (pa, 5);
    g.setColour (mc); g.drawRoundedRectangle (pa.reduced (0.5f), 5, 1.2f);
    g.setFont (font (13.0f)); g.drawFittedText (msg, pa.toNearestInt().reduced (8, 2), Justification::centred, 2);
    g.setColour (col::text); g.setFont (font (11.0f));
    g.drawText ("CORRELATION  " + String (corr, 2), corrArea.translated (0, -16).withHeight (14), Justification::centredLeft, false);
}
void ImagerPage::tick()
{
    scope.pull(); scope.repaint(); graph.repaint();
    corr += (proc.corrAll.load() - corr) * 0.3f;
    worst = 1.0f;
    for (int k = 0; k < 4; ++k) { bandCorr[k] += (proc.corr[k].load() - bandCorr[k]) * 0.3f; worst = jmin (worst, bandCorr[k]); }
    for (auto* s : strips) s->repaint();
    repaint (corrArea.expanded (2, 18)); repaint (phaseArea);
}

//==============================================================================
LoudnessPage::LoudnessPage (MasterSuiteProcessor& p)
    : Page (p, Mod::Loudness),
      gain (p.apvts, modColour (Mod::Loudness), "GAIN", "lu_gain"), ceiling (p.apvts, modColour (Mod::Loudness), "CEILING", "lu_ceiling"),
      release (p.apvts, modColour (Mod::Loudness), "RELEASE", "lu_release"), custom (p.apvts, modColour (Mod::Loudness), "CUSTOM TARGET", "lu_custom", true),
      follow (p.apvts, "CEILING = PLATFORM TP", modColour (Mod::Loudness), "lu_follow")
{
    for (int i = 0; i < (int) ms::targets().size(); ++i)
    {
        auto* b = targetBtns.add (new TextButton (ms::targets()[(size_t) i].name));
        b->setColour (TextButton::buttonOnColourId, modColour (Mod::Loudness));
        b->onClick = [this, i]
        {
            if (targetAtt) targetAtt->setValueAsCompleteGesture ((float) i);
            if (proc.get ("lu_follow") > 0.5f) proc.setParam ("lu_ceiling", ms::targets()[(size_t) i].tp);
        };
        addAndMakeVisible (b);
    }
    targetAtt = std::make_unique<ParameterAttachment> (*p.apvts.getParameter ("lu_target"), [this] (float v)
    {
        for (int i = 0; i < targetBtns.size(); ++i) targetBtns[i]->setToggleState (i == roundToInt (v), dontSendNotification);
        repaint();
    });
    targetAtt->sendInitialUpdate();
    for (auto* c : std::initializer_list<Component*> { &gain, &ceiling, &release, &custom, &follow, &match, &resetBtn }) addAndMakeVisible (c);
    match.setColour (TextButton::buttonOnColourId, modColour (Mod::Loudness));
    match.onClick = [this] { status = proc.matchTarget() ? "Gain set for the target - keep playing to confirm" : "Play the song for a few seconds first"; repaint(); };
    resetBtn.onClick = [this] { proc.meter.requestReset(); std::fill (hist.begin(), hist.end(), -100.0f); status = {}; };
}
void LoudnessPage::resized()
{
    auto r = content();
    auto left = r.removeFromLeft (250);
    left.removeFromTop (26);
    const int bh = 36;
    for (int i = 0; i < targetBtns.size(); ++i)
        targetBtns[i]->setBounds (left.getX() + (i % 2) * (left.getWidth() / 2 + 2), left.getY() + (i / 2) * (bh + 6), left.getWidth() / 2 - 4, bh);
    custom.setBounds (left.getX() + 50, left.getY() + 5 * (bh + 6) + 6, 150, 120);
    r.removeFromLeft (12);
    maxArea = r.removeFromRight (250);
    r.removeFromRight (12);
    readArea = r.removeFromTop (290);
    r.removeFromTop (10);
    histArea = r;
    auto m = maxArea.reduced (10); m.removeFromTop (26);
    auto k1 = m.removeFromTop (130);
    gain.setBounds (k1.removeFromLeft (k1.getWidth() / 2).reduced (4, 0)); ceiling.setBounds (k1.reduced (4, 0));
    auto k2 = m.removeFromTop (124);
    release.setBounds (k2.withSizeKeepingCentre (110, 124));
    m.removeFromTop (10);
    follow.setBounds (m.removeFromTop (28));
    m.removeFromTop (10);
    match.setBounds (m.removeFromTop (40));
    m.removeFromTop (8);
    resetBtn.setBounds (m.removeFromTop (28));
}
void LoudnessPage::paint (Graphics& g)
{
    Page::paint (g);
    auto r = content();
    auto left = r.removeFromLeft (250);
    g.setColour (modColour (Mod::Loudness)); g.setFont (font (11.5f));
    g.drawText ("TARGET PLATFORM", left.removeFromTop (20), Justification::left, false);
    panel (g, maxArea, "MAXIMIZER  (true-peak limiter)");
    // limiter GR
    {
        auto gm = maxArea.reduced (10).removeFromBottom (26);
        g.setColour (col::screen); g.fillRoundedRectangle (gm.toFloat(), 3);
        auto f = gm.toFloat().reduced (2); f = f.withWidth (f.getWidth() * jlimit (0.0f, 1.0f, gr / 12.0f));
        g.setColour (modColour (Mod::Loudness)); g.fillRoundedRectangle (f, 2);
        g.setColour (col::text); g.setFont (font (11.0f));
        g.drawText ("LIMITER GR -" + String (gr, 1) + " dB", gm, Justification::centred, false);
    }

    const int ti = (int) proc.get ("lu_target");
    const auto& t = ms::targets()[(size_t) jlimit (0, (int) ms::targets().size() - 1, ti)];
    const float target = String (t.name) == "Custom" ? proc.get ("lu_custom") : t.lufs;
    const float I = proc.meter.integrated.load(), S = proc.meter.shortTerm.load(), M = proc.meter.momentary.load();
    const float lra = proc.meter.lra.load(), tp = proc.meter.truePeakMax.load();

    panel (g, readArea, "LOUDNESS  (EBU R128 / ITU BS.1770)");
    auto ra = readArea.reduced (16); ra.removeFromTop (20);
    auto big = ra.removeFromTop (70);
    g.setColour (col::textDim); g.setFont (font (12.0f));
    g.drawText ("INTEGRATED", big.removeFromTop (16), Justification::left, false);
    g.setColour (col::text); g.setFont (font (50.0f));
    g.drawText (I > -70 ? String (I, 1) : String ("--.-"), big.removeFromLeft (160), Justification::left, false);
    g.setFont (font (16.0f)); g.setColour (col::textDim);
    g.drawText ("LUFS", big.removeFromLeft (60).withTrimmedTop (18), Justification::left, false);
    {
        auto tr = ra.removeFromTop (66);
        g.setColour (modColour (Mod::Loudness)); g.setFont (font (14.0f));
        g.drawText (String (t.name).toUpperCase() + " TARGET:  " + String (target, 1) + " LUFS   /   " + String (t.tp, 1) + " dBTP", tr.removeFromTop (22), Justification::left, false);
        if (I > -70)
        {
            const float d = I - target;
            String s; Colour c;
            if (std::abs (d) <= 0.5f) { s = "ON TARGET"; c = col::ok; }
            else if (d > 0) { s = "+" + String (d, 1) + " LU too loud - " + String (t.name) + " will turn it down " + String (d, 1) + " dB"; c = Colour (0xffffb52e); }
            else { s = String (d, 1) + " LU under target - you can push " + String (-d, 1) + " dB more"; c = Colour (0xff7fc8ff); }
            g.setColour (c); g.setFont (font (13.0f)); g.drawFittedText (s, tr.removeFromTop (40), Justification::topLeft, 2);
        }
        else { g.setColour (col::textDim); g.setFont (font (13.0f, false)); g.drawText ("play the song to measure", tr.removeFromTop (20), Justification::left, false); }
    }
    ra.removeFromTop (4);
    const int cw = ra.getWidth() / 4;
    auto cell = [&] (int i, const String& name, const String& v, const String& unit, Colour c)
    {
        Rectangle<int> b (ra.getX() + i * cw, ra.getY(), cw - 8, 92);
        g.setColour (col::screen); g.fillRoundedRectangle (b.toFloat(), 5);
        g.setColour (col::textDim); g.setFont (font (11.0f)); g.drawText (name, b.reduced (10, 6).removeFromTop (14), Justification::left, false);
        g.setColour (c); g.setFont (font (28.0f)); g.drawText (v, b.reduced (10, 0).withTrimmedTop (22).removeFromTop (40), Justification::left, false);
        g.setColour (col::textDim); g.setFont (font (11.0f, false)); g.drawText (unit, b.reduced (10, 6).removeFromBottom (14), Justification::left, false);
    };
    cell (0, "SHORT-TERM", S > -70 ? String (S, 1) : "--", "LUFS  3 s", col::text);
    cell (1, "MOMENTARY", M > -70 ? String (M, 1) : "--", "LUFS  0.4 s", col::text);
    cell (2, "RANGE (LRA)", String (lra, 1), "LU", col::text);
    cell (3, "TRUE PEAK", tp > -90 ? String (tp, 1) : "--", tp > t.tp + 0.05f ? "dBTP  OVER!" : "dBTP max", tp > t.tp + 0.05f ? col::warn : col::ok);

    // history
    panel (g, histArea, "SHORT-TERM HISTORY (last 30 s)");
    auto h = histArea.reduced (12).withTrimmedTop (16).toFloat();
    g.setColour (col::screen); g.fillRoundedRectangle (h, 4);
    const float top = -4, bot = -40;
    g.setColour (col::grid);
    for (float d = -8; d >= bot; d -= 4) { const float y = jmap (d, top, bot, h.getY(), h.getBottom()); g.drawHorizontalLine (roundToInt (y), h.getX(), h.getRight());
        g.setColour (col::textDim.withAlpha (0.5f)); g.setFont (font (9.5f, false)); g.drawText (String (roundToInt (d)), Rectangle<float> (h.getX() + 2, y - 6, 24, 12), Justification::left, false); g.setColour (col::grid); }
    const float ty = jmap (jlimit (bot, top, target), top, bot, h.getY(), h.getBottom());
    g.setColour (modColour (Mod::Loudness)); g.drawHorizontalLine (roundToInt (ty), h.getX(), h.getRight());
    g.setFont (font (10.0f)); g.drawText ("TARGET", Rectangle<float> (h.getRight() - 60, ty - 13, 56, 12), Justification::right, false);
    Path p; bool started = false;
    for (int i = 0; i < (int) hist.size(); ++i)
    {
        const float v = hist[(size_t) ((hpos + i) % (int) hist.size())];
        if (v < -70) { started = false; continue; }
        const float x = h.getX() + h.getWidth() * (float) i / (float) (hist.size() - 1), y = jmap (jlimit (bot, top, v), top, bot, h.getY(), h.getBottom());
        if (! started) { p.startNewSubPath (x, y); started = true; } else p.lineTo (x, y);
    }
    g.setColour (col::text.withAlpha (0.9f)); g.strokePath (p, PathStrokeType (1.6f));
    if (status.isNotEmpty())
    {
        g.setColour (modColour (Mod::Loudness)); g.setFont (font (12.0f));
        g.drawText (status, histArea.reduced (14, 8), Justification::topRight, false);
    }
    custom.setEnabled (String (t.name) == "Custom");
    custom.setAlpha (custom.isEnabled() ? 1.0f : 0.35f);
}
void LoudnessPage::tick()
{
    const float v = proc.limGr.exchange (0.0f); gr = v > gr ? v : gr * 0.85f;
    if (++frame % 1 == 0) { hist[(size_t) hpos] = proc.meter.shortTerm.load(); hpos = (hpos + 1) % (int) hist.size(); }
    repaint();
}

//==============================================================================
MasterMeter::MasterMeter (MasterSuiteProcessor& p)
    : proc (p), inTrim (p.apvts, Colour (0xffe8e2d4), "INPUT", "in_gain", true), outTrim (p.apvts, Colour (0xffe8e2d4), "OUTPUT", "out_gain", true)
{
    addAndMakeVisible (inTrim); addAndMakeVisible (outTrim); addAndMakeVisible (reset);
    reset.onClick = [this] { proc.meter.requestReset(); };
}
void MasterMeter::resized()
{
    auto r = getLocalBounds().reduced (12);
    auto b = r.removeFromBottom (120);
    inTrim.setBounds (b.removeFromLeft (b.getWidth() / 2)); outTrim.setBounds (b);
    reset.setBounds (getWidth() - 12 - 64, 12, 64, 22);
}
void MasterMeter::tick()
{
    auto ph = [] (float& v, float n, float fall) { v = n > v ? n : v - fall; };
    ph (inPk, proc.inPeak.exchange (-100.0f), 1.2f); ph (outPk, proc.outPeak.exchange (-100.0f), 1.2f);
    const float g = proc.limGr.load(); limGr = g > limGr ? g : limGr * 0.9f;
    corr += (proc.corrAll.load() - corr) * 0.3f;
    repaint();
}
void MasterMeter::paint (Graphics& g)
{
    auto rf = getLocalBounds().toFloat();
    g.setColour (Colour (0xff16191d)); g.fillRoundedRectangle (rf, 8.0f);
    g.setColour (col::edge); g.drawRoundedRectangle (rf.reduced (0.5f), 8.0f, 1.0f);
    auto r = getLocalBounds().reduced (12);
    g.setColour (modColour (Mod::Loudness)); g.setFont (font (13.0f));
    g.drawText ("MASTER METER", r.removeFromTop (22), Justification::left, false);
    const int ti = (int) proc.get ("lu_target");
    const auto& t = ms::targets()[(size_t) jlimit (0, (int) ms::targets().size() - 1, ti)];
    const float target = String (t.name) == "Custom" ? proc.get ("lu_custom") : t.lufs;
    const float I = proc.meter.integrated.load(), S = proc.meter.shortTerm.load(), M = proc.meter.momentary.load(), tp = proc.meter.truePeakMax.load();
    r.removeFromTop (6);
    auto big = r.removeFromTop (86);
    g.setColour (col::screen); g.fillRoundedRectangle (big.toFloat(), 6);
    g.setColour (col::textDim); g.setFont (font (11.0f)); g.drawText ("INTEGRATED LUFS", big.reduced (10, 6).removeFromTop (14), Justification::left, false);
    const bool onT = I > -70 && std::abs (I - target) <= 0.5f;
    g.setColour (I > -70 ? (onT ? col::ok : col::text) : col::textDim); g.setFont (font (38.0f));
    g.drawText (I > -70 ? String (I, 1) : "--.-", big.reduced (10, 0).withTrimmedTop (18).removeFromTop (44), Justification::left, false);
    g.setColour (modColour (Mod::Loudness)); g.setFont (font (11.0f));
    g.drawText (String (t.name) + "  " + String (target, 0) + " LUFS", big.reduced (10, 6).removeFromBottom (14), Justification::left, false);
    r.removeFromTop (8);
    auto line = [&] (const String& n, const String& v, Colour c)
    {
        auto l = r.removeFromTop (22);
        g.setColour (col::textDim); g.setFont (font (11.5f)); g.drawText (n, l, Justification::left, false);
        g.setColour (c); g.setFont (font (13.0f)); g.drawText (v, l, Justification::right, false);
    };
    line ("SHORT-TERM", S > -70 ? String (S, 1) + " LUFS" : "--", col::text);
    line ("MOMENTARY", M > -70 ? String (M, 1) + " LUFS" : "--", col::text);
    line ("TRUE PEAK", tp > -90 ? String (tp, 1) + " dBTP" : "--", tp > t.tp + 0.05f ? col::warn : col::text);
    line ("LRA", String (proc.meter.lra.load(), 1) + " LU", col::text);
    r.removeFromTop (10);
    // in / out bars
    auto bars = r.removeFromTop (230);
    auto bar = [&] (Rectangle<int> b, float db, const String& name, Colour c, bool down)
    {
        g.setColour (col::textDim); g.setFont (font (10.5f)); g.drawText (name, b.removeFromBottom (14), Justification::centred, false);
        g.setColour (col::screen); g.fillRoundedRectangle (b.toFloat(), 3);
        float frac = down ? jlimit (0.0f, 1.0f, db / 12.0f) : jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f);
        auto f = b.toFloat().reduced (2);
        if (down) f = f.withHeight (f.getHeight() * frac);
        else f = f.withTop (f.getBottom() - f.getHeight() * frac);
        g.setColour (! down && db > -0.5f ? col::warn : c); g.fillRoundedRectangle (f, 2);
    };
    const int bw = (bars.getWidth() - 20) / 3;
    bar (bars.removeFromLeft (bw), inPk, "IN", Colour (0xffc8ced6), false); bars.removeFromLeft (10);
    bar (bars.removeFromLeft (bw), limGr, "LIM GR", modColour (Mod::Loudness), true); bars.removeFromLeft (10);
    bar (bars.removeFromLeft (bw), outPk, "OUT", Colour (0xffc8ced6), false);
    r.removeFromTop (10);
    auto cr = r.removeFromTop (34);
    g.setColour (col::textDim); g.setFont (font (10.5f)); g.drawText ("CORRELATION " + String (corr, 2), cr.removeFromTop (14), Justification::left, false);
    auto c = cr.toFloat();
    g.setColour (col::screen); g.fillRoundedRectangle (c, 3);
    const float cx = c.getCentreX(), x = jmap (corr, -1.0f, 1.0f, c.getX() + 2, c.getRight() - 2);
    g.setColour (corr < 0 ? col::warn : col::ok); g.fillRect (Rectangle<float> (jmin (cx, x), c.getY() + 3, std::abs (x - cx), c.getHeight() - 6));
}

//==============================================================================
ModuleTabs::ModuleTabs (MasterSuiteProcessor& p) : proc (p) { startTimerHz (10); }

/** only modules that are in the chain get a tab; the rest live behind "+ ADD MODULE" */
std::vector<int> ModuleTabs::visible() const
{
    std::vector<int> v;
    for (int i = 0; i < (int) Mod::Count; ++i) if (proc.isInserted ((Mod) i)) v.push_back (i);
    return v;
}
bool ModuleTabs::canAdd() const { return visible().size() < (size_t) Mod::Count; }
Rectangle<float> ModuleTabs::addButton() const
{
    if (! canAdd()) return {};
    const auto v = visible(); const float gap = 12.0f, addW = 150.0f;
    const float w = jmin (200.0f, ((float) getWidth() - addW - gap * (float) v.size()) / (float) v.size());
    return { (float) v.size() * (w + gap), 0.0f, addW, (float) getHeight() };
}
Rectangle<float> ModuleTabs::tab (int slot) const
{
    const auto v = visible(); const float gap = 12.0f, addW = canAdd() ? 150.0f : 0.0f;
    const float n = (float) v.size();
    const float w = jmin (200.0f, ((float) getWidth() - addW - gap * (canAdd() ? n : n - 1)) / n);
    return { (float) slot * (w + gap), 0.0f, w, (float) getHeight() };
}
Rectangle<float> ModuleTabs::led (int slot) const { auto t = tab (slot); return { t.getX() + 10, t.getCentreY() - 8, 16, 16 }; }
Rectangle<float> ModuleTabs::cross (int slot) const { auto t = tab (slot); return { t.getRight() - 22, t.getY() + 4, 16, 16 }; }

void ModuleTabs::paint (Graphics& g)
{
    const auto v = visible();
    for (int s = 0; s < (int) v.size(); ++s)
    {
        const int i = v[(size_t) s];
        const auto m = (Mod) i; const auto t = tab (s); const auto c = modColour (m);
        const bool on = proc.get (String (modKey (m)) + "_on") > 0.5f;
        const bool sel = i == selected;
        g.setColour (sel ? col::panelHi : (s == hover ? Colour (0xff1f2328) : col::panel));
        g.fillRoundedRectangle (t, 6);
        g.setColour (sel ? c : col::edge); g.drawRoundedRectangle (t.reduced (0.5f), 6, sel ? 1.6f : 1.0f);
        if (sel) { g.setColour (c); g.fillRoundedRectangle (t.withTop (t.getBottom() - 4).reduced (8, 0), 2); }
        if (s < (int) v.size() - 1)
        {
            g.setColour (col::textDim.withAlpha (0.6f));
            Path a; const float ax = t.getRight() + 2, ay = t.getCentreY(); a.addTriangle (ax, ay - 4, ax + 8, ay, ax, ay + 4); g.fillPath (a);
        }
        const auto l = led (s);
        if (on) { g.setColour (c.withAlpha (0.3f)); g.fillEllipse (l.expanded (4)); }
        g.setColour (on ? c : Colour (0xff3b4047)); g.fillEllipse (l);
        g.setColour (Colours::black.withAlpha (0.6f)); g.drawEllipse (l, 1.0f);
        g.setColour (on ? col::text : col::textDim); g.setFont (font (14.0f));
        g.drawText (modName (m), t.withTrimmedLeft (34).withTrimmedRight (20).withTrimmedBottom (14), Justification::centredLeft, true);
        g.setColour (col::textDim.withAlpha (0.7f)); g.setFont (font (9.5f, false));
        g.drawText (modIsCore (m) ? (on ? "core" : "core - bypassed") : (on ? "add-on" : "add-on - bypassed"),
                    t.withTrimmedLeft (34).withTrimmedTop (24), Justification::centredLeft, true);
        if (! modIsCore (m))
        {
            const auto x = cross (s);
            g.setColour (col::textDim.withAlpha (hover == s ? 1.0f : 0.5f));
            g.drawLine (x.getX() + 4, x.getY() + 4, x.getRight() - 4, x.getBottom() - 4, 1.6f);
            g.drawLine (x.getRight() - 4, x.getY() + 4, x.getX() + 4, x.getBottom() - 4, 1.6f);
        }
    }
    if (canAdd())
    {
        const auto a = addButton();
        g.setColour (hover == 99 ? Colour (0xff23272d) : Colour (0xff181b1f)); g.fillRoundedRectangle (a, 6);
        g.setColour (col::textDim.withAlpha (0.6f));
        Path dash; dash.addRoundedRectangle (a.reduced (0.5f), 6);
        float dl[] = { 5.0f, 4.0f }; PathStrokeType (1.0f).createDashedStroke (dash, dash, dl, 2);
        g.fillPath (dash);
        g.setColour (hover == 99 ? col::text : col::textDim); g.setFont (font (13.5f));
        g.drawText ("+  ADD MODULE", a, Justification::centred, false);
    }
}
void ModuleTabs::mouseMove (const MouseEvent& e)
{
    int h = -1; const auto v = visible();
    for (int s = 0; s < (int) v.size(); ++s) if (tab (s).contains (e.position)) h = s;
    if (canAdd() && addButton().contains (e.position)) h = 99;
    if (h != hover) { hover = h; repaint(); }
}
void ModuleTabs::mouseDown (const MouseEvent& e)
{
    if (canAdd() && addButton().contains (e.position))
    {
        PopupMenu menu;
        for (int i = 0; i < (int) Mod::Count; ++i)
            if (! proc.isInserted ((Mod) i)) menu.addItem (i + 1, String ("Add ") + modName ((Mod) i));
        menu.showMenuAsync (PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (localAreaToGlobal (addButton().toNearestInt())),
                            [this] (int r)
                            {
                                if (r <= 0) return;
                                proc.setInserted ((Mod) (r - 1), true);
                                selected = r - 1; if (onSelect) onSelect (selected); repaint();
                            });
        return;
    }
    const auto v = visible();
    for (int s = 0; s < (int) v.size(); ++s)
    {
        if (! tab (s).contains (e.position)) continue;
        const int i = v[(size_t) s]; const auto m = (Mod) i;
        if (! modIsCore (m) && cross (s).expanded (4).contains (e.position))
        {
            proc.setInserted (m, false);   // removed from the chain and from the screen
            if (selected == i) { selected = 0; if (onSelect) onSelect (0); }
            hover = -1; repaint(); return;
        }
        if (led (s).expanded (6).contains (e.position))
        {
            const String id = String (modKey (m)) + "_on";
            proc.setParam (id, proc.get (id) > 0.5f ? 0.0f : 1.0f); repaint(); return;
        }
        selected = i; if (onSelect) onSelect (i); repaint(); return;
    }
}
} // namespace ui

//==============================================================================
MasterSuiteEditor::MasterSuiteEditor (MasterSuiteProcessor& p)
    : AudioProcessorEditor (&p), proc (p), tabs (p), meter (p),
      listen (p.apvts, StringArray { "STEREO", "MID", "SIDE", "LEFT", "RIGHT" }, "listen", true)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (root);
    root.addAndMakeVisible (tabs); root.addAndMakeVisible (meter); root.addAndMakeVisible (listen); root.addAndMakeVisible (presetBox);
    pages.add (new ui::EqPage (p, analyzer));
    pages.add (new ui::MultiPage (p, analyzer, Mod::Rebalance));
    pages.add (new ui::MultiPage (p, analyzer, Mod::Dynamics));
    pages.add (new ui::MultiPage (p, analyzer, Mod::Exciter));
    pages.add (new ui::ImagerPage (p, analyzer));
    pages.add (new ui::LoudnessPage (p));
    for (auto* pg : pages) root.addChildComponent (pg);
    tabs.onSelect = [this] (int i) { showPage (i); };
    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, dontSendNotification);
    presetBox.onChange = [this] { const int id = presetBox.getSelectedId(); if (id > 0) proc.setCurrentProgram (id - 1); };
    showPage (0);
    setResizable (true, true);
    setResizeLimits (W * 3 / 5, H * 3 / 5, W * 2, H * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) W / (double) H);
    setSize (W, H);
    startTimerHz (30);
}
MasterSuiteEditor::~MasterSuiteEditor() { stopTimer(); setLookAndFeel (nullptr); }

void MasterSuiteEditor::showPage (int i)
{
    if (! proc.isInserted ((Mod) i)) proc.setInserted ((Mod) i, true);
    current = i; tabs.selected = i; tabs.repaint();
    for (int k = 0; k < pages.size(); ++k) pages[k]->setVisible (k == i);
}

void MasterSuiteEditor::resized()
{
    root.setBounds (0, 0, W, H);
    root.setTransform (AffineTransform::scale ((float) getWidth() / (float) W));
    tabs.setBounds (14, 70, 996, 52);
    for (auto* pg : pages) pg->setBounds (14, 130, 996, 656);
    meter.setBounds (1022, 70, 224, 716);
    presetBox.setBounds (470, 18, 300, 30);
    listen.setBounds (W - 14 - 380, 18, 380, 30);
}

void MasterSuiteEditor::paint (Graphics& g)
{
    const float s = (float) getWidth() / (float) W;
    g.addTransform (AffineTransform::scale (s));
    g.setGradientFill (ColourGradient (Colour (0xff1a1d22), 0, 0, Colour (0xff0d0f12), 0, (float) H, false));
    g.fillRect (0, 0, W, H);
    g.setColour (Colour (0xff2a2f36)); g.fillRect (0, 62, W, 1);
    g.setGradientFill (ColourGradient (Colour (0xfffff4d6), 0, 14, Colour (0xffffb52e), 0, 46, false));
    g.setFont (ui::font (26.0f));
    g.drawText ("MASTER SUITE", 22, 12, 260, 30, Justification::centredLeft, false);
    g.setColour (ui::col::textDim); g.setFont (ui::font (11.0f));
    g.drawText ("ROHIT JI  |  BEAT KI PATHSHALA  |  MASTERING", 24, 40, 360, 16, Justification::centredLeft, false);
    g.drawText ("PRESET", 400, 18, 62, 30, Justification::centredRight, false);
    g.drawText ("LISTEN", W - 14 - 380 - 70, 18, 62, 30, Justification::centredRight, false);
    if (listen.value() != 0)
    {
        g.setColour (ui::chanColour (listen.value())); g.setFont (ui::font (11.0f));
        g.drawText ("monitoring " + ms::chanNames()[listen.value()] + " only", W - 14 - 380, 50, 380, 12, Justification::centred, false);
    }
}

void MasterSuiteEditor::timerCallback()
{
    analyzer.update (proc, proc.getFs());
    if (! proc.isInserted ((Mod) current)) showPage (0);
    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1) presetBox.setSelectedId (proc.getCurrentProgram() + 1, dontSendNotification);
    pages[current]->tick();
    meter.tick();
    repaint (0, 48, W, 14);
}
