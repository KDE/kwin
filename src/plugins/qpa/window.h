/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <epoxy/egl.h>

#include <QPointer>
#include <qpa/qplatformwindow.h>

class QOpenGLFramebufferObject;

namespace KWin
{

class InternalWindow;

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
    void requestActivateWindow() override;

    void bindContentFBO();
    const std::shared_ptr<QOpenGLFramebufferObject> &contentFBO() const;
    std::shared_ptr<QOpenGLFramebufferObject> swapFBO();

    InternalWindow *internalWindow() const;
    EGLSurface eglSurface() const;

private:
    void createFBO();
    void map();
    void unmap();

    QSurfaceFormat m_format;
    QPointer<InternalWindow> m_handle;
    std::shared_ptr<QOpenGLFramebufferObject> m_contentFBO;
    EGLDisplay m_eglDisplay = EGL_NO_DISPLAY;
    quint32 m_windowId;
    bool m_resized = false;
    qreal m_scale = 1;
};

}
}
