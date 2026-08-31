# Audio backends

`audioSystem` abstracts playback. Backends:

* `"sdl"` — SDL3 audio: opens the default device, loads WAVs, and plays
  them through SDL3 audio streams (overlapping playback, per-stream volume).
