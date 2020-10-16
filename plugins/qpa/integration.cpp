/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "integration.h"
#include "backingstore.h"
#include "eglplatformcontext.h"
#include "logging.h"
#include "offscreensurface.h"
#include "screen.h"
#include "window.h"
#include "../../main.h"
#include "../../platform.h"
#include "../../screens.h"

#include <QCoreApplication>
#include <QtConcurrentRun>

#include <qpa/qplatformwindow.h>
#include <qpa/qwindowsysteminterface.h>

#include <QtEventDispatcherSupport/private/qunixeventdispatcher_qpa_p.h>
#include <QtFontDatabaseSupport/private/qgenericunixfontdatabase_p.h>
#include <QtThemeSupport/private/qgenericunixthemes_p.h>

namespace KWin
{

namespace QPA
{

Integration::Integration()
    : QObject()
    , QPlatformIntegration()
    , m_fontDb(new QGenericUnixFontDatabase())
{
}

Integration::~Integration()
{
    for (QPlatformScreen *platformScreen : m_screens) {
        QWindowSystemInterface::handleScreenRemoved(platformScreen);
    }
}

bool Integration::hasCapability(Capability cap) const
{
    switch (cap) {
    case ThreadedPixmaps:
        return true;
    case OpenGL:
        return true;
    case ThreadedOpenGL:
        return false;
    case BufferQueueingOpenGL:
        return false;
    case MultipleWindows:
    case NonFullScreenWindows:
        return true;
    case RasterGLSurface:
        return false;
    default:
        return QPlatformIntegration::hasCapability(cap);
    }
}

void Integration::initialize()
{
    connect(kwinApp(), &Application::screensCreated, this,
        [this] {
            connect(screens(), &Screens::changed, this, &Integration::initScreens);
            initScreens();
        }
    );
    QPlatformIntegration::initialize();
    auto dummyScreen = new Screen(-1);
    QWindowSystemInterface::handleScreenAdded(dummyScreen);
    m_screens << dummyScreen;
}

QAbstractEventDispatcher *Integration::createEventDispatcher() const
{
    return new QUnixEventDispatcherQPA;
}

QPlatformBackingStore *Integration::createPlatformBackingStore(QWindow *window) const
{
    return new BackingStore(window);
}

QPlatformWindow *Integration::createPlatformWindow(QWindow *window) const
{
    return new Window(window);
}

QPlatformOffscreenSurface *Integration::createPlatformOffscreenSurface(QOffscreenSurface *surface) const
{
    return new OffscreenSurface(surface);
}

QPlatformFontDatabase *Integration::fontDatabase() const
{
    return m_fontDb.data();
}

QPlatformTheme *Integration::createPlatformTheme(const QString &name) const
{
    return QGenericUnixTheme::createUnixTheme(name);
}

QStringList Integration::themeNames() const
{
    if (qEnvironmentVariableIsSet("KDE_FULL_SESSION")) {
        return QStringList({QStringLiteral("kde")});
    }
    return QStringList({QLatin1String(QGenericUnixTheme::name)});
}

QPlatformOpenGLContext *Integration::createPlatformOpenGLContext(QOpenGLContext *context) const
{
    if (kwinApp()->platform()->sceneEglGlobalShareContext() == EGL_NO_CONTEXT) {
        qCWarning(KWIN_QPA) << "Attempting to create a QOpenGLContext before the scene is initialized";
        return nullptr;
    }
    const EGLDisplay eglDisplay = kwinApp()->platform()->sceneEglDisplay();
    if (eglDisplay != EGL_NO_DISPLAY) {
        EGLPlatformContext *platformContext = new EGLPlatformContext(context, eglDisplay);
        return platformContext;
    }
    return nullptr;
}

void Integration::initScreens()
{
    QVector<Screen*> newScreens;
    newScreens.reserve(qMax(screens()->count(), 1));
    for (int i = 0; i < screens()->count(); i++) {
        auto screen = new Screen(i);
        QWindowSystemInterface::handleScreenAdded(screen);
        newScreens << screen;
    }
    if (newScreens.isEmpty()) {
        auto dummyScreen = new Screen(-1);
        QWindowSystemInterface::handleScreenAdded(dummyScreen);
        newScreens << dummyScreen;
    }
    while (!m_screens.isEmpty()) {
        QWindowSystemInterface::handleScreenRemoved(m_screens.takeLast());
    }
    m_screens = newScreens;
}

}
}
