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

#include <unordered_map>

#include <xcb/xcb.h>
#include <xcb/present.h>

class NETWinInfo;

namespace KWin
{

class GraphicsBuffer;
class X11WindowedBackend;
class X11WindowedOutput;

struct DmaBufAttributes;
struct ShmAttributes;

class X11WindowedBuffer : public QObject
{
    Q_OBJECT

public:
    X11WindowedBuffer(X11WindowedOutput *output, xcb_pixmap_t pixmap, GraphicsBuffer *buffer);
    ~X11WindowedBuffer() override;

    GraphicsBuffer *buffer() const;
    xcb_pixmap_t pixmap() const;

    void lock();
    void unlock();

Q_SIGNALS:
    void defunct();

private:
    X11WindowedOutput *m_output;
    GraphicsBuffer *m_buffer;
    xcb_pixmap_t m_pixmap;
    bool m_locked = false;
};

class X11WindowedCursor
{
public:
    explicit X11WindowedCursor(X11WindowedOutput *output);
    ~X11WindowedCursor();

    void update(const QImage &image, const QPointF &hotspot);

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

    xcb_pixmap_t importBuffer(GraphicsBuffer *buffer);

    QPoint internalPosition() const;
    QPoint hostPosition() const;
    void setHostPosition(const QPoint &pos);

    void setWindowTitle(const QString &title);

    /**
     * Translates the global X11 screen coordinate @p pos to output coordinates.
     */
    QPointF mapFromGlobal(const QPointF &pos) const;

    bool setCursor(CursorSource *source) override;
    bool moveCursor(const QPointF &position) override;

    QRegion exposedArea() const;
    void addExposedArea(const QRect &rect);
    void clearExposedArea();

    void updateEnabled(bool enabled);

    void handlePresentCompleteNotify(xcb_present_complete_notify_event_t *event);
    void handlePresentIdleNotify(xcb_present_idle_notify_event_t *event);

private:
    void initXInputForWindow();

    xcb_pixmap_t importDmaBufBuffer(const DmaBufAttributes *attributes);
    xcb_pixmap_t importShmBuffer(const ShmAttributes *attributes);

    xcb_window_t m_window = XCB_WINDOW_NONE;
    xcb_present_event_t m_presentEvent = XCB_NONE;
    std::unique_ptr<NETWinInfo> m_winInfo;
    std::unique_ptr<RenderLoop> m_renderLoop;
    std::unique_ptr<X11WindowedCursor> m_cursor;
    std::unordered_map<GraphicsBuffer *, std::unique_ptr<X11WindowedBuffer>> m_buffers;
    QPoint m_hostPosition;
    QRegion m_exposedArea;

    X11WindowedBackend *m_backend;
};

} // namespace KWin
