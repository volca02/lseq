#pragma once

#include "common.h"

// A single midi event
class Event {
public:
    enum Status {
        EV_STATUS_BIT       = 0x80,
        EV_NOTE_OFF         = 0x80,
        EV_NOTE_ON          = 0x90,
        EV_AFTERTOUCH       = 0xA0,
        EV_CONTROL_CHANGE   = 0xB0,
        EV_PROGRAM_CHANGE   = 0xC0,
        EV_CHANNEL_PRESSURE = 0xD0,
        EV_PITCH_WHEEL      = 0xE0,
        EV_CLEAR_CHAN_MASK  = 0xF0,
        EV_MIDI_CLOCK       = 0xF8,
        EV_SYSEX            = 0xF0,
        EV_SYSEX_END        = 0xF7
    };

    bool is_linked() const { return linked; }
    Event *get_link() const { return linked; }
    void clear_link() { link(nullptr); }
    void link(Event *l) { linked = l; }

    // used when processing
    bool is_marked() const { return marked; }
    void mark() { marked = true; }
    void unmark() { marked = false; }

    uchar get_note() const { return data[1]; }
    Event &set_note(uchar note) { data[1] = note & 0x7F; return *this; }

    uchar get_velocity() const { return data[2]; }
    Event &set_velocity(uchar v) { data[2] = v & 0x7F; return *this;  }

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

    // event ranking for note ordering purposes
    int get_rank() const {
        // basically identical to stuff in seq24's event.cpp
        switch (status) {
        case EV_NOTE_OFF: return 10;
        case EV_NOTE_ON: return 9;
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
    ticks tick;
    uchar status;
    uchar data[2];
    bool  marked  = false;
    Event *linked = nullptr;
};
