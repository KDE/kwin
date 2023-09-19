/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2011 Arthur Arlt <a.arlt@stud.uni-heidelberg.de>
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "compositor.h"

namespace KWin
{

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
        ScriptSuspend = 1 << 2,
        AllReasonSuspend = 0xff
    };
    Q_DECLARE_FLAGS(SuspendReasons, SuspendReason)
    Q_ENUM(SuspendReason)
    Q_FLAG(SuspendReasons)

    static X11Compositor *create(QObject *parent = nullptr);
    ~X11Compositor() override;

    X11SyncManager *syncManager() const;

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

    void inhibit(Window *window) override;
    void uninhibit(Window *window) override;

    void toggleCompositing() override;
    void reinitialize() override;
    void configChanged() override;
    bool compositingPossible() const override;
    QString compositingNotPossibleReason() const override;
    bool openGLCompositingIsBroken() const override;
    void createOpenGLSafePoint(OpenGLSafePoint safePoint) override;

    /**
     * Checks whether @p w is the Scene's overlay window.
     */
    bool checkForOverlayWindow(WId w) const;

    /**
     * @returns Whether the Scene's Overlay X Window is visible.
     */
    bool isOverlayWindowVisible() const;

    static X11Compositor *self();

protected:
    void start() override;
    void stop() override;
    void composite(RenderLoop *renderLoop) override;

private:
    explicit X11Compositor(QObject *parent);

    std::unique_ptr<QThread> m_openGLFreezeProtectionThread;
    std::unique_ptr<QTimer> m_openGLFreezeProtection;
    std::unique_ptr<X11SyncManager> m_syncManager;
    /**
     * Whether the Compositor is currently suspended, 8 bits encoding the reason
     */
    SuspendReasons m_suspended;
    QSet<Window *> m_inhibitors;
    int m_framesToTestForSafety = 3;
};

} // namespace KWin
