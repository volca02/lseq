#include <iostream>
#include <ctime>

#include "ui.h"
#include "sequence.h"
#include "lseq.h"
#include "project.h"
#include "track.h"

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
/* ---- Track/Project setup View -------------------------------------------- */
/* -------------------------------------------------------------------------- */
void TrackScreen::on_key(const Launchpad::KeyEvent &ev) {
    lock l(mtx);

    if (ev.code == Launchpad::BC_MIXER) {
        shift = ev.press;
        return;
    }

    if (ev.type == Launchpad::BTN_GRID) {
        // on and off button presses are distinct to allow for long press and button combos
        if (ev.press) {
            if (shift) {
                updates.shift_grid_on.mark(ev.x, ev.y);
            } else {
                updates.grid_on.mark(ev.x, ev.y);
            }
        } else {
            updates.grid_off.mark(ev.x, ev.y);
        }

        updates.mark_dirty();
        return;
    }

    if (ev.press) {
        // only button press events here, no release events
        switch (ev.code) {
        case Launchpad::BC_LEFT : updates.left_right--; updates.mark_dirty(); return;
        case Launchpad::BC_RIGHT: updates.left_right++; updates.mark_dirty(); return;
            // TODO: Use note scaler here
        case Launchpad::BC_DOWN : updates.up_down--; updates.mark_dirty(); return;
        case Launchpad::BC_UP   : updates.up_down++; updates.mark_dirty(); return;
        }

        if (ev.type == Launchpad::BTN_SIDE) {
            // side button pressed
            updates.side_buttons |= 1 << ev.y;
            updates.mark_dirty();
        }
    }

}

void TrackScreen::on_enter() {
    // color up our mode button
    set_active_mode_button(0);

    repaint();

    // present
    launchpad.flip();
};

void TrackScreen::update() {
    if (!updates.dirty) return;

    UpdateBlock ub;
    {
        lock l(mtx);
        ub = updates;
        updates.clear();
    }

    // From here forward, we only use "ub", not "updates"
    bool dirty = false;

    Launchpad::Bitmap prev = held_buttons;

    // repaint whole screen if held buttons changed
    if (held_buttons != prev) dirty = true;

    if (ub.side_buttons) {
        // mute tracks that were pressed
        for (unsigned y = 0; y < Launchpad::MATRIX_H; ++y) {
            Track *tr = get_track_for_y(y);
            if (!tr) continue;
            if ((ub.side_buttons >> y) & 1) {
                tr->toggle_mute();
                dirty = true;
            }
        }
    }

    bool edit_press = false; uchar gx = 0, gy = 0;
    ub.grid_off.iterate([&](unsigned x, unsigned y) {
                            edit_press = shift_held_buttons.get(x, y);
                            gx = x;
                            gy = y;
                        });

    if (edit_press) {
            Sequence *seq = get_seq_for_xy(gx, gy);
            if (seq) {
                ui.sequence_screen.set_active_sequence(seq);
                ui.set_screen(SCR_SEQUENCE);
                return;
            }
    }

    // any presses to play the sequence?
    bool play = false;
    ub.grid_on.iterate([&](unsigned x, unsigned y) {
                           play = true;
                           gx = x;
                           gy = y;
                       });
    if (play) {
        schedule_sequence_for_xy(gx, gy);
    }

    held_buttons |= ub.grid_on;
    held_buttons &= ~ub.grid_off;
    shift_held_buttons |= ub.shift_grid_on;
    shift_held_buttons &= ~ub.grid_off;

    // ...
    if (dirty) repaint();
};

void TrackScreen::repaint() {
    uchar view[Launchpad::MATRIX_W][Launchpad::MATRIX_H];

    for (uchar y = 0; y < Launchpad::MATRIX_H; ++y) {
        for (uchar x = 0; x < Launchpad::MATRIX_W; ++x) {
            Sequence *s = get_seq_for_xy(x, y);

            if (!s) {
                view[x][y] = Launchpad::CL_BLACK;
                continue;
            }

            // TODO: Customizable color per track...
            uchar col = s->is_empty() ? Launchpad::CL_BLACK
                        : Launchpad::CL_AMBER;

            // all pressed keys will show up, but only first to be released
            if (held_buttons.get(x, y)) col = Launchpad::CL_RED;

            view[x][y] = col;
        }

        // mutes?
        Track *t = get_track_for_y(y);
        uchar col = t->is_muted() ? Launchpad::CL_BLACK : Launchpad::CL_GREEN;
        launchpad.set_color(Launchpad::coord_to_btn(8, y), col);
    }

    launchpad.fill_matrix(
            [&view](unsigned x, unsigned y) {
                return view[x][y];
            });

    // TODO: light up buttons based on positioning - can we go more to the
    // sides, etc?
    launchpad.flip(true);
};


Track *TrackScreen::get_track_for_y(uchar y) {
    unsigned tr = y + vy;
    if (tr >= project.get_track_count()) return nullptr;
    return project.get_track(tr);
}

Sequence *TrackScreen::get_seq_for_xy(uchar x, uchar y) {
    unsigned tr = y + vy;
    if (tr >= project.get_track_count()) return nullptr;

    Track *track = project.get_track(tr);

    if (!track) return nullptr;

    unsigned sq = x + vx;
    if (sq >= track->get_sequence_count()) return nullptr;

    return track->get_sequence(sq);
}

bool TrackScreen::schedule_sequence_for_xy(uchar x, uchar y) {
    unsigned tr = y + vy;
    if (tr >= project.get_track_count()) return false;

    Track *track = project.get_track(tr);

    if (!track) return false;

    unsigned sq = x + vx;
    if (sq >= track->get_sequence_count()) return false;

    ui.owner.get_sequencer().schedule_sequence(tr, sq);

    return true;
}

void TrackScreen::on_exit() {
    held_buttons.clear();
    shift_held_buttons.clear();
    shift = false;
};

/* -------------------------------------------------------------------------- */
/* ---- Song Arrangement View ----------------------------------------------- */
/* -------------------------------------------------------------------------- */
void SongScreen::on_key(const Launchpad::KeyEvent &ev) {
    lock l(mtx);

    // arrow keys move the view if applicable
}

void SongScreen::on_enter() {
    repaint();
};

void SongScreen::repaint() {
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
void SongScreen::on_exit() {
};

/* -------------------------------------------------------------------------- */
/* ---- Sequence View ------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
void SequenceScreen::on_exit() {
    held_buttons.clear();
};

void SequenceScreen::on_key(const Launchpad::KeyEvent &ev) {
    lock l(mtx);

    if (ev.code == Launchpad::BC_MIXER) {
        shift = ev.press;
        if (ev.press) {
            shift_only = true;
            shift_start = std::time(nullptr);
        } else {
            // TODO: do this so we get the effect before releasing (easier usage)
            // just mark up the start of the shift event and decide in update code
            // but that needs a timer to wake the ui screen up after the decided time
            updates.shift_only = shift_only;
            updates.shift_held = std::time(nullptr) - shift_start;
            updates.mark_dirty();
        }
        return;
    }

    shift_only = false;

    // handle grid ops
    if (ev.type == Launchpad::BTN_GRID) {
        // on and off button presses are distinct to allow for long press and button combos
        if (ev.press) {
            if (shift) {
                updates.shift_grid_on.mark(ev.x, ev.y);
            } else {
                updates.grid_on.mark(ev.x, ev.y);
            }
        } else {
            updates.grid_off.mark(ev.x, ev.y);
        }

        updates.mark_dirty();
        return;
    }

    if (!shift) {
        // NOTE: implement long-press events here

        if (ev.press) {
            // only button press events here, no release events
            switch (ev.code) {
            case Launchpad::BC_LEFT : updates.left_right--; updates.mark_dirty(); return;
            case Launchpad::BC_RIGHT: updates.left_right++; updates.mark_dirty(); return;
                // TODO: Use note scaler here
            case Launchpad::BC_DOWN : updates.up_down--; updates.mark_dirty(); return;
            case Launchpad::BC_UP   : updates.up_down++; updates.mark_dirty(); return;
            }
        }

        if (ev.type == Launchpad::BTN_SIDE) {
            // side button pressed
            updates.side_buttons |= 1 << ev.y;
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
        } else if (ev.type == Launchpad::BTN_TOP) {
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
    // skip to track screen if we have no sequence set
    if (!sequence) {
        ui.set_screen(SCR_TRACK);
        return;
    }

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

    // From here forward, we only use "b", not "updates"

    // we now walk through all the update points and update our display model
    bool dirty = false; // this means we need a global repaint...
    bool flip = false;

    // TODO: also schedule a midi event in router so that we hear what we press
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
            // TODO: Handle default velocity?
            add_note(x, y, !dirty);
            queue_note_on(note_scaler.to_note(y), DEFAULT_VELOCITY);
            modified_notes.mark(x, y);
        }
    });

    // TODO: Rework this to be initiated by timing, not by button release
    // if it's held for more than specified time, and released without pressing anything else
    if ((b.shift_held > 1) && b.shift_only) {
        sequence->unselect_all();
        dirty = true;
    }

    // selections to notes done are transfered to selections in sequence
    b.shift_grid_on.iterate([&](unsigned x, unsigned y) {
        // mark notes that are pressed with shift
        marked_notes++;
        ticks t = time_scaler.to_ticks(x);
        ticks s = time_scaler.get_step();
        uchar n = note_scaler.to_note(y);
        sequence->select_range(t, t+s, n, n+1, /*toggle*/ true);
        dirty = true;
    });

    b.grid_off.iterate([&](unsigned x, unsigned y) {
        if (modified_notes.get(x,y))
            queue_note_off(note_scaler.to_note(y));

        // only remove the note if it was a held note (not shift-marked)
        if ((view[x][y] & FS_HAS_NOTE) && held_buttons.get(x, y)
            && !modified_notes.get(x, y))
        {
            if (view[x][y] & FS_IS_SELECTED)
                --marked_notes;
            remove_note(x, y, !dirty);
        }

        modified_notes.unmark(x, y);
        flip = !dirty;
    });

    // update our held buttons with grid_on, grid_off bits
    held_buttons |= b.grid_on;
    held_buttons &= ~b.grid_off;

    // any side buttons pressed?
    if (held_buttons.has_value()) {
        if (b.side_buttons) {
            // highest bit set indicates velocity
            int vel_bit = highest_bit_set(b.side_buttons);

            if (vel_bit >= 0) {
                static const uchar velo_table[8] = {127,
                                                    7 * 16,
                                                    6 * 16,
                                                    5 * 16,
                                                    4 * 16,
                                                    3 * 16,
                                                    2 * 16,
                                                    1 * 16};

                // if so, we're setting velocity for the held notes
                set_note_velocities(velo_table[vel_bit]);

                // mark all currently held buttons as modified, so we won't
                // remove the notes
                modified_notes |= held_buttons;
                flip = !dirty;
            }
        } else {
            uchar velo = get_average_held_velocity();
            paint_sidebar_value(velo, Launchpad::CL_AMBER);
        }

    } else {
        // no held buttons. repaint status bar
        paint_status_sidebar();
    }

    if (marked_notes) {
        // handle left_right/up_down and time_scale differently - modifying all held notes
        if (b.left_right) {
            move_selected_notes(b.left_right, 0);
            dirty = true;
        }

        if (b.up_down) {
            move_selected_notes(0, b.up_down);
            dirty = true;
        }
    } else {
        if (b.left_right) {
            time_scaler.scroll(b.left_right);
            sequence->unmark_all();
            dirty = true;
        }

        if (b.time_scale) {
            time_scaler.scale(b.time_scale);
            sequence->unmark_all();
            dirty = true;
        }

        if (b.switch_triplets) {
            time_scaler.switch_triplets();
            sequence->unmark_all();
            dirty = true;
        }

        if (b.up_down) {
            note_scaler.scroll(b.up_down);
            sequence->unmark_all();
            dirty = true;
        }
    }

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

    // these will light up the arrows if there are more events in that direction
    bool x_pre    = false;
    bool x_post   = false;
    bool y_above  = false;
    bool y_below  = false;
    marked_notes  = 0;

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

        if (x + l <= 0) {
            x_pre = true;
            continue;
        }

        if (x >= Launchpad::MATRIX_W) {
            x_post = true;
            continue;
        }

        if (y < 0) {
            y_below = true;
            continue;
        }

        if (y >= Launchpad::MATRIX_H) {
            y_above = true;
            continue;
        }

        bool is_selected = false;

        if (x >= 0) {
            uchar c = view[x][y];

            if (c & FS_HAS_NOTE) c |= FS_MULTIPLE;

            c |= FS_HAS_NOTE;

            // if the note timing is not accurate, mark it down
            if (!accurate) c |= FS_INACCURATE;

            if (ev.is_selected()) {
                c |= FS_IS_SELECTED;
                is_selected = true;
                ++marked_notes;
            }

            view[x][y] = c;
        }

        // mark continution, including our base note (handy for updates)
        for (long c = 0; c < l; ++c) {
            long xc = x + c;
            if (xc < 0) continue;
            if (xc >= Launchpad::MATRIX_W) break;
            view[xc][y] |= FS_CONT;
            if (is_selected)
                view[xc][y] |= FS_IS_SELECTED;
        }
    }

    // TODO: Decide if we display indicators or note velocity based on held buttons
    // are we holding any notes? if so we display average velocity
    if (held_buttons.has_value()) {
        uchar velo = get_average_held_velocity();
        paint_sidebar_value(velo, Launchpad::CL_AMBER);
    } else {
        paint_status_sidebar();
    }

    // render the UI part
    launchpad.fill_matrix(
            [this](unsigned x, unsigned y) {
                return to_color(view, x, y);
            });

    // colorize the arrows based on the out-of-sight flags
    uchar out_col = Launchpad::CL_GREEN;
    launchpad.set_color(Launchpad::BC_UP,  y_below  ? out_col : 0);
    launchpad.set_color(Launchpad::BC_DOWN, y_above ? out_col : 0);
    launchpad.set_color(Launchpad::BC_LEFT,  x_pre  ? out_col : 0);
    launchpad.set_color(Launchpad::BC_RIGHT, x_post ? out_col : 0);

    launchpad.set_color(Launchpad::BC_MIXER,
                        (marked_notes > 0) ? Launchpad::CL_GREEN : 0);

    launchpad.flip(true);
}

// converts cell status info from given position to color for rendering
uchar SequenceScreen::to_color(View &v, unsigned x, unsigned y) {
    uchar s = v[x][y];

    uchar col = Launchpad::CL_BLACK;

    if (s & FS_SCALE_MARK)
        col = Launchpad::CL_YELLOW_M;

    if (s & FS_CONT)
        col = Launchpad::CL_RED_L;

    if (s & FS_HAS_NOTE)
        col = Launchpad::CL_RED;

    // both of these boil down to incomplete information kind of a deal
    if (s & FS_INACCURATE || s & FS_MULTIPLE)
        col = Launchpad::CL_AMBER;

    // marked notes are highest priority there is
    if (s & FS_IS_SELECTED) {
        col = ((s & FS_CONT) && !(s & FS_HAS_NOTE)) ? Launchpad::CL_GREEN_L
                                                    : Launchpad::CL_GREEN;
    }

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

    sequence->unmark_all();
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

void SequenceScreen::set_note_velocities(uchar velo) {
    sequence->unmark_all();

    // iterate all held notes
    held_buttons.iterate([&](unsigned x, unsigned y) {
        ticks t = time_scaler.to_ticks(x);
        ticks s = time_scaler.get_step();
        uchar n = note_scaler.to_note(y);

        sequence->mark_range(t, t + s, n, n + 1);
    });

    sequence->set_note_velocities(velo);

    // no repaint needed aside from the velocity indicator
    // which we do here locally
    // TODO: Paint the given velocity
    paint_sidebar_value(velo, Launchpad::CL_AMBER);
}

void SequenceScreen::move_selected_notes(int mx, int my) {
    // TODO: Move notes based on KEY, not HALFTONES. Needs note_scaler cooperation
    ticks mt = time_scaler.quantum_to_ticks(mx);
    sequence->move_selected_notes([&](ticks t, uint8_t pitch) {
        return std::pair<ticks, uchar>{(std::max(ticks(0), t + mt)),
                                       note_scaler.move_steps(pitch, my)};
    });
}



void SequenceScreen::paint_sidebar_value(uchar val, uchar color) {
    if (val > 127) val = 127;

    // convert velocity to 0-8 button lights
    val = (val + 1) * 8 / 128;

    // from the bottom up (inverted to be more readable)
    for (uchar y = 0; y < Launchpad::MATRIX_H; ++y) {
        launchpad.set_color(
                launchpad.coord_to_btn(8, Launchpad::MATRIX_H - 1 - y),
                (val > y) ? color : Launchpad::CL_BLACK);
    }
}

void SequenceScreen::paint_status_sidebar() {
    // render other indicators
    // triplets (green means triplets are enabled)
    launchpad.set_color(launchpad.coord_to_btn(8, 0),
                        time_scaler.get_triplets() ? Launchpad::CL_GREEN
                        : Launchpad::CL_BLACK);

    // clear the rest of the BTN_SIDE buttons
    for (uchar y = 1; y < Launchpad::MATRIX_H; ++y) {
        launchpad.set_color(launchpad.coord_to_btn(8, y),
                            Launchpad::CL_BLACK);
    }
}

uchar SequenceScreen::get_average_held_velocity() {
    sequence->unmark_all();

    held_buttons.iterate([&](unsigned x, unsigned y) {
        ticks t = time_scaler.to_ticks(x);
        ticks s = time_scaler.get_step();
        uchar n = note_scaler.to_note(y);
        sequence->mark_range(t, t + s, n, n + 1);
    });

    return sequence->get_average_velocity();
}

void SequenceScreen::queue_note_on(uchar n, uchar vel) {
    // TODO: Propper midi channel - (as specified by the sequence's owner)
    #warning todo: use the project/track to get midi chan
    ui.get_owner().get_router().queue_immediate(
            jack::MidiMessage::compose_note_on(MIDI_CH_DEFAULT, n, vel));
}

void SequenceScreen::queue_note_off(uchar n) {
    // TODO: Propper midi channel
    ui.get_owner().get_router().queue_immediate(
            jack::MidiMessage::compose_note_off(MIDI_CH_DEFAULT, n));
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
    case SCR_TRACK:    next = &track_screen; break;
    case SCR_SONG:     next = &song_screen; break;
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
