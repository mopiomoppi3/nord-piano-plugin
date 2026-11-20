#pragma once

#include <JuceHeader.h>
#include "NordFile.h"

// Playback engines for the decoded Nord files.
//  - PianoVoice: velocity-layered .npno strokes with sustain loops
//  - OrganVoice: additive drawbar organ for .ns4o presets (+ percussion)
//  - SynthVoice: analog-style oscillator + filter + amp env for .ns4y presets
// Approximations in v0.4 (see README): no key-release strokes yet, organ
// model tone differences and the synth wave-selector table are approximated,
// the vib/chorus switch drives a vibrato approximation (no scanner chorus
// or rotary yet).

// Shared live tuning state for the piano engine. The processor writes it
// from the parameters every block; piano sounds hold a pointer to it, so
// turning the TUNE / STRETCH knobs affects the next notes played.
struct PianoTuning
{
    std::atomic<float> cents { 0.0f };   // master tune, -100..+100 cents (A440 = 0)
    std::atomic<float> stretch { 0.0f }; // stretch tuning amount, 0..1
};

//========================================================================
class PianoZoneSound : public juce::SynthesiserSound
{
public:
    PianoZoneSound (NpnoZone zoneData, PianoTuning* tuningState)
        : zone (std::move (zoneData)), tuning (tuningState) {}

    bool appliesToNote (int midiNoteNumber) override
    {
        return midiNoteNumber >= zone.lowNote && midiNoteNumber <= zone.highNote;
    }
    bool appliesToChannel (int) override { return true; }

    NpnoZone zone;
    PianoTuning* tuning = nullptr;
    // Recordings sound 4 semitones above the key-map root (OpenNord finding).
    int normalPitchNote() const { return zone.rootNote + 4; }
    int pickLayer (int velocity127) const;
};

class PianoVoice : public juce::SynthesiserVoice
{
public:
    bool canPlaySound (juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<PianoZoneSound*> (sound) != nullptr;
    }
    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int pitchWheel) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}
    void renderNextBlock (juce::AudioBuffer<float>&, int startSample, int numSamples) override;

private:
    PianoZoneSound* sound = nullptr;
    const NpnoLayer* layer = nullptr;
    double position = 0.0;
    double pitchRatio = 1.0;
    float level = 0.0f;
    float loopGain = 1.0f;
    bool playing = false;
    bool releasing = false;
    float tail = 1.0f;
};

//========================================================================
// Live organ state: the processor fills it from a .ns4o preset, the editor's
// drawbars/switches write it, and organ voices read it every block - so the
// panel controls are the actual sound, never decoration.
struct OrganState
{
    std::atomic<bool> active { false };
    std::atomic<int> loadCounter { 0 };
    std::atomic<int> drawbars[9] {};
    std::atomic<int> model { 0 };
    std::atomic<float> volume { 1.0f };
    std::atomic<bool> vibChorusOn { false };
    std::atomic<bool> percOn { false };
    std::atomic<bool> perc3rd { false };
    std::atomic<bool> percFast { false };
    std::atomic<bool> percSoft { false };
};

class OrganSound : public juce::SynthesiserSound
{
public:
    explicit OrganSound (OrganState* s) : state (s) {}
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
    OrganState* state = nullptr;
};

class OrganVoice : public juce::SynthesiserVoice
{
public:
    bool canPlaySound (juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<OrganSound*> (sound) != nullptr;
    }
    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int pitchWheel) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}
    void renderNextBlock (juce::AudioBuffer<float>&, int startSample, int numSamples) override;

private:
    OrganSound* sound = nullptr;
    double phases[9] = {};
    double baseFreq = 440.0;
    float level = 0.0f;
    float env = 0.0f;       // click-free attack/release ramp
    bool releasing = false;
    double percPhase = 0.0;
    float percEnv = 0.0f;
    double vibTime = 0.0;   // vibrato LFO clock (vib/chorus switch)
};

//========================================================================
// Live synth state, same pattern as OrganState (from a .ns4y preset).
struct SynthState
{
    std::atomic<bool> active { false };
    std::atomic<int> loadCounter { 0 };
    std::atomic<bool> analog { false };
    std::atomic<int> analogType { 0 };
    std::atomic<int> analogWave { 0 };
    std::atomic<int> filterType { 0 };
    std::atomic<int> filterFreq { 0 };
    std::atomic<int> filterReso { 0 };
    std::atomic<int> ampAttack { 0 };
    std::atomic<int> ampDecay { 0 };
    std::atomic<int> ampRelease { 0 };
    std::atomic<float> volume { 1.0f };
};

class SynthSound : public juce::SynthesiserSound
{
public:
    explicit SynthSound (SynthState* s) : state (s) {}
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
    SynthState* state = nullptr;
};

class SynthVoice : public juce::SynthesiserVoice
{
public:
    bool canPlaySound (juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<SynthSound*> (sound) != nullptr;
    }
    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int pitchWheel) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}
    void renderNextBlock (juce::AudioBuffer<float>&, int startSample, int numSamples) override;

private:
    float waveSample (double phase) const;
    SynthSound* sound = nullptr;
    double phase = 0.0;
    double phaseInc = 0.0;
    float level = 0.0f;
    // amp envelope state: 0 attack, 1 decay, 2 sustain, 3 release, 4 done
    int envStage = 4;
    float envValue = 0.0f;
    juce::IIRFilter filterL, filterR;
};
