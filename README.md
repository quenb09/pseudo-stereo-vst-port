# Pseudo Stereo (VST3 for FL Studio)

A real VST3 port of Steve Daulton's Audacity Nyquist plug-in **Pseudo-Stereo** (`Pseudostereo.ny`,
included for reference). It takes one channel and makes a stereo image by delaying different
frequency bands by different amounts on the left and right outputs.

Controls (same as the original):

| Control | Range | Default |
|---|---|---|
| Source channel | Left (upper) / Right (lower) | Left |
| Delay factor | 0-100 % | 30 |
| Effect mix | 0-100 % | 80 |

Put it on a stereo mixer insert. It reads the chosen source channel and writes both outputs.

## Build without installing anything (GitHub Actions)

1. Create a new GitHub repository and upload everything in this folder (keep the `.github` folder).
2. Open the **Actions** tab, pick **Build VST3**, and wait for the run to finish (about 10 minutes the first time).
3. Download the **Pseudo-Stereo-VST3-Windows** artifact and unzip it. You get a `Pseudo-Stereo.vst3` folder.

## Build locally (Windows)

Install Visual Studio 2022 (Desktop development with C++) and CMake, then:

    cmake -B build
    cmake --build build --config Release

The plugin lands in `build/PseudoStereo_artefacts/Release/VST3/Pseudo-Stereo.vst3`.

## Install in FL Studio

1. Copy the whole `Pseudo-Stereo.vst3` folder to `C:\Program Files\Common Files\VST3\`.
2. FL Studio: Options > Manage plugins > **Find installed plugins** (use "Verify plugins" if it is not listed).
3. Load it from a mixer insert slot: Pseudo-Stereo appears under Installed > Effects.

## Tests

`tests/compare_with_ny.py` checks the C++ DSP against a literal, line-by-line transcription of
the `.ny` script (separate `lp`/`hp`/`feedback-delay` calls, Nyquist's TONE/ATONE equations):

    g++ -O2 -std=c++17 tests/dsp_test.cpp -o dsp_test
    python3 tests/compare_with_ny.py ./dsp_test

## Notes and licensing

- The effect adds no overall latency: only the 300-600 Hz and 1.2-2.4 kHz bands are delayed, and the dry signal is untouched.
- Below a 4800 Hz sample rate the plug-in passes the source channel to both sides (the original refuses to run there).
- The original is released under the GNU GPL v2 by Steve Daulton (http://easyspacepro.com); this port is a derivative work and carries the same terms.
- Building uses JUCE (AGPLv3 or commercial). That is fine for personal use. If you plan to distribute the plug-in, the GPLv2 terms and JUCE's AGPLv3 terms do not combine cleanly, so sort that out first.
