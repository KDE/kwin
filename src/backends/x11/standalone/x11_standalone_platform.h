/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_X11_PLATFORM_H
#define KWIN_X11_PLATFORM_H
#include "core/platform.h"

#include <kwin_export.h>

#include <QObject>

#include <memory>

#include <X11/Xlib-xcb.h>
#include <fixx11h.h>

namespace KWin
{
class RenderLoop;
class XInputIntegration;
class WindowSelector;
class X11EventFilter;
class X11Output;
class X11Keyboard;

class KWIN_EXPORT X11StandalonePlatform : public Platform
{
    Q_OBJECT

public:
    X11StandalonePlatform(QObject *parent = nullptr);
    ~X11StandalonePlatform() override;
    bool initialize() override;

    std::unique_ptr<OpenGLBackend> createOpenGLBackend() override;
    std::unique_ptr<Edge> createScreenEdge(ScreenEdges *parent) override;
    void createPlatformCursor(QObject *parent = nullptr) override;
    void startInteractiveWindowSelection(std::function<void(KWin::Window *)> callback, const QByteArray &cursorName = QByteArray()) override;
    void startInteractivePositionSelection(std::function<void(const QPoint &)> callback) override;

    PlatformCursorImage cursorImage() const override;

    std::unique_ptr<OverlayWindow> createOverlayWindow() override;
    std::unique_ptr<OutlineVisual> createOutline(Outline *outline) override;
    void createEffectsHandler(Compositor *compositor, Scene *scene) override;
    QVector<CompositingType> supportedCompositors() const override;

    void initOutputs();
    void scheduleUpdateOutputs();
    void updateOutputs();

    X11Keyboard *keyboard() const;
    RenderLoop *renderLoop() const;
    Outputs outputs() const override;

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
    template<typename T>
    void doUpdateOutputs();
    void updateRefreshRate();
    void updateCursor();

    std::unique_ptr<XInputIntegration> m_xinputIntegration;
    std::unique_ptr<QTimer> m_updateOutputsTimer;
    Display *m_x11Display;
    std::unique_ptr<WindowSelector> m_windowSelector;
    std::unique_ptr<X11EventFilter> m_screenEdgesFilter;
    std::unique_ptr<X11EventFilter> m_randrEventFilter;
    std::unique_ptr<X11Keyboard> m_keyboard;
    std::unique_ptr<RenderLoop> m_renderLoop;
    QVector<Output *> m_outputs;
};

}

#endif
