#include "NordFile.h"

//============================================================================
// Helpers

static juce::uint16 u16le (const juce::uint8* d, size_t o) { return (juce::uint16) (d[o] | (d[o + 1] << 8)); }
static juce::uint32 u32le (const juce::uint8* d, size_t o) { return (juce::uint32) u16le (d, o) | ((juce::uint32) u16le (d, o + 2) << 16); }
static juce::uint16 u16be (const juce::uint8* d, size_t o) { return (juce::uint16) ((d[o] << 8) | d[o + 1]); }
static juce::uint32 u32be (const juce::uint8* d, size_t o) { return ((juce::uint32) u16be (d, o) << 16) | (juce::uint32) u16be (d, o + 2); }
static juce::int16  i16be (const juce::uint8* d, size_t o) { return (juce::int16) u16be (d, o); }

CbinInfo parseCbin (const juce::uint8* data, size_t size)
{
    CbinInfo info;
    if (data == nullptr || size < 0x2C)
        return info;
    if (std::memcmp (data, "CBIN", 4) != 0)
        return info;

    info.valid = true;
    info.formatType = u32le (data, 0x04);
    char tag[5] = {};
    std::memcpy (tag, data + 0x08, 4);
    info.tag = juce::String (tag).trim(); // a 3-letter tag is NUL-padded
    info.bank = u16le (data, 0x0C);
    info.slot = u16le (data, 0x0E);
    info.versionRaw = u32le (data, 0x14);
    info.recordOffset = info.formatType == 1 ? 0x2C : 0x18;
    return info;
}

juce::uint32 readBitsMSB (const juce::uint8* data, size_t size, int begBit, int endBit)
{
    if (data == nullptr || begBit < 0 || endBit < begBit || (size_t) endBit >= size * 8)
        return 0;
    juce::uint32 v = 0;
    for (int absBit = begBit; absBit <= endBit; ++absBit)
    {
        const size_t byteIdx = (size_t) absBit >> 3;
        const int bitInByte = 7 - (absBit & 7); // MSB-first within the byte
        v = (v << 1) | (juce::uint32) ((data[byteIdx] >> bitInByte) & 1);
    }
    return v;
}

//============================================================================
// .npno decoding (port of OpenNord nsp.ts + nsp-audio.ts)

namespace
{
constexpr int DIR_OFFSET = 0x61C;
constexpr int LAYER_TABLE_OFFSET = 0x622;
constexpr int RECORDS_OFFSET = 0x732;
constexpr int RECORD_SIZE = 0x76;
constexpr int BLOCK_OVERLAP = 64;
constexpr int CNSP_V6 = 0x464;
// The recordings sound 4 semitones above their key-map root at 44.1 kHz;
// the instrument subtracts the same 4, so a zone's normal-pitch note is root+4.
constexpr int NSP_ROOT_OFFSET = 4;

const int PREDICTORS[5][4] = {
    { 0, 0, 0, 0 },
    { 1, 0, 0, 0 },
    { 2, -1, 0, 0 },
    { 3, -3, 1, 0 },
    { 4, -6, 4, -1 },
};
const int PREDICTOR_LEN[5] = { 0, 1, 2, 3, 4 };

int findCnsp (const juce::uint8* d, size_t size)
{
    const size_t limit = juce::jmin (size, (size_t) 0x80);
    for (size_t i = 0; i + 4 <= limit; ++i)
        if (d[i] == 'C' && d[i + 1] == 'N' && d[i + 2] == 'S' && d[i + 3] == 'P')
            return (int) i;
    return -1;
}

int framesPerBlock (int blockWords, int width, int channels)
{
    return ((blockWords - 1) * 16) / (width * channels) - BLOCK_OVERLAP;
}

struct RawStroke
{
    int offset = 0;
    int flags = 0;
    int velocityFloor = 0;
    int frames = 0;
    int blocks = 0;
    int history[2][4] = {}; // per channel, newest first
    int loopStart = 0;
    int loopLength = 0;
    double loopDecay = 1.0;
    int zone = 0;
};

juce::String firstAsciiRun (const juce::uint8* d, size_t from, size_t to, size_t size)
{
    juce::String run;
    for (size_t i = from; i < juce::jmin (to, size); ++i)
    {
        const auto c = d[i];
        if (c >= 0x20 && c < 0x7f)
            run += (char) c;
        else
        {
            if (run.length() >= 3)
                return run.trim();
            run.clear();
        }
    }
    return run.length() >= 3 ? run.trim() : juce::String();
}
}

NpnoFile decodeNpno (const juce::File& file)
{
    NpnoFile result;
    juce::MemoryBlock block;
    {
        juce::FileInputStream in (file);
        if (! in.openedOk())
        {
            result.error = "Could not open the file";
            return result;
        }
        in.readIntoMemoryBlock (block);
    }
    const auto* d = (const juce::uint8*) block.getData();
    const size_t size = block.getSize();

    const auto cbin = parseCbin (d, size);
    if (! cbin.valid || cbin.tag != "npno")
    {
        result.error = "Not a Nord Piano (.npno) file - missing CBIN/npno signature";
        return result;
    }

    const int base = findCnsp (d, size);
    if (base < 0)
    {
        result.error = "No CNSP container found";
        return result;
    }

    // Identity
    const int versionRaw = (int) u16le (d, 0x14);
    const int major = versionRaw / 100;
    result.version = juce::String (major) + "." + juce::String (versionRaw - major * 100).paddedLeft ('0', 2);
    const auto full = firstAsciiRun (d, (size_t) base + 0x16, (size_t) base + 0x16 + 0x67, size);
    const int hash = full.indexOfChar ('#');
    result.name = (hash < 0 ? full : full.substring (0, hash)).trim();
    result.variant = hash < 0 ? juce::String() : full.substring (hash + 1).trim();

    // Stroke directory
    if ((size_t) base + RECORDS_OFFSET > size)
    {
        result.error = "File too short for a CNSP directory";
        return result;
    }
    const int dirVersion = u16be (d, (size_t) base + DIR_OFFSET);
    const int channels = u16be (d, (size_t) base + DIR_OFFSET + 2);
    const int count = u16be (d, (size_t) base + DIR_OFFSET + 4);
    if ((channels != 1 && channels != 2) || count == 0
        || (size_t) base + RECORDS_OFFSET + (size_t) count * RECORD_SIZE > size)
    {
        result.error = "Unreadable CNSP directory";
        return result;
    }
    result.channels = channels;
    const int floorScale = dirVersion < CNSP_V6 ? 5 : 1;

    // Layers-per-root table
    int layerTable[128] = {};
    for (int n = 0; n < 128; ++n)
        layerTable[n] = u16be (d, (size_t) base + LAYER_TABLE_OFFSET + (size_t) n * 2);

    std::vector<RawStroke> raw;
    raw.reserve ((size_t) count);
    for (int i = 0; i < count; ++i)
    {
        const size_t at = (size_t) base + RECORDS_OFFSET + (size_t) i * RECORD_SIZE;
        RawStroke s;
        s.offset = base + (int) u32be (d, at);
        s.flags = d[at + 0x04] & 3;
        s.velocityFloor = d[at + 0x05] * floorScale;
        s.frames = (int) u32be (d, at + 0x06);
        s.blocks = u16be (d, at + 0x0A);
        for (int c = 0; c < channels; ++c)
        {
            const size_t h = at + (c == 0 ? 0x0C : 0x14);
            int oldestFirst[4];
            for (int k = 0; k < 4; ++k)
                oldestFirst[k] = i16be (d, h + (size_t) k * 2);
            for (int k = 0; k < 4; ++k)
                s.history[c][k] = oldestFirst[3 - k]; // newest first
        }
        s.loopStart = (int) u32be (d, at + 0x1C);
        s.loopLength = (int) u32be (d, at + 0x20);
        s.loopDecay = s.loopLength ? (double) u32be (d, at + 0x2E) / 8388608.0 : 1.0; // Q23
        raw.push_back (s);
    }

    // Zone of each stroke: layer table first, attenuation-floor fallback
    {
        std::vector<int> counts;
        for (int n = 0; n < 128; ++n)
            if (layerTable[n] > 0)
                counts.push_back (layerTable[n]);
        int sum = 0;
        for (int c : counts) sum += c;
        if (sum == count)
        {
            int idx = 0;
            for (size_t z = 0; z < counts.size(); ++z)
                for (int k = 0; k < counts[z]; ++k)
                    raw[(size_t) idx++].zone = (int) z;
        }
        else
        {
            int zone = -1;
            for (auto& s : raw)
            {
                if ((s.velocityFloor == 0 && s.flags != 2) || zone < 0)
                    ++zone;
                s.zone = zone;
            }
        }
    }

    // Zone roots in order (notes that hold strokes)
    std::vector<int> zoneRoots;
    for (int n = 0; n < 128; ++n)
        if (layerTable[n] > 0)
            zoneRoots.push_back (n);

    // Key map: per-note root byte at CNSP+0x8C
    struct Span { int root, low, high; };
    std::vector<Span> spans;
    {
        std::map<int, std::pair<int, int>> byRoot;
        const size_t at = (size_t) base + 0x8C;
        if (at + 128 <= size)
        {
            for (int note = 0; note < 128; ++note)
            {
                const int r = d[at + (size_t) note];
                if (r == 0 || r == 0xFF || r > 127)
                    continue;
                auto it = byRoot.find (r);
                if (it == byRoot.end())
                    byRoot[r] = { note, note };
                else
                {
                    it->second.first = juce::jmin (it->second.first, note);
                    it->second.second = juce::jmax (it->second.second, note);
                }
            }
        }
        for (auto& kv : byRoot)
            spans.push_back ({ kv.first, kv.second.first, kv.second.second });
        std::sort (spans.begin(), spans.end(), [] (const Span& a, const Span& b) { return a.low < b.low; });
    }

    // Block size from neighbouring strokes
    int blockWords = 511 * channels;
    if (raw.size() > 1 && raw[0].blocks > 0)
    {
        const int bw = (raw[1].offset - raw[0].offset) / raw[0].blocks / 2;
        if (bw >= 2)
            blockWords = bw;
    }

    // Decode strokes: normal layers and key-release strokes (the type-1
    // alternative list is still skipped). Each stroke is peak-normalised on
    // its own - a zone's layers peak alike at the same attenuation, so the
    // touch law in the voice sets the level (OpenNord §2.2).
    struct Decoded { const RawStroke* stroke; std::vector<std::vector<juce::int32>> pcm; int frames = 0; double peak = 0.0; };
    std::vector<Decoded> decoded;

    for (const auto& s : raw)
    {
        if (s.flags != 0 && s.flags != 2)
            continue;
        if (s.offset + s.blocks * blockWords * 2 > (int) size)
            continue;
        if (s.frames > s.blocks * framesPerBlock (blockWords, 1, channels))
            continue;

        Decoded dec;
        dec.stroke = &s;
        dec.pcm.assign ((size_t) channels, std::vector<juce::int32> ((size_t) s.frames, 0));
        int hist[2][4];
        std::memcpy (hist, s.history, sizeof (hist));

        int t = 0;
        bool ok = true;
        for (int b = 0; b < s.blocks && ok; ++b)
        {
            const size_t at = (size_t) s.offset + (size_t) b * (size_t) blockWords * 2;
            const int header = u16be (d, at);
            const int width = header & 0x1F;
            const int order = (header >> 5) & 7;
            if (width == 0 || order >= 5)
            {
                ok = false;
                break;
            }
            const int own = framesPerBlock (blockWords, width, channels);
            const int half = 1 << (width - 1);
            const int fullRange = 1 << width;
            int bit = 0; // bit position in the residual area (words 1..)
            for (int n = 0; n < own; ++n)
            {
                if (t >= s.frames)
                {
                    ok = false;
                    break;
                }
                for (int c = 0; c < channels; ++c)
                {
                    int v = 0;
                    for (int got = 0; got < width;)
                    {
                        const size_t wordAt = at + 2 + (size_t) (bit >> 4) * 2;
                        if (wordAt + 2 > size)
                        {
                            ok = false;
                            break;
                        }
                        const int word = u16be (d, wordAt);
                        const int offs = bit & 15;
                        const int take = juce::jmin (16 - offs, width - got);
                        v += ((word >> offs) & ((1 << take) - 1)) << got;
                        got += take;
                        bit += take;
                    }
                    if (! ok)
                        break;
                    const int r = v >= half ? v - fullRange : v;
                    int x = r;
                    for (int k = 0; k < PREDICTOR_LEN[order]; ++k)
                        x += PREDICTORS[order][k] * hist[c][k];
                    // shift history: newest first, keep 4
                    hist[c][3] = hist[c][2];
                    hist[c][2] = hist[c][1];
                    hist[c][1] = hist[c][0];
                    hist[c][0] = x;
                    dec.pcm[(size_t) c][(size_t) t] = x;
                    const double a = std::abs ((double) x);
                    if (a > dec.peak)
                        dec.peak = a;
                }
                ++t;
            }
        }
        dec.frames = t;
        if (ok && t > 0)
            decoded.push_back (std::move (dec));
    }

    if (decoded.empty())
    {
        result.error = "No strokes could be decoded";
        return result;
    }

    // Assemble zones
    int numZones = 0;
    for (const auto& s : raw)
        numZones = juce::jmax (numZones, s.zone + 1);

    for (int z = 0; z < numZones; ++z)
    {
        NpnoZone zone;
        zone.rootNote = z < (int) zoneRoots.size() ? zoneRoots[(size_t) z] : 60;
        zone.lowNote = zone.rootNote;
        zone.highNote = zone.rootNote;
        for (const auto& sp : spans)
            if (sp.root == zone.rootNote)
            {
                zone.lowNote = sp.low;
                zone.highNote = sp.high;
            }

        // Normal layers and release strokes of this zone, each loudest first.
        std::vector<const Decoded*> layers, releases;
        for (const auto& dec : decoded)
        {
            if (dec.stroke->zone != z || dec.peak <= 0.0)
                continue;
            (dec.stroke->flags == 2 ? releases : layers).push_back (&dec);
        }
        auto byFloor = [] (const Decoded* a, const Decoded* b) { return a->stroke->velocityFloor < b->stroke->velocityFloor; };
        std::sort (layers.begin(), layers.end(), byFloor);
        std::sort (releases.begin(), releases.end(), byFloor);

        auto buildLayer = [&] (const Decoded* dec) {
            NpnoLayer layer;
            layer.velocityFloor = dec->stroke->velocityFloor;
            const float gain = (float) (1.0 / dec->peak); // per-stroke peak normalisation
            layer.audio.setSize (channels, dec->frames);
            for (int c = 0; c < channels; ++c)
            {
                auto* w = layer.audio.getWritePointer (c);
                for (int i = 0; i < dec->frames; ++i)
                    w[i] = (float) dec->pcm[(size_t) c][(size_t) i] * gain;
            }
            if (dec->stroke->loopLength > 0)
            {
                layer.loopStart = dec->stroke->loopStart;
                layer.loopLength = dec->stroke->loopLength;
                layer.loopDecay = dec->stroke->loopDecay;
            }
            return layer;
        };
        for (const auto* dec : layers)
            zone.layers.push_back (buildLayer (dec));
        for (const auto* dec : releases)
            zone.releaseLayers.push_back (buildLayer (dec));
        if (! zone.layers.empty())
            result.zones.push_back (std::move (zone));
    }

    result.valid = ! result.zones.empty();
    if (! result.valid)
        result.error = "Zones could not be assembled";
    return result;
}

//============================================================================
// Stage 4 presets. Bit positions below are the program (ns4p) layer-A
// locations from ns4decode's bitmap (MIT); a preset relocates its engine
// block to byte 49, so subtract the engine shift (organ 45, synth 327 bytes).

static juce::String nameFromFilename (const juce::String& filename)
{
    auto name = juce::File (filename).getFileNameWithoutExtension();
    return name.isEmpty() ? juce::String ("Unnamed") : name;
}

Ns4OrganPreset parseNs4OrganPreset (const juce::uint8* data, size_t size, const juce::String& filename)
{
    Ns4OrganPreset p;
    const auto cbin = parseCbin (data, size);
    if (! cbin.valid || cbin.tag != "ns4o")
        return p;
    constexpr int shift = 45 * 8;
    auto read = [&] (int beg, int end) { return (int) readBitsMSB (data, size, beg - shift, end - shift); };

    p.name = nameFromFilename (filename);
    p.model = read (905, 907);
    p.volume = read (758, 764);
    for (int i = 0; i < 9; ++i)
    {
        const int beg = 928 + i * 19;
        p.drawbars[i] = read (beg, beg + 3);
    }
    p.vibChorusOn = read (1120, 1120) != 0;
    p.percOn = read (1121, 1121) != 0;
    p.perc3rd = read (1122, 1122) != 0;
    p.percFast = read (1123, 1123) != 0;
    p.percSoft = read (1124, 1124) != 0;

    static const char* models[] = { "B3", "VOX", "FARF", "PIPE1", "PIPE2", "B3 BASS" };
    juce::String bars;
    for (int i = 0; i < 9; ++i)
        bars += juce::String (p.drawbars[i]);
    p.summary = juce::String (p.model >= 0 && p.model < 6 ? models[p.model] : "Organ")
              + " / " + bars
              + (p.percOn ? (p.perc3rd ? " / Perc 3rd" : " / Perc 2nd") : juce::String())
              + (p.percOn && p.percFast ? " Fast" : juce::String())
              + (p.vibChorusOn ? " / Vib-Chorus ON" : juce::String());
    p.valid = true;
    return p;
}

Ns4SynthPreset parseNs4SynthPreset (const juce::uint8* data, size_t size, const juce::String& filename)
{
    Ns4SynthPreset p;
    const auto cbin = parseCbin (data, size);
    if (! cbin.valid || cbin.tag != "ns4y")
        return p;
    constexpr int shift = 327 * 8;
    auto read = [&] (int beg, int end) { return (int) readBitsMSB (data, size, beg - shift, end - shift); };

    p.name = nameFromFilename (filename);
    p.volume = read (3018, 3024);
    p.analog = read (3256, 3256) != 0;
    p.sampleId = (int) readBitsMSB (data, size, 3274 - shift, 3305 - shift);
    p.analogType = read (4482, 4483);
    p.analogWave = read (4491, 4496);
    p.oscCtrl = read (4497, 4503);
    p.filterType = read (4690, 4692);
    p.filterFreq = read (4693, 4699);
    p.filterReso = read (4724, 4730);
    p.ampAttack = read (4667, 4673);
    p.ampDecay = read (4674, 4680);
    p.ampRelease = read (4681, 4687);

    static const char* filters[] = { "LP12", "LP24", "LPM", "LPHP", "BP", "HP" };
    static const char* oscTypes[] = { "ANALOG", "FM-H", "FM-I", "WAVE" };
    p.summary = (p.analog
                    ? juce::String (p.analogType >= 0 && p.analogType < 4 ? oscTypes[p.analogType] : "Analog")
                          + " wave " + juce::String (p.analogWave)
                    : "Sample-based (needs its .nsmp file)")
              + " / " + juce::String (p.filterType >= 0 && p.filterType < 6 ? filters[p.filterType] : "LP")
              + " freq " + juce::String (p.filterFreq)
              + " reso " + juce::String (p.filterReso);
    p.valid = true;
    return p;
}
