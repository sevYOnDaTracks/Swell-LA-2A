#pragma once
#include <juce_dsp/juce_dsp.h>

namespace swell
{
// Nivelleur optique à lampes, famille « 2A ».
// Modèle de la cellule optique (panneau électroluminescent + photorésistance) :
//  - topologie en contre-réaction : le circuit de commande écoute la sortie de l'atténuateur, d'où un
//    ratio doux qui dépend du programme (~3:1 en Compress, bien plus raide en Limit) ;
//  - Peak Reduction règle la sensibilité de la commande, Gain le niveau de sortie (sans effet sur la compression) ;
//  - panneau EL : réponse rapide avec persistance ;
//  - photorésistance à deux constantes : partie rapide (attaque ~10 ms, retour ~60 ms) et partie lente
//    qui se charge pendant les compressions soutenues et relâche en 0,5 à 5 s : le retour en deux temps
//    caractéristique (vite jusqu'à mi-course, puis très lentement), avec mémoire ;
//  - Emphasis : accentue les aigus dans la commande (moins de sifflantes, comme le réglage interne d'origine) ;
//  - étage à lampes asymétrique (harmonique 2) et saturation des graves du transformateur de sortie.
class OptoEngine
{
public:
    void prepare (double sampleRate, int channels)
    {
        fs = sampleRate;
        numChannels = juce::jlimit (1, 2, channels);
        dt = (float) (1.0 / fs);
        makeup.reset (fs, 0.03);
        scGain.reset (fs, 0.03);
        auto coef = [this] (float seconds) { return std::exp (-dt / seconds); };
        elAtt = coef (0.002f); elRel = coef (0.012f);
        fastAtt = coef (0.010f); fastRel = coef (0.060f);
        slowAtt = coef (0.35f);
        emphCoef = std::exp (-juce::MathConstants<float>::twoPi * 1500.0f * dt);
        lfCoef = std::exp (-juce::MathConstants<float>::twoPi * 90.0f * dt);
        hfCoef = std::exp (-juce::MathConstants<float>::twoPi * juce::jmin (26000.0f, (float) fs * 0.45f) * dt);
        dcCoef = 1.0f - juce::MathConstants<float>::twoPi * 5.0f * dt;
    }

    void reset()
    {
        light = gFast = gSlow = memory = 0;
        for (int c = 0; c < 2; ++c) lastAtt[c] = emphLp[c] = lf[c] = hf[c] = dcX[c] = dcY[c] = 0;
        makeup.setCurrentAndTargetValue (makeup.getTargetValue());
        scGain.setCurrentAndTargetValue (scGain.getTargetValue());
    }

    // peakReduction et gain : 0..100 comme les cadrans ; emphasis 0..100.
    void setParameters (float peakReduction, float gain, bool limitMode, float emphasis)
    {
        const float pr = juce::jlimit (0.0f, 100.0f, peakReduction);
        scGain.setTargetValue (pr <= 0.0f ? 0.0f : juce::Decibels::decibelsToGain (-34.0f + pr * 0.62f) * (limitMode ? 2.2f : 1.0f));
        makeup.setTargetValue (juce::Decibels::decibelsToGain ((gain - 40.0f) * 0.6f));
        limit = limitMode;
        emph = emphasis * 0.01f * 3.0f;
    }

    float process (juce::dsp::AudioBlock<float>& block) noexcept
    {
        const int n = (int) block.getNumSamples();
        const int chs = juce::jmin (numChannels, (int) block.getNumChannels());
        float* ch[2] { block.getChannelPointer (0), chs > 1 ? block.getChannelPointer (1) : nullptr };
        const float gamma = limit ? 1.45f : 1.0f;
        float maxGr = 0;

        for (int i = 0; i < n; ++i)
        {
            const float sg = scGain.getNextValue(), mk = makeup.getNextValue();

            // Commande : sortie de l'atténuateur (échantillon précédent), aigus accentués, redressée, couplée.
            float drive = 0;
            for (int c = 0; c < chs; ++c)
            {
                emphLp[c] = emphCoef * emphLp[c] + (1.0f - emphCoef) * lastAtt[c];
                const float s = lastAtt[c] + emph * (lastAtt[c] - emphLp[c]);
                drive = juce::jmax (drive, std::abs (s));
            }
            drive *= sg;

            // Panneau électroluminescent : seuil d'allumage puis persistance.
            const float elTarget = juce::jmax (0.0f, drive - 0.015f);
            light = (elTarget > light ? elAtt : elRel) * (light - elTarget) + elTarget;

            // Photorésistance : conductance proportionnelle à la lumière (plus raide en Limit).
            const float target = 4.5f * (gamma == 1.0f ? light : std::pow (light, gamma));
            gFast = (target > gFast ? fastAtt : fastRel) * (gFast - target) + target;
            // Partie lente : se charge sous compression soutenue, relâche d'autant plus lentement
            // que la compression a duré (mémoire de la cellule).
            if (target > gSlow) gSlow = slowAtt * (gSlow - target) + target;
            else gSlow -= (gSlow - target) * dt / (0.5f + 4.5f * memory);
            memory += target > 0.5f ? (1.0f - memory) * dt * 0.2f : -memory * dt * 0.05f;

            const float conductance = 0.62f * gFast + 0.38f * gSlow;
            const float g = 1.0f / (1.0f + conductance);
            maxGr = juce::jmax (maxGr, conductance);

            for (int c = 0; c < chs; ++c)
            {
                const float att = ch[c][i] * g;
                lastAtt[c] = att;
                float y = tube (att * mk);
                // Transformateur : les graves saturent un peu avant le reste.
                lf[c] = lfCoef * lf[c] + (1.0f - lfCoef) * y;
                y += 0.9f * std::tanh (lf[c] / 0.9f) - lf[c];
                hf[c] = hfCoef * hf[c] + (1.0f - hfCoef) * y;   // bande passante finie du matériel
                const float out = hf[c] - dcX[c] + dcCoef * dcY[c];
                dcX[c] = hf[c]; dcY[c] = out;
                ch[c][i] = out;
            }
        }
        return 20.0f * std::log10 (1.0f + maxGr);
    }

private:
    static float tube (float y) noexcept
    {
        constexpr float C = 1.4f, b = 0.20f;
        const float t0 = std::tanh (b / C);
        return (C * std::tanh ((y + b) / C) - C * t0) / (1.0f - t0 * t0);
    }

    double fs = 44100.0;
    int numChannels = 2;
    float dt = 1.0f / 44100.0f;
    bool limit = false;
    float emph = 0;
    float elAtt = 0, elRel = 0, fastAtt = 0, fastRel = 0, slowAtt = 0;
    float emphCoef = 0, lfCoef = 0, hfCoef = 0, dcCoef = 0.999f;
    float light = 0, gFast = 0, gSlow = 0, memory = 0;
    float lastAtt[2] {}, emphLp[2] {}, lf[2] {}, hf[2] {}, dcX[2] {}, dcY[2] {};
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> makeup { 1.0f };
    juce::SmoothedValue<float> scGain { 0.0f };
};
} // namespace swell
