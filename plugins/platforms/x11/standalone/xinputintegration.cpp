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
#include "logging.h"
#include "x11cursor.h"

#include "keyboard_input.h"
#include "x11eventfilter.h"
#include <kwinglobals.h>

#include <X11/extensions/XInput2.h>
#include <X11/extensions/XI2proto.h>

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
        case XI_RawKeyPress:
            if (m_xkb) {
                m_xkb->updateKey(reinterpret_cast<xXIRawEvent*>(event)->detail - 8, InputRedirection::KeyboardKeyPressed);
            }
            break;
        case XI_RawKeyRelease:
            if (m_xkb) {
                m_xkb->updateKey(reinterpret_cast<xXIRawEvent*>(event)->detail - 8, InputRedirection::KeyboardKeyReleased);
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

    void setCursor(X11Cursor *cursor) {
        m_x11Cursor = QPointer<X11Cursor>(cursor);
    }
    void setXkb(Xkb *xkb) {
        m_xkb = xkb;
    }

private:
    QPointer<X11Cursor> m_x11Cursor;
    // TODO: QPointer
    Xkb *m_xkb = nullptr;
};


XInputIntegration::XInputIntegration(QObject *parent)
    : QObject(parent)
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
    m_xiEventFilter.reset(new XInputEventFilter(m_xiOpcode));
}

void XInputIntegration::setCursor(X11Cursor *cursor)
{
    m_xiEventFilter->setCursor(cursor);
}

void XInputIntegration::setXkb(Xkb *xkb)
{
    m_xiEventFilter->setXkb(xkb);
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
    if (m_majorVersion >= 2 && m_minorVersion >= 2) {
        // we need to listen to all events, which is only available with XInput 2.2
        XISetMask(mask1, XI_RawKeyPress);
        XISetMask(mask1, XI_RawKeyRelease);
    }

    evmasks[0].deviceid = XIAllMasterDevices;
    evmasks[0].mask_len = sizeof(mask1);
    evmasks[0].mask = mask1;
    XISelectEvents(display(), rootWindow(), evmasks, 1);
}

}
