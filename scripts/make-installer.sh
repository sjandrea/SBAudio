#!/bin/bash
# Lager en macOS-installasjonspakke (.pkg) med alle plugins som ligger i dist/.
# Kjør scripts/build.sh --all først.
#
#   scripts/make-installer.sh                 # -> dist/SLOWHAND-<versjon>.pkg
#   scripts/make-installer.sh 1.2.0           # egen versjon
#
# Pakken installerer AU i /Library/Audio/Plug-Ins/Components og VST3 i
# /Library/Audio/Plug-Ins/VST3. Brukeren kan velge plugins og formater.
#
# Signering (valgfritt): sett SBAUDIO_INSTALLER_IDENTITY til en
# «Developer ID Installer: …»-identitet. Uten den blir pakken usignert.
set -euo pipefail
export COPYFILE_DISABLE=1
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

# Sortert etter kategori (scripts/categories.sh) – kompressorer sammen, romklang og delay sammen osv.
source scripts/categories.sh
DIRS=()
for d in plugins/*/; do
  n="$(basename "$d")"
  [[ "$n" == _* ]] && continue
  [ -f "$d/CMakeLists.txt" ] || continue
  DIRS+=("$n")
done
PLUGINS=()      # produktnavn
PLUGIN_CATS=()  # kategorinøkkel per plugin
while IFS= read -r n; do
  PRODUCT=$(sed -n 's/.*PRODUCT_NAME *"\(.*\)".*/\1/p' "plugins/$n/CMakeLists.txt" | head -1)
  [ -d "dist/$PRODUCT.component" ] || [ -d "dist/$PRODUCT.vst3" ] || { echo "Hopper over $PRODUCT (ikke i dist/)"; continue; }
  PLUGINS+=("$PRODUCT")
  PLUGIN_CATS+=("$(sbaudio_category "plugins/$n" 2>/dev/null)")
done < <(sbaudio_sort_plugins ${DIRS[@]+"${DIRS[@]}"})
[ ${#PLUGINS[@]} -gt 0 ] || { echo "Ingen plugins i dist/. Kjør scripts/build.sh --all først."; exit 1; }

VERSION="${1:-$(cat VERSION 2>/dev/null)}"   # installasjonsversjon, se scripts/release.sh
VERSION="${VERSION:-1.0.0}"

WORK="build/installer"
rm -rf "$WORK"
mkdir -p "$WORK/pkgs" "$WORK/scripts" "$WORK/resources"

# Etter installasjon: få macOS til å skanne Audio Units på nytt
cat > "$WORK/scripts/postinstall" <<'EOF'
#!/bin/bash
killall -9 AudioComponentRegistrar 2>/dev/null || true
exit 0
EOF
chmod +x "$WORK/scripts/postinstall"

CHOICES_OUTLINE=""
CHOICES=""
REFS=""

# Hver plugin blir en gruppe i «Tilpass»-skjermen med AU og VST3 som undervalg.
add_pkg() { # <bundle-sti> <installasjonsmappe> <pkg-id> <tittel> <beskrivelse>
  local src="$1" dest="$2" id="$3" title="$4" desc="$5"
  local root="$WORK/root/$id"
  mkdir -p "$root"
  # Uten utvidede attributter, ellers havner ._-filer i pakken
  ditto --norsrc --noextattr --noacl "$src" "$root/$(basename "$src")"
  pkgbuild --analyze --root "$root" "$WORK/$id.plist" >/dev/null
  # Ikke la Installer flytte bundlen til en annen plassering den finner på disken
  plutil -replace 0.BundleIsRelocatable -bool NO "$WORK/$id.plist"
  pkgbuild --root "$root" --component-plist "$WORK/$id.plist" \
    --identifier "$id" --version "$VERSION" --install-location "$dest" \
    --scripts "$WORK/scripts" "$WORK/pkgs/$id.pkg" >/dev/null
  GROUP_LINES+="            <line choice=\"$id\"/>"$'\n'
  CHOICES+="    <choice id=\"$id\" title=\"$title\" description=\"$desc\">
        <pkg-ref id=\"$id\"/>
    </choice>"$'\n'
  REFS+="    <pkg-ref id=\"$id\" version=\"$VERSION\">$id.pkg</pkg-ref>"$'\n'
}

plugin_description() { # første linje i pluginens README, f.eks. «OptoTube – opto/rør-kompressor»
  local readme="plugins/$1/README.md"
  [ -f "$readme" ] && sed -n '1s/^# *//p' "$readme" | sed 's/&/\&amp;/g; s/</\&lt;/g; s/"/\&quot;/g' || echo "$1"
}

CUR_CAT=""
for idx in "${!PLUGINS[@]}"; do
  p="${PLUGINS[$idx]}"
  cat_key="${PLUGIN_CATS[$idx]}"
  if [ "$cat_key" != "$CUR_CAT" ]; then # ny kategori: egen gruppe i «Tilpass»-skjermen
    [ -n "$CUR_CAT" ] && CHOICES_OUTLINE+="        </line>"$'\n'
    cat_id="com.sbaudio.category.$(echo "$cat_key" | tr '[:upper:]' '[:lower:]')"
    cat_title=$(sbaudio_category_title "$cat_key")
    CHOICES_OUTLINE+="        <line choice=\"$cat_id\">"$'\n'
    CHOICES+="    <choice id=\"$cat_id\" title=\"$cat_title\" description=\"$cat_title\"/>"$'\n'
    CUR_CAT="$cat_key"
  fi
  lower=$(echo "$p" | tr '[:upper:]' '[:lower:]')
  GROUP_LINES=""
  [ -d "dist/$p.component" ] && add_pkg "dist/$p.component" /Library/Audio/Plug-Ins/Components \
    "com.sbaudio.$lower.au" "$p – Audio Unit" "$p for Logic Pro, GarageBand og andre AU-verter."
  [ -d "dist/$p.vst3" ] && add_pkg "dist/$p.vst3" /Library/Audio/Plug-Ins/VST3 \
    "com.sbaudio.$lower.vst3" "$p – VST3" "$p for VST3-verter (Ableton Live, Cubase, Reaper …)."
  CHOICES_OUTLINE+="        <line choice=\"com.sbaudio.$lower\">"$'\n'"$GROUP_LINES        </line>"$'\n'
  CHOICES+="    <choice id=\"com.sbaudio.$lower\" title=\"$p\" description=\"$(plugin_description "$p")\"/>"$'\n'
done
[ -n "$CUR_CAT" ] && CHOICES_OUTLINE+="        </line>"$'\n'

LIST=""; CUR_CAT=""
for idx in "${!PLUGINS[@]}"; do
  if [ "${PLUGIN_CATS[$idx]}" != "$CUR_CAT" ]; then
    [ -n "$CUR_CAT" ] && LIST+="</ul>"
    CUR_CAT="${PLUGIN_CATS[$idx]}"
    LIST+="<p style=\"margin-bottom:2px\"><b>$(sbaudio_category_title "$CUR_CAT")</b></p><ul style=\"margin-top:2px\">"
  fi
  LIST+="<li>${PLUGINS[$idx]}</li>"
done
[ -n "$CUR_CAT" ] && LIST+="</ul>"
cat > "$WORK/resources/welcome.html" <<EOF
<html><body style="font-family:-apple-system,sans-serif;font-size:13px">
<h2>SLOWHAND $VERSION</h2>
<p>Denne pakken installerer:</p>$LIST
<p>Audio Unit-versjonene legges i <code>/Library/Audio/Plug-Ins/Components</code>,
VST3 i <code>/Library/Audio/Plug-Ins/VST3</code>. På neste side velger du hvilke plugins
(og formater) som skal installeres.</p>
<p>Start Logic Pro på nytt etter installasjonen.</p>
</body></html>
EOF

cat > "$WORK/resources/conclusion.html" <<EOF
<html><body style="font-family:-apple-system,sans-serif;font-size:13px">
<h2>Ferdig</h2>
<p>Start Logic Pro på nytt. Pluginene ligger under <b>Audio Units › SLOWHAND</b>.</p>
<p>Hvis du tidligere har installert pluginene med <code>scripts/build.sh</code>, ligger det også
kopier i <code>~/Library/Audio/Plug-Ins</code>. Slett dem der så Logic ikke ser to versjoner.</p>
</body></html>
EOF

cat > "$WORK/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>SLOWHAND $VERSION</title>
    <organization>com.sbaudio</organization>
    <welcome file="welcome.html" mime-type="text/html"/>
    <conclusion file="conclusion.html" mime-type="text/html"/>
    <options customize="always" require-scripts="false" hostArchitectures="arm64,x86_64"/>
    <domains enable_localSystem="true"/>
    <os-version min="11.0"/>
    <choices-outline>
$CHOICES_OUTLINE    </choices-outline>
$CHOICES$REFS</installer-gui-script>
EOF

OUT="dist/SLOWHAND-$VERSION.pkg"
rm -f "$OUT"
productbuild --distribution "$WORK/distribution.xml" --package-path "$WORK/pkgs" \
  --resources "$WORK/resources" "$WORK/unsigned.pkg" >/dev/null

if [ -n "${SBAUDIO_INSTALLER_IDENTITY:-}" ]; then
  productsign --sign "$SBAUDIO_INSTALLER_IDENTITY" "$WORK/unsigned.pkg" "$OUT"
else
  mv "$WORK/unsigned.pkg" "$OUT"
fi
echo "Laget $OUT (${PLUGINS[*]})"
