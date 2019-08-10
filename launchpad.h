#pragma once

#include <functional>
#include <mutex>
#include <atomic>

#include <RtMidi.h>

#include "common.h"
#include "error.h"

// DOCS HERE https://d2xhy469pqj8rc.cloudfront.net/sites/default/files/novation/downloads/4080/launchpad-programmers-reference.pdf
// NOTE: Launchpad implements double buffering - 0xB0, 0x00, 0x31 - then 0xB0, 0x00, 0x34 to swap pages
// The clear/copy bits in color setting then control what the device does with the other than selected page

// Launchpad MK1. Not doing any abstractions here, but refactor to support other machines should not be hard
class Launchpad {
public:
    using lock  = std::scoped_lock<std::mutex>;

    static const unsigned MATRIX_W = 8;
    static const unsigned MATRIX_H = 8;

    enum ButtonCode {
        BC_UP       = 200,
        BC_DOWN     = 201,
        BC_LEFT     = 202,
        BC_RIGHT    = 203,
        BC_SESSION  = 204,
        BC_USER1    = 205,
        BC_USER2    = 206,
        BC_MIXER    = 207
    };

    enum ButtonType {
        BTN_GRID = 1, // Any grid button (x,y coordinates will be ginen)
        BTN_SIDE,     // Any right side button (y 0..7 gives the button)
        BTN_TOP,      // Top row button (x 0..7 gives the button)
    };

    // some basic colors (not all)
    enum Colors {
        CL_BLACK    = 0x00,
        CL_GREEN    = 0x30,
        CL_RED      = 0x03,
        CL_GREEN_M  = 0x20,
        CL_RED_M    = 0x02,
        CL_GREEN_L  = 0x10,
        CL_RED_L    = 0x01,
        CL_AMBER    = 0x33,
        CL_AMBER_L  = 0x11,
        CL_YELLOW   = 0x32
    };

    // converted keypress -
    struct KeyEvent {
        ButtonType type;
        unsigned code; // button code >=200 means top row, otherwise it's straight from the device
        unsigned int x, y; // coords for grid buttons, X for toprow buttons, Y for siderow
        bool press; // true for press, false for release
    };

    // NOTE: This callback is called from a different thread, so use atomics/mutexes
    using KeyCb    = std::function<void(Launchpad&, const KeyEvent&)>;

    // for fast_fill, this is a callback to get field color based on coords
    using ColorCb  = std::function<uchar(unsigned,unsigned)>;

    /** packed thread safe dirtiness flags for the grid part
     * we know each row can be represented by 8 bits, so half of the grid is uint32_t
     */
    struct Bitmap {

        void mark(unsigned x, unsigned y) {
            if (x >= MATRIX_W) return;
            if (y >= MATRIX_H) return;

            unsigned bank = y/4;
            unsigned bit  = x + (y & 0x03) * 8; // max is 7 + 3*8 == 31

            bits[bank] |= 1 << bit;
        }

        // iterates the bitfields and calls a callback
        template<typename CbT>
        void iterate(CbT cb) const {
            for (unsigned x = 0; x < MATRIX_W; ++x) {
                for (unsigned y = 0; y < MATRIX_H; ++y) {
                    unsigned bank = y/4;
                    unsigned bit  = x + (y & 0x03) * 8; // max is 7 + 3*8 == 31

                    if (bits[bank] & (1 << bit)) cb(x, y);
                }
            }
        }

        void clear() {
            bits[0] = 0;
            bits[1] = 0;
        }

        bool has_value() const {
            return (bits[0] | bits[1]) != 0;
        }

        uint32_t bits[2] = {0x0,0x0};
    };


    Launchpad(const Launchpad &) = delete;

    Launchpad(int port) : cur_page(false) {
        if (!matchName(midi_in.getPortName(port)))
            throw Exception("Given port is not Launchpad");

        if (port < 0)
            throw Exception("Cannot find Launchpad");

        // Don't ignore sysex, timing, or active sensing messages.
        // midiin->ignoreTypes(false, false, false);

        midi_in.setCallback([](double deltatime,
                               std::vector<uchar> *message,
                               void *userData) {
            static_cast<Launchpad *>(userData)->process_event(deltatime,
                                                              message);
        }, this);

        // TODO: LOG("launchpad is on {}", port);
        midi_in.openPort(port);
        midi_out.openPort(port);

        reset();
        set_grid_layout();
        // initially we update 0 and display 2
        set_double_buffer(cur_page, !cur_page);
    }

    ~Launchpad() { reset(); }

    void set_callback(KeyCb c) {
        lock l(mtx);
        callback = c;
    }

    // flips the currently active/displayed page
    void flip(bool copy = false) {
        cur_page = !cur_page;
        set_double_buffer(cur_page, !cur_page, copy);
    }

    // Sets the pad to be in grid layout
    void set_grid_layout() {
        send_msg({0xB0, 0, 1}); // 2 is drum rack layout, different numbering
    }

    void fill_matrix(uchar col) {
        fill_matrix(
             [col](unsigned x, unsigned y) { return col; });
    }

    /* Fills the whole matrix part of the device with colors given by callback
     *
     */
    void fill_matrix(ColorCb cb) {
        /*
        @note first 64 items is the button matrix (left-right, top-bottom)
        We don't use those, but next come:
        then 8 for the side row
        then 8 for the top row
        */
        for (unsigned y = 0; y < MATRIX_H; ++y) {
            for (unsigned x = 0; x < MATRIX_W; x += 2) {
                send_msg({0x92, cb(x,y), cb(x+1,y)});
            }
        }

        // as an appendix, to avoid the next fill matrix to continue
        // we send a bogus command that should do nothing
        send_msg({0xB0, 0x01, 0x0});
    }

    static constexpr uchar color(uchar r, uchar g) {
        return std::min(g, uchar(3)) << 4 | std::min(r, uchar(3));
    }

    /** Sets color of the button btn (as specified in KeyEvent code) */
    void set_color(unsigned btn, uchar r, uchar g) {
        // there are special bits 3, 2 - Clear and Copy. Used for double buffering
        uchar col = color(r, g);

        if (btn >= 200) { // automap
            if (btn > 207)
                return; // err!

            send_msg({0xB0, (uchar)(btn - 96), col});
        } else {
            send_msg({0x90, (uchar)btn, col});
        }
    }


    /** Sets color of the button btn (as specified in KeyEvent code) */
    void set_color(unsigned btn, uchar col) {
        if (btn >= 200) { // automap
            if (btn > 207)
                return; // err!

            send_msg({0xB0, (uchar)(btn - 96), col});
        } else {
            send_msg({0x90, (uchar)btn, col});
        }
    }

    static unsigned coord_to_btn(unsigned x, unsigned y) {
        return x | y << 4;
    }

    static bool matchName(const std::string &name) {
        return (name.rfind("Launchpad:", 0) == 0);
    }

protected:
    void send_msg(const std::vector<uchar> &buf) {
        midi_out.sendMessage(&buf);
    }

    // resets lighting on the whole pad. Called by default in ctor to
    // initialize the device to a known state
    void reset() {
        send_msg({0xB0, 0, 0});
    }

    /** Controls double buffering.
     * @param update sets the currently updated page (0/1)
     * @param display sets the currently displayed page (0/1)
     * @param copy causes the device to overwrite the currently updated
     *    page with data from previously displayed
     * @param flash when set to true the device rapidly swaps displayed page
     */
    void set_double_buffer(bool update, bool display, bool copy = false,
                           bool flash = false)
    {
        send_msg({0xB0, 0x00,
                  static_cast<uchar>(
                          0x20
                          | (update ? 4 : 0)
                          | (display ? 1 : 0)
                          | (copy ? 16 : 0)
                          | (flash ? 8 : 0))});
    }

    int findLaunchpad() {
        int port = -1;

        for (unsigned int i = 0; i < midi_in.getPortCount(); i++) {
            try {
                if (matchName(midi_in.getPortName(i))) {
                    return i;
                }
            } catch (RtMidiError &error) {
                error.printMessage();
                return 1;
            }
        }

        // TODO: LOG("Cannot find launchpad");
        return -1;
    }

    void process_event(double deltatime,
                       std::vector<uchar> *message)
    {
        if (message->size() != 3) {
            // TODO: log weird packet encounter
            return;
        }


        // got a keypress event. convert to key event and send out
        unsigned button = 0;
        unsigned cx = 0, cy = 0;
        ButtonType type = BTN_GRID;
        bool press = message->at(2) > 0;

        KeyCb cback;
        {
            lock l(mtx);
            cback = callback;
        }

        // no cback, no work
        if (!cback) return;

        if (message->at(0) == 0x90) {
            button = message->at(1);
            // classify - every button with lower nibble == 8 is side button
            if (button & 0x0F == 0x08)
                type = BTN_SIDE;
            cx = button & 0x0F;
            cy = button >> 4;

            cback(*this, {type, button, cx, cy, press});
        }

        if (message->at(0) == 0xB0) {
            // Top row buttons are shifted to 200 range
            button = message->at(1) + 100 - 4;
            type = BTN_TOP;
            cx = button - 200;
            cy = 0;

            cback(*this, {type, button, cx, cy, press});
        }

    }

    std::mutex mtx;

    KeyCb callback;
    RtMidiIn midi_in;
    RtMidiOut midi_out;
    bool cur_page;
};
