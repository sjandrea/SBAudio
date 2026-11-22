namespace sbaudio
{
using namespace juce;

//==============================================================================
PresetSaveOverlay::PresetSaveOverlay()
{
    name.setFont (uiFont (14.0f));
    name.setJustification (Justification::centredLeft);
    name.onReturnKey = [this] { saveBtn.triggerClick(); };
    name.onEscapeKey = [this] { setVisible (false); };
    name.onTextChange = [this] { updateHint(); };
    addAndMakeVisible (name);

    saveBtn.setColour (TextButton::buttonOnColourId, colours::accent);
    saveBtn.setToggleState (true, dontSendNotification);
    saveBtn.onClick = [this]
    {
        const auto n = name.getText().trim();
        if (n.isEmpty())
            return;
        setVisible (false);
        if (onSave) onSave (n);
    };
    cancelBtn.onClick = [this] { setVisible (false); };
    addAndMakeVisible (saveBtn);
    addAndMakeVisible (cancelBtn);

    hint.setFont (uiFont (11.0f));
    hint.setColour (Label::textColourId, colours::amber);
    addAndMakeVisible (hint);
}

void PresetSaveOverlay::show (const String& initialName)
{
    name.setText (initialName, dontSendNotification);
    updateHint();
    setVisible (true);
    toFront (true);
    name.grabKeyboardFocus();
    name.selectAll();
}

void PresetSaveOverlay::updateHint()
{
    const auto n = name.getText().trim();
    const bool ex = n.isNotEmpty() && exists && exists (n);
    hint.setText (ex ? String (CharPointer_UTF8 ("Finnes allerede – lagring overskriver den.")) : String(), dontSendNotification);
    saveBtn.setButtonText (ex ? "OVERSKRIV" : "LAGRE");
}

Rectangle<int> PresetSaveOverlay::box() const { return getLocalBounds().withSizeKeepingCentre (380, 150); }

void PresetSaveOverlay::paint (Graphics& g)
{
    g.fillAll (Colours::black.withAlpha (0.55f));
    auto b = box().toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (b, 8.0f);
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (b, 8.0f, 1.0f);
    g.setColour (colours::text);
    g.setFont (uiFont (14.0f, true));
    g.drawText ("Lagre preset", b.reduced (18.0f, 12.0f).removeFromTop (20.0f), Justification::centredLeft);
}

void PresetSaveOverlay::resized()
{
    auto b = box().reduced (18, 12);
    b.removeFromTop (26);
    name.setBounds (b.removeFromTop (30));
    hint.setBounds (b.removeFromTop (22));
    auto row = b.removeFromBottom (30);
    saveBtn.setBounds (row.removeFromRight (100));
    row.removeFromRight (8);
    cancelBtn.setBounds (row.removeFromRight (90));
}

//==============================================================================
PresetBar::PresetBar (PresetManager& pm) : presets (pm)
{
    for (auto* b : { &prevBtn, &nextBtn, &nameBtn, &saveBtn })
        addAndMakeVisible (b);

    auto loaded = [this] { refresh(); if (onPresetLoaded) onPresetLoaded(); };
    prevBtn.onClick = [this, loaded] { presets.step (-1); loaded(); };
    nextBtn.onClick = [this, loaded] { presets.step (1);  loaded(); };
    prevBtn.setTooltip ("Forrige preset");
    nextBtn.setTooltip ("Neste preset");
    nameBtn.onClick = [this] { showMenu(); };
    nameBtn.setTooltip ("Velg preset");
    saveBtn.onClick = [this] { overlay.show (presets.isCurrentFactory() ? String() : presets.getCurrentPresetName()); };
    saveBtn.setTooltip (CharPointer_UTF8 ("Lagre innstillingene som egen preset"));

    overlay.exists = [this] (const String& n) { return presets.userPresetExists (n); };
    overlay.onSave = [this] (const String& n)
    {
        if (! presets.saveUserPreset (n))
            AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Preset",
                                              String (CharPointer_UTF8 ("Kunne ikke lagre preset i "))
                                                  + presets.getUserPresetDirectory().getFullPathName());
        refresh();
    };

    refresh();
    startTimerHz (5); // picks up preset changes from the host (Logic's preset menu)
}

PresetBar::~PresetBar()
{
    if (auto* p = overlay.getParentComponent())
        p->removeChildComponent (&overlay);
}

void PresetBar::attachOverlayTo (Component& editor)
{
    editor.addChildComponent (overlay);
    overlay.setBounds (editor.getLocalBounds());
}

void PresetBar::refresh()
{
    shown = presets.getCurrentPresetName();
    shownFactory = presets.isCurrentFactory();
    nameBtn.setButtonText (shown.isEmpty() ? String ("(ingen preset)") : shown);
}

void PresetBar::timerCallback()
{
    if (presets.getCurrentPresetName() != shown || presets.isCurrentFactory() != shownFactory)
        refresh();
}

void PresetBar::resized()
{
    auto r = getLocalBounds();
    saveBtn.setBounds (r.removeFromRight (62));
    r.removeFromRight (6);
    prevBtn.setBounds (r.removeFromLeft (26));
    r.removeFromLeft (3);
    nextBtn.setBounds (r.removeFromRight (26));
    r.removeFromRight (3);
    nameBtn.setBounds (r);
}

void PresetBar::showMenu()
{
    auto all = presets.getAllPresets();
    const auto cur = presets.getCurrentPresetName();
    const bool curFactory = presets.isCurrentFactory();

    PopupMenu m;
    m.addSectionHeader ("Fabrikk-presets");

    // Many factory presets: group "Category – Name" into one submenu per category.
    const String dash (CharPointer_UTF8 (" – "));
    const bool grouped = presets.getNumFactoryPresets() > 30;
    StringArray categories;
    std::vector<PopupMenu> subMenus;
    std::vector<bool> subTicked;

    bool anyUser = false;
    for (int i = 0; i < (int) all.size(); ++i)
    {
        const auto& e = all[(size_t) i];
        const bool ticked = e.name == cur && e.factory == curFactory;
        if (e.factory && grouped && e.name.contains (dash))
        {
            const auto cat = e.name.upToFirstOccurrenceOf (dash, false, false);
            int idx = categories.indexOf (cat);
            if (idx < 0)
            {
                idx = categories.size();
                categories.add (cat);
                subMenus.emplace_back();
                subTicked.push_back (false);
            }
            subMenus[(size_t) idx].addItem (i + 1, e.name.fromFirstOccurrenceOf (dash, false, false), true, ticked);
            subTicked[(size_t) idx] = subTicked[(size_t) idx] || ticked;
            continue;
        }
        if (grouped && e.factory) { m.addItem (i + 1, e.name, true, ticked); continue; }

        if (! e.factory && ! anyUser)
        {
            for (size_t c = 0; c < subMenus.size(); ++c)
                m.addSubMenu (categories[(int) c], subMenus[c], true, nullptr, subTicked[c]);
            subMenus.clear();
            m.addSeparator();
            m.addSectionHeader ("Mine presets");
            anyUser = true;
        }
        m.addItem (i + 1, e.name, true, ticked);
    }
    for (size_t c = 0; c < subMenus.size(); ++c)
        m.addSubMenu (categories[(int) c], subMenus[c], true, nullptr, subTicked[c]);
    if (! anyUser)
    {
        m.addSeparator();
        m.addSectionHeader ("Mine presets");
        m.addItem (-1, CharPointer_UTF8 ("(ingen ennå – trykk LAGRE)"), false);
    }
    m.addSeparator();
    m.addItem (5000, CharPointer_UTF8 ("Lagre som ny…"));
    m.addItem (5001, String (CharPointer_UTF8 ("Slett «")) + cur + String (CharPointer_UTF8 ("»")),
               ! curFactory && cur.isNotEmpty());
    m.addItem (5002, "Vis preset-mappen i Finder");

    SafePointer<PresetBar> safe (this);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (nameBtn), [safe, all] (int r)
    {
        if (safe == nullptr)
            return;
        auto& self = *safe;
        if (r >= 1 && r <= (int) all.size())
        {
            self.presets.load (all[(size_t) r - 1]);
            if (self.onPresetLoaded) self.onPresetLoaded();
        }
        else if (r == 5000)
            self.overlay.show ({});
        else if (r == 5001)
            self.presets.deleteUserPreset (self.presets.getCurrentPresetName());
        else if (r == 5002)
        {
            auto dir = self.presets.getUserPresetDirectory();
            dir.createDirectory();
            dir.startAsProcess();
        }
        self.refresh();
    });
}
}
