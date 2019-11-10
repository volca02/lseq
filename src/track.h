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

protected:
    Sequence sequences[MAX_SEQUENCE];
};
