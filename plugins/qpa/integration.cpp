/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "integration.h"
#include "backingstore.h"
#include "offscreensurface.h"
#include "screen.h"
#include "sharingplatformcontext.h"
#include "window.h"
#include "../../main.h"
#include "../../platform.h"
#include "../../screens.h"
#include "../../virtualkeyboard.h"

#include <QCoreApplication>
#include <QtConcurrentRun>

#include <qpa/qplatforminputcontext.h>
#include <qpa/qplatforminputcontextfactory_p.h>
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
    , m_inputContext()
{
}

Integration::~Integration()
{
    qDeleteAll(m_screens);
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
    m_inputContext.reset(QPlatformInputContextFactory::create(QStringLiteral("qtvirtualkeyboard")));
    qunsetenv("QT_IM_MODULE");
    if (!m_inputContext.isNull()) {
        connect(qApp, &QGuiApplication::focusObjectChanged, this,
            [this] {
                if (VirtualKeyboard::self() && qApp->focusObject() != VirtualKeyboard::self()) {
                    m_inputContext->setFocusObject(VirtualKeyboard::self());
                }
            }
        );
        connect(kwinApp(), &Application::workspaceCreated, this,
            [this] {
                if (VirtualKeyboard::self()) {
                    m_inputContext->setFocusObject(VirtualKeyboard::self());
                }
            }
        );
        connect(qApp->inputMethod(), &QInputMethod::visibleChanged, this,
            [] {
                if (qApp->inputMethod()->isVisible()) {
                    if (QWindow *w = VirtualKeyboard::self()->inputPanel()) {
                        QWindowSystemInterface::handleWindowActivated(w, Qt::ActiveWindowFocusReason);
                    }
                }
            }
        );
    }
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
    if (kwinApp()->platform()->supportsQpaContext()) {
        return new SharingPlatformContext(context);
    }
    if (kwinApp()->platform()->sceneEglDisplay() != EGL_NO_DISPLAY) {
        auto s = kwinApp()->platform()->sceneEglSurface();
        if (s != EGL_NO_SURFACE) {
            // try a SharingPlatformContext with a created surface
            return new SharingPlatformContext(context, s, kwinApp()->platform()->sceneEglConfig());
        }
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

QPlatformInputContext *Integration::inputContext() const
{
    return m_inputContext.data();
}

}
}
