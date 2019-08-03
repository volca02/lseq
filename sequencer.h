#pragma once

#include "launchpad.h"
#include "ui.h"

class Sequencer {
public:
    Sequencer() : l(), ui(*this, l) {
    }

    ~Sequencer() {
    }

    void run() {
        std::cout << "\nReading MIDI input ... press <enter> to quit.\n";
        char input;
        std::cin.get(input);
    }

private:
    // TODO: support any no. of devices by scanning them and incoroprating them.
    Launchpad l;
    UI ui;
};
