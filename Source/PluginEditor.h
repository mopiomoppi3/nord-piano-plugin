#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

// Hardware-style look and feel (v0.5): drawn knobs with value arcs, drawbar
// handles (brown for the two sub footages, cream for the rest), LED toggle
// buttons and dark combo boxes, so the editor reads like the red hardware
// panel instead of stock JUCE widgets.
class NordLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};

// Nord Piano 5 style editor, v0.5 layout (1280x560):
//  wood side cheeks, red panel with darker inset sections
//  top panel: LCD file display + load, VOLUME / REVERB / TUNE / STRETCH, A-Hz presets
//  ORGAN section: 9 drawbars, model selector, percussion + vib/chorus switches
//  SYNTH section: wave / filter / amp-envelope knobs, filter type selector
//  bottom: full-width 88-key keyboard with a felt strip
// Section controls write the live engine states in the processor, so every
// control on screen is wired to the sound. A section is dimmed until a
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
    NordLookAndFeel lookAndFeel;

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
