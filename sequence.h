#pragma once

#include <mutex>
#include <list>

#include "event.h"

// I drew inspiration in seq24's code here.

// a single linear sequence of notes
class Sequence {
public:
    using Events = std::list<Event>;
    using mutex = std::mutex;
    using lock  = std::scoped_lock<std::mutex>;

    // unmarks all notes
    void unmark_all();

    // adds a note into the sequence
    void add_note(ticks start, ticks length, uchar note,
                  uchar velocity = DEFAULT_VELOCITY);

    // marks a specified note(s) from the sequence - by window (unmarks all first)
    void mark_range(ticks start, ticks end, uchar note_low, uchar note_hi);

    // removes marked notes
    void remove_marked();

    // sets note length for marked range
    void set_length(ticks l);

    struct handle {
        using const_iterator = Events::const_iterator;

        handle(Sequence &s) : s(s), l(s.mtx) {
        }

        const_iterator begin() const { return s.events.begin(); }
        const_iterator end()   const { return s.events.end(); }

        Sequence &s;
        lock l;
    };

    handle get_handle() { return {*this}; }

protected:
    // re-links all note on/offs, purges singluar events (note ons without note
    // offs and vice versa)
    void _tidy();

    // unlocked versions of the public methods
    void _unmark_all();
    void _remove_marked();
    // adds note, does NOT sort. _tidy is mandatory call before unlocking the sequence
    void _add_note(ticks start, ticks length, uchar note, uchar velocity);

    // adds an event into the right sorted place in sequence
    void _add_event(const Event &ev);

    Events events;

    // mutex for multithreaded access locking
    std::mutex mtx;
};
