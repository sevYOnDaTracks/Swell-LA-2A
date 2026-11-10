#pragma once
#include "AnalogLook.h"
#include "AnalogProcessorBase.h"

namespace swell::analog
{
// Afficheur de presets encastré dans la façade : fenêtre noire rétroéclairée, flèches ‹ › pour passer
// au précédent / suivant (usine puis presets perso), clic sur le nom = menu complet
// (presets d'usine par catégorie, presets perso, enregistrer, ouvrir le dossier).
class PresetBar : public juce::Component
{
public:
    PresetBar (AnalogProcessorBase&, bool onDarkPanel);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { setHover (-1); }
    // Appelé à chaque image par l'éditeur : ne repeint que si le nom a changé (automation, hôte...).
    void refresh();

private:
    int zoneAt (juce::Point<int>) const;   // 0 = flèche gauche, 1 = nom, 2 = flèche droite
    void setHover (int z) { if (z != hover) { hover = z; repaint(); } }
    void step (int direction);
    void showMenu();
    void saveAs();

    AnalogProcessorBase& proc;
    const bool dark;
    juce::String shown;
    int hover = -1;
    std::unique_ptr<juce::FileChooser> chooser;
};
} // namespace swell::analog
