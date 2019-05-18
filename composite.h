/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2011 Arthur Arlt <a.arlt@stud.uni-heidelberg.de>
Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

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
#pragma once

#include <kwinglobals.h>

#include <QObject>
#include <QElapsedTimer>
#include <QTimer>
#include <QBasicTimer>
#include <QRegion>

namespace KWin
{
class AbstractOutput;
class Client;
class CompositorSelectionOwner;
class Scene;

class KWIN_EXPORT Compositor : public QObject
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

    enum class State {
        On = 0,
        Off,
        Starting,
        Stopping
    };

    ~Compositor() override;

    // when adding repaints caused by a window, you probably want to use
    // either Toplevel::addRepaint() or Toplevel::addWorkspaceRepaint()
    void addRepaint(const QRect& r);
    void addRepaint(const QRegion& r);
    void addRepaint(int x, int y, int w, int h);
    void addRepaintFull();

    /**
     * Schedules a new repaint if no repaint is currently scheduled.
     **/
    void scheduleRepaint();

    /**
     * Notifies the compositor that SwapBuffers() is about to be called.
     * Rendering of the next frame will be deferred until bufferSwapComplete()
     * is called.
     **/
    void aboutToSwapBuffers();

    /**
     * Notifies the compositor that a pending buffer swap has completed.
     **/
    void bufferSwapComplete();

    /**
     * @brief Suspends the Compositor if it is currently active.
     *
     * Note: it is possible that the Compositor is not able to suspend. Use isActive to check
     * whether the Compositor has been suspended.
     *
     * @return void
     * @see resume
     * @see isActive
     **/
    Q_INVOKABLE void suspend(Compositor::SuspendReason reason);

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
     **/
    Q_INVOKABLE void resume(Compositor::SuspendReason reason);

    /**
     * Toggles compositing, that is if the Compositor is suspended it will be resumed
     * and if the Compositor is active it will be suspended.
     * Invoked by keybinding (shortcut default: Shift + Alt + F12).
     **/
    void toggleCompositing();

    /**
     * Re-initializes the Compositor completely.
     * Connected to the D-Bus signal org.kde.KWin /KWin reinitCompositing
     **/
    void reinitialize();

    /**
     * Whether the Compositor is active. That is a Scene is present and the Compositor is
     * not shutting down itself.
     **/
    bool isActive();
    int xrrRefreshRate() const {
        return m_xrrRefreshRate;
    }

    bool hasScene() const {
        return m_scene != NULL;
    }

    /**
     * Checks whether @p w is the Scene's overlay window.
     **/
    bool checkForOverlayWindow(WId w) const;
    /**
     * @returns Whether the Scene's Overlay X Window is visible.
     **/
    bool isOverlayWindowVisible() const;

    Scene *scene() {
        return m_scene;
    }

    /**
     * @brief Static check to test whether the Compositor is available and active.
     *
     * @return bool @c true if there is a Compositor and it is active, @c false otherwise
     **/
    static bool compositing() {
        return s_compositor != NULL && s_compositor->isActive();
    }

    void updateCompositeBlocking();
    void updateClientCompositeBlocking(KWin::Client* c);

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
    void timerEvent(QTimerEvent *te) override;

private:
    Q_INVOKABLE void start();
    void stop();

    void claimCompositorSelection();

    /**
     * Continues the startup after Scene And Workspace are created
     **/
    void startupWithWorkspace();
    void setupX11Support();

    void scheduleUsingSwapTimestamp(const AbstractOutput *output);
    void setCompositeTimer();
    void performCompositing();
    bool windowRepaintsPending() const;

    void releaseCompositorSelection();
    void deleteUnusedSupportProperties();

    AbstractOutput *mostRecentlyUpdatedOutput() const;

    void slotConfigChanged();

    State m_state;
    /**
     * Whether the Compositor is currently suspended, 8 bits encoding the reason
     **/
    SuspendReasons m_suspended;

    QBasicTimer compositeTimer;
    CompositorSelectionOwner *m_selectionOwner;
    QTimer m_releaseSelectionTimer;
    QList<xcb_atom_t> m_unusedSupportProperties;
    QTimer m_unusedSupportPropertyTimer;
    qint64 vBlankInterval, fpsInterval;
    int m_xrrRefreshRate;
    QRegion repaints_region;

    qint64 m_timeSinceLastVBlank;

    Scene *m_scene;

    bool m_bufferSwapPending;
    bool m_scheduleAtSwapCompletion;

    int m_framesToTestForSafety = 3;
    QElapsedTimer m_monotonicClock;

    KWIN_SINGLETON_VARIABLE(Compositor, s_compositor)
};

}
