#pragma once

#include "common.h"
#include "sequence.h"

class Track {
public:
    static constexpr unsigned MAX_SEQUENCE = 64; // 64 tracks maximum total per track.

    Track() {};

    unsigned get_sequence_count() { return MAX_SEQUENCE; }
    Sequence *get_sequence(unsigned num) {
        if (num >= MAX_SEQUENCE) return nullptr;
        return &sequences[num];
    }

    bool is_muted() const { return muted; }
    void toggle_mute() { muted = !muted; }

    /// 0-15
    uchar get_midi_channel() const { return midi_chan; }

    /// 0-15
    void set_midi_channel(uchar chan) { midi_chan = chan & 0x0F; }

protected:
    uchar midi_chan = 0;
    Sequence sequences[MAX_SEQUENCE];
    bool muted = false;
};
