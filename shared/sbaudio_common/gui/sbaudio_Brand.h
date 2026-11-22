#pragma once

namespace sbaudio
{
    /** Brand name shown in every plugin's title block. */
    inline const char* brandName() { return "SLOWHAND"; }

    /** Draws the shared title block used in every plugin's top bar:

            SLOWHAND                 <- small, letter-spaced, dim
            PluginName  TYPE         <- large name in the plugin's colour, small type

        The block is drawn left-aligned inside 'area' (normally the left part of the top bar)
        and shrinks the fonts if the text would not fit in area's width.
        Returns the width actually used. */
    inline float drawBrandTitle (juce::Graphics& g, juce::Rectangle<float> area,
                                 const juce::String& pluginName, const juce::String& pluginType,
                                 const juce::FillType& nameFill,
                                 juce::Colour dim = colours::textDim)
    {
        using namespace juce;
        const float h = area.getHeight();
        const float gap = 7.0f;

        auto brandFont = uiFont (jlimit (8.0f, 11.0f, h * 0.19f), true).withExtraKerningFactor (0.32f);
        auto nameFont  = uiFont (jlimit (15.0f, 24.0f, h * 0.40f), true);
        auto typeFont  = uiFont (jlimit (8.0f, 11.0f, h * 0.20f), true).withExtraKerningFactor (0.14f);

        auto width = [&]
        {
            return GlyphArrangement::getStringWidth (nameFont, pluginName) + gap
                 + GlyphArrangement::getStringWidth (typeFont, pluginType);
        };

        if (const float w = width(); w > area.getWidth() && w > 0.0f)
        {
            const float s = jmax (0.65f, area.getWidth() / w);
            nameFont = nameFont.withHeight (nameFont.getHeight() * s);
            typeFont = typeFont.withHeight (typeFont.getHeight() * s);
            brandFont = brandFont.withHeight (brandFont.getHeight() * jmax (0.8f, s));
        }

        // line 1: brand
        const float brandH = brandFont.getHeight() + 2.0f;
        const float nameH  = nameFont.getHeight();
        const float top = area.getY() + (h - brandH - nameH) * 0.5f;

        g.setFont (brandFont);
        g.setColour (dim);
        g.drawText (brandName(), Rectangle<float> (area.getX() + 1.0f, top, area.getWidth(), brandH),
                    Justification::centredLeft, false);

        // line 2: name + type, sharing a baseline
        const float baseline = top + brandH + nameFont.getAscent();
        const float nameW = GlyphArrangement::getStringWidth (nameFont, pluginName);

        g.setFont (nameFont);
        g.setFillType (nameFill);
        g.drawSingleLineText (pluginName, roundToInt (area.getX()), roundToInt (baseline));

        g.setFont (typeFont);
        g.setColour (dim);
        g.drawSingleLineText (pluginType, roundToInt (area.getX() + nameW + gap), roundToInt (baseline));

        return jmax (GlyphArrangement::getStringWidth (brandFont, brandName()), width());
    }

    inline float drawBrandTitle (juce::Graphics& g, juce::Rectangle<float> area,
                                 const juce::String& pluginName, const juce::String& pluginType,
                                 juce::Colour nameColour, juce::Colour dim = colours::textDim)
    {
        return drawBrandTitle (g, area, pluginName, pluginType, juce::FillType (nameColour), dim);
    }
}
