#include "ui.h"
#include "sequence.h"
#include "lseq.h"

UIScreen::UIScreen(UI &ui) : ui(ui), launchpad(ui.launchpad) {}

void UIScreen::set_active_mode_button(unsigned m) {
    launchpad.set_color(Launchpad::BC_SESSION, 0, m == 0 ? 3 : 0);
    launchpad.set_color(Launchpad::BC_USER1, 0, m == 1 ? 3 : 0);
    launchpad.set_color(Launchpad::BC_USER2, 0, m == 2 ? 3 : 0);
    launchpad.set_color(Launchpad::BC_MIXER, 0, m == 3 ? 3 : 0);
}

void UIScreen::wake_up() {
    ui.wake_up();
}

/* -------------------------------------------------------------------------- */
/* ---- Project View -------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
void ProjectScreen::on_key(const Launchpad::KeyEvent &ev) {

}

void ProjectScreen::on_enter() {
    // color up our mode button
    set_active_mode_button(0);

    // render the UI part
    launchpad.fill_matrix(
            [this](unsigned x, unsigned y) {
                if (y == 0)
                    return Launchpad::color(3, 3);

                if (y == 7)
                    return Launchpad::color(0, 3);

                return Launchpad::color(0, 0);
            });

    // present
    launchpad.flip();
};

void ProjectScreen::on_exit() {
};

/* -------------------------------------------------------------------------- */
/* ---- Track View ---------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
void TrackScreen::on_key(const Launchpad::KeyEvent &ev) {

}

void TrackScreen::on_enter() {
    repaint();
};

void TrackScreen::repaint() {
    // color up our mode button
    set_active_mode_button(1);

    // render the UI part
    launchpad.fill_matrix(
            [this](unsigned x, unsigned y) {
                return Launchpad::color((x + y) % 4, 0);
            });

    // present
    launchpad.flip();
}

// Note - no need to flip here maybe.
void TrackScreen::on_exit() {
};

/* -------------------------------------------------------------------------- */
/* ---- Sequence View ------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
void SequenceScreen::on_exit() {
};

void SequenceScreen::on_key(const Launchpad::KeyEvent &ev) {
    lock l(mtx);

    if (ev.code == Launchpad::BC_MIXER) {
        shift = ev.press;
        return;
    }

    if (!shift) {
        // NOTE: implement long-press events here

        if (ev.press) {
            // only button press events here, no release events
            switch (ev.code) {
            case Launchpad::BC_LEFT : updates.time_shift--; updates.mark_dirty(); return;
            case Launchpad::BC_RIGHT: updates.time_shift++; updates.mark_dirty(); return;
                // TODO: Use note scaler here
            case Launchpad::BC_DOWN : updates.note_shift--; updates.mark_dirty(); return;
            case Launchpad::BC_UP   : updates.note_shift++; updates.mark_dirty(); return;
            }
        }

        if (ev.type == Launchpad::BTN_GRID) {
            // on and off button presses are distinct to allow for long press and button combos
            if (ev.press)
                updates.grid_on.mark(ev.x, ev.y);
            else
                updates.grid_off.mark(ev.x, ev.y);

            // TODO: also schedule a midi event in router so that we hear what we press

            updates.mark_dirty();
        }
    } else {
        if (!ev.press) return;

        // special mode buttons while shift is pressed
        if (ev.type == Launchpad::BTN_SIDE) {
            switch (ev.y) {
            case 0:  // switch triplets on/off
                updates.switch_triplets = true;
                updates.mark_dirty();
                break;
            }
        } /*else if (ev.type == Launchpad::BTN_TOP)*/ {
            switch (ev.code) {
            case Launchpad::BC_LEFT:  // zoom out
                updates.time_scale--;
                updates.mark_dirty();
                break;
            case Launchpad::BC_RIGHT: // zoom in
                updates.time_scale++;
                updates.mark_dirty();
                break;
            }
        }
    }
}

void SequenceScreen::on_enter() {
    repaint();
};

void SequenceScreen::update() {
    if (!updates.dirty) return;

    UpdateBlock b;
    {
        lock l(mtx);
        b = updates;
        updates.clear();
    }

    // we now walk through all the update points and update our display model
    bool dirty = false; // this means we need a global repaint...

    if (b.time_shift) {
        time_scaler.scroll(b.time_shift);
        dirty = true;
    }

    if (b.time_scale) {
        time_scaler.scale(b.time_scale);
        dirty = true;
    }

    if (b.switch_triplets) {
        time_scaler.switch_triplets();
        dirty = true;
    }

    if (b.note_shift) {
        note_scaler.scroll(b.note_shift);
        dirty = true;
    }

    bool flip = false;

    // update from note press bitmap
    b.grid_on.iterate([&](unsigned x, unsigned y) {
        flip = !dirty;

        // see the status of the current field, if there is a note don't add
        // another one
        // if buttons are held, see if any of them is in row
        // if so, we just change length of the note and repaint
        uchar row = held_buttons.row(y);

        if (row) {
            // there are buttons being held in the row where button event occured
            // calculate the new length
            // find the nearest lower order button that is pressed
            unsigned near_x = nearest_lower_bit(row, x);

            if (near_x < x) {
                // enables us to toggle the last bit of length
                unsigned toggle = view[x][y] & FS_CONT ? 0 : 1;
                unsigned len = x - near_x + toggle;

                // lengthen the notes
                set_note_lengths(near_x, y, len, !dirty);
                modified_notes.mark(near_x, y);
                // already done, do not add a note now!
                return;
            }
        }

        if ((view[x][y] & FS_HAS_NOTE) == 0) {
            add_note(x, y, !dirty);
            modified_notes.mark(x, y);
        }
    });

    b.grid_off.iterate([&](unsigned x, unsigned y) {
        if ((view[x][y] & FS_HAS_NOTE) && !modified_notes.get(x, y)) {
            remove_note(x, y, !dirty);
        }

        modified_notes.unmark(x, y);
        flip = !dirty;
    });

    // update our held buttons with grid_on, grid_off bits
    held_buttons |= b.grid_on;
    held_buttons &= ~b.grid_off;

    if (dirty) repaint();

    // present
    if (flip) launchpad.flip(true); // we use copy if we do partial updates too
}

void SequenceScreen::paint() {
}

void SequenceScreen::repaint() {
    // color up our mode button
    set_active_mode_button(2);

    if (!sequence) {
        launchpad.flip();
        return;
    }

    // TODO: if we're live, also draw a time bar

    // prepare a buffer for the sequence display
    clear_view();

    auto seq_handle = sequence->get_handle();

    // we DO have a sequence to work on
    // prepare the view beforehand
    for (const auto &ev : seq_handle) {
        // skip non-note-on events
        if (!ev.is_note_on()) continue;

        long x = time_scaler.to_quantum(ev.get_ticks());
        bool accurate = time_scaler.is_scale_accurate(ev.get_ticks());
        long y = note_scaler.to_grid(ev.get_note());
        bool in_scale = note_scaler.is_in_scale(ev.get_note());

        // also quantize the length
        long l = time_scaler.length_to_quantum(ev.get_length());

        if (y < 0) continue;
        if (y >= Launchpad::MATRIX_H) continue;

        if (x + l < 0) continue;
        if (x >= Launchpad::MATRIX_W) continue;

        if (x >= 0) {
            uchar c = view[x][y];

            if (c & FS_HAS_NOTE) c |= FS_MULTIPLE;

            c |= FS_HAS_NOTE;

            // if the note timing is not accurate, mark it down
            if (!accurate) c |= FS_INACCURATE;

            view[x][y] = c;
        }

        // mark continution, including our base note (handy for updates)
        for (long c = 0; c < l; ++c) {
            long xc = x + c;
            if (xc < 0) continue;
            if (xc >= Launchpad::MATRIX_W) break;
            view[xc][y] |= FS_CONT;
        }
    }

    // render the UI part
    launchpad.fill_matrix(
            [this](unsigned x, unsigned y) {
                return to_color(view, x, y);
            });

    launchpad.flip(true);
}

// converts cell status info from given position to color for rendering
uchar SequenceScreen::to_color(View &v, unsigned x, unsigned y) {
    uchar s = v[x][y];

    uchar col = Launchpad::CL_BLACK;

    if (s & FS_SCALE_MARK)
        col = Launchpad::CL_AMBER_L;

    if (s & FS_CONT)
        col = Launchpad::CL_RED_L;

    if (s & FS_HAS_NOTE)
        col = Launchpad::CL_RED;

    // both of these boil down to incomplete information kind of a deal
    if (s & FS_INACCURATE || s & FS_MULTIPLE)
        col = Launchpad::CL_AMBER;

    return col;
}

void SequenceScreen::clear_view() {
    // TODO: Implement halftone marks as well?
    for (uchar x = 0; x < Launchpad::MATRIX_W; ++x)
        for (uchar y = 0; y < Launchpad::MATRIX_H; ++y)
            view[x][y] = bg_flags(x, y);
}

uchar SequenceScreen::bg_flags(unsigned x, unsigned y) {
    return note_scaler.is_scale_mark(y) ? FS_SCALE_MARK : 0;
}

void SequenceScreen::set_active_sequence(Sequence *seq) {
    sequence = seq;
}

void SequenceScreen::add_note(unsigned x, unsigned y, bool repaint) {
    ticks t = time_scaler.to_ticks(x);
    ticks s = time_scaler.get_step();
    uchar n = note_scaler.to_note(y);

    sequence->add_note(t, time_scaler.get_step(), n);
    view[x][y] |= FS_HAS_NOTE;

    if (repaint) {
        unsigned btn = Launchpad::coord_to_btn(x, y);
        launchpad.set_color(btn, to_color(view, x, y));
    }
}

void SequenceScreen::remove_note(unsigned x, unsigned y, bool repaint) {
    ticks t = time_scaler.to_ticks(x);
    ticks s = time_scaler.get_step();
    uchar n = note_scaler.to_note(y);

    sequence->mark_range(t, t+s, n, n+1);
    sequence->remove_marked();
    uchar c = view[x][y];

    view[x][y] = bg_flags(x, y); // clear note position

    // set to other value if we have continuations
    uchar last_x = x;

    // clear continuations
    if (c & FS_CONT) {
        for (uchar xc = x + 1; xc < Launchpad::MATRIX_W; ++xc)
        {
            // stop on no continuations or on a new note
            if (view[xc][y] & FS_CONT == 0) break;
            if (view[xc][y] & FS_HAS_NOTE) break;
            view[xc][y] = bg_flags(x, y);
            last_x = xc;
        }
    }

    for (uchar xc = x; xc <= last_x; ++xc) {
        unsigned btn = Launchpad::coord_to_btn(xc, y);
        launchpad.set_color(btn, to_color(view, xc, y));
    }
}

void SequenceScreen::set_note_lengths(unsigned x, unsigned y, unsigned len, bool repaint) {
    ticks t = time_scaler.to_ticks(x);
    ticks s = time_scaler.get_step();
    uchar n = note_scaler.to_note(y);

    sequence->mark_range(t, t+s, n, n+1);
    sequence->set_note_lengths(s * len);

    uchar last_x = x;

    for (uchar xc = x; xc < Launchpad::MATRIX_W; ++xc)
    {
        int cl = xc - x;
        // stop on no continuations or on a new note
        if ((view[xc][y] & FS_CONT == 0) && (cl >= len)) break;
        if (view[xc][y] & FS_HAS_NOTE && xc != x) break; // next note already, break it up
        // not over new length?
        if (cl < len) {
            if (len > 1)
                view[xc][y] |= FS_CONT;
        } else {
            view[xc][y] = bg_flags(x, y);
        }
        last_x = xc;
    }

    for (uchar xc = x; xc <= last_x; ++xc) {
        unsigned btn = Launchpad::coord_to_btn(xc, y);
        launchpad.set_color(btn, to_color(view, xc, y));
    }
}

/* -------------------------------------------------------------------------- */
/* ---- UI ------------------------------------------------------------------ */
/* -------------------------------------------------------------------------- */
void UI::wake_up() {
    owner.wake_up();
}


void UI::set_screen(ScreenType t) {
    std::scoped_lock<std::mutex> l(mut);
    UIScreen *next = nullptr;

    switch (t) {
    case SCR_SESSION: next = &project_screen; break;
    case SCR_TRACK: next = &track_screen; break;
    case SCR_SEQUENCE: next = &sequence_screen; break;
    }

    if (next != current_screen) {
        if (current_screen) current_screen->on_exit();
        if (next) next->on_enter();
        current_screen = next;
    }
}

UIScreen *UI::get_current_screen() const {
    std::scoped_lock<std::mutex> l(mut);
    return current_screen;
}
