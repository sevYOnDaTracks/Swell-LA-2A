#include "AnalogProcessorBase.h"

namespace swell
{
using namespace juce;

void AnalogProcessorBase::addCommonParameters (Layout& layout, float defaultMix)
{
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix", NormalisableRange<float> (0.0f, 100.0f, 0.1f), defaultMix,
                                                       AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "power", 1 }, "Power", true));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "oversampling", 1 }, "Oversampling", StringArray { "Off", "2x", "4x" }, 1));
}

AnalogProcessorBase::AnalogProcessorBase (Layout layout)
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", std::move (layout))
{
    mixParam = apvts.getRawParameterValue ("mix");
    powerParam = apvts.getRawParameterValue ("power");
    osParam = apvts.getRawParameterValue ("oversampling");
}

bool AnalogProcessorBase::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    return in == out && (out == AudioChannelSet::mono() || out == AudioChannelSet::stereo());
}

void AnalogProcessorBase::prepareToPlay (double sampleRate, int maxBlock)
{
    baseRate = sampleRate;
    const auto channels = (size_t) jmax (1, getTotalNumOutputChannels());
    for (size_t i = 0; i < oversamplers.size(); ++i)
    {
        if (i == 0) { oversamplers[i].reset(); continue; }
        oversamplers[i] = std::make_unique<dsp::Oversampling<float>> (channels, i, dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
        oversamplers[i]->initProcessing ((size_t) maxBlock);
    }
    mixer.prepare ({ sampleRate, (uint32) maxBlock, (uint32) channels });
    mixer.setMixingRule (dsp::DryWetMixingRule::linear);
    currentOs = -1;
    selectOversampling ((int) osParam->load());
    mixer.reset();
    outEnv = inEnv = 0;
}

void AnalogProcessorBase::selectOversampling (int index)
{
    index = jlimit (0, 2, index);
    if (index == currentOs) return;
    currentOs = index;
    const double factor = (double) (1 << index);
    prepareEngine (baseRate * factor, jmax (1, getTotalNumOutputChannels()));
    resetEngine();
    float latency = 0;
    if (auto& os = oversamplers[(size_t) index])
    {
        os->reset();
        latency = os->getLatencyInSamples();   // entière grâce à l'option de compensation
    }
    mixer.setWetLatency (latency);
    setLatencySamples (roundToInt (latency));
}

void AnalogProcessorBase::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int numCh = jmin (buffer.getNumChannels(), getTotalNumOutputChannels());
    for (int c = getTotalNumInputChannels(); c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());
    if (numCh == 0 || buffer.getNumSamples() == 0) return;
    if (! licence->isUnlocked())
    {
        buffer.clear();
        meterGainReductionDb.store (0.0f);
        meterOutputRms.store (0.0f);
        meterInputRms.store (0.0f);
        return;
    }

    selectOversampling ((int) osParam->load());
    const bool on = powerParam->load() > 0.5f;
    mixer.setWetMixProportion (on ? mixParam->load() * 0.01f : 0.0f);

    auto block = dsp::AudioBlock<float> (buffer).getSubsetChannelBlock (0, (size_t) numCh);
    const int n = buffer.getNumSamples();

    float inSum = 0;
    for (int c = 0; c < numCh; ++c)
    {
        const float* d = buffer.getReadPointer (c);
        for (int i = 0; i < n; ++i) inSum += std::abs (d[i]);
    }

    mixer.pushDrySamples (block);
    float gr = 0;
    if (auto& os = oversamplers[(size_t) currentOs])
    {
        auto up = os->processSamplesUp (block);
        gr = processEngine (up);
        os->processSamplesDown (block);
    }
    else
    {
        gr = processEngine (block);
    }
    mixer.mixWetSamples (block);

    float outSum = 0;
    for (int c = 0; c < numCh; ++c)
    {
        const float* d = buffer.getReadPointer (c);
        for (int i = 0; i < n; ++i) outSum += std::abs (d[i]);
    }
    // Valeur moyenne redressée x 1,11 = RMS pour une sinusoïde, comme un vrai VU. Lissage ~10 ms.
    const float a = std::exp (-(float) n / (0.010f * (float) getSampleRate()));
    inEnv = a * inEnv + (1 - a) * inSum / (float) (n * numCh) * 1.1107f;
    outEnv = a * outEnv + (1 - a) * outSum / (float) (n * numCh) * 1.1107f;
    meterInputRms.store (inEnv);
    meterOutputRms.store (outEnv);
    meterGainReductionDb.store (on ? gr : 0.0f);
}

//==============================================================================
const String AnalogProcessorBase::getProgramName (int index)
{
    const auto& list = factoryPresets();
    return isPositiveAndBelow (index, (int) list.size()) ? String (list[(size_t) index].name) : String();
}

void AnalogProcessorBase::setPresetName (const String& name)
{
    apvts.state.setProperty ("presetName", name, nullptr);
}

String AnalogProcessorBase::getPresetName() const
{
    return apvts.state.getProperty ("presetName", "Default").toString();
}

void AnalogProcessorBase::loadFactoryPreset (int index)
{
    const auto& list = factoryPresets();
    if (! isPositiveAndBelow (index, (int) list.size())) return;
    const auto& preset = list[(size_t) index];
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (p); rp != nullptr && isSoundParameter (rp->getParameterID()))
        {
            float value = rp->convertFrom0to1 (rp->getDefaultValue());
            for (auto& [id, v] : preset.values)
                if (rp->getParameterID() == id) value = v;
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (rp->convertTo0to1 (value));
            rp->endChangeGesture();
        }
    currentFactory = index;
    setPresetName (preset.name);
    updateHostDisplay (ChangeDetails().withProgramChanged (true));
}

File AnalogProcessorBase::userPresetFolder() const
{
    return File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("Swell Audio").getChildFile (productName() + " Presets");
}

Array<File> AnalogProcessorBase::userPresets() const
{
    auto files = userPresetFolder().findChildFiles (File::findFiles, false, String ("*") + presetExtension);
    files.sort();
    return files;
}

bool AnalogProcessorBase::saveUserPreset (const File& file)
{
    XmlElement xml ("SwellPreset");
    xml.setAttribute ("plugin", productName());
    xml.setAttribute ("name", file.getFileNameWithoutExtension());
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (p); rp != nullptr && isSoundParameter (rp->getParameterID()))
        {
            auto* e = xml.createNewChildElement ("PARAM");
            e->setAttribute ("id", rp->getParameterID());
            e->setAttribute ("value", (double) rp->convertFrom0to1 (rp->getValue()));
        }
    file.getParentDirectory().createDirectory();
    if (! xml.writeTo (file)) return false;
    currentFactory = -1;
    setPresetName (file.getFileNameWithoutExtension());
    return true;
}

bool AnalogProcessorBase::loadUserPreset (const File& file)
{
    auto xml = parseXMLIfTagMatches (file, "SwellPreset");
    if (xml == nullptr || xml->getStringAttribute ("plugin") != productName()) return false;
    for (auto* e : xml->getChildWithTagNameIterator ("PARAM"))
        if (auto* rp = apvts.getParameter (e->getStringAttribute ("id")); rp != nullptr && isSoundParameter (rp->getParameterID()))
        {
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (rp->convertTo0to1 ((float) e->getDoubleAttribute ("value")));
            rp->endChangeGesture();
        }
    currentFactory = -1;
    setPresetName (xml->getStringAttribute ("name", file.getFileNameWithoutExtension()));
    return true;
}

float AnalogProcessorBase::getUiScale() const
{
    return (float) apvts.state.getProperty ("uiScale", 1.0f);
}

void AnalogProcessorBase::setUiScale (float s)
{
    apvts.state.setProperty ("uiScale", s, nullptr);
}

void AnalogProcessorBase::getStateInformation (MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void AnalogProcessorBase::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (ValueTree::fromXml (*xml));
            currentFactory = -1;
            const auto& list = factoryPresets();
            for (size_t i = 0; i < list.size(); ++i)
                if (getPresetName() == list[i].name) currentFactory = (int) i;
        }
}
} // namespace swell
