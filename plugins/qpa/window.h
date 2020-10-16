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
#include "fixqopengl.h"
#include <fixx11h.h>

#include <QPointer>
#include <qpa/qplatformwindow.h>

class QOpenGLFramebufferObject;

namespace KWin
{

class InternalClient;

namespace QPA
{

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

    void bindContentFBO();
    const QSharedPointer<QOpenGLFramebufferObject> &contentFBO() const;
    QSharedPointer<QOpenGLFramebufferObject> swapFBO();

    InternalClient *client() const;
    EGLSurface eglSurface() const;

private:
    void createFBO();
    void createPbuffer();
    void map();
    void unmap();

    QSurfaceFormat m_format;
    QPointer<InternalClient> m_handle;
    QSharedPointer<QOpenGLFramebufferObject> m_contentFBO;
    EGLDisplay m_eglDisplay = EGL_NO_DISPLAY;
    EGLSurface m_eglSurface = EGL_NO_SURFACE;
    quint32 m_windowId;
    bool m_resized = false;
    int m_scale = 1;
};

}
}

#endif
