#pragma once
#include "Common/AnalogEditor.h"
#include "OptoProcessor.h"

class OptoEditor : public swell::analog::AnalogEditor
{
public:
    explicit OptoEditor (OptoProcessor&);
    ~OptoEditor() override { sliderAttachments.clear(); }   // les boutons meurent avant la classe de base
    void settleMeters() { updateMeter(); meter.jumpToTarget(); }   // captures hors ligne

private:
    void drawFacePlate (juce::Graphics&);
    void updateMeter();

    OptoProcessor& opto;
    using Knob = swell::analog::Knob;
    Knob gain { Knob::Style::OptoLarge }, peak { Knob::Style::OptoLarge };
    Knob emphasis { Knob::Style::OptoSmall }, mix { Knob::Style::OptoSmall };
    Knob meterSelect { Knob::Style::ChickenHead };
    swell::analog::BatToggle mode, power;
    swell::analog::PilotLamp lamp { juce::Colour (0xffe8321e) };
    swell::analog::VuMeter meter;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;
    swell::LicenceBadge badge { juce::Colour (0x991f1e1c), juce::Colour (0xff1f1e1c), juce::Justification::centredRight };
    swell::analog::PresetBar presets { proc, false };
    juce::VBlankAttachment vblank;
};
