#pragma once

#include "common.h"

/// Holds all project data for one session. Should allow
/// serialization/deserialization to persist the projects, and
/// act as a central storage for all the project data.
class Project {
public:
    Project() : bpm(DEFAULT_BPM) {}

    // sets the projects BPM tempo.
    void set_bpm(double b) { bpm_ = b; }
    double get_bpm() const { return bpm; }

protected:
    double bpm;
};
