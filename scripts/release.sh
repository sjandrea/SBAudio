#!/bin/bash
# Lager en ny versjon av installasjonsfilen: 1.x.0 -> 1.(x+1).0.
# Oppdaterer VERSION, committer, tagger og pusher. GitHub bygger så alle plugins
# og legger SLOWHAND-<versjon>.dmg ut som release (se .github/workflows/build-macos.yml).
#
#   scripts/release.sh          # f.eks. 1.2.0 -> 1.3.0
#
# GitHub bygger fra taggen, så ucommittede endringer i arbeidsmappen kommer ikke med.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

git diff --quiet -- VERSION || { echo "VERSION har ucommittede endringer."; exit 1; }
git fetch -q origin --tags
[ "$(git rev-parse HEAD)" = "$(git rev-parse '@{u}')" ] || { echo "main er ikke lik origin/main. Pull/push først."; exit 1; }

OLD="$(cat VERSION)"
IFS=. read -r MAJOR MINOR _ <<< "$OLD"
# Ta høyde for tagger som er nyere enn VERSION
for t in $(git tag -l "$MAJOR.*.0"); do
  m=$(echo "$t" | cut -d. -f2); [ "$m" -gt "$MINOR" ] 2>/dev/null && MINOR=$m
done
NEW="$MAJOR.$((MINOR + 1)).0"

echo "$NEW" > VERSION
git commit -q -m "Installasjonsfil $NEW" -- VERSION
git tag -a "$NEW" -m "SLOWHAND $NEW"
git push -q origin HEAD "$NEW"
echo "Versjon $NEW er tagget og pushet. GitHub bygger og publiserer releasen (ca. 30–40 min)."
