#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;
using ms::Chan;

//==============================================================================
namespace
{
    using F = AudioParameterFloat;
    using A = AudioParameterFloatAttributes;

    NormalisableRange<float> skew (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi); r.setSkewForCentre (centre); return r;
    }
    A hzAttr() { return A().withLabel ("Hz").withStringFromValueFunction ([] (float x, int) { return x >= 1000.0f ? String (x / 1000.0f, x >= 10000.0f ? 1 : 2) + " k" : String (roundToInt (x)) + " Hz"; }); }
    A dbAttr() { return A().withLabel ("dB").withStringFromValueFunction ([] (float x, int) { return (x > 0.04f ? "+" : "") + String (x, 1) + " dB"; }); }
    A msAttr() { return A().withLabel ("ms").withStringFromValueFunction ([] (float x, int) { return x < 10.0f ? String (x, 1) + " ms" : String (roundToInt (x)) + " ms"; }); }
    A pctAttr() { return A().withLabel ("%").withStringFromValueFunction ([] (float x, int) { return String (roundToInt (x)) + " %"; }); }
    A ratioAttr() { return A().withStringFromValueFunction ([] (float x, int) { return String (x, x < 10.0f ? 1 : 0) + " : 1"; }); }
    A qAttr() { return A().withStringFromValueFunction ([] (float x, int) { return "Q " + String (x, 2); }); }

    struct EqDef { int type; float f, g, q; };
    const EqDef eqDefaults[6] = { { 3, 30, 0, 0.7f }, { 1, 90, 0, 0.7f }, { 0, 280, 0, 0.9f }, { 0, 1500, 0, 0.9f }, { 0, 4500, 0, 0.9f }, { 2, 11000, 0, 0.7f } };

    struct Preset
    {
        const char* name;
        std::vector<std::pair<String, float>> values;
    };
    const std::vector<Preset>& presets()
    {
        static const std::vector<Preset> p =
        {
            { "Init - Clean Master", {} },
            { "Streaming -14 Balanced", { { "lu_target", 0 }, { "lu_gain", 4 }, { "glue_thr", -14 }, { "glue_ratio", 2 }, { "eq1_on", 1 }, { "eq1_freq", 28 },
                                          { "eq6_gain", 1.0f }, { "eq2_gain", 0.5f } } },
            { "Loud Pop / EDM", { { "lu_target", 8 }, { "lu_gain", 9 }, { "glue_thr", -16 }, { "glue_ratio", 3 }, { "dyn1_thr", -20 }, { "dyn1_ratio", 2.5f },
                                  { "ex_in", 1 }, { "ex2_drive", 20 }, { "ex4_drive", 25 }, { "ex4_type", 4 }, { "im_in", 1 }, { "im1_width", 0 }, { "im4_width", 125 } } },
            { "Warm Bollywood / Bhakti", { { "lu_target", 2 }, { "lu_gain", 5 }, { "eq2_gain", 1.5f }, { "eq4_gain", -1.0f }, { "eq6_gain", 1.5f },
                                           { "ex_in", 1 }, { "ex1_drive", 25 }, { "ex1_type", 2 }, { "ex2_drive", 30 }, { "ex2_type", 0 }, { "ex3_drive", 15 }, { "ex3_type", 1 } } },
            { "Wide & Airy", { { "im_in", 1 }, { "im1_width", 0 }, { "im3_width", 120 }, { "im4_width", 140 }, { "eq6_gain", 2.0f }, { "eq6_chan", 2 },
                               { "ex_in", 1 }, { "ex4_drive", 30 }, { "ex4_type", 4 }, { "ex4_chan", 2 }, { "lu_gain", 4 } } },
            { "Bass Mono + Tight Low End", { { "im_in", 1 }, { "im1_width", 0 }, { "dyn1_chan", 1 }, { "dyn1_thr", -22 }, { "dyn1_ratio", 3 }, { "dyn1_att", 25 },
                                             { "eq1_on", 1 }, { "eq1_freq", 30 }, { "eq1_chan", 0 }, { "lu_gain", 3 } } },
            { "Vocal Forward (Mid)", { { "eq4_gain", 1.5f }, { "eq4_chan", 1 }, { "eq4_freq", 2500 }, { "rb_in", 1 }, { "rb2_gain", 1.0f }, { "rb2_chan", 1 },
                                       { "dyn3_thr", -20 }, { "dyn3_ratio", 2 }, { "dyn3_chan", 1 }, { "lu_gain", 4 } } },
        };
        return p;
    }
}

//==============================================================================
AudioProcessorValueTreeState::ParameterLayout MasterSuiteProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> v;
    auto choice = [&] (const String& id, const String& name, const StringArray& opts, int def)
    { v.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, opts, def)); };
    auto toggle = [&] (const String& id, const String& name, bool def)
    { v.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };
    auto flt = [&] (const String& id, const String& name, NormalisableRange<float> r, float def, A a)
    { v.push_back (std::make_unique<F> (ParameterID { id, 1 }, name, r, def, a)); };

    // global
    flt ("in_gain", "Input Gain", NormalisableRange<float> (-18, 18, 0.1f), 0, dbAttr());
    flt ("out_gain", "Output Trim", NormalisableRange<float> (-18, 6, 0.1f), 0, dbAttr());
    choice ("listen", "Listen", StringArray { "Stereo", "Mid", "Side", "Left", "Right" }, 0);
    flt ("xo1", "Crossover Low", skew (40, 500, 140), 120, hzAttr());
    flt ("xo2", "Crossover Mid", skew (300, 5000, 1500), 1200, hzAttr());
    flt ("xo3", "Crossover High", skew (2000, 16000, 6000), 6000, hzAttr());

    // module on/off + insert (core modules are always inserted)
    for (int m = 0; m < (int) Mod::Count; ++m)
    {
        const String k = modKey ((Mod) m);
        toggle (k + "_on", String (modName ((Mod) m)) + " On", true);
        if (! modIsCore ((Mod) m)) toggle (k + "_in", String (modName ((Mod) m)) + " Inserted", false);
    }

    // EQ
    StringArray listenOpts { "Off" }; for (int i = 1; i <= 6; ++i) listenOpts.add ("Band " + String (i));
    choice ("eq_listen", "EQ Band Listen", listenOpts, 0);
    for (int i = 1; i <= 6; ++i)
    {
        const auto& d = eqDefaults[i - 1];
        const String n = "EQ " + String (i) + " ";
        toggle (ids::b ("eq", i, "on"), n + "On", i != 1);
        choice (ids::b ("eq", i, "type"), n + "Type", ms::eqTypeNames(), d.type);
        flt (ids::b ("eq", i, "freq"), n + "Freq", skew (20, 20000, 1000), d.f, hzAttr());
        flt (ids::b ("eq", i, "gain"), n + "Gain", NormalisableRange<float> (-18, 18, 0.05f), d.g, dbAttr());
        flt (ids::b ("eq", i, "q"), n + "Q", skew (0.1f, 18.0f, 1.0f), d.q, qAttr());
        choice (ids::b ("eq", i, "chan"), n + "Channel", ms::chanNames(), 0);
    }
    // Rebalance
    for (int i = 1; i <= 4; ++i)
    {
        const String n = "Rebalance " + String (i) + " ";
        toggle (ids::b ("rb", i, "on"), n + "On", true);
        toggle (ids::b ("rb", i, "solo"), n + "Solo", false);
        choice (ids::b ("rb", i, "chan"), n + "Channel", ms::chanNames(), 0);
        flt (ids::b ("rb", i, "gain"), n + "Level", NormalisableRange<float> (-12, 12, 0.05f), 0, dbAttr());
    }
    // Dynamics
    for (int i = 1; i <= 4; ++i)
    {
        const String n = "Dyn " + String (i) + " ";
        toggle (ids::b ("dyn", i, "on"), n + "On", true);
        toggle (ids::b ("dyn", i, "solo"), n + "Solo", false);
        choice (ids::b ("dyn", i, "chan"), n + "Channel", ms::chanNames(), 0);
        flt (ids::b ("dyn", i, "thr"), n + "Threshold", NormalisableRange<float> (-48, 0, 0.1f), -14, dbAttr());
        flt (ids::b ("dyn", i, "ratio"), n + "Ratio", skew (1, 10, 2), 1.5f, ratioAttr());
        flt (ids::b ("dyn", i, "att"), n + "Attack", skew (0.1f, 200, 15), i == 1 ? 30.0f : (i == 4 ? 8.0f : 15.0f), msAttr());
        flt (ids::b ("dyn", i, "rel"), n + "Release", skew (10, 2000, 200), i == 1 ? 250.0f : 150.0f, msAttr());
        flt (ids::b ("dyn", i, "mk"), n + "Makeup", NormalisableRange<float> (-12, 12, 0.05f), 0, dbAttr());
    }
    toggle ("glue_on", "Glue On", true);
    choice ("glue_chan", "Glue Channel", ms::chanNames(), 0);
    flt ("glue_thr", "Glue Threshold", NormalisableRange<float> (-40, 0, 0.1f), -10, dbAttr());
    flt ("glue_ratio", "Glue Ratio", skew (1, 10, 2), 2, ratioAttr());
    flt ("glue_att", "Glue Attack", skew (0.1f, 100, 10), 10, msAttr());
    flt ("glue_rel", "Glue Release", skew (50, 1500, 300), 300, msAttr());
    flt ("glue_mk", "Glue Makeup", NormalisableRange<float> (-6, 12, 0.05f), 0, dbAttr());
    flt ("glue_mix", "Glue Mix", NormalisableRange<float> (0, 100), 100, pctAttr());
    flt ("glue_hpf", "Glue Sidechain HPF", skew (20, 300, 90), 90, hzAttr());
    // Exciter
    flt ("ex_mix", "Exciter Mix", NormalisableRange<float> (0, 100), 100, pctAttr());
    for (int i = 1; i <= 4; ++i)
    {
        const String n = "Exciter " + String (i) + " ";
        toggle (ids::b ("ex", i, "on"), n + "On", true);
        toggle (ids::b ("ex", i, "solo"), n + "Solo", false);
        choice (ids::b ("ex", i, "chan"), n + "Channel", ms::chanNames(), 0);
        choice (ids::b ("ex", i, "type"), n + "Emulation", ms::exTypeNames(), i == 4 ? 4 : 0);
        flt (ids::b ("ex", i, "drive"), n + "Drive", NormalisableRange<float> (0, 100), 0, pctAttr());
    }
    // Imager
    for (int i = 1; i <= 4; ++i)
    {
        const String n = "Imager " + String (i) + " ";
        toggle (ids::b ("im", i, "solo"), n + "Solo", false);
        flt (ids::b ("im", i, "width"), n + "Width", NormalisableRange<float> (0, 200), 100, pctAttr());
    }
    // Loudness / maximizer
    StringArray tnames; for (auto& t : ms::targets()) tnames.add (t.name);
    choice ("lu_target", "Loudness Target", tnames, 0);
    flt ("lu_gain", "Maximizer Gain", NormalisableRange<float> (0, 24, 0.05f), 0, dbAttr());
    flt ("lu_ceiling", "Ceiling (True Peak)", NormalisableRange<float> (-6, 0, 0.05f), -1, dbAttr());
    flt ("lu_release", "Limiter Release", skew (5, 1000, 100), 100, msAttr());
    flt ("lu_custom", "Custom Target", NormalisableRange<float> (-24, -6, 0.5f), -14,
         A().withLabel ("LUFS").withStringFromValueFunction ([] (float x, int) { return String (x, 1) + " LUFS"; }));
    toggle ("lu_follow", "Ceiling follows target", true);

    return { v.begin(), v.end() };
}

//==============================================================================
MasterSuiteProcessor::MasterSuiteProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    cacheParams();
}

void MasterSuiteProcessor::cacheParams()
{
    auto c = [this] (const String& id) { P x; x.p = apvts.getRawParameterValue (id); jassert (x.p != nullptr); return x; };
    const char* eqW[] = { "on", "type", "freq", "gain", "q", "chan" };
    const char* rbW[] = { "on", "solo", "chan", "gain" };
    const char* dynW[] = { "on", "solo", "chan", "thr", "ratio", "att", "rel", "mk" };
    const char* exW[] = { "on", "solo", "chan", "type", "drive" };
    const char* imW[] = { "solo", "width" };
    for (int i = 0; i < 6; ++i) for (int k = 0; k < 6; ++k) eqP[i][k] = c (ids::b ("eq", i + 1, eqW[k]));
    for (int i = 0; i < 4; ++i)
    {
        for (int k = 0; k < 4; ++k) rbP[i][k] = c (ids::b ("rb", i + 1, rbW[k]));
        for (int k = 0; k < 8; ++k) dynP[i][k] = c (ids::b ("dyn", i + 1, dynW[k]));
        for (int k = 0; k < 5; ++k) exP[i][k] = c (ids::b ("ex", i + 1, exW[k]));
        for (int k = 0; k < 2; ++k) imP[i][k] = c (ids::b ("im", i + 1, imW[k]));
    }
    gIn = c ("in_gain"); gOut = c ("out_gain"); gListen = c ("listen"); gEqListen = c ("eq_listen");
    for (int k = 0; k < 3; ++k) gXo[k] = c ("xo" + String (k + 1));
    for (int m = 0; m < (int) Mod::Count; ++m)
    {
        gOn[m] = c (String (modKey ((Mod) m)) + "_on");
        if (! modIsCore ((Mod) m)) gIns[m] = c (String (modKey ((Mod) m)) + "_in");
    }
    const char* gw[] = { "glue_on", "glue_chan", "glue_thr", "glue_ratio", "glue_att", "glue_rel", "glue_mk", "glue_mix", "glue_hpf" };
    for (int k = 0; k < 9; ++k) glueP[k] = c (gw[k]);
    exMix = c ("ex_mix"); luGainP = c ("lu_gain"); luCeil = c ("lu_ceiling"); luRel = c ("lu_release");
}

bool MasterSuiteProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return (out == AudioChannelSet::stereo() || out == AudioChannelSet::mono()) && l.getMainInputChannelSet() == out;
}

void MasterSuiteProcessor::setParam (const String& id, float value)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

void MasterSuiteProcessor::setInserted (Mod m, bool on)
{
    if (modIsCore (m)) return;
    setParam (String (modKey (m)) + "_in", on ? 1.0f : 0.0f);
    if (on) setParam (String (modKey (m)) + "_on", 1.0f);
}

//==============================================================================
void MasterSuiteProcessor::prepareToPlay (double sampleRate, int block)
{
    fs = sampleRate;
    eq.prepare (fs); rb.prepare (fs); dyn.prepare (fs); im.prepare (fs);
    os.reset();
    osMax = jmax (block, 2048);
    os.initProcessing ((size_t) osMax);
    ex.prepare (fs * 2.0);
    lim.prepare (fs);
    meter.prepare (fs);
    inGain.reset (fs, 0.05); outGain.reset (fs, 0.05); luGain.reset (fs, 0.05);
    inGain.setCurrentAndTargetValue (ms::dbToGain (gIn()));
    outGain.setCurrentAndTargetValue (ms::dbToGain (gOut()));
    luGain.setCurrentAndTargetValue (ms::dbToGain (luGainP()));
    fA.assign ((size_t) jmax (1, block), 0.0f); fB.assign ((size_t) jmax (1, block), 0.0f);
    rScratch.assign ((size_t) jmax (1, block), 0.0f);
    corrCoef = (float) (1.0 - std::exp (-1.0 / (0.3 * fs)));
    corrAcc[0] = corrAcc[1] = corrAcc[2] = 0;
    pullParams();
    eq.finishSetup();
    setLatencySamples (roundToInt (os.getLatencyInSamples()) + lim.latency());
}

void MasterSuiteProcessor::pullParams()
{
    const float x1 = gXo[0](), x2 = gXo[1](), x3 = gXo[2]();
    for (int i = 0; i < 6; ++i)
    {
        ms::ParametricEQ::Band b;
        b.on = eqP[i][0]() > 0.5f; b.type = (ms::EqType) (int) eqP[i][1]();
        b.freq = eqP[i][2](); b.gain = eqP[i][3](); b.q = eqP[i][4](); b.chan = (Chan) (int) eqP[i][5]();
        eq.setBand (i, b);
    }
    eq.setListen ((int) gEqListen());

    rb.setCrossover (x1, x2, x3);
    dyn.setCrossover (x1, x2, x3);
    ex.setCrossover (x1, x2, x3);
    im.setCrossover (x1, x2, x3);
    for (int i = 0; i < 4; ++i)
    {
        ms::Rebalance::Band r;
        r.f.on = rbP[i][0]() > 0.5f; r.f.solo = rbP[i][1]() > 0.5f; r.f.chan = (Chan) (int) rbP[i][2](); r.gain = rbP[i][3]();
        rb.setBand (i, r);

        ms::MultibandDynamics::Band d;
        d.f.on = dynP[i][0]() > 0.5f; d.f.solo = dynP[i][1]() > 0.5f; d.f.chan = (Chan) (int) dynP[i][2]();
        d.thr = dynP[i][3](); d.ratio = dynP[i][4](); d.att = dynP[i][5](); d.rel = dynP[i][6](); d.makeup = dynP[i][7]();
        dyn.setBand (i, d);

        ms::MultibandExciter::Band e;
        e.f.on = exP[i][0]() > 0.5f; e.f.solo = exP[i][1]() > 0.5f; e.f.chan = (Chan) (int) exP[i][2]();
        e.type = (ms::ExType) (int) exP[i][3](); e.drive = exP[i][4]();
        ex.setBand (i, e);

        ms::MultibandImager::Band m;
        m.solo = imP[i][0]() > 0.5f; m.width = imP[i][1]();
        im.setBand (i, m);
    }
    {
        ms::MultibandDynamics::Glue g;
        g.on = glueP[0]() > 0.5f; g.chan = (Chan) (int) glueP[1]();
        g.thr = glueP[2](); g.ratio = glueP[3](); g.att = glueP[4](); g.rel = glueP[5]();
        g.makeup = glueP[6](); g.mix = glueP[7](); g.scHpf = glueP[8]();
        dyn.setGlue (g);
    }
    ex.setMix (exMix());
    lim.set (luCeil(), luRel());
}

//==============================================================================
void MasterSuiteProcessor::processModule (Mod m, float* L, float* R, int n, bool on)
{
    switch (m)
    {
        case Mod::EQ:        if (on) for (int i = 0; i < n; ++i) eq.process (L[i], R[i]); break;
        case Mod::Rebalance: if (on) for (int i = 0; i < n; ++i) rb.process (L[i], R[i]); break;
        case Mod::Dynamics:  if (on) for (int i = 0; i < n; ++i) dyn.process (L[i], R[i]); break;
        case Mod::Imager:    if (on) for (int i = 0; i < n; ++i) im.process (L[i], R[i]); break;
        case Mod::Exciter:
        {
            // the oversampler always runs, so latency never changes when the exciter is added or removed
            float* ch[2] = { L, R };
            for (int start = 0; start < n; start += osMax)
            {
                const int len = jmin (osMax, n - start);
                dsp::AudioBlock<float> blk (ch, 2, (size_t) start, (size_t) len);
                auto up = os.processSamplesUp (blk);
                if (on)
                {
                    float* uL = up.getChannelPointer (0); float* uR = up.getChannelPointer (1);
                    for (size_t i = 0; i < up.getNumSamples(); ++i) ex.process (uL[i], uR[i]);
                }
                os.processSamplesDown (blk);
            }
            break;
        }
        default: break;
    }
}

//==============================================================================
void MasterSuiteProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nch = buffer.getNumChannels();
    if (n == 0 || nch == 0) return;
    if ((int) fA.size() < n) { fA.assign ((size_t) n, 0.0f); fB.assign ((size_t) n, 0.0f); rScratch.assign ((size_t) n, 0.0f); }   // host sent a bigger block

    pullParams();
    inGain.setTargetValue (ms::dbToGain (gIn()));
    outGain.setTargetValue (ms::dbToGain (gOut()));
    auto active = [this] (Mod m) { return (modIsCore (m) || gIns[(int) m]() > 0.5f) && gOn[(int) m]() > 0.5f; };
    const bool luOn = active (Mod::Loudness);
    luGain.setTargetValue (luOn ? ms::dbToGain (luGainP()) : 1.0f);
    if (! luOn) lim.set (24.0f, luRel());

    const bool eqOn = active (Mod::EQ) || (int) gEqListen() > 0;
    const bool rbOn = active (Mod::Rebalance);
    const bool dynOn = active (Mod::Dynamics);
    const bool exOn = active (Mod::Exciter);
    const bool imOn = active (Mod::Imager);
    const int listen = (int) gListen();

    float* L = buffer.getWritePointer (0);
    float* R = nch > 1 ? buffer.getWritePointer (1) : nullptr;

    if (R == nullptr)
    {   // mono track: run the chain in stereo on a copy, fold back at the end
        if ((int) rScratch.size() < n) rScratch.assign ((size_t) n, 0.0f);
        std::copy (L, L + n, rScratch.begin());
        R = rScratch.data();
    }

    // ---- input ----
    float inPk = 0;
    for (int i = 0; i < n; ++i)
    {
        const float g = inGain.getNextValue();
        L[i] *= g; R[i] *= g;
        inPk = std::max ({ inPk, std::abs (L[i]), std::abs (R[i]) });
        fA[(size_t) i] = 0.5f * (L[i] + R[i]); fB[(size_t) i] = 0.5f * (L[i] - R[i]);
    }
    preFifo.push (fA.data(), fB.data(), n);

    // ---- the movable modules, in the order chosen on the tabs ----
    const bool onFlag[(int) Mod::Count] = { eqOn, rbOn, dynOn, exOn, imOn, luOn };
    for (int k = 0; k < numMovable; ++k)
    {
        const int m = order[k].load();
        processModule ((Mod) m, L, R, n, onFlag[m]);
    }

    // ---- maximizer (always last), meters, listen ----
    float outPk = 0;
    const bool mono = R == rScratch.data();
    for (int i = 0; i < n; ++i)
    {
        float l = L[i], r = R[i];
        const float g = luGain.getNextValue();
        l *= g; r *= g;
        lim.process (l, r);
        const float og = outGain.getNextValue();
        l *= og; r *= og;
        meter.process (l, r);
        corrAcc[0] += corrCoef * (l * r - corrAcc[0]);
        corrAcc[1] += corrCoef * (l * l - corrAcc[1]);
        corrAcc[2] += corrCoef * (r * r - corrAcc[2]);
        outPk = std::max ({ outPk, std::abs (l), std::abs (r) });
        fA[(size_t) i] = l; fB[(size_t) i] = r;
        float ol, orr;
        ms::ChanIO::listen ((Chan) listen, l, r, ol, orr);
        if (mono) L[i] = 0.5f * (ol + orr);
        else { L[i] = ol; R[i] = orr; }
    }
    scopeFifo.push (fA.data(), fB.data(), n);
    for (int i = 0; i < n; ++i) { const float l = fA[(size_t) i], r = fB[(size_t) i]; fA[(size_t) i] = 0.5f * (l + r); fB[(size_t) i] = 0.5f * (l - r); }
    postFifo.push (fA.data(), fB.data(), n);

    for (int c = 2; c < nch; ++c) buffer.clear (c, 0, n);

    // ---- meters ----
    auto peakHold = [] (std::atomic<float>& a, float v) { if (v > a.load()) a.store (v); };
    for (int k = 0; k < 4; ++k) { peakHold (dynGr[k], dyn.takeGr (k)); corr[k].store (im.correlation (k)); }
    peakHold (glueGr, dyn.takeGlueGr());
    peakHold (limGr, lim.takeGr());
    peakHold (inPeak, ms::gainToDb (inPk));
    peakHold (outPeak, ms::gainToDb (outPk));
    const float d = std::sqrt (corrAcc[1] * corrAcc[2]);
    corrAll.store (d < 1.0e-9f ? 1.0f : jlimit (-1.0f, 1.0f, corrAcc[0] / d));
}

//==============================================================================
bool MasterSuiteProcessor::matchTarget()
{
    const float integrated = meter.integrated.load();
    if (integrated < -60.0f) return false;
    const int ti = (int) get ("lu_target");
    const auto& t = ms::targets()[(size_t) jlimit (0, (int) ms::targets().size() - 1, ti)];
    const float target = String (t.name) == "Custom" ? get ("lu_custom") : t.lufs;
    setParam ("lu_gain", jlimit (0.0f, 24.0f, get ("lu_gain") + (target - integrated)));
    if (get ("lu_follow") > 0.5f) setParam ("lu_ceiling", t.tp);
    meter.requestReset();
    return true;
}

int MasterSuiteProcessor::getNumPrograms() { return (int) presets().size(); }
const String MasterSuiteProcessor::getProgramName (int i) { return isPositiveAndBelow (i, (int) presets().size()) ? presets()[(size_t) i].name : String(); }

void MasterSuiteProcessor::setCurrentProgram (int index)
{
    if (! isPositiveAndBelow (index, (int) presets().size())) return;
    currentPreset = index;
    // back to defaults first, then apply the preset's changes
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (p))
        {
            if (rp->getParameterID() == "listen") continue;
            rp->beginChangeGesture(); rp->setValueNotifyingHost (rp->getDefaultValue()); rp->endChangeGesture();
        }
    for (auto& kv : presets()[(size_t) index].values) setParam (kv.first, kv.second);
}

void MasterSuiteProcessor::setOrder (const std::array<int, numMovable>& o)
{
    // accept only a real permutation of the movable modules
    bool seen[numMovable] {};
    for (int v : o) { if (v < 0 || v >= numMovable || seen[v]) return; seen[v] = true; }
    for (int i = 0; i < numMovable; ++i) order[i].store (o[(size_t) i]);
}

void MasterSuiteProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset, nullptr);
    String ord; for (int i = 0; i < numMovable; ++i) ord << order[i].load() << (i < numMovable - 1 ? "," : "");
    state.setProperty ("order", ord, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void MasterSuiteProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = ValueTree::fromXml (*xml);
            currentPreset = vt.getProperty ("preset", 0);
            StringArray parts; parts.addTokens (vt.getProperty ("order", "0,1,2,3,4").toString(), ",", {});
            std::array<int, numMovable> o { 0, 1, 2, 3, 4 };
            if (parts.size() == numMovable) for (int i = 0; i < numMovable; ++i) o[(size_t) i] = parts[i].getIntValue();
            setOrder (o);
            apvts.replaceState (vt);
        }
}

AudioProcessorEditor* MasterSuiteProcessor::createEditor() { return new MasterSuiteEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MasterSuiteProcessor(); }
