#!/bin/bash
# Bygger én eller flere SB Audio-plugins som AU + VST3, installerer i
# ~/Library/Audio/Plug-Ins, ad-hoc-signerer, validerer med auval og kopierer til dist/.
#
#   scripts/build.sh VoxPilot            # én plugin (Release)
#   scripts/build.sh VoxPilot Debug      # Debug-bygg
#   scripts/build.sh --all               # alle plugins i plugins/
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

# CMake installert med «pip install --user cmake» ligger ikke i PATH som standard
if ! command -v cmake >/dev/null; then
  for d in "$HOME"/Library/Python/*/bin; do [ -x "$d/cmake" ] && PATH="$d:$PATH"; done
fi
command -v cmake >/dev/null || { echo "Fant ikke cmake. Installer med: python3 -m pip install --user cmake"; exit 1; }

if [ $# -lt 1 ]; then
  echo "Bruk: scripts/build.sh <PluginNavn|--all> [Release|Debug]"; exit 1
fi

if [ "$1" = "--all" ]; then
  NAMES=()
  for d in plugins/*/; do
    n="$(basename "$d")"
    [[ "$n" == _* ]] && continue
    [ -f "$d/CMakeLists.txt" ] && NAMES+=("$n")
  done
else
  NAMES=("$1")
fi
CONFIG="${2:-Release}"

cmake -S . -B build -DCMAKE_BUILD_TYPE="$CONFIG" -DSBAUDIO_ONLY=

TARGETS=()
for n in "${NAMES[@]}"; do
  [ -f "plugins/$n/CMakeLists.txt" ] || { echo "Fant ikke plugins/$n"; exit 1; }
  TARGETS+=("${n}_AU" "${n}_VST3")
done
cmake --build build --config "$CONFIG" --target "${TARGETS[@]}" -j "$(sysctl -n hw.ncpu)"

mkdir -p dist
killall -9 AudioComponentRegistrar 2>/dev/null || true

for n in "${NAMES[@]}"; do
  CM="plugins/$n/CMakeLists.txt"
  PRODUCT=$(sed -n 's/.*PRODUCT_NAME *"\(.*\)".*/\1/p' "$CM" | head -1)
  CODE=$(awk '$1=="PLUGIN_CODE"{print $2; exit}' "$CM")
  case "$(awk '$1=="AU_MAIN_TYPE"{print $2; exit}' "$CM")" in
    kAudioUnitType_MusicDevice) AUTYPE=aumu ;;
    kAudioUnitType_MusicEffect) AUTYPE=aumf ;;
    kAudioUnitType_MIDIProcessor) AUTYPE=aumi ;;
    *) AUTYPE=aufx ;;
  esac

  COMP="$HOME/Library/Audio/Plug-Ins/Components/$PRODUCT.component"
  VST3="$HOME/Library/Audio/Plug-Ins/VST3/$PRODUCT.vst3"
  codesign --force --deep -s - "$COMP"          # kreves for arm64 / Logic
  [ -d "$VST3" ] && codesign --force --deep -s - "$VST3"

  echo "--- auval $AUTYPE $CODE Sbau ($PRODUCT) ---"
  auval -v "$AUTYPE" "$CODE" Sbau | tail -n 5

  rm -rf "dist/$PRODUCT.component" "dist/$PRODUCT.vst3"
  cp -R "$COMP" dist/
  [ -d "$VST3" ] && cp -R "$VST3" dist/
  echo "Ferdig: $PRODUCT"
done
