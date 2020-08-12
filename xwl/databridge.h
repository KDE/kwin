/*
    KWin - the KDE window manager

    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_XWL_DATABRIDGE
#define KWIN_XWL_DATABRIDGE

#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QPoint>

namespace KWayland
{
namespace Client
{
class DataDevice;
}
}
namespace KWaylandServer
{
class DataDeviceInterface;
class SurfaceInterface;
}

namespace KWin
{
class Toplevel;

namespace Xwl
{
class Xwayland;
class Clipboard;
class Dnd;
enum class DragEventReply;

/**
 * Interface class for all data sharing in the context of X selections
 * and Wayland's internal mechanism.
 *
 * Exists only once per Xwayland session.
 */
class DataBridge : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    static DataBridge *self();

    explicit DataBridge(QObject *parent = nullptr);
    ~DataBridge() override;

    DragEventReply dragMoveFilter(Toplevel *target, const QPoint &pos);

    KWayland::Client::DataDevice *dataDevice() const
    {
        return m_dataDevice;
    }
    KWaylandServer::DataDeviceInterface *dataDeviceIface() const
    {
        return m_dataDeviceInterface;
    }
    Dnd *dnd() const
    {
        return m_dnd;
    }

    bool nativeEventFilter(const QByteArray &eventType, void *message, long int *result) override;

private:
    void init();

    Clipboard *m_clipboard = nullptr;
    Dnd *m_dnd = nullptr;

    /* Internal data device interface */
    KWayland::Client::DataDevice *m_dataDevice = nullptr;
    KWaylandServer::DataDeviceInterface *m_dataDeviceInterface = nullptr;

    Q_DISABLE_COPY(DataBridge)
};

} // namespace Xwl
} // namespace KWin

#endif
