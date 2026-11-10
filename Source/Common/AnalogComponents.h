#pragma once
#include "AnalogLook.h"

namespace swell::analog
{
const juce::Image& grainTile();   // bruit 256 x 256 à superposer en mosaïque (papier, plastique)

// Bulle de valeur pendant le réglage + polices.
class AnalogLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AnalogLookAndFeel();
    void drawBubble (juce::Graphics&, juce::BubbleComponent&, const juce::Point<float>& tip, const juce::Rectangle<float>& body) override;
    juce::Font getSliderPopupFont (juce::Slider&) override { return panelFont (13.0f, true); }
    int getSliderPopupPlacement (juce::Slider&) override { return juce::BubbleComponent::above; }
};

// Bouton rotatif. La lumière reste fixe pendant la rotation (comme un vrai objet photographié) :
// le corps, la face et le reflet sont en cache, seuls les cannelures et l'index sont redessinés.
class Knob : public juce::Slider
{
public:
    enum class Style { FetLarge, FetSmall, OptoLarge, OptoSmall, ChickenHead };
    explicit Knob (Style);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    void paintChickenHead (juce::Graphics&, juce::Point<float> c, float r, float angle);
    Style style;
    LayerCache base, face, gloss;
};

// Vu-mètre à aiguille : balistique VU réelle (masse-ressort amortie, ~300 ms, léger dépassement),
// butées avec rebond, cadran rétroéclairé, ombre de l'aiguille et reflets du verre.
class VuMeter : public juce::Component
{
public:
    struct Style
    {
        juce::Colour face { 0xfff0e8d2 }, ink { 0xff1a1714 }, red { 0xffb3261e }, glow { 0xffffd9a0 };
        float glowAmount = 0.35f;
        juce::String caption = "SWELL";
    };
    explicit VuMeter (Style);
    // Valeur affichée en dB VU (0 = repère 0). Pour la réduction de gain : -GR.
    void setTargetDb (float vuDb) noexcept;
    void setLit (bool on) { if (on != lit) { lit = on; back.invalidate(); repaint(); } }
    void jumpToTarget() { pos = target; vel = 0; repaint(); }
    void paint (juce::Graphics&) override;
private:
    void tick (double now);
    juce::Rectangle<float> window() const;
    void geometry (juce::Point<float>& pivot, float& radius) const;
    float angleFor (float fraction) const { return -0.62f + 1.24f * fraction; }
    static float fractionFor (float db) { return std::pow (10.0f, (db - 3.0f) / 20.0f); }
    void paintBack (juce::Graphics&);
    void paintGlass (juce::Graphics&);

    Style style;
    bool lit = true;
    float target = 0, pos = 0, vel = 0, painted = -1;
    double lastTime = 0;
    LayerCache back, glass;
    juce::VBlankAttachment vblank;
};

// Touche carrée à verrouillage (sélecteurs de ratio et de mesure).
class PushKey : public juce::Button
{
public:
    PushKey() : juce::Button ({}) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

// Inverseur à levier chromé (alimentation, mode).
class BatToggle : public juce::Button
{
public:
    BatToggle() : juce::Button ({}) { setClickingTogglesState (true); setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

// Voyant à cabochon de verre taillé avec halo.
class PilotLamp : public juce::Component
{
public:
    explicit PilotLamp (juce::Colour c) : colour (c) { setInterceptsMouseClicks (false, false); }
    void setLit (bool on) { if (on != lit) { lit = on; repaint(); } }
    void paint (juce::Graphics&) override;
private:
    juce::Colour colour;
    bool lit = true;
};
} // namespace swell::analog
