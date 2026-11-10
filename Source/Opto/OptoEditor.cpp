#include "OptoEditor.h"

using namespace juce;
using namespace swell::analog;

namespace
{
constexpr int W = 1100, H = 380;
const Colour ink (0xff1f1e1c);
const Rectangle<int> meterArea (405, 58, 290, 232);
const Point<int> gainC { 228, 192 }, peakC { 872, 192 };
const Point<int> selC { 470, 338 }, modeC { 560, 336 }, powerC { 640, 336 }, lampC { 684, 336 };
const Point<int> emphC { 330, 334 }, mixC { 770, 334 };

VuMeter::Style meterStyle()
{
    VuMeter::Style s;
    s.face = Colour (0xffeee0b8);
    s.glow = Colour (0xffffc86a);
    s.glowAmount = 0.50f;
    s.caption = "SWELL OPTO";
    return s;
}

const Image& greyPaint()
{
    static const Image img = []
    {
        auto i = paintedPanel (W * 2, H * 2, Colour (0xffb6b5ae), 0.55f, 0x2a);
        return i;
    }();
    return img;
}
}

OptoEditor::OptoEditor (OptoProcessor& p)
    : AnalogEditor (p, W, H), opto (p), meter (meterStyle()),
      vblank (this, [this] { updateMeter(); })
{
    panel.drawStatic = [this] (Graphics& g) { drawFacePlate (g); };

    attach (gain, "gain", 176, gainC);
    attach (peak, "peak", 176, peakC);
    attach (emphasis, "emphasis", 62, emphC);
    attach (mix, "mix", 62, mixC);
    meterSelect.setRotaryParameters (MathConstants<float>::twoPi - 0.85f, MathConstants<float>::twoPi + 0.85f, true);
    attach (meterSelect, "meter", 64, selC);

    panel.addAndMakeVisible (meter);
    meter.setBounds (meterArea);

    for (auto [button, id, centre] : { std::tuple { &mode, "mode", modeC }, std::tuple { &power, "power", powerC } })
    {
        panel.addAndMakeVisible (*button);
        button->setBounds (Rectangle<int> (56, 56).withCentre (centre));
        buttonAttachments.push_back (std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, id, *button));
    }
    panel.addAndMakeVisible (lamp);
    lamp.setBounds (Rectangle<int> (36, 36).withCentre (lampC));

    panel.addAndMakeVisible (presets);
    presets.setBounds (420, 16, 260, 30);
    panel.addAndMakeVisible (badge);
    badge.setBounds (790, 350, 230, 18);

    restoreSize();
}

void OptoEditor::updateMeter()
{
    presets.refresh();
    const bool on = power.getToggleState();
    lamp.setLit (on);
    meter.setLit (on);
    const int sel = roundToInt (meterSelect.getValue());
    if (! on) { meter.setTargetDb (-100.0f); return; }
    if (sel == 0) { meter.setTargetDb (-opto.meterGainReductionDb.load()); return; }
    const float rmsDb = Decibels::gainToDecibels (opto.meterOutputRms.load(), -100.0f);
    meter.setTargetDb (rmsDb + (sel == 2 ? 18.0f : 12.0f));   // +4 : 0 VU = -18 dBFS, +10 : 0 VU = -12 dBFS
}

void OptoEditor::drawFacePlate (Graphics& g)
{
    const auto full = Rectangle<float> (0, 0, (float) W, (float) H);
    g.drawImage (greyPaint(), full, RectanglePlacement::stretchToFit);
    drawPanelLight (g, full, 0.9f);
    drawBevelEdge (g, full, 0, 1.0f);
    drawRackEar (g, { 0, 0, 52, (float) H }, Colour (0xffb6b5ae), false);
    drawRackEar (g, { (float) W - 52, 0, 52, (float) H }, Colour (0xffb6b5ae), false);

    Path cut;
    cut.addRoundedRectangle (meterArea.toFloat(), 12.0f);
    softShadow (g, cut, Colours::black.withAlpha (0.65f), 12.0f, { 2, 6 });

    drawPrinted (g, "S W E L L", { 72, 16, 200, 30 }, panelFont (24, true), ink, Justification::centredLeft);
    g.setColour (ink.withAlpha (0.8f));
    g.fillRect (72.0f, 47.0f, 150.0f, 1.3f);
    drawPrinted (g, "OPTO 2", { 230, 16, 140, 30 }, panelFont (24, false), ink, Justification::centredLeft);
    drawPrinted (g, "LEVELING AMPLIFIER  -  SWELL-AUDIO FRANCE", { 620, 22, 400, 20 }, panelFont (12, true), ink.withAlpha (0.85f), Justification::centredRight);

    ScaleSpec big;
    big.radius = 80; big.tickLen = 8; big.labelRadius = 100; big.start = -2.36f; big.end = 2.36f;
    big.minorPerMajor = 2; big.fontSize = 11.5f; big.ink = ink;
    big.labels = { "0", "10", "20", "30", "40", "50", "60", "70", "80", "90", "100" };
    big.centre = gainC.toFloat(); drawKnobScale (g, big);
    big.centre = peakC.toFloat(); drawKnobScale (g, big);
    drawPrinted (g, "GAIN", Rectangle<float> (200, 20).withCentre (gainC.toFloat().translated (0, 112)), panelFont (14, true), ink);
    drawPrinted (g, "PEAK REDUCTION", Rectangle<float> (220, 20).withCentre (peakC.toFloat().translated (0, 112)), panelFont (14, true), ink);

    ScaleSpec sm;
    sm.radius = 28; sm.tickLen = 4; sm.labelRadius = 39; sm.start = -2.36f; sm.end = 2.36f;
    sm.fontSize = 8; sm.ink = ink;
    sm.labels = { "0", "", "", "", "10" };
    sm.centre = emphC.toFloat(); drawKnobScale (g, sm);
    sm.labels = { "DRY", "", "", "", "WET" };
    sm.centre = mixC.toFloat(); drawKnobScale (g, sm);
    drawPrinted (g, "EMPHASIS", Rectangle<float> (120, 14).withCentre (emphC.toFloat().translated (-82, 0)), panelFont (10, true), ink);
    drawPrinted (g, "MIX", Rectangle<float> (80, 14).withCentre (mixC.toFloat().translated (68, 0)), panelFont (10, true), ink);

    // Sélecteur du vu-mètre : repères imprimés à chaque cran.
    const char* sel[] { "GAIN RED.", "+10", "+4" };
    for (int i = 0; i < 3; ++i)
    {
        const float a = -0.85f + 0.85f * (float) i;
        const auto c = selC.toFloat();
        g.setColour (ink);
        g.drawLine ({ c.getPointOnCircumference (30, a), c.getPointOnCircumference (35, a) }, 1.4f);
        drawPrinted (g, sel[i], Rectangle<float> (60, 12).withCentre (c.getPointOnCircumference (42, a)), panelFont (8, true), ink);
    }
    drawPrinted (g, "METER", Rectangle<float> (80, 12).withCentre (selC.toFloat().translated (0, 30)), panelFont (9, true), ink);
    drawPrinted (g, "LIMIT", Rectangle<float> (80, 12).withCentre (modeC.toFloat().translated (0, -36)), panelFont (9, true), ink);
    drawPrinted (g, "COMPRESS", Rectangle<float> (80, 12).withCentre (modeC.toFloat().translated (0, 36)), panelFont (9, true), ink);
    drawPrinted (g, "ON", Rectangle<float> (80, 12).withCentre (powerC.toFloat().translated (0, -36)), panelFont (9, true), ink);
    drawPrinted (g, "POWER", Rectangle<float> (80, 12).withCentre (powerC.toFloat().translated (0, 36)), panelFont (9, true), ink);

    drawPrinted (g, "TUBE  -  OPTICAL ATTENUATOR", { 72, 352, 260, 14 }, panelFont (8, true), ink.withAlpha (0.55f), Justification::centredLeft);
}
