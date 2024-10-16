/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2011 Arthur Arlt <a.arlt@stud.uni-heidelberg.de>
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "compositor.h"
#include <QSet>

namespace KWin
{

class X11CompositorSelectionOwner;
class X11SyncManager;
class X11Window;

class KWIN_EXPORT X11Compositor final : public Compositor
{
    Q_OBJECT
public:
    enum SuspendReason {
        NoReasonSuspend = 0,
        UserSuspend = 1 << 0,
        BlockRuleSuspend = 1 << 1,
        AllReasonSuspend = 0xff
    };
    Q_DECLARE_FLAGS(SuspendReasons, SuspendReason)
    Q_ENUM(SuspendReason)
    Q_FLAG(SuspendReasons)

    static X11Compositor *create(QObject *parent = nullptr);
    ~X11Compositor() override;

    X11SyncManager *syncManager() const;

    void start() override;
    void stop() override;

    /**
     * Toggles compositing, that is if the Compositor is suspended it will be resumed
     * and if the Compositor is active it will be suspended.
     * Invoked by keybinding (shortcut default: Shift + Alt + F12).
     */
    void toggle();

    /**
     * @brief Suspends the Compositor if it is currently active.
     *
     * Note: it is possible that the Compositor is not able to suspend. Use isActive to check
     * whether the Compositor has been suspended.
     *
     * @return void
     * @see resume
     * @see isActive
     */
    void suspend(SuspendReason reason);

    /**
     * @brief Resumes the Compositor if it is currently suspended.
     *
     * Note: it is possible that the Compositor cannot be resumed, that is there might be Clients
     * blocking the usage of Compositing or the Scene might be broken. Use isActive to check
     * whether the Compositor has been resumed. Also check isCompositingPossible and
     * isOpenGLBroken.
     *
     * Note: The starting of the Compositor can require some time and is partially done threaded.
     * After this method returns the setup may not have been completed.
     *
     * @return void
     * @see suspend
     * @see isActive
     * @see isCompositingPossible
     * @see isOpenGLBroken
     */
    void resume(SuspendReason reason);

    enum class OpenGLSafePoint {
        PreInit,
        PostInit,
        PreFrame,
        PostFrame,
        PostLastGuardedFrame
    };
    /**
     * This method is invoked before and after creating the OpenGL rendering Scene.
     * An implementing Platform can use it to detect crashes triggered by the OpenGL implementation.
     * This can be used for openGLCompositingIsBroken.
     *
     * The default implementation does nothing.
     * @see openGLCompositingIsBroken.
     */
    void createOpenGLSafePoint(OpenGLSafePoint safePoint);

    void inhibit(Window *window) override;
    void uninhibit(Window *window) override;

    void reinitialize() override;
    bool compositingPossible() const override;
    QString compositingNotPossibleReason() const override;
    bool openGLCompositingIsBroken() const override;

    static X11Compositor *self();

protected:
    void composite(RenderLoop *renderLoop) override;

private:
    explicit X11Compositor(QObject *parent);

    bool attemptOpenGLCompositing();

    void releaseCompositorSelection();
    void destroyCompositorSelection();

    std::unique_ptr<QThread> m_openGLFreezeProtectionThread;
    std::unique_ptr<QTimer> m_openGLFreezeProtection;
    std::unique_ptr<X11SyncManager> m_syncManager;
    std::unique_ptr<X11CompositorSelectionOwner> m_selectionOwner;
    QTimer m_releaseSelectionTimer;
    /**
     * Whether the Compositor is currently suspended, 8 bits encoding the reason
     */
    SuspendReasons m_suspended;
    QSet<Window *> m_inhibitors;
    int m_framesToTestForSafety = 3;
};

} // namespace KWin
