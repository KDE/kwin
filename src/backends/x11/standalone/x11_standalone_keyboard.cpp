/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "x11_standalone_keyboard.h"

#include "main.h"

#include <QDebug>

#define explicit dont_use_cxx_explicit
#include <xcb/xkb.h>
#undef explicit
#include <xkbcommon/xkbcommon-x11.h>

namespace KWin
{

class X11KeyboardFilter : public X11EventFilter
{
public:
    X11KeyboardFilter(X11Keyboard *kbd, int eventType)
        : X11EventFilter(eventType)
        , m_kbd(kbd)
    {
    }

    bool event(xcb_generic_event_t *event) override
    {
        return m_kbd->event(event);
    }

private:
    X11Keyboard *m_kbd;
};

X11Keyboard::X11Keyboard()
    : m_xkbContext(xkb_context_new(XKB_CONTEXT_NO_FLAGS))
{
    const xcb_query_extension_reply_t *reply = xcb_get_extension_data(kwinApp()->x11Connection(), &xcb_xkb_id);
    if (!reply || !reply->present) {
        qWarning() << "XKeyboard extension is unavailable";
        return;
    }

    m_deviceId = xkb_x11_get_core_keyboard_device_id(kwinApp()->x11Connection());
    if (m_deviceId == -1) {
        qWarning() << "xkb_x11_get_core_keyboard_device_id() failed";
        return;
    }

    enum {
        requiredEvents =
            (XCB_XKB_EVENT_TYPE_NEW_KEYBOARD_NOTIFY
             | XCB_XKB_EVENT_TYPE_MAP_NOTIFY
             | XCB_XKB_EVENT_TYPE_STATE_NOTIFY),

        requiredNknDetails =
            (XCB_XKB_NKN_DETAIL_KEYCODES),

        requiredMapParts =
            (XCB_XKB_MAP_PART_KEY_TYPES
             | XCB_XKB_MAP_PART_KEY_SYMS
             | XCB_XKB_MAP_PART_MODIFIER_MAP
             | XCB_XKB_MAP_PART_EXPLICIT_COMPONENTS
             | XCB_XKB_MAP_PART_KEY_ACTIONS
             | XCB_XKB_MAP_PART_VIRTUAL_MODS
             | XCB_XKB_MAP_PART_VIRTUAL_MOD_MAP),

        requiredStateDetails =
            (XCB_XKB_STATE_PART_MODIFIER_BASE
             | XCB_XKB_STATE_PART_MODIFIER_LATCH
             | XCB_XKB_STATE_PART_MODIFIER_LOCK
             | XCB_XKB_STATE_PART_GROUP_BASE
             | XCB_XKB_STATE_PART_GROUP_LATCH
             | XCB_XKB_STATE_PART_GROUP_LOCK),
    };

    static const xcb_xkb_select_events_details_t details = {
        .affectNewKeyboard = requiredNknDetails,
        .newKeyboardDetails = requiredNknDetails,
        .affectState = requiredStateDetails,
        .stateDetails = requiredStateDetails,
    };

    xcb_void_cookie_t cookie =
        xcb_xkb_select_events_aux_checked(kwinApp()->x11Connection(),
                                          m_deviceId,
                                          requiredEvents,
                                          0,
                                          0,
                                          requiredMapParts,
                                          requiredMapParts,
                                          &details);

    xcb_generic_error_t *error = xcb_request_check(kwinApp()->x11Connection(), cookie);
    if (error) {
        free(error);
        return;
    }

    updateKeymap();

    m_filter = std::make_unique<X11KeyboardFilter>(this, reply->first_event);
}

X11Keyboard::~X11Keyboard()
{
    if (m_xkbState) {
        xkb_state_unref(m_xkbState);
        m_xkbState = nullptr;
    }
    if (m_xkbKeymap) {
        xkb_keymap_unref(m_xkbKeymap);
        m_xkbKeymap = nullptr;
    }
    if (m_xkbContext) {
        xkb_context_unref(m_xkbContext);
        m_xkbContext = nullptr;
    }
}

bool X11Keyboard::event(xcb_generic_event_t *gevent)
{
    union xkb_event {
        struct
        {
            uint8_t response_type;
            uint8_t xkbType;
            uint16_t sequence;
            xcb_timestamp_t time;
            uint8_t deviceID;
        } any;
        xcb_xkb_new_keyboard_notify_event_t new_keyboard_notify;
        xcb_xkb_map_notify_event_t map_notify;
        xcb_xkb_state_notify_event_t state_notify;
    } *event = reinterpret_cast<union xkb_event *>(gevent);

    if (event->any.deviceID == m_deviceId) {
        switch (event->any.xkbType) {
        case XCB_XKB_NEW_KEYBOARD_NOTIFY:
            if (event->new_keyboard_notify.changed & XCB_XKB_NKN_DETAIL_KEYCODES) {
                updateKeymap();
            }
            break;

        case XCB_XKB_MAP_NOTIFY:
            updateKeymap();
            break;

        case XCB_XKB_STATE_NOTIFY:
            xkb_state_update_mask(m_xkbState,
                                  event->state_notify.baseMods,
                                  event->state_notify.latchedMods,
                                  event->state_notify.lockedMods,
                                  event->state_notify.baseGroup,
                                  event->state_notify.latchedGroup,
                                  event->state_notify.lockedGroup);
            break;
        }
    }

    return false;
}

void X11Keyboard::updateKeymap()
{
    xkb_keymap *keymap = xkb_x11_keymap_new_from_device(m_xkbContext, kwinApp()->x11Connection(), m_deviceId, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!keymap) {
        qWarning() << "xkb_x11_keymap_new_from_device() failed";
        return;
    }

    xkb_state *state = xkb_x11_state_new_from_device(keymap, kwinApp()->x11Connection(), m_deviceId);
    if (!state) {
        xkb_keymap_unref(keymap);
        qWarning() << "xkb_x11_state_new_from_device() failed";
        return;
    }

    if (m_xkbState) {
        xkb_state_unref(m_xkbState);
    }
    if (m_xkbKeymap) {
        xkb_keymap_unref(m_xkbKeymap);
    }

    m_xkbState = state;
    m_xkbKeymap = keymap;
}

xkb_keymap *X11Keyboard::xkbKeymap() const
{
    return m_xkbKeymap;
}

xkb_state *X11Keyboard::xkbState() const
{
    return m_xkbState;
}

Qt::KeyboardModifiers X11Keyboard::modifiers() const
{
    Qt::KeyboardModifiers mods;

    if (xkb_state_mod_name_is_active(m_xkbState, XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE) == 1 || xkb_state_mod_name_is_active(m_xkbState, XKB_MOD_NAME_CAPS, XKB_STATE_MODS_EFFECTIVE) == 1) {
        mods |= Qt::ShiftModifier;
    }
    if (xkb_state_mod_name_is_active(m_xkbState, XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE) == 1) {
        mods |= Qt::AltModifier;
    }
    if (xkb_state_mod_name_is_active(m_xkbState, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE) == 1) {
        mods |= Qt::ControlModifier;
    }
    if (xkb_state_mod_name_is_active(m_xkbState, XKB_MOD_NAME_LOGO, XKB_STATE_MODS_EFFECTIVE) == 1) {
        mods |= Qt::MetaModifier;
    }

    return mods;
}

} // namespace KWin
