#include "app/MainComponent.h"
#include "Version.h"
#include "app/views/HZLook.h"

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
        juce::LookAndFeel::setDefaultLookAndFeel (&look);
        mainWindow = std::make_unique<MainWindow> (getApplicationName());

        // --editor: open the grid editor directly (for tests and screenshots)
        if (auto* mc = dynamic_cast<MainComponent*> (mainWindow->getContentComponent()))
        {
            if (commandLine.contains ("--editor"))
                mc->openEditorForTesting();
            if (commandLine.contains ("--arranger"))
                mc->openArrangerForTesting();
            if (commandLine.contains ("--play"))   // play immediately (tests/screenshots)
                mc->startForTesting();
            if (commandLine.contains ("--mixer"))
                mc->openMixerForTesting();
        }
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
        juce::Logger::writeToLog ("HZ-Drummer beendet");
        juce::Logger::setCurrentLogger (nullptr); logger.reset();
    }

    void systemRequestedQuit() override { quit(); }

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
    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (const juce::String& name)
            : DocumentWindow (name, juce::Colour (0xff121214), DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);
            setResizable (true, true);
            setResizeLimits (640, 420, 4000, 3000);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }

        void closeButtonPressed() override { JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

    std::unique_ptr<juce::FileLogger> logger;
    HZLook look;   // the program's look, installed before any window exists
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (HZDrummerApplication)
