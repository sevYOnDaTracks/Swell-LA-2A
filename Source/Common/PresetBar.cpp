#include "PresetBar.h"

namespace swell::analog
{
using namespace juce;

PresetBar::PresetBar (AnalogProcessorBase& p, bool onDarkPanel) : proc (p), dark (onDarkPanel)
{
    setMouseCursor (MouseCursor::PointingHandCursor);
    shown = proc.getPresetName();
}

void PresetBar::refresh()
{
    const auto name = proc.getPresetName();
    if (name != shown) { shown = name; repaint(); }
}

int PresetBar::zoneAt (Point<int> p) const
{
    const int arrow = getHeight();
    if (p.x < arrow) return 0;
    if (p.x > getWidth() - arrow) return 2;
    return 1;
}

void PresetBar::mouseMove (const MouseEvent& e) { setHover (zoneAt (e.getPosition())); }

void PresetBar::mouseUp (const MouseEvent& e)
{
    if (! contains (e.getPosition())) return;
    switch (zoneAt (e.getPosition()))
    {
        case 0: step (-1); break;
        case 2: step (1); break;
        default: showMenu(); break;
    }
}

void PresetBar::step (int direction)
{
    const int numFactory = (int) proc.factoryPresets().size();
    const auto user = proc.userPresets();
    const int total = numFactory + user.size();
    if (total == 0) return;

    int current = proc.getCurrentFactoryIndex();
    if (current < 0)
        for (int i = 0; i < user.size(); ++i)
            if (user[i].getFileNameWithoutExtension() == proc.getPresetName()) current = numFactory + i;
    const int next = ((current < 0 ? 0 : current + direction) % total + total) % total;
    if (next < numFactory) proc.loadFactoryPreset (next);
    else proc.loadUserPreset (user[next - numFactory]);
    refresh();
}

void PresetBar::showMenu()
{
    PopupMenu menu;
    const auto& factory = proc.factoryPresets();
    String category;
    for (size_t i = 0; i < factory.size(); ++i)
    {
        if (category != factory[i].category)
        {
            category = factory[i].category;
            menu.addSectionHeader (category);
        }
        menu.addItem (PopupMenu::Item (factory[i].name).setID (1 + (int) i)
                                                       .setTicked ((int) i == proc.getCurrentFactoryIndex()));
    }
    const auto user = proc.userPresets();
    menu.addSectionHeader ("Mes presets");
    if (user.isEmpty())
        menu.addItem (PopupMenu::Item ("(aucun pour l'instant)").setEnabled (false));
    for (int i = 0; i < user.size(); ++i)
        menu.addItem (PopupMenu::Item (user[i].getFileNameWithoutExtension()).setID (1000 + i)
                                   .setTicked (proc.getCurrentFactoryIndex() < 0 && user[i].getFileNameWithoutExtension() == proc.getPresetName()));
    menu.addSeparator();
    menu.addItem (PopupMenu::Item (String::fromUTF8 ("Enregistrer le preset\xe2\x80\xa6")).setID (900));
    menu.addItem (PopupMenu::Item ("Ouvrir le dossier des presets").setID (901));

    SafePointer<PresetBar> safe (this);
    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()),
                        [safe, user] (int id)
                        {
                            if (safe == nullptr || id == 0) return;
                            auto& self = *safe;
                            if (id >= 1000) self.proc.loadUserPreset (user[id - 1000]);
                            else if (id == 900) self.saveAs();
                            else if (id == 901)
                            {
                                auto folder = self.proc.userPresetFolder();
                                folder.createDirectory();
                                folder.startAsProcess();
                            }
                            else self.proc.loadFactoryPreset (id - 1);
                            self.refresh();
                        });
}

void PresetBar::saveAs()
{
    auto folder = proc.userPresetFolder();
    folder.createDirectory();
    chooser = std::make_unique<FileChooser> ("Enregistrer le preset", folder.getChildFile (proc.getPresetName() + AnalogProcessorBase::presetExtension),
                                             String ("*") + AnalogProcessorBase::presetExtension);
    SafePointer<PresetBar> safe (this);
    chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting,
                          [safe] (const FileChooser& fc)
                          {
                              const auto file = fc.getResult();
                              if (safe == nullptr || file == File()) return;
                              safe->proc.saveUserPreset (file.withFileExtension (AnalogProcessorBase::presetExtension));
                              safe->refresh();
                          });
}

void PresetBar::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float h = b.getHeight();
    const float corner = h * 0.18f;

    // Logement : arête claire en bas (lumière d'en haut), ombre portée à l'intérieur en haut.
    g.setColour (Colours::white.withAlpha (dark ? 0.12f : 0.55f));
    g.drawRoundedRectangle (b.translated (0, 1.0f).reduced (0.5f), corner, 1.0f);
    const auto win = b.reduced (1.0f);
    g.setGradientFill (ColourGradient (Colour (0xff030303), 0, win.getY(), Colour (0xff1a1816), 0, win.getBottom(), false));
    g.fillRoundedRectangle (win, corner);
    g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.8f), 0, win.getY(), Colours::transparentBlack, 0, win.getY() + h * 0.3f, false));
    g.fillRoundedRectangle (win, corner);

    const Colour lamp (0xfff3d79c);   // rétroéclairage chaud
    auto arrow = [&] (Rectangle<float> r, bool left, bool lit)
    {
        const auto c = r.getCentre();
        const float s = h * 0.17f;
        Path t;
        if (left) t.addTriangle (c.x + s * 0.6f, c.y - s, c.x + s * 0.6f, c.y + s, c.x - s * 0.7f, c.y);
        else      t.addTriangle (c.x - s * 0.6f, c.y - s, c.x - s * 0.6f, c.y + s, c.x + s * 0.7f, c.y);
        g.setColour (lamp.withAlpha (lit ? 0.95f : 0.45f));
        g.fillPath (t);
    };
    arrow (win.withWidth (h), true, hover == 0);
    arrow (win.withTrimmedLeft (win.getWidth() - h), false, hover == 2);

    // Nom : léger halo autour des lettres, comme une fenêtre éclairée par l'arrière.
    const auto textArea = win.reduced (h, 0);
    g.setFont (panelFont (h * 0.46f, true));
    g.setColour (lamp.withAlpha (0.16f));
    for (auto off : { Point<float> (-0.7f, 0), Point<float> (0.7f, 0), Point<float> (0, -0.7f), Point<float> (0, 0.7f) })
        g.drawText (shown.toUpperCase(), textArea.translated (off.x, off.y), Justification::centred, true);
    g.setColour (lamp.withAlpha (hover == 1 ? 1.0f : 0.9f));
    g.drawText (shown.toUpperCase(), textArea, Justification::centred, true);

    // Vitre : reflet en haut.
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.10f), 0, win.getY(), Colours::transparentWhite, 0, win.getCentreY(), false));
    g.fillRoundedRectangle (win.reduced (1.0f), corner);
}
} // namespace swell::analog
