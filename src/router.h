#pragma once

#include <iostream>

#include "jackmidi.h"

/// midi event router/scheduler. Connects to specified output port for midi output
/// receives midi events with/without time markings to be output there.
class Router {
public:
    static constexpr size_t RINGBUFFER_SIZE = 1024 * sizeof(jack::MidiMessage);

    Router(jack::Client &client, const char *output_name = nullptr)
            : client(client)
            , inport(client, "router::in", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput)
            , outport(client, "router::out", JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput)
            , immediate_events(RINGBUFFER_SIZE)
            , queued_events(RINGBUFFER_SIZE)
    {
        // TODO: connect the input port(s) to get midi keyboard support
        if (output_name) outport.connect_to(output_name);
        immediate_events.mlock();
        queued_events.mlock();
    }

    void process(jack_nframes_t nframes) {
        /// ======================== INTPUT ==========================
        // TODO: Process the input here.
        jack::MidiBuffer buf = inport.get_midi_buffer(nframes);

        int nevents = buf.get_event_count();

        for (int n = 0; n < nevents; ++n) {
            jack_midi_event_t ev;
            buf.get_event(ev, n);
            // TODO: process the event!
        }

        /// ======================== OUTPUT ==========================
        jack::MidiBuffer jbuf = outport.get_midi_buffer(nframes);
        jbuf.clear();

        jack_nframes_t last_frame_time = client.last_frame_time();

        // iterate all available Midi messages in the immediate ringbuffer
        output_events(jbuf, immediate_events, nframes, last_frame_time);
        // and also in the queued ringbuffer
        output_events(jbuf, queued_events, nframes, last_frame_time);
    }

    void output_events(jack::MidiBuffer &jbuf,
                       jack::RingBuffer &rb,
                       jack_nframes_t nframes,
                       jack_nframes_t last_frame_time)
    {
        while (rb.read_space()) {
            jack::MidiMessage msg;
            size_t read = rb.peek((char *)&msg, sizeof(msg));

            if (read != sizeof(msg)) {
                // TODO: report problem!
                rb.read_advance(read);
                continue;
            }

            // if the time of the event is out of this window, break out of the loop
            int t = msg.time + nframes - last_frame_time;

            // sometimes we have an event queued that should already be out?!
            if (t < 0) t = 0;
            if (t >= nframes) break;

            // skip the read bytes now
            rb.read_advance(read);

            jack_midi_data_t *evbuf = jbuf.event_reserve(t, 3);

            if (evbuf) {
                // TODO: add debug event logging here
                std::copy(std::begin(msg.data), std::end(msg.data), evbuf);
            } else {
                // TODO: report underrun/not enough space
            }
        }
    }

    // queues an event to be immediately output
    // the copy in the param is here by design, we modify the timing of the event
    bool queue_immediate(jack::MidiMessage msg) {
        // immediate send - use current frame time
        msg.time = client.frame_time();

        if (immediate_events.write_space() < sizeof(msg)) {
            // TODO: report overruns
            return false;
        }

        immediate_events.write(reinterpret_cast<const char*>(&msg), sizeof(msg));
        return true;
    }

    /** queues event to be output in time specified in the msg.time
     *  @note the msg.time has to be set relative to client.last_frame_time
     *  @note queued events have to be ordered by time
     */
    bool queue_event(const jack::MidiMessage &msg) {
        if (queued_events.write_space() < sizeof(msg)) {
            // TODO: report overruns
            return false;
        }

        queued_events.write(reinterpret_cast<const char*>(&msg), sizeof(msg));
        return true;
    }

protected:
    jack::Client &client;
    jack::Port inport;
    jack::Port outport;
    jack::RingBuffer immediate_events, queued_events;
};
