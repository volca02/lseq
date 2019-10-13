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
            , ringbuffer(RINGBUFFER_SIZE)
    {
        // TODO: connect the input port(s) to get midi keyboard support
        if (output_name) outport.connect_to(output_name);
        ringbuffer.mlock();
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

        // iterate all available Midi messages in the ringbuffer
        while (ringbuffer.read_space()) {
            jack::MidiMessage msg;
            size_t read = ringbuffer.peek((char *)&msg, sizeof(msg));

            if (read != sizeof(msg)) {
                // TODO: report problem!
                ringbuffer.read_advance(read);
                continue;
            }

            // if the time of the event is out of this window, break out of the loop
            int t = msg.time + nframes - last_frame_time;

            // sometimes we have an event queued that should already be out?!
            if (t < 0) t = 0;
            if (t >= nframes) break;

            // skip the read bytes now
            ringbuffer.read_advance(read);

            jack_midi_data_t *evbuf = jbuf.event_reserve(t, 3);
            std::copy(std::begin(msg.data), std::end(msg.data), evbuf);
        }
    }

    void queue_immediate(jack::MidiMessage msg) {
        // immediate send - use current frame time
        msg.time = client.frame_time();

        if (ringbuffer.write_space() < sizeof(msg)) {
            // TODO: report overruns
            return;
        }

        ringbuffer.write(reinterpret_cast<const char*>(&msg), sizeof(msg));
    }

protected:
    jack::Client &client;
    jack::Port inport;
    jack::Port outport;
    jack::RingBuffer ringbuffer;
};
