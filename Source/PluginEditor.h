#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace ui
{
namespace col
{
    const juce::Colour bg       { 0xff121417 };
    const juce::Colour panel    { 0xff1b1e23 };
    const juce::Colour panelHi  { 0xff23272d };
    const juce::Colour edge     { 0xff30353d };
    const juce::Colour screen   { 0xff0a0c0f };
    const juce::Colour text     { 0xfff4f1ea };
    const juce::Colour textDim  { 0xff9aa3ad };
    const juce::Colour grid     { 0xff1e242b };
    const juce::Colour warn     { 0xffff5a47 };
    const juce::Colour ok       { 0xff6fe08a };
}
juce::Colour modColour (Mod m);
juce::Colour chanColour (int chan);
juce::Font font (float size, bool bold = true);

class LNF : public juce::LookAndFeel_V4
{
public:
    LNF();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (13.0f); }
    juce::Font getPopupMenuFont() override { return font (14.0f, false); }
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
};

using APVTS = juce::AudioProcessorValueTreeState;

/** Rotary knob with title and value read-out; can be re-bound to another parameter. */
class Knob : public juce::Component
{
public:
    Knob (APVTS& s, juce::Colour accent, const juce::String& title, const juce::String& id = {}, bool small = false);
    void bind (const juce::String& id, const juce::String& newTitle = {});
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Slider slider;
private:
    APVTS& state; juce::String title, pid; juce::Colour accent; bool small;
    std::unique_ptr<APVTS::SliderAttachment> att;
};

/** Row of buttons for a choice parameter (e.g. Stereo / Mid / Side / Left / Right). */
class Seg : public juce::Component
{
public:
    Seg (APVTS& s, const juce::StringArray& labels, const juce::String& id = {}, bool chanColours = false);
    void bind (const juce::String& id);
    void resized() override;
    int value() const { return current; }
    std::function<void (int)> onChange;
private:
    void update (int);
    APVTS& state; juce::String pid; bool chanCol;
    juce::OwnedArray<juce::TextButton> buttons;
    std::unique_ptr<juce::ParameterAttachment> att;
    int current = 0;
};

/** On/off button for a bool parameter. */
class Toggle : public juce::TextButton
{
public:
    Toggle (APVTS& s, const juce::String& label, juce::Colour on, const juce::String& id = {});
    void bind (const juce::String& id);
private:
    APVTS& state; std::unique_ptr<APVTS::ButtonAttachment> att;
};

//==============================================================================
/** Spectrum analysis of the processor's scope FIFOs (runs on the message thread). */
class Analyzer
{
public:
    static constexpr int order = 12, N = 1 << order, P = 220;     // P display points
    Analyzer();
    void update (MasterSuiteProcessor&, double fs);
    std::array<float, P> preMid, postMid, postSide;   // dB, tilted 4.5 dB/oct
    float freqAt (int i) const { return 20.0f * std::pow (1000.0f, (float) i / (float) (P - 1)); }
private:
    void compute (std::vector<float>& ring, int pos, std::array<float, P>& out, double fs);
    juce::dsp::FFT fft { order };
    juce::dsp::WindowingFunction<float> win { (size_t) N, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> rPreM, rPreS, rPostM, rPostS, work;
    int pos = 0; std::vector<float> tA, tB;
};

/** Log-frequency x axis helpers shared by the graphs. */
struct FreqAxis
{
    juce::Rectangle<float> r;
    float x (float f) const { return r.getX() + std::log (f / 20.0f) / std::log (1000.0f) * r.getWidth(); }
    float f (float x) const { return 20.0f * std::pow (1000.0f, juce::jlimit (0.0f, 1.0f, (x - r.getX()) / r.getWidth())); }
};
void drawFreqGrid (juce::Graphics&, const FreqAxis&, float dbTop, float dbBottom, float dbStep, bool labels);
void drawSpectrum (juce::Graphics&, const FreqAxis&, const std::array<float, Analyzer::P>&, const Analyzer&, juce::Colour, float topDb, float bottomDb, bool fill);

//==============================================================================
class EqGraph : public juce::Component
{
public:
    EqGraph (MasterSuiteProcessor&, Analyzer&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    int selected = 0;
    std::function<void (int)> onSelect;
private:
    juce::Point<float> nodePos (int band) const;
    int hit (juce::Point<float>) const;
    float yOf (float db) const; float dbOf (float y) const;
    MasterSuiteProcessor& proc; Analyzer& an;
    FreqAxis ax; int dragging = -1;
    static constexpr float range = 18.0f;
};

/** Spectrum with the 4 bands shaded and draggable crossover handles. */
class XoverGraph : public juce::Component
{
public:
    XoverGraph (MasterSuiteProcessor&, Analyzer&, juce::Colour accent);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    std::function<void (juce::Graphics&, const FreqAxis&, juce::Rectangle<float>)> overlay;
    std::function<float (int)> bandValue;   // optional per-band value text (e.g. gain)
    std::function<juce::String (int)> bandText;
private:
    float xo (int k) const;
    MasterSuiteProcessor& proc; Analyzer& an; juce::Colour accent;
    FreqAxis ax; int dragging = -1, hover = -1;
};

//==============================================================================
class Page : public juce::Component
{
public:
    Page (MasterSuiteProcessor& p, Mod m);
    void paint (juce::Graphics&) override;
    virtual void tick() {}
protected:
    juce::Rectangle<int> content() const { return getLocalBounds().reduced (14).withTrimmedTop (34); }
    void panel (juce::Graphics&, juce::Rectangle<int>, const juce::String& title) const;
    MasterSuiteProcessor& proc; Mod mod;
    Toggle power;
};

class EqPage : public Page
{
public:
    EqPage (MasterSuiteProcessor&, Analyzer&);
    void resized() override; void paint (juce::Graphics&) override; void tick() override;
private:
    void select (int band);
    EqGraph graph;
    juce::OwnedArray<juce::TextButton> bandBtns;
    juce::ComboBox type; std::unique_ptr<APVTS::ComboBoxAttachment> typeAtt;
    Seg chan; Knob freq, gain, q; Toggle bandOn; juce::TextButton listenBtn { "LISTEN" };
    int sel = 2;
    juce::Rectangle<int> lblType, lblChan, hintArea;
};

/** One column of controls for a band of a multiband module. */
class BandStrip : public juce::Component
{
public:
    BandStrip (MasterSuiteProcessor&, Mod, int band, juce::Colour);
    void resized() override; void paint (juce::Graphics&) override;
    juce::OwnedArray<Knob> knobs;
    std::unique_ptr<Toggle> on, solo; std::unique_ptr<Seg> chan; std::unique_ptr<juce::ComboBox> combo;
    std::unique_ptr<APVTS::ComboBoxAttachment> comboAtt;
    std::function<void (juce::Graphics&, juce::Rectangle<int>)> meter;
    int band; juce::Colour accent; MasterSuiteProcessor& proc;
};

class MultiPage : public Page
{
public:
    MultiPage (MasterSuiteProcessor&, Analyzer&, Mod);
    void resized() override; void paint (juce::Graphics&) override; void tick() override;
private:
    XoverGraph graph;
    juce::OwnedArray<BandStrip> strips;
    // dynamics glue section / exciter mix
    juce::OwnedArray<Knob> extraKnobs; std::unique_ptr<Seg> glueChan; std::unique_ptr<Toggle> glueOn;
    float gr[4] {}, glue = 0;
};

class Vectorscope : public juce::Component
{
public:
    explicit Vectorscope (MasterSuiteProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void pull();
private:
    MasterSuiteProcessor& proc;
    std::vector<float> L = std::vector<float> (2048), R = std::vector<float> (2048); int wpos = 0;
};

class ImagerPage : public Page
{
public:
    ImagerPage (MasterSuiteProcessor&, Analyzer&);
    void resized() override; void paint (juce::Graphics&) override; void tick() override;
private:
    XoverGraph graph; Vectorscope scope;
    juce::OwnedArray<BandStrip> strips;
    float corr = 1, bandCorr[4] { 1, 1, 1, 1 }, worst = 1;
    juce::Rectangle<int> corrArea, phaseArea;
    Analyzer& an;
};

class LoudnessPage : public Page
{
public:
    explicit LoudnessPage (MasterSuiteProcessor&);
    void resized() override; void paint (juce::Graphics&) override; void tick() override;
private:
    juce::OwnedArray<juce::TextButton> targetBtns; std::unique_ptr<juce::ParameterAttachment> targetAtt;
    Knob gain, ceiling, release, custom; Toggle follow;
    juce::TextButton match { "MATCH TARGET" }, resetBtn { "RESET METER" };
    std::vector<float> hist = std::vector<float> (900, -100.0f); int hpos = 0, frame = 0;
    float gr = 0; juce::String status;
    juce::Rectangle<int> readArea, histArea, maxArea;
};

/** Always-visible loudness / level column on the right. */
class MasterMeter : public juce::Component
{
public:
    explicit MasterMeter (MasterSuiteProcessor&);
    void paint (juce::Graphics&) override; void resized() override;
    void tick();
private:
    MasterSuiteProcessor& proc;
    Knob inTrim, outTrim; juce::TextButton reset { "RESET" };
    float inPk = -100, outPk = -100, limGr = 0, corr = 1;
};

/** Module chain tabs with power LEDs and add / remove for optional modules. */
class ModuleTabs : public juce::Component, private juce::Timer
{
public:
    explicit ModuleTabs (MasterSuiteProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    int selected = 0;
    std::function<void (int)> onSelect;
private:
    void timerCallback() override { repaint(); }
    juce::Rectangle<float> tab (int i) const;
    juce::Rectangle<float> led (int i) const;
    juce::Rectangle<float> cross (int i) const;
    MasterSuiteProcessor& proc; int hover = -1;
};
} // namespace ui

//==============================================================================
class MasterSuiteEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MasterSuiteEditor (MasterSuiteProcessor&);
    ~MasterSuiteEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void showPage (int);
    void tickForTest() { timerCallback(); }
    static constexpr int W = 1260, H = 800;

private:
    void timerCallback() override;
    MasterSuiteProcessor& proc;
    ui::LNF lnf;
    ui::Analyzer analyzer;
    juce::Component root;
    ui::ModuleTabs tabs;
    ui::MasterMeter meter;
    juce::OwnedArray<ui::Page> pages;
    ui::Seg listen;
    juce::ComboBox presetBox;
    int current = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterSuiteEditor)
};
