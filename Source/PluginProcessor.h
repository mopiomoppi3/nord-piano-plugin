#pragma once

#include <JuceHeader.h>
#include "Engines.h"

// Nord Piano - v0.4
// Real in this build: AU/VST3/Standalone synth, MIDI input, WAV/AIFF samples,
// .npno piano decoding (zones, velocity layers, sustain loops), piano tuning
// (master tune in cents with 440/442/444 presets, stretch tuning), .ns4o organ
// presets with a full live panel (9 drawbars, model, percussion, vib/chorus
// switch), .ns4y synth presets with a full live panel (wave, filter type /
// freq / reso, amp env), master volume, reverb mix, on-screen keyboard.
// Not yet: .nsmp sample-library decoding, key-release strokes, organ
// scanner-chorus + rotary sound modelling (the vib/chorus switch drives a
// vibrato approximation), synth FM/WAVE engines, per-note tuning.
class NordPianoAudioProcessor : public juce::AudioProcessor
{
public:
    NordPianoAudioProcessor();
    ~NordPianoAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Called from the editor's file chooser.
    // Returns true if the file was decoded and mapped across the keyboard.
    bool loadSampleFile (const juce::File& file);
    juce::String getLoadedFileName() const { return loadedFileName; }
    bool isNordFilePending() const { return nordFilePending; }

    juce::MidiKeyboardState keyboardState;
    juce::AudioProcessorValueTreeState parameters;

    // Live engine states shared with the editor controls and the voices.
    OrganState organState;
    SynthState synthState;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::Synthesiser synth;
    juce::AudioFormatManager formatManager;

    juce::Reverb reverb;
    juce::Reverb::Parameters reverbParams;

    std::atomic<float>* volumeParam = nullptr;
    std::atomic<float>* reverbParam = nullptr;
    std::atomic<float>* tuneParam = nullptr;
    std::atomic<float>* stretchParam = nullptr;

    PianoTuning pianoTuning;

    juce::String loadedFileName;
    bool nordFilePending = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NordPianoAudioProcessor)
};
