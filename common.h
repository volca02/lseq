#pragma once

using uchar = unsigned char;
using ticks = unsigned long;

const uchar DEFAULT_VELOCITY = 100;
const ticks PPQN = 192; // ticks per quarter note
const uchar NOTE_C3 = 60; // note numbers are in semitones
const uchar NOTE_MAX = 127;

// quantizes ticks based on offset and slope
class TimeScaler {
public:
    TimeScaler(ticks offset, ticks step) : offset(offset), step(step) {}

    long to_quantum(ticks t) {
        // the cast is there to ensure we handle negative values too
        return (static_cast<long>(t) - offset) / static_cast<long>(step);
    }

    ticks to_ticks(long quantum) {
        long tck = quantum * step + offset;

        if (tck < 0)
            return 0;

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

    void scale(int scale) {
        for (;scale < 0;++scale)
            scale_down();
        for (;scale > 0;--scale)
            scale_up();
    }

    void set_step(ticks s) {
        step = s;
    }

    void scale_up() {
        step *= 2;
    }

    void scale_down() {
        ticks sb = step;
        step /= 2;
        if (!step) step = sb;
    }

    // TODO: triplet view
protected:
    long offset;
    ticks step;
};

// scales and also drum sequences...
// scales notes to positions in the grid
class NoteScaler {
public:
    enum ScaleMode {
        SM_CHROMATIC = 0, // am lazy, this is the easiest
    };

    NoteScaler(long offset, long mtx_h, ScaleMode mode = SM_CHROMATIC)
        : offset(offset), mtx_h(mtx_h), mode(mode)
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

        return (uchar)r;
    }

    long to_grid(uchar note) {
        return mtx_h - 1 - (note - offset);
   }

    bool is_in_scale(uchar note) {
        // TODO: implement after implementing different scales
        return true;
    }

    bool is_scale_mark(int y) {
        // mark all Cs by default
        long r = offset + mtx_h - 1 - y;
        return r % 12 == 0;
    }

protected:
    long offset;
    long mtx_h;
    ScaleMode mode;
};
