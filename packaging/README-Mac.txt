T150 Synth - Mac
================

An MS-20 style monophonic synth, played straight into a tube amp
(the T-150) and speaker cabinet.
Works on Apple Silicon and Intel Macs, macOS 11 or newer.

INSTALL (about 1 minute)
------------------------
1. Double-click "Install.command".
   - If macOS says it can't be opened: right-click it, choose Open,
     then click Open again.
2. Restart your DAW and load "T150 Synth" on an instrument (MIDI) track.
   (Logic and GarageBand use the Audio Unit version; Ableton, Bitwig,
   Reaper, FL Studio and most others can use the VST3.)

WHAT THE INSTALLER DOES
-----------------------
It copies the plugins into your user plugin folders:
   ~/Library/Audio/Plug-Ins/VST3/T150 Synth.vst3
   ~/Library/Audio/Plug-Ins/Components/T150 Synth.component
and tells macOS you trust them. That's needed because this plugin
isn't signed with a paid Apple developer account.

PREFER TO DO IT BY HAND?
------------------------
1. Copy the two files above into those folders.
2. Open Terminal and run:
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"T150 Synth.vst3" ~/Library/Audio/Plug-Ins/Components/"T150 Synth.component"

JUST WANT TO TRY IT?
--------------------
Open "T150 Synth.app" (right-click > Open the first time).
In its Options, pick your audio output and your MIDI keyboard.

QUICK START
-----------
- It plays one note at a time, like the original. Hold two keys and the
  lower one wins.
- LOW PASS CUTOFF and PEAK are the heart of the sound. Turn PEAK up
  for the classic MS-20 scream.
- The FILTER switch picks EARLY (wild, screaming) or LATE (smoother).
- Turn the AMP's MASTER up for heavy power-tube distortion, or flip the
  AMP switch off to hear the synth clean.
- PATCH > AMP FB feeds the amp's output back into the filter. Start low!
- Double-click any knob to reset it.
