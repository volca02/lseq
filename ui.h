#pragma once

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
      : UIScreen(ui), time_scaler(0, PPQN),
        note_scaler(NOTE_C3, Launchpad::MATRIX_H)
  {}

  virtual ScreenType get_type() const { return SCR_SEQUENCE; };

  void set_active_sequence(Sequence *seq) { sequence = seq; }

  void on_key(const Launchpad::KeyEvent &ev) override;
  void on_enter() override;
  void on_exit() override;

private:
    void mark_dirty() { repaint(); } // TODO: make this deffered
    void repaint();
    void clear_view();

    // converts view state for given coords to color for rendering
    uchar to_color(unsigned x, unsigned y);

    Sequence *sequence = nullptr;

    bool shift = false; // mixer key status

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
    uchar view[Launchpad::MATRIX_W][Launchpad::MATRIX_H] = {};
};

class UI {
public:
    UI(Launchpad &l)
        : launchpad(l)
        , project_screen(*this)
        , track_screen(*this)
        , sequence_screen(*this)
        , current_screen(nullptr)
    {
        launchpad.set_callback(
                [this](Launchpad &l, const Launchpad::KeyEvent &ev) { cb(l, ev); });

        // set current screen to project screen
        set_screen(SCR_SESSION);

        sequence_screen.set_active_sequence(&sdef);
    }

    void set_screen(ScreenType t) {
        UIScreen *next = nullptr;

        switch (t) {
        case SCR_SESSION: next = &project_screen; break;
        case SCR_TRACK: next = &track_screen; break;
        case SCR_SEQUENCE: next = &sequence_screen; break;
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
        case Launchpad::BC_SESSION: if (ev.press) set_screen(SCR_SESSION); return;
        case Launchpad::BC_USER1: if (ev.press) set_screen(SCR_TRACK); return;
        case Launchpad::BC_USER2: if (ev.press) set_screen(SCR_SEQUENCE); return;
        default:
            // event dispatcher. The events get distributed to currently selected UI
            // screen
            if (current_screen) current_screen->on_key(ev);
        }
    }

    Launchpad &launchpad;

    // various screens
    ProjectScreen project_screen;
    TrackScreen track_screen;
    Sequence sdef; // TODO: Remove, just temporary sequence
    SequenceScreen sequence_screen;

    UIScreen *current_screen;
};
