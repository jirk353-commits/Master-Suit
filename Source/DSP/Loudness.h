#pragma once
#include "Common.h"
#include <vector>

namespace ms
{
//==============================================================================
/** ITU-R BS.1770-4 / EBU R128 loudness meter: momentary, short-term, integrated, LRA, true peak. */
class LoudnessMeter
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        // K-weighting (coefficients for any sample rate, as in libebur128)
        {
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            const double K = std::tan (juce::MathConstants<double>::pi * f0 / fs);
            const double Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K;
            pre.set (Vh + Vb * K / Q + K * K, 2.0 * (K * K - Vh), Vh - Vb * K / Q + K * K, a0, 2.0 * (K * K - 1.0), 1.0 - K / Q + K * K);
        }
        {
            const double f0 = 38.13547087602444, Q = 0.5003270373238773;
            const double K = std::tan (juce::MathConstants<double>::pi * f0 / fs);
            const double a0 = 1.0 + K / Q + K * K;
            rlb.set (a0, -2.0 * a0, a0, a0, 2.0 * (K * K - 1.0), 1.0 - K / Q + K * K);   // b = [1,-2,1]
        }
        hop = juce::jmax (1, (int) std::round (fs * 0.1));
        tp.prepare();
        resetAll();
    }
    void resetAll()
    {
        pre.reset(); rlb.reset();
        acc = 0; accN = 0; ringPos = 0; ringFill = 0;
        std::fill (std::begin (ring), std::end (ring), 0.0);
        std::fill (std::begin (histI), std::end (histI), 0u);
        std::fill (std::begin (histS), std::end (histS), 0u);
        momentary.store (-100); shortTerm.store (-100); integrated.store (-100); lra.store (0);
        truePeakMax.store (-100); truePeakNow = -100; tp.reset();
        resetRequested.store (false);
    }
    void requestReset() { resetRequested.store (true); }

    inline void process (float l, float r) noexcept
    {
        const float wl = rlb.process (0, pre.process (0, l));
        const float wr = rlb.process (1, pre.process (1, r));
        acc += (double) wl * wl + (double) wr * wr;
        const float pl = tp.process (0, l);
        const float pr = tp.process (1, r);
        const float p = std::max (pl, pr);
        if (p > tpBlock) tpBlock = p;
        if (++accN >= hop) endHop();
    }

    std::atomic<float> momentary { -100 }, shortTerm { -100 }, integrated { -100 }, lra { 0 }, truePeakMax { -100 };
    std::atomic<float> truePeakLive { -100 };

private:
    void endHop()
    {
        if (resetRequested.load()) resetAll();
        ring[ringPos] = acc / (double) accN;
        ringPos = (ringPos + 1) % 30; ringFill = std::min (ringFill + 1, 30);
        acc = 0; accN = 0;
        auto meanOf = [this] (int n)
        {
            double s = 0; const int k = std::min (n, ringFill);
            for (int i = 1; i <= k; ++i) s += ring[(ringPos - i + 30) % 30];
            return k > 0 ? s / (double) n : 0.0;
        };
        const float m = toLufs (meanOf (4)), s = toLufs (meanOf (30));
        momentary.store (m); shortTerm.store (s);
        if (ringFill >= 4 && m > -70.0f) histI[bin (m)]++;
        if (ringFill >= 30 && s > -70.0f) histS[bin (s)]++;
        integrated.store (gatedMean (histI, -10.0f));
        lra.store (range (histS));
        const float tpDb = gainToDb (tpBlock);
        truePeakLive.store (tpDb);
        if (tpDb > truePeakMax.load()) truePeakMax.store (tpDb);
        tpBlock = 0;
    }
    static float toLufs (double ms) { return ms <= 1.0e-12 ? -100.0f : (float) (-0.691 + 10.0 * std::log10 (ms)); }
    static constexpr int NB = 800;   // 0.1 LU bins from -70 to +10 LUFS
    static int bin (float lufs) { return juce::jlimit (0, NB - 1, (int) ((lufs + 70.0f) * 10.0f)); }
    static float centre (int b) { return -70.0f + ((float) b + 0.5f) * 0.1f; }
    static float gatedMean (const unsigned* h, float rel)
    {
        double e = 0; double n = 0;
        for (int b = 0; b < NB; ++b) if (h[b]) { e += h[b] * std::pow (10.0, (centre (b) + 0.691) / 10.0); n += h[b]; }
        if (n <= 0) return -100.0f;
        const float gate = toLufs (e / n) + rel;
        e = 0; n = 0;
        for (int b = 0; b < NB; ++b) if (h[b] && centre (b) >= gate) { e += h[b] * std::pow (10.0, (centre (b) + 0.691) / 10.0); n += h[b]; }
        return n > 0 ? toLufs (e / n) : -100.0f;
    }
    static float range (const unsigned* h)
    {
        double e = 0, n = 0;
        for (int b = 0; b < NB; ++b) if (h[b]) { e += h[b] * std::pow (10.0, (centre (b) + 0.691) / 10.0); n += h[b]; }
        if (n < 10) return 0.0f;
        const float gate = toLufs (e / n) - 20.0f;
        double tot = 0; for (int b = 0; b < NB; ++b) if (centre (b) >= gate) tot += h[b];
        if (tot < 2) return 0.0f;
        double c = 0; float lo = 0, hi = 0; bool loSet = false;
        for (int b = 0; b < NB; ++b)
        {
            if (centre (b) < gate) continue;
            c += h[b];
            if (! loSet && c >= 0.10 * tot) { lo = centre (b); loSet = true; }
            if (c >= 0.95 * tot) { hi = centre (b); break; }
        }
        return std::max (0.0f, hi - lo);
    }

public:
    /** 8x oversampled true-peak estimator (polyphase windowed sinc). */
    struct TruePeak
    {
        static constexpr int P = 8, T = 24;   // phases, taps per phase
        float h[P][T] {}; float buf[2][T] {}; int pos = 0;
        void prepare()
        {
            for (int p = 0; p < P; ++p)
                for (int t = 0; t < T; ++t)
                {
                    const double x = (double) (t - T / 2) + (double) p / P;   // fractional position
                    const double sinc = std::abs (x) < 1e-9 ? 1.0 : std::sin (juce::MathConstants<double>::pi * x) / (juce::MathConstants<double>::pi * x);
                    const double w = 0.5 + 0.5 * std::cos (juce::MathConstants<double>::pi * x / (T / 2 + 1));
                    h[p][t] = (float) (sinc * w);
                }
            reset();
        }
        void reset() { std::fill (&buf[0][0], &buf[0][0] + 2 * T, 0.0f); pos = 0; }
        inline float process (int ch, float x) noexcept
        {
            auto* b = buf[ch];
            if (ch == 0) pos = (pos + 1) % T;
            b[(pos) % T] = x;
            float pk = 0;
            for (int p = 0; p < P; ++p)
            {
                float s = 0;
                for (int t = 0; t < T; ++t) s += h[p][t] * b[(pos - t + T) % T];
                pk = std::max (pk, std::abs (s));
            }
            return pk;
        }
    };

private:
    double fs = 48000;
    Biquad pre, rlb;
    double acc = 0; int accN = 0, hop = 4800;
    double ring[30] {}; int ringPos = 0, ringFill = 0;
    unsigned histI[NB] {}, histS[NB] {};
    TruePeak tp; float tpBlock = 0, truePeakNow = -100;
    std::atomic<bool> resetRequested { false };
};

//==============================================================================
/** Transparent look-ahead true-peak limiter (maximizer). */
class TruePeakLimiter
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        L = juce::jmax (8, (int) std::round (fs * 0.0015));          // 1.5 ms look-ahead
        delay.assign ((size_t) (2 * (L + TPD + 4)), 0.0f);
        dq.assign ((size_t) (L + 3), { 0, 1.0f });
        ma.assign ((size_t) L, 1.0f);
        tp.prepare();
        reset();
    }
    void reset()
    {
        std::fill (delay.begin(), delay.end(), 0.0f); dpos = 0;
        head = tail = 0; n = 0; std::fill (ma.begin(), ma.end(), 1.0f); maSum = (double) L; maPos = 0; env = 1.0f;
        tp.reset(); grMax = 0;
    }
    int latency() const { return L + TPD; }
    void set (float ceilingDb, float releaseMs)
    {
        ceiling = dbToGain (ceilingDb - 0.2f);
        rel = std::exp (-1.0f / (0.001f * juce::jmax (1.0f, releaseMs) * (float) fs));
    }

    inline void process (float& l, float& r) noexcept
    {
        const float pk = std::max ({ tp.process (0, l), tp.process (1, r), std::abs (l), std::abs (r) });
        const float req = pk > ceiling ? ceiling / pk : 1.0f;
        // sliding minimum of the required gain over L samples (monotonic deque)
        const int cap = L + 3;
        while (head != tail && dq[(size_t) ((tail - 1 + cap) % cap)].second >= req) tail = (tail - 1 + cap) % cap;
        dq[(size_t) tail] = { n, req }; tail = (tail + 1) % cap;
        while (dq[(size_t) head].first <= n - (L + 1)) head = (head + 1) % cap;   // window of L+1 samples
        ++n;
        const float mn = dq[(size_t) head].second;
        env = mn < env ? mn : mn + rel * (env - mn);
        // moving average over L: the gain is fully down when the peak leaves the delay line
        maSum += env - ma[(size_t) maPos]; ma[(size_t) maPos] = env; maPos = (maPos + 1) % L;
        const float g = (float) (maSum / (double) L);
        // delay the audio by L
        const int D = (int) delay.size() / 2;
        delay[(size_t) (dpos * 2)] = l; delay[(size_t) (dpos * 2 + 1)] = r;
        const int rp = (dpos - (L + TPD) + D) % D; dpos = (dpos + 1) % D;
        l = juce::jlimit (-ceiling, ceiling, delay[(size_t) (rp * 2)] * g);
        r = juce::jlimit (-ceiling, ceiling, delay[(size_t) (rp * 2 + 1)] * g);
        grMax = std::max (grMax, -gainToDb (g));
    }
    float takeGr() { const float v = grMax; grMax = 0; return v; }

private:
    double fs = 48000;
    int L = 72;
    std::vector<float> delay; int dpos = 0;
    static constexpr int TPD = LoudnessMeter::TruePeak::T / 2;   // group delay of the true-peak detector
    std::vector<std::pair<long long, float>> dq; int head = 0, tail = 0; long long n = 0;
    std::vector<float> ma; double maSum = 0; int maPos = 0;
    float env = 1, ceiling = 0.89f, rel = 0.999f, grMax = 0;
    LoudnessMeter::TruePeak tp;
};

/** Streaming / delivery targets. */
struct Target { const char* name; float lufs; float tp; };
inline const std::array<Target, 10>& targets()
{
    static const std::array<Target, 10> t {{
        { "Spotify",       -14.0f, -1.0f },
        { "Spotify Loud",  -11.0f, -2.0f },
        { "YouTube",       -14.0f, -1.0f },
        { "Apple Music",   -16.0f, -1.0f },
        { "Amazon Music",  -14.0f, -2.0f },
        { "Tidal",         -14.0f, -1.0f },
        { "Deezer",        -15.0f, -1.0f },
        { "SoundCloud",    -14.0f, -1.0f },
        { "CD / Club",      -9.0f, -0.3f },
        { "Custom",        -14.0f, -1.0f },
    }};
    return t;
}
} // namespace ms
