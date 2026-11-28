#pragma once

#include "RepairDsp.h"

// NB: parameter IDs must never change once the plugin is in use (Logic projects/automation).
namespace ParamIDs
{
    // global
    inline constexpr const char* out   = "out";
    inline constexpr const char* mix   = "mix";
    inline constexpr const char* delta = "delta";

    // de-clip
    inline constexpr const char* dcOn = "dc_on", *dcMode = "dc_mode", *dcThr = "dc_thr", *dcGain = "dc_gain";
    // de-click / de-crackle / mouth de-click
    inline constexpr const char* ckOn = "ck_on", *ckSens = "ck_sens", *ckWidth = "ck_width";
    inline constexpr const char* crOn = "cr_on", *crAmt = "cr_amt";
    inline constexpr const char* mcOn = "mc_on", *mcSens = "mc_sens", *mcWidth = "mc_width";
    // de-hum
    inline constexpr const char* hmOn = "hm_on", *hmBase = "hm_base", *hmHarm = "hm_harm", *hmQ = "hm_q",
                                 *hmDepth = "hm_depth", *hmSlope = "hm_slope", *hmHpf = "hm_hpf";
    // de-plosive
    inline constexpr const char* plOn = "pl_on", *plSens = "pl_sens", *plFreq = "pl_freq", *plMax = "pl_max";
    // de-bleed
    inline constexpr const char* blOn = "bl_on", *blAmt = "bl_amt", *blMax = "bl_max";
    // de-noise
    inline constexpr const char* dnOn = "dn_on", *dnSrc = "dn_src", *dnType = "dn_type", *dnRed = "dn_red",
                                 *dnThr = "dn_thr", *dnSmooth = "dn_smooth";
    // dialogue isolate
    inline constexpr const char* isOn = "is_on", *isAmt = "is_amt", *isBg = "is_bg";
    // de-reverb
    inline constexpr const char* rvOn = "rv_on", *rvAmt = "rv_amt", *rvTail = "rv_tail";
    // breath control
    inline constexpr const char* brOn = "br_on", *brSens = "br_sens", *brGain = "br_gain";
    // de-ess
    inline constexpr const char* dsOn = "ds_on", *dsMode = "ds_mode", *dsThr = "ds_thr", *dsFreq = "ds_freq", *dsMax = "ds_max";
    // rebalance
    inline constexpr const char* rbOn = "rb_on", *rbVox = "rb_vox", *rbBass = "rb_bass", *rbPerc = "rb_perc",
                                 *rbOther = "rb_other", *rbSep = "rb_sep";
}

namespace renske
{
    /** One module in the chain, in processing order. 'events' = activity is counted repairs (shown per second),
        otherwise activity is reduction in dB. */
    struct ModuleInfo
    {
        int index;               // Shared::Module
        const char* name;        // shown in the chain
        const char* stage;       // "TID" (time domain) or "SPEKTRAL"
        const char* help;        // UTF-8 description
        const char* onId;
        std::vector<const char*> params;
        bool events;
    };

    inline const std::vector<ModuleInfo>& modules()
    {
        using namespace ParamIDs;
        static const std::vector<ModuleInfo> m {
            { Shared::mDeclip, "De-clip", "TID",
              "Tegner opp igjen bølgetopper som er kuttet av (digital klipping eller overstyrt forforsterker). "
              "AUTO finner klippenivået selv; MANUELL bruker terskelen. Gain senker nivået etterpå så de gjenskapte toppene får plass.",
              dcOn, { dcMode, dcThr, dcGain }, true },
            { Shared::mClick, "De-click", "TID",
              "Finner korte klikk og smell (digitale feil, redigeringsklikk, vinylklikk) og fyller dem inn med lyd beregnet fra omgivelsene.",
              ckOn, { ckSens, ckWidth }, true },
            { Shared::mCrackle, "De-crackle", "TID",
              "Fjerner tett, svak knitring (vinyl, dårlige kabler, forvrengning fra gamle opptak). Mange små reparasjoner per sekund.",
              crOn, { crAmt }, true },
            { Shared::mMouth, "Munnklikk", "TID",
              "Fjerner smatting, tungeklikk og spytt i tale og vokal. Lytter etter korte, lyse klikk.",
              mcOn, { mcSens, mcWidth }, true },
            { Shared::mHum, "De-hum", "TID",
              "Fjerner brumm fra strømnettet (50 Hz i Europa, 60 Hz i USA) og overtonene. AUTO finner grunntonen og følger den.",
              hmOn, { hmBase, hmHarm, hmQ, hmDepth, hmSlope, hmHpf }, false },
            { Shared::mPlosive, "De-plosive", "TID",
              "Demper popp fra P og B og vindstøt i mikrofonen. Bare bassen under delefrekvensen dempes, og bare når den plutselig tar over.",
              plOn, { plSens, plFreq, plMax }, false },
            { Shared::mBleed, "De-bleed", "SPEKTRAL",
              "Fjerner lekkasje fra en annen kilde (f.eks. klikkspor i vokalmikrofonen). Krever sidechain: send kilden som lekker inn "
              "til pluginens sidechain-inngang i Logic.",
              blOn, { blAmt, blMax }, false },
            { Shared::mDenoise, "De-noise", "SPEKTRAL",
              "Fjerner sus, vifte- og romstøy. ADAPTIV følger støyen selv; LÆRT PROFIL bruker et støyavtrykk du lærer inn fra et parti med bare støy.",
              dnOn, { dnSrc, dnType, dnRed, dnThr, dnSmooth }, false },
            { Shared::mIsolate, "Dialog-isolering", "SPEKTRAL",
              "Holder på stemmen og skrur ned alt annet (trafikk, publikum, rom). Klassisk signalbehandling – best når stemmen er tydelig over bakgrunnen.",
              isOn, { isAmt, isBg }, false },
            { Shared::mReverb, "De-reverb", "SPEKTRAL",
              "Demper romklang i opptaket. Etterklang-tiden bør omtrent tilsvare rommet (kort for små rom, lang for kirker og haller).",
              rvOn, { rvAmt, rvTail }, false },
            { Shared::mBreath, "Pustekontroll", "SPEKTRAL",
              "Finner innpust mellom fraser i vokal og tale og senker dem med valgt mengde.",
              brOn, { brSens, brGain }, false },
            { Shared::mEss, "De-ess", "SPEKTRAL",
              "Demper skarpe S-, T- og SJ-lyder. SPEKTRAL demper bare de mest skarpe frekvensene, KLASSISK hele området over frekvensen.",
              dsOn, { dsMode, dsThr, dsFreq, dsMax }, false },
            { Shared::mRebal, "Rebalanse", "SPEKTRAL",
              "Endrer balansen i en ferdig miks: vokal, bass, perkusjon og resten hver for seg. Vokal finnes i midten av stereobildet.",
              rbOn, { rbVox, rbBass, rbPerc, rbOther, rbSep }, false },
        };
        return m;
    }
}
