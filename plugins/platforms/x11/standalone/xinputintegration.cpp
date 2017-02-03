/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "xinputintegration.h"
#include "main.h"
#include "logging.h"
#include "platform.h"
#include "x11cursor.h"

#include "input.h"
#include "x11eventfilter.h"
#include "modifier_only_shortcuts.h"
#include <kwinglobals.h>

#include <X11/extensions/XInput2.h>
#include <X11/extensions/XI2proto.h>

#include <linux/input.h>

namespace KWin
{

class XInputEventFilter : public X11EventFilter
{
public:
    XInputEventFilter(int xi_opcode)
        : X11EventFilter(XCB_GE_GENERIC, xi_opcode, QVector<int>{XI_RawMotion, XI_RawButtonPress, XI_RawButtonRelease, XI_RawKeyPress, XI_RawKeyRelease})
        {}
    virtual ~XInputEventFilter() = default;

    bool event(xcb_generic_event_t *event) override {
        xcb_ge_generic_event_t *ge = reinterpret_cast<xcb_ge_generic_event_t *>(event);
        switch (ge->event_type) {
        case XI_RawKeyPress: {
            auto re = reinterpret_cast<xXIRawEvent*>(event);
            kwinApp()->platform()->keyboardKeyPressed(re->detail - 8, re->time);
            break;
        }
        case XI_RawKeyRelease: {
            auto re = reinterpret_cast<xXIRawEvent*>(event);
            kwinApp()->platform()->keyboardKeyReleased(re->detail - 8, re->time);
            break;
        }
        case XI_RawButtonPress: {
                auto e = reinterpret_cast<xXIRawEvent*>(event);
                switch (e->detail) {
                // TODO: this currently ignores left handed settings, for current usage not needed
                // if we want to use also for global mouse shortcuts, this needs to reflect state correctly
                case XCB_BUTTON_INDEX_1:
                    kwinApp()->platform()->pointerButtonPressed(BTN_LEFT, e->time);
                    break;
                case XCB_BUTTON_INDEX_2:
                    kwinApp()->platform()->pointerButtonPressed(BTN_MIDDLE, e->time);
                    break;
                case XCB_BUTTON_INDEX_3:
                    kwinApp()->platform()->pointerButtonPressed(BTN_RIGHT, e->time);
                    break;
                case XCB_BUTTON_INDEX_4:
                case XCB_BUTTON_INDEX_5:
                    // vertical axis, ignore on press
                    break;
                // TODO: further buttons, horizontal scrolling?
                }
            }
            if (m_x11Cursor) {
                m_x11Cursor->schedulePoll();
            }
            break;
        case XI_RawButtonRelease: {
                auto e = reinterpret_cast<xXIRawEvent*>(event);
                switch (e->detail) {
                // TODO: this currently ignores left handed settings, for current usage not needed
                // if we want to use also for global mouse shortcuts, this needs to reflect state correctly
                case XCB_BUTTON_INDEX_1:
                    kwinApp()->platform()->pointerButtonReleased(BTN_LEFT, e->time);
                    break;
                case XCB_BUTTON_INDEX_2:
                    kwinApp()->platform()->pointerButtonReleased(BTN_MIDDLE, e->time);
                    break;
                case XCB_BUTTON_INDEX_3:
                    kwinApp()->platform()->pointerButtonReleased(BTN_RIGHT, e->time);
                    break;
                case XCB_BUTTON_INDEX_4:
                    kwinApp()->platform()->pointerAxisVertical(120, e->time);
                    break;
                case XCB_BUTTON_INDEX_5:
                    kwinApp()->platform()->pointerAxisVertical(-120, e->time);
                    break;
                // TODO: further buttons, horizontal scrolling?
                }
            }
            if (m_x11Cursor) {
                m_x11Cursor->schedulePoll();
            }
            break;
        default:
            if (m_x11Cursor) {
                m_x11Cursor->schedulePoll();
            }
            break;
        }
        return false;
    }

    void setCursor(const QPointer<X11Cursor> &cursor) {
        m_x11Cursor = cursor;
    }

private:
    QPointer<X11Cursor> m_x11Cursor;
};

class XKeyPressReleaseEventFilter : public X11EventFilter
{
public:
    XKeyPressReleaseEventFilter(uint32_t type)
        : X11EventFilter(type)
    {}
    ~XKeyPressReleaseEventFilter() = default;

    bool event(xcb_generic_event_t *event) override {
        xcb_key_press_event_t *ke = reinterpret_cast<xcb_key_press_event_t *>(event);
        if (ke->event == ke->root) {
            const uint8_t eventType = event->response_type & ~0x80;
            if (eventType == XCB_KEY_PRESS) {
                kwinApp()->platform()->keyboardKeyPressed(ke->detail - 8, ke->time);
            } else {
                kwinApp()->platform()->keyboardKeyReleased(ke->detail - 8, ke->time);
            }
        }
        return false;
    }
};

XInputIntegration::XInputIntegration(Display *display, QObject *parent)
    : QObject(parent)
    , m_x11Display(display)
{
}

XInputIntegration::~XInputIntegration() = default;

void XInputIntegration::init()
{
    Display *dpy = display();
    int xi_opcode, event, error;
    // init XInput extension
    if (!XQueryExtension(dpy, "XInputExtension", &xi_opcode, &event, &error)) {
        qCDebug(KWIN_X11STANDALONE) << "XInputExtension not present";
        return;
    }

    // verify that the XInput extension is at at least version 2.0
    int major = 2, minor = 0;
    int result = XIQueryVersion(dpy, &major, &minor);
    if (result == BadImplementation) {
        // Xinput 2.2 returns BadImplementation if checked against 2.0
        major = 2;
        minor = 2;
        if (XIQueryVersion(dpy, &major, &minor) != Success) {
            qCDebug(KWIN_X11STANDALONE) << "Failed to init XInput";
            return;
        }
    } else if (result != Success) {
        qCDebug(KWIN_X11STANDALONE) << "Failed to init XInput";
        return;
    }
    m_hasXInput = true;
    m_xiOpcode = xi_opcode;
    m_majorVersion = major;
    m_minorVersion = minor;
    qCDebug(KWIN_X11STANDALONE) << "Has XInput support" << m_majorVersion << "." << m_minorVersion;
}

void XInputIntegration::setCursor(X11Cursor *cursor)
{
    m_x11Cursor = QPointer<X11Cursor>(cursor);
}

void XInputIntegration::startListening()
{
    // this assumes KWin is the only one setting events on the root window
    // given Qt's source code this seems to be true. If it breaks, we need to change
    XIEventMask evmasks[1];
    unsigned char mask1[XIMaskLen(XI_LASTEVENT)];

    memset(mask1, 0, sizeof(mask1));

    XISetMask(mask1, XI_RawMotion);
    XISetMask(mask1, XI_RawButtonPress);
    XISetMask(mask1, XI_RawButtonRelease);
    if (m_majorVersion >= 2 && m_minorVersion >= 1) {
        // we need to listen to all events, which is only available with XInput 2.1
        XISetMask(mask1, XI_RawKeyPress);
        XISetMask(mask1, XI_RawKeyRelease);
    }

    evmasks[0].deviceid = XIAllMasterDevices;
    evmasks[0].mask_len = sizeof(mask1);
    evmasks[0].mask = mask1;
    XISelectEvents(display(), rootWindow(), evmasks, 1);
    m_xiEventFilter.reset(new XInputEventFilter(m_xiOpcode));
    m_xiEventFilter->setCursor(m_x11Cursor);
    m_keyPressFilter.reset(new XKeyPressReleaseEventFilter(XCB_KEY_PRESS));
    m_keyReleaseFilter.reset(new XKeyPressReleaseEventFilter(XCB_KEY_RELEASE));

    // install the input event spies also relevant for X11 platform
    input()->installInputEventSpy(new ModifierOnlyShortcuts);
}

}
