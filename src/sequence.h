#pragma once

#include <mutex>
#include <list>
#include <functional>

#include "event.h"

// I drew inspiration in seq24's code here.

// a single linear sequence of notes
class Sequence {
public:
    using Events = std::list<Event>;
    using mutex = std::mutex;
    using lock  = std::unique_lock<std::mutex>;

    // unmarks all notes
    void unmark_all();

    // unselects all notes
    void unselect_all();

    // adds a note into the sequence
    void add_note(ticks start, ticks length, uchar note,
                  uchar velocity = DEFAULT_VELOCITY);

    // marks a specified note(s) from the sequence - by window (unmarks all first)
    void mark_range(ticks start, ticks end, uchar note_low, uchar note_hi);

    // selectes a specified note(s) from the sequence - by window (unmarks all first)
    void select_range(ticks start, ticks end, uchar note_low, uchar note_hi, bool toggle = true);

    // removes marked notes
    void remove_marked();

    // sets note length for marked range
    void set_note_lengths(ticks l);

    // sets the overall length of the sequence
    void set_length(ticks l);

    // length in ticks of the sequence
    ticks get_length() const { return length; }

    // sets velocity for marked notes
    void set_note_velocities(uchar velo);

    // returns average velocity for marked range. unmarks all note_ons
    uchar get_average_velocity();

    // TODO: Make this accept 3 params and return 3 params (start, pitch, length) and allow user to change the lengths as well
    // moves notes in time/pitch as specified by the callback
    void move_selected_notes(
            std::function<std::pair<ticks, uchar>(ticks, uchar)> mover);

    struct handle {
        using const_iterator = Events::const_iterator;

        handle(Sequence &s) : s(s), l(s.mtx) {
        }

        handle(handle &&h) : s(h.s), l(std::move(h.l)) {
        }

        const_iterator begin() const { return s.events.begin(); }
        const_iterator end()   const { return s.events.end(); }

        Sequence &s;
        lock l;
    };

    handle get_handle() { return {*this}; }

    bool is_empty() const;

    // user-defined flags
    unsigned get_flags() const { return flags; }
    void set_flags(unsigned f) { flags = f; }

protected:
    // re-links all note on/offs, purges singluar events (note ons without note
    // offs and vice versa)
    void _tidy();

    // unlocked versions of the public methods
    void _unmark_all();
    void _remove_marked();
    // adds note, does NOT sort. _tidy is mandatory call before unlocking the sequence
    void _add_note(ticks start, ticks length, uchar note, uchar velocity, bool selected = false);

    // adds an event into the right sorted place in sequence
    void _add_event(const Event &ev);

    Events events;
    ticks length;

    unsigned flags = 0;

    // mutex for multithreaded access locking
    mutable std::mutex mtx;
};
