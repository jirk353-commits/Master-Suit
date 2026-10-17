#pragma once
#include "Common.h"

namespace ms
{
/** 4-band Linkwitz-Riley (24 dB/oct) crossover. Bands sum back to a flat (allpass) response. */
class Crossover4
{
public:
    static constexpr int B = 4;

    void prepare (double sampleRate) { fs = sampleRate; reset(); setFreqs (120, 1200, 6000, true); }
    void reset()
    {
        for (auto* q : all()) q->reset();
    }
    void setFreqs (float f1, float f2, float f3, bool force = false)
    {
        f1 = juce::jlimit (30.0f, 500.0f, f1);
        f2 = juce::jlimit (f1 * 1.5f, 5000.0f, f2);
        f3 = juce::jlimit (f2 * 1.5f, (float) fs * 0.45f, f3);
        if (! force && f1 == cf[0] && f2 == cf[1] && f3 == cf[2]) return;
        cf = { f1, f2, f3 };
        for (int k = 0; k < 3; ++k)
        {
            for (int s = 0; s < 2; ++s) { bw (lp[k][s], cf[(size_t) k], true); bw (hp[k][s], cf[(size_t) k], false); }
        }
        ap (ap2a, cf[1]); ap (ap3a, cf[2]); ap (ap3b, cf[2]);
    }
    float freq (int k) const { return cf[(size_t) k]; }

    /** splits one channel sample into 4 bands */
    inline void split (int ch, float x, float* out) noexcept
    {
        const float l1 = lp[0][1].process (ch, lp[0][0].process (ch, x));
        const float h1 = hp[0][1].process (ch, hp[0][0].process (ch, x));
        const float l2 = lp[1][1].process (ch, lp[1][0].process (ch, h1));
        const float h2 = hp[1][1].process (ch, hp[1][0].process (ch, h1));
        const float l3 = lp[2][1].process (ch, lp[2][0].process (ch, h2));
        const float h3 = hp[2][1].process (ch, hp[2][0].process (ch, h2));
        out[0] = ap3a.process (ch, ap2a.process (ch, l1));
        out[1] = ap3b.process (ch, l2);
        out[2] = l3;
        out[3] = h3;
    }

private:
    void bw (Biquad& q, double f, bool low)
    {
        const double w0 = juce::MathConstants<double>::twoPi * f / fs, cw = std::cos (w0), alpha = std::sin (w0) / (2.0 * 0.70710678);
        if (low) q.set ((1 - cw) / 2, 1 - cw, (1 - cw) / 2, 1 + alpha, -2 * cw, 1 - alpha);
        else     q.set ((1 + cw) / 2, -(1 + cw), (1 + cw) / 2, 1 + alpha, -2 * cw, 1 - alpha);
    }
    void ap (Biquad& q, double f)
    {
        const double w0 = juce::MathConstants<double>::twoPi * f / fs, cw = std::cos (w0), alpha = std::sin (w0) / (2.0 * 0.70710678);
        q.set (1 - alpha, -2 * cw, 1 + alpha, 1 + alpha, -2 * cw, 1 - alpha);
    }
    std::array<Biquad*, 21> all() { return { &lp[0][0], &lp[0][1], &lp[1][0], &lp[1][1], &lp[2][0], &lp[2][1],
                                             &hp[0][0], &hp[0][1], &hp[1][0], &hp[1][1], &hp[2][0], &hp[2][1],
                                             &ap2a, &ap3a, &ap3b, &ap3a, &ap3a, &ap3a, &ap3a, &ap3a, &ap3a }; }
    double fs = 48000;
    std::array<float, 3> cf { -1, -1, -1 };
    Biquad lp[3][2], hp[3][2], ap2a, ap3a, ap3b;
};
} // namespace ms
