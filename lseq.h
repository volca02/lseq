#pragma once

#include <map>
#include <condition_variable>

#include "launchpad.h"
#include "ui.h"

#include "jackmidi.h"

/** Main class - holds stuff together
 */
class LSeq : public jack::Client::Callback {
public:
    // TODO: Find available output midi devices - or let user specify
    LSeq() : client("lseq") {
        client.set_callback(*this);
        client.activate();
        spawn();
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

    int process(jack_nframes_t nframes) override {
        for (auto &u : launchpads) u.second.l.process(nframes);
        return 0;
    }

private:
    void spawn() {
        launchpads.try_emplace(
                0,
                client,
                0,
                *this,
                "a2j:Launchpad (capture): Launchpad MIDI 1",
                "a2j:Launchpad (playback): Launchpad MIDI 1");
    }

    struct LaunchpadUI {
        LaunchpadUI(jack::Client &client,
                    int order,
                    LSeq &lseq,
                    const char *inport,
                    const char *outport)
                : l(client, "launchpad " + std::to_string(order)), ui(lseq, l)
        {
            l.connect(inport, outport);
        }

        // not copyable. just to be sure here
        LaunchpadUI(LaunchpadUI &o) = delete;
        LaunchpadUI &operator=(LaunchpadUI &o) = delete;

        Launchpad l;
        UI ui;
    };


    std::mutex m;
    std::atomic<bool> do_exit = false;
    std::map<int, LaunchpadUI> launchpads;
    jack::Client client;
    std::condition_variable cv;
};
