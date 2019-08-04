#pragma once

#include "launchpad.h"
#include "ui.h"

/** Main class - holds stuff together
 */
class LSeq {
public:
    // TODO: Scan the midi bus, instantiate UI views for all Launchpads found
    // TODO: Find available output midi devices - or let user specify
    LSeq() : l(), ui(l) {
    }

    ~LSeq() {
    }

    void run() {
        // NOTE: Temporary. Will be replaced by a mutex probably.
        // TODO: watch for C-c and terminate cleanly here
        std::cout << "\nReading MIDI input ... press <enter> to quit.\n";
        char input;
        std::cin.get(input);
    }

private:
    // TODO: support any no. of devices by scanning them and incoroprating them.
    Launchpad l;
    UI ui;
};
