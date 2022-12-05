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

class X11WindowedQPainterOutput : public OutputLayer
{
public:
    X11WindowedQPainterOutput(X11WindowedOutput *output, xcb_window_t window);

    void ensureBuffer();

    std::optional<OutputLayerBeginFrameInfo> beginFrame() override;
    bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;

    xcb_window_t window;
    QImage buffer;
    X11WindowedOutput *const m_output;
};

class X11WindowedQPainterBackend : public QPainterBackend
{
    Q_OBJECT
public:
    X11WindowedQPainterBackend(X11WindowedBackend *backend);
    ~X11WindowedQPainterBackend() override;

    void present(Output *output) override;
    OutputLayer *primaryLayer(Output *output) override;
    OutputLayer *cursorLayer(Output *output) override;

private:
    void addOutput(Output *output);
    void removeOutput(Output *output);

    xcb_gcontext_t m_gc = XCB_NONE;
    X11WindowedBackend *m_backend;
    std::map<Output *, std::unique_ptr<X11WindowedQPainterOutput>> m_outputs;
};

} // namespace KWin
