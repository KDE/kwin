#include "async.h"
#include "main.h"

#include <xcb/xcb_event.h>

namespace KWin
{

X11Async::X11Async()
{
    xcb_connection_t *connection = kwinApp()->x11Connection();

    const uint32_t eventMask = XCB_EVENT_MASK_PROPERTY_CHANGE;

    m_window = xcb_generate_id(connection);
    xcb_create_window(connection, XCB_COPY_FROM_PARENT, m_window, kwinApp()->x11RootWindow(), -1, -1, 1, 1, 0, XCB_WINDOW_CLASS_INPUT_ONLY, XCB_COPY_FROM_PARENT, XCB_CW_EVENT_MASK, &eventMask);

    const char counterAtomName[] = "counter";
    xcb_intern_atom_cookie_t cookie = xcb_intern_atom(connection, 0, sizeof(counterAtomName), counterAtomName);
    if (xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(connection, cookie, nullptr)) {
        m_counterAtom = reply->atom;
        free(reply);
    }
}

X11Async::~X11Async()
{
    xcb_destroy_window(kwinApp()->x11Connection(), m_window);
}

bool X11Async::event(xcb_generic_event_t *event)
{
    if (XCB_EVENT_RESPONSE_TYPE(event) == XCB_PROPERTY_NOTIFY) {
        const auto propertyNotifyEvent = reinterpret_cast<xcb_property_notify_event_t *>(event);
        if (propertyNotifyEvent->window == m_window && propertyNotifyEvent->atom == m_counterAtom) {
            const auto it = m_roundtrips.find(event->full_sequence);
            if (it != m_roundtrips.end()) {
                it->callback();
                m_roundtrips.erase(it);
            }

            return true;
        }
    }

    return false;
}

void X11Async::roundtrip(const QObject *context, std::function<void()> callback)
{
    ++m_counterValue;

    xcb_void_cookie_t cookie = xcb_change_property(kwinApp()->x11Connection(), XCB_PROP_MODE_REPLACE, m_window, m_counterAtom, XCB_ATOM_INTEGER, 32, 1, &m_counterValue);
    m_roundtrips[cookie.sequence] = X11AsyncData{
        .context = context,
        .callback = callback,
    };
}

} // namespace KWin
