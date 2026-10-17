#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <array>

namespace ms
{
/** Where a band (or module) works: both channels, only mid, only side, only left or only right. */
enum class Chan { Stereo = 0, Mid, Side, Left, Right };
inline const juce::StringArray& chanNames() { static const juce::StringArray s { "Stereo", "Mid", "Side", "Left", "Right" }; return s; }

inline float dbToGain (float db) noexcept { return std::exp (0.11512925465f * db); }
inline float gainToDb (float g) noexcept  { return 8.68588963807f * std::log (std::max (g, 1.0e-9f)); }

/** Splits a stereo sample into the part a band processes, and puts it back. */
struct ChanIO
{
    // returns how many components (1 or 2) are processed; a/b hold them
    static inline int split (Chan c, float l, float r, float& a, float& b, float& keep) noexcept
    {
        switch (c)
        {
            case Chan::Stereo: a = l; b = r; keep = 0; return 2;
            case Chan::Mid:    a = 0.5f * (l + r); keep = 0.5f * (l - r); b = 0; return 1;
            case Chan::Side:   a = 0.5f * (l - r); keep = 0.5f * (l + r); b = 0; return 1;
            case Chan::Left:   a = l; keep = r; b = 0; return 1;
            case Chan::Right:  a = r; keep = l; b = 0; return 1;
        }
        a = l; b = r; keep = 0; return 2;
    }
    static inline void join (Chan c, float a, float b, float keep, float& l, float& r) noexcept
    {
        switch (c)
        {
            case Chan::Stereo: l = a; r = b; return;
            case Chan::Mid:    l = a + keep; r = a - keep; return;
            case Chan::Side:   l = keep + a; r = keep - a; return;
            case Chan::Left:   l = a; r = keep; return;
            case Chan::Right:  l = keep; r = a; return;
        }
    }
    /** what you hear when you listen to (solo) a component: mid / side in mono, left / right in mono */
    static inline void listen (Chan c, float l, float r, float& ol, float& orr) noexcept
    {
        switch (c)
        {
            case Chan::Stereo: ol = l; orr = r; return;
            case Chan::Mid:    ol = orr = 0.5f * (l + r); return;
            case Chan::Side:   ol = orr = 0.5f * (l - r); return;
            case Chan::Left:   ol = orr = l; return;
            case Chan::Right:  ol = orr = r; return;
        }
    }
};

/** TDF-II biquad in double precision (clean at low frequencies). */
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1[2] {}, z2[2] {};
    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }
    inline float process (int ch, float xf) noexcept
    {
        const double x = xf;
        const double y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return (float) y;
    }
    double magnitudeDb (double f, double fs) const
    {
        const double w = juce::MathConstants<double>::twoPi * f / fs;
        const std::complex<double> z = std::polar (1.0, -w), z2c = z * z;
        const auto num = b0 + b1 * z + b2 * z2c, den = 1.0 + a1 * z + a2 * z2c;
        return 20.0 * std::log10 (std::max (1e-12, std::abs (num / den)));
    }
    void set (double nb0, double nb1, double nb2, double a0, double na1, double na2)
    {
        b0 = nb0 / a0; b1 = nb1 / a0; b2 = nb2 / a0; a1 = na1 / a0; a2 = na2 / a0;
    }
};

enum class EqType { Bell = 0, LowShelf, HighShelf, LowCut, HighCut };
inline const juce::StringArray& eqTypeNames() { static const juce::StringArray s { "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut" }; return s; }

/** RBJ cookbook designs (bilinear, pre-warped). */
inline void designEq (Biquad& q, EqType t, double fs, double f, double gainDb, double Q)
{
    using namespace std;
    f = juce::jlimit (10.0, fs * 0.49, f);
    Q = juce::jlimit (0.1, 18.0, Q);
    const double A = pow (10.0, gainDb / 40.0), w0 = juce::MathConstants<double>::twoPi * f / fs;
    const double cw = cos (w0), sw = sin (w0), alpha = sw / (2.0 * Q);
    switch (t)
    {
        case EqType::Bell:
            q.set (1 + alpha * A, -2 * cw, 1 - alpha * A, 1 + alpha / A, -2 * cw, 1 - alpha / A); break;
        case EqType::LowShelf:
        case EqType::HighShelf:
        {
            // shelves use Q as slope-ish control (0.707 = classic)
            const double sA = sqrt (A), al = sw / 2.0 * sqrt ((A + 1 / A) * (1 / juce::jlimit (0.3, 2.0, Q) - 1) + 2);
            const bool low = t == EqType::LowShelf;
            if (low)
                q.set (A * ((A + 1) - (A - 1) * cw + 2 * sA * al), 2 * A * ((A - 1) - (A + 1) * cw), A * ((A + 1) - (A - 1) * cw - 2 * sA * al),
                       (A + 1) + (A - 1) * cw + 2 * sA * al, -2 * ((A - 1) + (A + 1) * cw), (A + 1) + (A - 1) * cw - 2 * sA * al);
            else
                q.set (A * ((A + 1) + (A - 1) * cw + 2 * sA * al), -2 * A * ((A - 1) + (A + 1) * cw), A * ((A + 1) + (A - 1) * cw - 2 * sA * al),
                       (A + 1) - (A - 1) * cw + 2 * sA * al, 2 * ((A - 1) - (A + 1) * cw), (A + 1) - (A - 1) * cw - 2 * sA * al);
            break;
        }
        case EqType::LowCut:
            q.set ((1 + cw) / 2, -(1 + cw), (1 + cw) / 2, 1 + alpha, -2 * cw, 1 - alpha); break;
        case EqType::HighCut:
            q.set ((1 - cw) / 2, 1 - cw, (1 - cw) / 2, 1 + alpha, -2 * cw, 1 - alpha); break;
    }
}

inline void designBandpass (Biquad& q, double fs, double f, double Q)
{
    f = juce::jlimit (10.0, fs * 0.49, f);
    const double w0 = juce::MathConstants<double>::twoPi * f / fs, alpha = std::sin (w0) / (2.0 * juce::jlimit (0.3, 18.0, Q));
    q.set (alpha, 0, -alpha, 1 + alpha, -2 * std::cos (w0), 1 - alpha);
}

/** Lock-free single-producer single-consumer sample FIFO for analyzers (audio -> GUI). */
template <int Size>
struct ScopeFifo
{
    juce::AbstractFifo fifo { Size };
    std::array<float, Size> a {}, b {};
    void push (const float* x, const float* y, int n)
    {
        int s1, n1, s2, n2;
        fifo.prepareToWrite (n, s1, n1, s2, n2);
        for (int i = 0; i < n1; ++i) { a[(size_t) (s1 + i)] = x[i]; b[(size_t) (s1 + i)] = y[i]; }
        for (int i = 0; i < n2; ++i) { a[(size_t) (s2 + i)] = x[n1 + i]; b[(size_t) (s2 + i)] = y[n1 + i]; }
        fifo.finishedWrite (n1 + n2);
    }
    int pull (float* x, float* y, int n)
    {
        int s1, n1, s2, n2;
        fifo.prepareToRead (n, s1, n1, s2, n2);
        for (int i = 0; i < n1; ++i) { x[i] = a[(size_t) (s1 + i)]; y[i] = b[(size_t) (s1 + i)]; }
        for (int i = 0; i < n2; ++i) { x[n1 + i] = a[(size_t) (s2 + i)]; y[n1 + i] = b[(size_t) (s2 + i)]; }
        fifo.finishedRead (n1 + n2);
        return n1 + n2;
    }
};
} // namespace ms
