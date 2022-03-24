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

class X11WindowedQPainterLayer : public OutputLayer
{
public:
    X11WindowedQPainterLayer(AbstractOutput *output, X11WindowedBackend *backend);
    ~X11WindowedQPainterLayer();

    QImage *image() override;
    QRegion beginFrame() override;
    void endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;

    xcb_window_t window() const;

private:
    AbstractOutput *const m_output;
    X11WindowedBackend *const m_backend;

    xcb_window_t m_window;
    QImage m_buffer;
};

class X11WindowedQPainterBackend : public QPainterBackend
{
    Q_OBJECT
public:
    X11WindowedQPainterBackend(X11WindowedBackend *backend);
    ~X11WindowedQPainterBackend();

    void present(AbstractOutput *output) override;
    OutputLayer *getLayer(RenderOutput *output) override;

private:
    void createOutputs();
    xcb_gcontext_t m_gc = XCB_NONE;
    X11WindowedBackend *m_backend;
    QMap<AbstractOutput *, QSharedPointer<X11WindowedQPainterLayer>> m_outputs;
};

}

#endif
