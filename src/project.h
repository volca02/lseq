#pragma once

#include "common.h"
#include "track.h"


/// Holds all project data for one session. Should allow
/// serialization/deserialization to persist the projects, and
/// act as a central storage for all the project data.
class Project {
public:
    static constexpr unsigned MAX_TRACK = 16; // 16 tracks maximum total.

    Project() : bpm(DEFAULT_BPM) {}

    // sets the projects BPM tempo.
    void set_bpm(double b) { bpm = b; }
    double get_bpm() const { return bpm; }

    unsigned get_track_count() { return MAX_TRACK; }
    Track *get_track(unsigned num) {
        if (num >= MAX_TRACK) return nullptr;
        return &tracks[num];
    }

protected:
    // TODO: ID
    // TODO: Serialization
    double bpm;
    Track tracks[MAX_TRACK];
};
