#include "AnalogLook.h"

namespace swell::analog
{
using namespace juce;

Font panelFont (float height, bool bold)
{
   #if JUCE_MAC
    return Font (FontOptions ("Futura", height, bold ? Font::bold : Font::plain));
   #else
    return Font (FontOptions ("Arial", height, bold ? Font::bold : Font::plain));
   #endif
}

Font condensedFont (float height)
{
   #if JUCE_MAC
    return Font (FontOptions ("Avenir Next Condensed", height, Font::bold));
   #else
    return Font (FontOptions ("Arial Narrow", height, Font::bold));
   #endif
}

float physicalScale (Graphics& g)
{
    return jlimit (0.5f, 6.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
}

//==============================================================================
static void boxBlurRows (std::vector<float>& buf, int w, int h, int radius)
{
    std::vector<float> row ((size_t) w);
    const float norm = 1.0f / (float) (2 * radius + 1);
    for (int y = 0; y < h; ++y)
    {
        float* p = buf.data() + (size_t) y * (size_t) w;
        float sum = 0;
        for (int k = -radius; k <= radius; ++k) sum += p[(k + w) % w];
        for (int x = 0; x < w; ++x)
        {
            row[(size_t) x] = sum * norm;
            sum += p[(x + radius + 1) % w] - p[(x - radius + w) % w];
        }
        std::copy (row.begin(), row.end(), p);
    }
}

static void normalise (std::vector<float>& buf)
{
    double mean = 0, sq = 0;
    for (auto v : buf) { mean += v; sq += (double) v * v; }
    mean /= (double) buf.size();
    const auto sd = std::sqrt (jmax (1.0e-12, sq / (double) buf.size() - mean * mean));
    for (auto& v : buf) v = (float) (((double) v - mean) / sd);
}

static void writePixels (Image& img, const std::vector<float>& lum, Colour base, float strength,
                         std::function<float (float, float)> shading)
{
    const int w = img.getWidth(), h = img.getHeight();
    Image::BitmapData data (img, Image::BitmapData::writeOnly);
    const float r0 = base.getFloatRed(), g0 = base.getFloatGreen(), b0 = base.getFloatBlue();
    for (int y = 0; y < h; ++y)
    {
        const float fy = (float) y / (float) h;
        for (int x = 0; x < w; ++x)
        {
            const float k = (1.0f + strength * lum[(size_t) y * (size_t) w + (size_t) x]) * shading ((float) x / (float) w, fy);
            data.setPixelColour (x, y, Colour::fromFloatRGBA (jlimit (0.0f, 1.0f, r0 * k),
                                                              jlimit (0.0f, 1.0f, g0 * k),
                                                              jlimit (0.0f, 1.0f, b0 * k), 1.0f));
        }
    }
}

Image brushedMetal (int w, int h, Colour base, float strength, uint32_t seed)
{
    Rng r (seed);
    std::vector<float> streaks ((size_t) w * (size_t) h);
    for (int y = 0; y < h; ++y)
    {
        const float rowTone = r.bipolar() * 0.6f;   // chaque passe de brosse a sa propre teinte
        for (int x = 0; x < w; ++x)
            streaks[(size_t) y * (size_t) w + (size_t) x] = rowTone + r.bipolar();
    }
    const int radius = jmax (2, w / 70);
    for (int pass = 0; pass < 3; ++pass)
        boxBlurRows (streaks, w, h, radius);
    normalise (streaks);
    for (auto& v : streaks) v = v * 0.85f + r.bipolar() * 0.35f;   // micro-rayures

    Image img (Image::RGB, w, h, false);
    writePixels (img, streaks, base, strength, [] (float, float fy)
    {
        // Reflet anisotrope typique du métal brossé : une bande claire horizontale.
        return 1.0f + 0.10f * std::exp (-std::pow ((fy - 0.28f) / 0.16f, 2.0f)) - 0.07f * fy;
    });
    return img;
}

static float valueNoise (const std::vector<float>& grid, int gw, int gh, float x, float y)
{
    const int x0 = (int) std::floor (x), y0 = (int) std::floor (y);
    const float fx = x - (float) x0, fy = y - (float) y0;
    auto at = [&] (int ix, int iy) { return grid[(size_t) (((iy % gh) + gh) % gh) * (size_t) gw + (size_t) (((ix % gw) + gw) % gw)]; };
    const float sx = fx * fx * (3 - 2 * fx), sy = fy * fy * (3 - 2 * fy);
    const float a = at (x0, y0) + (at (x0 + 1, y0) - at (x0, y0)) * sx;
    const float b = at (x0, y0 + 1) + (at (x0 + 1, y0 + 1) - at (x0, y0 + 1)) * sx;
    return a + (b - a) * sy;
}

Image paintedPanel (int w, int h, Colour base, float mottling, uint32_t seed)
{
    Rng r (seed);
    const int gw = 64, gh = 64;
    std::vector<float> g1 ((size_t) (gw * gh)), g2 ((size_t) (gw * gh));
    for (auto& v : g1) v = r.bipolar();
    for (auto& v : g2) v = r.bipolar();

    std::vector<float> lum ((size_t) w * (size_t) h);
    const float cell1 = (float) w / 28.0f, cell2 = (float) w / 110.0f;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            const float m = valueNoise (g1, gw, gh, (float) x / cell1, (float) y / cell1) * 0.65f
                          + valueNoise (g2, gw, gh, (float) x / cell2, (float) y / cell2) * 0.35f;
            lum[(size_t) y * (size_t) w + (size_t) x] = m * mottling + r.bipolar() * 0.35f;
        }

    Image img (Image::RGB, w, h, false);
    writePixels (img, lum, base, 0.05f, [] (float, float fy) { return 1.03f - 0.06f * fy; });
    return img;
}

void addGrain (Image& img, float amount, uint32_t seed)
{
    Rng r (seed);
    Image::BitmapData data (img, Image::BitmapData::readWrite);
    for (int y = 0; y < img.getHeight(); ++y)
        for (int x = 0; x < img.getWidth(); ++x)
        {
            auto c = data.getPixelColour (x, y);
            if (c.getAlpha() == 0) continue;
            const float k = 1.0f + r.bipolar() * amount;
            data.setPixelColour (x, y, Colour::fromFloatRGBA (jlimit (0.0f, 1.0f, c.getFloatRed() * k),
                                                              jlimit (0.0f, 1.0f, c.getFloatGreen() * k),
                                                              jlimit (0.0f, 1.0f, c.getFloatBlue() * k), c.getFloatAlpha()));
        }
}

//==============================================================================
void softShadow (Graphics& g, const Path& p, Colour c, float radius, Point<float> offset)
{
    // DropShadow travaille en pixels entiers : on le rend dans le repère physique pour rester net en Retina.
    const auto s = physicalScale (g);
    Graphics::ScopedSaveState save (g);
    g.addTransform (AffineTransform::scale (1.0f / s));
    Path scaled (p);
    scaled.applyTransform (AffineTransform::scale (s));
    DropShadow (c, jmax (1, roundToInt (radius * s)), (offset * s).roundToInt()).drawForPath (g, scaled);
}

void drawScrew (Graphics& g, Point<float> c, float r, float angle, bool phillips)
{
    Path head;
    head.addEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
    softShadow (g, head, Colours::black.withAlpha (0.55f), r * 0.45f, { r * 0.12f, r * 0.22f });

    // Fraisure : petit anneau sombre où la vis entre dans le panneau.
    g.setColour (Colours::black.withAlpha (0.35f));
    g.drawEllipse (c.x - r * 1.06f, c.y - r * 1.06f, r * 2.12f, r * 2.12f, r * 0.12f);

    ColourGradient metal (Colour (0xffeeeeea), c.x - r * 0.6f, c.y - r * 0.7f, Colour (0xff55575a), c.x + r * 0.7f, c.y + r * 0.8f, true);
    metal.addColour (0.45, Colour (0xffb4b6b8));
    g.setGradientFill (metal);
    g.fillPath (head);

    // Usinage concentrique.
    for (int i = 1; i < 6; ++i)
    {
        const float rr = r * (float) i / 6.0f;
        g.setColour (Colours::white.withAlpha (i % 2 ? 0.06f : 0.0f));
        g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, r * 0.05f);
    }

    auto slot = [&] (float a)
    {
        Path s;
        s.addRoundedRectangle (-r * 0.78f, -r * 0.13f, r * 1.56f, r * 0.26f, r * 0.08f);
        s.applyTransform (AffineTransform::rotation (a).translated (c));
        g.setColour (Colour (0xff1c1d1f));
        g.fillPath (s);
        // Arête éclairée côté opposé à la lumière.
        Path lip (s);
        lip.applyTransform (AffineTransform::translation (r * 0.04f, r * 0.07f));
        g.setColour (Colours::white.withAlpha (0.28f));
        g.strokePath (lip, PathStrokeType (r * 0.05f));
    };
    slot (angle);
    if (phillips) slot (angle + MathConstants<float>::halfPi);

    ColourGradient spec (Colours::white.withAlpha (0.55f), c.x - r * 0.35f, c.y - r * 0.45f,
                         Colours::white.withAlpha (0.0f), c.x - r * 0.35f + r * 0.55f, c.y - r * 0.45f, true);
    g.setGradientFill (spec);
    g.fillPath (head);
    g.setColour (Colours::black.withAlpha (0.5f));
    g.strokePath (head, PathStrokeType (r * 0.06f));
}

void drawRackEar (Graphics& g, Rectangle<float> ear, Colour, bool dark)
{
    const float sw = ear.getWidth() * 0.34f, sh = ear.getHeight() * 0.16f;
    for (float fy : { 0.2f, 0.8f })
    {
        const Rectangle<float> slot (ear.getCentreX() - sw * 0.5f, ear.getY() + ear.getHeight() * fy - sh * 0.5f, sw, sh);
        Path hole;
        hole.addRoundedRectangle (slot, sw * 0.5f);
        // Le trou laisse voir le rail du rack, dans l'ombre.
        g.setColour (Colour (0xff0b0b0c));
        g.fillPath (hole);
        {
            Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (hole);
            ColourGradient rail (Colour (0xff3a3b3e), slot.getX(), slot.getY(), Colour (0xff111112), slot.getRight(), slot.getBottom(), false);
            g.setGradientFill (rail);
            g.fillRect (slot.reduced (sw * 0.15f, 0));
            // Ombre portée de l'épaisseur du panneau.
            g.setColour (Colours::black.withAlpha (0.7f));
            g.fillRect (slot.withHeight (sh * 0.22f));
        }
        g.setColour (dark ? Colours::white.withAlpha (0.10f) : Colours::white.withAlpha (0.45f));
        g.drawLine (slot.getX() + sw * 0.25f, slot.getBottom() + 0.5f, slot.getRight() - sw * 0.25f, slot.getBottom() + 0.5f, 0.8f);

        // Vis de rack avec rondelle.
        const auto c = slot.getCentre().translated (0, sh * 0.08f);
        const float wr = sw * 0.62f;
        Path washer;
        washer.addEllipse (c.x - wr, c.y - wr, wr * 2, wr * 2);
        softShadow (g, washer, Colours::black.withAlpha (0.6f), wr * 0.35f, { wr * 0.1f, wr * 0.25f });
        ColourGradient wg (Colour (0xffd8d8d4), c.x - wr, c.y - wr, Colour (0xff6d6e70), c.x + wr, c.y + wr, false);
        g.setGradientFill (wg);
        g.fillPath (washer);
        drawScrew (g, c, wr * 0.74f, 0.4f + fy, true);
    }
}

void drawPrinted (Graphics& g, const String& text, Rectangle<float> area, const Font& font, Colour ink, Justification j)
{
    g.setFont (font);
    g.setColour (ink);
    g.drawText (text, area, j, false);
}

void drawPanelLight (Graphics& g, Rectangle<float> r, float strength)
{
    ColourGradient v (Colours::white.withAlpha (0.10f * strength), r.getX(), r.getY(),
                      Colours::black.withAlpha (0.22f * strength), r.getX(), r.getBottom(), false);
    v.addColour (0.35, Colours::transparentWhite);
    g.setGradientFill (v);
    g.fillRect (r);

    ColourGradient h (Colours::black.withAlpha (0.16f * strength), r.getX(), r.getY(),
                      Colours::black.withAlpha (0.20f * strength), r.getRight(), r.getY(), false);
    h.addColour (0.12, Colours::transparentBlack);
    h.addColour (0.88, Colours::transparentBlack);
    g.setGradientFill (h);
    g.fillRect (r);
}

void drawBevelEdge (Graphics& g, Rectangle<float> r, float corner, float strength)
{
    Path top, bottom;
    top.startNewSubPath (r.getX() + corner, r.getY() + 0.75f);
    top.lineTo (r.getRight() - corner, r.getY() + 0.75f);
    bottom.startNewSubPath (r.getX() + corner, r.getBottom() - 0.75f);
    bottom.lineTo (r.getRight() - corner, r.getBottom() - 0.75f);
    g.setColour (Colours::white.withAlpha (0.35f * strength));
    g.strokePath (top, PathStrokeType (1.2f));
    g.setColour (Colours::black.withAlpha (0.6f * strength));
    g.strokePath (bottom, PathStrokeType (1.5f));
}

void drawKnobScale (Graphics& g, const ScaleSpec& s)
{
    const int majors = jmax (2, s.labels.size());
    const int steps = (majors - 1) * s.minorPerMajor;
    g.setColour (s.ink);
    for (int i = 0; i <= steps; ++i)
    {
        const float a = s.start + (s.end - s.start) * (float) i / (float) steps;
        const bool major = i % s.minorPerMajor == 0;
        const float len = major ? s.tickLen : s.tickLen * 0.55f;
        const auto p1 = s.centre.getPointOnCircumference (s.radius, a);
        const auto p2 = s.centre.getPointOnCircumference (s.radius + len, a);
        g.drawLine ({ p1, p2 }, major ? 1.6f : 1.0f);
    }
    g.setFont (panelFont (s.fontSize, true));
    for (int i = 0; i < s.labels.size(); ++i)
    {
        const float a = s.start + (s.end - s.start) * (float) i / (float) (majors - 1);
        const auto p = s.centre.getPointOnCircumference (s.labelRadius, a);
        g.drawText (s.labels[i], Rectangle<float> (40, s.fontSize + 4).withCentre (p), Justification::centred, false);
    }
}
} // namespace swell::analog
