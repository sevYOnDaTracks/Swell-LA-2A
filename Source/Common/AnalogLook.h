#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Rendu « matériel réel » sans aucune image fournie : tout est calculé (textures, lumière, ombres, verre).
// Principe de performance : ce qui ne bouge pas est rendu une fois dans une image à la résolution physique
// de l'écran (Retina compris) puis simplement recopié ; seules les pièces mobiles sont redessinées.
namespace swell::analog
{
// Lumière principale : en haut à gauche, comme une photo de studio.
inline constexpr float lightAngle = -2.35f;   // radians, sens JUCE (0 = vers la droite, y vers le bas)

juce::Font panelFont (float height, bool bold = true);
juce::Font condensedFont (float height);
float physicalScale (juce::Graphics&);

// Générateur pseudo-aléatoire déterministe : les textures sont identiques à chaque ouverture.
struct Rng
{
    explicit Rng (uint32_t seed) : s (seed ? seed : 0x9e3779b9u) {}
    uint32_t nextInt() noexcept { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float next() noexcept { return (float) (nextInt() & 0xffffff) / 16777216.0f; }   // [0, 1)
    float bipolar() noexcept { return next() * 2.0f - 1.0f; }
    uint32_t s;
};

// --- Textures procédurales (calculées une fois, à haute résolution) -------------------------------
// Aluminium brossé : stries horizontales, flou anisotrope, reflet en bande.
juce::Image brushedMetal (int w, int h, juce::Colour base, float strength, uint32_t seed);
// Peinture satinée / martelée : marbrures basse fréquence + grain fin.
juce::Image paintedPanel (int w, int h, juce::Colour base, float mottling, uint32_t seed);
// Grain fin superposable (papier, plastique).
void addGrain (juce::Image&, float amount, uint32_t seed);

// --- Éléments dessinés ------------------------------------------------------------------------------
void softShadow (juce::Graphics&, const juce::Path&, juce::Colour, float radius, juce::Point<float> offset);
void drawScrew (juce::Graphics&, juce::Point<float> centre, float radius, float angle, bool phillips = true);
void drawRackEar (juce::Graphics&, juce::Rectangle<float> ear, juce::Colour panelTone, bool dark);
void drawPrinted (juce::Graphics&, const juce::String&, juce::Rectangle<float>, const juce::Font&,
                  juce::Colour ink, juce::Justification = juce::Justification::centred);
void drawPanelLight (juce::Graphics&, juce::Rectangle<float>, float strength);
void drawBevelEdge (juce::Graphics&, juce::Rectangle<float>, float corner, float strength);

// Graduations imprimées autour d'un bouton (repères + nombres).
struct ScaleSpec
{
    juce::Point<float> centre;
    float radius = 50, tickLen = 6, labelRadius = 66;
    float start = -2.4f, end = 2.4f;       // angles JUCE (0 = midi)
    juce::StringArray labels;              // répartis régulièrement de start à end
    int minorPerMajor = 1;
    float fontSize = 11;
    juce::Colour ink;
};
void drawKnobScale (juce::Graphics&, const ScaleSpec&);

// Cache d'image à la résolution physique : rendu paresseux, invalidé si la taille ou l'échelle change.
class LayerCache
{
public:
    template <typename Fn>
    const juce::Image& get (juce::Graphics& g, juce::Rectangle<int> area, Fn&& draw)
    {
        const auto scale = physicalScale (g);
        if (image.isNull() || area.getWidth() != w || area.getHeight() != h || std::abs (scale - lastScale) > 0.01f)
        {
            w = area.getWidth(); h = area.getHeight(); lastScale = scale;
            image = juce::Image (juce::Image::ARGB, juce::jmax (1, juce::roundToInt ((float) w * scale)),
                                 juce::jmax (1, juce::roundToInt ((float) h * scale)), true);
            juce::Graphics ig (image);
            ig.addTransform (juce::AffineTransform::scale (scale));
            draw (ig);
        }
        return image;
    }
    void blit (juce::Graphics& g, juce::Rectangle<float> dest) const
    {
        if (image.isValid()) g.drawImage (image, dest, juce::RectanglePlacement::stretchToFit);
    }
    void invalidate() { image = {}; }
private:
    juce::Image image;
    int w = 0, h = 0;
    float lastScale = 0;
};
} // namespace swell::analog
