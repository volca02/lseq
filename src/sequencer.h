#pragma once

#include "common.h"
#include "util.h"
#include "jackmidi.h"
#include "project.h"
#include "router.h"

/// helper class that wraps all needed data to walk a sequence and schedule notes
struct SequenceWalker {
    SequenceWalker(unsigned track, Sequence &seq, ticks start)
        : track(track)
        , start(start)
        , handle(seq)
        , iter(handle.begin())
    {}

    // absolute time ticks (offset by the when_started field of the track)
    ticks get_ticks() {
        if (iter != handle.end())
            return iter->get_ticks() + start;

        return 0;
    }

    // moves to a start tick - first tick after the specified window start
    void advance_to(ticks window) {
        while (iter != handle.end() && get_ticks() < window) {
            ++iter;
        }
    }

    bool at_end() {
        return iter == handle.end();
    }

    unsigned track;
    ticks start; // offset to start of the sequence (timing)
    Sequence::handle handle;
    Sequence::handle::const_iterator iter;
};

/** this acts like streamer for the project whilst it plays
 *  and it feeds router with events to be played.
 */
class Sequencer : public jack::Client::Callback {
public:
    Sequencer(Project &proj, Router &r, jack::Client &client)
            : project(proj), router(r), client(client)
    {
    }

    bool schedule_sequence(unsigned track, unsigned sequence) {
        return schedule_sequence(track, sequence, get_follow_up_ticks(track));
    }

    bool schedule_sequence(unsigned track, unsigned sequence, ticks when) {
        // queue a track to change sequence on
        Track *t = project.get_track(track);

        if (t) {
            Sequence *seq = t->get_sequence(sequence);

            if (seq) {
                tracks[track].next = seq;
                tracks[track].when_change = when;
                return true;
            }
        }

        return false;
    }

    int process(jack_nframes_t nframes) override {
        auto last_frames = client.last_frame_time();

        auto strt_us = client.frames_to_time(last_frames);
        auto end_us = client.frames_to_time(last_frames + nframes);

        // calculate current tick window
        ticks last_ticks = current_ticks;
        ticks w_start = us_to_ticks(strt_us, project.get_bpm());
        current_ticks = w_start;
        ticks w_stop = us_to_ticks(end_us, project.get_bpm());

        // TODO: Handle XRun

        if (current_ticks != last_ticks) {
            swap_sequences();

            // TODO: May revise that and remember note-off time for all notes, and
            // schedule those out of scope of the current track selection.
            // This would enable us to have out-of sequence note ends that would be
            // still valid and properly scheduled.

            // last step - schedule notes on the current set of active track's sequences
            schedule_notes(w_start, w_stop);
        }

        return 0;
    }

    // stops all playback immediately and unconditionally (well... it will be
    // done in the process callback asap)
    void stop() {
        for (unsigned t = 0; t < Project::MAX_TRACK; ++t) {
            tracks[t].next = nullptr;
            tracks[t].when_change = 0; // 0 will mean immediate change
        }
    }

    /** returns a tick onto which to schedule a sequence
     * to follow up on the last one, or start at the next bar
     * if none is playing right now
     */
    ticks get_follow_up_ticks(unsigned track) {
        Sequence *cur = tracks[track].current;
        if (cur == nullptr) {
            return next_opportunity();
        } else {
            // get the time till end
            return cur->get_length() + tracks[track].when_started;
        }
    }

protected:
    ticks next_opportunity() {
        return next_multiple(current_ticks, PPQN);
    }

    void swap_sequences() {
        ticks current = current_ticks;
        for (unsigned t = 0; t < Project::MAX_TRACK; ++t) {
            ticks when = tracks[t].when_change;
            if (when > 0 && when <= current) {
                tracks[t].current = tracks[t].next;

                if (tracks[t].current->get_flags() & SEQF_REPEATED) {
                    tracks[t].when_change = current + tracks[t].current->get_length();
                } else {
                    tracks[t].when_change = 0;
                    tracks[t].next        = nullptr;
                }

                tracks[t].when_started = current;

                uchar channel = project.get_track(t)->get_midi_channel();

                // queue immediate note-offs
                for (uchar i = 0; i < NOTE_MAX; ++i) {
                    if (tracks[t].playing_notes[i]) {
                        tracks[t].playing_notes[i] = false;
                        router.queue_immediate(jack::MidiMessage::compose_note_off(channel, i));
                    }
                }
            }
        }
    }

    /// returns true if there's any sequence playing right now
    bool schedule_notes(ticks w_start, ticks w_stop) {
        // we lock all the track's sequences here and gain iterators
        auto walkers = lock_all_tracks();

        if (walkers.size() == 0) return false;

        bool added = false;

        // move all the sequences to the start of the window
        for (auto &sw : walkers) {
            sw.advance_to(w_start);

            // no more notes? the track is to be stopped
            if (sw.at_end()) {
                auto current = tracks[sw.track].current;
                tracks[sw.track].current = nullptr;
            }
        }

        do {
            // scan through all track handles to see who's next
            added = false;
            ticks c_ticks = 0;
            int   c_index = -1;
            int   i = -1;

            for (auto &w : walkers) {
                ++i;

                if (w.iter != w.handle.end()) {
                    ticks t = w.get_ticks();

                    if (t >= w_stop) continue;

                    if (t < c_ticks || c_index < 0) {
                        c_index = i;
                        c_ticks = t;
                    }
                }
            }

            // there's something to be queued
            if (c_index >= 0) {
                unsigned track = walkers[c_index].track;
                uchar channel = project.get_track(track)->get_midi_channel();

                auto event = *walkers[c_index].iter;

                // here, we remember the current active notes
                bool noteon = event.is_note_on();
                bool noteoff = event.is_note_off();

                if (noteon || noteoff) {
                    tracks[c_index].playing_notes[event.get_note()] = noteon;
                }

                jack::MidiMessage msg = midi_event_to_msg(
                        event, channel);

                router.queue_event(msg);
                // move the iter
                ++walkers[c_index].iter;
                added = true;
            }
        } while (added);

        return true;
    }

    std::vector<SequenceWalker> lock_all_tracks() {
        std::vector<SequenceWalker> result;

        result.reserve(Project::MAX_TRACK);

        for (unsigned t = 0; t < Project::MAX_TRACK; ++t) {
            if (tracks[t].current) {
                result.emplace_back(t, *tracks[t].current, tracks[t].when_started);
            }
        }

        return result;
    }

    struct TrackStatus {
        // only used in jack thread context
        bool playing_notes[NOTE_MAX];
        // atomics here because we lock-lessly access these
        Sequence *current = nullptr;
        std::atomic<Sequence *> next    = nullptr;
        std::atomic<ticks> when_started = 0; // ticks when the current sequence started playing
        std::atomic<ticks> when_change  = 0; // when do we change to the next track?
    };

    Project &project;
    Router  &router;
    jack::Client &client;
    std::atomic<ticks> current_ticks;
    TrackStatus tracks[Project::MAX_TRACK];
};
