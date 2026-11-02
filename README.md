# LayShit — the delay you lay

A VST3 delay plugin. The GUI is a real-time 3D top-down view into a toilet bowl: turn the knobs and the poop shifts, spins, melts, crystallises, and when Crystals and Shimmer are both up it ascends and god-rays shoot out of the bowl.

## Build

Requires Visual Studio 2022 (C++ workload) and CMake. JUCE 8.0.4 lives in `libs/JUCE` (not committed; on a fresh clone run `git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git libs/JUCE`).

## Releases

Push a tag like `v1.0.1` and GitHub Actions builds on Windows, runs the DSP tests, and attaches `LayShit-v1.0.1-Windows-VST3.zip` (plus a Standalone zip) to a GitHub Release. Running the workflow by hand from the Actions tab gives you the same zips as a workflow artifact.

```
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target LayShit_VST3 LayShit_Standalone
```

Output:
- VST3: `build/LayShit_artefacts/Release/VST3/LayShit.vst3` (copy to `C:\Program Files\Common Files\VST3`)
- Standalone: `build/LayShit_artefacts/Release/Standalone/LayShit.exe`

DSP tests (no plugin needed):
```
cmake -B build-tests -G "Visual Studio 17 2022" -A x64 -DLAYSHIT_BUILD_PLUGIN=OFF
cmake --build build-tests --config Release --target LayShitTests
build-tests/LayShitTests_artefacts/Release/LayShitTests.exe
```

## Signal flow

```
in ─► delay line ─► [CLOG ─ SHIFT ─ CRYSTALS ─ PLUNGE]* ─► Low/High cut ─► soft limit ─┬─► wet (ducked by Courtesy) ─► mix ─► out
          ▲                                                                            │
          └──────────────────────────── feedback (up to 110%) ◄────────────────────────┘
* order set by dragging the pipes
```

| Section | Controls | What it does | What the bowl does |
|---|---|---|---|
| The Drop | Time / Sync, Feedback, Mix, Ping-Pong | core delay, up to 4 s, 110% feedback self-oscillates (limited) | poop drifts and bobs; >95% the bowl overflows onto the floor |
| | Wobble, Wobble Rate | tape-style time modulation | poop jiggles like jelly |
| | Low Cut, High Cut | filters inside the loop | |
| | Courtesy, Release | ducks the repeats while you play | poop sinks while you play, surfaces when you stop |
| Clog | Drive, Crush, Mix | asymmetric saturation + bitcrush/downsample | crusty, darker, rougher poop |
| Shift | Shift (±1000 Hz), Mix | frequency shifter in the loop: barber-pole spirals | poop spins (direction = sign), water swirls |
| Crystals | Crystals, Grain Size, Density, Spray, Pitch, Reverse, Freeze | granular cloud on the loop | facets, iridescence, sparkles, floating motes |
| | Shimmer, Interval | pitch-shifted repeats (+12/+7/+19) | with Crystals: **holy mode** — gold, halo, god-rays |
| Plunge | Plunge, Size | allpass diffusion, repeats smear into a wash | water haze |
| Top bar | Bristol (1–7) | macro: 1 = hard gritty lumps, 7 = liquid smeared and dark | poop turns into nuggets … or melts into a puddle |
| Chain bar | FLUSH button | drains the buffer with a pitch dive | whirlpool, poop spirals down, respawns |
| Top bar | Preset pill | 11 factory presets (also exposed as host programs) | |

Input transients make the water splash, and the ripples echo at every delay tap.

**The visitor:** crank Clog Drive past ~65% and an old man walks in, drops his trousers, sits down and strains: red-faced, sweating, steaming, fists clenched. Bring the drive below 50% and he stands up and leaves (the poop is a little bigger afterwards). The camera pulls back while the room is occupied.

## Dev snapshot mode

The standalone app can render itself to a PNG and quit:
`LAYSHIT_SNAPSHOT=out.png [LAYSHIT_PRESET=9] [LAYSHIT_DRIVE=1] [LAYSHIT_PAGE=0..3] [LAYSHIT_SECONDS=3] [LAYSHIT_PULSES=1] [LAYSHIT_FLUSH_AT=ms] LayShit.exe`
