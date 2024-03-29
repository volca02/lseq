#pragma once

#include "error.h"
#include "common.h"

// A single midi event
class Event {
public:
    Event() : marked(false) {};

    Event(const unsigned char *srcbuf, int srcsize) {
        if (srcsize != 3)
            throw Exception("Midi Event: Input source buffer size mismatch");

        status = srcbuf[0];
        data[0] = srcbuf[1];
        data[1] = srcbuf[2];
    }

    bool is_linked() const { return linked; }
    Event *get_link() const { return linked; }
    void clear_link() { link(nullptr); }
    void link(Event *l) { linked = l; }

    // used when processing
    bool is_marked() const { return marked; }
    void mark() { marked = true; }
    void unmark() { marked = false; }

    // selection - used when transposing etc
    bool is_selected() const { return selected; }
    void select() { selected = true; }
    void unselect() { selected = false; }
    // sets selected or if toggle is set, inverts the selection
    void select_or_toggle(bool toggle) { selected = !(selected && toggle); };
    Event &set_selected(bool sel) { selected = sel; return *this; }

    uchar get_note() const { return data[0]; }
    Event &set_note(uchar note) { data[0] = note & 0x7F; return *this; }

    uchar get_velocity() const { return data[1]; }
    Event &set_velocity(uchar v) { data[1] = v & 0x7F; return *this;  }

    bool is_note_on() const { return status == EV_NOTE_ON; }
    bool is_note_off() const { return status == EV_NOTE_OFF; }

    uchar get_status() const { return status; }

    // sets the status byte, clearing out the midi channel portion
    Event &set_status(uchar st) {
        if (st >= EV_SYSEX)
            status = st;
        else
            status = st & EV_CLEAR_CHAN_MASK;
        return *this;
    }

    ticks get_ticks() const { return tick; }
    Event &set_ticks(ticks t) { tick = t;  return *this; }

    /** for linked events, this returns the length of the note */
    ticks get_length() const {
        if (!linked) return 0;
        if (linked->get_ticks() < get_ticks()) return 0;
        return linked->get_ticks() - get_ticks();
    }

    // will move the note-off event's tick time
    // this will most probably invalidate sequence in which this event resides
    void set_length(ticks len) {
        if (!linked) return;
        if (linked->get_ticks() < get_ticks()) {
            // we're the note-off
            tick = linked->get_ticks() + len;
        } else {
            linked->tick = get_ticks() + len;
        }
    }

    // returns pointer to data (the second and third byte of the message triad)
    const uchar *get_data() const { return data; }

    // event ranking for note ordering purposes
    int get_rank() const {
        // basically identical to stuff in seq24's event.cpp
        switch (status) {
        case EV_NOTE_OFF: return 9;
        case EV_NOTE_ON: return 10;
        case EV_AFTERTOUCH:
        case EV_CHANNEL_PRESSURE:
        case EV_PITCH_WHEEL: return 5;
        case EV_CONTROL_CHANGE: return 1;
        default: return 0;
        }
    }

    bool operator>(const Event &o) const {
        if (tick == o.tick)
            return get_rank() > o.get_rank();
        else
            return tick > o.tick;
    }

    bool operator<(const Event &o) const {
        if (tick == o.tick)
            return get_rank() < o.get_rank();
        else
            return tick < o.tick;
    }

protected:
    ticks tick    = 0;
    uchar status  = 0;
    uchar data[2] = {0x0, 0x0};
    bool  marked    = false;
    bool  selected  = false;
    Event *linked   = nullptr;
};
