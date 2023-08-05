/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QPointer>
#include <qpa/qplatformwindow.h>

namespace KWin
{

class InternalWindow;

namespace QPA
{

class Swapchain;

class Window : public QPlatformWindow
{
public:
    explicit Window(QWindow *window);
    ~Window() override;

    void invalidateSurface() override;
    QSurfaceFormat format() const override;
    void setVisible(bool visible) override;
    void setGeometry(const QRect &rect) override;
    WId winId() const override;
    qreal devicePixelRatio() const override;
    void requestActivateWindow() override;

    InternalWindow *internalWindow() const;
    Swapchain *swapchain();

private:
    void map();
    void unmap();

    QSurfaceFormat m_format;
    QPointer<InternalWindow> m_handle;
    std::unique_ptr<Swapchain> m_swapchain;
    quint32 m_windowId;
    qreal m_scale = 1;
};

}
}
