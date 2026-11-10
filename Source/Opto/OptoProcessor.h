#pragma once
#include "Common/AnalogProcessorBase.h"
#include "OptoEngine.h"

class OptoProcessor : public swell::AnalogProcessorBase
{
public:
    OptoProcessor();
    const juce::String getName() const override { return "Swell Opto 2"; }

    const std::vector<FactoryPreset>& factoryPresets() const override;
    juce::String productName() const override { return getName(); }

protected:
    void prepareEngine (double rate, int channels) override { engine.prepare (rate, channels); }
    void resetEngine() override { engine.reset(); }
    float processEngine (juce::dsp::AudioBlock<float>&) override;

private:
    juce::AudioProcessorEditor* createEditor() override;
    static Layout createLayout();

    swell::OptoEngine engine;
    std::atomic<float> *peak, *gain, *mode, *emphasis;
};
