#pragma once

#include <atomic>

#include "common.h"
#include "launchpad.h"
#include "sequence.h"

class UI;
class LSeq;

enum ScreenType {
    SCR_SESSION  = 1, // main project screen
    SCR_TRACK    = 2, // track screen - flow of sequences
    SCR_SEQUENCE = 3 // a single sequence view
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

    virtual void update() {};

    virtual void wake_up();

protected:
    using mutex = std::mutex;
    using lock  = std::scoped_lock<std::mutex>;

    void set_active_mode_button(unsigned o);

    UI &ui;
    Launchpad &launchpad;

    // mutex for multithreaded access locking
    std::mutex mtx;

};

/** Project screen. Track mapping to midi channels
 * Upper 2 Rows is midi tracks
 * Bottom buttons are track type selectors: NONE, ...
 */
class ProjectScreen : public UIScreen {
public:
    ProjectScreen(UI &ui) : UIScreen(ui) {}

    virtual ScreenType get_type() const { return SCR_SESSION; };

    void on_key(const Launchpad::KeyEvent &ev) override;
    void on_enter() override;
    void on_exit() override;

private:
    void repaint();
};

/** Track screen. Shows the flow of all the sequences in project
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

// Sequence screen. Shows one single sequence as either drum or melodic sequence
class SequenceScreen : public UIScreen {
public:
    // default to quarter note view...
    SequenceScreen(UI &ui)
        : UIScreen(ui)
        , updates(this)
        , time_scaler(0, PPQN) // we default to quarter note steps
        , note_scaler(NOTE_C3, Launchpad::MATRIX_H)
    {}

    virtual ScreenType get_type() const { return SCR_SEQUENCE; };

    void set_active_sequence(Sequence *seq);

    void on_key(const Launchpad::KeyEvent &ev) override;
    void on_enter() override;
    void on_exit() override;

    virtual void update() override;

private:
    using View = uchar[Launchpad::MATRIX_W][Launchpad::MATRIX_H];

    // total repaint of the view
    void repaint();

    // updates dirty parts of the grid onl
    void paint();

    // clears up the view - prepares it to be filled with notes
    void clear_view();

    // converts view state for given coords to color for rendering
    uchar to_color(View &v, unsigned x, unsigned y);

    uchar bg_flags(unsigned x, unsigned y);

    Sequence *sequence = nullptr;

    bool shift = false; // mixer key status TODO: make it thread safe?

    // contains all events accumulated
    struct UpdateBlock {
        UpdateBlock(SequenceScreen *s = nullptr) : owner(s) {}

        void clear() {
            time_shift = 0;
            time_scale = 0;
            note_shift = 0;
            grid_on.clear();
            grid_off.clear();
            dirty = false;
        }

        void mark_dirty() {
            dirty = true;
            if (owner) owner->wake_up();
        }

        UpdateBlock &operator=(UpdateBlock &o) {
            time_shift = o.time_shift;
            time_scale = o.time_scale;
            note_shift = o.note_shift;
            grid_on = o.grid_on;
            grid_off = o.grid_off;
            return *this;
        }

        SequenceScreen *owner;
        std::atomic<bool> dirty;
        int time_shift = 0;
        int time_scale = 0;
        int note_shift = 0;
        Launchpad::Bitmap grid_on;  // any pressed button is stored here
        Launchpad::Bitmap grid_off; // any pressed button is stored here
    };

    void add_note(unsigned x, unsigned y, bool repaint);
    void remove_note(unsigned x, unsigned y, bool repaint);
    void set_note_lengths(unsigned x, unsigned y, unsigned len, bool repaint);

    UpdateBlock updates; // current updates
    Launchpad::Bitmap held_buttons;
    Launchpad::Bitmap modified_notes;

    TimeScaler time_scaler;
    NoteScaler note_scaler;

    // Field status bitmap
    enum FieldStatus {
        FS_HAS_NOTE   = 1, // at least one note is present
        FS_MULTIPLE   = 2, // more than one note is present
        FS_INACCURATE = 4, // some notes present are not quantized according to current view
        FS_CONT       = 8, // continuation from previous field for longer notes
        FS_IN_SCALE   = 16, // the note presented is in set scale
        FS_SCALE_MARK = 32, // scale indicates mark for this position
    };

    // this encodes the current view. each field is a bitmap (see FieldStatus)
    View view = {};
};

class UI {
public:
    UI(LSeq &owner, Launchpad &l)
        : owner(owner)
        , launchpad(l)
        , project_screen(*this)
        , track_screen(*this)
        , sequence_screen(*this)
        , current_screen(nullptr)
    {
        // set current screen to project screen
        set_screen(SCR_SESSION);

        sequence_screen.set_active_sequence(&sdef);

        launchpad.set_callback(
                [this](Launchpad &l, const Launchpad::KeyEvent &ev) { cb(l, ev); });
    }

    void set_screen(ScreenType t);
    UIScreen *get_current_screen() const;

    void update() {
        UIScreen *screen = get_current_screen();
        if (screen) screen->update();
    }

    // causes the main loop to wake up from sleep
    void wake_up();

private:
    friend class UIScreen;

    UI(UI &o) = delete;
    UI &operator=(UI &o) = delete;

    void cb(Launchpad &l, const Launchpad::KeyEvent &ev) {
        // we have top 4 buttons on the right reserved to screen change
        switch (ev.code) {
        case Launchpad::BC_SESSION: if (ev.press) set_screen(SCR_SESSION); return;
        case Launchpad::BC_USER1: if (ev.press) set_screen(SCR_TRACK); return;
        case Launchpad::BC_USER2: if (ev.press) set_screen(SCR_SEQUENCE); return;
        default:
            // event dispatcher. The events get distributed to currently selected UI
            // screen
            UIScreen *screen = get_current_screen();
            if (screen) screen->on_key(ev);
        }
    }

    LSeq &owner;
    Launchpad &launchpad;

    // various screens
    ProjectScreen project_screen;
    TrackScreen track_screen;
    Sequence sdef; // TODO: Remove, just temporary sequence
    SequenceScreen sequence_screen;

    // current screen has to be locked via mutex while accesing
    mutable std::mutex mut;
    UIScreen *current_screen;
};
