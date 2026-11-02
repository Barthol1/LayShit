# LayShit — the delay you lay

## Context
LayShit is a new VST3 delay plugin for the user's own tracks and for friends. The GUI is a real-time 3D top-down view into a toilet. As knobs go up, the poop in the bowl shifts, morphs and spins. When Crystals (granular) and the pitch shimmer are high, the toilet goes **holy** and god-rays shoot out of the bowl. The repo `C:\dev\layshit` is empty, so this is a greenfield build. The toolchain is VS 2022 and git. CMake comes bundled with VS, and JUCE will be fetched via CMake FetchContent.

## Decisions (from the quiz)
- **Stack:** JUCE 8 (C++20) with the CMake build, **VST3** plus a Standalone target for dev testing.
- **Routing:** **draggable "pipes"** to reorder the modules inside the feedback loop.
- **Render:** **full real-time 3D** with `juce::OpenGLContext` and custom GLSL. Meshes are **procedural** (lathe-profile bowl, deformable coil poop, water plane).
- **Holy Mode:** emerges from the knobs as `holy = smoothstep(Crystals * shimmerAmt)`. Visuals and a subtle shimmer track it, and there is no switch.
- **Layout:** the bowl fills the center, knobs sit on the porcelain rim, and the pipes row runs along the bottom.
- **v1 modules:** Core Delay, Clog, Shift, Crystals (+shimmer), Plunge, Flush, Courtesy Flush.
- **Wishlist (after v1):** Bristol macro knob, overflow/self-oscillation, toilet-paper preset roll, transient splash.

## Architecture
```
Source/
  PluginProcessor.{h,cpp}   APVTS params, processBlock, module order state
  dsp/
    DelayLine.h             fractional (Hermite) circular buffer, modulated read
    ModuleSlot.h            interface: prepare/reset/process(block) + bypass/mix
    Clog.h                  drive (tanh/asym) + bitcrush/downsample
    Shift.h                 Hilbert-transform (IIR allpass pair) frequency shifter ±Hz
    Crystals.h              grain engine on its own capture buffer: size, density, spray, pitch, reverse prob, freeze
    Shimmer.h               pitch shifter (2-tap crossfade) for +12/+7 → feeds "holy"
    Plunge.h                allpass diffusion chain (4–8 stages)
    Ducker.h                envelope follower on dry input → feedback/wet gain (Courtesy Flush)
    FlushFx.h               one-shot: pitch-down sweep + buffer drain
    FeedbackChain.h         ordered array<ModuleSlot*>, lock-free order swap
  gfx/
    BowlRenderer.{h,cpp}    OpenGLRenderer: camera, passes
    Meshes.{h,cpp}          lathe bowl, coil poop (tube along a spiral spline), water grid
    shaders/*.glsl          porcelain (PBR-ish + env), water (normal-mapped ripples, refraction), poop (vertex deform), godrays (radial blur post)
    VisualState.h           atomics written by the audio thread (levels, tap times, holy, flush phase)
  ui/
    RimKnobs.{h,cpp}        custom LookAndFeel knobs placed on the rim ellipse
    PipesRow.{h,cpp}        drag-to-reorder module pipe segments
```
**Signal flow:** in → [Ducker sidechain] → DelayLine write(in + fb). The read output passes through FeedbackChain (the user's order of Clog/Shift/Crystals+Shimmer/Plunge), then through filters (Fiber HP/LP), then a soft-limiter, and loops back as feedback. The wet signal is mixed with dry.
**Thread safety:** module order is held in an atomic index array and applied at block start, with a short crossfade on order change. GUI ↔ audio talk only through APVTS and `VisualState` atomics.

## Parameters (APVTS)
Time (ms / sync note), Sync, Feedback, Mix, PingPong, Wobble (rate/depth), Fiber HP/LP; Clog Drive/Crush; Shift Hz; Crystals Amount/Size/Density/Spray/Pitch/Reverse/Freeze; Shimmer Amt/Interval; Plunge Diffuse; Courtesy Amount/Release; Flush (trigger); module order (stored in state XML, not automatable).

## Visual mapping (sound → bowl)
| Control | Poop / bowl behavior |
|---|---|
| Feedback | drift radius, bob amplitude; >95% water level rises |
| Time | ripple spacing; ripple per delay tap |
| Wobble | jelly vertex wobble |
| Clog | surface roughness, darker/crustier shading |
| Shift | rotation speed + direction (±Hz) and spiral swirl in the water |
| Crystals | crystalline facets/sparkles on poop, floating particles |
| Shimmer × Crystals | **holy**: golden emissive, halo ring, volumetric god-rays out of bowl, bloom |
| Plunge | water haze/blur depth |
| Courtesy duck | poop sinks while playing, surfaces on release |
| Flush | whirlpool vortex animation, poop spirals down, respawns |

## Phases (each ends in a loadable VST3)
1. **Skeleton:** CMake + FetchContent JUCE, VST3 + Standalone, APVTS, core delay (time/sync/fb/mix/pingpong/wobble/filters), and a basic generic UI.
2. **Loop modules:** Clog, Shift, Plunge, Crystals + Shimmer, Ducker, Flush in a fixed order, plus a soft limiter.
3. **Reorderable chain:** FeedbackChain with order state, save/restore, and click-free swapping.
4. **3D bowl:** OpenGL context, procedural meshes, porcelain and water shaders, top-down camera, rim knobs.
5. **Mapping + Holy:** VisualState feed, poop deformation, ripples per tap, god-ray post-pass, flush vortex.
6. **Pipes UI + polish:** drag-reorder row, a few factory presets, CPU/GPU profiling.
7. **Wishlist:** Bristol macro, overflow, TP preset roll, transient splash.

First step of execution: write the design spec to `docs/superpowers/specs/2026-11-02-layshit-design.md`. After that, write a per-phase implementation plan via writing-plans.

## Verification
- Build with `cmake -B build -G "Visual Studio 17 2022"` and `cmake --build build --config Release` (CMake installed via winget — VS 2022 Community has MSVC 14.44 but no bundled CMake).
- Run **pluginval** (strictness 5+) on the VST3 every phase.
- Write DSP unit tests (JUCE UnitTest or Catch2 console target) covering impulse → tap at the expected sample offset, feedback decay < 1 with the limiter, shifter Hz accuracy via FFT peak, order swap with no NaNs/denormals, and silence in → silence out.
- Test by hand in the Standalone app and in the user's DAW: automation, state save/reload, rapid order drags, and Flush spam.
- Check GPU behavior: the GL context survives editor close/reopen and runs at 60 fps with acceptable CPU.

## Module DSP details
- **DelayLine:** max 4 s per channel, Hermite interpolation, time smoothed (~50 ms) to avoid zipper noise; Wobble = sine + smoothed-random LFO on read position (0–10 ms depth).
- **Clog:** pre-gain → asymmetric tanh → sample-rate reduce (1–32x hold) + bit depth (16→4 bits) blended by Crush; DC blocker after.
- **Shift:** 2x 4-stage allpass Hilbert pair → complex multiply with quadrature oscillator; range ±1000 Hz, exponential knob curve, center detent at 0.
- **Crystals:** 2 s capture ring buffer; up to 64 Hann-windowed grains; Size 10–500 ms, Density 1–100 grains/s, Spray 0–1 s position jitter, Pitch ±24 st, Reverse probability 0–1, Freeze stops capture writes. Amount = dry/grain blend within the slot.
- **Shimmer:** dual-tap crossfading pitch shifter (+12 / +7 / +19 st), Amt 0–1; lives in the Crystals slot (post-grain).
- **Plunge:** 6 Schroeder allpasses (prime delays 3–40 ms scaled), g = 0.5–0.75 by Diffuse.
- **Ducker (Courtesy Flush):** envelope follower on dry input (attack 5 ms, release 50–2000 ms); gain reduction up to Amount applied to wet output.
- **Flush:** on trigger, ~1.5 s: playback rate ramps down (pitch dive) while feedback → 0 and buffer fades to silence; VisualState.flushPhase 0→1.
- **Safety:** feedback clamped ≤ 1.1, soft-clip limiter in loop, denormal flush (ScopedNoDenormals), NaN guard resets loop.

## Holy formula
`holy = smoothstep(0.25, 0.75, crystalsAmount * shimmerAmt * 2)` — smoothed 200 ms, published to VisualState; drives emissive gold, halo, god-ray intensity, bloom.

## Out of scope for v1
AU/CLAP, Mac builds, installers, licensing, MIDI control beyond host automation, oversampling (revisit if Clog aliases badly).
- **Local only:** no git repo, no commits, no pushes.
