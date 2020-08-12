/*
    KWin - the KDE window manager

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_QPA_WINDOW_H
#define KWIN_QPA_WINDOW_H

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

    void setVisible(bool visible) override;
    void setGeometry(const QRect &rect) override;
    WId winId() const override;
    qreal devicePixelRatio() const override;

    void bindContentFBO();
    const QSharedPointer<QOpenGLFramebufferObject> &contentFBO() const;
    QSharedPointer<QOpenGLFramebufferObject> swapFBO();

    InternalClient *client() const;

private:
    void createFBO();
    void map();
    void unmap();

    InternalClient *m_handle = nullptr;
    QSharedPointer<QOpenGLFramebufferObject> m_contentFBO;
    quint32 m_windowId;
    bool m_resized = false;
    int m_scale = 1;
};

}
}

#endif
