#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

// Nord Piano 5 style editor, v0.4 layout (1280x560):
//  top panel: load + file status, VOLUME / REVERB / TUNE / STRETCH, A-Hz presets
//  ORGAN section: 9 drawbars, model selector, percussion + vib/chorus switches
//  SYNTH section: wave / filter / amp-envelope knobs, filter type selector
//  bottom: full keyboard
// Section controls write the live engine states in the processor, so every
// control on screen is wired to the sound. A section is greyed out until a
// preset of that kind is loaded.
class NordPianoAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit NordPianoAudioProcessorEditor (NordPianoAudioProcessor&);
    ~NordPianoAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void openFileChooser();
    void setTuneHz (double hz);
    void timerCallback() override;
    void refreshOrganControls();
    void refreshSynthControls();
    void setOrganControlsEnabled (bool);
    void setSynthControlsEnabled (bool);

    NordPianoAudioProcessor& processor;

    juce::TextButton loadButton { "Load File..." };
    juce::Label fileLabel;
    juce::Slider volumeSlider, reverbSlider, tuneSlider, stretchSlider;
    juce::Label volumeLabel { {}, "VOLUME" };
    juce::Label reverbLabel { {}, "REVERB" };
    juce::Label tuneLabel { {}, "TUNE" };
    juce::Label stretchLabel { {}, "STRETCH" };
    juce::Label hzLabel;
    juce::TextButton hz440Button { "440" };
    juce::TextButton hz442Button { "442" };
    juce::TextButton hz444Button { "444" };

    // Organ section
    juce::Slider drawbarSliders[9];
    juce::Label drawbarLabels[9];
    juce::ComboBox organModelBox;
    juce::Label organModelLabel { {}, "MODEL" };
    juce::ToggleButton percToggle { "Percussion" };
    juce::ToggleButton perc3rdToggle { "3rd harmonic" };
    juce::ToggleButton percFastToggle { "Fast decay" };
    juce::ToggleButton percSoftToggle { "Soft volume" };
    juce::ToggleButton vibToggle { "Vib / Chorus" };

    // Synth section
    juce::Slider synthKnobs[6]; // WAVE, FREQ, RESO, ATTACK, DECAY, RELEASE
    juce::Label synthKnobLabels[6];
    juce::ComboBox filterTypeBox;
    juce::Label filterTypeLabel { {}, "FILTER" };
    juce::Label synthSourceLabel;

    juce::MidiKeyboardComponent keyboard;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volumeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> reverbAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tuneAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> stretchAttachment;
    std::unique_ptr<juce::FileChooser> chooser;

    int lastOrganLoad = -1;
    int lastSynthLoad = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NordPianoAudioProcessorEditor)
};
