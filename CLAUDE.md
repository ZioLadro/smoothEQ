# SMOOTHEQ – note per lo sviluppo

- JUCE 8 audio effect (VST3/AU, optional AAX and VST2): a dynamic resonance suppressor. Fourth of the family after SONIC GRAIN, DR. SEGG-UENCER and MULTITAILS: same look and toolchain, violet ground with a mint slab and silk ribbons (`ribbon` in `SmoothUI.h`); `silkBand` replaces the siblings' hazard stripes / fur band. Section titles are text only (kicker Rubik Medium 11 / 0.42, section Rubik Bold 17 / 0.32, line at 48 px), as in the family rule of 2026-11-04.
- `Source/SmoothCore.h` and `SmoothEngine.h` hold no JUCE and must stay real-time safe: no allocation, no locks after `prepare()`. `Engine::prepare` allocates for the largest FFT any resolution needs at that sample rate, so a resolution change on the audio thread only rebuilds tables (and clears the buffers: a short dropout, accepted).
- Latency = FFT size (`fftSizeFor`). The processor learns of a change in `run()` and tells the host from the message thread (`AsyncUpdater`). Bypass (`processBlockBypassed`) fades to the delayed dry signal, so the latency never changes.
- With Depth 0 the output equals the input delayed by N within 1e-6 (tested at every rate and resolution): keep the windows and the OLA scaling (Hann x Hann at 75 % overlap sums to 1.5) as they are.
- Build + tests: `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DSMOOTHEQ_BUILD_TESTS=ON -DSMOOTHEQ_COPY_AFTER_BUILD=OFF && cmake --build build --target SmoothEQTests -j2`, then run `build/Tests/SmoothEQTests_artefacts/Release/SmoothEQTests`. `--snap file.png band preset scale` renders the editor (preset -1 = demo curve); look at it after any UI change.
- JUCE: `-DJUCE_DIR=<path>` for a local copy; otherwise FetchContent downloads tag 8.0.10.
- Headless Linux without the X dev packages: export `CXXFLAGS="-DJUCE_USE_XRANDR=0 -DJUCE_USE_XINERAMA=0 -DJUCE_USE_XCURSOR=0 -DJUCE_USE_XRENDER=0 -DJUCE_ALSA=0 -DJUCE_JACK=0"` before configuring.
- Never change these, or hosts lose saved sessions: plugin code `Smeq`, manufacturer code `Gsof`, parameter ids (`id::` in `Params.h`, `b1_freq` ... `b6_q`), or the order of any choice list (`modeNames`, `resolutionNames`, `stereoNames`). Append only.
- On/off parameters are 2-item choices (`bparam`), so pluginval strictness 8 passes the state-restore test.
- The plugin target builds with JUCE's recommended warning flags and has no warnings: keep it that way (`exactlyEqual` for intended float comparisons).
- Release: update `CHANGELOG.md` and `project(... VERSION x.y.z)` in `CMakeLists.txt`, then push tag `vx.y.z` (or `scripts/release.*`). CI builds and publishes the Release.
- The user is Italian: user-facing docs (README, CHANGELOG, scripts) are in Italian. The interface is in English, like the sibling plugins.
