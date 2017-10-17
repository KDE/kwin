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
#ifndef KWIN_QPA_INTEGRATION_H
#define KWIN_QPA_INTEGRATION_H

#include <epoxy/egl.h>
#include <fixx11h.h>
#include <qpa/qplatformintegration.h>
#include <QObject>

namespace KWayland
{
namespace Client
{
class Registry;
class Compositor;
class Shell;
}
}

namespace KWin
{
namespace QPA
{

class Screen;

class Integration : public QObject, public QPlatformIntegration
{
    Q_OBJECT
public:
    explicit Integration();
    virtual ~Integration();

    bool hasCapability(Capability cap) const override;
    QPlatformWindow *createPlatformWindow(QWindow *window) const override;
    QPlatformBackingStore *createPlatformBackingStore(QWindow *window) const override;
    QAbstractEventDispatcher *createEventDispatcher() const override;
    QPlatformFontDatabase *fontDatabase() const override;
    QStringList themeNames() const override;
    QPlatformTheme *createPlatformTheme(const QString &name) const override;
    QPlatformNativeInterface *nativeInterface() const override;
    QPlatformOpenGLContext *createPlatformOpenGLContext(QOpenGLContext *context) const override;

    void initialize() override;
    QPlatformInputContext *inputContext() const override;

    KWayland::Client::Compositor *compositor() const;
    EGLDisplay eglDisplay() const;

private:
    void initScreens();
    void initEgl();
    KWayland::Client::Shell *shell() const;

    QPlatformFontDatabase *m_fontDb;
    QPlatformNativeInterface *m_nativeInterface;
    KWayland::Client::Compositor *m_compositor = nullptr;
    KWayland::Client::Shell *m_shell = nullptr;
    EGLDisplay m_eglDisplay = EGL_NO_DISPLAY;
    Screen *m_dummyScreen = nullptr;
    QScopedPointer<QPlatformInputContext> m_inputContext;
    QVector<Screen*> m_screens;
};

}
}

#endif
