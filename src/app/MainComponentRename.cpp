// MainComponent, part 6: renaming own patterns and songs (right click in the pattern menu).

#include "app/MainComponent.h"

#include "app/Text.h"
#include "app/views/InWindowDialog.h"

namespace
{
    void askName (juce::Component* near, const juce::String& title, const juce::String& initial,
                  std::function<void (const juce::String&)> done)
    {
        auto* w = new InWindowDialog (title, "Neuer Name:", juce::MessageBoxIconType::NoIcon, near);
        w->addTextEditor ("name", initial, "Name");
        w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Abbrechen", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        w->enterModalState (true, juce::ModalCallbackFunction::create ([w, done] (int r)
        {
            const auto name = w->getTextEditorContents ("name").trim();
            if (r == 1 && name.isNotEmpty())
                done (name);
        }), true);
    }
}

void MainComponent::askRenameEntry (const std::string& id)
{
    const int index = library.indexOf (id);
    if (index < 0)
        return;
    getTopLevelComponent()->toFront (true);   // the dialog appears in this window: not behind the workshop
    if (! library.isSong (index) && ! library.isUserPattern (index))
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Umbenennen",
            "Mitgelieferte Patterns lassen sich nicht umbenennen. Im Raster macht \"Speichern unter\" eine eigene Kopie daraus, die sich umbenennen lässt."_u,
            "OK", this);
        return;
    }
    const auto oldName = library.get (index).name;
    askName (this, library.isSong (index) ? "Song umbenennen" : "Pattern umbenennen", juce::String (oldName),
             [safe = juce::Component::SafePointer<MainComponent> (this), id, oldName] (const juce::String& name)
    {
        if (safe == nullptr)
            return;
        auto& self = *safe;
        const auto selectedId = self.patternMode() ? self.library.get (self.currentPattern).id : std::string();
        const auto error = self.library.rename (id, name);   // reloads: indices change
        self.currentPattern = self.library.indexOf (selectedId);
        if (error.isNotEmpty())
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Umbenennen", error);

        // the open workshop follows what was saved: the arranger's copies and song name, the raster's pattern
        const int renamed = self.library.indexOf (id);
        const bool saved = renamed >= 0 && juce::String (self.library.get (renamed).name) == name.trim();
        if (saved && self.editorWindow != nullptr)
        {
            auto& arranger = self.editorWindow->getArranger();
            if (Library::areaOf (id) == FolderTree::Songs)
                arranger.songRenamed (id, name.trim().toStdString());
            else
                arranger.patternRenamed (id, oldName, name.trim().toStdString());
            arranger.libraryChanged();
            auto& editor = self.editorWindow->getEditor();
            if (const int i = self.library.indexOf (id); i >= 0 && editor.getPattern().id == id && ! editor.hasUnsavedChanges())
                editor.setPattern (self.library.get (i), true);
        }
        self.quickSearch.close();
        self.updateButtons();
        self.updateView();
    });
}
