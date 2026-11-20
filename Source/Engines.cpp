#include "Engines.h"

//========================================================================
// Piano (.npno)

int PianoZoneSound::pickLayer (int velocity127) const
{
    if (zone.layers.empty())
        return -1;
    // Velocity -> attenuation in 0.2 dB steps (OpenNord touchAttenuation):
    // a -40 dB law, clamped to the 0..200 range the records use.
    const double v = juce::jlimit (1, 127, velocity127);
    const double att = juce::jlimit (0.0, 200.0, -200.0 * std::log10 (v / 127.0));
    int pick = 0;
    for (size_t i = 1; i < zone.layers.size(); ++i)
    {
        const int floorI = zone.layers[i].velocityFloor;
        const int floorPrev = zone.layers[i - 1].velocityFloor;
        const int switchPoint = floorI - (floorI - floorPrev) / 2;
        if ((double) switchPoint <= att)
            pick = (int) i;
    }
    return pick;
}

void PianoVoice::startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound* s, int)
{
    sound = dynamic_cast<PianoZoneSound*> (s);
    if (sound == nullptr || sound->zone.layers.empty())
        return;
    const int vel127 = juce::jlimit (1, 127, (int) std::round (velocity * 127.0f));
    const int idx = sound->pickLayer (vel127);
    if (idx < 0)
        return;
    layer = &sound->zone.layers[(size_t) idx];

    // Tuning: master cents plus stretch (Railsback-style: the farther from
    // middle C, the more the octaves widen - up to +-30 cents at full stretch).
    double tuneCents = 0.0;
    if (sound->tuning != nullptr)
    {
        tuneCents = sound->tuning->cents.load();
        const double stretchAmt = sound->tuning->stretch.load();
        const double d = (midiNoteNumber - 60) / 60.0;
        tuneCents += stretchAmt * 30.0 * d * std::abs (d);
    }
    pitchRatio = std::pow (2.0, (midiNoteNumber - sound->normalPitchNote() + tuneCents / 100.0) / 12.0);
    position = 0.0;
    level = 0.25f + 0.75f * velocity;
    loopGain = 1.0f;
    tail = 1.0f;
    releasing = false;
    playing = true;
}

void PianoVoice::stopNote (float, bool allowTailOff)
{
    if (! playing)
        return;
    if (allowTailOff)
        releasing = true;
    else
    {
        playing = false;
        clearCurrentNote();
    }
}

void PianoVoice::renderNextBlock (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    if (! playing || layer == nullptr)
        return;

    const auto& audio = layer->audio;
    const int frames = audio.getNumSamples();
    const int chans = audio.getNumChannels();
    if (frames == 0)
    {
        playing = false;
        clearCurrentNote();
        return;
    }

    const double sampleRate = getSampleRate();
    const bool hasLoop = layer->loopLength > 0 && layer->loopStart >= 0
                         && layer->loopStart + layer->loopLength <= frames;
    const int loopEnd = hasLoop ? layer->loopStart + layer->loopLength : 0;
    // loopDecay applies 22050 times a second on the instrument.
    const double perSampleDecay = hasLoop ? std::pow (juce::jlimit (0.0, 1.0, layer->loopDecay), 22050.0 / sampleRate) : 1.0;
    const float tailStep = (float) (1.0 / (0.25 * sampleRate)); // 0.25 s release

    for (int i = 0; i < numSamples; ++i)
    {
        if (hasLoop)
        {
            if (position >= loopEnd)
                position = layer->loopStart + std::fmod (position - layer->loopStart, (double) layer->loopLength);
            if (position >= layer->loopStart)
                loopGain *= (float) perSampleDecay;
        }
        else if (position >= frames - 1)
        {
            playing = false;
            clearCurrentNote();
            return;
        }

        const int idx = (int) position;
        const float frac = (float) (position - idx);
        const int idx2 = juce::jmin (idx + 1, frames - 1);

        for (int outCh = 0; outCh < buffer.getNumChannels(); ++outCh)
        {
            const int srcCh = juce::jmin (outCh, chans - 1);
            const float a = audio.getSample (srcCh, idx);
            const float b = audio.getSample (srcCh, idx2);
            const float sample = (a + (b - a) * frac) * level * loopGain * tail;
            buffer.addSample (outCh, startSample + i, sample);
        }

        position += pitchRatio;

        if (releasing)
        {
            tail -= tailStep;
            if (tail <= 0.0f)
            {
                playing = false;
                clearCurrentNote();
                return;
            }
        }
    }
}

//========================================================================
// Organ (.ns4o) - additive drawbar engine

namespace
{
// Hammond footage multipliers relative to the 8' fundamental.
const double FOOTAGE[9] = { 0.5, 1.5, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 8.0 };
// Drawbar value 0..8 -> gain (classic Hammond dB steps).
float drawbarGain (int v)
{
    static const float db[9] = { -60.0f, -25.0f, -20.0f, -15.0f, -12.0f, -9.0f, -6.0f, -3.0f, 0.0f };
    if (v <= 0)
        return 0.0f;
    return std::pow (10.0f, db[juce::jlimit (0, 8, v)] / 20.0f);
}
}

void OrganVoice::startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound* s, int)
{
    sound = dynamic_cast<OrganSound*> (s);
    if (sound == nullptr || sound->state == nullptr)
        return;
    baseFreq = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
    for (double& p : phases)
        p = 0.0;
    level = velocity * (0.35f + 0.65f * sound->state->volume.load());
    env = 0.0f;
    releasing = false;
    percPhase = 0.0;
    percEnv = sound->state->percOn.load() ? 1.0f : 0.0f;
    vibTime = 0.0;
}

void OrganVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
        releasing = true;
    else
    {
        env = 0.0f;
        clearCurrentNote();
    }
}

void OrganVoice::renderNextBlock (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    if (sound == nullptr || sound->state == nullptr)
        return;
    const auto* state = sound->state;
    const double sampleRate = getSampleRate();
    const float attackStep = (float) (1.0 / (0.005 * sampleRate));
    const float releaseStep = (float) (1.0 / (0.03 * sampleRate));
    // Percussion decay: fast ~0.25 s, slow ~1 s, exponential to near zero.
    const bool percFast = state->percFast.load();
    const bool perc3rd = state->perc3rd.load();
    const bool percSoft = state->percSoft.load();
    const float percDecay = (float) std::exp (-1.0 / ((percFast ? 0.08 : 0.35) * sampleRate));
    const double percFreq = baseFreq * (perc3rd ? 3.0 : 2.0);
    const float percLevel = percSoft ? 0.35f : 0.6f;

    float gains[9];
    float total = 0.0f;
    for (int i = 0; i < 9; ++i)
    {
        gains[i] = drawbarGain (state->drawbars[i].load());
        total += gains[i];
    }
    const float norm = total > 0.0f ? 0.7f / total : 0.0f;

    const bool vibOn = state->vibChorusOn.load();

    for (int i = 0; i < numSamples; ++i)
    {
        if (releasing)
        {
            env -= releaseStep;
            if (env <= 0.0f)
            {
                env = 0.0f;
                clearCurrentNote();
                return;
            }
        }
        else if (env < 1.0f)
        {
            env = juce::jmin (1.0f, env + attackStep);
        }

        // Vib/Chorus switch: vibrato approximation (6.8 Hz, +-0.4% pitch).
        // The scanner chorus / rotary speaker themselves are not modelled yet.
        double vibMult = 1.0;
        if (vibOn)
        {
            vibMult = 1.0 + 0.004 * std::sin (juce::MathConstants<double>::twoPi * 6.8 * vibTime);
            vibTime += 1.0 / sampleRate;
        }

        float sample = 0.0f;
        for (int h = 0; h < 9; ++h)
        {
            if (gains[h] <= 0.0f)
                continue;
            phases[h] += juce::MathConstants<double>::twoPi * baseFreq * FOOTAGE[h] * vibMult / sampleRate;
            if (phases[h] >= juce::MathConstants<double>::twoPi)
                phases[h] -= juce::MathConstants<double>::twoPi;
            sample += gains[h] * (float) std::sin (phases[h]);
        }
        sample *= norm;

        if (percEnv > 0.001f)
        {
            percPhase += juce::MathConstants<double>::twoPi * percFreq / sampleRate;
            if (percPhase >= juce::MathConstants<double>::twoPi)
                percPhase -= juce::MathConstants<double>::twoPi;
            sample += percLevel * percEnv * (float) std::sin (percPhase);
            percEnv *= percDecay;
        }

        sample *= level * env;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.addSample (ch, startSample + i, sample);
    }
}

//========================================================================
// Synth (.ns4y) - analog oscillator + filter + amp envelope (approximation)

namespace
{
double rawToSeconds (int raw, double maxSeconds)
{
    const double n = juce::jlimit (0, 127, raw) / 127.0;
    return 0.003 + n * n * maxSeconds;
}
float rawToFreqHz (int raw)
{
    const double n = juce::jlimit (0, 127, raw) / 127.0;
    return (float) (40.0 * std::pow (18000.0 / 40.0, n));
}
}

float SynthVoice::waveSample (double phase) const
{
    // v0.2 approximation of the analog wave selector (exact table not ported):
    // 0-15 saw, 16-31 square, 32-47 triangle, 48-63 sine. FM/WAVE types fall
    // back to saw until their engines are ported.
    if (sound == nullptr || sound->state == nullptr)
        return 0.0f;
    const auto* state = sound->state;
    if (state->analogType.load() != 0)
        return (float) (2.0 * (phase - std::floor (phase + 0.5))); // saw-ish
    const int w = state->analogWave.load();
    if (w < 16)
        return (float) (2.0 * (phase - std::floor (phase + 0.5)));
    if (w < 32)
        return phase - std::floor (phase) < 0.5 ? 0.8f : -0.8f;
    if (w < 48)
    {
        const double f = phase - std::floor (phase);
        return (float) (f < 0.5 ? -1.0 + 4.0 * f : 3.0 - 4.0 * f);
    }
    return (float) std::sin (juce::MathConstants<double>::twoPi * phase);
}

void SynthVoice::startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound* s, int)
{
    sound = dynamic_cast<SynthSound*> (s);
    if (sound == nullptr || sound->state == nullptr)
        return;
    const auto* state = sound->state;
    phase = 0.0;
    phaseInc = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber) / getSampleRate();
    level = velocity * (0.3f + 0.7f * state->volume.load());
    envStage = 0;
    envValue = 0.0f;

    juce::IIRCoefficients coeff;
    const float freq = rawToFreqHz (state->filterFreq.load());
    const float q = 0.7f + (juce::jlimit (0, 127, state->filterReso.load()) / 127.0f) * 8.0f;
    switch (state->filterType.load())
    {
        case 4:  coeff = juce::IIRCoefficients::makeBandPass (getSampleRate(), freq, q); break;
        case 5:  coeff = juce::IIRCoefficients::makeHighPass (getSampleRate(), freq, q); break;
        default: coeff = juce::IIRCoefficients::makeLowPass (getSampleRate(), freq, q); break;
    }
    filterL.setCoefficients (coeff);
    filterR.setCoefficients (coeff);
    filterL.reset();
    filterR.reset();
}

void SynthVoice::stopNote (float, bool allowTailOff)
{
    if (sound == nullptr)
        return;
    if (allowTailOff)
        envStage = 3;
    else
    {
        envStage = 4;
        envValue = 0.0f;
        clearCurrentNote();
    }
}

void SynthVoice::renderNextBlock (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    if (sound == nullptr || sound->state == nullptr || envStage == 4)
        return;
    const auto* state = sound->state;
    const double sampleRate = getSampleRate();
    const float attackStep = (float) (1.0 / (rawToSeconds (state->ampAttack.load(), 4.0) * sampleRate));
    const float decayStep = (float) (1.0 / (rawToSeconds (state->ampDecay.load(), 4.0) * sampleRate));
    const float releaseStep = (float) (1.0 / (rawToSeconds (state->ampRelease.load(), 6.0) * sampleRate));
    const float sustain = 0.6f; // sustain level is not in the decoded subset yet

    for (int i = 0; i < numSamples; ++i)
    {
        switch (envStage)
        {
            case 0:
                envValue += attackStep;
                if (envValue >= 1.0f) { envValue = 1.0f; envStage = 1; }
                break;
            case 1:
                envValue -= decayStep * (1.0f - sustain);
                if (envValue <= sustain) { envValue = sustain; envStage = 2; }
                break;
            case 2:
                break;
            case 3:
                envValue -= releaseStep;
                if (envValue <= 0.0f)
                {
                    envValue = 0.0f;
                    envStage = 4;
                    clearCurrentNote();
                    return;
                }
                break;
            default:
                return;
        }

        float sample = waveSample (phase) * envValue * level * 0.6f;
        phase += phaseInc;
        if (phase >= 1.0)
            phase -= 1.0;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto& filter = ch == 0 ? filterL : filterR;
            buffer.addSample (ch, startSample + i, filter.processSingleSampleRaw (sample));
        }
    }
}
