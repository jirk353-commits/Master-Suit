#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/Modules.h"
#include "DSP/Loudness.h"

namespace ids
{
    inline juce::String b (const char* base, int n, const char* what) { return juce::String (base) + juce::String (n) + "_" + what; }
}

/** Module order in the chain. EQ, Dynamics and Loudness are always inserted. */
enum class Mod { EQ = 0, Rebalance, Dynamics, Exciter, Imager, Loudness, Count };
inline const char* modName (Mod m)
{
    static const char* n[] = { "EQ", "REBALANCE", "DYNAMICS", "EXCITER", "IMAGER", "LOUDNESS" };
    return n[(int) m];
}
inline const char* modKey (Mod m)
{
    static const char* k[] = { "eq", "rb", "dyn", "ex", "im", "lu" };
    return k[(int) m];
}
inline bool modIsCore (Mod m) { return m == Mod::EQ || m == Mod::Dynamics || m == Mod::Loudness; }

class MasterSuiteProcessor : public juce::AudioProcessor
{
public:
    MasterSuiteProcessor();
    ~MasterSuiteProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Master Suite"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    float get (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }
    void setParam (const juce::String& id, float value);
    bool isInserted (Mod m) const { return modIsCore (m) || get (juce::String (modKey (m)) + "_in") > 0.5f; }
    void setInserted (Mod m, bool on);
    /** sets the maximizer gain so the integrated loudness lands on the chosen target */
    bool matchTarget();
    double getFs() const { return fs; }

    /** processing order of the movable modules (Loudness is always last) */
    static constexpr int numMovable = (int) Mod::Loudness;
    std::array<int, numMovable> getOrder() const { std::array<int, numMovable> o; for (int i = 0; i < numMovable; ++i) o[(size_t) i] = order[i].load(); return o; }
    void setOrder (const std::array<int, numMovable>&);

    // ---- meters for the editor (written by the audio thread) ----
    std::atomic<float> dynGr[4] {}, glueGr { 0 }, limGr { 0 };
    std::atomic<float> corr[4] {}, corrAll { 1 };
    std::atomic<float> inPeak { -100 }, outPeak { -100 };
    ms::LoudnessMeter meter;
    ms::ScopeFifo<32768> preFifo, postFifo;     // mid / side, before and after the chain (spectrum)
    ms::ScopeFifo<16384> scopeFifo;            // left / right output (vectorscope)

private:
    void pullParams();
    void cacheParams();
    // parameter pointers cached once, so the audio thread never builds strings or searches
    struct P { std::atomic<float>* p = nullptr; float operator()() const { return p->load(); } };
    P eqP[6][6], rbP[4][4], dynP[4][8], exP[4][5], imP[4][2];
    P gIn, gOut, gListen, gXo[3], gOn[(int) Mod::Count], gIns[(int) Mod::Count], gEqListen,
      glueP[9], exMix, luGainP, luCeil, luRel;

    double fs = 48000;
    ms::ParametricEQ eq;
    ms::Rebalance rb;
    ms::MultibandDynamics dyn;
    ms::MultibandExciter ex;
    ms::MultibandImager im;
    ms::TruePeakLimiter lim;
    juce::dsp::Oversampling<float> os { 2, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true };
    juce::SmoothedValue<float> inGain, outGain, luGain;
    juce::AudioBuffer<float> monoScratch;
    std::vector<float> fA, fB;
    float corrAcc[3] {};
    float corrCoef = 0.0001f;
    int currentPreset = 0, osMax = 2048;
    std::atomic<int> order[numMovable] { 0, 1, 2, 3, 4 };
    std::vector<float> rScratch;
    void processModule (Mod, float* L, float* R, int n, bool on);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterSuiteProcessor)
};
