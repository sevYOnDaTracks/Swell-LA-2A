#include "AnalogComponents.h"

namespace swell::analog
{
using namespace juce;

const Image& grainTile()
{
    static const Image tile = []
    {
        Image img (Image::ARGB, 256, 256, true);
        Rng r (77);
        Image::BitmapData d (img, Image::BitmapData::writeOnly);
        for (int y = 0; y < 256; ++y)
            for (int x = 0; x < 256; ++x)
                d.setPixelColour (x, y, (r.next() < 0.5f ? Colours::white : Colours::black).withAlpha (r.next() * 0.35f));
        return img;
    }();
    return tile;
}

//==============================================================================
AnalogLookAndFeel::AnalogLookAndFeel()
{
    setColour (TooltipWindow::textColourId, Colour (0xfff2ead6));
    setColour (BubbleComponent::backgroundColourId, Colour (0xee151413));
    setColour (BubbleComponent::outlineColourId, Colour (0xff5a5650));
}

void AnalogLookAndFeel::drawBubble (Graphics& g, BubbleComponent&, const Point<float>&, const Rectangle<float>& body)
{
    Path p;
    p.addRoundedRectangle (body, 4.0f);
    softShadow (g, p, Colours::black.withAlpha (0.5f), 5.0f, { 0, 2 });
    g.setColour (Colour (0xee151413));
    g.fillPath (p);
    g.setColour (Colour (0xff5a5650));
    g.strokePath (p, PathStrokeType (1.0f));
}

//==============================================================================
Knob::Knob (Style s) : style (s)
{
    setSliderStyle (Slider::RotaryVerticalDrag);
    setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (MathConstants<float>::twoPi - 2.36f, MathConstants<float>::twoPi + 2.36f, true);
    setPopupDisplayEnabled (true, false, nullptr);
    setMouseCursor (MouseCursor::PointingHandCursor);
    setMouseDragSensitivity (220);
}

void Knob::mouseDown (const MouseEvent& e)
{
    // Shift au clic : réglage fin pour tout le geste.
    setMouseDragSensitivity (e.mods.isShiftDown() ? 1100 : 220);
    Slider::mouseDown (e);
}

namespace
{
struct KnobGeo { bool skirt; int ridges; float grip, face; };
KnobGeo geoFor (Knob::Style s)
{
    switch (s)
    {
        case Knob::Style::FetLarge:  return { false, 44, 1.00f, 0.80f };
        case Knob::Style::FetSmall:  return { false, 30, 1.00f, 0.76f };
        case Knob::Style::OptoLarge: return { true, 18, 0.70f, 0.55f };
        case Knob::Style::OptoSmall: return { true, 14, 0.72f, 0.56f };
        case Knob::Style::ChickenHead: break;
    }
    return { false, 0, 1, 1 };
}
}

void Knob::paint (Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto c = bounds.getCentre();
    const float R = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f * 0.84f;
    const auto rp = getRotaryParameters();
    const float angle = rp.startAngleRadians + (float) valueToProportionOfLength (getValue()) * (rp.endAngleRadians - rp.startAngleRadians);

    if (style == Style::ChickenHead) { paintChickenHead (g, c, R, angle); return; }

    const auto geo = geoFor (style);
    const float gripR = R * geo.grip, faceR = R * geo.face;
    auto circle = [&] (float r) { Path p; p.addEllipse (c.x - r, c.y - r, r * 2, r * 2); return p; };

    // 1. Ombres portées, jupe et corps sombre (cache).
    base.get (g, getLocalBounds(), [&] (Graphics& ig)
    {
        auto outer = circle (R);
        softShadow (ig, outer, Colours::black.withAlpha (0.55f), R * 0.28f, { R * 0.10f, R * 0.20f });
        softShadow (ig, outer, Colours::black.withAlpha (0.6f), R * 0.06f, { R * 0.02f, R * 0.05f });
        if (geo.skirt)
        {
            ColourGradient sk (Colour (0xff2e2e30), c.x - R * 0.6f, c.y - R * 0.7f, Colour (0xff050505), c.x + R * 0.7f, c.y + R * 0.8f, false);
            ig.setGradientFill (sk);
            ig.fillPath (outer);
            for (int i = 0; i < 8; ++i)   // moulage concentrique de la jupe
            {
                const float rr = gripR + (R - gripR) * ((float) i + 0.5f) / 8.0f;
                ig.setColour (Colours::white.withAlpha (i % 2 ? 0.035f : 0.0f));
                ig.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 0.7f);
            }
            ColourGradient rim (Colours::white.withAlpha (0.32f), c.x - R * 0.7f, c.y - R * 0.7f, Colours::transparentWhite, c.x, c.y, false);
            ig.setGradientFill (rim);
            ig.strokePath (circle (R - 0.6f), PathStrokeType (1.2f));
            auto body = circle (gripR);
            softShadow (ig, body, Colours::black.withAlpha (0.7f), R * 0.10f, { R * 0.04f, R * 0.08f });
        }
        ColourGradient gb (Colour (0xff1e1e20), c.x - gripR, c.y - gripR, Colour (0xff040404), c.x + gripR, c.y + gripR, false);
        ig.setGradientFill (gb);
        ig.fillPath (circle (gripR));
    });
    base.blit (g, bounds);

    // 2. Cannelures : elles tournent, mais leur éclairage dépend de leur orientation face à la lampe.
    {
        const int n = geo.ridges;
        const float r0 = faceR * 0.98f, r1 = gripR * 0.985f;
        const float width = MathConstants<float>::twoPi * r1 / (float) n * 0.34f;
        for (int i = 0; i < n; ++i)
        {
            const float th = angle + MathConstants<float>::twoPi * (float) i / (float) n;
            const float facing = std::cos (th + MathConstants<float>::pi * 0.25f);
            const float soft = geo.skirt ? 0.55f : 1.0f;   // les cannelures moulées de la bakélite brillent moins
            const float lit = (0.05f + 0.45f * std::pow (jmax (0.0f, facing), 2.0f) + 0.06f * jmax (0.0f, -facing)) * soft;
            g.setColour (Colours::white.withAlpha (lit));
            g.drawLine ({ c.getPointOnCircumference (r0, th), c.getPointOnCircumference (r1, th) }, width);
            const float th2 = th + MathConstants<float>::pi / (float) n;
            g.setColour (Colours::black.withAlpha (geo.skirt ? 0.35f : 0.55f));
            g.drawLine ({ c.getPointOnCircumference (r0, th2), c.getPointOnCircumference (r1, th2) }, width * 0.8f);
        }
    }

    // 3. Face supérieure usinée (cache).
    face.get (g, getLocalBounds(), [&] (Graphics& ig)
    {
        auto top = circle (faceR);
        softShadow (ig, top, Colours::black.withAlpha (0.5f), R * 0.05f, { 0, R * 0.03f });
        ColourGradient fg (Colour (0xff3b3b3e), c.x - faceR * 0.35f, c.y - faceR * 0.45f, Colour (0xff070708), c.x + faceR * 0.75f, c.y + faceR * 0.85f, true);
        ig.setGradientFill (fg);
        ig.fillPath (top);
        for (int i = 1; i < 14; ++i)
        {
            const float rr = faceR * (float) i / 14.0f;
            ig.setColour (Colours::white.withAlpha (i % 2 ? 0.028f : 0.0f));
            ig.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 0.6f);
        }
        ColourGradient rim (Colours::white.withAlpha (0.38f), c.x - faceR * 0.7f, c.y - faceR * 0.7f, Colours::transparentWhite, c.x + faceR * 0.1f, c.y + faceR * 0.1f, false);
        ig.setGradientFill (rim);
        ig.strokePath (circle (faceR - 0.5f), PathStrokeType (1.1f));
        ig.setColour (Colours::black.withAlpha (0.8f));
        ig.strokePath (circle (faceR + 0.4f), PathStrokeType (0.8f));
    });
    face.blit (g, bounds);

    // 4. Index blanc.
    {
        const float w = R * (geo.skirt ? 0.055f : 0.075f);
        const float a0 = geo.skirt ? gripR * 1.04f : faceR * 0.22f;
        const float a1 = geo.skirt ? R * 0.97f : faceR * 0.90f;
        const Line<float> l (c.getPointOnCircumference (a0, angle), c.getPointOnCircumference (a1, angle));
        Path p;
        p.startNewSubPath (l.getStart());
        p.lineTo (l.getEnd());
        g.setColour (Colours::black.withAlpha (0.7f));
        g.strokePath (p, PathStrokeType (w * 1.5f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (Colour (0xfff1ebdc));
        g.strokePath (p, PathStrokeType (w, PathStrokeType::curved, PathStrokeType::rounded));
        if (geo.skirt)
        {
            const auto dot = c.getPointOnCircumference (faceR * 0.62f, angle);
            g.fillEllipse (Rectangle<float> (w * 1.4f, w * 1.4f).withCentre (dot));
        }
    }

    // 5. Reflet spéculaire par-dessus tout (cache).
    gloss.get (g, getLocalBounds(), [&] (Graphics& ig)
    {
        ig.reduceClipRegion (circle (faceR));
        ColourGradient spec (Colours::white.withAlpha (0.30f), c.x - faceR * 0.32f, c.y - faceR * 0.45f,
                             Colours::transparentWhite, c.x - faceR * 0.32f + faceR * 0.75f, c.y - faceR * 0.45f, true);
        ig.setGradientFill (spec);
        ig.fillEllipse (c.x - faceR, c.y - faceR, faceR * 2, faceR * 2);
        ig.setTiledImageFill (grainTile(), 0, 0, 0.05f);
        ig.fillEllipse (c.x - faceR, c.y - faceR, faceR * 2, faceR * 2);
    });
    gloss.blit (g, bounds);
}

void Knob::paintChickenHead (Graphics& g, Point<float> c, float R, float angle)
{
    Path p;   // pointe vers le haut dans son repère
    p.startNewSubPath (0, -R);
    p.quadraticTo (R * 0.20f, -R * 0.95f, R * 0.26f, -R * 0.42f);
    p.quadraticTo (R * 0.46f, -R * 0.18f, R * 0.40f, R * 0.18f);
    p.quadraticTo (R * 0.34f, R * 0.62f, 0, R * 0.66f);
    p.quadraticTo (-R * 0.34f, R * 0.62f, -R * 0.40f, R * 0.18f);
    p.quadraticTo (-R * 0.46f, -R * 0.18f, -R * 0.26f, -R * 0.42f);
    p.quadraticTo (-R * 0.20f, -R * 0.95f, 0, -R);
    p.closeSubPath();
    p.applyTransform (AffineTransform::rotation (angle).translated (c));

    softShadow (g, p, Colours::black.withAlpha (0.6f), R * 0.22f, { R * 0.10f, R * 0.18f });
    ColourGradient fill (Colour (0xff3c3c3e), c.x - R * 0.5f, c.y - R * 0.6f, Colour (0xff050505), c.x + R * 0.6f, c.y + R * 0.7f, false);
    g.setGradientFill (fill);
    g.fillPath (p);
    ColourGradient rim (Colours::white.withAlpha (0.35f), c.x - R * 0.6f, c.y - R * 0.6f, Colours::transparentWhite, c.x + R * 0.2f, c.y + R * 0.2f, false);
    g.setGradientFill (rim);
    g.strokePath (p, PathStrokeType (1.2f));
    g.setColour (Colours::black);
    g.strokePath (p, PathStrokeType (0.6f));

    Path line;
    line.startNewSubPath (c.getPointOnCircumference (R * 0.15f, angle));
    line.lineTo (c.getPointOnCircumference (R * 0.86f, angle));
    g.setColour (Colour (0xfff1ebdc));
    g.strokePath (line, PathStrokeType (R * 0.07f, PathStrokeType::curved, PathStrokeType::rounded));

    ColourGradient spec (Colours::white.withAlpha (0.28f), c.x - R * 0.15f, c.y - R * 0.2f, Colours::transparentWhite, c.x + R * 0.35f, c.y - R * 0.2f, true);
    g.setGradientFill (spec);
    g.fillEllipse (c.x - R * 0.45f, c.y - R * 0.45f, R * 0.9f, R * 0.9f);
}

//==============================================================================
VuMeter::VuMeter (Style s) : style (s), vblank (this, [this] (double t) { tick (t); })
{
    setInterceptsMouseClicks (false, false);
}

void VuMeter::setTargetDb (float db) noexcept
{
    target = db < -60.0f ? 0.0f : jlimit (0.0f, 1.2f, fractionFor (db));
}

void VuMeter::tick (double now)
{
    if (lastTime <= 0) lastTime = now;
    const double dt = jlimit (0.0, 0.05, now - lastTime);
    lastTime = now;

    // Équipage mobile d'un galvanomètre : 2e ordre, ζ ≈ 0,8 et ω ≈ 13 rad/s donnent
    // le temps d'intégration normalisé (~300 ms) et un dépassement d'environ 1,5 %.
    constexpr float w = 13.0f, zeta = 0.80f, h = 0.001f;
    const int steps = (int) std::ceil (dt / h);
    for (int i = 0; i < steps; ++i)
    {
        vel += (w * w * (target - pos) - 2.0f * zeta * w * vel) * h;
        pos += vel * h;
        if (pos < -0.015f) { pos = -0.015f; vel = -vel * 0.25f; }   // butée gauche
        if (pos > 1.07f)   { pos = 1.07f;   vel = -vel * 0.25f; }   // butée droite
    }
    if (std::abs (pos - painted) > 0.0004f) repaint();
}

Rectangle<float> VuMeter::window() const
{
    auto b = getLocalBounds().toFloat();
    return b.reduced (b.getWidth() * 0.065f, b.getHeight() * 0.085f);
}

void VuMeter::geometry (Point<float>& pivot, float& radius) const
{
    const auto win = window();
    radius = jmin (win.getHeight() * 0.92f, win.getWidth() * 0.40f / std::sin (0.62f));
    pivot = { win.getCentreX(), win.getY() + win.getHeight() * 0.40f + radius };
}

void VuMeter::paintBack (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto win = window();
    const float corner = b.getHeight() * 0.05f;

    // Cadre en bakélite avec double biseau.
    Path bezel;
    bezel.addRoundedRectangle (b.reduced (1.0f), corner);
    g.setGradientFill (ColourGradient (Colour (0xff36363a), b.getX(), b.getY(), Colour (0xff070708), b.getX(), b.getBottom(), false));
    g.fillPath (bezel);
    g.setColour (Colours::white.withAlpha (0.16f));
    g.drawRoundedRectangle (b.reduced (1.6f), corner, 1.0f);
    const auto inner = b.reduced (b.getHeight() * 0.035f);
    g.setGradientFill (ColourGradient (Colour (0xff0a0a0b), inner.getX(), inner.getY(), Colour (0xff2a2a2d), inner.getX(), inner.getBottom(), false));
    g.fillRoundedRectangle (inner, corner * 0.7f);
    g.setTiledImageFill (grainTile(), 0, 0, 0.05f);
    g.fillPath (bezel);

    // Petites vis du cadre.
    const float sr = b.getHeight() * 0.024f;
    const float mx = (win.getX() - b.getX()) * 0.5f + b.getX(), my = (win.getY() - b.getY()) * 0.5f + b.getY();
    for (auto p : { Point<float> (mx, my), Point<float> (b.getRight() - (mx - b.getX()), my),
                    Point<float> (mx, b.getBottom() - (my - b.getY())), Point<float> (b.getRight() - (mx - b.getX()), b.getBottom() - (my - b.getY())) })
        drawScrew (g, p, sr, 0.7f + p.x * 0.01f, false);

    // Cadran.
    g.setColour (lit ? style.face : style.face.darker (0.55f));
    g.fillRect (win);
    if (lit)
    {
        ColourGradient glow (style.glow.withAlpha (style.glowAmount), win.getCentreX(), win.getBottom() + win.getHeight() * 0.05f,
                             style.glow.withAlpha (0.0f), win.getCentreX(), win.getY() - win.getHeight() * 0.25f, true);
        g.setGradientFill (glow);
        g.fillRect (win);
    }
    g.setTiledImageFill (grainTile(), 0, 0, 0.07f);
    g.fillRect (win);

    Point<float> pv; float R;
    geometry (pv, R);
    const float fs = win.getHeight() * 0.075f;
    auto dbAngle = [&] (float db) { return angleFor (fractionFor (db)); };
    auto ink = lit ? style.ink : style.ink.withAlpha (0.8f);

    // Arc principal puis zone rouge.
    Path arc;
    arc.addCentredArc (pv.x, pv.y, R, R, 0, dbAngle (-20.0f), dbAngle (0.0f), true);
    g.setColour (ink);
    g.strokePath (arc, PathStrokeType (1.3f));
    Path red;
    red.addCentredArc (pv.x, pv.y, R + fs * 0.22f, R + fs * 0.22f, 0, dbAngle (0.0f), dbAngle (3.0f), true);
    g.setColour (style.red);
    g.strokePath (red, PathStrokeType (fs * 0.45f));

    struct Mark { float db; const char* label; };
    const Mark marks[] { { -20, "20" }, { -10, "10" }, { -7, "7" }, { -6, nullptr }, { -5, "5" }, { -4, nullptr }, { -3, "3" },
                         { -2, "2" }, { -1, "1" }, { 0, "0" }, { 1, "1" }, { 2, "2" }, { 3, "3" } };
    g.setFont (panelFont (fs, false));
    for (auto& m : marks)
    {
        const float a = dbAngle (m.db);
        const bool major = m.label != nullptr;
        g.setColour (m.db > 0 ? style.red : ink);
        g.drawLine ({ pv.getPointOnCircumference (R, a), pv.getPointOnCircumference (R + fs * (major ? 0.75f : 0.45f), a) }, major ? 1.5f : 1.0f);
        if (major)
            g.drawText (m.label, Rectangle<float> (fs * 2.2f, fs * 1.2f).withCentre (pv.getPointOnCircumference (R + fs * 1.45f, a)), Justification::centred, false);
    }
    for (float db : { -15.0f, -12.0f, -8.5f })   // graduations fines
    {
        const float a = dbAngle (db);
        g.setColour (ink);
        g.drawLine ({ pv.getPointOnCircumference (R, a), pv.getPointOnCircumference (R + fs * 0.35f, a) }, 0.8f);
    }
    // Échelle de pourcentage sous l'arc (100 % = 0 VU).
    g.setFont (panelFont (fs * 0.62f, false));
    for (int pc = 0; pc <= 100; pc += 20)
    {
        const float a = angleFor (0.7079f * (float) pc / 100.0f);
        g.setColour (ink.withAlpha (0.85f));
        g.drawLine ({ pv.getPointOnCircumference (R - 1.0f, a), pv.getPointOnCircumference (R - fs * 0.4f, a) }, 0.9f);
        g.drawText (String (pc), Rectangle<float> (fs * 2.0f, fs).withCentre (pv.getPointOnCircumference (R - fs * 0.95f, a)), Justification::centred, false);
    }
    g.setFont (panelFont (fs * 1.25f, true));
    g.setColour (ink);
    g.drawText (String::fromUTF8 ("\xe2\x80\x93"), Rectangle<float> (fs * 2, fs * 1.4f).withCentre (pv.getPointOnCircumference (R + fs * 1.5f, dbAngle (-20.0f) - 0.10f)), Justification::centred, false);
    g.setColour (style.red);
    g.drawText ("+", Rectangle<float> (fs * 2, fs * 1.4f).withCentre (pv.getPointOnCircumference (R + fs * 1.5f, dbAngle (3.0f) + 0.10f)), Justification::centred, false);

    g.setColour (ink);
    g.setFont (panelFont (fs * 2.1f, true));
    g.drawText ("VU", Rectangle<float> (win.getWidth(), fs * 2.4f).withCentre ({ win.getCentreX(), win.getY() + win.getHeight() * 0.64f }), Justification::centred, false);
    g.setFont (panelFont (fs * 0.62f, true));
    g.drawText (style.caption, Rectangle<float> (win.getWidth() * 0.3f, fs).withPosition (win.getRight() - win.getWidth() * 0.32f, win.getBottom() - fs * 1.4f), Justification::centredRight, false);

    // Ombres internes : le cadran est en retrait du cadre.
    auto edge = [&] (Rectangle<float> r, float x0, float y0, float x1, float y1, float a)
    {
        g.setGradientFill (ColourGradient (Colours::black.withAlpha (a), x0, y0, Colours::transparentBlack, x1, y1, false));
        g.fillRect (r);
    };
    edge (win.withHeight (win.getHeight() * 0.14f), 0, win.getY(), 0, win.getY() + win.getHeight() * 0.14f, 0.55f);
    edge (win.withWidth (win.getWidth() * 0.05f), win.getX(), 0, win.getX() + win.getWidth() * 0.05f, 0, 0.35f);
    edge (win.withTrimmedLeft (win.getWidth() * 0.95f), win.getRight(), 0, win.getRight() - win.getWidth() * 0.05f, 0, 0.3f);
    edge (win.withTrimmedTop (win.getHeight() * 0.94f), 0, win.getBottom(), 0, win.getBottom() - win.getHeight() * 0.06f, 0.25f);
    g.setColour (Colours::black.withAlpha (0.85f));
    g.drawRect (win, 1.2f);
}

void VuMeter::paintGlass (Graphics& g)
{
    const auto win = window();
    Graphics::ScopedSaveState s (g);
    g.reduceClipRegion (win.toNearestInt());

    // Carter du mécanisme, d'où sort l'aiguille.
    const float hr = win.getHeight() * 0.19f;
    Path housing;
    housing.addEllipse (win.getCentreX() - hr * 1.5f, win.getBottom() - hr, hr * 3.0f, hr * 2.0f);
    softShadow (g, housing, Colours::black.withAlpha (0.45f), hr * 0.3f, { 0, -hr * 0.08f });
    g.setGradientFill (ColourGradient (Colour (0xff3a3a3d), win.getCentreX(), win.getBottom() - hr, Colour (0xff060606), win.getCentreX(), win.getBottom(), false));
    g.fillPath (housing);
    g.setColour (Colours::white.withAlpha (0.18f));
    g.strokePath (housing, PathStrokeType (1.0f));
    const Point<float> zc (win.getCentreX(), win.getBottom() - hr * 0.48f);   // vis de remise à zéro
    const float zr = hr * 0.17f;
    g.setGradientFill (ColourGradient (Colour (0xff4a4a4c), zc.x - zr, zc.y - zr, Colour (0xff0a0a0a), zc.x + zr, zc.y + zr, false));
    g.fillEllipse (zc.x - zr, zc.y - zr, zr * 2, zr * 2);
    g.setColour (Colours::black);
    g.drawLine (zc.x - zr * 0.8f, zc.y, zc.x + zr * 0.8f, zc.y, zr * 0.35f);

    // Verre : grand reflet diagonal, liseré clair en haut, léger voile.
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.20f), win.getX(), win.getY(),
                                       Colours::transparentWhite, win.getX() + win.getWidth() * 0.42f, win.getY() + win.getHeight() * 0.62f, false));
    g.fillRect (win);
    Path sheen;
    sheen.startNewSubPath (win.getX() + win.getWidth() * 0.55f, win.getY());
    sheen.lineTo (win.getX() + win.getWidth() * 0.72f, win.getY());
    sheen.lineTo (win.getX() + win.getWidth() * 0.40f, win.getBottom());
    sheen.lineTo (win.getX() + win.getWidth() * 0.30f, win.getBottom());
    sheen.closeSubPath();
    g.setColour (Colours::white.withAlpha (0.045f));
    g.fillPath (sheen);
    g.setColour (Colours::white.withAlpha (0.30f));
    g.drawLine (win.getX() + 2, win.getY() + 1.5f, win.getRight() - 2, win.getY() + 1.5f, 0.8f);
}

void VuMeter::paint (Graphics& g)
{
    back.get (g, getLocalBounds(), [this] (Graphics& ig) { paintBack (ig); });
    back.blit (g, getLocalBounds().toFloat());

    Point<float> pv; float R;
    geometry (pv, R);
    const auto win = window();
    const float a = angleFor (jlimit (-0.015f, 1.07f, pos));
    painted = pos;
    {
        Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (win.toNearestInt());
        auto needle = [&] (float baseW, float tipW)
        {
            const auto dir = Point<float> (std::sin (a), -std::cos (a));
            const auto nrm = Point<float> (-dir.y, dir.x);
            const auto p0 = pv + dir * (R * 0.30f), p1 = pv + dir * (R * 1.05f);
            Path p;
            p.startNewSubPath (p0 + nrm * baseW);
            p.lineTo (p1 + nrm * tipW);
            p.lineTo (p1 - nrm * tipW);
            p.lineTo (p0 - nrm * baseW);
            p.closeSubPath();
            return p;
        };
        // Ombre de l'aiguille sur le cadran (l'aiguille flotte quelques millimètres au-dessus).
        const auto off = AffineTransform::translation (win.getHeight() * 0.014f, win.getHeight() * 0.035f);
        g.setColour (Colours::black.withAlpha (0.10f));
        g.fillPath (needle (3.2f, 1.8f), off);
        g.setColour (Colours::black.withAlpha (0.16f));
        g.fillPath (needle (1.8f, 0.9f), off);
        g.setColour (Colour (0xff121110));
        g.fillPath (needle (1.25f, 0.45f));
    }
    glass.get (g, getLocalBounds(), [this] (Graphics& ig) { paintGlass (ig); });
    glass.blit (g, getLocalBounds().toFloat());
}

//==============================================================================
void PushKey::paintButton (Graphics& g, bool highlighted, bool down)
{
    const auto b = getLocalBounds().toFloat().reduced (1.0f);
    const bool pressed = getToggleState() || down;

    // Logement dans la façade.
    g.setColour (Colour (0xff0a0a0b));
    g.fillRoundedRectangle (b, 3.0f);
    g.setGradientFill (ColourGradient (Colours::black, b.getX(), b.getY(), Colour (0xff2b2b2e), b.getX(), b.getBottom(), false));
    g.drawRoundedRectangle (b, 3.0f, 1.0f);

    auto cap = b.reduced (b.getWidth() * 0.09f, b.getHeight() * 0.11f);
    if (pressed) cap = cap.translated (0, 1.6f).reduced (0.4f);
    else
    {
        g.setColour (Colours::black.withAlpha (0.65f));
        g.fillRoundedRectangle (cap.translated (0.6f, 2.2f), 2.5f);
    }
    const float tone = pressed ? 0.86f : (highlighted ? 1.03f : 1.0f);
    auto shade = [tone] (uint32 argb) { auto c = Colour (argb); return Colour::fromFloatRGBA (jmin (1.0f, c.getFloatRed() * tone), jmin (1.0f, c.getFloatGreen() * tone), jmin (1.0f, c.getFloatBlue() * tone), 1.0f); };
    g.setGradientFill (ColourGradient (shade (0xffeae5d6), cap.getX(), cap.getY(), shade (0xffa9a392), cap.getX(), cap.getBottom(), false));
    g.fillRoundedRectangle (cap, 2.5f);
    const auto top = cap.reduced (cap.getWidth() * 0.10f, 0).withTrimmedTop (cap.getHeight() * 0.06f).withTrimmedBottom (cap.getHeight() * 0.22f);
    g.setGradientFill (ColourGradient (shade (0xfffbf8ef), top.getX(), top.getY(), shade (0xffd9d3c3), top.getX(), top.getBottom(), false));
    g.fillRoundedRectangle (top, 2.0f);
    g.setColour (Colours::white.withAlpha (pressed ? 0.25f : 0.6f));
    g.drawLine (top.getX() + 1, top.getY() + 0.6f, top.getRight() - 1, top.getY() + 0.6f, 0.8f);
    g.setTiledImageFill (grainTile(), 0, 0, 0.05f);
    g.fillRoundedRectangle (cap, 2.5f);
    if (pressed)   // la touche enfoncée passe dans l'ombre du logement
    {
        g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.35f), cap.getX(), cap.getY(), Colours::transparentBlack, cap.getX(), cap.getY() + cap.getHeight() * 0.4f, false));
        g.fillRoundedRectangle (cap, 2.5f);
    }
}

//==============================================================================
void BatToggle::paintButton (Graphics& g, bool highlighted, bool)
{
    const auto b = getLocalBounds().toFloat();
    const auto c = b.getCentre();
    const float rn = jmin (b.getWidth(), b.getHeight()) * 0.17f;

    Path nut;
    nut.addPolygon (c, 6, rn * 1.25f, MathConstants<float>::pi / 6.0f);
    softShadow (g, nut, Colours::black.withAlpha (0.6f), rn * 0.3f, { rn * 0.1f, rn * 0.25f });
    g.setGradientFill (ColourGradient (Colour (0xfff0f0ec), c.x - rn, c.y - rn, Colour (0xff6a6b6e), c.x + rn, c.y + rn, false));
    g.fillPath (nut);
    g.setColour (Colours::black.withAlpha (0.45f));
    g.strokePath (nut, PathStrokeType (0.8f));
    g.setGradientFill (ColourGradient (Colour (0xff8d8e91), c.x - rn, c.y - rn, Colour (0xffd8d8d4), c.x + rn, c.y + rn, false));
    g.fillEllipse (c.x - rn * 0.82f, c.y - rn * 0.82f, rn * 1.64f, rn * 1.64f);
    for (int i = 0; i < 3; ++i)   // filetage de la douille
    {
        const float rr = rn * (0.78f - 0.1f * (float) i);
        g.setColour (Colours::black.withAlpha (0.22f));
        g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 0.7f);
    }
    g.setColour (Colour (0xff121214));
    g.fillEllipse (c.x - rn * 0.42f, c.y - rn * 0.42f, rn * 0.84f, rn * 0.84f);

    const bool up = getToggleState();
    const float L = jmin (b.getWidth(), b.getHeight()) * 0.40f;
    const float bw = rn * 0.30f, tw = rn * 0.21f, ball = rn * 0.30f;
    Path lever;
    lever.startNewSubPath (-bw, 0);
    lever.lineTo (-tw, -L);
    lever.lineTo (tw, -L);
    lever.lineTo (bw, 0);
    lever.closeSubPath();
    lever.addEllipse (-ball, -L - ball, ball * 2, ball * 2);
    const auto xf = AffineTransform::scale (1.0f, up ? 1.0f : -1.0f).translated (c);
    Path placed (lever);
    placed.applyTransform (xf);
    softShadow (g, placed, Colours::black.withAlpha (0.55f), rn * 0.35f, { rn * 0.45f, rn * 0.65f });

    Graphics::ScopedSaveState s (g);
    g.addTransform (xf);
    ColourGradient chrome (Colour (0xff4a4b4e), -bw, 0, Colour (0xff3a3b3e), bw, 0, false);
    chrome.addColour (0.25, Colour (0xfffbfbf8));
    chrome.addColour (0.45, Colour (0xff9a9b9e));
    chrome.addColour (0.70, Colour (0xff2a2b2d));
    chrome.addColour (0.88, Colour (0xffcfcfcc));
    g.setGradientFill (chrome);
    g.fillPath (lever);
    g.setGradientFill (ColourGradient (Colours::white, -ball * 0.4f, -L - ball * 0.45f, Colour (0xff505154), ball, -L + ball, true));
    g.fillEllipse (-ball, -L - ball, ball * 2, ball * 2);
    if (highlighted)
    {
        g.setColour (Colours::white.withAlpha (0.08f));
        g.fillPath (lever);
    }
}

//==============================================================================
void PilotLamp::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto c = b.getCentre();
    const float r = jmin (b.getWidth(), b.getHeight()) * 0.24f;

    if (lit)
    {
        g.setGradientFill (ColourGradient (colour.withAlpha (0.45f), c.x, c.y, colour.withAlpha (0.0f), c.x + r * 2.1f, c.y, true));
        g.fillEllipse (c.x - r * 2.1f, c.y - r * 2.1f, r * 4.2f, r * 4.2f);
    }
    Path bezel;
    bezel.addEllipse (c.x - r * 1.3f, c.y - r * 1.3f, r * 2.6f, r * 2.6f);
    softShadow (g, bezel, Colours::black.withAlpha (0.6f), r * 0.3f, { r * 0.1f, r * 0.25f });
    g.setGradientFill (ColourGradient (Colour (0xfff2f2ee), c.x - r, c.y - r, Colour (0xff57585b), c.x + r, c.y + r, false));
    g.fillPath (bezel);
    g.setColour (Colour (0xff1a1a1a));
    g.fillEllipse (c.x - r * 1.04f, c.y - r * 1.04f, r * 2.08f, r * 2.08f);

    ColourGradient dome = lit ? ColourGradient (colour.brighter (1.4f), c.x - r * 0.25f, c.y - r * 0.3f, colour.darker (0.7f), c.x + r, c.y + r, true)
                              : ColourGradient (colour.darker (1.2f), c.x - r * 0.25f, c.y - r * 0.3f, colour.darker (3.5f), c.x + r, c.y + r, true);
    if (lit) dome.addColour (0.45, colour);
    g.setGradientFill (dome);
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    for (int i = 0; i < 8; ++i)   // facettes du cabochon
    {
        const float a = MathConstants<float>::twoPi * (float) i / 8.0f;
        g.setColour (Colours::white.withAlpha (i % 2 ? 0.10f : 0.05f));
        g.drawLine ({ c.getPointOnCircumference (r * 0.35f, a), c.getPointOnCircumference (r * 0.97f, a) }, 0.7f);
    }
    Path inner;
    inner.addPolygon (c, 8, r * 0.38f, 0);
    g.setColour (Colours::white.withAlpha (lit ? 0.22f : 0.08f));
    g.strokePath (inner, PathStrokeType (0.7f));
    g.setColour (Colours::white.withAlpha (0.75f));
    g.fillEllipse (c.x - r * 0.55f, c.y - r * 0.6f, r * 0.35f, r * 0.22f);
}
} // namespace swell::analog
