/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_X11STANDALONE_PLATFORM_H
#define KWIN_X11STANDALONE_PLATFORM_H
#include "platform.h"

#include <kwin_export.h>

#include <QObject>

namespace KWin
{
class XInputIntegration;
class WindowSelector;
class X11EventFilter;

class KWIN_EXPORT X11StandalonePlatform : public Platform
{
    Q_OBJECT
    Q_INTERFACES(KWin::Platform)
    Q_PLUGIN_METADATA(IID "org.kde.kwin.Platform" FILE "x11.json")
public:
    X11StandalonePlatform(QObject *parent = nullptr);
    virtual ~X11StandalonePlatform();
    void init() override;

    Screens *createScreens(QObject *parent = nullptr) override;
    OpenGLBackend *createOpenGLBackend() override;
    Edge *createScreenEdge(ScreenEdges *parent) override;
    void createPlatformCursor(QObject *parent = nullptr) override;
    bool requiresCompositing() const override;
    bool compositingPossible() const override;
    QString compositingNotPossibleReason() const override;
    bool openGLCompositingIsBroken() const override;
    void createOpenGLSafePoint(OpenGLSafePoint safePoint) override;
    void startInteractiveWindowSelection(std::function<void (KWin::Toplevel *)> callback, const QByteArray &cursorName = QByteArray()) override;

    PlatformCursorImage cursorImage() const override;

    void setupActionForGlobalAccel(QAction *action) override;

    OverlayWindow *createOverlayWindow() override;

    void updateXTime() override;
    OutlineVisual *createOutline(Outline *outline) override;
    Decoration::Renderer *createDecorationRenderer(Decoration::DecoratedClientImpl *client) override;

    void invertScreen() override;

protected:
    void doHideCursor() override;
    void doShowCursor() override;

private:
    /**
     * Tests whether GLX is supported and returns @c true
     * in case KWin is compiled with OpenGL support and GLX
     * is available.
     *
     * If KWin is compiled with OpenGL ES or without OpenGL at
     * all, @c false is returned.
     * @returns @c true if GLX is available, @c false otherwise and if not build with OpenGL support.
     **/
    static bool hasGlx();

    XInputIntegration *m_xinputIntegration = nullptr;
    QThread *m_openGLFreezeProtectionThread = nullptr;
    QTimer *m_openGLFreezeProtection = nullptr;
    Display *m_x11Display;
    QScopedPointer<WindowSelector> m_windowSelector;
    QScopedPointer<X11EventFilter> m_screenEdgesFilter;

};

}

#endif
