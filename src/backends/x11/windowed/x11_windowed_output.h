/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/output.h"
#include <kwin_export.h>

#include <QObject>
#include <QSize>
#include <QString>

#include <xcb/xcb.h>
#include <xcb/present.h>

class NETWinInfo;

namespace KWin
{

class X11WindowedBackend;
class X11WindowedOutput;
class X11WindowedEglBackend;
class X11WindowedQPainterBackend;

class X11WindowedCursor
{
public:
    explicit X11WindowedCursor(X11WindowedOutput *output);
    ~X11WindowedCursor();

    void update(const QImage &image, const QPoint &hotspot);

private:
    X11WindowedOutput *m_output;
    xcb_cursor_t m_handle = XCB_CURSOR_NONE;
};

/**
 * Wayland outputs in a nested X11 setup
 */
class KWIN_EXPORT X11WindowedOutput : public Output
{
    Q_OBJECT
public:
    explicit X11WindowedOutput(X11WindowedBackend *backend);
    ~X11WindowedOutput() override;

    RenderLoop *renderLoop() const override;

    void init(const QSize &pixelSize, qreal scale);
    void resize(const QSize &pixelSize);

    X11WindowedBackend *backend() const;
    X11WindowedCursor *cursor() const;
    xcb_window_t window() const;
    int depth() const;

    QPoint internalPosition() const;
    QPoint hostPosition() const;
    void setHostPosition(const QPoint &pos);

    void setWindowTitle(const QString &title);

    /**
     * Translates the global X11 screen coordinate @p pos to output coordinates.
     */
    QPointF mapFromGlobal(const QPointF &pos) const;

    bool setCursor(CursorSource *source) override;
    bool moveCursor(const QPoint &position) override;

    QRegion exposedArea() const;
    void addExposedArea(const QRect &rect);
    void clearExposedArea();

    void updateEnabled(bool enabled);

    void handlePresentCompleteNotify(xcb_present_complete_notify_event_t *event);

private:
    void initXInputForWindow();
    void renderCursorOpengl(X11WindowedEglBackend *backend, CursorSource *source);
    void renderCursorQPainter(X11WindowedQPainterBackend *backend, CursorSource *source);

    xcb_window_t m_window = XCB_WINDOW_NONE;
    xcb_present_event_t m_presentEvent = XCB_NONE;
    std::unique_ptr<NETWinInfo> m_winInfo;
    std::unique_ptr<RenderLoop> m_renderLoop;
    std::unique_ptr<X11WindowedCursor> m_cursor;
    QPoint m_hostPosition;
    QRegion m_exposedArea;

    X11WindowedBackend *m_backend;
};

} // namespace KWin
