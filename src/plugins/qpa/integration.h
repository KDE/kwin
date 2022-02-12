/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_QPA_INTEGRATION_H
#define KWIN_QPA_INTEGRATION_H

#include <epoxy/egl.h>

#include <qpa/qplatformintegration.h>
#include <QtServiceSupport/private/qgenericunixservices_p.h>
#include <QObject>

namespace KWin
{

class AbstractOutput;

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
    QPlatformServices *services() const override;
    void initialize() override;

    QHash<AbstractOutput *, Screen *> screens() const;

private Q_SLOTS:
    void handleOutputEnabled(AbstractOutput *output);
    void handleOutputDisabled(AbstractOutput *output);
    void handlePlatformCreated();

private:
    QScopedPointer<QPlatformFontDatabase> m_fontDb;
    QPlatformNativeInterface *m_nativeInterface;
    QPlatformPlaceholderScreen *m_dummyScreen = nullptr;
    QHash<AbstractOutput *, Screen *> m_screens;
    QScopedPointer<QGenericUnixServices> m_services;
};

}
}

#endif
