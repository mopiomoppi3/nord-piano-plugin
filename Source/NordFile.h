#pragma once

#include <JuceHeader.h>

// Nord file reading, ported from the open-source OpenNord project
// (AGPL-3.0, https://github.com/simonflore/opennord) and, for the Stage 4
// parameter bit map, ns4decode by Randy (MIT). See NOTICE.md.
//
// Layers implemented here:
//  - CBIN container envelope (every Nord file)
//  - .npno piano: CNSP header, key map, stroke directory, LPC block codec
//  - .ns4o organ preset / .ns4y synth preset: engine parameter block decode
//    (the preset body is the Stage 4 program's layer-A block relocated to
//    byte 49, so program bit offsets are shifted by a fixed per-engine amount)

struct CbinInfo
{
    bool valid = false;
    juce::String tag;          // 4-char file type, e.g. "npno", "ns4o"
    juce::uint32 formatType = 0; // 0 = legacy header, 1 = NSM-era header
    int recordOffset = 0;      // 0x2C (NSM) or 0x18 (legacy)
    juce::uint16 bank = 0;
    juce::uint16 slot = 0;
    juce::uint32 versionRaw = 0; // version x100 as stored at 0x14
};

CbinInfo parseCbin (const juce::uint8* data, size_t size);

// MSB-first bit field read, absolute bit positions from the file start
// (ns4decode convention). Returns 0 when the range is outside the data.
juce::uint32 readBitsMSB (const juce::uint8* data, size_t size, int begBit, int endBit);

//========================================================================
// .npno (Nord Piano library)

struct NpnoLayer
{
    juce::AudioBuffer<float> audio; // decoded PCM, normalised per stroke to peak 1.0
    int velocityFloor = 0;          // attenuation floor, 0.2 dB steps (v5: 1 dB x5)
    int loopStart = -1;             // frames, -1 = no loop
    int loopLength = 0;
    double loopDecay = 1.0;         // per-envelope-tick factor inside the loop
};

struct NpnoZone
{
    int rootNote = 60;
    int lowNote = 0;
    int highNote = 127;
    std::vector<NpnoLayer> layers;        // normal strokes, loudest first
    std::vector<NpnoLayer> releaseLayers; // key-release strokes (flags 2), loudest first
};

struct NpnoFile
{
    bool valid = false;
    juce::String name;
    juce::String variant;
    juce::String version;           // e.g. "6.10"
    int channels = 0;
    std::vector<NpnoZone> zones;
    juce::String error;
};

// Reads the whole file into memory and decodes every stroke.
// Large (L/XL) libraries take real time and RAM - start with S/M sizes.
NpnoFile decodeNpno (const juce::File& file);

//========================================================================
// .ns4o (Stage 4 organ preset)

struct Ns4OrganPreset
{
    bool valid = false;
    juce::String name;              // presets store no name; taken from the filename
    int model = 0;                  // 0 B3, 1 VOX, 2 FARF, 3 PIPE1, 4 PIPE2, 5 B3 BASS
    int drawbars[9] = {};           // 0..8 each
    int volume = 0;                 // raw 0..127
    bool vibChorusOn = false;
    bool percOn = false;
    bool perc3rd = false;           // false = 2nd harmonic
    bool percFast = false;
    bool percSoft = false;
    juce::String summary;
};

Ns4OrganPreset parseNs4OrganPreset (const juce::uint8* data, size_t size, const juce::String& filename);

//========================================================================
// .ns4y (Stage 4 synth preset)

struct Ns4SynthPreset
{
    bool valid = false;
    juce::String name;              // from the filename
    bool analog = false;            // false = sample-based source
    int sampleId = 0;               // sample reference (needs the .nsmp file itself to sound)
    int analogType = 0;             // 0 ANALOG, 1 FM-H, 2 FM-I, 3 WAVE
    int analogWave = 0;             // raw wave/partial selector 0..63
    int oscCtrl = 0;                // raw 0..127
    int filterType = 0;             // 0 LP12, 1 LP24, 2 LPM, 3 LPHP, 4 BP, 5 HP
    int filterFreq = 0;             // raw 0..127
    int filterReso = 0;             // raw 0..127
    int ampAttack = 0, ampDecay = 0, ampRelease = 0; // raw 0..127
    int volume = 0;                 // raw 0..127
    juce::String summary;
};

Ns4SynthPreset parseNs4SynthPreset (const juce::uint8* data, size_t size, const juce::String& filename);
