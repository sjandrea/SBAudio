# Kategorier i installasjonsfilene (dmg og pkg). Brukes av make-installer-app.sh og make-installer.sh.
#
# Hver plugin velger kategori i plugins/<Navn>/docs/category.txt (én linje, nøkkelen til venstre under).
# Rekkefølgen her er rekkefølgen i installasjonsprogrammet; innenfor en kategori sorteres alfabetisk.
# Mangler category.txt, havner instrumenter under «Instrument» og effekter under «Annet».
SBAUDIO_CATEGORIES=(
  "Dynamikk|Kompressorer og dynamikk"
  "EQ|EQ og tonal balanse"
  "Metning|Metning"
  "Rom|Romklang og delay"
  "Mastering|Mastering"
  "Annet|Andre effekter"
  "Instrument|Instrumenter"
)

# sbaudio_category <plugin-mappe> -> kategorinøkkel
sbaudio_category() {
  local dir="$1" c=""
  [ -f "$dir/docs/category.txt" ] && c=$(head -1 "$dir/docs/category.txt" | tr -d '[:space:]')
  if [ -z "$c" ]; then
    if grep -q 'kAudioUnitType_MusicDevice' "$dir/CMakeLists.txt" 2>/dev/null; then c="Instrument"; else c="Annet"; fi
  fi
  local e
  for e in "${SBAUDIO_CATEGORIES[@]}"; do [ "${e%%|*}" = "$c" ] && { echo "$c"; return; }; done
  echo "Merk: ukjent kategori «$c» i $dir/docs/category.txt – bruker Annet" >&2
  echo "Annet"
}

# sbaudio_category_title <nøkkel> -> overskrift
sbaudio_category_title() {
  local e
  for e in "${SBAUDIO_CATEGORIES[@]}"; do [ "${e%%|*}" = "$1" ] && { echo "${e#*|}"; return; }; done
  echo "$1"
}

# sbaudio_sort_plugins <mappenavn>... -> mappenavnene sortert etter kategori, så navn (én per linje)
sbaudio_sort_plugins() {
  local n c i e
  for n in "$@"; do
    c=$(sbaudio_category "plugins/$n")
    i=0
    for e in "${SBAUDIO_CATEGORIES[@]}"; do [ "${e%%|*}" = "$c" ] && break; i=$((i + 1)); done
    printf '%02d\t%s\n' "$i" "$n"
  done | LC_ALL=C sort -f -t $'\t' -k1,1 -k2,2 | cut -f2
}
