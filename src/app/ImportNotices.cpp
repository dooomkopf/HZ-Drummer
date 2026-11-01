#include "app/ImportNotices.h"

#include "app/views/Theme.h"

#include <cmath>

namespace
{
    const juce::Colour green { 0xff39ff14 };    // the neon of the ghost menu
    const juce::Colour orange { 0xffffa040 };
    constexpr int keep = 50;

    // what the click shows: the messages, newest first, problems in orange; two ways on
    class NoticeList : public juce::Component
    {
    public:
        NoticeList (std::vector<ImportNoticeDot::Notice> list, const juce::String& busy, std::function<void()> openFolder,
                    std::function<void()> showLog, std::function<void()> closed)
            : rows (std::move (list)), reading (busy), onClosed (std::move (closed))
        {
            folderButton.onClick = [openFolder] { if (openFolder) openFolder(); };
            logButton.onClick = [showLog] { if (showLog) showLog(); };
            for (auto* b : { &folderButton, &logButton })
                addAndMakeVisible (*b);
            addAndMakeVisible (view);
            view.setViewedComponent (&lines, false);
            view.setScrollBarsShown (true, false);
            lines.paintRows = [this] (juce::Graphics& g, int width) { paintRows (g, width); };
            const int count = (int) rows.size() + (reading.isNotEmpty() ? 1 : 0);
            setSize (720, juce::jlimit (120, 460, 70 + juce::jmax (1, count) * rowHeight));
        }

        ~NoticeList() override   { if (onClosed) onClosed(); }   // closed: everything shown counts as read

        void resized() override
        {
            auto r = getLocalBounds().reduced (10);
            auto buttons = r.removeFromBottom (32);
            folderButton.setBounds (buttons.removeFromLeft (200));
            buttons.removeFromLeft (8);
            logButton.setBounds (buttons.removeFromLeft (140));
            r.removeFromBottom (8);
            view.setBounds (r);
            const int count = (int) rows.size() + (reading.isNotEmpty() ? 1 : 0);
            lines.setSize (r.getWidth() - 12, juce::jmax (r.getHeight(), juce::jmax (1, count) * rowHeight));
        }

    private:
        struct Lines : juce::Component
        {
            std::function<void (juce::Graphics&, int)> paintRows;
            void paint (juce::Graphics& g) override   { if (paintRows) paintRows (g, getWidth()); }
        };

        void paintRows (juce::Graphics& g, int width)
        {
            g.setFont (juce::FontOptions (15.0f));
            int y = 0;
            const auto row = [&] (const juce::String& time, const juce::String& file, const juce::String& text, juce::Colour c)
            {
                g.setColour (theme::textDim);
                g.drawText (time, 0, y, 90, rowHeight, juce::Justification::centredLeft);
                g.drawFittedText (file, 92, y, 230, rowHeight, juce::Justification::centredLeft, 1);
                g.setColour (c);
                g.drawFittedText (text, 330, y, width - 330, rowHeight, juce::Justification::centredLeft, 1);
                y += rowHeight;
            };
            if (reading.isNotEmpty())
                row ("jetzt", reading, juce::String::fromUTF8 ("wird gelesen …"), green);
            for (auto it = rows.rbegin(); it != rows.rend(); ++it)
                row (it->time, it->file, it->text, it->problem ? orange : (it->read ? theme::textMain : green));
            if (rows.empty() && reading.isEmpty())
                row ({}, {}, "Noch keine Meldungen", theme::textDim);
        }

        static constexpr int rowHeight = 24;
        std::vector<ImportNoticeDot::Notice> rows;
        juce::String reading;
        std::function<void()> onClosed;
        juce::Viewport view;
        Lines lines;
        juce::TextButton folderButton { juce::String::fromUTF8 ("Eingangsordner öffnen") }, logButton { "Import-Log" };
    };
}

ImportNoticeDot::ImportNoticeDot()
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    const auto json = juce::JSON::parse (file());
    if (const auto* list = json["meldungen"].getArray())
        for (const auto& m : *list)
            notices.push_back ({ m["zeit"].toString(), m["datei"].toString(), m["text"].toString(), (bool) m["problem"], (bool) m["gelesen"] });
    update();
}

juce::File ImportNoticeDot::file()
{
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".cache/HZ-Drummer/eingang-meldungen.json");
}

void ImportNoticeDot::add (const juce::String& fileName, const juce::String& text, bool problem)
{
    notices.push_back ({ juce::Time::getCurrentTime().formatted ("%d.%m. %H:%M"), fileName, text, problem, false });
    if ((int) notices.size() > keep)
        notices.erase (notices.begin(), notices.end() - keep);
    save();
    update();
}

void ImportNoticeDot::setBusy (const juce::String& fileName)
{
    busyFile = fileName;
    update();
}

void ImportNoticeDot::update()
{
    int unread = 0;
    for (const auto& n : notices)
        unread += n.read ? 0 : 1;
    setVisible (busyFile.isNotEmpty() || unread > 0);
    setTooltip (busyFile.isNotEmpty() ? juce::String::fromUTF8 ("Eingangsordner: liest ") + busyFile + juce::String::fromUTF8 (" …")
                                      : "Eingangsordner: " + juce::String (unread) + (unread == 1 ? " neue Meldung" : " neue Meldungen")
                                            + juce::String::fromUTF8 (", Klick zeigt sie"));
    if (busyFile.isNotEmpty())
        startTimerHz (30);   // pulse while reading
    else
        stopTimer();
    repaint();
}

void ImportNoticeDot::timerCallback()
{
    phase = std::fmod (phase + 0.06f, juce::MathConstants<float>::twoPi);
    repaint();
}

void ImportNoticeDot::paint (juce::Graphics& g)
{
    bool problem = false;
    for (const auto& n : notices)
        problem = problem || (! n.read && n.problem);
    const auto colour = problem && busyFile.isEmpty() ? orange : green;
    const float alpha = busyFile.isNotEmpty() ? 0.45f + 0.55f * (0.5f + 0.5f * std::sin (phase)) : 1.0f;
    const auto dot = getLocalBounds().toFloat().withSizeKeepingCentre (12.0f, 12.0f);
    g.setColour (colour.withAlpha (0.25f * alpha));
    g.fillEllipse (dot.expanded (4.0f));   // a faint glow around it
    g.setColour (colour.withAlpha (alpha));
    g.fillEllipse (dot);
}

void ImportNoticeDot::mouseUp (const juce::MouseEvent& e)
{
    if (! e.mods.isLeftButtonDown() || ! getLocalBounds().contains (e.getPosition()))
        return;
    auto list = std::make_unique<NoticeList> (notices, busyFile, onOpenFolder, onShowLog,
                                              [safe = juce::Component::SafePointer<ImportNoticeDot> (this)]
                                              {
                                                  if (safe != nullptr)
                                                      safe->markRead();
                                              });
    juce::CallOutBox::launchAsynchronously (std::move (list), getScreenBounds(), nullptr);
}

void ImportNoticeDot::markRead()
{
    for (auto& n : notices)
        n.read = true;
    save();
    update();
}

void ImportNoticeDot::save() const
{
    juce::Array<juce::var> list;
    for (const auto& n : notices)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("zeit", n.time);
        o->setProperty ("datei", n.file);
        o->setProperty ("text", n.text);
        o->setProperty ("problem", n.problem);
        o->setProperty ("gelesen", n.read);
        list.add (juce::var (o));
    }
    auto* root = new juce::DynamicObject();
    const juce::var keepAlive (root);
    root->setProperty ("meldungen", list);
    file().getParentDirectory().createDirectory();
    file().replaceWithText (juce::JSON::toString (keepAlive), false, false, "\n");
}
