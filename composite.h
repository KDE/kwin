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

#ifndef KWIN_COMPOSITE_H
#define KWIN_COMPOSITE_H
// KWin
#include <kwinglobals.h>
// KDE
#include <KSelectionOwner>
// Qt
#include <QObject>
#include <QElapsedTimer>
#include <QTimer>
#include <QBasicTimer>
#include <QRegion>

namespace KWin {

class Client;
class Scene;

class CompositorSelectionOwner : public KSelectionOwner
{
    Q_OBJECT
public:
    CompositorSelectionOwner(const char *selection);
private:
    friend class Compositor;
    bool owning;
private Q_SLOTS:
    void looseOwnership();
};

class KWIN_EXPORT Compositor : public QObject {
    Q_OBJECT
public:
    enum SuspendReason { NoReasonSuspend = 0, UserSuspend = 1<<0, BlockRuleSuspend = 1<<1, ScriptSuspend = 1<<2, AllReasonSuspend = 0xff };
    Q_DECLARE_FLAGS(SuspendReasons, SuspendReason)
    ~Compositor();
    // when adding repaints caused by a window, you probably want to use
    // either Toplevel::addRepaint() or Toplevel::addWorkspaceRepaint()
    void addRepaint(const QRect& r);
    void addRepaint(const QRegion& r);
    void addRepaint(int x, int y, int w, int h);
    /**
     * Whether the Compositor is active. That is a Scene is present and the Compositor is
     * not shutting down itself.
     **/
    bool isActive();
    int xrrRefreshRate() const {
        return m_xrrRefreshRate;
    }
    void setCompositeResetTimer(int msecs);

    bool hasScene() const {
        return m_scene != NULL;
    }

    /**
     * Checks whether @p w is the Scene's overlay window.
     **/
    bool checkForOverlayWindow(WId w) const;
    /**
     * @returns The Scene's Overlay X Window.
     **/
    WId overlayWindow() const;
    /**
     * @returns Whether the Scene's Overlay X Window is visible.
     **/
    bool isOverlayWindowVisible() const;
    /**
     * Set's the Scene's Overlay X Window visibility to @p visible.
     **/
    void setOverlayWindowVisibility(bool visible);

    Scene *scene() {
        return m_scene;
    }

    /**
     * @brief Checks whether the Compositor has already been created by the Workspace.
     *
     * This method can be used to check whether self will return the Compositor instance or @c null.
     *
     * @return bool @c true if the Compositor has been created, @c false otherwise
     **/
    static bool isCreated() {
        return s_compositor != NULL;
    }
    /**
     * @brief Static check to test whether the Compositor is available and active.
     *
     * @return bool @c true if there is a Compositor and it is active, @c false otherwise
     **/
    static bool compositing() {
        return s_compositor != NULL && s_compositor->isActive();
    }

    // for delayed supportproperty management of effects
    void keepSupportProperty(xcb_atom_t atom);
    void removeSupportProperty(xcb_atom_t atom);

public Q_SLOTS:
    void addRepaintFull();
    /**
     * @brief Suspends the Compositor if it is currently active.
     *
     * Note: it is possible that the Compositor is not able to suspend. Use @link isActive to check
     * whether the Compositor has been suspended.
     *
     * @return void
     * @see resume
     * @see isActive
     **/
    void suspend(Compositor::SuspendReason reason);
    /**
     * @brief Resumes the Compositor if it is currently suspended.
     *
     * Note: it is possible that the Compositor cannot be resumed, that is there might be Clients
     * blocking the usage of Compositing or the Scene might be broken. Use @link isActive to check
     * whether the Compositor has been resumed. Also check @link isCompositingPossible and
     * @link isOpenGLBroken.
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
    void resume(Compositor::SuspendReason reason);
    /**
     * Actual slot to perform the toggling compositing.
     * That is if the Compositor is suspended it will be resumed and if the Compositor is active
     * it will be suspended.
     * Invoked primarily by the keybinding.
     * TODO: make private slot
     **/
    void slotToggleCompositing();
    /**
     * Re-initializes the Compositor completely.
     * Connected to the D-Bus signal org.kde.KWin /KWin reinitCompositing
     **/
    void slotReinitialize();
    /**
     * Schedules a new repaint if no repaint is currently scheduled.
     **/
    void scheduleRepaint();
    void updateCompositeBlocking();
    void updateCompositeBlocking(KWin::Client* c);

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

Q_SIGNALS:
    void compositingToggled(bool active);
    void aboutToDestroy();
    void sceneCreated();

protected:
    void timerEvent(QTimerEvent *te);

private Q_SLOTS:
    void setup();
    /**
     * Called from setupCompositing() when the CompositingPrefs are ready.
     **/
    void slotCompositingOptionsInitialized();
    void finish();
    /**
     * Restarts the Compositor if running.
     * That is the Compositor will be stopped and started again.
     **/
    void restart();
    void fallbackToXRenderCompositing();
    void performCompositing();
    void slotConfigChanged();
    void releaseCompositorSelection();
    void deleteUnusedSupportProperties();

private:
    void claimCompositorSelection();
    void setCompositeTimer();
    bool windowRepaintsPending() const;
    /**
     * Continues the startup after Scene And Workspace are created
     **/
    void startupWithWorkspace();

    /**
     * Whether the Compositor is currently suspended, 8 bits encoding the reason
     **/
    SuspendReasons m_suspended;

    QBasicTimer compositeTimer;
    CompositorSelectionOwner* cm_selection;
    QTimer m_releaseSelectionTimer;
    QList<xcb_atom_t> m_unusedSupportProperties;
    QTimer m_unusedSupportPropertyTimer;
    qint64 vBlankInterval, fpsInterval;
    int m_xrrRefreshRate;
    QElapsedTimer nextPaintReference;
    QRegion repaints_region;

    QTimer compositeResetTimer; // for compressing composite resets
    bool m_finishing; // finish() sets this variable while shutting down
    bool m_starting; // start() sets this variable while starting
    qint64 m_timeSinceLastVBlank;
    qint64 m_timeSinceStart = 0;
    Scene *m_scene;
    bool m_bufferSwapPending;
    bool m_composeAtSwapCompletion;
    int m_framesToTestForSafety = 3;

    KWIN_SINGLETON_VARIABLE(Compositor, s_compositor)
};
}

# endif // KWIN_COMPOSITE_H
