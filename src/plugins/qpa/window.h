/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_QPA_WINDOW_H
#define KWIN_QPA_WINDOW_H

#include <epoxy/egl.h>

#include <QPointer>
#include <qpa/qplatformwindow.h>

namespace KWin
{

class ClientBufferInternal;
class InternalClient;

namespace QPA
{
class Swapchain;

class Window : public QPlatformWindow
{
public:
    explicit Window(QWindow *window);
    ~Window() override;

    QSurfaceFormat format() const override;
    void setVisible(bool visible) override;
    void setGeometry(const QRect &rect) override;
    WId winId() const override;
    qreal devicePixelRatio() const override;

    ClientBufferInternal *backbuffer() const;
    ClientBufferInternal *swap();

    InternalClient *client() const;
    EGLSurface eglSurface() const;

private:
    void createFBO();
    void createPbuffer();
    void map();
    void unmap();

    QSurfaceFormat m_format;
    QPointer<InternalClient> m_handle;
    QPointer<ClientBufferInternal> m_backBuffer;
    QScopedPointer<Swapchain> m_swapchain;
    EGLDisplay m_eglDisplay = EGL_NO_DISPLAY;
    EGLSurface m_eglSurface = EGL_NO_SURFACE;
    quint32 m_windowId;
    qreal m_scale = 1;
};

}
}

#endif
