# SLOWHAND – plugin-samling

Audio-plugins (AU + VST3) for Logic Pro, bygget med JUCE 8 og CMake. Brukeren skriver norsk; svar på norsk.
README-er og brukertekster skrives på norsk, kode og kommentarer i koden kan være på engelsk.

## Struktur

- `CMakeLists.txt` – rot: henter JUCE én gang, legger til `shared/` og alle `plugins/*` automatisk.
- `cmake/SBAudio.cmake` – felles verdier (`SBAUDIO_COMPANY`, `SBAUDIO_MANUFACTURER`, `SBAUDIO_FORMATS`)
  og `sbaudio_configure_plugin(<target>)` med felles defines og lenking.
- `shared/sbaudio_common/` – JUCE-modul med felles kode (farger, `sbaudio::LookAndFeel`, `sbaudio::uiFont`,
  `sbaudio::PresetManager` + `sbaudio::PresetBar` for fabrikk-/brukerpresets, `sbaudio::Knob`,
  `sbaudio::drawBrandTitle` for tittelen øverst til venstre («SLOWHAND» over «Navn TYPE»),
  `sbaudio::AnalyzerRing` for spektrumanalysatorer, `aura::Engine` reverb i `dsp/sbaudio_ReverbEngine.h`). Lenkes inn i alle plugins.
  Kode som brukes av to eller flere plugins flyttes hit.
- `plugins/<Navn>/` – én mappe per plugin: `CMakeLists.txt`, `Source/`, `README.md`, `docs/`.
  Mapper som starter med `_` ignoreres av bygget.
- `scripts/build.sh <Navn>|--all [Release|Debug]` – bygg, installer, signer, `auval`, kopier til `dist/`.
- `scripts/make-installer.sh [versjon]` – lager `dist/SLOWHAND-<versjon>.pkg` av det som ligger i `dist/`.
- `scripts/make-installer-app.sh [versjon]` – lager `dist/SLOWHAND-<versjon>.dmg` med en grafisk installasjonsapp
  (`installer/SBAudioInstaller.swift`) som viser hver plugin som kort med bilde og beskrivelse.
  Nye plugins plukkes opp automatisk (alle `plugins/*` med bygg i `dist/`); `build.sh` lager dmg + pkg på nytt
  etter hvert bygg.
- `scripts/release.sh` – ny versjon av installasjonsfilen: øker `VERSION` fra 1.x.0 til 1.(x+1).0, committer,
  tagger og pusher. GitHub bygger og legger `SLOWHAND-<versjon>.dmg` ut som release.

## Faste regler

- Merkenavn: **SLOWHAND** (`SBAUDIO_COMPANY`, vises i Logic og i grensesnittet). Manufacturer code **`Sbau`**
  (aldri endre – Logic identifiserer plugins med den). Intern kode heter fortsatt `sbaudio`.
- Installasjonsfilen har eget versjonsnummer i `VERSION` (1.x.0). Hver gang installasjonsfilen endres og legges ut,
  økes x med `scripts/release.sh` – aldri gjenbruk et versjonsnummer.
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
| VoxHeat  | `Vxht`      | aufx    | Vokal-saturator (TG-konsoll + rør, à la Abbey Road Saturator) |
| Nordlys  | `Nrdl`      | aumu    | Atmosfærisk synth: 4 lag, granulær, Orb, arp (à la Omnisphere) |
| Domina   | `Glns`      | aufx    | Mastering: analyse, stil, loudness-mål, true-peak-limiter (à la LANDR). Het Glans før – koden er beholdt |
| Stressulf | `Strs`     | aufx    | Karakterkompressor med Dist 2/3, Nuke, Opto og Smash (à la Distressor) |

## Ny plugin

1. Kopier `plugins/VoxPilot/CMakeLists.txt` til `plugins/<Navn>/` og endre navn, bundle-ID, `PLUGIN_CODE`,
   beskrivelse, `AU_MAIN_TYPE` (`kAudioUnitType_Effect` / `MusicDevice` for instrument) og `VST3_CATEGORIES`.
2. Lag `Source/PluginProcessor.*` og `Source/PluginEditor.*`. Bruk `sbaudio::colours` og felles komponenter fra
   `sbaudio_common` så pluginene ser like ut. Tittelen i toppfeltet tegnes med
   `sbaudio::drawBrandTitle (g, område, "Navn", "TYPE", farge)` – type på engelsk, f.eks. REVERB, COMPRESSOR,
   SATURATION. Skriv aldri «SB Audio» i grensesnittet. Presets: `sbaudio::PresetManager` (fabrikk + egne i
   `~/Library/Audio/Presets/SB Audio/<Navn>`) og `sbaudio::PresetBar` – se OptoTube for et enkelt eksempel.
3. Legg pluginen inn i registeret over og lag en `README.md` (samme oppbygging som VoxPilot), `docs/screenshot.png`
   og `docs/installer.txt` (2–3 setninger som vises i installasjonsappen).
4. Bygg med `scripts/build.sh <Navn>` og sørg for at `auval` passerer.

## Bygging

Krav: Xcode Command Line Tools og CMake. Første konfigurering laster ned JUCE til `build/_deps`.
`build/` og `dist/` er git-ignorert.
