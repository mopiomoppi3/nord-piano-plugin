# Third-party notices

This project's Nord file decoding is a C++ port of open-source work:

- **OpenNord** by Simon Flore and contributors — CBIN container layout,
  the `.npno` (CNSP) header / stroke directory / piano block codec, and the
  Stage 4 preset relocation scheme (a preset body is the program's layer-A
  engine block shifted to byte 49).
  https://github.com/simonflore/opennord — **AGPL-3.0**.
  Because of this, the Nord decoding parts of this project (Source/NordFile.*)
  are likewise distributed under AGPL-3.0. Full text:
  https://www.gnu.org/licenses/agpl-3.0.txt

- **ns4decode** by Randy — the Stage 4 parameter bit map (bit locations of
  organ drawbars, synth oscillator/filter/envelope parameters, and their
  value tables) used for `.ns4o` / `.ns4y` decoding.
  https://ns4decode.netlify.app/ — **MIT**.

- **JUCE** 8.0.4 — GPLv3 / commercial dual license.
  https://github.com/juce-framework/JUCE

No Nord / Clavia sample content is included in this project. The plugin only
reads files the user owns and supplies themselves. Not affiliated with
Clavia DMI AB / Nord Keyboards.
