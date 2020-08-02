/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2011 Arthur Arlt <a.arlt@stud.uni-heidelberg.de>
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <kwinglobals.h>

#include <QObject>
#include <QElapsedTimer>
#include <QTimer>
#include <QBasicTimer>
#include <QRegion>

namespace KWin
{
class CompositorSelectionOwner;
class Scene;
class X11Client;

class KWIN_EXPORT Compositor : public QObject
{
    Q_OBJECT
public:
    enum class State {
        On = 0,
        Off,
        Starting,
        Stopping
    };

    ~Compositor() override;
    static Compositor *self();

    // when adding repaints caused by a window, you probably want to use
    // either Toplevel::addRepaint() or Toplevel::addWorkspaceRepaint()
    void addRepaint(const QRect& r);
    void addRepaint(const QRegion& r);
    void addRepaint(int x, int y, int w, int h);
    void addRepaintFull();

    /**
     * Schedules a new repaint if no repaint is currently scheduled.
     */
    void scheduleRepaint();

    /**
     * Notifies the compositor that SwapBuffers() is about to be called.
     * Rendering of the next frame will be deferred until bufferSwapComplete()
     * is called.
     */
    void aboutToSwapBuffers();

    /**
     * Notifies the compositor that a pending buffer swap has completed.
     */
    void bufferSwapComplete();

    /**
     * Toggles compositing, that is if the Compositor is suspended it will be resumed
     * and if the Compositor is active it will be suspended.
     * Invoked by keybinding (shortcut default: Shift + Alt + F12).
     */
    virtual void toggleCompositing() = 0;

    /**
     * Re-initializes the Compositor completely.
     * Connected to the D-Bus signal org.kde.KWin /KWin reinitCompositing
     */
    virtual void reinitialize();

    /**
     * Whether the Compositor is active. That is a Scene is present and the Compositor is
     * not shutting down itself.
     */
    bool isActive();
    virtual int refreshRate() const = 0;

    Scene *scene() const {
        return m_scene;
    }

    /**
     * @brief Static check to test whether the Compositor is available and active.
     *
     * @return bool @c true if there is a Compositor and it is active, @c false otherwise
     */
    static bool compositing() {
        return s_compositor != nullptr && s_compositor->isActive();
    }

    // for delayed supportproperty management of effects
    void keepSupportProperty(xcb_atom_t atom);
    void removeSupportProperty(xcb_atom_t atom);

Q_SIGNALS:
    void compositingToggled(bool active);
    void aboutToDestroy();
    void aboutToToggleCompositing();
    void sceneCreated();
    void bufferSwapCompleted();

protected:
    explicit Compositor(QObject *parent = nullptr);
    void timerEvent(QTimerEvent *te) override;

    virtual void start() = 0;
    void stop();

    /**
     * @brief Prepares start.
     * @return bool @c true if start should be continued and @c if not.
     */
    bool setupStart();
    /**
     * Continues the startup after Scene And Workspace are created
     */
    void startupWithWorkspace();
    virtual void performCompositing();

    virtual void configChanged();

    void destroyCompositorSelection();

    static Compositor *s_compositor;

private:
    void initializeX11();
    void cleanupX11();

    void setCompositeTimer();
    bool windowRepaintsPending() const;

    void releaseCompositorSelection();
    void deleteUnusedSupportProperties();

    State m_state;

    QBasicTimer compositeTimer;
    CompositorSelectionOwner *m_selectionOwner;
    QTimer m_releaseSelectionTimer;
    QList<xcb_atom_t> m_unusedSupportProperties;
    QTimer m_unusedSupportPropertyTimer;
    qint64 vBlankInterval, fpsInterval;
    QRegion repaints_region;

    qint64 m_timeSinceLastVBlank;

    Scene *m_scene;

    bool m_bufferSwapPending;
    bool m_composeAtSwapCompletion;

    int m_framesToTestForSafety = 3;
    QElapsedTimer m_monotonicClock;
};

class KWIN_EXPORT WaylandCompositor : public Compositor
{
    Q_OBJECT
public:
    static WaylandCompositor *create(QObject *parent = nullptr);

    int refreshRate() const override;

    void toggleCompositing() override;

protected:
    void start() override;

private:
    explicit WaylandCompositor(QObject *parent);
};

class KWIN_EXPORT X11Compositor : public Compositor
{
    Q_OBJECT
public:
    enum SuspendReason {
        NoReasonSuspend     = 0,
        UserSuspend         = 1 << 0,
        BlockRuleSuspend    = 1 << 1,
        ScriptSuspend       = 1 << 2,
        AllReasonSuspend    = 0xff
    };
    Q_DECLARE_FLAGS(SuspendReasons, SuspendReason)
    Q_ENUM(SuspendReason)
    Q_FLAG(SuspendReasons)

    static X11Compositor *create(QObject *parent = nullptr);

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

    void toggleCompositing() override;
    void reinitialize() override;

    void configChanged() override;

    /**
     * Checks whether @p w is the Scene's overlay window.
     */
    bool checkForOverlayWindow(WId w) const;

    /**
     * @returns Whether the Scene's Overlay X Window is visible.
     */
    bool isOverlayWindowVisible() const;

    int refreshRate() const override;

    void updateClientCompositeBlocking(X11Client *client = nullptr);

    static X11Compositor *self();

protected:
    void start() override;
    void performCompositing() override;

private:
    explicit X11Compositor(QObject *parent);
    /**
     * Whether the Compositor is currently suspended, 8 bits encoding the reason
     */
    SuspendReasons m_suspended;

    int m_xrrRefreshRate;
};

}
