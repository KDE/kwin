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
#ifndef KWIN_QPA_WINDOW_H
#define KWIN_QPA_WINDOW_H

#include <epoxy/egl.h>
#include "fixqopengl.h"

#include <fixx11h.h>
#include <qpa/qplatformwindow.h>

class QOpenGLFramebufferObject;


namespace KWayland
{
namespace Client
{
class Surface;
class ShellSurface;
}
}

namespace KWin
{

class ShellClient;

namespace QPA
{

class Integration;

class Window : public QPlatformWindow
{
public:
    explicit Window(QWindow *window, KWayland::Client::Surface *surface, KWayland::Client::ShellSurface *shellSurface, const Integration *integration);
    ~Window() override;

    void setVisible(bool visible) override;
    void setGeometry(const QRect &rect) override;
    WId winId() const override;

    KWayland::Client::Surface *surface() const {
        return m_surface;
    }

    int scale() const;
    qreal devicePixelRatio() const override;

    void bindContentFBO();
    const QSharedPointer<QOpenGLFramebufferObject> &contentFBO() const {
        return m_contentFBO;
    }
    QSharedPointer<QOpenGLFramebufferObject> swapFBO();
    ShellClient *shellClient();

private:
    void unmap();
    void createFBO();

    KWayland::Client::Surface *m_surface;
    KWayland::Client::ShellSurface *m_shellSurface;
    QSharedPointer<QOpenGLFramebufferObject> m_contentFBO;
    bool m_resized = false;
    ShellClient *m_shellClient = nullptr;
    quint32 m_windowId;
    const Integration *m_integration;
    int m_scale = 1;
};

}
}

#endif
