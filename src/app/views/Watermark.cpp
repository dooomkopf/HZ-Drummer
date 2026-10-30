#include "app/views/Watermark.h"

#include "BinaryData.h"

namespace
{
    constexpr float strength = 0.05f;        // letters; the kit gets half of it
    constexpr float fill = 0.95f;            // share of the window it may cover in either direction
    const juce::Colour letterColour { 0xffe12d2d };

    // dark kit -> light, red letters -> red, white paper and outlines -> transparent
    juce::Image extract (const juce::Image& picture)
    {
        juce::Image out (juce::Image::ARGB, picture.getWidth(), picture.getHeight(), true);
        for (int y = 0; y < picture.getHeight(); ++y)
            for (int x = 0; x < picture.getWidth(); ++x)
            {
                const auto c = picture.getPixelAt (x, y);
                const int r = c.getRed(), g = c.getGreen(), b = c.getBlue();
                const float red = juce::jlimit (0.0f, 1.0f, (float) (r - juce::jmax (g, b) - 50) / 100.0f);
                const float dark = juce::jlimit (0.0f, 1.0f, (float) (600 - (r + g + b)) / 450.0f);
                if (red >= dark && red > 0.0f)
                    out.setPixelAt (x, y, letterColour.withAlpha (red * c.getFloatAlpha()));
                else if (dark > 0.0f)
                    out.setPixelAt (x, y, juce::Colours::white.withAlpha (0.5f * dark * c.getFloatAlpha()));
            }
        return out;
    }
}

void Watermark::draw (juce::Graphics& g, juce::Rectangle<int> area)
{
    if (source.isNull())
    {
        const auto picture = juce::ImageCache::getFromMemory (BinaryData::drumkit_png, BinaryData::drumkit_pngSize);
        if (picture.isNull())
            return;
        source = extract (picture);
    }

    const float scale = juce::jmin ((float) area.getWidth() * fill / (float) source.getWidth(),
                                    (float) area.getHeight() * fill / (float) source.getHeight());
    const int w = (int) ((float) source.getWidth() * scale), h = (int) ((float) source.getHeight() * scale);
    if (w <= 0 || h <= 0)
        return;
    if (scaled.getWidth() != w || scaled.getHeight() != h)
        scaled = source.rescaled (w, h, juce::Graphics::highResamplingQuality);

    // centred, a bit low: the cymbals sit behind the title, the drums behind the displays
    const auto at = juce::Rectangle<int> (w, h).withCentre (area.getCentre().translated (0, area.getHeight() / 20));
    g.setOpacity (strength);
    g.drawImageAt (scaled, at.getX(), at.getY());
    g.setOpacity (1.0f);
}
