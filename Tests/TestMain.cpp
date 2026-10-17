// Offline test runner for Master Suite: DSP correctness checks + optional UI screenshots.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cstdio>
#include <random>

using namespace juce;

static int failures = 0;
static void check (bool ok, const String& what)
{
    std::printf ("[%s] %s\n", ok ? " OK " : "FAIL", what.toRawUTF8());
    if (! ok) ++failures;
}

static void setP (MasterSuiteProcessor& p, const String& id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    jassert (prm != nullptr);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

/** neutral chain: everything that changes sound is off / zero */
static void neutral (MasterSuiteProcessor& p)
{
    setP (p, "dyn_on", 0); setP (p, "lu_gain", 0); setP (p, "lu_ceiling", 0);
}

struct Stereo { std::vector<float> l, r; };

static Stereo run (MasterSuiteProcessor& p, const Stereo& in, int block = 512)
{
    Stereo out; out.l.resize (in.l.size()); out.r.resize (in.l.size());
    AudioBuffer<float> buf (2, block); MidiBuffer midi;
    for (size_t pos = 0; pos < in.l.size(); pos += (size_t) block)
    {
        const int n = (int) std::min ((size_t) block, in.l.size() - pos);
        AudioBuffer<float> b (buf.getArrayOfWritePointers(), 2, 0, n);
        for (int i = 0; i < n; ++i) { b.setSample (0, i, in.l[pos + (size_t) i]); b.setSample (1, i, in.r[pos + (size_t) i]); }
        p.processBlock (b, midi);
        for (int i = 0; i < n; ++i) { out.l[pos + (size_t) i] = b.getSample (0, i); out.r[pos + (size_t) i] = b.getSample (1, i); }
    }
    return out;
}

static Stereo sine (double fs, double f, float ampL, float ampR, double secs)
{
    Stereo s; const size_t n = (size_t) (fs * secs); s.l.resize (n); s.r.resize (n);
    for (size_t i = 0; i < n; ++i) { const float v = (float) std::sin (MathConstants<double>::twoPi * f * (double) i / fs); s.l[i] = v * ampL; s.r[i] = v * ampR; }
    return s;
}
static Stereo noise (double fs, double secs, float amp, unsigned seed = 1, bool correlated = false)
{
    std::mt19937 rng (seed); std::normal_distribution<float> nd (0.0f, 1.0f);
    Stereo s; const size_t n = (size_t) (fs * secs); s.l.resize (n); s.r.resize (n);
    for (size_t i = 0; i < n; ++i) { s.l[i] = nd (rng) * amp; s.r[i] = correlated ? s.l[i] : nd (rng) * amp; }
    return s;
}
static float rmsDb (const std::vector<float>& x, size_t a, size_t b)
{
    double s = 0; for (size_t i = a; i < b; ++i) s += (double) x[i] * x[i];
    return 10.0f * (float) std::log10 (s / (double) std::max<size_t> (1, b - a) + 1e-30);
}
static std::unique_ptr<MasterSuiteProcessor> make (double fs, int block = 512)
{
    auto p = std::make_unique<MasterSuiteProcessor>();
    p->setPlayConfigDetails (2, 2, fs, block);
    p->prepareToPlay (fs, block);
    return p;
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const double fs = 48000;

    {
        ms::TruePeakLimiter lim; lim.prepare (48000); lim.set (-1.0f, 100);
        std::mt19937 rng (9); std::normal_distribution<float> nd (0, 1);
        float pk = 0; int over = 0;
        for (int i = 0; i < 144000; i++) { float l = nd (rng) * 1.2f, r = nd (rng) * 1.2f; const float l0 = l; lim.process (l, r);
            if (i > 24000) { pk = std::max (pk, std::abs (l)); if (std::abs (l) > 0.886f) over++; } (void) l0; }
        check (over == 0 && pk < 0.886f, "limiter alone never touches its safety clip (" + String (20 * std::log10 (pk), 2) + " dB)");
    }
    // ---------- latency is what we report ----------
    {
        auto p = make (fs); neutral (*p); p->prepareToPlay (fs, 512);
        Stereo imp; imp.l.assign (8192, 0.0f); imp.r.assign (8192, 0.0f); imp.l[100] = imp.r[100] = 0.5f;
        auto o = run (*p, imp);
        size_t pk = 0; for (size_t i = 0; i < o.l.size(); ++i) if (std::abs (o.l[i]) > std::abs (o.l[pk])) pk = i;
        const int lat = (int) pk - 100;
        check (std::abs (lat - p->getLatencySamples()) <= 1, "reported latency matches (" + String (lat) + " vs " + String (p->getLatencySamples()) + ")");
    }
    // ---------- neutral chain is transparent ----------
    {
        auto p = make (fs); neutral (*p); p->prepareToPlay (fs, 512);
        auto in = noise (fs, 2.0, 0.05f, 3);
        {   // music-like bandwidth: nothing above 16 kHz
            ms::Biquad lp[3]; for (auto& q : lp) ms::designEq (q, ms::EqType::HighCut, fs, 16000, 0, 0.7);
            for (size_t i = 0; i < in.l.size(); ++i) { float a = in.l[i], b = in.r[i]; for (auto& q : lp) { a = q.process (0, a); b = q.process (1, b); } in.l[i] = a; in.r[i] = b; }
        }
        auto o = run (*p, in);
        const int lat = p->getLatencySamples();
        double e = 0, s = 0;
        for (size_t i = 20000; i < in.l.size(); ++i) { const double d = o.l[i] - in.l[i - (size_t) lat]; e += d * d; s += (double) in.l[i - (size_t) lat] * in.l[i - (size_t) lat]; }
        const double errDb = 10 * std::log10 (e / s + 1e-30);
        check (errDb < -40.0, "neutral chain nulls with the dry signal (" + String (errDb, 1) + " dB)");
    }
    // ---------- crossover sums flat (rebalance inserted, 0 dB) ----------
    {
        bool ok = true; String info;
        for (double f : { 40.0, 120.0, 400.0, 1200.0, 3000.0, 6000.0, 12000.0 })
        {
            auto p = make (fs); neutral (*p); p->setInserted (Mod::Rebalance, true); setP (*p, "eq_on", 0); p->prepareToPlay (fs, 512);
            auto in = sine (fs, f, 0.1f, 0.1f, 1.0);
            auto o = run (*p, in);
            const float d = rmsDb (o.l, 24000, 48000) - rmsDb (in.l, 24000, 48000);
            info << String (f, 0) << ":" << String (d, 2) << " ";
            ok &= std::abs (d) < 0.1f;
        }
        check (ok, "multiband split sums flat  " + info);
    }
    // ---------- mid/side routing ----------
    {
        auto p = make (fs); neutral (*p);
        setP (*p, "eq3_chan", 2); setP (*p, "eq3_gain", 12); setP (*p, "eq3_freq", 1000); p->prepareToPlay (fs, 512);
        auto mono = sine (fs, 1000, 0.1f, 0.1f, 1.0);
        auto o = run (*p, mono);
        const float d = rmsDb (o.l, 24000, 48000) - rmsDb (mono.l, 24000, 48000);
        check (std::abs (d) < 0.05f, "SIDE EQ band leaves a mono (mid only) signal alone (" + String (d, 2) + " dB)");
        auto side = sine (fs, 1000, 0.1f, -0.1f, 1.0);
        auto p2 = make (fs); neutral (*p2); setP (*p2, "eq3_chan", 2); setP (*p2, "eq3_gain", 12); setP (*p2, "eq3_freq", 1000); p2->prepareToPlay (fs, 512);
        auto o2 = run (*p2, side);
        const float d2 = rmsDb (o2.l, 24000, 48000) - rmsDb (side.l, 24000, 48000);
        check (std::abs (d2 - 12.0f) < 0.3f, "SIDE EQ band boosts a side signal (" + String (d2, 2) + " dB)");
        auto p3 = make (fs); neutral (*p3); setP (*p3, "eq3_chan", 3); setP (*p3, "eq3_gain", 6); setP (*p3, "eq3_freq", 1000); p3->prepareToPlay (fs, 512);
        auto o3 = run (*p3, mono);
        const float dl = rmsDb (o3.l, 24000, 48000) - rmsDb (mono.l, 24000, 48000), dr = rmsDb (o3.r, 24000, 48000) - rmsDb (mono.r, 24000, 48000);
        check (std::abs (dl - 6) < 0.3f && std::abs (dr) < 0.05f, "LEFT EQ band changes only the left channel");
    }
    // ---------- dynamics: band in MID mode ignores pure side ----------
    {
        auto p = make (fs); setP (*p, "lu_gain", 0); setP (*p, "lu_ceiling", 0); setP (*p, "glue_on", 0);
        for (int i = 1; i <= 4; ++i) { setP (*p, ids::b ("dyn", i, "chan"), 1); setP (*p, ids::b ("dyn", i, "thr"), -40); setP (*p, ids::b ("dyn", i, "ratio"), 8); }
        p->prepareToPlay (fs, 512);
        auto side = sine (fs, 1000, 0.3f, -0.3f, 1.0);
        auto o = run (*p, side);
        const float d = rmsDb (o.l, 24000, 48000) - rmsDb (side.l, 24000, 48000);
        check (std::abs (d) < 0.1f, "MID compressor does not touch the side signal (" + String (d, 2) + " dB)");
        auto mid = sine (fs, 1000, 0.3f, 0.3f, 1.0);
        auto p2 = make (fs); setP (*p2, "lu_gain", 0); setP (*p2, "lu_ceiling", 0); setP (*p2, "glue_on", 0);
        for (int i = 1; i <= 4; ++i) { setP (*p2, ids::b ("dyn", i, "chan"), 1); setP (*p2, ids::b ("dyn", i, "thr"), -40); setP (*p2, ids::b ("dyn", i, "ratio"), 8); }
        p2->prepareToPlay (fs, 512);
        auto o2 = run (*p2, mid);
        const float d2 = rmsDb (o2.l, 24000, 48000) - rmsDb (mid.l, 24000, 48000);
        check (d2 < -10.0f, "MID compressor compresses the mid signal (" + String (d2, 1) + " dB)");
    }
    // ---------- compressor is clean (no colour) ----------
    {
        auto p = make (fs); setP (*p, "lu_gain", 0); setP (*p, "lu_ceiling", 0); setP (*p, "glue_thr", -30); setP (*p, "glue_ratio", 4);
        p->prepareToPlay (fs, 512);
        auto in = sine (fs, 1000, 0.5f, 0.5f, 2.0);
        auto o = run (*p, in);
        // THD: remove fundamental with a least-squares fit, look at the rest
        double ss = 0, sc = 0, cc = 0, xs = 0, xc = 0; const size_t a = 48000, b = 96000;
        for (size_t i = a; i < b; ++i) { const double ph = MathConstants<double>::twoPi * 1000.0 * (double) i / fs; const double s = std::sin (ph), c = std::cos (ph);
            ss += s * s; cc += c * c; sc += s * c; xs += o.l[i] * s; xc += o.l[i] * c; }
        const double det = ss * cc - sc * sc, A = (xs * cc - xc * sc) / det, B = (xc * ss - xs * sc) / det;
        double res = 0, tot = 0;
        for (size_t i = a; i < b; ++i) { const double ph = MathConstants<double>::twoPi * 1000.0 * (double) i / fs; const double fit = A * std::sin (ph) + B * std::cos (ph);
            res += (o.l[i] - fit) * (o.l[i] - fit); tot += (double) o.l[i] * o.l[i]; }
        const double thd = 10 * std::log10 (res / tot + 1e-30);
        check (thd < -55.0, "compression adds no colour, THD " + String (thd, 1) + " dB");
    }
    // ---------- exciter adds harmonics, keeps level ----------
    {
        auto p = make (fs); neutral (*p); p->setInserted (Mod::Exciter, true);
        for (int i = 1; i <= 4; ++i) setP (*p, ids::b ("ex", i, "drive"), 60);
        p->prepareToPlay (fs, 512);
        auto in = sine (fs, 220, 0.3f, 0.3f, 1.0);
        auto o = run (*p, in);
        const float d = rmsDb (o.l, 24000, 48000) - rmsDb (in.l, 24000, 48000);
        check (std::abs (d) < 2.5f, "exciter keeps the level (" + String (d, 2) + " dB)");
        bool finite = true; for (auto v : o.l) finite &= std::isfinite (v);
        check (finite, "exciter output finite");
    }
    // ---------- imager ----------
    {
        auto p = make (fs); neutral (*p); p->setInserted (Mod::Imager, true);
        for (int i = 1; i <= 4; ++i) setP (*p, ids::b ("im", i, "width"), 0);
        p->prepareToPlay (fs, 512);
        auto in = noise (fs, 1.0, 0.05f, 5);
        auto o = run (*p, in);
        double sd = 0, sm = 0; for (size_t i = 10000; i < o.l.size(); ++i) { sd += std::pow (o.l[i] - o.r[i], 2); sm += std::pow (o.l[i] + o.r[i], 2); }
        check (10 * std::log10 (sd / sm + 1e-30) < -50, "imager width 0 makes it mono");
        check (p->corrAll.load() > 0.99f, "correlation meter reads +1 for mono (" + String (p->corrAll.load(), 2) + ")");
        auto p2 = make (fs); neutral (*p2); p2->prepareToPlay (fs, 512);
        auto anti = sine (fs, 500, 0.2f, -0.2f, 1.0);
        run (*p2, anti);
        check (p2->corrAll.load() < -0.95f, "correlation meter reads -1 for out-of-phase (" + String (p2->corrAll.load(), 2) + ")");
    }
    // ---------- loudness meter accuracy ----------
    {
        auto p = make (fs); neutral (*p); setP (*p, "eq_on", 0); p->prepareToPlay (fs, 512);
        const float a = std::pow (10.0f, -23.0f / 20.0f);
        auto in = sine (fs, 997, a, a, 10.0);
        run (*p, in);
        const float I = p->meter.integrated.load(), S = p->meter.shortTerm.load(), M = p->meter.momentary.load();
        check (std::abs (I + 23.0f) < 0.15f && std::abs (S + 23.0f) < 0.15f && std::abs (M + 23.0f) < 0.15f,
               "EBU 1 kHz -23 dBFS sine reads -23 LUFS (I " + String (I, 2) + " S " + String (S, 2) + " M " + String (M, 2) + ")");
        const float tp = p->meter.truePeakMax.load();
        check (std::abs (tp + 23.0f) < 0.3f, "true peak of the sine reads -23 dBTP (" + String (tp, 2) + ")");
    }
    // ---------- limiter holds the true-peak ceiling ----------
    {
        auto p = make (fs); setP (*p, "lu_gain", 18); setP (*p, "lu_ceiling", -1.0f); p->prepareToPlay (fs, 512);
        auto in = noise (fs, 3.0, 0.15f, 9);
        auto o = run (*p, in);
        // measure true peak at 8x by sinc interpolation
        float tp = 0;
        for (size_t i = 24000; i + 40 < o.l.size(); ++i)
            for (int ph = 0; ph < 8; ++ph)
            {
                double s = 0; for (int t = -16; t <= 16; ++t) { const double x = (double) t - ph / 8.0; const double w = std::abs (x) < 1e-9 ? 1.0 : std::sin (MathConstants<double>::pi * x) / (MathConstants<double>::pi * x) * (0.5 + 0.5 * std::cos (MathConstants<double>::pi * x / 17));
                    s += w * o.l[i + (size_t) t + 16]; }
                tp = std::max (tp, (float) std::abs (s));
            }
        const float tpDb = 20 * std::log10 (tp);
        check (tpDb <= -0.85f, "limiter keeps true peak under the -1 dBTP ceiling (" + String (tpDb, 2) + " dBTP)");
        check (p->meter.integrated.load() > -12.0f, "maximizer makes it loud (" + String (p->meter.integrated.load(), 1) + " LUFS)");
    }
    // ---------- match target ----------
    {
        auto p = make (fs); neutral (*p); setP (*p, "lu_target", 0); setP (*p, "lu_ceiling", -1.0f); p->prepareToPlay (fs, 512);
        auto in = noise (fs, 6.0, 0.03f, 11);
        run (*p, in);
        const bool ok = p->matchTarget();
        auto o = run (*p, noise (fs, 8.0, 0.03f, 12));
        const float I = p->meter.integrated.load();
        check (ok && std::abs (I + 14.0f) < 1.0f, "MATCH TARGET lands on Spotify -14 LUFS (" + String (I, 2) + ")");
    }
    // ---------- band solo ----------
    {
        auto p = make (fs); neutral (*p); setP (*p, "dyn_on", 1); setP (*p, "dyn1_solo", 1); setP (*p, "glue_on", 0);
        for (int i = 1; i <= 4; ++i) setP (*p, ids::b ("dyn", i, "ratio"), 1);
        p->prepareToPlay (fs, 512);
        auto hi = sine (fs, 8000, 0.2f, 0.2f, 1.0);
        auto o = run (*p, hi);
        check (rmsDb (o.l, 24000, 48000) - rmsDb (hi.l, 24000, 48000) < -40.0f, "solo low band removes the highs");
    }
    // ---------- presets and sample rates are stable ----------
    {
        bool ok = true;
        for (double r : { 44100.0, 48000.0, 96000.0 })
        {
            auto p = make (r);
            for (int pr = 0; pr < p->getNumPrograms(); ++pr)
            {
                p->setCurrentProgram (pr); p->prepareToPlay (r, 256);
                auto o = run (*p, noise (r, 1.0, 0.2f, (unsigned) pr + 20), 256);
                for (auto v : o.l) ok &= std::isfinite (v) && std::abs (v) < 1.0f;
            }
        }
        check (ok, "all presets stable at 44.1 / 48 / 96 kHz");
    }
    // ---------- CPU: everything inserted ----------
    {
        auto p = make (fs); p->setInserted (Mod::Exciter, true); p->setInserted (Mod::Imager, true); p->setInserted (Mod::Rebalance, true);
        for (int i = 1; i <= 4; ++i) setP (*p, ids::b ("ex", i, "drive"), 40);
        p->prepareToPlay (fs, 512);
        auto in = noise (fs, 20.0, 0.1f, 77);
        const auto t0 = Time::getMillisecondCounterHiRes();
        run (*p, in);
        const double ms = Time::getMillisecondCounterHiRes() - t0;
        const double load = ms / 20000.0 * 100.0;
        check (load < 25.0, "CPU with every module on: " + String (load, 1) + " % of one core");
    }
    // ---------- state ----------
    {
        auto a = make (fs); setP (*a, "eq4_gain", 3.5f); a->setInserted (Mod::Imager, true);
        MemoryBlock mb; a->getStateInformation (mb);
        auto b = make (fs); b->setStateInformation (mb.getData(), (int) mb.getSize());
        check (std::abs (b->get ("eq4_gain") - 3.5f) < 0.01f && b->isInserted (Mod::Imager), "state save / restore keeps modules and values");
    }

#if MS_SHOTS
    // default view: only the 3 core modules + ADD MODULE
    {
        auto p = make (fs);
        std::unique_ptr<AudioProcessorEditor> ed (p->createEditor());
        ed->setVisible (true);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        File f (File::getCurrentWorkingDirectory().getChildFile ("shot_default.png"));
        f.deleteFile(); FileOutputStream os (f); PNGImageFormat().writeImageToStream (img, os);
    }
    // screenshots of every page
    {
        auto p = make (fs);
        p->setInserted (Mod::Exciter, true); p->setInserted (Mod::Imager, true); p->setInserted (Mod::Rebalance, true);
        setP (*p, "eq2_gain", 2.0f); setP (*p, "eq4_gain", -1.5f); setP (*p, "eq4_chan", 2); setP (*p, "eq6_gain", 2.5f); setP (*p, "eq3_chan", 1); setP (*p, "eq3_gain", 1.2f);
        setP (*p, "lu_gain", 6);
        // feed some audio so meters / analyzers move
        auto music = noise (fs, 4.0, 0.08f, 33);
        for (size_t i = 0; i < music.l.size(); ++i) { music.r[i] = 0.7f * music.l[i] + 0.3f * music.r[i]; }
        std::unique_ptr<AudioProcessorEditor> ed (p->createEditor());
        ed->setVisible (true);
        for (int page = 0; page < 6; ++page)
        {
            if (auto* e = dynamic_cast<MasterSuiteEditor*> (ed.get())) e->showPage (page);
            run (*p, music);
            for (int k = 0; k < 40; ++k) { if (auto* e = dynamic_cast<MasterSuiteEditor*> (ed.get())) e->tickForTest(); }
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            File f (File::getCurrentWorkingDirectory().getChildFile ("shot_" + String (page) + ".png"));
            f.deleteFile(); FileOutputStream os (f); PNGImageFormat().writeImageToStream (img, os);
        }
    }
#endif
    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED", failures);
    return failures == 0 ? 0 : 1;
}
