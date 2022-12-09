/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/outputlayer.h"
#include "qpainterbackend.h"

#include <QImage>
#include <QObject>
#include <QVector>

#include <memory>

#include <xcb/xcb.h>

namespace KWin
{

class X11WindowedBackend;
class X11WindowedOutput;

class X11WindowedQPainterPrimaryLayer : public OutputLayer
{
public:
    explicit X11WindowedQPainterPrimaryLayer(X11WindowedOutput *output);

    void ensureBuffer();

    std::optional<OutputLayerBeginFrameInfo> beginFrame() override;
    bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;

    QImage buffer;
    X11WindowedOutput *const m_output;
};

class X11WindowedQPainterCursorLayer : public OutputLayer
{
    Q_OBJECT

public:
    explicit X11WindowedQPainterCursorLayer(X11WindowedOutput *output);

    QPoint hotspot() const;
    void setHotspot(const QPoint &hotspot);

    QSize size() const;
    void setSize(const QSize &size);

    std::optional<OutputLayerBeginFrameInfo> beginFrame() override;
    bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;

private:
    QImage m_buffer;
    X11WindowedOutput *m_output;
    QPoint m_hotspot;
    QSize m_size;
};

class X11WindowedQPainterBackend : public QPainterBackend
{
    Q_OBJECT
public:
    X11WindowedQPainterBackend(X11WindowedBackend *backend);
    ~X11WindowedQPainterBackend() override;

    void present(Output *output) override;
    OutputLayer *primaryLayer(Output *output) override;
    X11WindowedQPainterCursorLayer *cursorLayer(Output *output);

private:
    void addOutput(Output *output);
    void removeOutput(Output *output);

    struct Layers
    {
        std::unique_ptr<X11WindowedQPainterPrimaryLayer> primaryLayer;
        std::unique_ptr<X11WindowedQPainterCursorLayer> cursorLayer;
    };

    xcb_gcontext_t m_gc = XCB_NONE;
    X11WindowedBackend *m_backend;
    std::map<Output *, Layers> m_outputs;
};

} // namespace KWin
