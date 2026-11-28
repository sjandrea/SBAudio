#include "PluginEditor.h"

using namespace juce;
namespace col = sbaudio::colours;
using sbaudio::uiFont;

namespace
{
    const Colour rCol  { 0xff7ee0a8 };   // fresh "clean" green
    const Colour rSoft { 0xffc4f2d6 };

    String u8 (const char* t) { return String (CharPointer_UTF8 (t)); }

    void drawWell (Graphics& g, Rectangle<float> r)
    {
        g.setColour (col::well);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (col::panelEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    }

    void drawPanel (Graphics& g, Rectangle<float> r)
    {
        g.setColour (col::panel);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (col::panelEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    }

    struct ParamText { const char* id; const char* caption; const char* tip; };
    const ParamText paramTexts[] = {
        { ParamIDs::dcMode,  "MODUS", "AUTO finner klippenivået ut fra de høyeste toppene. MANUELL bruker terskelen." },
        { ParamIDs::dcThr,   "TERSKEL", "Nivået der signalet regnes som klippet (bare i MANUELL)." },
        { ParamIDs::dcGain,  "GAIN", "Senker nivået etter reparasjonen så de gjenskapte toppene ikke klipper igjen." },
        { ParamIDs::ckSens,  "FØLSOMHET", "Høyere = flere klikk blir funnet. For høyt kan ta med anslag i trommer og konsonanter." },
        { ParamIDs::ckWidth, "BREDDE", "Hvor lange klikk som repareres. Øk for lengre smell, senk for skarpe digitale klikk." },
        { ParamIDs::crAmt,   "MENGDE", "Hvor mye knitring som fjernes." },
        { ParamIDs::mcSens,  "FØLSOMHET", "Høyere = flere munnlyder blir fjernet." },
        { ParamIDs::mcWidth, "BREDDE", "Hvor lange munnklikk som repareres." },
        { ParamIDs::hmBase,  "GRUNNTONE", "AUTO finner og følger brummen. Velg 50 Hz (Europa) eller 60 Hz (USA) for fast frekvens." },
        { ParamIDs::hmHarm,  "OVERTONER", "Hvor mange av brummens overtoner som fjernes (inkludert grunntonen)." },
        { ParamIDs::hmQ,     "Q", "Bredden på hvert filter. Høyere Q = smalere filter, mindre av musikken påvirkes." },
        { ParamIDs::hmDepth, "DYBDE", "Hvor mye hver brummtone dempes." },
        { ParamIDs::hmSlope, "DISKANT-SLAKK", "Demper de høyeste overtonene mindre. Bevarer mer av musikken i mellomtonen." },
        { ParamIDs::hmHpf,   "LAVKUTT", "Kutter også alt under grunntonen (bassbrumm og rumling)." },
        { ParamIDs::plSens,  "FØLSOMHET", "Høyere = mildere popp blir også dempet." },
        { ParamIDs::plFreq,  "FREKVENS", "Bare bass under denne frekvensen dempes. 100–200 Hz passer for de fleste stemmer." },
        { ParamIDs::plMax,   "MAKS DEMPING", "Den største dempingen av et popp." },
        { ParamIDs::blAmt,   "MENGDE", "Hvor mye av lekkasjen som trekkes fra." },
        { ParamIDs::blMax,   "MAKS DEMPING", "Den største dempingen per frekvens. Lavere verdi gir færre artefakter." },
        { ParamIDs::dnSrc,   "KILDE", "ADAPTIV følger støyen kontinuerlig. LÆRT PROFIL bruker avtrykket fra LÆR STØY." },
        { ParamIDs::dnType,  "TYPE", "GENERELL: lik behandling i hele spekteret. TALE: skåner stemmeområdet. GITAR: skåner kropp og overtoner." },
        { ParamIDs::dnRed,   "DEMPING", "Hvor mye støyen dempes. 6–12 dB er naturlig, mer kan gi en «undervanns»-lyd." },
        { ParamIDs::dnThr,   "TERSKEL", "Hvor mye over støygulvet lyden må være for å slippe gjennom. Høyere = mer fjernes." },
        { ParamIDs::dnSmooth,"GLATTING", "Glatter reduksjonen i tid og frekvens. Mot «kvitrende» artefakter." },
        { ParamIDs::isAmt,   "SEPARASJON", "Hvor strengt stemmen skilles fra bakgrunnen." },
        { ParamIDs::isBg,    "BAKGRUNN", "Hvor mye av bakgrunnen som blir igjen." },
        { ParamIDs::rvAmt,   "MENGDE", "Hvor mye romklang som fjernes." },
        { ParamIDs::rvTail,  "ETTERKLANG", "Omtrent hvor lang romklangen i opptaket er (RT60)." },
        { ParamIDs::brSens,  "FØLSOMHET", "Høyere = flere og kraftigere innpust blir funnet." },
        { ParamIDs::brGain,  "NIVÅ", "Hvor mye pusten senkes." },
        { ParamIDs::dsMode,  "MODUS", "SPEKTRAL demper bare de skarpeste frekvensene. KLASSISK demper hele området over frekvensen." },
        { ParamIDs::dsThr,   "TERSKEL", "Nivået der de-esseren begynner å dempe." },
        { ParamIDs::dsFreq,  "FREKVENS", "Nedre grense for sibilantene. 5–7 kHz for de fleste stemmer, lavere for mørke stemmer." },
        { ParamIDs::dsMax,   "MAKS DEMPING", "Den største dempingen av en S-lyd." },
        { ParamIDs::rbVox,   "VOKAL", "Nivå på vokal (det som ligger i midten og er tonalt)." },
        { ParamIDs::rbBass,  "BASS", "Nivå på bass og lave toner." },
        { ParamIDs::rbPerc,  "PERKUSJON", "Nivå på trommer og anslag." },
        { ParamIDs::rbOther, "RESTEN", "Nivå på alt annet (instrumenter på sidene, pads)." },
        { ParamIDs::rbSep,   "SEPARASJON", "Hvor strengt lydene skilles. Høyere = tydeligere skille, men flere artefakter." },
    };

    const ParamText* textFor (const char* id)
    {
        for (auto& t : paramTexts) if (String (t.id) == id) return &t;
        return nullptr;
    }

    bool isBipolar (const String& id)
    {
        return id == ParamIDs::dnThr || (id.startsWith ("rb_") && id != ParamIDs::rbSep);
    }

    Colour heat (float t)
    {
        t = jlimit (0.0f, 1.0f, t);
        struct Stop { float p; Colour c; };
        static const Stop stops[] = { { 0.0f, Colour (0xff0b0d10) }, { 0.30f, Colour (0xff1d3b5a) }, { 0.55f, Colour (0xff2fa39a) },
                                      { 0.78f, Colour (0xffe8c15a) }, { 1.0f, Colour (0xffffffff) } };
        for (int i = 1; i < 5; ++i)
            if (t <= stops[i].p)
                return stops[i - 1].c.interpolatedWith (stops[i].c, (t - stops[i - 1].p) / (stops[i].p - stops[i - 1].p));
        return stops[4].c;
    }
}

namespace renskeui
{
    //==========================================================================
    ChainRow::ChainRow (RenskeProcessor& p, const renske::ModuleInfo& m) : info (m)
    {
        power.setClickingTogglesState (true);
        power.setColour (TextButton::buttonOnColourId, rCol);
        power.setColour (TextButton::textColourOnId, col::bg);
        power.setButtonText ("");
        power.setTooltip (u8 ("Slå modulen av eller på"));
        addAndMakeVisible (power);
        att = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, m.onId, power);
        power.onStateChange = [this] { if (power.getToggleState() != on) { on = power.getToggleState(); repaint(); } };
        on = power.getToggleState();
    }

    void ChainRow::resized()
    {
        power.setBounds (8, (getHeight() - 16) / 2, 16, 16);
    }

    void ChainRow::mouseUp (const MouseEvent& e)
    {
        if (e.mouseWasClicked() && onSelect) onSelect();
    }

    void ChainRow::setSelected (bool s) { if (s != selected) { selected = s; repaint(); } }

    void ChainRow::setActivity (float v, bool isOn)
    {
        if (std::abs (v - activity) > 0.05f || isOn != on) { activity = v; on = isOn; repaint(); }
    }

    void ChainRow::paint (Graphics& g)
    {
        auto r = getLocalBounds().toFloat().reduced (0.0f, 1.0f);
        if (selected)
        {
            g.setColour (rCol.withAlpha (0.12f));
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (rCol.withAlpha (0.6f));
            g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
        }

        auto text = r.withTrimmedLeft (32.0f);
        auto actR = text.removeFromRight (62.0f);
        g.setColour (on ? col::text : col::textDim);
        g.setFont (uiFont (12.0f, true));
        g.drawText (u8 (info.name), text, Justification::centredLeft);

        if (! on) return;
        String v;
        float t = 0.0f;
        if (info.events)
        {
            v = activity < 0.05f ? String ("0/s") : (activity < 10.0f ? String (activity, 1) : String (roundToInt (activity))) + "/s";
            t = jlimit (0.0f, 1.0f, std::log10 (1.0f + activity) / 2.0f);
        }
        else
        {
            v = info.index == renske::Shared::mRebal ? String ("aktiv") : activity < 0.05f ? String ("0.0 dB") : "-" + String (activity, 1) + " dB";
            t = jlimit (0.0f, 1.0f, activity / 24.0f);
        }
        auto bar = actR.removeFromBottom (4.0f).reduced (4.0f, 0.0f);
        g.setColour (col::well);
        g.fillRoundedRectangle (bar, 2.0f);
        g.setColour (rCol);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * t), 2.0f);
        g.setColour (rSoft);
        g.setFont (uiFont (10.0f, true));
        g.drawText (v, actR.reduced (4.0f, 0.0f), Justification::centredRight);
    }

    //==========================================================================
    Spectrogram::Spectrogram()
    {
        const char* names[] = { "INN", "UT", "FJERNET" };
        const char* tips[] = { "Viser lyden før behandling", "Viser lyden etter behandling", "Viser bare det som blir fjernet" };
        for (int i = 0; i < 3; ++i)
        {
            auto& b = modeBtns[i];
            b.setButtonText (names[i]);
            b.setTooltip (u8 (tips[i]));
            b.setColour (TextButton::buttonOnColourId, rCol);
            b.setColour (TextButton::textColourOnId, col::bg);
            b.onClick = [this, i]
            {
                mode = i;
                for (int j = 0; j < 3; ++j) modeBtns[j].setToggleState (j == mode, dontSendNotification);
                img.clear (img.getBounds(), col::well);
                repaint();
            };
            addAndMakeVisible (b);
        }
        modeBtns[mode].setToggleState (true, dontSendNotification);
        img.clear (img.getBounds(), col::well);
    }

    Rectangle<int> Spectrogram::plotArea() const
    {
        return getLocalBounds().reduced (12).withTrimmedTop (26).withTrimmedLeft (36);
    }

    void Spectrogram::resized()
    {
        auto r = getLocalBounds().reduced (12, 8).removeFromTop (20);
        for (int i = 2; i >= 0; --i)
        {
            modeBtns[i].setBounds (r.removeFromRight (i == 2 ? 74 : 50));
            r.removeFromRight (4);
        }
    }

    void Spectrogram::push (const std::array<float, renske::Shared::numBands>& in,
                            const std::array<float, renske::Shared::numBands>& out)
    {
        Image::BitmapData bd (img, writeX, 0, 1, img.getHeight(), Image::BitmapData::writeOnly);
        const int nb = renske::Shared::numBands;
        for (int b = 0; b < nb; ++b)
        {
            float db = in[(size_t) b];
            if (mode == 1) db = out[(size_t) b];
            else if (mode == 2)
            {
                const float pi = std::pow (10.0f, in[(size_t) b] * 0.1f), po = std::pow (10.0f, out[(size_t) b] * 0.1f);
                db = 10.0f * std::log10 (jmax (1.0e-20f, pi - po));
            }
            bd.setPixelColour (0, nb - 1 - b, heat ((db + 105.0f) / 95.0f));
        }
        writeX = (writeX + 1) % numCols;
        repaint (plotArea());
    }

    void Spectrogram::paint (Graphics& g)
    {
        auto full = getLocalBounds().toFloat();
        drawWell (g, full);
        g.setColour (col::text);
        g.setFont (uiFont (11.0f, true));
        g.drawText ("SPEKTROGRAM", full.reduced (12.0f, 8.0f).removeFromTop (20.0f), Justification::centredLeft);

        const auto p = plotArea();
        g.setImageResamplingQuality (Graphics::mediumResamplingQuality);
        // oldest column (writeX) on the left
        const float colW = (float) p.getWidth() / (float) numCols;
        const int firstW = numCols - writeX;
        g.drawImage (img, p.getX(), p.getY(), roundToInt ((float) firstW * colW), p.getHeight(), writeX, 0, firstW, img.getHeight());
        if (writeX > 0)
            g.drawImage (img, p.getX() + roundToInt ((float) firstW * colW), p.getY(), p.getWidth() - roundToInt ((float) firstW * colW), p.getHeight(),
                         0, 0, writeX, img.getHeight());

        // frequency labels
        g.setFont (uiFont (9.5f));
        for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
        {
            const float t = std::log (f / 30.0f) / std::log (20000.0f / 30.0f);
            const float y = (float) p.getBottom() - t * (float) p.getHeight();
            g.setColour (col::panelEdge.withAlpha (0.5f));
            g.drawHorizontalLine (roundToInt (y), (float) p.getX(), (float) p.getX() + 4.0f);
            g.setColour (col::textDim);
            g.drawText (f < 1000.0f ? String (roundToInt (f)) : String (roundToInt (f / 1000.0f)) + "k",
                        Rectangle<float> ((float) p.getX() - 34.0f, y - 6.0f, 30.0f, 12.0f), Justification::centredRight);
        }
    }

    //==========================================================================
    ModulePanel::ModulePanel (RenskeProcessor& p) : proc (p)
    {
        power.setClickingTogglesState (true);
        power.setColour (TextButton::buttonOnColourId, rCol);
        power.setColour (TextButton::textColourOnId, col::bg);
        power.onStateChange = [this] { power.setButtonText (power.getToggleState() ? "PÅ" : "AV"); };
        addAndMakeVisible (power);

        learnBtn.setColour (TextButton::buttonOnColourId, col::amber);
        learnBtn.setColour (TextButton::textColourOnId, col::bg);
        learnBtn.setTooltip (u8 ("Spill av et parti med bare støy (ingen tale eller musikk), trykk LÆR STØY, og trykk igjen etter "
                                 "1–3 sekunder. Profilen lagres i prosjektet."));
        learnBtn.onClick = [this]
        {
            if (proc.shared.learning.load())
            {
                proc.finishLearn();
                if (auto* prm = proc.apvts.getParameter (ParamIDs::dnSrc))
                {
                    prm->beginChangeGesture(); prm->setValueNotifyingHost (1.0f); prm->endChangeGesture();
                }
                if (auto* prm = proc.apvts.getParameter (ParamIDs::dnOn))
                {
                    prm->beginChangeGesture(); prm->setValueNotifyingHost (1.0f); prm->endChangeGesture();
                }
            }
            else proc.startLearn();
        };
        addChildComponent (learnBtn);
        startTimerHz (10);
    }

    ModulePanel::~ModulePanel() { stopTimer(); }

    void ModulePanel::show (int moduleIndex)
    {
        sliderAtts.clear(); comboAtts.clear(); buttonAtts.clear();
        knobs.clear(); combos.clear(); toggles.clear(); captions.clear();
        powerAtt.reset();

        info = nullptr;
        for (auto& m : renske::modules()) if (m.index == moduleIndex) info = &m;
        if (info == nullptr) return;

        powerAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, info->onId, power);
        power.setButtonText (power.getToggleState() ? u8 ("PÅ") : String ("AV"));

        for (auto* id : info->params)
        {
            auto* prm = proc.apvts.getParameter (id);
            if (prm == nullptr) continue;
            const auto* t = textFor (id);
            const String caption = t ? u8 (t->caption) : prm->getName (20).toUpperCase();
            const String tip = t ? u8 (t->tip) : String();

            if (dynamic_cast<AudioParameterChoice*> (prm) != nullptr)
            {
                auto* cb = combos.add (new ComboBox());
                cb->addItemList (prm->getAllValueStrings(), 1);
                cb->setTooltip (tip);
                cb->setName (caption);
                addAndMakeVisible (cb);
                comboAtts.push_back (std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, *cb));
                auto* l = captions.add (new Label (String(), caption));
                l->setFont (uiFont (10.0f, true));
                l->setColour (Label::textColourId, col::textDim);
                l->setComponentID (String ("c") + String (combos.size() - 1));
                addAndMakeVisible (l);
            }
            else if (dynamic_cast<AudioParameterBool*> (prm) != nullptr)
            {
                auto* b = toggles.add (new TextButton (caption));
                b->setClickingTogglesState (true);
                b->setColour (TextButton::buttonOnColourId, rCol);
                b->setColour (TextButton::textColourOnId, col::bg);
                b->setTooltip (tip);
                addAndMakeVisible (b);
                buttonAtts.push_back (std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, *b));
            }
            else
            {
                auto* k = knobs.add (new sbaudio::Knob (caption));
                k->slider.setTooltip (tip);
                k->slider.setColour (Slider::rotarySliderFillColourId, rCol);
                if (isBipolar (id)) k->slider.getProperties().set ("bipolar", true);
                addAndMakeVisible (k);
                sliderAtts.push_back (std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k->slider));
                k->slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
            }
        }

        learnBtn.setVisible (moduleIndex == renske::Shared::mDenoise);
        updateLearnButton();
        resized();
        repaint();
    }

    void ModulePanel::resized()
    {
        auto r = getLocalBounds().reduced (16, 12);
        auto head = r.removeFromTop (26);
        power.setBounds (head.removeFromRight (56).reduced (0, 2));
        r.removeFromTop (52);   // help text

        // row of choices / toggles / learn
        if (combos.size() > 0 || toggles.size() > 0 || learnBtn.isVisible())
        {
            auto row = r.removeFromTop (44);
            auto capRow = row.removeFromTop (14);
            for (int i = 0; i < combos.size(); ++i)
            {
                combos[i]->setBounds (row.removeFromLeft (150).withHeight (26));
                row.removeFromLeft (12);
            }
            // captions above their combo
            for (int i = 0; i < combos.size(); ++i)
                for (auto* l : captions)
                    if (l->getComponentID() == "c" + String (i))
                        l->setBounds (combos[i]->getX(), capRow.getY(), 150, 14);
            for (auto* t : toggles)
            {
                t->setBounds (row.removeFromLeft (110).withHeight (26));
                row.removeFromLeft (12);
            }
            if (learnBtn.isVisible())
                learnBtn.setBounds (row.removeFromLeft (150).withHeight (26));
            r.removeFromTop (8);
        }

        // knobs
        if (knobs.size() > 0)
        {
            auto kr = r.removeFromTop (jmin (r.getHeight(), 128));
            const int w = jmin (112, kr.getWidth() / knobs.size());
            for (auto* k : knobs) k->setBounds (kr.removeFromLeft (w).reduced (4, 0));
        }
    }

    void ModulePanel::paint (Graphics& g)
    {
        auto full = getLocalBounds().toFloat();
        drawPanel (g, full);
        if (info == nullptr) return;

        auto r = full.reduced (16.0f, 12.0f);
        auto head = r.removeFromTop (26.0f);
        g.setColour (rCol);
        g.setFont (uiFont (17.0f, true));
        const String name = u8 (info->name).toUpperCase();
        g.drawText (name, head, Justification::centredLeft);
        const float nw = GlyphArrangement::getStringWidth (uiFont (17.0f, true), name);
        g.setColour (col::textDim);
        g.setFont (uiFont (10.0f, true));
        g.drawText (String (info->stage) + "-TRINN", head.withTrimmedLeft (nw + 10.0f), Justification::centredLeft);

        auto help = r.removeFromTop (52.0f).reduced (0.0f, 4.0f);
        g.setColour (col::text.withAlpha (0.8f));
        g.setFont (uiFont (11.5f));
        g.drawFittedText (u8 (info->help), help.toNearestInt(), Justification::topLeft, 3, 1.0f);

        if (status.isNotEmpty())
        {
            g.setColour (rSoft);
            g.setFont (uiFont (10.5f, true));
            g.drawText (status, full.reduced (16.0f, 10.0f).removeFromBottom (16.0f), Justification::centredLeft);
        }
    }

    void ModulePanel::updateLearnButton()
    {
        const bool l = proc.shared.learning.load();
        learnBtn.setToggleState (l, dontSendNotification);
        learnBtn.setButtonText (l ? u8 ("STOPP  ") + String (proc.shared.learnSeconds.load(), 1) + " s" : u8 ("LÆR STØY"));
    }

    void ModulePanel::timerCallback()
    {
        if (info == nullptr) return;
        String s;
        switch (info->index)
        {
            case renske::Shared::mDenoise:
                updateLearnButton();
                s = proc.shared.learning.load() ? u8 ("Lærer støyprofil … spill av et parti med bare støy.")
                  : proc.hasNoiseProfile() ? u8 ("Støyprofil lært. Brukes når KILDE er LÆRT PROFIL.")
                  : u8 ("Ingen støyprofil lært ennå (LÆRT PROFIL faller tilbake til adaptiv).");
                break;
            case renske::Shared::mHum:
                s = u8 ("Grunntone i bruk: ") + String (proc.shared.humFreq.load(), 2) + " Hz";
                break;
            case renske::Shared::mBleed:
                s = proc.sidechainActive.load() ? u8 ("Sidechain: signal mottas.")
                                                : u8 ("Sidechain: ingen signal. Velg kilden i sidechain-menyen øverst i plugin-vinduet i Logic.");
                break;
            default: break;
        }
        s += (s.isNotEmpty() ? u8 ("   ·   ") : String()) + u8 ("Total forsinkelse: ")
             + String (roundToInt (proc.getTotalLatency() * 1000.0 / jmax (1.0, proc.getSampleRate()))) + u8 (" ms (kompenseres av Logic)");
        if (s != status) { status = s; repaint(); }
    }

    //==========================================================================
    void Meters::setLevels (float i, float o)
    {
        in = jmax (i, in - 1.5f);
        out = jmax (o, out - 1.5f);
        if (i >= inHold) { inHold = i; inHoldFrames = 45; } else if (--inHoldFrames <= 0) inHold = jmax (i, inHold - 0.5f);
        if (o >= outHold) { outHold = o; outHoldFrames = 45; } else if (--outHoldFrames <= 0) outHold = jmax (o, outHold - 0.5f);
        repaint();
    }

    void Meters::paint (Graphics& g)
    {
        auto r = getLocalBounds().toFloat();
        const float w = (r.getWidth() - 8.0f) / 2.0f;
        auto bar = [&] (Rectangle<float> b, const String& label, float db, float hold)
        {
            auto labelR = b.removeFromBottom (14.0f);
            auto valueR = b.removeFromTop (14.0f);
            b.removeFromTop (2.0f);
            g.setColour (col::well);
            g.fillRoundedRectangle (b, 2.0f);
            g.setColour (col::panelEdge);
            g.drawRoundedRectangle (b.reduced (0.5f), 2.0f, 1.0f);
            auto lvl = [] (float d) { return jlimit (0.0f, 1.0f, (d + 60.0f) / 60.0f); };
            auto fill = b.withTop (b.getBottom() - b.getHeight() * lvl (db));
            g.setGradientFill (ColourGradient (rCol, 0.0f, b.getBottom(), hold > -0.5f ? col::red : rSoft, 0.0f, b.getY(), false));
            g.fillRoundedRectangle (fill.reduced (1.0f, 0.0f), 2.0f);
            const float hy = b.getBottom() - b.getHeight() * lvl (hold);
            g.setColour (hold > -0.3f ? col::red : col::text);
            g.fillRect (b.getX() + 1.0f, hy - 1.0f, b.getWidth() - 2.0f, 2.0f);
            g.setColour (col::textDim);
            g.setFont (uiFont (9.5f, true));
            g.drawText (label, labelR, Justification::centred);
            g.setColour (col::text);
            g.drawText (hold <= -99.0f ? String ("-inf") : String (hold, 1), valueR, Justification::centred);
        };
        bar (r.removeFromLeft (w), "INN", in, inHold);
        r.removeFromLeft (8.0f);
        bar (r, "UT", out, outHold);
    }

    //==========================================================================
    Assistant::Assistant (RenskeProcessor& p) : proc (p)
    {
        analyseBtn.setColour (TextButton::buttonOnColourId, col::amber);
        analyseBtn.setColour (TextButton::textColourOnId, col::bg);
        analyseBtn.setTooltip (u8 ("Spill av et typisk parti (10 sekunder) mens assistenten lytter. Den foreslår så en kjede av moduler."));
        analyseBtn.onClick = [this]
        {
            if (proc.isAnalysing()) { proc.stopAnalysis(); showResults(); }
            else { suggestions.clear(); results.setText (u8 ("Lytter … spill av lyden nå."), false); applyBtn.setEnabled (false); proc.startAnalysis(); }
        };
        addAndMakeVisible (analyseBtn);

        applyBtn.setColour (TextButton::buttonColourId, rCol.withAlpha (0.25f));
        applyBtn.setTooltip (u8 ("Slår på de foreslåtte modulene med forslagene, og slår av resten."));
        applyBtn.setEnabled (false);
        applyBtn.onClick = [this] { proc.applySuggestions (suggestions); };
        addAndMakeVisible (applyBtn);

        results.setMultiLine (true, true);
        results.setReadOnly (true);
        results.setScrollbarsShown (true);
        results.setCaretVisible (false);
        results.setFont (uiFont (11.0f));
        results.setColour (TextEditor::backgroundColourId, col::well);
        results.setColour (TextEditor::outlineColourId, col::panelEdge);
        results.setColour (TextEditor::focusedOutlineColourId, col::panelEdge);
        results.setColour (TextEditor::textColourId, col::text);
        results.setText (u8 ("Trykk ANALYSER og spill av lyden. Assistenten ser etter klipping, klikk, brumm, popp, støy, "
                             "romklang, pust og S-lyder, og foreslår innstillinger."), false);
        addAndMakeVisible (results);
    }

    void Assistant::resized()
    {
        auto r = getLocalBounds().reduced (12, 10);
        r.removeFromTop (20);
        auto row = r.removeFromTop (26);
        analyseBtn.setBounds (row.removeFromLeft ((row.getWidth() - 8) / 2));
        row.removeFromLeft (8);
        applyBtn.setBounds (row);
        r.removeFromTop (12);
        results.setBounds (r);
    }

    void Assistant::paint (Graphics& g)
    {
        auto full = getLocalBounds().toFloat();
        drawPanel (g, full);
        auto head = full.reduced (12.0f, 10.0f).removeFromTop (16.0f);
        g.setColour (col::amber);
        g.setFont (uiFont (11.0f, true));
        g.drawText ("ASSISTENT", head, Justification::centredLeft);
        if (proc.isAnalysing())
        {
            auto bar = Rectangle<float> ((float) analyseBtn.getX(), (float) analyseBtn.getBottom() + 4.0f,
                                         (float) (applyBtn.getRight() - analyseBtn.getX()), 4.0f);
            g.setColour (col::well);
            g.fillRoundedRectangle (bar, 2.0f);
            g.setColour (col::amber);
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * proc.analysisProgress()), 2.0f);
        }
    }

    void Assistant::showResults()
    {
        suggestions = proc.makeSuggestions();
        String t;
        bool any = false;
        for (auto& s : suggestions)
        {
            t << u8 ("• ") << s.text << "\n";
            any = any || ! s.values.empty();
        }
        results.setText (t.trimEnd(), false);
        applyBtn.setEnabled (any);
    }

    void Assistant::tick()
    {
        const bool a = proc.isAnalysing();
        analyseBtn.setToggleState (a, dontSendNotification);
        analyseBtn.setButtonText (a ? String ("STOPP") : String ("ANALYSER"));
        if (wasAnalysing && ! a && suggestions.empty()) showResults();
        if (a) repaint();
        else if (wasAnalysing) repaint();
        wasAnalysing = a;
    }
}

//==============================================================================
RenskeEditor::RenskeEditor (RenskeProcessor& p)
    : AudioProcessorEditor (&p), proc (p), presetBar (*p.presets), modulePanel (p), assistant (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    content.addAndMakeVisible (presetBar);
    presetBar.attachOverlayTo (content);

    for (auto& m : renske::modules())
    {
        auto* row = rows.add (new renskeui::ChainRow (proc, m));
        const int idx = m.index;
        row->onSelect = [this, idx] { select (idx); };
        content.addAndMakeVisible (row);
    }

    content.addAndMakeVisible (spectrogram);
    content.addAndMakeVisible (modulePanel);
    content.addAndMakeVisible (meters);
    meters.setInterceptsMouseClicks (false, false);
    content.addAndMakeVisible (assistant);

    for (auto [k, id] : { std::pair<sbaudio::Knob*, const char*> { &outKnob, ParamIDs::out }, { &mixKnob, ParamIDs::mix } })
    {
        content.addAndMakeVisible (k);
        sliderAtts.push_back (std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k->slider));
        if (auto* prm = proc.apvts.getParameter (id))
            k->slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
        k->slider.setColour (Slider::rotarySliderFillColourId, rCol);
    }
    outKnob.slider.getProperties().set ("bipolar", true);
    outKnob.slider.setTooltip (u8 ("Nivå ut etter hele kjeden."));
    mixKnob.slider.setTooltip (u8 ("Blander behandlet lyd med original. 100 % = bare behandlet."));

    deltaBtn.setButtonText ("KUN FJERNET");
    deltaBtn.setClickingTogglesState (true);
    deltaBtn.setColour (TextButton::buttonOnColourId, col::amber);
    deltaBtn.setColour (TextButton::textColourOnId, col::bg);
    deltaBtn.setTooltip (u8 ("Lytt bare på det kjeden fjerner. Hører du stemme eller musikk her, er innstillingene for kraftige."));
    content.addAndMakeVisible (deltaBtn);
    deltaAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ParamIDs::delta, deltaBtn);

    select (renske::Shared::mDenoise);

    setResizable (true, true);
    setResizeLimits (designW * 3 / 4, designH * 3 / 4, designW * 2, designH * 2);
    getConstrainer()->setFixedAspectRatio ((double) designW / (double) designH);
    setSize (designW, designH);
    startTimerHz (30);
}

RenskeEditor::~RenskeEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void RenskeEditor::paint (Graphics& g) { g.fillAll (col::bg); }

void RenskeEditor::resized()
{
    const float s = (float) getWidth() / (float) designW;
    content.setBounds (0, 0, designW, designH);
    content.setTransform (AffineTransform::scale (s));
    layoutContent();
}

void RenskeEditor::layoutContent()
{
    presetBar.setBounds (designW - 8 - 470, 8, 470, 28);
    presetBar.getOverlay().setBounds (content.getLocalBounds());

    chainPanel = { 12, 56, 240, 30 + rows.size() * 30 + 10 };
    int y = chainPanel.getY() + 30;
    for (auto* r : rows) { r->setBounds (chainPanel.getX() + 6, y, chainPanel.getWidth() - 12, 30); y += 30; }
    assistant.setBounds (12, chainPanel.getBottom() + 10, 240, designH - 12 - (chainPanel.getBottom() + 10));

    rightPanel = { designW - 12 - 150, 56, 150, designH - 12 - 56 };
    const int cx = chainPanel.getRight() + 10, cw = rightPanel.getX() - 10 - cx;
    spectrogram.setBounds (cx, 56, cw, 270);
    modulePanel.setBounds (cx, spectrogram.getBottom() + 10, cw, designH - 12 - (spectrogram.getBottom() + 10));

    auto rp = rightPanel.reduced (14, 12);
    rp.removeFromTop (22);
    meters.setBounds (rp.removeFromTop (250).reduced (8, 0));
    rp.removeFromTop (16);
    outKnob.setBounds (rp.removeFromTop (112));
    rp.removeFromTop (6);
    mixKnob.setBounds (rp.removeFromTop (112));
    rp.removeFromTop (12);
    deltaBtn.setBounds (rp.removeFromTop (28));
}

void RenskeEditor::paintContent (Graphics& g)
{
    const auto full = content.getLocalBounds().toFloat();
    g.fillAll (col::bg);

    auto bar = full.withHeight (44.0f);
    g.setColour (col::panel);
    g.fillRect (bar);
    g.setColour (col::panelEdge);
    g.fillRect (bar.removeFromBottom (1.0f));
    sbaudio::drawBrandTitle (g, { 14.0f, 0.0f, 340.0f, 44.0f }, "Renske", "REPAIR", rCol);

    drawPanel (g, chainPanel.toFloat());
    g.setColour (rCol);
    g.setFont (uiFont (11.0f, true));
    g.drawText ("MODULKJEDE", chainPanel.toFloat().reduced (14.0f, 8.0f).removeFromTop (16.0f), Justification::centredLeft);
    g.setColour (col::textDim);
    g.setFont (uiFont (10.0f, true));
    g.drawText (u8 ("i rekkefølge ↓"), chainPanel.toFloat().reduced (14.0f, 8.0f).removeFromTop (16.0f), Justification::centredRight);

    // separator between time-domain and spectral modules
    for (int i = 1; i < rows.size(); ++i)
        if (String (rows[i]->info.stage) != rows[i - 1]->info.stage)
        {
            g.setColour (col::panelEdge);
            g.fillRect ((float) chainPanel.getX() + 12.0f, (float) rows[i]->getY() - 0.5f, (float) chainPanel.getWidth() - 24.0f, 1.0f);
        }

    drawPanel (g, rightPanel.toFloat());
    g.setColour (rCol);
    g.setFont (uiFont (11.0f, true));
    g.drawText ("UT", rightPanel.toFloat().reduced (14.0f, 8.0f).removeFromTop (16.0f), Justification::centredLeft);
}

void RenskeEditor::select (int index)
{
    selected = index;
    for (auto* r : rows) r->setSelected (r->info.index == index);
    modulePanel.show (index);
}

void RenskeEditor::timerCallback()
{
    // activity per module
    for (auto* r : rows)
    {
        const auto& m = r->info;
        const float v = proc.shared.activity[(size_t) m.index].exchange (0.0f);
        float& shown = shownActivity[(size_t) m.index];
        if (m.events) shown += 0.05f * (v * 30.0f - shown);          // events per second, smoothed
        else          shown = jmax (v, shown - 0.4f);
        const bool on = proc.apvts.getRawParameterValue (m.onId)->load() > 0.5f;
        if (! on) shown = 0.0f;
        r->setActivity (shown, on);
    }

    // spectrogram
    auto& fifo = proc.shared.fifo;
    int s1, n1, s2, n2;
    fifo.prepareToRead (fifo.getNumReady(), s1, n1, s2, n2);
    for (int i = 0; i < n1; ++i) spectrogram.push (proc.shared.colsIn[(size_t) (s1 + i)], proc.shared.colsOut[(size_t) (s1 + i)]);
    for (int i = 0; i < n2; ++i) spectrogram.push (proc.shared.colsIn[(size_t) (s2 + i)], proc.shared.colsOut[(size_t) (s2 + i)]);
    fifo.finishedRead (n1 + n2);

    // meters
    const float in = proc.meterIn.exchange (0.0f), out = proc.meterOut.exchange (0.0f);
    meters.setLevels (Decibels::gainToDecibels (in, -100.0f), Decibels::gainToDecibels (out, -100.0f));

    assistant.tick();
}
