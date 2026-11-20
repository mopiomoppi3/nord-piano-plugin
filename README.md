# Nord Piano (AU plugin) - v0.4

Nord Piano 5 style AU plugin for Logic Pro, built with JUCE.
Personal project. Not affiliated with Clavia / Nord. No Nord samples are included;
the plugin reads files you own and supply yourself.

## What is real in v0.4

- AU (.component), VST3 and Standalone builds from the same source
- MIDI input and an on-screen 88-key keyboard (A0-C8)
- **`.npno` Nord Piano libraries - fully decoded** (ported from OpenNord):
  name/variant/version, key-map zones, velocity layers chosen by the
  instrument's own attenuation-floor rule, sustain loops with loop decay,
  stereo/mono strokes at 44.1 kHz. Start with S/M sizes: an XL library is
  decoded fully into RAM and takes real time and memory.
- **Full live panels for organ and synth**: loading a preset lights up its
  section and every control is wired to the sound -
  ORGAN: 9 drawbar faders (with footage labels and value readouts), model
  selector (B3/VOX/FARF/PIPE1/PIPE2/B3 BASS), Percussion, 3rd harmonic,
  Fast decay, Soft volume and Vib/Chorus switches.
  SYNTH: WAVE / FREQ / RESO / ATTACK / DECAY / RELEASE knobs and a filter
  type selector (LP12/LP24/LPM/LPHP/BP/HP), with the oscillator source
  shown. Sections stay greyed out until their preset kind is loaded.
- **`.ns4o` Stage 4 organ presets - decoded and playable**: organ model,
  all 9 drawbars, volume, percussion (2nd/3rd, fast/slow, soft/normal) and
  the vibrato/chorus flag are read from the file and drive an additive
  drawbar organ engine with percussion.
- **`.ns4y` Stage 4 synth presets - decoded and playable** for analog-type
  presets: oscillator type/wave, filter type/frequency/resonance and the
  amp envelope drive an analog-style synth voice.
- **Piano tuning**: TUNE knob in cents (-100..+100) with **440 / 442 / 444**
  preset buttons and a live "A = xxx.x Hz" readout (442 Hz is only +7.9
  cents), plus a STRETCH knob for stretch tuning (octaves widen away from
  middle C, up to +-30 cents at full stretch, Railsback-style). Tuning
  applies to the piano engine; organ and synth stay at A440 for now.
- WAV / AIFF single-sample loading (root = middle C) for quick tests
- VOLUME and REVERB knobs (real parameters, automatable in Logic)
- Red Piano 5 style panel, "nord piano" name at the right

## Honest approximations in v0.2

- Piano key-release strokes and the alternative stroke list are not
  played yet (normal velocity layers only).
- Organ model differences (B3 vs VOX vs FARF vs pipe) are parsed and shown;
  the tone engine is a sine-drawbar approximation for all of them. The
  Vib/Chorus switch drives a vibrato approximation (6.8 Hz); the scanner
  chorus and rotary speaker are not modelled yet.
- Synth wave selector uses an approximate table (saw/square/triangle/sine
  ranges); FM and wavetable oscillator types fall back to saw for now.
  Sample-based synth presets need their `.nsmp` file, which is not decoded
  yet - they fall back to the synth voice so the preset still sounds.
- Synth sustain level is fixed (not in the decoded parameter subset yet).
- Per-note individual tuning (a tuner-style note-by-note table) is not in
  yet; TUNE + STRETCH cover the global tuning.

## What is NOT in v0.2 yet

- `.nsmp` / `.nsmp3` / `.nsmp4` sample-library decoding (the codec is the
  next port; the loader says so instead of failing silently).
- `.ns4n` piano presets and `.ns4p` programs (they reference piano/sample
  content; preset decode for them is a later step).
- String resonance / pedal noise modelling.

## How to build (GitHub)

1. Create a new GitHub repository (Public is recommended, macOS minutes are
   wider on public repos).
2. Upload ALL contents of this folder, including the hidden `.github` folder.
   On a Mac Finder, press Cmd+Shift+. to show hidden files.
   The `.github` folder must end up at the repository root.
3. The "Build macOS plugin" action runs automatically on push
   (or run it manually from the Actions tab).
4. When it finishes, download the `NordPiano-macOS` artifact and unzip it.

## How to install in Logic

1. Quit Logic first.
2. Copy `Nord Piano.component` into:
   `~/Library/Audio/Plug-Ins/Components/`
   (the VST3 goes to `~/Library/Audio/Plug-Ins/VST3/` if you want it,
   the .app is a standalone version you can just run.)
3. If macOS blocks it the first time (unsigned / ad-hoc signed build):
   - Remove quarantine in Terminal:
     `xattr -cr ~/Library/Audio/Plug-Ins/Components/"Nord Piano.component"`
   - Or allow it in System Settings > Privacy & Security.
   This is a first-time-only step.
4. Start Logic. It validates the AU (auval) and "Nord Piano" appears
   as a software instrument.
5. In the plugin window, use "Load File..." and pick a `.npno`, `.ns4o`,
   `.ns4y` or WAV/AIFF file. The status line shows what was decoded
   (piano name + zones/layers, organ registration, synth settings).

## Local build (optional, needs Xcode)

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Layout

- `CMakeLists.txt` - JUCE is fetched automatically (8.0.4)
- `Source/PluginProcessor.*` - plugin core, parameters, file dispatch
- `Source/PluginEditor.*` - Piano 5 style UI
- `Source/NordFile.*` - CBIN + .npno + .ns4o/.ns4y decoding (see NOTICE.md)
- `Source/Engines.*` - piano / organ / synth voices
- `.github/workflows/build-macos.yml` - macOS CI build, ad-hoc sign, ditto zip
