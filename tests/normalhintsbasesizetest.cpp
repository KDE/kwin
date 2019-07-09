/*
 * Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <cstdlib>
#include <iostream>
#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>

/*
 * This is a small test app to ensure that KWin calculates the size of a window correctly
 * according to ICCCM section 4.1.2.3
 *
 * The application creates a window and specifies the normal hints with:
 * * min size
 * * base size
 * * size increment
 *
 * With these normal flags the size should be calculated as:
 *     width = base_width + (i * width_inc)
 *     height = base_height + (j * height_inc)
 *
 * With i and j being non-negative integers!
 *
 * This application waits for configure notify events and calculates the i and j and
 * tries to calculate the size it expects. If it doesn't match it exits with a non-zero
 * exit code and prints the mismatching i and/or j value to stderr.
 *
 * To simply quit the application just click into the window. This will return with exit code 0.
 */
int main(int, char **)
{
    int screenNumber;
    xcb_connection_t *c = xcb_connect(nullptr, &screenNumber);

    auto getScreen = [=]() {
        const xcb_setup_t *setup = xcb_get_setup(c);
        auto it = xcb_setup_roots_iterator (setup);
        for (int i = 0; i < screenNumber; ++i) {
            xcb_screen_next(&it);
        }
        return it.data;
    };
    xcb_screen_t *screen = getScreen();

    xcb_window_t w = xcb_generate_id(c);
    const uint32_t values[2] = {
        screen->white_pixel,
        XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_STRUCTURE_NOTIFY
    };

    xcb_create_window(c, 0, w, screen->root, 0, 0, 365, 104, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      screen->root_visual, XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK, values);

    // set the normal hints
    xcb_size_hints_t hints;
    hints.flags = XCB_ICCCM_SIZE_HINT_P_MIN_SIZE | XCB_ICCCM_SIZE_HINT_BASE_SIZE | XCB_ICCCM_SIZE_HINT_P_RESIZE_INC;
    hints.min_width = 365;
    hints.min_height = 104;
    hints.base_width = 15;
    hints.base_height = 64;
    hints.width_inc = 9;
    hints.height_inc = 18;
    xcb_icccm_set_wm_normal_hints(c, w, &hints);

    // and map the window
    xcb_map_window(c, w);
    xcb_flush(c);

    bool error = false;
    while (xcb_generic_event_t *event = xcb_wait_for_event(c)) {
        bool exit = false;
        if ((event->response_type & ~0x80) == XCB_BUTTON_RELEASE) {
            exit = true;
        } else if ((event->response_type & ~0x80) == XCB_CONFIGURE_NOTIFY) {
            auto *ce = reinterpret_cast<xcb_configure_notify_event_t*>(event);
            const double i = (ce->width - hints.base_width) / (double)hints.width_inc;
            const double j = (ce->height - hints.base_height) / (double)hints.height_inc;
            // according to ICCCM the size should be:
            // width = base_width + (i * width_inc)
            // height = base_height + (j * height_inc)
            // thus if the window manager configured correctly we get the same result
            if (hints.base_width + (int(i) * hints.width_inc) != ce->width) {
                std::cerr << "Incorrect width - i factor is " << i << std::endl;
                exit = true;
                error = true;
            }
            if (hints.base_height + (int(j) * hints.height_inc) != ce->height) {
                std::cerr << "Incorrect height - j factor is " << i << std::endl;
                exit = true;
                error = true;
            }
        }
        free(event);
        if (exit) {
            break;
        }
    }

    xcb_disconnect(c);
    return error ? 1 : 0;
}
