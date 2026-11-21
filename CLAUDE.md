# SB Audio – plugin-samling

Audio-plugins (AU + VST3) for Logic Pro, bygget med JUCE 8 og CMake. Brukeren skriver norsk; svar på norsk.
README-er og brukertekster skrives på norsk, kode og kommentarer i koden kan være på engelsk.

## Struktur

- `CMakeLists.txt` – rot: henter JUCE én gang, legger til `shared/` og alle `plugins/*` automatisk.
- `cmake/SBAudio.cmake` – felles verdier (`SBAUDIO_COMPANY`, `SBAUDIO_MANUFACTURER`, `SBAUDIO_FORMATS`)
  og `sbaudio_configure_plugin(<target>)` med felles defines og lenking.
- `shared/sbaudio_common/` – JUCE-modul med felles kode (farger, `sbaudio::LookAndFeel`, `sbaudio::uiFont`,
  `sbaudio::PresetManager` + `sbaudio::PresetBar` for fabrikk-/brukerpresets, `sbaudio::Knob`,
  `sbaudio::AnalyzerRing` for spektrumanalysatorer). Lenkes inn i alle plugins.
  Kode som brukes av to eller flere plugins flyttes hit.
- `plugins/<Navn>/` – én mappe per plugin: `CMakeLists.txt`, `Source/`, `README.md`, `docs/`.
  Mapper som starter med `_` ignoreres av bygget.
- `scripts/build.sh <Navn>|--all [Release|Debug]` – bygg, installer, signer, `auval`, kopier til `dist/`.
- `scripts/make-installer.sh [versjon]` – lager `dist/SBAudio-<versjon>.pkg` av det som ligger i `dist/`.

## Faste regler

- Produsent: **SB Audio**, manufacturer code **`Sbau`** (aldri endre – Logic identifiserer plugins med den).
- Bundle-ID: `com.sbaudio.<navn-med-små-bokstaver>`.
- Universal binary (arm64 + x86_64), macOS 11+, C++17.
- `PLUGIN_CODE` er 4 tegn, minst én stor bokstav, og **må være unik** – sjekk og oppdater tabellen under.
- En plugin er ikke ferdig før `auval` passerer (`scripts/build.sh <Navn>`).
- Ikke endre parameter-ID-er i en plugin som er tatt i bruk – det ødelegger lagrede Logic-prosjekter og automasjon.

## Plugin-register

| Plugin   | PLUGIN_CODE | AU-type | Beskrivelse                  |
|----------|-------------|---------|------------------------------|
| VoxPilot | `Vxpl`      | aufx    | Automatisk vokal-rider       |
| PrismEQ  | `Prsm`      | aufx    | Dynamisk parametrisk EQ      |
| OptoTube | `Optb`      | aufx    | Opto/rør-kompressor (LA-2A + CL 1B) |
| Ember    | `Embr`      | aufx    | Multiband-metning (à la Saturn) |
| Aura     | `Aura`      | aufx    | Reverb for vokal og akustisk gitar |
| Foss     | `Foss`      | aufx    | Smart EQ: Learn + profil (à la smart:EQ) og adaptiv Recover/Tame (à la Gullfoss) |

## Ny plugin

1. Kopier `plugins/VoxPilot/CMakeLists.txt` til `plugins/<Navn>/` og endre navn, bundle-ID, `PLUGIN_CODE`,
   beskrivelse, `AU_MAIN_TYPE` (`kAudioUnitType_Effect` / `MusicDevice` for instrument) og `VST3_CATEGORIES`.
2. Lag `Source/PluginProcessor.*` og `Source/PluginEditor.*`. Bruk `sbaudio::colours` og felles komponenter fra
   `sbaudio_common` så pluginene ser like ut. Presets: `sbaudio::PresetManager` (fabrikk + egne i
   `~/Library/Audio/Presets/SB Audio/<Navn>`) og `sbaudio::PresetBar` – se OptoTube for et enkelt eksempel.
3. Legg pluginen inn i registeret over og lag en `README.md` (samme oppbygging som VoxPilot).
4. Bygg med `scripts/build.sh <Navn>` og sørg for at `auval` passerer.

## Bygging

Krav: Xcode Command Line Tools og CMake. Første konfigurering laster ned JUCE til `build/_deps`.
`build/` og `dist/` er git-ignorert.
