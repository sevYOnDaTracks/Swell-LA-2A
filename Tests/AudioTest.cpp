// Tests audio : stabilite, compression effective, transparence (mix 0 et bypass, latence compensee),
// trois facteurs de surechantillonnage, reglages extremes, sauvegarde d'etat.
#include "Opto/OptoProcessor.h"
using Proc = OptoProcessor;
static const char* name = "Opto 2";

static int failures = 0;
static void check (bool ok, const juce::String& what)
{
    std::printf ("  [%s] %s\n", ok ? " ok " : "FAIL", what.toRawUTF8());
    if (! ok) ++failures;
}

static void setParam (Proc& p, const char* id, float value)
{
    auto* param = p.apvts.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (value));
}

struct Result { float maxAbs = 0, gr = 0, maxDiff = 0; bool finite = true; };

static Result run (Proc& p, float amplitude, bool noise, double seconds = 1.5)
{
    constexpr int bs = 256;
    constexpr double sr = 48000;
    p.prepareToPlay (sr, bs);
    juce::AudioBuffer<float> b (2, bs);
    juce::MidiBuffer midi;
    juce::Random rng (11);
    Result r;
    const int latency = p.getLatencySamples();
    std::vector<float> history;
    const int blocks = (int) (seconds * sr / bs);
    for (int k = 0; k < blocks; ++k)
    {
        for (int n = 0; n < bs; ++n)
        {
            const int t = k * bs + n;
            const float s = amplitude * (noise ? rng.nextFloat() * 2 - 1 : std::sin (6.2831853f * 1000.0f * (float) t / (float) sr));
            history.push_back (s);
            b.setSample (0, n, s);
            b.setSample (1, n, s);
        }
        p.processBlock (b, midi);
        for (int n = 0; n < bs; ++n)
        {
            const float y = b.getSample (0, n);
            r.finite &= std::isfinite (y);
            r.maxAbs = juce::jmax (r.maxAbs, std::abs (y));
            const int src = k * bs + n - latency;
            if (k > blocks / 2 && src >= 0) r.maxDiff = juce::jmax (r.maxDiff, std::abs (y - history[(size_t) src]));
        }
        r.gr = p.meterGainReductionDb.load();
    }
    return r;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    std::printf ("Swell %s\n", name);

    {
        // Sans fichier de licence (slug neuf sur cette machine) : silence total.
        Proc p;
        auto r = run (p, 0.5f, false, 0.2);
        const bool hasLicence = p.licence->isUnlocked();
        check (hasLicence || (r.maxAbs == 0.0f && r.finite), hasLicence ? juce::String ("licence presente sur cette machine : test du silence ignore")
                                                                         : juce::String ("sans licence : silence"));
    }
    swell::Licence::unlockForTesting(); // la suite teste l'audio, sans fichier de licence

    for (int os = 0; os < 3; ++os)
    {
        Proc p;
        setParam (p, "oversampling", (float) os);
        setParam (p, "peak", 60.0f);
        auto r = run (p, 0.5f, false);
        check (r.finite && r.maxAbs < 4.0f, "oversampling " + juce::String (1 << os) + "x : sortie finie (crete " + juce::String (r.maxAbs, 2) + ")");
        check (r.gr > 3.0f, "oversampling " + juce::String (1 << os) + "x : compresse (" + juce::String (r.gr, 1) + " dB de reduction)");
    }

    {
        Proc p;
        setParam (p, "mix", 0.0f);
        auto r = run (p, 0.3f, true);
        check (r.maxDiff < 1.0e-3f, "mix 0 % : signal sec aligne sur la latence (ecart " + juce::String (r.maxDiff, 6) + ")");
    }
    {
        Proc p;
        setParam (p, "power", 0.0f);
        auto r = run (p, 0.3f, true);
        check (r.maxDiff < 1.0e-3f && r.gr == 0.0f, "alimentation coupee : transparent (ecart " + juce::String (r.maxDiff, 6) + ")");
    }
    {
        Proc p;
        setParam (p, "peak", 100.0f); setParam (p, "gain", 100.0f); setParam (p, "mode", 1.0f); setParam (p, "emphasis", 100.0f);
        setParam (p, "oversampling", 2.0f);
        auto r = run (p, 1.0f, true);
        check (r.finite && r.maxAbs < 8.0f, "reglages extremes + bruit plein niveau : stable (crete " + juce::String (r.maxAbs, 2) + ")");
    }
    {
        Proc a, b;
        setParam (a, "mix", 42.0f);
        juce::MemoryBlock state;
        a.getStateInformation (state);
        b.setStateInformation (state.getData(), (int) state.getSize());
        check (std::abs (b.apvts.getRawParameterValue ("mix")->load() - 42.0f) < 0.01f, "sauvegarde / rappel de l'etat");
    }

    {
        // Presets d'usine : chacun se charge, reste stable et ne touche ni Power ni le suréchantillonnage.
        Proc p;
        setParam (p, "oversampling", 2.0f);
        bool allOk = true;
        for (int i = 0; i < p.getNumPrograms(); ++i)
        {
            p.setCurrentProgram (i);
            auto r = run (p, 0.5f, true, 0.3);
            allOk &= r.finite && r.maxAbs < 4.0f && p.getPresetName() == p.getProgramName (i)
                     && p.apvts.getRawParameterValue ("oversampling")->load() > 1.5f;
        }
        check (allOk && p.getNumPrograms() >= 15, juce::String (p.getNumPrograms()) + " presets d'usine : chargement et stabilite");
    }
    {
        // Preset perso : enregistrer puis recharger rend les memes reglages.
        Proc a, b;
        a.setCurrentProgram (3);
        setParam (a, "mix", 63.0f);
        auto file = juce::File::createTempFile (swell::AnalogProcessorBase::presetExtension);
        const bool saved = a.saveUserPreset (file);
        const bool loaded = b.loadUserPreset (file);
        bool same = true;
        for (auto* prm : a.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
                same &= std::abs (rp->getValue() - b.apvts.getParameter (rp->getParameterID())->getValue()) < 1.0e-4f;
        file.deleteFile();
        check (saved && loaded && same && b.getPresetName() == file.getFileNameWithoutExtension(), "preset perso : enregistrer / recharger");
    }

    std::printf (failures == 0 ? "Tout est bon.\n" : "%d echec(s).\n", failures);
    return failures == 0 ? 0 : 1;
}
