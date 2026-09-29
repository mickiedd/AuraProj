#!/bin/zsh
# Launch the isolated, staged Canton engineering district.
set -euo pipefail
cd "${0:A:h}"
app="$PWD/Saved/CantonContinuation/Stage/Mac/Aura.app"
if [[ ! -x "$app/Contents/MacOS/Aura" ]]; then
  print -u2 'Canton delivery is not staged. See QA/Canton_Continuation/README.md.'
  exit 1
fi
# This terrain walkthrough has no browser UI. Keep CEF out of this profile.
open -n "$app" --args /Game/Canton/DistrictPrototype/Maps/L_Canton_District_PROVISIONAL -nocef -windowed -ResX=1920 -ResY=1080 -ForceRes -ExecCmds="sg.ViewDistanceQuality 2,sg.AntiAliasingQuality 2,sg.ShadowQuality 2,sg.GlobalIlluminationQuality 2,sg.ReflectionQuality 2,sg.PostProcessQuality 2,sg.TextureQuality 2,sg.EffectsQuality 2,sg.FoliageQuality 2,sg.ShadingQuality 2,r.ScreenPercentage 100"
