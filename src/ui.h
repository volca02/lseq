#pragma once

#include <atomic>

#include "common.h"
#include "launchpad.h"
#include "sequence.h"

class UI;
class LSeq;
class Project;
class Track;

enum ScreenType {
    SCR_TRACK    = 1, // main project screen
    SCR_SONG     = 2, // track screen - flow of sequences
    SCR_SEQUENCE = 3 // a single sequence view
};

// UI screen interface
class UIScreen {
public:
    UIScreen(const UIScreen &) = delete;

    UIScreen(UI &ui);

    virtual void on_key(const Launchpad::KeyEvent &ev) = 0;

    virtual ScreenType on_enter() = 0;

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

/** Project/Track screen. Track sequence contents, track mapping to midi channels...
 */
class TrackScreen : public UIScreen {
public:
    TrackScreen(UI &ui, Project &project)
            : UIScreen(ui), project(project), updates(this)
    {
    }

    virtual ScreenType get_type() const override { return SCR_TRACK; };

    void on_key(const Launchpad::KeyEvent &ev) override;
    ScreenType on_enter() override;
    void on_exit() override;

    void update() override;

private:
    // TODO: Could we base this on some baseclass perhaps?
    struct UpdateBlock {
        UpdateBlock(TrackScreen *s = nullptr) : owner(s) {}

        void clear() {
            up_down = 0;
            left_right = 0;
            side_buttons = 0;
            grid_on.clear();
            grid_off.clear();
            shift_grid_on.clear();
            dirty = false;
        }

        void mark_dirty() {
            dirty = true;
            if (owner) owner->wake_up();
        }

        UpdateBlock &operator=(UpdateBlock &o) {
            left_right      = o.left_right;
            up_down         = o.up_down;
            side_buttons    = o.side_buttons;
            grid_on         = o.grid_on;
            grid_off        = o.grid_off;
            shift_grid_on   = o.shift_grid_on;
            return *this;
        }

        TrackScreen *owner;
        std::atomic<bool> dirty;

        int up_down    = 0; // counts requests to move up/down
        int left_right = 0; // counts requests to move left/right
        unsigned side_buttons = 0; // bitmap of side buttons pressed
        Launchpad::Bitmap grid_on;  // key-on events from the grid
        Launchpad::Bitmap grid_off; // key-off envets from the grid
        Launchpad::Bitmap shift_grid_on;  // any shift pressed button is stored here
    };

    // total repaint of the view
    void repaint();

    Track    *get_track_for_y(uchar y);
    std::pair<Track *, Sequence *> get_seq_for_xy(uchar x, uchar y);
    bool      schedule_sequence_for_xy(uchar x, uchar y);

    std::atomic<bool> shift = false; // mixer key status TODO: make it thread safe?

    Project &project;

    // View coords
    int vx = 0, vy = 0;

    Launchpad::Bitmap held_buttons;
    Launchpad::Bitmap shift_held_buttons;
    UpdateBlock updates;
};

/** Track screen. Shows the flow of all the sequences in project
 */
class SongScreen : public UIScreen {
public:
    SongScreen(UI &ui) : UIScreen(ui) {}

    virtual ScreenType get_type() const override { return SCR_TRACK; };

    void on_key(const Launchpad::KeyEvent &ev) override;
    ScreenType on_enter() override;
    void on_exit() override;

private:
    // total repaint of the view
    void repaint();
};

// Sequence screen. Shows one single sequence as either drum or melodic sequence
class SequenceScreen : public UIScreen {
public:
    // default to quarter note view...
    SequenceScreen(UI &ui)
        : UIScreen(ui)
        , updates(this)
        , time_scaler(0)
        , note_scaler(NOTE_C3, Launchpad::MATRIX_H)
    {}

    virtual ScreenType get_type() const override { return SCR_SEQUENCE; };

    void set_active_sequence(Track *track, Sequence *seq);

    void on_key(const Launchpad::KeyEvent &ev) override;
    ScreenType on_enter() override;
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

    Track    *track    = nullptr;
    Sequence *sequence = nullptr;

    // contains all events accumulated
    struct UpdateBlock {
        UpdateBlock(SequenceScreen *s = nullptr) : owner(s) {}

        void clear() {
            up_down = 0;
            time_scale = 0;
            left_right = 0;
            switch_triplets = false;
            switch_scale    = false;
            side_buttons = 0;
            shift_only = false;
            shift_held = 0;
            grid_on.clear();
            grid_off.clear();
            shift_grid_on.clear();
            dirty = false;
        }

        void mark_dirty() {
            dirty = true;
            if (owner) owner->wake_up();
        }

        UpdateBlock &operator=(UpdateBlock &o) {
            left_right      = o.left_right;
            time_scale      = o.time_scale;
            up_down         = o.up_down;
            switch_triplets = o.switch_triplets;
            switch_scale    = o.switch_scale;
            side_buttons    = o.side_buttons;
            grid_on         = o.grid_on;
            shift_grid_on   = o.shift_grid_on;
            grid_off        = o.grid_off;
            shift_only      = o.shift_only;
            shift_held      = o.shift_held;
            return *this;
        }

        SequenceScreen *owner;
        std::atomic<bool> dirty;

        int left_right = 0; // counts requests to move left/right (in time view)
        int time_scale = 0; // counts requests to scale the timing up/down
        int up_down    = 0; // counts requests to move up/down (in note view)
        bool switch_triplets = false; // indicates triplet switch was requested
        bool switch_scale    = false; // indicates scale switch was requested
        unsigned side_buttons = 0; // bitmap of side buttons pressed
        bool shift_only = false; // only shift was held, no button pressed
        time_t shift_held = 0; // time in seconds the shift was held for
        Launchpad::Bitmap grid_on;  // any pressed button is stored here
        Launchpad::Bitmap shift_grid_on;  // any shift pressed button is stored here
        Launchpad::Bitmap grid_off; // any pressed button is stored here
    };

    /// Used in on_key, different thread than the rest!
    bool shift = false; // mixer key status TODO: make it thread safe?
    bool shift_only = false;
    time_t shift_start = 0; // when the shift was pressed. used on release
    UpdateBlock updates; // current updates - accessed in both threads
    /// end of on_key variable block

    void add_note(unsigned x, unsigned y, bool repaint);
    void remove_note(unsigned x, unsigned y, bool repaint);
    void set_note_lengths(unsigned x, unsigned y, unsigned len, bool repaint);
    void set_note_velocities(uchar velo);
    void move_selected_notes(int mx, int my);
    void paint_sidebar_value(uchar val, uchar color);
    void paint_status_sidebar();
    uchar get_average_held_velocity();

    void queue_note_on(uchar n, uchar vel);
    void queue_note_off(uchar n);

    Launchpad::Bitmap held_buttons;
    Launchpad::Bitmap modified_notes;

    // counter of visible marked notes
    unsigned marked_notes;

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
        FS_IS_SELECTED  = 64, // note is selected for editing
        FS_SEQ_END    = 128 // sequence ends here
    };

    // this encodes the current view. each field is a bitmap (see FieldStatus)
    View view = {};
};

class UI {
public:
    UI(LSeq &owner, Project &project, Launchpad &l)
        : owner(owner)
        , launchpad(l)
        , track_screen(*this, project)
        , song_screen(*this)
        , sequence_screen(*this)
        , current_screen(nullptr)
    {
        // set current screen to project screen
        set_screen(SCR_TRACK);
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

    // returns owner of this ui - i.e. the center lseq object
    LSeq &get_owner() { return owner; }

    UIScreen *get_screen(ScreenType st);

private:
    friend class UIScreen;

    UI(UI &o) = delete;
    UI &operator=(UI &o) = delete;

    void cb(Launchpad &l, const Launchpad::KeyEvent &ev) {
        // we have top 3 buttons on the right reserved to screen change
        // NOTE: could use a 2 screen mode (red/green) on any of these (press again to swap screens)
        switch (ev.code) {
        case Launchpad::BC_SESSION: if (ev.press) set_screen(SCR_TRACK); return;
        case Launchpad::BC_USER1: if (ev.press) set_screen(SCR_SONG); return;
        case Launchpad::BC_USER2: if (ev.press) set_screen(SCR_SEQUENCE); return;
        default:
            // event dispatcher. The events get distributed to currently selected UI
            // screen
            UIScreen *screen = get_current_screen();
            if (screen) screen->on_key(ev);
        }
    }

public:
    LSeq &owner;
    Launchpad &launchpad;

    // various screens
    TrackScreen track_screen;
    SongScreen song_screen;
    SequenceScreen sequence_screen;

protected:
    // current screen has to be locked via mutex while accesing
    mutable std::mutex mut;
    UIScreen *current_screen;
};
