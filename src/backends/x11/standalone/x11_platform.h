/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_X11_PLATFORM_H
#define KWIN_X11_PLATFORM_H
#include "platform.h"

#include <kwin_export.h>

#include <QObject>

#include <memory>

#include <X11/Xlib-xcb.h>
#include <fixx11h.h>

namespace KWin
{
class XInputIntegration;
class WindowSelector;
class X11EventFilter;
class X11Output;

class KWIN_EXPORT X11StandalonePlatform : public Platform
{
    Q_OBJECT
    Q_INTERFACES(KWin::Platform)
    Q_PLUGIN_METADATA(IID "org.kde.kwin.Platform" FILE "x11.json")
public:
    X11StandalonePlatform(QObject *parent = nullptr);
    ~X11StandalonePlatform() override;
    bool initialize() override;
    Session *session() const override;

    OpenGLBackend *createOpenGLBackend() override;
    Edge *createScreenEdge(ScreenEdges *parent) override;
    void createPlatformCursor(QObject *parent = nullptr) override;
    bool requiresCompositing() const override;
    bool compositingPossible() const override;
    QString compositingNotPossibleReason() const override;
    bool openGLCompositingIsBroken() const override;
    void createOpenGLSafePoint(OpenGLSafePoint safePoint) override;
    void startInteractiveWindowSelection(std::function<void (KWin::Toplevel *)> callback, const QByteArray &cursorName = QByteArray()) override;
    void startInteractivePositionSelection(std::function<void (const QPoint &)> callback) override;

    PlatformCursorImage cursorImage() const override;

    void setupActionForGlobalAccel(QAction *action) override;

    OverlayWindow *createOverlayWindow() override;
    OutlineVisual *createOutline(Outline *outline) override;

    void invertScreen() override;

    void createEffectsHandler(Compositor *compositor, Scene *scene) override;
    QVector<CompositingType> supportedCompositors() const override;

    void initOutputs();
    void scheduleUpdateOutputs();
    void updateOutputs();

    RenderLoop *renderLoop() const override;
    Outputs outputs() const override;
    Outputs enabledOutputs() const override;

private:
    /**
     * Tests whether GLX is supported and returns @c true
     * in case KWin is compiled with OpenGL support and GLX
     * is available.
     *
     * If KWin is compiled with OpenGL ES or without OpenGL at
     * all, @c false is returned.
     * @returns @c true if GLX is available, @c false otherwise and if not build with OpenGL support.
     */
    static bool hasGlx();

    X11Output *findX11Output(const QString &name) const;
    template <typename T>
    void doUpdateOutputs();
    void updateRefreshRate();
    void updateCursor();

    Session *m_session;
    XInputIntegration *m_xinputIntegration = nullptr;
    QThread *m_openGLFreezeProtectionThread = nullptr;
    QTimer *m_openGLFreezeProtection = nullptr;
    QTimer *m_updateOutputsTimer = nullptr;
    Display *m_x11Display;
    QScopedPointer<WindowSelector> m_windowSelector;
    QScopedPointer<X11EventFilter> m_screenEdgesFilter;
    QScopedPointer<X11EventFilter> m_randrEventFilter;
    RenderLoop *m_renderLoop;
    QVector<AbstractOutput *> m_outputs;
};

}

#endif
