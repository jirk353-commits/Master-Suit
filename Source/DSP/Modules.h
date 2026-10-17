#pragma once
#include "Crossover.h"

namespace ms
{
//==============================================================================
/** 6-band parametric EQ, every band can work on Stereo / Mid / Side / Left / Right. */
class ParametricEQ
{
public:
    static constexpr int N = 6;
    struct Band { bool on = false; EqType type = EqType::Bell; float freq = 1000, gain = 0, q = 0.7f; Chan chan = Chan::Stereo; };

    void prepare (double sampleRate) { fs = sampleRate; for (auto& b : bq) b.reset(); for (auto& b : bp) b.reset(); dirty = true; }
    void reset() { for (auto& b : bq) b.reset(); for (auto& b : bp) b.reset(); }

    void setBand (int i, const Band& b)
    {
        auto& cur = bands[(size_t) i];
        if (cur.on == b.on && cur.type == b.type && cur.freq == b.freq && cur.gain == b.gain && cur.q == b.q && cur.chan == b.chan && ! dirty)
            return;
        const bool chanChanged = cur.chan != b.chan || cur.type != b.type;
        cur = b;
        designEq (bq[(size_t) i], b.type, fs, b.freq, b.gain, b.q);
        designBandpass (bp[(size_t) i], fs, b.freq, b.type == EqType::Bell ? b.q : 0.9);
        if (chanChanged) { bq[(size_t) i].reset(); bp[(size_t) i].reset(); }
    }
    void finishSetup() { dirty = false; }
    /** 0 = normal, 1..N = listen to only what band k touches (band monitor) */
    void setListen (int k) { listen = k; }

    inline void process (float& l, float& r) noexcept
    {
        const float inL = l, inR = r;
        for (int i = 0; i < N; ++i)
        {
            const auto& b = bands[(size_t) i];
            if (! b.on) continue;
            float a, c, keep;
            const int n = ChanIO::split (b.chan, l, r, a, c, keep);
            a = bq[(size_t) i].process (0, a);
            if (n == 2) c = bq[(size_t) i].process (1, c);
            ChanIO::join (b.chan, a, c, keep, l, r);
        }
        if (listen > 0)
        {
            const auto& b = bands[(size_t) listen - 1];
            float ol, orr;
            ChanIO::listen (b.chan, inL, inR, ol, orr);
            auto& f = bp[(size_t) listen - 1];
            if (b.type == EqType::LowCut || b.type == EqType::HighCut || b.type == EqType::LowShelf || b.type == EqType::HighShelf)
            {   // shelves / cuts: hear the region the band acts on
                l = f.process (0, ol); r = b.chan == Chan::Stereo ? f.process (1, orr) : l;
            }
            else { l = f.process (0, ol); r = b.chan == Chan::Stereo ? f.process (1, orr) : l; }
        }
    }

    double responseDb (int band, double f) const { return bands[(size_t) band].on ? bq[(size_t) band].magnitudeDb (f, fs) : 0.0; }
    const Band& band (int i) const { return bands[(size_t) i]; }

private:
    double fs = 48000;
    std::array<Band, N> bands;
    std::array<Biquad, N> bq, bp;
    int listen = 0;
    bool dirty = true;
};

//==============================================================================
/** Clean feed-forward compressor gain computer with log-domain smoothing: no colour, only level. */
struct CleanComp
{
    float thr = -18, ratio = 2, knee = 6, attMs = 10, relMs = 120;
    float aCoef = 0, rCoef = 0, gr = 0;      // gr in dB (>= 0)
    void prepare (double fs) { setTimes (fs); gr = 0; }
    void setTimes (double fs)
    {
        aCoef = std::exp (-1.0f / (0.001f * juce::jmax (0.05f, attMs) * (float) fs));
        rCoef = std::exp (-1.0f / (0.001f * juce::jmax (5.0f, relMs) * (float) fs));
    }
    inline float target (float levelDb) const noexcept
    {
        const float over = levelDb - thr;
        if (ratio <= 1.0f) return 0;
        const float slope = 1.0f - 1.0f / ratio;
        if (over <= -knee * 0.5f) return 0;
        if (over >= knee * 0.5f) return over * slope;
        const float x = over + knee * 0.5f;
        return slope * x * x / (2.0f * knee);
    }
    /** returns gain (linear) for detector level (linear abs) */
    inline float process (float det) noexcept
    {
        const float t = target (gainToDb (det));
        const float c = t > gr ? aCoef : rCoef;
        gr = t + c * (gr - t);
        return dbToGain (-gr);
    }
};

//==============================================================================
/** Shared helpers for the four multiband modules */
struct BandFlags { bool on = true; bool solo = false; Chan chan = Chan::Stereo; };

/** 4-band transparent compressor + wideband glue stage. */
class MultibandDynamics
{
public:
    static constexpr int B = 4;
    struct Band { BandFlags f; float thr = -18, ratio = 2, att = 15, rel = 150, makeup = 0; };
    struct Glue { bool on = true; Chan chan = Chan::Stereo; float thr = -12, ratio = 2, att = 10, rel = 300, makeup = 0, mix = 100, scHpf = 90; };

    void prepare (double sampleRate)
    {
        fs = sampleRate; xo.prepare (fs);
        for (auto& c : comp) c.prepare (fs);
        glueComp.prepare (fs);
        for (auto& m : mk) m.reset (fs, 0.05);
        glueMk.reset (fs, 0.05);
        designEq (scFilter, EqType::LowCut, fs, 90, 0, 0.7);
        scFilter.reset();
        std::fill (std::begin (grMeter), std::end (grMeter), 0.0f); glueGr = 0;
    }
    void reset() { xo.reset(); scFilter.reset(); for (auto& c : comp) c.gr = 0; glueComp.gr = 0; }
    void setCrossover (float a, float b, float c) { xo.setFreqs (a, b, c); }
    void setBand (int i, const Band& b)
    {
        bands[(size_t) i] = b;
        auto& c = comp[(size_t) i];
        c.thr = b.thr; c.ratio = b.ratio; c.attMs = b.att; c.relMs = b.rel; c.setTimes (fs);
        mk[(size_t) i].setTargetValue (dbToGain (b.makeup));
    }
    void setGlue (const Glue& g)
    {
        if (g.scHpf != glue.scHpf) designEq (scFilter, EqType::LowCut, fs, g.scHpf, 0, 0.7);
        glue = g;
        glueComp.thr = g.thr; glueComp.ratio = g.ratio; glueComp.attMs = g.att; glueComp.relMs = g.rel; glueComp.setTimes (fs);
        glueMk.setTargetValue (dbToGain (g.makeup));
    }

    inline void process (float& l, float& r) noexcept
    {
        float bl[B], br[B];
        xo.split (0, l, bl); xo.split (1, r, br);
        bool anySolo = false;
        for (auto& b : bands) anySolo |= b.f.solo;
        float ol = 0, orr = 0;
        for (int k = 0; k < B; ++k)
        {
            const auto& b = bands[(size_t) k];
            float L = bl[k], R = br[k];
            const float m = mk[(size_t) k].getNextValue();
            if (b.f.on)
            {
                float a, c, keep;
                const int n = ChanIO::split (b.f.chan, L, R, a, c, keep);
                const float det = n == 2 ? std::max (std::abs (a), std::abs (c)) : std::abs (a);
                const float g = comp[(size_t) k].process (det) * m;
                a *= g; c *= g;
                ChanIO::join (b.f.chan, a, c, keep, L, R);
            }
            if (b.f.solo)
            {
                float sl, sr; ChanIO::listen (b.f.chan, L, R, sl, sr);
                ol += sl; orr += sr;
            }
            else if (! anySolo) { ol += L; orr += R; }
        }
        for (int k = 0; k < B; ++k) grMeter[k] = std::max (grMeter[k], comp[(size_t) k].gr);

        if (glue.on && ! anySolo)
        {
            float a, c, keep;
            const int n = ChanIO::split (glue.chan, ol, orr, a, c, keep);
            const float dA = scFilter.process (0, a), dC = n == 2 ? scFilter.process (1, c) : 0.0f;
            const float g = glueComp.process (std::max (std::abs (dA), std::abs (dC))) * glueMk.getNextValue();
            const float mix = glue.mix * 0.01f;
            a = a * (1.0f - mix + mix * g); c = c * (1.0f - mix + mix * g);
            ChanIO::join (glue.chan, a, c, keep, ol, orr);
            glueGr = std::max (glueGr, glueComp.gr);
        }
        l = ol; r = orr;
    }
    /** read-and-clear meter peaks (dB of gain reduction) */
    float takeGr (int k) { const float v = grMeter[k]; grMeter[k] = 0; return v; }
    float takeGlueGr() { const float v = glueGr; glueGr = 0; return v; }
    float transfer (int k, float inDb) const { return inDb - comp[(size_t) k].target (inDb) + bands[(size_t) k].makeup; }
    float glueTransfer (float inDb) const { return inDb - glueComp.target (inDb) + glue.makeup; }

private:
    double fs = 48000;
    Crossover4 xo;
    std::array<Band, B> bands;
    std::array<CleanComp, B> comp;
    std::array<juce::SmoothedValue<float>, B> mk;
    Glue glue; CleanComp glueComp; juce::SmoothedValue<float> glueMk; Biquad scFilter;
    float grMeter[B] {}, glueGr = 0;
};

//==============================================================================
/** Harmonic exciter with analog-style curves, 4 bands, runs at 2x sample rate (caller oversamples). */
enum class ExType { Tube = 0, Tape, Transformer, Console, Crystal };
inline const juce::StringArray& exTypeNames() { static const juce::StringArray s { "Tube", "Tape", "Transformer", "Console", "Crystal" }; return s; }

class MultibandExciter
{
public:
    static constexpr int B = 4;
    struct Band { BandFlags f; ExType type = ExType::Tube; float drive = 0; };

    void prepare (double sampleRateOS)
    {
        fs = sampleRateOS; xo.prepare (fs);
        for (auto& d : dc) { designEq (d, EqType::LowCut, fs, 18, 0, 0.7); d.reset(); }
        for (auto& e : hpEdge) e = -1;
        for (auto& s : drv) s.reset (fs, 0.05);
        mixSm.reset (fs, 0.05);
    }
    void reset() { xo.reset(); for (auto& d : dc) d.reset(); }
    void setCrossover (float a, float b, float c)
    {
        xo.setFreqs (a, b, c);
        const float edge[B] = { 20.0f, xo.freq (0), xo.freq (1), xo.freq (2) };
        for (int k = 0; k < B; ++k)
            if (edge[k] != hpEdge[k])
            {   // harmonics are kept above the band's own low edge, so no rumble / mud is added
                hpEdge[k] = edge[k];
                designEq (dc[(size_t) k * 2], EqType::LowCut, fs, edge[k] * 0.9f, 0, 0.7);
                dc[(size_t) k * 2 + 1] = dc[(size_t) k * 2];
            }
    }
    void setBand (int i, const Band& b) { bands[(size_t) i] = b; drv[(size_t) i].setTargetValue (b.drive * 0.01f); }
    void setMix (float pct) { mixSm.setTargetValue (pct * 0.01f); }

    static inline float shape (ExType t, float x, float k) noexcept
    {
        // each curve returns only the harmonics (the linear part is removed) so the tone/level stays
        switch (t)
        {
            case ExType::Tube:        { const float b = 0.35f; return (std::tanh (k * x + b) - std::tanh (b)) / (k * (1 - std::tanh (b) * std::tanh (b))) - x; }
            case ExType::Tape:        { const float y = k * x; return (y / std::sqrt (1.0f + y * y * 0.6f)) / k - x; }
            case ExType::Transformer: { const float y = k * x; return (y - 0.18f * y * y * y + 0.12f * y * std::abs (y)) / (1 + 0.6f * y * y) / k - x; }
            case ExType::Console:     { const float y = k * x; return std::sin (juce::jlimit (-1.5707963f, 1.5707963f, y)) / k - x + 0.06f * y * y / k; }
            case ExType::Crystal:     { const float y = k * x; return (std::abs (y) - 0.0f) * 0.35f / k + (std::tanh (y) / k - x) * 0.4f; }
        }
        return 0;
    }

    inline void process (float& l, float& r) noexcept
    {
        float bl[B], br[B];
        xo.split (0, l, bl); xo.split (1, r, br);
        bool anySolo = false; for (auto& b : bands) anySolo |= b.f.solo;
        const float mix = mixSm.getNextValue();
        float ol = 0, orr = 0;
        for (int k = 0; k < B; ++k)
        {
            const auto& b = bands[(size_t) k];
            float L = bl[k], R = br[k];
            const float d = drv[(size_t) k].getNextValue();
            if (b.f.on && d > 0.0005f)
            {
                float a, c, keep;
                const int n = ChanIO::split (b.f.chan, L, R, a, c, keep);
                const float kd = 1.0f + 7.0f * d, amt = mix * (0.25f + 1.6f * d);
                a += amt * dc[(size_t) k * 2].process (0, shape (b.type, a, kd));
                if (n == 2) c += amt * dc[(size_t) k * 2 + 1].process (0, shape (b.type, c, kd));
                ChanIO::join (b.f.chan, a, c, keep, L, R);
            }
            if (b.f.solo) { float sl, sr; ChanIO::listen (b.f.chan, L, R, sl, sr); ol += sl; orr += sr; }
            else if (! anySolo) { ol += L; orr += R; }
        }
        l = ol; r = orr;
    }

private:
    double fs = 96000;
    Crossover4 xo;
    std::array<Band, B> bands;
    std::array<Biquad, B * 2> dc;     // high-pass on the generated harmonics (removes DC / envelope rumble)
    float hpEdge[B] { -1, -1, -1, -1 };
    std::array<juce::SmoothedValue<float>, B> drv;
    juce::SmoothedValue<float> mixSm;
};

//==============================================================================
/** 4-band stereo imager (mid/side width) with per-band correlation metering. */
class MultibandImager
{
public:
    static constexpr int B = 4;
    struct Band { bool solo = false; float width = 100; };
    void prepare (double sampleRate)
    {
        fs = sampleRate; xo.prepare (fs);
        for (auto& w : wsm) w.reset (fs, 0.05);
        corrA = (float) (1.0 - std::exp (-1.0 / (0.3 * fs)));
        for (auto& c : acc) c = { 0, 0, 0 };
    }
    void reset() { xo.reset(); }
    void setCrossover (float a, float b, float c) { xo.setFreqs (a, b, c); }
    void setBand (int i, const Band& b) { bands[(size_t) i] = b; wsm[(size_t) i].setTargetValue (b.width * 0.01f); }

    inline void process (float& l, float& r) noexcept
    {
        float bl[B], br[B];
        xo.split (0, l, bl); xo.split (1, r, br);
        bool anySolo = false; for (auto& b : bands) anySolo |= b.solo;
        float ol = 0, orr = 0;
        for (int k = 0; k < B; ++k)
        {
            const float w = wsm[(size_t) k].getNextValue();
            const float m = 0.5f * (bl[k] + br[k]), s = 0.5f * (bl[k] - br[k]) * w;
            const float L = m + s, R = m - s;
            auto& a = acc[(size_t) k];
            a[0] += corrA * (L * R - a[0]); a[1] += corrA * (L * L - a[1]); a[2] += corrA * (R * R - a[2]);
            if (bands[(size_t) k].solo || ! anySolo) { ol += L; orr += R; }
        }
        l = ol; r = orr;
    }
    float correlation (int k) const
    {
        const auto& a = acc[(size_t) k];
        const float d = std::sqrt (a[1] * a[2]);
        return d < 1.0e-9f ? 1.0f : juce::jlimit (-1.0f, 1.0f, a[0] / d);
    }

private:
    double fs = 48000;
    Crossover4 xo;
    std::array<Band, B> bands;
    std::array<juce::SmoothedValue<float>, B> wsm;
    std::array<std::array<float, 3>, B> acc {};
    float corrA = 0.0001f;
};

//==============================================================================
/** Master rebalance: 4-band level shift, every band on its own channel mode. */
class Rebalance
{
public:
    static constexpr int B = 4;
    struct Band { BandFlags f; float gain = 0; };
    void prepare (double sampleRate) { fs = sampleRate; xo.prepare (fs); for (auto& g : gsm) g.reset (fs, 0.05); }
    void reset() { xo.reset(); }
    void setCrossover (float a, float b, float c) { xo.setFreqs (a, b, c); }
    void setBand (int i, const Band& b) { bands[(size_t) i] = b; gsm[(size_t) i].setTargetValue (b.f.on ? dbToGain (b.gain) : 1.0f); }
    inline void process (float& l, float& r) noexcept
    {
        float bl[B], br[B];
        xo.split (0, l, bl); xo.split (1, r, br);
        bool anySolo = false; for (auto& b : bands) anySolo |= b.f.solo;
        float ol = 0, orr = 0;
        for (int k = 0; k < B; ++k)
        {
            const auto& b = bands[(size_t) k];
            float L = bl[k], R = br[k];
            const float g = gsm[(size_t) k].getNextValue();
            float a, c, keep;
            const int n = ChanIO::split (b.f.chan, L, R, a, c, keep);
            a *= g; if (n == 2) c *= g;
            ChanIO::join (b.f.chan, a, c, keep, L, R);
            if (b.f.solo) { float sl, sr; ChanIO::listen (b.f.chan, L, R, sl, sr); ol += sl; orr += sr; }
            else if (! anySolo) { ol += L; orr += R; }
        }
        l = ol; r = orr;
    }
private:
    double fs = 48000;
    Crossover4 xo;
    std::array<Band, B> bands;
    std::array<juce::SmoothedValue<float>, B> gsm;
};
} // namespace ms
