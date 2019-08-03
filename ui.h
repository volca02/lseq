#pragma once

#include "launchpad.h"

class UI;
class Sequencer;

enum ScreenType {
    SCR_PROJECT = 1, // main project screen
    SCR_TRACK   = 2, // TO BE SPECIFIED
    SCR_UNDEF2  = 3,
    SCR_UNDEF3  = 4
};

// UI screen interface
class UIScreen {
public:
    UIScreen(const UIScreen &) = delete;

    UIScreen(UI &ui);

    virtual void on_key(const Launchpad::KeyEvent &ev) = 0;

    virtual void on_enter() {};

    virtual void on_exit() {};

    virtual ScreenType get_type() const = 0;

protected:
    void set_active_mode_button(unsigned o);

    UI &ui;
    Launchpad &launchpad;
};

/** Project screen. Track mapping to midi channels
 * Upper 2 Rows is midi tracks
 * Bottom buttons are track type selectors: NONE, ...
 */
class ProjectScreen : public UIScreen {
public:
    ProjectScreen(UI &ui) : UIScreen(ui) {}

    virtual ScreenType get_type() const { return SCR_PROJECT; };

    void on_key(const Launchpad::KeyEvent &ev) override;
    void on_enter() override;
    void on_exit() override;

private:
    void repaint();
};

/** Track screen. Shows the flow of the current track
 */
class TrackScreen : public UIScreen {
public:
    TrackScreen(UI &ui) : UIScreen(ui) {}

    virtual ScreenType get_type() const { return SCR_TRACK; };

    void on_key(const Launchpad::KeyEvent &ev) override;
    void on_enter() override;
    void on_exit() override;

private:
    void repaint();
};

class UI {
public:
    UI(Sequencer &s, Launchpad &l)
        : sequencer(s)
        , launchpad(l)
        , project_screen(*this)
        , track_screen(*this)
        , current_screen(nullptr)
    {
        launchpad.set_callback(
                [this](Launchpad &l, const Launchpad::KeyEvent &ev) { cb(l, ev); });

        // set current screen to project screen
        set_screen(SCR_PROJECT);
    }

    void set_screen(ScreenType t) {
        UIScreen *next = nullptr;

        switch (t) {
        case SCR_PROJECT: next = &project_screen; break;
        case SCR_TRACK: next = &track_screen; break;
        }

        if (next != current_screen) {
            if (current_screen) current_screen->on_exit();
            if (next) next->on_enter();
            current_screen = next;
        }
    }

private:
    friend class UIScreen;

    void cb(Launchpad &l, const Launchpad::KeyEvent &ev) {
        // we have top 4 buttons on the right reserved to screen change
        switch (ev.code) {
        case Launchpad::BC_SESSION: set_screen(SCR_PROJECT); return;
        case Launchpad::BC_USER1: set_screen(SCR_TRACK); return;
        case Launchpad::BC_USER2: set_screen(SCR_UNDEF2); return;
        case Launchpad::BC_MIXER: set_screen(SCR_UNDEF3); return;
        default:
            // event dispatcher. The events get distributed to currently selected UI
            // screen
            if (current_screen) current_screen->on_key(ev);
        }
    }

    Sequencer &sequencer;
    Launchpad &launchpad;

    // various screens
    ProjectScreen project_screen; // the default screen
    TrackScreen track_screen;

    UIScreen *current_screen;
};
