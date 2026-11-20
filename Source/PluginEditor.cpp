#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
const juce::Colour nordRed    (0xffa6192e);
const juce::Colour panelRed   (0xff8f1220); // darker inset section on the red body
const juce::Colour panelEdge  (0xff6d0d1b);
const juce::Colour creamText  (0xfff3e9d2);
const juce::Colour panelText  (0xffe8ecef);
const juce::Colour lcdBack    (0xff0d1417);
const juce::Colour lcdText    (0xffcfe8dc);

juce::Rectangle<int> topPanelBounds()   { return { 120, 12, 890, 146 }; }
juce::Rectangle<int> organPanelBounds() { return { 36, 170, 596, 214 }; }
juce::Rectangle<int> synthPanelBounds() { return { 648, 170, 596, 214 }; }
juce::Rectangle<int> keyboardBounds()   { return { 30, 398, 1220, 152 }; }

void styleRotary (juce::Slider& s)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
}
void styleLabel (juce::Label& l)
{
    l.setColour (juce::Label::textColourId, creamText);
    l.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    l.setJustificationType (juce::Justification::centred);
}
void drawWoodCheek (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (juce::Colour (0xff7c4a26));
    g.fillRect (area);
    for (int i = 0; i < area.getWidth(); ++i)
    {
        const float n = std::sin ((float) i * 12.9898f) * 43758.5453f;
        const float f = n - std::floor (n);
        g.setColour (juce::Colour (0xff633818).interpolatedWith (juce::Colour (0xffa06c36), f).withAlpha (0.35f));
        g.fillRect (area.getX() + i, area.getY(), 1, area.getHeight());
    }
    juce::ColourGradient gloss (juce::Colours::white.withAlpha (0.14f), 0.0f, (float) area.getY(),
                                juce::Colours::transparentWhite, 0.0f, (float) area.getBottom(), false);
    g.setGradientFill (gloss);
    g.fillRect (area);
}
void drawPanel (juce::Graphics& g, juce::Rectangle<int> r)
{
    g.setColour (panelRed);
    g.fillRoundedRectangle (r.toFloat(), 8.0f);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8.0f, 1.0f);
    g.setColour (panelEdge);
    g.drawRoundedRectangle (r.toFloat().expanded (1.0f), 9.0f, 1.0f);
    // corner screws
    for (auto c : { juce::Point<float> ((float) r.getX() + 10, (float) r.getY() + 10),
                    juce::Point<float> ((float) r.getRight() - 10, (float) r.getY() + 10),
                    juce::Point<float> ((float) r.getX() + 10, (float) r.getBottom() - 10),
                    juce::Point<float> ((float) r.getRight() - 10, (float) r.getBottom() - 10) })
    {
        g.setColour (juce::Colour (0xffc9c9c9));
        g.fillEllipse (c.x - 3.0f, c.y - 3.0f, 6.0f, 6.0f);
        g.setColour (juce::Colour (0xff555c60));
        g.drawLine (c.x - 2.2f, c.y, c.x + 2.2f, c.y, 1.2f);
    }
}
}

//==============================================================================
// NordLookAndFeel

void NordLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPosProportional, float rotaryStartAngle,
                                        float rotaryEndAngle, juce::Slider& slider)
{
    juce::Graphics::ScopedSaveState save (g);
    if (! slider.isEnabled())
        g.setOpacity (0.35f);

    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (2.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();
    const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    juce::Path track;
    track.addCentredArc (cx, cy, radius - 2.0f, radius - 2.0f, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colours::white.withAlpha (0.20f));
    g.strokePath (track, juce::PathStrokeType (2.0f));

    juce::Path filled;
    filled.addCentredArc (cx, cy, radius - 2.0f, radius - 2.0f, 0.0f, rotaryStartAngle, angle, true);
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.strokePath (filled, juce::PathStrokeType (2.0f));

    const float bodyR = radius - 7.0f;
    const auto body = juce::Rectangle<float> (cx - bodyR, cy - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    juce::ColourGradient grad (juce::Colour (0xff454a50), body.getX(), body.getY(),
                               juce::Colour (0xff131416), body.getX(), body.getBottom(), false);
    g.setGradientFill (grad);
    g.fillEllipse (body);
    g.setColour (juce::Colours::white.withAlpha (0.28f));
    g.drawEllipse (body.reduced (0.5f), 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawEllipse (body.expanded (1.5f), 1.0f);

    const float px = cx + std::sin (angle) * (bodyR - 4.0f);
    const float py = cy - std::cos (angle) * (bodyR - 4.0f);
    g.setColour (juce::Colours::white);
    g.drawLine (cx, cy, px, py, 2.5f);
}

void NordLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float minSliderPos, float maxSliderPos,
                                        juce::Slider::SliderStyle, juce::Slider& slider)
{
    juce::Graphics::ScopedSaveState save (g);
    if (! slider.isEnabled())
        g.setOpacity (0.35f);

    const float cx = (float) x + (float) width * 0.5f;
    const float top = maxSliderPos;
    const float bottom = minSliderPos;

    // slot
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (cx - 3.0f, top - 6.0f, 6.0f, (bottom - top) + 12.0f, 3.0f);
    g.setColour (juce::Colours::white.withAlpha (0.15f));
    g.drawLine (cx, top - 6.0f, cx, bottom + 6.0f, 1.0f);

    // scale ticks, 0..8
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    for (int i = 0; i <= 8; ++i)
    {
        const float ty = top + (bottom - top) * (float) i / 8.0f;
        g.drawLine (cx - 14.0f, ty, cx - 9.0f, ty, 1.0f);
    }

    // handle: brown for the sub footages, cream for the rest (thumbColourId)
    const auto handleColour = slider.findColour (juce::Slider::thumbColourId);
    const auto handle = juce::Rectangle<float> (cx - (float) width * 0.5f + 5.0f, sliderPos - 10.0f,
                                                (float) width - 10.0f, 20.0f);
    juce::ColourGradient grad (handleColour.brighter (0.25f), handle.getX(), handle.getY(),
                               handleColour.darker (0.25f), handle.getX(), handle.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (handle, 3.0f);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawRoundedRectangle (handle, 3.0f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.drawLine (handle.getX() + 4.0f, sliderPos, handle.getRight() - 4.0f, sliderPos, 1.6f);
}

void NordLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                        bool shouldDrawButtonAsHighlighted, bool)
{
    juce::Graphics::ScopedSaveState save (g);
    if (! button.isEnabled())
        g.setOpacity (0.35f);

    const auto bounds = button.getLocalBounds().toFloat();
    const float ledCx = bounds.getX() + 8.0f;
    const float ledCy = bounds.getCentreY();

    if (shouldDrawButtonAsHighlighted)
    {
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRoundedRectangle (bounds, 4.0f);
    }

    const bool on = button.getToggleState();
    if (on)
    {
        g.setColour (juce::Colour (0xffff6a33).withAlpha (0.35f));
        g.fillEllipse (ledCx - 7.0f, ledCy - 7.0f, 14.0f, 14.0f);
        g.setColour (juce::Colour (0xffff7a40));
    }
    else
    {
        g.setColour (juce::Colour (0xff3d2320));
    }
    g.fillEllipse (ledCx - 4.0f, ledCy - 4.0f, 8.0f, 8.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (ledCx - 4.0f, ledCy - 4.0f, 8.0f, 8.0f, 1.0f);

    g.setColour (button.findColour (juce::ToggleButton::textColourId));
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawText (button.getButtonText(),
                juce::Rectangle<float> (bounds.getX() + 20.0f, bounds.getY(),
                                        bounds.getWidth() - 20.0f, bounds.getHeight()),
                juce::Justification::centredLeft, false);
}

void NordLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                    int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox& box)
{
    juce::Graphics::ScopedSaveState save (g);
    if (! box.isEnabled())
        g.setOpacity (0.35f);

    const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
    juce::ColourGradient grad (juce::Colour (0xff2c3338), 0.0f, 0.0f,
                               juce::Colour (0xff171b1e), 0.0f, (float) height, false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (juce::Colours::white.withAlpha (0.22f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);

    const float ax = (float) (buttonX + buttonW / 2);
    const float ay = (float) (buttonY + buttonH / 2);
    juce::Path arrow;
    arrow.addTriangle (ax - 5.0f, ay - 3.0f, ax + 5.0f, ay - 3.0f, ax, ay + 4.0f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.fillPath (arrow);
}

juce::Font NordLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (juce::FontOptions (13.0f));
}

void NordLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                            const juce::Colour&, bool shouldDrawButtonAsHighlighted,
                                            bool shouldDrawButtonAsDown)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    juce::Colour top (0xff39434a), bottom (0xff1d2226);
    if (shouldDrawButtonAsDown)
    {
        top = juce::Colour (0xff20262b);
        bottom = juce::Colour (0xff121518);
    }
    else if (shouldDrawButtonAsHighlighted)
    {
        top = juce::Colour (0xff46525b);
        bottom = juce::Colour (0xff232a2f);
    }
    juce::ColourGradient grad (top, bounds.getX(), bounds.getY(), bottom, bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (bounds, 5.0f);
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.drawRoundedRectangle (bounds, 5.0f, 1.0f);
}

//==============================================================================

NordPianoAudioProcessorEditor::NordPianoAudioProcessorEditor (NordPianoAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lookAndFeel);
    setSize (1280, 560);

    loadButton.onClick = [this] { openFileChooser(); };
    loadButton.setColour (juce::TextButton::textColourOffId, panelText);
    addAndMakeVisible (loadButton);

    fileLabel.setText ("Load: WAV/AIFF, .npno piano, .ns4o organ preset, .ns4y synth preset (.nsmp is next)",
                       juce::dontSendNotification);
    fileLabel.setColour (juce::Label::textColourId, lcdText);
    fileLabel.setColour (juce::Label::backgroundColourId, lcdBack);
    fileLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
    fileLabel.setBorderSize (juce::BorderSize<int> (6));
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

    hzLabel.setColour (juce::Label::textColourId, creamText);
    hzLabel.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    hzLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (hzLabel);
    hz440Button.onClick = [this] { setTuneHz (440.0); };
    hz442Button.onClick = [this] { setTuneHz (442.0); };
    hz444Button.onClick = [this] { setTuneHz (444.0); };
    for (auto* b : { &hz440Button, &hz442Button, &hz444Button })
    {
        b->setColour (juce::TextButton::textColourOffId, panelText);
        addAndMakeVisible (b);
    }

    //---- Organ section ------------------------------------------------------
    static const char* footage[9] = { "16", "5 1/3", "8", "4", "2 2/3", "2", "1 3/5", "1 1/3", "1" };
    for (int i = 0; i < 9; ++i)
    {
        auto& s = drawbarSliders[i];
        s.setSliderStyle (juce::Slider::LinearVertical);
        s.setRange (0.0, 8.0, 1.0);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 36, 16);
        s.setColour (juce::Slider::textBoxTextColourId, creamText);
        s.setColour (juce::Slider::textBoxBackgroundColourId, panelRed);
        s.setColour (juce::Slider::textBoxOutlineColourId, panelRed);
        s.setColour (juce::Slider::thumbColourId, i < 2 ? juce::Colour (0xff7a4a2b) : juce::Colour (0xfff1e6cf));
        s.onValueChange = [this, i] { processor.organState.drawbars[i] = (int) drawbarSliders[i].getValue(); };
        addAndMakeVisible (s);
        drawbarLabels[i].setText (footage[i], juce::dontSendNotification);
        styleLabel (drawbarLabels[i]);
        addAndMakeVisible (drawbarLabels[i]);
    }
    organModelBox.addItemList ({ "B3", "VOX", "FARF", "PIPE1", "PIPE2", "B3 BASS" }, 1);
    organModelBox.setColour (juce::ComboBox::textColourId, panelText);
    organModelBox.setColour (juce::ComboBox::arrowColourId, panelText);
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
        t->setColour (juce::ToggleButton::textColourId, creamText);
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
    filterTypeBox.setColour (juce::ComboBox::textColourId, panelText);
    filterTypeBox.setColour (juce::ComboBox::arrowColourId, panelText);
    filterTypeBox.onChange = [this]
    {
        if (filterTypeBox.getSelectedId() > 0)
            processor.synthState.filterType = filterTypeBox.getSelectedId() - 1;
    };
    addAndMakeVisible (filterTypeBox);
    styleLabel (filterTypeLabel);
    addAndMakeVisible (filterTypeLabel);
    synthSourceLabel.setColour (juce::Label::textColourId, creamText);
    synthSourceLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    addAndMakeVisible (synthSourceLabel);

    keyboard.setAvailableRange (21, 108); // A0 - C8, like an 88-key piano
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xfff7f1e3));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff14161a));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xffb9ac9c));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, juce::Colour (0x22a6192e));
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, juce::Colour (0x66a6192e));
    keyboard.setColour (juce::MidiKeyboardComponent::textLabelColourId, juce::Colour (0xff555049));
    addAndMakeVisible (keyboard);

    setOrganControlsEnabled (false);
    setSynthControlsEnabled (false);
    timerCallback();
    startTimer (250);
}

NordPianoAudioProcessorEditor::~NordPianoAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

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

    juce::ColourGradient bodyGrad (nordRed.brighter (0.06f), 0.0f, 0.0f,
                                   nordRed.darker (0.12f), 0.0f, (float) bounds.getHeight(), false);
    g.setGradientFill (bodyGrad);
    g.fillRect (bounds);

    drawWoodCheek (g, { 0, 0, 24, bounds.getHeight() });
    drawWoodCheek (g, { bounds.getWidth() - 24, 0, 24, bounds.getHeight() });
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawVerticalLine (24, 0.0f, (float) bounds.getHeight());
    g.drawVerticalLine (bounds.getWidth() - 25, 0.0f, (float) bounds.getHeight());

    drawPanel (g, topPanelBounds());
    drawPanel (g, organPanelBounds());
    drawPanel (g, synthPanelBounds());

    g.setColour (creamText);
    g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
    g.drawText ("ORGAN", organPanelBounds().withHeight (28), juce::Justification::centred, false);
    g.drawText ("SYNTH", synthPanelBounds().withHeight (28), juce::Justification::centred, false);
    g.drawText ("PIANO", topPanelBounds().removeFromRight (170).withHeight (28),
                juce::Justification::centredRight, false);
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.setColour (creamText.withAlpha (0.75f));
    g.drawText ("preset controls - load an .ns4o file to enable",
                organPanelBounds().withTrimmedTop (organPanelBounds().getHeight() - 18)
                    .withHeight (16).withTrimmedLeft (22).withWidth (440),
                juce::Justification::centredLeft, false);
    g.drawText ("preset controls - load an .ns4y file to enable",
                synthPanelBounds().withTrimmedTop (synthPanelBounds().getHeight() - 18)
                    .withHeight (16).withTrimmedLeft (24).withWidth (440),
                juce::Justification::centredLeft, false);

    // felt strip above the keys
    const auto kb = keyboardBounds();
    g.setColour (juce::Colour (0xff6d0a18));
    g.fillRect (kb.getX(), kb.getY() - 6, kb.getWidth(), 6);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawHorizontalLine (kb.getY() - 1, (float) kb.getX(), (float) kb.getRight());

    // Plugin name at the right, like the hardware model name
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions (27.0f)));
    g.drawText ("nord piano", 1024, 44, 220, 36, juce::Justification::right, false);
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText ("AU PLUGIN EDITION", 1024, 82, 220, 16, juce::Justification::right, false);
    g.drawText ("v0.5", 36, 18, 100, 16, juce::Justification::left, false);
}

void NordPianoAudioProcessorEditor::resized()
{
    const auto kb = keyboardBounds();
    keyboard.setBounds (kb);
    keyboard.setKeyWidth ((float) kb.getWidth() / 52.0f); // 52 white keys on an 88

    const auto top = topPanelBounds();
    loadButton.setBounds (top.getX() + 16, top.getY() + 10, 130, 28);
    fileLabel.setBounds (top.getX() + 156, top.getY() + 10, top.getWidth() - 172, 28);

    const int knobY = top.getY() + 48;
    volumeSlider.setBounds (top.getX() + 16, knobY, 68, 68);
    volumeLabel.setBounds (top.getX() + 16, knobY + 70, 68, 14);
    reverbSlider.setBounds (top.getX() + 126, knobY, 68, 68);
    reverbLabel.setBounds (top.getX() + 126, knobY + 70, 68, 14);
    tuneSlider.setBounds (top.getX() + 236, knobY, 68, 68);
    tuneLabel.setBounds (top.getX() + 236, knobY + 70, 68, 14);
    stretchSlider.setBounds (top.getX() + 346, knobY, 68, 68);
    stretchLabel.setBounds (top.getX() + 346, knobY + 70, 68, 14);

    hzLabel.setBounds (top.getX() + 470, knobY + 2, 130, 20);
    hz440Button.setBounds (top.getX() + 464, knobY + 28, 40, 26);
    hz442Button.setBounds (top.getX() + 508, knobY + 28, 40, 26);
    hz444Button.setBounds (top.getX() + 552, knobY + 28, 40, 26);

    const auto organ = organPanelBounds();
    for (int i = 0; i < 9; ++i)
    {
        const int x = organ.getX() + 22 + i * 48;
        drawbarLabels[i].setBounds (x, organ.getY() + 30, 44, 14);
        drawbarSliders[i].setBounds (x, organ.getY() + 46, 44, 138);
    }
    organModelLabel.setBounds (organ.getX() + 470, organ.getY() + 30, 110, 14);
    organModelBox.setBounds (organ.getX() + 470, organ.getY() + 48, 110, 24);
    percToggle.setBounds (organ.getX() + 470, organ.getY() + 80, 124, 22);
    perc3rdToggle.setBounds (organ.getX() + 470, organ.getY() + 104, 124, 22);
    percFastToggle.setBounds (organ.getX() + 470, organ.getY() + 128, 124, 22);
    percSoftToggle.setBounds (organ.getX() + 470, organ.getY() + 152, 124, 22);
    vibToggle.setBounds (organ.getX() + 470, organ.getY() + 176, 124, 22);

    const auto synth = synthPanelBounds();
    for (int i = 0; i < 6; ++i)
    {
        const int x = synth.getX() + 24 + i * 96;
        synthKnobs[i].setBounds (x, synth.getY() + 46, 64, 64);
        synthKnobLabels[i].setBounds (x, synth.getY() + 112, 64, 14);
    }
    filterTypeLabel.setBounds (synth.getX() + 24, synth.getY() + 140, 150, 14);
    filterTypeBox.setBounds (synth.getX() + 24, synth.getY() + 156, 150, 24);
    synthSourceLabel.setBounds (synth.getX() + 198, synth.getY() + 156, 380, 24);
}
