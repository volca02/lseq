#pragma once

#include <mutex>
#include <list>

#include "event.h"

// I drew inspiration in seq24's code here.

// a single linear sequence of notes
class Sequence {
public:
    using Events = std::list<Event>;
    using const_iterator = Events::const_iterator;

    // unmarks all notes
    void unmark_all();

    // adds a note into the sequence
    void add_note(ticks start, ticks length, uchar note);

    // marks a specified note(s) from the sequence - by window (unmarks all first)
    void mark_range(ticks start, ticks end, uchar note_low, uchar note_hi);

    // removes marked notes
    void remove_marked();

    // sets note length for marked range
    void set_length(ticks l);

    const_iterator begin() const { return events.begin(); }
    const_iterator end()   const { return events.end(); }

protected:
    using mutex = std::mutex;
    using lock  = std::scoped_lock<std::mutex>;

    // re-links all note on/offs, purges singluar events (note ons without note
    // offs and vice versa)
    void _tidy();

    // unlocked versions of the public methods
    void _unmark_all();
    void _remove_marked();
    void _add_note(ticks start, ticks length, uchar note);

    // adds an event into the right sorted place in sequence
    void _add_event(const Event &ev);

    Events events;

    // mutex for multithreaded access locking
    std::mutex mtx;
};
