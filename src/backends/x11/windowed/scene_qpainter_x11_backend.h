/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_SCENE_QPAINTER_X11_BACKEND_H
#define KWIN_SCENE_QPAINTER_X11_BACKEND_H

#include "outputlayer.h"
#include "qpainterbackend.h"

#include <QImage>
#include <QMap>
#include <QObject>
#include <QVector>

#include <xcb/xcb.h>

namespace KWin
{

class X11WindowedBackend;

class X11WindowedQPainterOutput : public OutputLayer
{
public:
    X11WindowedQPainterOutput(Output *output, xcb_window_t window);

    OutputLayerBeginFrameInfo beginFrame() override;
    bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;

    xcb_window_t window;
    QImage buffer;
    Output *const m_output;
};

class X11WindowedQPainterBackend : public QPainterBackend
{
    Q_OBJECT
public:
    X11WindowedQPainterBackend(X11WindowedBackend *backend);
    ~X11WindowedQPainterBackend() override;

    void present(Output *output) override;
    OutputLayer *primaryLayer(Output *output) override;

private:
    void createOutputs();
    xcb_gcontext_t m_gc = XCB_NONE;
    X11WindowedBackend *m_backend;
    QMap<Output *, QSharedPointer<X11WindowedQPainterOutput>> m_outputs;
};

}

#endif
