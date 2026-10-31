#include "app/MainComponent.h"
#include "Version.h"
#include "app/views/HZLook.h"
#include "app/views/Theme.h"
#include "app/views/Watermark.h"

#include <juce_gui_extra/juce_gui_extra.h>

class HZDrummerApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return HZ_VERSION; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise (const juce::String& commandLine) override
    {
        const auto logFile = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".cache/HZ-Drummer/app.log");
        logFile.getParentDirectory().createDirectory();
        logger = std::make_unique<juce::FileLogger> (logFile, "HZ-Drummer v" HZ_VERSION " gestartet", 1024 * 1024);
        juce::Logger::setCurrentLogger (logger.get());
        juce::SystemStats::setApplicationCrashHandler ([] (void*)
        {
            // best effort in a crashed process: the call stack into app.log, so a crash can be found later
            juce::Logger::writeToLog ("ABSTURZ (HZ-Drummer v" HZ_VERSION "), Aufrufstapel:\n" + juce::SystemStats::getStackBacktrace());
        });
        juce::LookAndFeel::setDefaultLookAndFeel (&look);

        // the window first shows a splash, then builds the main component (the library takes a second or two)
        mainWindow = std::make_unique<MainWindow> (getApplicationName(), [commandLine] (MainComponent& mc)
        {
            // --editor: open the grid editor directly (for tests and screenshots)
            if (commandLine.contains ("--editor"))
                mc.openEditorForTesting();
            if (commandLine.contains ("--arranger"))
                mc.openArrangerForTesting();
            if (commandLine.contains ("--play"))   // play immediately (tests/screenshots)
                mc.startForTesting();
            if (commandLine.contains ("--mixer"))
                mc.openMixerForTesting();
        });
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
        juce::Logger::writeToLog ("HZ-Drummer beendet");
        juce::Logger::setCurrentLogger (nullptr); logger.reset();
    }

    void systemRequestedQuit() override
    {
        // asks first if the raster holds unsaved changes
        if (auto* mc = mainWindow != nullptr ? dynamic_cast<MainComponent*> (mainWindow->getContentComponent()) : nullptr)
            mc->requestQuit();
        else
            quit();
    }

    // Second launch (e.g. click on the icon although already open): bring the existing window to the front
    void anotherInstanceStarted (const juce::String&) override
    {
        if (mainWindow != nullptr)
        {
            mainWindow->setMinimised (false);
            mainWindow->toFront (true);
        }
    }

private:
    // What the main window shows while the main component is built: the kit exactly where the main window's
    // watermark sits (same size, same place, only stronger), "Initializing" above and "Drummer" below it,
    // so that it reads "Initializing HZ Drummer". It fades in, then asks for the main component.
    class SplashContent : public juce::Component, private juce::Timer
    {
    public:
        explicit SplashContent (std::function<void()> whenShown) : onShown (std::move (whenShown))
        {
            setOpaque (true);
            setSize (1000, 700);   // the main component's size
            startTimerHz (60);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (theme::background);
            watermark.draw (g, getLocalBounds(), 0.45f * fade);   // as MainComponent::paint, stronger
            const auto h = (float) getHeight();
            g.setColour (juce::Colour (0xff8a8a92).withMultipliedAlpha (fade));
            g.setFont (juce::FontOptions (h * 0.045f));
            g.drawText ("Initializing", getLocalBounds().removeFromTop ((int) (h * 0.17f)), juce::Justification::centred);
            // "Drummer" heavier than the font's bold: the letters as outlines, half again as tall as the font
            // makes them (the width stays), grown upwards from the baseline into the H, filled and thickened
            // by a stroke that leaves the inner lines (the bar of the e) open
            const juce::Font drummerFont (juce::FontOptions (h * 0.234f, juce::Font::bold));
            const auto line = getLocalBounds().removeFromBottom ((int) (h * 0.24f)).toFloat();
            juce::GlyphArrangement glyphs;
            glyphs.addFittedText (drummerFont, "Drummer", line.getX(), line.getY(), line.getWidth(), line.getHeight(),
                                  juce::Justification::centred, 1);
            juce::Path letters;
            glyphs.createPath (letters);
            const auto box = letters.getBounds();
            letters.applyTransform (juce::AffineTransform::scale (1.0f, 1.5f, box.getCentreX(), box.getBottom()));
            g.setColour (juce::Colour (0xffb8b8be).withMultipliedAlpha (fade));
            g.fillPath (letters);
            g.strokePath (letters, juce::PathStrokeType (drummerFont.getHeight() * 0.065f, juce::PathStrokeType::curved,
                                                         juce::PathStrokeType::rounded));
        }

    private:
        void timerCallback() override
        {
            fade = juce::jmin (1.0f, fade + 0.125f);   // about 130 ms
            repaint();
            if (fade < 1.0f)
                return;
            stopTimer();
            if (auto* peer = getPeer())
                peer->performAnyPendingRepaintsNow();   // fully there before the long build blocks the program
            juce::MessageManager::callAsync (onShown);
        }

        std::function<void()> onShown;
        Watermark watermark;
        float fade = 0.0f;
    };

    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow (const juce::String& name, std::function<void (MainComponent&)> whenReady)
            : DocumentWindow (name, juce::Colour (0xff121214), DocumentWindow::allButtons, false)
        {
            // onto the desktop only once size and place are known: on Linux, switching a window that is
            // already there to the native title bar (or to resizable) shows it at once, small and empty
            // in the top left corner
            setUsingNativeTitleBar (true);
            setContentOwned (new SplashContent ([safe = juce::Component::SafePointer<MainWindow> (this), whenReady]
            {
                if (safe == nullptr)
                    return;   // quit during the splash
                auto* main = new MainComponent();
                safe->setContentOwned (main, false);   // the window keeps its size: the kit stays where it was
                if (whenReady)
                    whenReady (*main);
            }), true);
            setResizable (true, true);
            setResizeLimits (640, 420, 4000, 3000);
            centreWithSize (getWidth(), getHeight());
            addToDesktop();
            setVisible (true);
        }

        void closeButtonPressed() override { JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

    std::unique_ptr<juce::FileLogger> logger;
    HZLook look;   // the program's look, installed before any window exists
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (HZDrummerApplication)
