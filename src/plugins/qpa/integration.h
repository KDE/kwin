/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <epoxy/egl.h>

#include <QObject>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QtServiceSupport/private/qgenericunixservices_p.h>
#else
#include <QtGui/private/qgenericunixservices_p.h>
#endif
#include <qpa/qplatformintegration.h>

namespace KWin
{

class Output;

namespace QPA
{

class Screen;

class Integration : public QObject, public QPlatformIntegration
{
    Q_OBJECT
public:
    explicit Integration();
    ~Integration() override;

    bool hasCapability(Capability cap) const override;
    QPlatformWindow *createPlatformWindow(QWindow *window) const override;
    QPlatformOffscreenSurface *createPlatformOffscreenSurface(QOffscreenSurface *surface) const override;
    QPlatformBackingStore *createPlatformBackingStore(QWindow *window) const override;
    QAbstractEventDispatcher *createEventDispatcher() const override;
    QPlatformFontDatabase *fontDatabase() const override;
    QStringList themeNames() const override;
    QPlatformTheme *createPlatformTheme(const QString &name) const override;
    QPlatformOpenGLContext *createPlatformOpenGLContext(QOpenGLContext *context) const override;
    QPlatformAccessibility *accessibility() const override;
    QPlatformNativeInterface *nativeInterface() const override;
    QPlatformServices *services() const override;
    void initialize() override;

    QHash<Output *, Screen *> screens() const;

private Q_SLOTS:
    void handleOutputEnabled(Output *output);
    void handleOutputDisabled(Output *output);
    void handleWorkspaceCreated();

private:
    std::unique_ptr<QPlatformFontDatabase> m_fontDb;
    mutable std::unique_ptr<QPlatformAccessibility> m_accessibility;
    std::unique_ptr<QPlatformNativeInterface> m_nativeInterface;
    QPlatformPlaceholderScreen *m_dummyScreen = nullptr;
    QHash<Output *, Screen *> m_screens;
    std::unique_ptr<QGenericUnixServices> m_services;
};

}
}
