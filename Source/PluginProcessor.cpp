#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "NordFile.h"
#include "Engines.h"

NordPianoAudioProcessor::NordPianoAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "PARAMS", createLayout())
{
    formatManager.registerBasicFormats();

    volumeParam = parameters.getRawParameterValue ("volume");
    reverbParam = parameters.getRawParameterValue ("reverb");
    tuneParam = parameters.getRawParameterValue ("tuneCents");
    stretchParam = parameters.getRawParameterValue ("stretch");

    for (int i = 0; i < 8; ++i)
        synth.addVoice (new juce::SamplerVoice());
    for (int i = 0; i < 12; ++i)
        synth.addVoice (new PianoVoice());
    for (int i = 0; i < 8; ++i)
        synth.addVoice (new OrganVoice());
    for (int i = 0; i < 8; ++i)
        synth.addVoice (new SynthVoice());

    reverbParams.roomSize = 0.6f;
    reverbParams.damping = 0.4f;
    reverbParams.wetLevel = 0.0f;
    reverbParams.dryLevel = 1.0f;
    reverb.setParameters (reverbParams);
}

NordPianoAudioProcessor::~NordPianoAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout NordPianoAudioProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "volume", 1 }, "Volume", 0.0f, 1.0f, 0.8f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "reverb", 1 }, "Reverb", 0.0f, 1.0f, 0.25f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "tuneCents", 1 }, "Tune (cents)", -100.0f, 100.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "stretch", 1 }, "Stretch Tuning", 0.0f, 1.0f, 0.0f));
    return { params.begin(), params.end() };
}

void NordPianoAudioProcessor::prepareToPlay (double sampleRate, int)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    reverb.setSampleRate (sampleRate);
}

void NordPianoAudioProcessor::releaseResources() {}

bool NordPianoAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}

bool NordPianoAudioProcessor::loadSampleFile (const juce::File& file)
{
    const auto ext = file.getFileExtension().toLowerCase();
    organState.active = false;
    synthState.active = false;

    // Nord Piano library: full decode (header, zones, velocity layers, loops)
    if (ext == ".npno")
    {
        auto piano = decodeNpno (file);
        if (! piano.valid)
        {
            loadedFileName = piano.error;
            return false;
        }
        synth.clearSounds();
        int layerCount = 0;
        for (auto& zone : piano.zones)
        {
            layerCount += (int) zone.layers.size();
            synth.addSound (new PianoZoneSound (std::move (zone), &pianoTuning));
        }
        nordFilePending = false;
        loadedFileName = piano.name
            + (piano.variant.isNotEmpty() ? " (" + piano.variant + ")" : juce::String())
            + " - " + juce::String ((int) piano.zones.size()) + " zones, "
            + juce::String (layerCount) + " layers, v" + piano.version;
        return true;
    }

    // Stage 4 presets: organ (.ns4o) and synth (.ns4y) parameter decode
    if (ext == ".ns4o" || ext == ".ns4y")
    {
        juce::MemoryBlock block;
        juce::FileInputStream in (file);
        if (! in.openedOk() || ! in.readIntoMemoryBlock (block))
        {
            loadedFileName = "Could not read the preset file";
            return false;
        }
        const auto* data = (const juce::uint8*) block.getData();
        const size_t size = block.getSize();
        synth.clearSounds();
        organState.active = false;
        synthState.active = false;
        if (ext == ".ns4o")
        {
            auto preset = parseNs4OrganPreset (data, size, file.getFullPathName());
            if (! preset.valid)
            {
                loadedFileName = "Not an .ns4o organ preset";
                return false;
            }
            loadedFileName = preset.name + " - " + preset.summary;
            for (int i = 0; i < 9; ++i)
                organState.drawbars[i] = preset.drawbars[i];
            organState.model = preset.model;
            organState.volume = preset.volume / 127.0f;
            organState.vibChorusOn = preset.vibChorusOn;
            organState.percOn = preset.percOn;
            organState.perc3rd = preset.perc3rd;
            organState.percFast = preset.percFast;
            organState.percSoft = preset.percSoft;
            organState.active = true;
            organState.loadCounter = organState.loadCounter.load() + 1;
            synth.addSound (new OrganSound (&organState));
        }
        else
        {
            auto preset = parseNs4SynthPreset (data, size, file.getFullPathName());
            if (! preset.valid)
            {
                loadedFileName = "Not an .ns4y synth preset";
                return false;
            }
            loadedFileName = preset.name + " - " + preset.summary;
            synthState.analog = preset.analog;
            synthState.analogType = preset.analogType;
            synthState.analogWave = preset.analogWave;
            synthState.filterType = preset.filterType;
            synthState.filterFreq = preset.filterFreq;
            synthState.filterReso = preset.filterReso;
            synthState.ampAttack = preset.ampAttack;
            synthState.ampDecay = preset.ampDecay;
            synthState.ampRelease = preset.ampRelease;
            synthState.volume = preset.volume / 127.0f;
            synthState.active = true;
            synthState.loadCounter = synthState.loadCounter.load() + 1;
            synth.addSound (new SynthSound (&synthState));
        }
        nordFilePending = false;
        return true;
    }

    if (ext == ".nsmp" || ext == ".nsmp3" || ext == ".nsmp4")
    {
        // Sample-library decoding is the next step after .npno.
        nordFilePending = true;
        loadedFileName = file.getFileName() + "  (.nsmp decoder not yet in this build)";
        return false;
    }

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr)
        return false;

    juce::BigInteger allNotes;
    allNotes.setRange (0, 128, true);

    juce::SamplerSound::Ptr sound = new juce::SamplerSound (
        file.getFileNameWithoutExtension(),
        *reader,
        allNotes,
        60,    // root note: middle C
        0.0,   // attack
        0.1,   // release
        30.0); // max sample length (seconds)

    synth.clearSounds();
    synth.addSound (sound);
    nordFilePending = false;
    loadedFileName = file.getFileName();
    return true;
}

void NordPianoAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    pianoTuning.cents = tuneParam != nullptr ? tuneParam->load() : 0.0f;
    pianoTuning.stretch = stretchParam != nullptr ? stretchParam->load() : 0.0f;

    keyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);
    synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());

    const float gain = volumeParam != nullptr ? volumeParam->load() : 0.8f;
    buffer.applyGain (gain);

    const float wet = reverbParam != nullptr ? reverbParam->load() * 0.6f : 0.0f;
    reverbParams.wetLevel = wet;
    reverbParams.dryLevel = 1.0f - wet * 0.5f;
    reverb.setParameters (reverbParams);
    if (buffer.getNumChannels() >= 2)
        reverb.processStereo (buffer.getWritePointer (0), buffer.getWritePointer (1), buffer.getNumSamples());
    else if (buffer.getNumChannels() == 1)
        reverb.processMono (buffer.getWritePointer (0), buffer.getNumSamples());
}

juce::AudioProcessorEditor* NordPianoAudioProcessor::createEditor()
{
    return new NordPianoAudioProcessorEditor (*this);
}

void NordPianoAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = parameters.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void NordPianoAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (parameters.state.getType()))
            parameters.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NordPianoAudioProcessor();
}
