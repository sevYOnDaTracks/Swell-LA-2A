#pragma once
#include "AnalogComponents.h"
#include "AnalogProcessorBase.h"
#include "PresetBar.h"

namespace swell::analog
{
// Socle des deux éditeurs : façade dessinée à une taille de référence puis mise à l'échelle par
// transformation (tout reste vectoriel/procédural, donc net à 50 % comme à 200 %), redimensionnement
// à proportions fixes, taille mémorisée dans la session.
class AnalogEditor : public juce::AudioProcessorEditor
{
public:
    AnalogEditor (AnalogProcessorBase& p, int designWidth, int designHeight)
        : juce::AudioProcessorEditor (p), proc (p), designW (designWidth), designH (designHeight)
    {
        setLookAndFeel (&lnf);
        addAndMakeVisible (panel);
        panel.setBounds (0, 0, designW, designH);
        setResizable (true, true);
        setResizeLimits (designW / 2, designH / 2, designW * 2, designH * 2);
        if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) designW / (double) designH);
    }
    ~AnalogEditor() override { setLookAndFeel (nullptr); }

    void resized() override
    {
        const float s = (float) getWidth() / (float) designW;
        panel.setTransform (juce::AffineTransform::scale (s));
        if (sizeRestored) proc.setUiScale (s);   // pas pendant la construction (taille mini provisoire)
    }

    // À appeler en fin de constructeur des classes filles.
    void restoreSize()
    {
        const float s = juce::jlimit (0.5f, 2.0f, proc.getUiScale());
        setSize (juce::roundToInt ((float) designW * s), juce::roundToInt ((float) designH * s));
        sizeRestored = true;
    }

    // Façade : le dessin statique (textures, sérigraphie, vis) est rendu une fois en cache.
    struct Panel : juce::Component
    {
        std::function<void (juce::Graphics&)> drawStatic;
        LayerCache cache;
        void paint (juce::Graphics& g) override
        {
            cache.get (g, getLocalBounds(), [this] (juce::Graphics& ig) { if (drawStatic) drawStatic (ig); });
            cache.blit (g, getLocalBounds().toFloat());
        }
    };

protected:
    // Bouton relié à un paramètre (attachement standard JUCE : automation, undo hôte, gestes).
    void attach (Knob& k, const juce::String& id, int size, juce::Point<int> centre)
    {
        panel.addAndMakeVisible (k);
        k.setBounds (juce::Rectangle<int> (size, size).withCentre (centre));
        sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k));
        if (auto* param = proc.apvts.getParameter (id))
            k.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    }

    AnalogProcessorBase& proc;
    const int designW, designH;
    bool sizeRestored = false;
    AnalogLookAndFeel lnf;
    Panel panel;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
};
} // namespace swell::analog
