#!/bin/bash
# Lager en grafisk installasjonsapp («SLOWHAND Installer.app») i en .dmg.
# Appen viser alle plugins som kort med bilde og kort beskrivelse, og brukeren
# krysser av for hvilke som skal installeres (og om AU og/eller VST3).
# Kjør scripts/build.sh --all først.
#
#   scripts/make-installer-app.sh             # -> dist/SLOWHAND-<versjon>.dmg
#   scripts/make-installer-app.sh 1.2.0       # egen versjon
#
# Per plugin brukes:
#   plugins/<Navn>/docs/screenshot.png  – bildet på kortet
#   plugins/<Navn>/docs/installer.txt   – kort beskrivelse (2–3 setninger)
#   plugins/<Navn>/README.md            – første linje gir undertittelen
#
# Signering (valgfritt): sett SBAUDIO_APP_IDENTITY til en «Developer ID Application: …»-
# identitet. Uten den blir appen ad-hoc-signert (høyreklikk › Åpne første gang).
set -euo pipefail
export COPYFILE_DISABLE=1
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

PLUGINS=()
for d in plugins/*/; do
  n="$(basename "$d")"
  [[ "$n" == _* ]] && continue
  [ -f "$d/CMakeLists.txt" ] || continue
  PRODUCT=$(sed -n 's/.*PRODUCT_NAME *"\(.*\)".*/\1/p' "$d/CMakeLists.txt" | head -1)
  [ -d "dist/$PRODUCT.component" ] || [ -d "dist/$PRODUCT.vst3" ] || { echo "Hopper over $PRODUCT (ikke i dist/)"; continue; }
  PLUGINS+=("$n")
done
[ ${#PLUGINS[@]} -gt 0 ] || { echo "Ingen plugins i dist/. Kjør scripts/build.sh --all først."; exit 1; }

VERSION="${1:-$(cat VERSION 2>/dev/null)}"   # installasjonsversjon, se scripts/release.sh
VERSION="${VERSION:-1.0.0}"

WORK="build/installer-app"
APP="$WORK/dmg/SLOWHAND Installer.app"
RES="$APP/Contents/Resources"
rm -rf "$WORK"
mkdir -p "$APP/Contents/MacOS" "$RES/Images" "$RES/Plugins"

# --- Manifest + ressurser ---------------------------------------------------
MANIFEST="$RES/plugins.plist"
plutil -create xml1 "$MANIFEST"
plutil -insert version -string "$VERSION" "$MANIFEST"
plutil -insert plugins -array "$MANIFEST"

i=0
for n in "${PLUGINS[@]}"; do
  dir="plugins/$n"
  PRODUCT=$(sed -n 's/.*PRODUCT_NAME *"\(.*\)".*/\1/p' "$dir/CMakeLists.txt" | head -1)
  # Undertittel: «VoxPilot – automatisk vokal-rider» -> «Automatisk vokal-rider»
  tagline=$(sed -n '1s/^# *//p' "$dir/README.md" 2>/dev/null | sed 's/^[^ ]* – //')
  tagline="$(echo "${tagline:0:1}" | tr '[:lower:]' '[:upper:]')${tagline:1}"
  if [ -f "$dir/docs/installer.txt" ]; then
    summary=$(tr '\n' ' ' < "$dir/docs/installer.txt" | sed 's/ *$//')
  else # første avsnitt i README, kortet ned til ca. 300 tegn (hele setninger)
    echo "Merk: plugins/$n/docs/installer.txt mangler – bruker starten av README.md"
    summary=$(awk 'NR>1 && NF{p=1} p&&!NF{exit} p' "$dir/README.md" 2>/dev/null | tr '\n' ' ' \
      | sed 's/\*\*//g; s/!\[[^]]*\]([^)]*)//g; s/ *$//')
    summary=$(echo "$summary" | awk '{ out=""; n=split($0, s, /\. /);
      for (k=1; k<=n; k++) { t = s[k] (k<n ? ". " : ""); if (length(out)>0 && length(out t)>300) break; out = out t }
      sub(/ +$/, "", out); print out }')
    [ -n "$summary" ] || summary="$PRODUCT fra SLOWHAND."
  fi
  [ -n "$tagline" ] || tagline="SLOWHAND-plugin"
  hasAU=false; hasVST3=false
  [ -d "dist/$PRODUCT.component" ] && { hasAU=true; ditto --norsrc --noextattr "dist/$PRODUCT.component" "$RES/Plugins/$PRODUCT.component"; }
  [ -d "dist/$PRODUCT.vst3" ] && { hasVST3=true; ditto --norsrc --noextattr "dist/$PRODUCT.vst3" "$RES/Plugins/$PRODUCT.vst3"; }
  if [ -f "$dir/docs/screenshot.png" ]; then
    sips -Z 1000 "$dir/docs/screenshot.png" --out "$RES/Images/$PRODUCT.png" >/dev/null
  else
    echo "Merk: plugins/$n/docs/screenshot.png mangler – kortet får et navneskilt i stedet for bilde"
  fi

  plutil -insert "plugins.$i" -dictionary "$MANIFEST"
  plutil -insert "plugins.$i.name" -string "$PRODUCT" "$MANIFEST"
  plutil -insert "plugins.$i.tagline" -string "$tagline" "$MANIFEST"
  plutil -insert "plugins.$i.summary" -string "$summary" "$MANIFEST"
  plutil -insert "plugins.$i.hasAU" -bool "$hasAU" "$MANIFEST"
  plutil -insert "plugins.$i.hasVST3" -bool "$hasVST3" "$MANIFEST"
  i=$((i + 1))
done

# --- Kompiler (universal) ----------------------------------------------------
for arch in arm64 x86_64; do
  # Nyere Command Line Tools mangler x86_64-utgaven av Swift-kompatibilitetsbibliotekene.
  # Appen bruker ikke async/await e.l., så de trengs ikke.
  extra=()
  [ "$arch" = x86_64 ] && extra=(-runtime-compatibility-version none)
  swiftc -O -swift-version 5 -parse-as-library -target "$arch-apple-macos11.0" ${extra[@]+"${extra[@]}"} \
    installer/SBAudioInstaller.swift -o "$WORK/installer-$arch" 2>&1 | { grep -v '^ld: warning' || true; }
  [ -x "$WORK/installer-$arch" ] || { echo "Kompilering feilet ($arch)"; exit 1; }
done
lipo -create "$WORK/installer-arm64" "$WORK/installer-x86_64" -output "$APP/Contents/MacOS/SBAudioInstaller"

cat > "$APP/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>CFBundleExecutable</key><string>SBAudioInstaller</string>
  <key>CFBundleIdentifier</key><string>com.sbaudio.installer</string>
  <key>CFBundleName</key><string>SLOWHAND Installer</string>
  <key>CFBundleDisplayName</key><string>SLOWHAND Installer</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleShortVersionString</key><string>$VERSION</string>
  <key>CFBundleVersion</key><string>$VERSION</string>
  <key>LSMinimumSystemVersion</key><string>11.0</string>
  <key>NSHighResolutionCapable</key><true/>
  <key>NSPrincipalClass</key><string>NSApplication</string>
</dict></plist>
EOF

# Pluginene inne i appen er allerede signert – signer bare selve appen (ikke --deep)
codesign --force --options runtime -s "${SBAUDIO_APP_IDENTITY:--}" "$APP"

# --- DMG ---------------------------------------------------------------------
OUT="dist/SLOWHAND-$VERSION.dmg"
rm -f "$OUT"
hdiutil create -volname "SLOWHAND $VERSION" -srcfolder "$WORK/dmg" -ov -format UDZO "$OUT" >/dev/null
echo "Laget $OUT (${PLUGINS[*]})"
