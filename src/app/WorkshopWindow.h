#pragma once

#include "app/PatternEditor.h"
#include "app/arranger/ArrangerPanel.h"
#include "app/views/ActiveGlow.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

// The "Werkstatt" window: tabs for the raster editor and the arranger.
// Owns both; closing asks about unsaved changes and then tells the host.
class WorkshopWindow : public juce::DocumentWindow
{
public:
    WorkshopWindow (EditorHost&, ArrangerHost&, juce::PropertiesFile&);
    ~WorkshopWindow() override;

    PatternEditor& getEditor()   { return *editor; }
    ArrangerPanel& getArranger() { return *arranger; }
    void showRaster()            { tabs.setCurrentTabIndex (0); }
    void showArranger()          { tabs.setCurrentTabIndex (1); }
    void closeButtonPressed() override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;   // keys that no focused component took: to the active tab

private:
    // Tab bar: dark, the active tab with a soft blue glow
    struct TabLook : juce::LookAndFeel_V4
    {
        void drawTabButton (juce::TabBarButton&, juce::Graphics&, bool isMouseOver, bool isMouseDown) override;
        void drawTabAreaBehindFrontButton (juce::TabbedButtonBar&, juce::Graphics&, int, int) override {}
        int getTabButtonBestWidth (juce::TabBarButton&, int) override { return 120; }
    };
    TabLook tabLook;

    // tab bar with a change callback (JUCE only offers a virtual for that)
    struct Tabs : juce::TabbedComponent
    {
        Tabs() : juce::TabbedComponent (juce::TabbedButtonBar::TabsAtTop) {}
        std::function<void (int)> onChange;
        void currentTabChanged (int index, const juce::String&) override { if (onChange) onChange (index); }
    };

    EditorHost& host;
    juce::PropertiesFile& props;
    Tabs tabs;
    PatternEditor* editor = nullptr;     // owned by 'tabs'
    ArrangerPanel* arranger = nullptr;   // owned by 'tabs'
    ActiveGlow activeGlow;

    // the raster tab is only clickable when a pattern is loaded in it
    void updateRasterTab();
    // "Werkstatt: <song name>", following renames at once
    void updateTitle();
    juce::TimedCallback stateCheck { [this] { updateRasterTab(); updateTitle(); } };
};
