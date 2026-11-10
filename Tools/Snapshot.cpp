// Rendu hors ligne de l'interface en PNG : Snapshot<plugin> <sortie.png> [échelle] [locked]
// « locked » montre l'écran d'activation au lieu de débloquer la licence.
// Le processeur reçoit d'abord un signal de batterie synthétique pour que l'aiguille indique une vraie mesure.
#include "Opto/OptoProcessor.h"
#include "Opto/OptoEditor.h"
using Proc = OptoProcessor; using Editor = OptoEditor;

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const bool locked = argc > 3 && juce::String (argv[3]) == "locked";
    if (! locked) swell::Licence::unlockForTesting(); // rendu hors ligne, sans fichier de licence
    Proc p;
    p.setCurrentProgram (3);   // « Vocal Aggressive » / « Rap Vocal Forward »
    p.prepareToPlay (48000, 256);
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> b (2, 256);
    juce::Random rng (3);
    float env = 0;
    for (int blockIndex = 0; blockIndex < 400; ++blockIndex)
    {
        for (int n = 0; n < 256; ++n)
        {
            const int t = blockIndex * 256 + n;
            if (t % 12000 == 0) env = 1.0f;
            env *= 0.9995f;
            const float s = (0.5f * std::sin ((float) t * 0.0131f) + 0.5f * rng.nextFloat() - 0.25f) * env * 0.7f;
            b.setSample (0, n, s);
            b.setSample (1, n, s);
        }
        p.processBlock (b, midi);
    }
    const float scale = argc > 2 ? juce::String (argv[2]).getFloatValue() : 2.0f;
    std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditorAndMakeActive());
    if (auto* ed = dynamic_cast<Editor*> (e.get())) ed->settleMeters();
    // Capture propre : sans le badge de licence (vide hors ligne) ni le coin de redimensionnement.
    std::function<void (juce::Component&)> tidy = [&] (juce::Component& c)
    {
        for (auto* child : c.getChildren())
        {
            if (! locked && (dynamic_cast<swell::LicenceBadge*> (child) != nullptr || dynamic_cast<juce::ResizableCornerComponent*> (child) != nullptr))
                child->setVisible (false);
            tidy (*child);
        }
    };
    tidy (*e);
    auto image = e->createComponentSnapshot (e->getLocalBounds(), true, scale);
    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "snapshot.png"));
    out.deleteFile();
    juce::FileOutputStream s (out);
    juce::PNGImageFormat().writeImageToStream (image, s);
    std::printf ("%s  GR %.1f dB\n", out.getFullPathName().toRawUTF8(), (double) p.meterGainReductionDb.load());
    p.editorBeingDeleted (e.get());
    e.reset();
    return 0;
}
