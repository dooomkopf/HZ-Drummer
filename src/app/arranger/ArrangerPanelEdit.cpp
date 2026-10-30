// ArrangerPanel, part 2: a block or a card of the Ablage into the raster and back, their menus, bar ranges,
// "Original", local renaming.

#include "app/arranger/ArrangerPanel.h"

#include "app/Library.h"
#include "app/NavigationMenus.h"
#include "app/Text.h"
#include "app/arranger/ArrangerStyle.h"
#include "app/views/InWindowDialog.h"
#include "app/views/PlaybackView.h"

#include <algorithm>

using namespace arrangerstyle;

void ArrangerPanel::showCardMenu (int i)
{
    // listening (click) and removing (x) are on the card itself; the menu edits it or adds it to the sequence
    juce::PopupMenu menu;
    const auto& card = model.scratch()[(size_t) i];
    menu.addItem (3, "Bearbeiten (Kopie im Raster)", card.kind == PartKind::Pattern && card.copy != nullptr);
    menu.addSeparator();
    // before / after a block marked in the sequence; without a mark only dragging with the mouse places it
    const bool hasMark = sequence.getSelected() >= 0 && sequence.getSelected() < (int) model.song().parts.size();
    menu.addSectionHeader ("Pattern im Ablauf anklicken/markieren");
    menu.addItem (2, juce::String (juce::CharPointer_UTF8 ("vor Pattern einf\xc3\xbcgen")), hasMark);
    menu.addItem (4, juce::String (juce::CharPointer_UTF8 ("nach Pattern einf\xc3\xbcgen")), hasMark);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&scratch).withMousePosition(),
                        [safe = juce::Component::SafePointer<ArrangerPanel> (this), i] (int r)
    {
        if (safe == nullptr || r == 0 || i >= (int) safe->model.scratch().size())
            return;
        const auto& part = safe->model.scratch()[(size_t) i];
        const int sel = safe->sequence.getSelected();
        const bool hasSel = sel >= 0 && sel < (int) safe->model.song().parts.size();
        if (r == 2 && hasSel) safe->addToSequence ({ part }, sel);       // before the marked block
        if (r == 4 && hasSel) safe->addToSequence ({ part }, sel + 1);   // after it
        if (r == 3) safe->editScratchCard (i);
    });
}

void ArrangerPanel::askRange()
{
    const int p = sequence.getSelected();
    if (p < 0 || p >= (int) model.song().parts.size())
        return;
    const auto& part = model.song().parts[(size_t) p];
    if (part.kind != PartKind::Pattern || ! part.copy)
        return;
    const int total = (int) part.copy->bars.size();
    const int from = part.numBars > 0 ? part.firstBar + 1 : 1;
    const int to = part.numBars > 0 ? part.firstBar + part.numBars : total;

    auto* w = new InWindowDialog ("Takte eingrenzen", "Welche Takte von \""_u + juce::String (part.copy->name) + "\" sollen gespielt werden? (1 bis "
                                     + juce::String (total) + ")", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("from", juce::String (from), "von Takt:");
    w->addTextEditor ("to", juce::String (to), "bis Takt:");
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Ganzes Pattern", 2);
    w->addButton ("Abbrechen", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    const auto uid = part.uid;
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<ArrangerPanel> (this), w, uid, total] (int r)
    {
        if (safe == nullptr || r == 0)
            return;
        // the block is found by its identity, wherever it moved meanwhile; gone = nothing happens
        const int p = safe->model.indexOfUid (uid);
        if (p < 0 || ! safe->model.song().parts[(size_t) p].copy || (int) safe->model.song().parts[(size_t) p].copy->bars.size() != total)
            return;
        if (r == 2)
            safe->model.setRange (p, 0, 0);
        else
        {
            const int a = juce::jlimit (1, total, w->getTextEditorContents ("from").getIntValue());
            const int b = juce::jlimit (a, total, w->getTextEditorContents ("to").getIntValue());
            safe->model.setRange (p, a - 1, b - a + 1);
        }
    }), true);
}

void ArrangerPanel::editBlock()
{
    const int p = sequence.getSelected();
    if (p < 0 || p >= (int) model.song().parts.size() || ! onEditInRaster)
        return;
    const auto& part = model.song().parts[(size_t) p];
    if (part.kind != PartKind::Pattern || ! part.copy)
        return;

    juce::String title;
    for (auto& sp : spans)
        if (sp.part == p) { title = sp.label; break; }

    // The edit comes back later. The block is found by its identity; if its copy changed in the
    // meantime (undo, "Original"), the user decides instead of losing that silently.
    // 'basis' is the copy the raster's edit builds on: it follows our own applies, otherwise the
    // second "In Ablauf übernehmen" would take the first one for a change from elsewhere.
    const auto uid = part.uid;
    auto basis = std::make_shared<std::shared_ptr<const hz::Pattern>> (part.copy);
    onEditInRaster (*part.copy, title, [safe = juce::Component::SafePointer<ArrangerPanel> (this), uid, basis] (const hz::Pattern& edited)
    {
        if (safe == nullptr)
            return false;
        const int p = safe->model.indexOfUid (uid);
        if (p < 0)
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "In Ablauf übernehmen"_u,
                                                    "Der Block ist nicht mehr im Ablauf. Nichts geändert."_u);
            return false;
        }
        if (safe->model.song().parts[(size_t) p].copy != *basis)
        {
            // asked, not decided here: the raster keeps its edit until the answer arrives
            juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "In Ablauf übernehmen"_u,
                "Der Block wurde inzwischen im Arranger geändert (Rückgängig oder \"Original\"). Trotzdem mit der Raster-Fassung ersetzen?"_u,
                "Ersetzen", "Abbrechen", safe,
                juce::ModalCallbackFunction::create ([safe, uid, edited, basis] (int r)
                {
                    if (safe == nullptr || r != 1)
                        return;
                    if (const int q = safe->model.indexOfUid (uid); q >= 0 && safe->model.setCopy (q, edited, true))
                    {
                        *basis = safe->model.song().parts[(size_t) q].copy;
                        if (safe->onBlockApplied)
                            safe->onBlockApplied (edited);   // the raster counts it as taken over now
                    }
                }));
            return false;
        }
        if (! safe->model.setCopy (p, edited, true))
            return false;
        *basis = safe->model.song().parts[(size_t) p].copy;
        return true;
    });
}

bool ArrangerPanel::editSelectedBlock()
{
    const int p = sequence.getSelected();
    if (p < 0 || p >= (int) model.song().parts.size() || model.song().parts[(size_t) p].kind != PartKind::Pattern)
        return false;
    editBlock();
    return true;
}

void ArrangerPanel::showBlockMenu (int part)
{
    if (part < 0 || part >= (int) model.song().parts.size())
        return;
    const auto& p = model.song().parts[(size_t) part];
    bool canUpdate = false;
    for (auto& sp : spans)
        if (sp.part == part) { canUpdate = sp.sourceDiffers; break; }

    juce::PopupMenu menu;
    menu.addItem (7, "Lokale Umbenennung");
    menu.addItem (1, "Bearbeiten (Kopie im Raster)", p.kind == PartKind::Pattern && p.copy != nullptr);
    menu.addItem (2, "Duplizieren");
    menu.addItem (3, "Takte eingrenzen", p.kind == PartKind::Pattern && p.copy && p.copy->bars.size() > 1);
    menu.addItem (4, "Original", canUpdate);
    menu.addItem (8, "In die Ablage legen");
    menu.addSeparator();
    menu.addItem (5, "Play ab hier");
    menu.addSeparator();
    menu.addItem (6, "Entfernen");
    const auto uid = p.uid;
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&sequence).withMousePosition(),
                        [safe = juce::Component::SafePointer<ArrangerPanel> (this), uid] (int r)
    {
        if (safe == nullptr || r == 0)
            return;
        const int part = safe->model.indexOfUid (uid);
        if (part < 0)
            return;
        safe->sequence.setSelected (part);
        switch (r)
        {
            case 1: safe->editBlock(); break;
            case 2: safe->addToSequence ({ safe->model.song().parts[(size_t) part] }, part + 1); break;
            case 3: safe->askRange(); break;
            case 4: safe->updateFromSource(); break;
            case 5: safe->playFrom (part); break;
            case 6: safe->model.removePart (part); safe->selectPart (juce::jmin (part, (int) safe->model.song().parts.size() - 1)); break;
            case 7: safe->askRename (part); break;
            case 8: safe->model.addToScratch ({ safe->model.song().parts[(size_t) part] }); break;
            default: break;
        }
    });
}

void ArrangerPanel::editScratchCard (int index)
{
    if (index < 0 || index >= (int) model.scratch().size() || ! onEditCardInRaster)
        return;
    const auto& card = model.scratch()[(size_t) index];
    if (card.kind != PartKind::Pattern || ! card.copy)
        return;

    // as editBlock: the card is found again by its identity, 'basis' follows our own applies
    const auto uid = card.uid;
    const juce::String title = ! card.label.empty() ? juce::String (card.label) : juce::String (card.copy->name);
    auto basis = std::make_shared<std::shared_ptr<const hz::Pattern>> (card.copy);
    auto apply = [safe = juce::Component::SafePointer<ArrangerPanel> (this), uid, basis] (const hz::Pattern& edited)
    {
        if (safe == nullptr)
            return false;
        const int i = safe->model.indexOfScratchUid (uid);
        if (i < 0)
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "In Ablage übernehmen"_u,
                                                    "Die Karte ist nicht mehr in der Ablage. Nichts geändert."_u);
            return false;
        }
        if (safe->model.scratch()[(size_t) i].copy != *basis)
        {
            juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "In Ablage übernehmen"_u,
                "Die Karte wurde inzwischen geändert (Rückgängig). Trotzdem mit der Raster-Fassung ersetzen?"_u,
                "Ersetzen", "Abbrechen", safe,
                juce::ModalCallbackFunction::create ([safe, uid, edited, basis] (int r)
                {
                    if (safe == nullptr || r != 1)
                        return;
                    if (const int q = safe->model.indexOfScratchUid (uid); q >= 0 && safe->model.setScratchCopy (q, edited, true))
                    {
                        *basis = safe->model.scratch()[(size_t) q].copy;
                        if (safe->onBlockApplied)
                            safe->onBlockApplied (edited);   // the raster counts it as taken over now
                    }
                }));
            return false;
        }
        if (! safe->model.setScratchCopy (i, edited, true))
            return false;
        *basis = safe->model.scratch()[(size_t) i].copy;
        return true;
    };
    // "Speichern unter": the new own pattern becomes a card right after this one (at the end if it is gone)
    auto savedAsNew = [safe = juce::Component::SafePointer<ArrangerPanel> (this), uid] (const hz::Pattern& saved)
    {
        if (safe == nullptr)
            return;
        const int i = safe->model.indexOfScratchUid (uid);
        safe->model.insertIntoScratch (i >= 0 ? i + 1 : (int) safe->model.scratch().size(), makePatternPart (saved));
    };
    onEditCardInRaster (*card.copy, title, std::move (apply), std::move (savedAsNew));
}

void ArrangerPanel::updateFromSource()
{
    const int p = sequence.getSelected();
    if (p < 0 || p >= (int) model.song().parts.size())
        return;
    const auto& part = model.song().parts[(size_t) p];
    if (part.kind != PartKind::Pattern || ! sourceDiffers (part, host.getLibrary().all()))
        return;
    const auto all = model.partsWithSource (part.sourceId);
    juce::String sourceName (part.copy ? part.copy->name : part.sourceId);
    if (const int i = host.getLibrary().indexOf (part.sourceId); i >= 0)
        sourceName = juce::String (host.getLibrary().get (i).name);

    // the warning the user asked for: this replaces the copy, local edits included
    juce::String text = "Der Block wird durch die aktuelle Fassung von \""_u + sourceName + "\" aus der Bibliothek ersetzt.";
    if (part.edited)
        text << "\n\nAchtung: Die Änderungen, die du an diesem Block im Song gemacht hast, gehen dabei verloren."_u;
    text << "\n\n(Strg+Z macht es rückgängig.)"_u;

    auto* w = new InWindowDialog ("Original", text, juce::MessageBoxIconType::WarningIcon, this);
    w->addButton ("Nur diesen Block", 1, juce::KeyPress (juce::KeyPress::returnKey));
    if (all.size() > 1)
        w->addButton ("Alle " + juce::String ((int) all.size()) + " Bl" + juce::String (juce::CharPointer_UTF8 ("\xc3\xb6")) + "cke dieser Quelle", 2);
    w->addButton ("Abbrechen", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    const auto sourceId = part.sourceId;
    const auto uid = part.uid;
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<ArrangerPanel> (this), uid, sourceId] (int r)
    {
        if (safe == nullptr || r == 0)
            return;
        const int p = safe->model.indexOfUid (uid);
        if (p < 0)
            return;
        safe->model.updateFromSource (r == 2 ? safe->model.partsWithSource (sourceId) : std::vector<int> { p }, safe->host.getLibrary().all());
    }), true);
}

void ArrangerPanel::askRename (int p)
{
    if (p < 0 || p >= (int) model.song().parts.size())
        return;
    const auto& part = model.song().parts[(size_t) p];
    juce::String origin;
    for (auto& sp : spans)
        if (sp.part == p) { origin = sp.label; break; }
    const juce::String sourceName = part.copy ? juce::String (part.copy->name) : juce::String (part.sourceId);

    auto* w = new InWindowDialog ("Block umbenennen",
                                     "Name dieses Blocks im Ablauf. Das Pattern selbst (\"" + sourceName + "\") wird nicht umbenannt.\n"
                                     "Leer lassen = Pattern-Name anzeigen. Strg+Z macht es rückgängig."_u,
                                     juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", juce::String (part.label), "Name im Ablauf:");
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Abbrechen", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    const auto uid = part.uid;
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<ArrangerPanel> (this), w, uid] (int r)
    {
        if (safe == nullptr || r == 0)
            return;
        const int p = safe->model.indexOfUid (uid);
        if (p < 0)
            return;
        safe->model.setLabel (p, w->getTextEditorContents ("name").trim().toStdString());
    }), true);
}
