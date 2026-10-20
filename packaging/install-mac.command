#!/bin/bash
# Installs T150 Synth into your user plugin folders. Double-click to run.

cd "$(dirname "$0")" || exit 1

VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"

echo "Installing T150 Synth..."
mkdir -p "$VST3_DIR" "$AU_DIR"

rm -rf "$VST3_DIR/T150 Synth.vst3" "$AU_DIR/T150 Synth.component"
cp -R "T150 Synth.vst3"      "$VST3_DIR/"
cp -R "T150 Synth.component" "$AU_DIR/"

# Tell macOS these downloaded files are OK to load.
xattr -dr com.apple.quarantine "$VST3_DIR/T150 Synth.vst3" "$AU_DIR/T150 Synth.component" 2>/dev/null
xattr -dr com.apple.quarantine "T150 Synth.app" 2>/dev/null

# Make macOS notice the new Audio Unit without a reboot.
killall -9 AudioComponentRegistrar 2>/dev/null

echo ""
echo "Done! Restart your DAW and look for \"T150 Synth\"."
echo "You can close this window."
