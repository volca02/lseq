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

        // no long-press events beyond this point
        if (!ev.press) return;

        switch (ev.code) {
        case Launchpad::BC_LEFT : updates.time_shift--; updates.mark_dirty(); return;
        case Launchpad::BC_RIGHT: updates.time_shift++; updates.mark_dirty(); return;
        // TODO: Use note scaler here
        case Launchpad::BC_UP   : updates.note_shift--; updates.mark_dirty(); return;
        case Launchpad::BC_DOWN : updates.note_shift++; updates.mark_dirty(); return;
        }

        if (sequence && ev.type == Launchpad::BTN_GRID) {
            updates.grid.mark(ev.x, ev.y);
            updates.mark_dirty();
        }
    } else {
        if (!ev.press) return;

        switch (ev.code) {
        case Launchpad::BC_LEFT: // shorten the sequence step by 1/2
            updates.time_scale++;
            updates.mark_dirty();
            break;
        case Launchpad::BC_RIGHT:
            updates.time_scale--;
            updates.mark_dirty();
            break;
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

    if (b.note_shift) {
        note_scaler.scroll(b.note_shift);
        dirty = true;
    }

    bool flip = dirty;

    // update from note press bitmap
    b.grid.iterate([&](unsigned x, unsigned y) {
                       ticks t = time_scaler.to_ticks(x);
                       ticks s = time_scaler.get_step();
                       uchar n = note_scaler.to_note(y);

                       // see the status of the current field
                       if (view[x][y] & FS_HAS_NOTE) {
                           sequence->mark_range(t, t+s, n, n+1);
                           sequence->remove_marked();
                           view[x][y] = bg_flags(x, y); // clear note position
                           // TODO: also need to clear continuations - so need to iterate erased notes and see their lengths!
                       } else {
                           sequence->add_note(t, time_scaler.get_step(), n);
                           view[x][y] |= FS_HAS_NOTE;
                       }

                       // not dirty, also re-paint
                       if (!dirty) {
                           unsigned coord = Launchpad::coord_to_btn(x, y);
                           launchpad.set_color(coord, to_color(view, x, y));
                           flip = true;
                       }
                  });

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

        // does the note come into the view at all?
        if (x + l < 0) continue;
        if (x >= Launchpad::MATRIX_W) continue;

        uchar c = view[x][y];

        if (c & FS_HAS_NOTE)
            c |= FS_MULTIPLE;
        else
            c |= FS_HAS_NOTE;

        // if the note timing is not accurate, mark it down
        if (!accurate)
            c |= FS_INACCURATE;

        // mark continution
        for (long c = 1; c < l; ++c) {
            if (x + c < 0) continue;
            if (x + c > Launchpad::MATRIX_W) break;
            view[x + c][y] |= FS_CONT;
        }

        view[x][y] = c;
    }

    // render the UI part
    launchpad.fill_matrix(
            [this](unsigned x, unsigned y) {
                return to_color(view, x, y);
            });
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

/* -------------------------------------------------------------------------- */
/* ---- UI ------------------------------------------------------------------ */
/* -------------------------------------------------------------------------- */
void UI::wake_up() {
    owner.wake_up();
}
