#include "ui.h"

UIScreen::UIScreen(UI &ui) : ui(ui), launchpad(ui.launchpad) {}

void UIScreen::set_active_mode_button(unsigned m) {
    launchpad.set_color(Launchpad::BC_SESSION, 0, m == 0 ? 3 : 0);
    launchpad.set_color(Launchpad::BC_USER1, 0, m == 1 ? 3 : 0);
    launchpad.set_color(Launchpad::BC_USER2, 0, m == 2 ? 3 : 0);
    launchpad.set_color(Launchpad::BC_MIXER, 0, m == 3 ? 3 : 0);
}

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
