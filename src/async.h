#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>

#include <xcb/xcb.h>

namespace KWin
{

struct X11AsyncData
{
    QPointer<const QObject> context;
    std::function<void()> callback;
};

class X11Async
{
public:
    explicit X11Async();
    ~X11Async();

    bool event(xcb_generic_event_t *event);

    void roundtrip(const QObject *context, std::function<void()> callback);

private:
    xcb_window_t m_window = XCB_WINDOW_NONE;
    xcb_atom_t m_counterAtom = XCB_ATOM_NONE;
    uint32_t m_counterValue = 0;
    QHash<uint32_t, X11AsyncData> m_roundtrips;
};

} // namespace KWin
