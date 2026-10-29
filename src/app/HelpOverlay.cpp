#include "app/HelpOverlay.h"
#include "app/Text.h"

void HelpOverlay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xf0101013));

    auto area = getLocalBounds().reduced (40, 30);

    g.setColour (juce::Colour (0xffeeeeee));
    g.setFont (juce::FontOptions (30.0f, juce::Font::bold));
    g.drawText ("Tastenbelegung", area.removeFromTop (44), juce::Justification::centredLeft);
    area.removeFromTop (10);

    const int rowH = juce::jlimit (26, 40, area.getHeight() / juce::jmax (1, (int) entries.size() + 1));
    const int keyW = juce::jmin (260, area.getWidth() / 3);

    for (const auto& e : entries)
    {
        auto row = area.removeFromTop (rowH);
        g.setColour (juce::Colour (0xffe89a2c));
        g.setFont (juce::FontOptions ((float) rowH * 0.62f, juce::Font::bold));
        g.drawText (e.keys, row.removeFromLeft (keyW), juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xffd0d0d6));
        g.setFont (juce::FontOptions ((float) rowH * 0.62f));
        g.drawText (e.description, row, juce::Justification::centredLeft);
    }

    g.setColour (juce::Colour (0xff8a8a92));
    g.setFont (juce::FontOptions (19.0f));
    g.drawText ("Beliebige Taste oder Klick schließt die Hilfe"_u, getLocalBounds().reduced (40, 16).removeFromBottom (24),
                juce::Justification::centredLeft);
}
