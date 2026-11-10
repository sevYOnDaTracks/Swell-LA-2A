#include "OptoProcessor.h"
#include "OptoEditor.h"

using namespace juce;

OptoProcessor::Layout OptoProcessor::createLayout()
{
    Layout l;
    auto dial = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1); });
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "peak", 1 }, "Peak Reduction", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 30.0f, dial));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "gain", 1 }, "Gain", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 40.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                  { const float db = (v - 40.0f) * 0.6f; return String (v, 1) + "  (" + (db > 0 ? "+" : "") + String (db, 1) + " dB)"; })));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode", StringArray { "Compress", "Limit" }, 0));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "emphasis", 1 }, "HF Emphasis", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, dial));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "meter", 1 }, "Meter", StringArray { "Gain Reduction", "+10", "+4" }, 0));
    addCommonParameters (l);
    return l;
}

OptoProcessor::OptoProcessor() : AnalogProcessorBase (createLayout())
{
    peak = apvts.getRawParameterValue ("peak");
    gain = apvts.getRawParameterValue ("gain");
    mode = apvts.getRawParameterValue ("mode");
    emphasis = apvts.getRawParameterValue ("emphasis");
}

// Presets d'usine. Mode : 0 = Compress, 1 = Limit. Gain 40 = niveau inchangé (6 points = 3,6 dB).
const std::vector<OptoProcessor::FactoryPreset>& OptoProcessor::factoryPresets() const
{
    static const std::vector<FactoryPreset> list {
        { "Init",        "Default",               {} },
        { "Vocals",      "Vocal Leveling",        { { "peak", 45 }, { "gain", 46 }, { "emphasis", 20 } } },
        { "Vocals",      "Vocal Silk",            { { "peak", 35 }, { "gain", 44 }, { "emphasis", 40 } } },
        { "Vocals",      "Rap Vocal Forward",     { { "peak", 55 }, { "gain", 50 }, { "emphasis", 30 } } },
        { "Vocals",      "De-Ess Leveler",        { { "peak", 45 }, { "gain", 46 }, { "emphasis", 90 } } },
        { "Drums",       "Drum Bus Glue",         { { "peak", 30 }, { "gain", 42 } } },
        { "Drums",       "Drum Parallel Squash",  { { "peak", 75 }, { "gain", 55 }, { "mode", 1 }, { "mix", 40 } } },
        { "Drums",       "Kick Thump",            { { "peak", 40 }, { "gain", 45 }, { "mode", 1 } } },
        { "Bass",        "Bass DI Smooth",        { { "peak", 50 }, { "gain", 48 } } },
        { "Bass",        "Bass Limit",            { { "peak", 60 }, { "gain", 50 }, { "mode", 1 } } },
        { "Instruments", "Acoustic Guitar",       { { "peak", 35 }, { "gain", 43 }, { "emphasis", 10 } } },
        { "Instruments", "Electric Sustain",      { { "peak", 60 }, { "gain", 50 } } },
        { "Instruments", "Piano Even",            { { "peak", 32 }, { "gain", 43 }, { "emphasis", 15 } } },
        { "Instruments", "Strings & Pads",        { { "peak", 40 }, { "gain", 45 } } },
        { "Instruments", "Brass Tame",            { { "peak", 50 }, { "gain", 47 }, { "mode", 1 }, { "emphasis", 30 } } },
        { "Bus",         "Mix Bus Gentle",        { { "peak", 18 }, { "gain", 41 } } },
    };
    return list;
}

float OptoProcessor::processEngine (dsp::AudioBlock<float>& block)
{
    engine.setParameters (peak->load(), gain->load(), mode->load() > 0.5f, emphasis->load());
    return engine.process (block);
}

AudioProcessorEditor* OptoProcessor::createEditor() { return swell::LicenceGuard::attach (new OptoEditor (*this)); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new OptoProcessor(); }
