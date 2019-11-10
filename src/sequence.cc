#include "sequence.h"

void Sequence::unmark_all() {
    lock l(mutex);

    _unmark_all();
}

void Sequence::unselect_all() {
    lock l(mutex);
    for (auto &ev : events) ev.unselect();
}

void Sequence::add_note(ticks start, ticks length, uchar note, uchar velocity) {
    // scope-lock the sequence
    lock l(mtx);

    _add_note(start, length, note, velocity);

    _tidy();
}

void Sequence::mark_range(ticks start, ticks end, uchar note_low,
                          uchar note_hi)
{
    // scope-lock the sequence
    lock l(mtx);

    // iterate the sequence and find note-ons in time-range
    for (auto &e : events) {
        if (e.is_note_on() &&
            e.get_ticks() >= start &&
            e.get_ticks() < end &&
            e.get_note() >= note_low &&
            e.get_note() < note_hi)
        {
            e.mark();

            // also mark linked to remove note-off
            if (e.is_linked())
                e.get_link()->mark();
        }
    }
}

void Sequence::select_range(ticks start, ticks end, uchar note_low,
                            uchar note_hi, bool toggle)
{
    // scope-lock the sequence
    lock l(mtx);

    // iterate the sequence and find note-ons in time-range
    for (auto &e : events) {
        if (e.is_note_on() &&
            e.get_ticks() >= start &&
            e.get_ticks() < end &&
            e.get_note() >= note_low &&
            e.get_note() < note_hi)
        {
            e.select_or_toggle(toggle);

            // also mark linked to remove note-off
            if (e.is_linked())
                e.get_link()->select_or_toggle(toggle);
        }
    }
}

void Sequence::remove_marked() {
    lock l(mtx);
    _remove_marked();
}

void Sequence::set_note_lengths(ticks len) {
    lock l(mtx);

    // TODO: Replace note ends only!

    for (auto &ev: events) {
        // construct a new event in place of the old one, with new length
        if (ev.is_marked() && ev.is_note_on()) {
            _add_note(ev.get_ticks(), len, ev.get_note(), ev.get_velocity());
        }
    }

    _remove_marked();
    _tidy();
}

void Sequence::set_note_velocities(uchar velo) {
    lock l(mtx);

    for (auto &ev: events) {
        // no need to juggle around with anything here
        if (ev.is_marked() && ev.is_note_on()) {
            ev.set_velocity(velo);
            ev.unmark();
        }
    }
}

void Sequence::set_length(ticks l) {
    ticks old_len = length;
    length = l;

    // if the new sequence is longer, no fixup is necessary
    if (old_len <= length) return;

    // shorten all to be contained within l ticks
    for (auto &ev: events) {
        // construct a new event in place of the old one, with new length
        ticks start = ev.get_ticks();
        if (start >= length) {
            ev.mark();
        };

        if (!ev.is_note_on()) continue;

        if (start + ev.get_length() >= length) {
            ev.set_length(length - start);
        }
    }

    _remove_marked();
    _tidy();
}

uchar Sequence::get_average_velocity() {
    lock l(mtx);

    unsigned total = 0, count = 0;

    for (auto &ev: events) {
        // no need to juggle around with anything here
        if (ev.is_marked() && ev.is_note_on()) {
            total += ev.get_velocity();
            count++;
            ev.unmark();
        }
    }

    if (count)
        return total/count;

    return 0;
}

bool Sequence::is_empty() const {
    lock l(mtx);
    return events.begin() == events.end();
}

void Sequence::move_selected_notes(
            std::function<std::pair<ticks, uchar>(ticks, uchar)> mover)
{
    lock l(mtx);

    for (auto &ev: events) {
        if (ev.is_selected()) {
            ev.mark(); // mark for processing and removal...
        }
    }

    // mark selected first
    for (auto &ev: events) {
        // construct a new event in place of the old one, with new length
        if (ev.is_marked() && ev.is_note_on()) {
            auto np = mover(ev.get_ticks(), ev.get_note());
            _add_note(np.first, ev.get_length(), np.second, ev.get_velocity(), true);
        }
    }

    _remove_marked();
    _tidy();
}


// largerly inspired by seq24's verify_and_link
void Sequence::_tidy() {
    events.sort();

    for (auto &ev : events) {
        ev.clear_link();
        ev.unmark();
    }

    // go through notes and pair note-on and note-off events
    Events::iterator it = events.begin(), iend = events.end();

    for (; it != iend; ++it) {
        // for note-on events we find and mark corresponding note-off events
        if (it->is_note_on()) {
            Events::iterator off = it;
            ++off;

            for (; off != iend; ++off) {
                if (off->is_note_off() &&
                    off->get_note() == it->get_note() &&
                    !off->is_marked())
                {
                    it->link(&(*off));
                    off->link(&(*it));
                    it->mark();
                    off->mark();
                    break;
                }
            }
        }
    }

    _unmark_all();
}

void Sequence::_remove_marked() {
    // TODO: Stop playback of any note that's removed if it's currently playing

    Events::iterator it = events.begin(), iend = events.end();

    while (it != iend) {
        if (it->is_marked()) {
            // TODO: note-off events removed have to be muted while playing!
            Events::iterator ier = it;
            ++it;
            events.erase(ier);
        } else {
            ++it;
        }
    }
}

void Sequence::_add_note(ticks start, ticks length, uchar note,
                         uchar velocity, bool selected)
{
    Event ev;

    ev.set_status(EV_NOTE_ON)
      .set_note(note)
      .set_velocity(velocity)
      .set_ticks(start)
      .set_selected(selected);

    _add_event(ev);

    // add corresponding note-off as well.
    ev.set_status(EV_NOTE_OFF)
      .set_note(note)
      .set_velocity(velocity)
      .set_ticks(start + length);

    _add_event(ev);
}

void Sequence::_unmark_all() {
    for (auto &ev : events) ev.unmark();
}

void Sequence::_add_event(const Event &ev) {
    events.push_front(ev);
}
