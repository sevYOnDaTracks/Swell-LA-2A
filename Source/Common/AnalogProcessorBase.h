#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Licence/SwellLicence.h"

namespace swell
{
// Socle commun aux deux compresseurs :
//  - suréchantillonnage 1x / 2x / 4x (demi-bande polyphase IIR, faible latence), tous préparés d'avance :
//    changer de facteur pendant la lecture n'alloue rien ;
//  - mélange dry/wet aligné sur la latence (compression parallèle sans filtrage en peigne) ;
//  - interrupteur d'alimentation = bypass progressif ;
//  - mesures pour l'interface (atomiques, lues par le thread graphique).
class AnalogProcessorBase : public juce::AudioProcessor
{
public:
    using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;
    static void addCommonParameters (Layout&, float defaultMixPercent = 100.0f);

    explicit AnalogProcessorBase (Layout);

    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    bool hasEditor() const override { return true; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    // Presets d'usine exposés aussi comme « programmes » de l'hôte.
    int getNumPrograms() override { return juce::jmax (1, (int) factoryPresets().size()); }
    int getCurrentProgram() override { return juce::jmax (0, currentFactory); }
    void setCurrentProgram (int index) override { loadFactoryPreset (index); }
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // Licence Swell : partagée par toutes les instances ; silence tant que le plugin n'est pas activé.
    juce::SharedResourcePointer<swell::Licence> licence;

    // Mesures (thread audio -> interface).
    std::atomic<float> meterGainReductionDb { 0.0f };
    std::atomic<float> meterOutputRms { 0.0f };   // valeur moyenne redressée, calibrée en RMS sinus
    std::atomic<float> meterInputRms { 0.0f };

    // --- Presets ------------------------------------------------------------------------------------
    // Un preset ne touche qu'au son : Power, suréchantillonnage et sélecteur du vu-mètre restent en place.
    struct FactoryPreset
    {
        const char* category;
        const char* name;
        std::vector<std::pair<const char*, float>> values;   // paramètres absents = valeur par défaut
    };
    virtual const std::vector<FactoryPreset>& factoryPresets() const = 0;
    virtual juce::String productName() const = 0;      // « Swell FET 76 »

    void loadFactoryPreset (int index);
    bool saveUserPreset (const juce::File&);
    bool loadUserPreset (const juce::File&);
    juce::File userPresetFolder() const;               // Documents/Swell Audio/<produit> Presets
    juce::Array<juce::File> userPresets() const;
    static constexpr const char* presetExtension = ".swellpreset";
    juce::String getPresetName() const;
    int getCurrentFactoryIndex() const noexcept { return currentFactory; }

    // Taille de l'interface mémorisée avec la session.
    float getUiScale() const;
    void setUiScale (float);

protected:
    virtual void prepareEngine (double oversampledRate, int numChannels) = 0;
    virtual void resetEngine() = 0;
    // Traite le bloc suréchantillonné, renvoie la réduction de gain maximale du bloc (dB, positive).
    virtual float processEngine (juce::dsp::AudioBlock<float>&) = 0;

private:
    void selectOversampling (int index);

    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 3> oversamplers;
    int currentOs = -1;
    double baseRate = 44100.0;
    juce::dsp::DryWetMixer<float> mixer { 64 };
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* powerParam = nullptr;
    std::atomic<float>* osParam = nullptr;
    float outEnv = 0, inEnv = 0;
    int currentFactory = 0;
    static bool isSoundParameter (const juce::String& id) { return id != "power" && id != "oversampling" && id != "meter"; }
    void setPresetName (const juce::String&);
};
} // namespace swell
