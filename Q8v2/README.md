# Q8 Strip V2

Channel strip VST3 (JUCE 8): input trim, transformer + discrete preamp (with Unity switch),
harmonics generator, high-pass, 3-band LC EQ derived from the Quad Eight MM-310 BL schematic
(with per-band solo and gain compensation), compressor, line amp, output trim.

## Build
1. Put all files at the top level of your GitHub repo (CMakeLists.txt, the four Plugin*.cpp/.h files, and .github/workflows/main.yml).
2. Actions tab -> run the build -> download the `Q8Strip-VST3` artifact.
3. Copy `Q8 Strip V2.vst3` to `C:\Program Files\Common Files\VST3\`, rescan plugins in FL Studio.

## Notes
- EQ: centre frequencies, tuning capacitors and the A2 stage resistors are read from the MM-310 BL schematic.
  Inductor loss (Q 12/15/15), the preamp, harmonics and compressor are modelled, not read from a schematic.
- Circuit reference: `Q8_circuit_reference.svg`.
