#pragma once

#include <array>
#include <string>
#include <sstream>
#include <iostream>


#include <jack/types.h>

using uchar = unsigned char;
// signed for easier math ops around this
using ticks = long;

/// default note velocity...
const uchar DEFAULT_VELOCITY = 100;
const ticks PPQN             = 192;  // ticks per quarter note
const uchar NOTE_C3          = 48;   // note numbers are in semitones
const uchar NOTE_MAX         = 127;  // this is the highest note MIDI can handle
const double DEFAULT_BPM     = 120;  // default BPM for new projects

// channel is here really for the easiness of fixing - number is hard to grep
const uchar MIDI_CH_DEFAULT  = 2;    // default MIDI channel (ch 1. is 0 here, 9 means ch 10)

const ticks SEQUENCE_DEFAULT_LENGTH = 8*PPQN; // default length of the sequence - 8 quarter notes


/// converts the tick bpm to microsecond tick length
inline double pulse_length_us(double bpm, ticks ppqn) {
    return 60000000.0 / ppqn / bpm;
}

/// rounds up to the nearest multiple of interval i for ticks t
inline ticks next_multiple(ticks t, ticks i) {
    return ((t + i - 1) / i) * i;
}

/** Converts ticks (presumably tick delta) to microseconds.
 *
 * Ticks are time-independent - i.e. 4*PPQN give us whole note, regardless of
 * the set tempo. To calculate the microsecond steps per one tick, we have to
 * know the tempo (BPM). For output, as we use jacks jack_nframes_t (which
 * depends on the sample rate), we first have to normalize it to microseconds,
 * then we convert that to frames within the jack output code.
 *
 */
inline double ticks_to_us(ticks t, double bpm) {
    return double(t) * pulse_length_us(bpm, PPQN);
}

/** converts microseconds to ticks
 */
inline ticks us_to_ticks(double us, double bpm) {
    return us / pulse_length_us(bpm, PPQN);
}

/// Types of midi status
enum MidiStatus : uchar {
    EV_STATUS_BIT       = 0x80,
    EV_NOTE_OFF         = 0x80,
    EV_NOTE_ON          = 0x90,
    EV_AFTERTOUCH       = 0xA0,
    EV_CONTROL_CHANGE   = 0xB0,
    EV_PROGRAM_CHANGE   = 0xC0,
    EV_CHANNEL_PRESSURE = 0xD0,
    EV_PITCH_WHEEL      = 0xE0,
    EV_CLEAR_CHAN_MASK  = 0xF0,
    EV_MIDI_CLOCK       = 0xF8,
    EV_SYSEX            = 0xF0,
    EV_SYSEX_END        = 0xF7
};

/// bitmap of flags applicable to sequences
enum SequenceFlags : unsigned {
    SEQF_REPEATED       = 1
};

/** quantizes ticks based on offset and slope */
class TimeScaler {
public:
    using Scaling = std::pair<const char *, ticks>;

    /// scaling table. even rows are normal, odd rows are triplets
    /// the name references the time quantity of each step
    static constexpr std::array<Scaling, 16> scales{{
            {"1",   PPQN * 4},
            {"1",   PPQN * 4},  // does not make sense to 1/3 this...
            {"1/2", PPQN * 2},
            {"1/3", PPQN * 4 / 3},
            {"1/4", PPQN},
            {"1/6", PPQN * 2 / 3},
            {"1/8", PPQN / 2},
            {"1/12", PPQN / 3},
            {"1/16", PPQN / 4},
            {"1/24", PPQN / 2 / 3},
            {"1/32", PPQN / 8},
            {"1/48", PPQN / 4 / 3},
            {"1/64", PPQN / 16},
            {"1/96", PPQN / 8 / 3},
            {"1/128", PPQN / 32},
            {"1/192", PPQN / 16 / 3}}};

    TimeScaler(ticks offset) : offset(offset), step(PPQN) { update_scaling(); }

    long to_quantum(ticks t) {
        // the cast is there to ensure we handle negative values too
        return (static_cast<long>(t) - offset) / static_cast<long>(step);
    }

    // convers relative positioning (number of steps on LP) to ticks
    ticks quantum_to_ticks(long quantum) {
        return quantum * step;
    }

    // converts the X position on LP to absolute ticks
    ticks to_ticks(long quantum) {
        long tck = quantum * step + offset;
        return (ticks)tck;
    }

    long length_to_quantum(ticks l) {
        return l / step;
    }

    bool is_scale_accurate(ticks t) {
        return (t - offset) % step == 0;
    }

    void scroll(int direction) {
        if (direction < 0)
            offset -= step;
        else if (direction > 0)
            offset += step;

        if (offset < 0) offset = 0;
    }

    long get_offset() const { return offset; }

    ticks get_step() const { return step; }

    /// move scaling out/in by specified number of steps (negative numbers zoom
    /// out)
    void scale(int scale) {
        for (;scale < 0;++scale)
            scale_out();
        for (;scale > 0;--scale)
            scale_in();
    }

    void set_step(ticks s) {
        step = s;
    }

    /// each division will contain more ticks
    void scale_out() {
        if (scaling >= 2) {
            scaling -= 2;
            update_scaling();
        }
    }

    /// each division will contain less ticks
    void scale_in() {
        if (scaling + 2 <= scales.size()) {
            scaling += 2;
            update_scaling();
        }
    }

    bool get_triplets() const { return triplet; }

    void set_triplets(bool t) {
        triplet = t;
        update_scaling();
    }

    void switch_triplets() {
        triplet = !triplet;
        update_scaling();
    }

    const char *scale_name() const {
        return scales[scale_index()].first;
    }

protected:
    void update_scaling() {
        // fixup in case we overflow
        if (scaling + 1 >= scales.size()) {
            scaling = scales.size() - 2;
        }

        step = scales[scale_index()].second;
    }

    unsigned scale_index() const {
        if (scaling >= 2)
            return scaling + (triplet ? -1 : 0);
        else
            return scaling;
    }

    bool triplet = false;
    long offset = 0;

    unsigned scaling = 4; // default is 1/4

    ticks step = PPQN; // default to quarter notes
};


/// implements a semitone list based scale/position bidirectional conversion
class Scale {
public:
    constexpr static const uchar INVALID = 0xFF;

    constexpr Scale(const char *name,
                    std::initializer_list<uchar> lst)
        : count(lst.size())
    {
        uint8_t pos = 0;
        uchar idx   = 0;

        for (uchar i = 0; i < 12; ++i) {
            scale[i] = 0;
            inverse[i] = INVALID;
        }

        for (uchar c : lst) {
            scale[idx] = c;
            inverse[c] = idx;
            mask |= 1 << c;
            ++idx;
        }
    }

    /** Converts positional coordinate (in scale steps) to note value, based on
     * our scale and base note of the scale.
     */
    uchar position_to_note(uchar base_note, uchar position) const {
        // whole number of count occurences is count of octaves below
        uchar reps     = position / count;
        // position in the octave itself
        uchar relative = position - reps * count;
        uchar note     = reps * 12 + base_note + scale[relative];
        return note;
    }

    /** Returns position of the given note in this scaling, or INVALID for
     * invalid notes.
     */
    uchar note_to_position(uchar base_note, uchar note) const {
        // position in repetition is based on the nearest lower multiply of base_note
        uchar reps     = (note - base_note) / 12;
        uchar relative = (note - base_note) % 12;

        uchar pos = inverse[relative];

        if (pos != INVALID) {
            return reps * count + pos;
        }

        return INVALID;
    }

private:
    // forward (position to tone)
    uchar count;
    uchar scale[12]   = {0,0,0,0,0,0,0,0,0,0,0,0};
    // backward (tone to position)
    uchar inverse[12] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    uint16_t mask      = 0;
};

// scales and also drum sequences...
// scales notes to positions in the grid
class NoteScaler {
public:
    using Bitmask = uint16_t; // enough bits for one octave
    static constexpr const uchar INVALID = 0xFF;

    /** 10 Different scale modes, listing relative semitone lists each
     *  1 3   6 8 10
     * 0 2 4 5 7 9 11
     */
    static constexpr std::array<Scale, 11> scales{{
            {"Chromatic",        {0,1,2,3,4,5,6,7,8,9,10,11}},
            {"Major",            {0,  2,  4,5,  7,  9,   11}},
            {"Minor",            {0,  2,3,  5,  7,8,  10}},
            {"Melodic Minor",    {0,  2,3,  5,  7,  9,   11}},
            {"Harmonic Minor",   {0,  2,3,  5,  7,8,     11}},
            {"Blues",            {0,    3,  5,6,7,    10}},
            {"Myxolidian",       {0,  2,  4,5,  7,  9,10}},
            {"Dorian",           {0,  2,3,  5,  7,  9,10}},
            {"Major Pentatonic", {0,  2,  4,    7,  9   }},
            {"Minor Pentatonic", {0,    3,4,    7,    10}},
            {"Diminished",       {0,  2,  4,  6,  8,  10}}}};

    NoteScaler(long offset, long mtx_h, uchar scale = 0)
        : offset(offset), mtx_h(mtx_h), scidx(scale)
    {}

    void scroll(int direction) {
        if (direction < 0)
            offset -= 1;
        else if (direction > 0)
            offset += 1;
    }

    uchar to_note(int y) {
        long r = offset + mtx_h - 1 - y;
        if (r < 0)
            return 0;
        if (r > NOTE_MAX)
            return NOTE_MAX;

        // use scale to convert the position to note
        auto rnote = scale().position_to_note(base_note, (uchar)r);
        return rnote;
    }

    long to_grid(uchar note) {
        uchar pos = scale().note_to_position(base_note, note);

        if (pos == Scale::INVALID) {
            // TODO: Handle invalid notes!
            return 0;
        }

        // recalc to window
        return mtx_h - 1 - (pos - offset);
   }

    bool is_in_scale(uchar note) {
        return to_grid(note) != Scale::INVALID;
    }

    uchar move_steps(uchar note, int8_t steps) {
        return std::min(127, std::max(0, note + steps));
    }

    bool is_scale_mark(int y) {
        long note = to_note(y);
        return (note + base_note) % 12 == 0;
    }

    void switch_scale() {
        // bottom row should stay on the same position
        // but offset is relative to the old scale
        // so we have to get note of the bottom row and back
        uchar note_off = to_note(mtx_h - 1);
        scidx = (scidx + 1) % scales.size();
        offset = scale().note_to_position(base_note, note_off);
        // TODO: Notification system should output scale change here
    }

    const Scale &scale() const {
        if (scidx >= scales.size())
            return scales[0];
        return scales[scidx];
    }

protected:
    long offset;
    long mtx_h;
    uchar scidx     = 0;
    uchar base_note = 0; // base note for current scale (0 == C)
};

inline int lowest_bit_set(uchar c) {
    // 4 bit lookup
    int nib[16] = {-1, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0};

    if (c & 0xf)
        return nib[c & 0xf];

    if (c >> 4)
        return nib[c >> 4] + 4;

    return -1;
}

/*
0000 -1
0001  0
0010  1
0011  1
0100  2
0101  2
0110  2
0111  2
1...  3
..
*/

// essentially a logarithm
inline int highest_bit_set(uchar c) {
    // 4 bit lookup
    int nib[16] = {-1, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3};

    if (c >> 4)
        return nib[c >> 4] + 4;

    if (c & 0xf)
        return nib[c & 0xf];

    return -1;
}

// TODO: This is basically highest_bit_set(c & (0xff >> (7 - pos)))
// which would probably be faster than a cycle with comparison
inline uchar nearest_lower_bit(uchar c, uchar pos) {
    uchar cand = pos;

    for (uchar o = 0; o < pos; ++o) {
        if (c >> o) cand = o;
    }

    return cand;
}

template <typename HeadT>
void format_to(std::ostringstream &oss, const HeadT &head)
{
    oss << head;
}

template <typename HeadT, typename... ArgsT>
void format_to(std::ostringstream &oss, const HeadT &head, ArgsT... rest)
{
    oss << head;
    format_to(oss, rest...);
}

// string stream formatters with more comfortable signatures
template <typename ...ArgsT> std::string format(ArgsT... args) {
    std::ostringstream ss;

    format_to(ss, args...);

    return ss.str();
}
