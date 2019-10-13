#include "sequence.h"

void Sequence::unmark_all() {
    lock l(mutex);

    _unmark_all();
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

    _unmark_all();

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
/*            ev.set_length(len);
            ev.unmark();*/
            _add_note(ev.get_ticks(), len, ev.get_note(), ev.get_velocity());
        }
    }

    _remove_marked();
    _tidy();
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
                         uchar velocity)
{
    Event ev;

    ev.set_status(Event::EV_NOTE_ON)
      .set_note(note)
      .set_velocity(velocity)
      .set_ticks(start);

    _add_event(ev);

    // add corresponding note-off as well.
    ev.set_status(Event::EV_NOTE_OFF)
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