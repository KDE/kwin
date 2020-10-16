/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_X11WINDOWED_OUTPUT_H
#define KWIN_X11WINDOWED_OUTPUT_H

#include "abstract_wayland_output.h"
#include <kwin_export.h>

#include <QObject>
#include <QSize>
#include <QString>

#include <xcb/xcb.h>

class NETWinInfo;

namespace KWin
{
class X11WindowedBackend;

/**
 * Wayland outputs in a nested X11 setup
 */
class KWIN_EXPORT X11WindowedOutput : public AbstractWaylandOutput
{
    Q_OBJECT
public:
    explicit X11WindowedOutput(X11WindowedBackend *backend);
    ~X11WindowedOutput() override;

    void init(const QPoint &logicalPosition, const QSize &pixelSize);

    xcb_window_t window() const {
        return m_window;
    }

    QPoint internalPosition() const;
    QPoint hostPosition() const {
        return m_hostPosition;
    }
    void setHostPosition(const QPoint &pos);

    void setWindowTitle(const QString &title);

    /**
     * @brief defines the geometry of the output
     * @param logicalPosition top left position of the output in compositor space
     * @param pixelSize output size as seen from the outside
     */
    void setGeometry(const QPoint &logicalPosition, const QSize &pixelSize);

    /**
     * Translates the global X11 screen coordinate @p pos to output coordinates.
     */
    QPointF mapFromGlobal(const QPointF &pos) const;

private:
    void initXInputForWindow();

    xcb_window_t m_window = XCB_WINDOW_NONE;
    NETWinInfo *m_winInfo = nullptr;

    QPoint m_hostPosition;

    X11WindowedBackend *m_backend;
};

}

#endif
