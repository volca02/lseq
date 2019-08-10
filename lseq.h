#pragma once

#include <map>
#include <condition_variable>

#include "launchpad.h"
#include "ui.h"

/** Main class - holds stuff together
 */
class LSeq {
public:
    // TODO: Find available output midi devices - or let user specify
    LSeq() {
        find_and_spawn();
    }

    ~LSeq() {
    }

    void run() {
        // TODO: watch for C-c and terminate cleanly here
        while (true) {
            std::unique_lock<std::mutex> lk(m);
            cv.wait(lk);

            if (do_exit) break;

            // iterate uis and update them
            for (auto &u : launchpads)
                u.second.ui.update();
        }
    }

    void wake_up() {
        cv.notify_one();
    };

    void exit() {
        do_exit = true;
        cv.notify_one();
    }

private:
    // finds all launchpads and spawns UIs for them
    void find_and_spawn() {
        RtMidiIn midi;

        for (unsigned int i = 0; i < midi.getPortCount(); i++) {
            try {
                if (Launchpad::matchName(midi.getPortName(i))) {
                    launchpads.try_emplace(i, *this, i);
                }
            } catch (RtMidiError &error) {
                error.printMessage();
            }
        }
    }

    struct LaunchpadUI {
        LaunchpadUI(LSeq &lseq, int port) : l(port), ui(lseq, l) {}

        // not copyable. just to be sure here
        LaunchpadUI(LaunchpadUI &o) = delete;
        LaunchpadUI &operator=(LaunchpadUI &o) = delete;

        Launchpad l;
        UI ui;
    };


    std::mutex m;
    std::atomic<bool> do_exit = false;
    std::map<int, LaunchpadUI> launchpads;
    std::condition_variable cv;
};
