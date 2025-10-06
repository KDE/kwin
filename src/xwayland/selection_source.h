/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QList>
#include <QObject>

#include <xcb/xcb.h>

class QSocketNotifier;

struct xcb_selection_request_event_t;
struct xcb_xfixes_selection_notify_event_t;

namespace KWin
{
class DataDeviceInterface;
class DataSourceInterface;
class AbstractDataSource;

namespace Xwl
{
class Selection;
class XwlDataSource;

/**
 * Base class representing a data source.
 */
class SelectionSource : public QObject
{
    Q_OBJECT

public:
    SelectionSource(Selection *selection);

    xcb_timestamp_t timestamp() const
    {
        return m_timestamp;
    }
    void setTimestamp(xcb_timestamp_t time)
    {
        m_timestamp = time;
    }

protected:
    Selection *selection() const
    {
        return m_selection;
    }
    void setWindow(xcb_window_t window)
    {
        m_window = window;
    }
    xcb_window_t window() const
    {
        return m_window;
    }

private:
    xcb_timestamp_t m_timestamp = XCB_CURRENT_TIME;
    Selection *m_selection;
    xcb_window_t m_window;

    Q_DISABLE_COPY(SelectionSource)
};

/**
 * Representing a Wayland native data source.
 */
class WlSource : public SelectionSource
{
    Q_OBJECT

public:
    WlSource(AbstractDataSource *dataSource, Selection *selection);

    bool handleSelectionRequest(xcb_selection_request_event_t *event);
    void sendTargets(xcb_selection_request_event_t *event);
    void sendTimestamp(xcb_selection_request_event_t *event);

    void receiveOffer(const QString &mime);
    void sendSelectionNotify(xcb_selection_request_event_t *event, bool success);

Q_SIGNALS:
    void transferReady(xcb_selection_request_event_t *event, qint32 fd);

private:
    bool checkStartTransfer(xcb_selection_request_event_t *event);

    AbstractDataSource *m_dsi = nullptr;
    QStringList m_offers;

    Q_DISABLE_COPY(WlSource)
};

using Mimes = QList<QPair<QString, xcb_atom_t>>;

/**
 * Representing an X data source.
 */
class X11Source : public SelectionSource
{
    Q_OBJECT

public:
    X11Source(Selection *selection, xcb_xfixes_selection_notify_event_t *event);
    ~X11Source() override;

    void getTargets();

    Mimes offers() const
    {
        return m_offers;
    }
    void setOffers(const Mimes &offers);

    XwlDataSource *dataSource() const
    {
        return m_dataSource.get();
    }

    bool handleSelectionNotify(xcb_selection_notify_event_t *event);

    void setRequestor(xcb_window_t window)
    {
        setWindow(window);
    }

    void startTransfer(const QString &mimeName, qint32 fd);
Q_SIGNALS:
    void offersChanged(const QStringList &added, const QStringList &removed);
    void transferReady(xcb_atom_t target, qint32 fd);

private:
    void handleTargets();

    Mimes m_offers;
    std::unique_ptr<XwlDataSource> m_dataSource;

    Q_DISABLE_COPY(X11Source)
};

} // namespace Xwl
} // namespace KWin
