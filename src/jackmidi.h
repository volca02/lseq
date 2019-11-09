#pragma once

#include <vector>
#include <memory>

#include <jack/jack.h>
#include <jack/midiport.h>
#include <jack/ringbuffer.h>

#include "common.h"
#include "error.h"

namespace jack {

class Port;
class Client;

class JackException : public Exception {
public:
    JackException(const char *msg) : Exception(msg) {}
};

// error/info function binders - we want the jack log suppressed unless specified
class LogHandler {
public:
    LogHandler() {
        jack_set_info_function(&info_handler);
        jack_set_error_function(&err_handler);
    }

    static void info_handler(const char *msg) {}
    static void err_handler(const char *msg) {}
};

class Client {
public:
    Client(const char *name)
            : client(nullptr)
    {
        jack_status_t status;
        client = jack_client_open(name, JackNullOption, &status);

        if (!client || (status & JackServerFailed)) {
            throw JackException("Cannot create client, is jack server running?");
        }
    }

    ~Client() {
        jack_deactivate(client);
        jack_client_close(client);
        client = nullptr;
    }

    // Movable, not copyable
    Client(const Client &) = delete;
    Client &operator =(const Client &) = delete;
    Client(Client &&other) : client(other.client) { other.client = nullptr; }
    Client &operator =(Client &&o) {
        if (this == &o) return *this;
        client = o.client;
        o.client = nullptr;
        return *this;
    }

    operator jack_client_t*() { return client; }
    operator const jack_client_t*() const { return client; }

    std::vector<std::string> get_ports(const char *port_name_pattern = nullptr,
                                       const char *type_name_pattern = nullptr,
                                       unsigned long flags = 0) const
    {
        std::unique_ptr<const char *, decltype(&jack_free)> ports(
                jack_get_ports(
                        client, port_name_pattern, type_name_pattern, flags),
                &jack_free);

        const char **cur = ports.get();

        // iterate all values until null
        if (!cur) return {};

        std::vector<std::string> result;

        for (;*cur;++cur) {
            result.emplace_back(*cur);
        }

        return result;
    }

    void activate() {
        jack_activate(client);
    }

    void deactivate() {
        jack_deactivate(client);
    }

    jack_nframes_t last_frame_time() {
        return jack_last_frame_time(client);
    }

    jack_nframes_t frame_time() {
        return jack_frame_time(client);
    }

    /// sample rate per second. in combination with nframes of process call
    /// this can be used to determine maximal amount of data transferrable
    /// each process call.
    jack_nframes_t sample_rate() {
        return jack_get_sample_rate(client);
    }

    // converts no. of frames ti miliseconds
    double frames_to_ms(jack_nframes_t nframes) {
        // this may be used to calculate the maximal amount of
        // bytes per frame that we can output to midi without saturating it
        jack_nframes_t sr = sample_rate();
        return (nframes * 1000) / sr;
    }



    // implement in a class listening to the jack events
    struct Callback {
        virtual int process(jack_nframes_t nframes) = 0;
    };

    void set_callback(Callback &cb) {
        if (jack_set_process_callback(client, jackProcessCallback, &cb))
            throw JackException("Cannot set process callback");
    }

protected:
    // callback adapter
    static int jackProcessCallback(jack_nframes_t nframes, void *arg) {
        if (arg) {
            return static_cast<Callback*>(arg)->process(nframes);
        }

        return 0;
    }

    jack_client_t *client;
};

class Port;

class MidiBuffer {
public:
    int get_event_count() {
        return jack_midi_get_event_count(buffer);
    }

    void clear() {
        jack_midi_clear_buffer(buffer);
    }

    void get_event(jack_midi_event_t &ev, int order) {
        jack_midi_event_get(&ev, buffer, order);
    }

    // this allocates a buffer for midi data for given frame time
    jack_midi_data_t *event_reserve(jack_nframes_t time, size_t data_size)
    {
        return jack_midi_event_reserve(buffer, time, data_size);
    }

    MidiBuffer(MidiBuffer &&other) : buffer(other.buffer)
    {
        other.buffer = nullptr;
    }

    MidiBuffer &operator=(MidiBuffer &&o)
    {
        if (this == &o) return *this;
        buffer = o.buffer;
        o.buffer = nullptr;
        return *this;
    }

protected:
    friend class Port;
    MidiBuffer(const MidiBuffer &) = delete;
    MidiBuffer &operator =(const MidiBuffer &) = delete;

    MidiBuffer(void *buf) : buffer(buf) {}

    void *buffer;
};

class Port {
public:
    Port(Client &client,
         const char *name,
         const char *port_type = JACK_DEFAULT_MIDI_TYPE,
         unsigned long flags   = JackPortIsOutput,
         unsigned long buffer_size = 0)
            : client(client),
              port(jack_port_register(
                    client, name, port_type, flags, 0))
    {
        if (!port) {
            throw JackException("Cannot create port, is jack server running?");
        }
    }

    ~Port() {
        jack_port_unregister(client, port);
        port = nullptr;
    }

    // movable, not copyable
    Port(const Port &) = delete;
    Port &operator =(const Port &) = delete;
    Port(Port &&other) : client(other.client), port(other.port)
    {
        other.port = nullptr;
    }
    Port &operator=(Port &&o)
    {
        if (this == &o) return *this;
        port = o.port;
        o.port = nullptr;
        return *this;
    }

    operator jack_port_t *() { return port; }
    operator const jack_port_t *() const { return port; }

    MidiBuffer get_midi_buffer(int nframes) {
        return {jack_port_get_buffer(port, nframes)};
    }

    // establishes a connection of this registered port with a different one
    // allowing for data to go through
    void connect_from(const char *target) {
        auto ec = jack_connect(client, target, name());
        if (ec)
            throw Exception(format(
                    "Cannot bind port ", name(), " from ", target, ": ", ec));
    }

    void disconnect_from(const char *target) {
        auto ec = jack_disconnect(client, target, name());
        if (ec)
            throw Exception(format(
                    "Cannot unbind port ", name(), " from ", target, ": ", ec));
    }

    void connect_to(const char *target) {
        auto ec = jack_connect(client, name(), target);
        if (ec)
            throw Exception(format(
                    "Cannot bind port ", name(), " to ", target, ": ", ec));
    }

    void disconnect_to(const char *target) {
        auto ec = jack_disconnect(client, name(), target);
        if (ec)
            throw Exception(format(
                    "Cannot unbind port ", name(), " to ", target, ": ", ec));
    }

    const char *name() {
        return jack_port_name(port);
    }

protected:
    Client &client;
    jack_port_t *port;
};

// A simplified midi message used to work with the ring buffer
struct MidiMessage {
    MidiMessage() = default;

    MidiMessage(std::initializer_list<uchar> l) {
        len = l.size();
        uint8_t pos = 0;
        for (uchar c : l) {
            if (pos >= 3) break;
            data[pos++] = c;
        }
        len = pos;
    }

    static MidiMessage compose_note_on(uchar channel, uchar note, uchar velo) {
        return {static_cast<uchar>(EV_NOTE_ON | channel), note, velo};
    }

    static MidiMessage compose_note_off(uchar channel, uchar note) {
        return {static_cast<uchar>(EV_NOTE_OFF | channel), note, 0x0};
    }

    jack_nframes_t time = 0;
    uint8_t len = 0;
    uchar data[3] = {0x0,0x0,0x0};
};

/** event queue for midi events. Used to schedule events to be output in
 * process call.
 * @note Should be used by two threads - one read thread and one write thread.
 */
class RingBuffer {
public:
    RingBuffer(size_t size)
            : size(size), ringbuffer(jack_ringbuffer_create(size))
    {
    }

    ~RingBuffer() {
        jack_ringbuffer_free(ringbuffer);
        ringbuffer = nullptr;
    }

    size_t read_space() {
        return jack_ringbuffer_read_space(ringbuffer);
    }

    size_t write_space() {
        return jack_ringbuffer_write_space(ringbuffer);
    }

    int mlock() {
        return jack_ringbuffer_mlock(ringbuffer);
    }

    void reset() {
        jack_ringbuffer_reset(ringbuffer);
    }

    size_t peek(char *data, size_t size) {
        return jack_ringbuffer_peek(ringbuffer, data, size);
    }

    size_t read(char *data, size_t size) {
        return jack_ringbuffer_read(ringbuffer, data, size);
    }

    void read_advance(size_t size) {
        jack_ringbuffer_read_advance(ringbuffer, size);
    }

    size_t write(const char *data, size_t size) {
        return jack_ringbuffer_write(ringbuffer, data, size);
    }

protected:
    size_t size;
    jack_ringbuffer_t *ringbuffer;
};

} // namespace jack
