/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright 2019 Roman Gilg <subdiff@gmail.com>

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

    void init();

    xcb_window_t window() const {
        return m_window;
    }

    QPoint internalPosition() const;
    QPoint hostPosition() const {
        return m_hostPosition;
    }
    void setHostPosition(const QPoint &pos);

    void setWindowTitle(const QString &title);

    QSize pixelSize() const override;

    /**
     * @brief defines the geometry of the output
     * @param logicalPosition top left position of the output in compositor space
     * @param pixelSize output size as seen from the outside
     */
    void setGeometry(const QPoint &logicalPosition, const QSize &pixelSize);

private:
    void initXInputForWindow();

    xcb_window_t m_window = XCB_WINDOW_NONE;
    NETWinInfo *m_winInfo = nullptr;

    QPoint m_hostPosition;
    QSize m_pixelSize;

    X11WindowedBackend *m_backend;
};

}

#endif
