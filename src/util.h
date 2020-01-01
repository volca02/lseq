// Utilities with thicker include dependencies

#include "common.h"
#include "event.h"
#include "jackmidi.h"

inline jack::MidiMessage midi_event_to_msg(const Event &ev, uchar channel) {
    const auto &data = ev.get_data();
    return {static_cast<uchar>(ev.get_status() | channel), data[0], data[1]};
}
