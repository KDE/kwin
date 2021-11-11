/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_SCENE_QPAINTER_X11_BACKEND_H
#define KWIN_SCENE_QPAINTER_X11_BACKEND_H

#include "qpainterbackend.h"

#include <QObject>
#include <QImage>
#include <QVector>
#include <QMap>

#include <xcb/xcb.h>

namespace KWin
{

class X11WindowedBackend;

class X11WindowedQPainterBackend : public QPainterBackend
{
    Q_OBJECT
public:
    X11WindowedQPainterBackend(X11WindowedBackend *backend);
    ~X11WindowedQPainterBackend() override;

    QImage *bufferForScreen(AbstractOutput *output) override;
    QRegion beginFrame(AbstractOutput *output) override;
    void endFrame(AbstractOutput *output, const QRegion &renderedRegion, const QRegion &damagedRegion) override;

private:
    void createOutputs();
    xcb_gcontext_t m_gc = XCB_NONE;
    X11WindowedBackend *m_backend;
    struct Output {
        xcb_window_t window;
        QImage buffer;
    };
    QMap<AbstractOutput *, Output*> m_outputs;
};

}

#endif
