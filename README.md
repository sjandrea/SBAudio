# SB Audio

Audio Unit- og VST3-plugins for Logic Pro.

| Plugin | Hva den gjør |
|---|---|
| [VoxPilot](plugins/VoxPilot/README.md) | Automatisk vokal-rider |

## Bygge på Mac

Krav: macOS 11+, Xcode Command Line Tools (`xcode-select --install`) og CMake (`brew install cmake`).

```bash
scripts/build.sh VoxPilot      # én plugin
scripts/build.sh --all         # alle
```

Skriptet laster ned JUCE 8 automatisk (første gang), bygger universelle AU/VST3 (Apple Silicon + Intel),
installerer i `~/Library/Audio/Plug-Ins/`, ad-hoc-signerer, kjører `auval` og legger kopier i `dist/`.
Start Logic på nytt etterpå.

### GitHub

Workflowen `.github/workflows/build-macos.yml` bygger og validerer alle plugins ved hver push og legger dem ved
som nedlastbar artefakt. Etter nedlasting: `xattr -dr com.apple.quarantine <Plugin>.component` før du kopierer
den til `~/Library/Audio/Plug-Ins/Components/`.

JUCE er lisensiert under AGPLv3 / JUCE-lisens (gratis for personlig bruk under omsetningsgrensen).
