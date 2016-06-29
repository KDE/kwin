/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_SCENE_QPAINTER_FB_BACKEND_H
#define KWIN_SCENE_QPAINTER_FB_BACKEND_H
#include "scene_qpainter.h"

#include <QObject>

namespace KWin
{
class FramebufferBackend;

class FramebufferQPainterBackend : public QObject, public QPainterBackend
{
    Q_OBJECT
public:
    FramebufferQPainterBackend(FramebufferBackend *backend);
    virtual ~FramebufferQPainterBackend();

    QImage *buffer() override;
    bool needsFullRepaint() const override;
    bool usesOverlayWindow() const override;
    void prepareRenderingFrame() override;
    void present(int mask, const QRegion &damage) override;

private:
    QImage m_renderBuffer;
    QImage m_backBuffer;
    FramebufferBackend *m_backend;
};

}

#endif
