#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
const juce::Colour nordRed   (0xffa6192e);
const juce::Colour panelGrey (0xff37474f);
const juce::Colour panelDark (0xff2b363c);
const juce::Colour panelText (0xffe8ecef);

juce::Rectangle<int> topPanelBounds()   { return { 190, 12, 900, 146 }; }
juce::Rectangle<int> organPanelBounds() { return { 16, 170, 616, 214 }; }
juce::Rectangle<int> synthPanelBounds() { return { 648, 170, 616, 214 }; }

void styleRotary (juce::Slider& s)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setColour (juce::Slider::rotarySliderFillColourId, juce::Colours::white);
    s.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colours::black);
}
void styleLabel (juce::Label& l)
{
    l.setColour (juce::Label::textColourId, panelText);
    l.setFont (juce::Font (juce::FontOptions (11.0f)));
    l.setJustificationType (juce::Justification::centred);
}
}

NordPianoAudioProcessorEditor::NordPianoAudioProcessorEditor (NordPianoAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setSize (1280, 560);

    loadButton.onClick = [this] { openFileChooser(); };
    addAndMakeVisible (loadButton);

    fileLabel.setText ("Load: WAV/AIFF, .npno piano, .ns4o organ preset, .ns4y synth preset (.nsmp is next)",
                       juce::dontSendNotification);
    fileLabel.setColour (juce::Label::textColourId, panelText);
    fileLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
    addAndMakeVisible (fileLabel);

    for (auto* s : { &volumeSlider, &reverbSlider, &tuneSlider, &stretchSlider })
    {
        styleRotary (*s);
        addAndMakeVisible (s);
    }
    for (auto* l : { &volumeLabel, &reverbLabel, &tuneLabel, &stretchLabel })
    {
        styleLabel (*l);
        addAndMakeVisible (l);
    }
    volumeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.parameters, "volume", volumeSlider);
    reverbAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.parameters, "reverb", reverbSlider);
    tuneAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.parameters, "tuneCents", tuneSlider);
    stretchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.parameters, "stretch", stretchSlider);

    hzLabel.setColour (juce::Label::textColourId, panelText);
    hzLabel.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    hzLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (hzLabel);
    hz440Button.onClick = [this] { setTuneHz (440.0); };
    hz442Button.onClick = [this] { setTuneHz (442.0); };
    hz444Button.onClick = [this] { setTuneHz (444.0); };
    addAndMakeVisible (hz440Button);
    addAndMakeVisible (hz442Button);
    addAndMakeVisible (hz444Button);

    //---- Organ section ------------------------------------------------------
    static const char* footage[9] = { "16", "5 1/3", "8", "4", "2 2/3", "2", "1 3/5", "1 1/3", "1" };
    for (int i = 0; i < 9; ++i)
    {
        auto& s = drawbarSliders[i];
        s.setSliderStyle (juce::Slider::LinearVertical);
        s.setRange (0.0, 8.0, 1.0);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 36, 16);
        s.setColour (juce::Slider::trackColourId, juce::Colours::white);
        s.setColour (juce::Slider::textBoxTextColourId, panelText);
        s.setColour (juce::Slider::textBoxBackgroundColourId, panelDark);
        s.setColour (juce::Slider::textBoxOutlineColourId, panelGrey);
        s.setColour (juce::Slider::thumbColourId, i < 2 ? juce::Colour (0xff6d4c41) : juce::Colours::white);
        s.onValueChange = [this, i] { processor.organState.drawbars[i] = (int) drawbarSliders[i].getValue(); };
        addAndMakeVisible (s);
        drawbarLabels[i].setText (footage[i], juce::dontSendNotification);
        styleLabel (drawbarLabels[i]);
        addAndMakeVisible (drawbarLabels[i]);
    }
    organModelBox.addItemList ({ "B3", "VOX", "FARF", "PIPE1", "PIPE2", "B3 BASS" }, 1);
    organModelBox.onChange = [this]
    {
        if (organModelBox.getSelectedId() > 0)
            processor.organState.model = organModelBox.getSelectedId() - 1;
    };
    addAndMakeVisible (organModelBox);
    styleLabel (organModelLabel);
    addAndMakeVisible (organModelLabel);

    percToggle.onClick = [this] { processor.organState.percOn = percToggle.getToggleState(); };
    perc3rdToggle.onClick = [this] { processor.organState.perc3rd = perc3rdToggle.getToggleState(); };
    percFastToggle.onClick = [this] { processor.organState.percFast = percFastToggle.getToggleState(); };
    percSoftToggle.onClick = [this] { processor.organState.percSoft = percSoftToggle.getToggleState(); };
    vibToggle.onClick = [this] { processor.organState.vibChorusOn = vibToggle.getToggleState(); };
    for (auto* t : { &percToggle, &perc3rdToggle, &percFastToggle, &percSoftToggle, &vibToggle })
    {
        t->setColour (juce::ToggleButton::textColourId, panelText);
        addAndMakeVisible (t);
    }

    //---- Synth section ------------------------------------------------------
    static const char* knobNames[6] = { "WAVE", "FREQ", "RESO", "ATTACK", "DECAY", "RELEASE" };
    for (int i = 0; i < 6; ++i)
    {
        auto& s = synthKnobs[i];
        styleRotary (s);
        s.setRange (0.0, i == 0 ? 63.0 : 127.0, 1.0);
        s.onValueChange = [this, i]
        {
            const int v = (int) synthKnobs[i].getValue();
            auto& st = processor.synthState;
            switch (i)
            {
                case 0: st.analogWave = v; break;
                case 1: st.filterFreq = v; break;
                case 2: st.filterReso = v; break;
                case 3: st.ampAttack = v; break;
                case 4: st.ampDecay = v; break;
                case 5: st.ampRelease = v; break;
                default: break;
            }
        };
        addAndMakeVisible (s);
        synthKnobLabels[i].setText (knobNames[i], juce::dontSendNotification);
        styleLabel (synthKnobLabels[i]);
        addAndMakeVisible (synthKnobLabels[i]);
    }
    filterTypeBox.addItemList ({ "LP12", "LP24", "LPM", "LPHP", "BP", "HP" }, 1);
    filterTypeBox.onChange = [this]
    {
        if (filterTypeBox.getSelectedId() > 0)
            processor.synthState.filterType = filterTypeBox.getSelectedId() - 1;
    };
    addAndMakeVisible (filterTypeBox);
    styleLabel (filterTypeLabel);
    addAndMakeVisible (filterTypeLabel);
    synthSourceLabel.setColour (juce::Label::textColourId, panelText);
    synthSourceLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    addAndMakeVisible (synthSourceLabel);

    keyboard.setAvailableRange (21, 108); // A0 - C8, like an 88-key piano
    addAndMakeVisible (keyboard);

    setOrganControlsEnabled (false);
    setSynthControlsEnabled (false);
    timerCallback();
    startTimer (250);
}

NordPianoAudioProcessorEditor::~NordPianoAudioProcessorEditor() = default;

void NordPianoAudioProcessorEditor::setOrganControlsEnabled (bool enabled)
{
    for (auto& s : drawbarSliders)
        s.setEnabled (enabled);
    organModelBox.setEnabled (enabled);
    for (auto* t : { &percToggle, &perc3rdToggle, &percFastToggle, &percSoftToggle, &vibToggle })
        t->setEnabled (enabled);
}

void NordPianoAudioProcessorEditor::setSynthControlsEnabled (bool enabled)
{
    for (auto& s : synthKnobs)
        s.setEnabled (enabled);
    filterTypeBox.setEnabled (enabled);
}

void NordPianoAudioProcessorEditor::refreshOrganControls()
{
    const auto& st = processor.organState;
    for (int i = 0; i < 9; ++i)
        drawbarSliders[i].setValue (st.drawbars[i].load(), juce::dontSendNotification);
    organModelBox.setSelectedId (st.model.load() + 1, juce::dontSendNotification);
    percToggle.setToggleState (st.percOn.load(), juce::dontSendNotification);
    perc3rdToggle.setToggleState (st.perc3rd.load(), juce::dontSendNotification);
    percFastToggle.setToggleState (st.percFast.load(), juce::dontSendNotification);
    percSoftToggle.setToggleState (st.percSoft.load(), juce::dontSendNotification);
    vibToggle.setToggleState (st.vibChorusOn.load(), juce::dontSendNotification);
}

void NordPianoAudioProcessorEditor::refreshSynthControls()
{
    const auto& st = processor.synthState;
    synthKnobs[0].setValue (st.analogWave.load(), juce::dontSendNotification);
    synthKnobs[1].setValue (st.filterFreq.load(), juce::dontSendNotification);
    synthKnobs[2].setValue (st.filterReso.load(), juce::dontSendNotification);
    synthKnobs[3].setValue (st.ampAttack.load(), juce::dontSendNotification);
    synthKnobs[4].setValue (st.ampDecay.load(), juce::dontSendNotification);
    synthKnobs[5].setValue (st.ampRelease.load(), juce::dontSendNotification);
    filterTypeBox.setSelectedId (st.filterType.load() + 1, juce::dontSendNotification);
    static const char* oscTypes[] = { "ANALOG", "FM-H", "FM-I", "WAVE" };
    synthSourceLabel.setText (
        st.analog.load()
            ? juce::String ("Osc: ") + (st.analogType.load() >= 0 && st.analogType.load() < 4
                                            ? oscTypes[st.analogType.load()] : "ANALOG")
            : juce::String ("Sample-based preset (fallback voice)"),
        juce::dontSendNotification);
}

void NordPianoAudioProcessorEditor::setTuneHz (double hz)
{
    const double cents = 1200.0 * std::log2 (hz / 440.0);
    if (auto* param = processor.parameters.getParameter ("tuneCents"))
    {
        const float normalised = (float) ((cents + 100.0) / 200.0);
        param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalised));
    }
}

void NordPianoAudioProcessorEditor::timerCallback()
{
    const float cents = processor.parameters.getRawParameterValue ("tuneCents")->load();
    const double hz = 440.0 * std::pow (2.0, cents / 1200.0);
    hzLabel.setText ("A = " + juce::String (hz, 1) + " Hz", juce::dontSendNotification);

    const int organLoad = processor.organState.loadCounter.load();
    if (organLoad != lastOrganLoad)
    {
        lastOrganLoad = organLoad;
        refreshOrganControls();
    }
    setOrganControlsEnabled (processor.organState.active.load());

    const int synthLoad = processor.synthState.loadCounter.load();
    if (synthLoad != lastSynthLoad)
    {
        lastSynthLoad = synthLoad;
        refreshSynthControls();
    }
    setSynthControlsEnabled (processor.synthState.active.load());
}

void NordPianoAudioProcessorEditor::openFileChooser()
{
    chooser = std::make_unique<juce::FileChooser> (
        "Load a sample file",
        juce::File{},
        "*.wav;*.aif;*.aiff;*.npno;*.nsmp;*.nsmp3;*.nsmp4;*.ns4o;*.ns4y");

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file.existsAsFile())
            {
                processor.loadSampleFile (file);
                fileLabel.setText (processor.getLoadedFileName(), juce::dontSendNotification);
            }
        });
}

void NordPianoAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    g.setColour (nordRed);
    g.fillRect (bounds);

    g.setColour (panelGrey);
    g.fillRoundedRectangle (topPanelBounds().toFloat(), 6.0f);
    g.setColour (panelDark);
    g.fillRoundedRectangle (organPanelBounds().toFloat(), 6.0f);
    g.fillRoundedRectangle (synthPanelBounds().toFloat(), 6.0f);

    g.setColour (panelText);
    g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
    g.drawText ("ORGAN", organPanelBounds().withHeight (24), juce::Justification::centred, false);
    g.drawText ("SYNTH", synthPanelBounds().withHeight (24), juce::Justification::centred, false);
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText ("preset controls - load an .ns4o / .ns4y file to enable",
                organPanelBounds().withTrimmedTop (organPanelBounds().getHeight() - 18).withHeight (16),
                juce::Justification::centred, false);
    g.drawText ("preset controls - load an .ns4o / .ns4y file to enable",
                synthPanelBounds().withTrimmedTop (synthPanelBounds().getHeight() - 18).withHeight (16),
                juce::Justification::centred, false);

    // Plugin name at the right, like the hardware model name
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions (27.0f)));
    g.drawText ("nord piano", 1104, 44, 164, 36, juce::Justification::right, false);
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText ("AU PLUGIN EDITION", 1104, 82, 164, 16, juce::Justification::right, false);
    g.drawText ("v0.4", 20, 20, 100, 16, juce::Justification::left, false);
}

void NordPianoAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    keyboard.setBounds (bounds.removeFromBottom (150).reduced (10, 5));

    const auto top = topPanelBounds();
    loadButton.setBounds (top.getX() + 16, top.getY() + 10, 130, 28);
    fileLabel.setBounds (top.getX() + 156, top.getY() + 10, top.getWidth() - 172, 28);

    const int knobY = top.getY() + 48;
    volumeSlider.setBounds (top.getX() + 16, knobY, 68, 68);
    volumeLabel.setBounds (top.getX() + 16, knobY + 70, 68, 14);
    reverbSlider.setBounds (top.getX() + 116, knobY, 68, 68);
    reverbLabel.setBounds (top.getX() + 116, knobY + 70, 68, 14);
    tuneSlider.setBounds (top.getX() + 216, knobY, 68, 68);
    tuneLabel.setBounds (top.getX() + 216, knobY + 70, 68, 14);
    stretchSlider.setBounds (top.getX() + 316, knobY, 68, 68);
    stretchLabel.setBounds (top.getX() + 316, knobY + 70, 68, 14);

    hzLabel.setBounds (top.getX() + 430, knobY + 2, 130, 20);
    hz440Button.setBounds (top.getX() + 424, knobY + 28, 40, 26);
    hz442Button.setBounds (top.getX() + 468, knobY + 28, 40, 26);
    hz444Button.setBounds (top.getX() + 512, knobY + 28, 40, 26);

    const auto organ = organPanelBounds();
    for (int i = 0; i < 9; ++i)
    {
        const int x = organ.getX() + 24 + i * 50;
        drawbarLabels[i].setBounds (x, organ.getY() + 28, 44, 14);
        drawbarSliders[i].setBounds (x, organ.getY() + 44, 44, 140);
    }
    organModelLabel.setBounds (organ.getX() + 486, organ.getY() + 28, 110, 14);
    organModelBox.setBounds (organ.getX() + 486, organ.getY() + 44, 110, 24);
    percToggle.setBounds (organ.getX() + 486, organ.getY() + 76, 120, 22);
    perc3rdToggle.setBounds (organ.getX() + 486, organ.getY() + 100, 120, 22);
    percFastToggle.setBounds (organ.getX() + 486, organ.getY() + 124, 120, 22);
    percSoftToggle.setBounds (organ.getX() + 486, organ.getY() + 148, 120, 22);
    vibToggle.setBounds (organ.getX() + 486, organ.getY() + 172, 120, 22);

    const auto synth = synthPanelBounds();
    for (int i = 0; i < 6; ++i)
    {
        const int x = synth.getX() + 28 + i * 98;
        synthKnobs[i].setBounds (x, synth.getY() + 44, 64, 64);
        synthKnobLabels[i].setBounds (x, synth.getY() + 110, 64, 14);
    }
    filterTypeLabel.setBounds (synth.getX() + 28, synth.getY() + 140, 150, 14);
    filterTypeBox.setBounds (synth.getX() + 28, synth.getY() + 156, 150, 24);
    synthSourceLabel.setBounds (synth.getX() + 200, synth.getY() + 156, 380, 24);
}
