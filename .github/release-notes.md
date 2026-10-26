Nave is a reverb suite for Ableton Live: twenty-nine reverb types with one set
of controls for shaping them, a search box to find them, and a display that
shows the actual tail you've dialled in.

## What's in 0.1.0

- **29 types** in six categories: rooms (including a movable-source
  Reflections room), halls, plates, springs, ambient (Cloud, Bloom, Shimmer,
  Halo, Abyss, Choir, Swell) and special (Nonlinear, Gated, Reverse, Tape Echo,
  Lo-Fi, Resonant).
- **Search** types and presets by name or by sound: `dark`, `drums`, `80s`,
  `octave`, `church`...
- **Shaping** for every type: DECAY, SIZE, PRE-DELAY (with tempo SYNC),
  DIFFUSION, EARLY; TONE, LOW END, MID, LOW CUT, HIGH CUT; DEPTH and RATE;
  DUCK, GATE and RELEASE; WIDTH. Plus up to three CHARACTER knobs and a choice
  that belong to the type.
- **HOLD**, INFINITE or FREEZE.
- **TAIL display**: the real impulse response of the current settings, with
  the decay times of the whole tail, its lows and its highs.
- 33 factory presets, two of them for return tracks, and KEEP MIX.

## Install

Unzip and copy `Nave.vst3` into your VST3 folder (macOS
`~/Library/Audio/Plug-Ins/VST3`, Windows `C:\Program Files\Common Files\VST3`,
Linux `~/.vst3`), then rescan plug-ins in Live. The getting started guide
(PDF) is in the download.

**macOS:** the plugin isn't notarised. If macOS refuses to load it, run
`xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Nave.vst3`.
