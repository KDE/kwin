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

#include <QtCore/QObject>
#include <QtCore/QElapsedTimer>
#include <QtCore/QTimer>
#include <QtCore/QElapsedTimer>
#include <QtCore/QBasicTimer>
#include <QRegion>

class KSelectionOwner;

namespace KWin {

class Client;
class Scene;

class Compositor : public QObject {
    Q_OBJECT
public:
    ~Compositor();
    // when adding repaints caused by a window, you probably want to use
    // either Toplevel::addRepaint() or Toplevel::addWorkspaceRepaint()
    void addRepaint(const QRect& r);
    void addRepaint(const QRegion& r);
    void addRepaint(int x, int y, int w, int h);
    /**
     * Called from the D-Bus interface. Does the same as slotToggleCompositing with the
     * addition to show a notification on how to revert the compositing state.
     **/
    void toggleCompositing();
    // Mouse polling
    void startMousePolling();
    void stopMousePolling();
    /**
     * Whether the Compositor is active. That is a Scene is present and the Compositor is
     * not shutting down itself.
     **/
    bool isActive();
    int xrrRefreshRate() const {
        return m_xrrRefreshRate;
    }
    void setCompositeResetTimer(int msecs);
    // returns the _estimated_ delay to the next screen update
    // good for having a rough idea to calculate transformations, bad to rely on.
    // might happen few ms earlier, might be an entire frame to short. This is NOT deterministic.
    int nextFrameDelay() const {
        return m_nextFrameDelay;
    }
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
     * @brief Factory Method to create the Compositor singleton.
     *
     * This method is mainly used by Workspace to create the Compositor Singleton as a child
     * of the Workspace.
     *
     * To actually access the Compositor instance use @link self.
     *
     * @param parent The parent object
     * @return :Compositor* Created Compositor if not already created
     * @warning This method is not Thread safe.
     * @see self
     **/
    static Compositor *createCompositor(QObject *parent);
    /**
     * @brief Singleton getter for the Compositor object.
     *
     * Ensure that the Compositor has been created through createCompositor prior to access
     * this method.
     *
     * @return :Compositor* The Compositor instance
     * @see createCompositor
     **/
    static Compositor *self() {
        Q_ASSERT(s_compositor);
        return s_compositor;
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

public Q_SLOTS:
    void addRepaintFull();
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
    void checkUnredirect();
    void checkUnredirect(bool force);
    void updateCompositeBlocking();
    void updateCompositeBlocking(KWin::Client* c);

Q_SIGNALS:
    void compositingToggled(bool active);

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
    void performMousePoll();
    void delayedCheckUnredirect();
    void slotConfigChanged();

private:
    Compositor(QObject *workspace);
    /**
     * Suspends or Resumes the Compositor.
     * That is stops the Scene in case of @p suspend and restores otherwise.
     * @param suspend If @c true suspends the Compositor, if @c false starts the Compositor.
     * TODO: split in two methods: suspend and resume
     **/
    void suspendResume(bool suspend = true);
    void setCompositeTimer();
    bool windowRepaintsPending() const;

    /**
     * Restarts the Window Manager in case that the Qt's GraphicsSystem need to be changed
     * for the chosen Compositing backend.
     * @param reason The reason why the Window Manager is being restarted, this is logged
     **/
    void restartKWin(const QString &reason);

    /**
     * Whether the Compositor is currently suspended.
     **/
    bool m_suspended;
    /**
     * Whether the Compositor is currently blocked by at least one Client requesting full resources.
     **/
    bool m_blocked;
    QBasicTimer compositeTimer;
    KSelectionOwner* cm_selection;
    uint vBlankInterval, fpsInterval;
    int m_xrrRefreshRate;
    QElapsedTimer nextPaintReference;
    QTimer mousePollingTimer;
    QRegion repaints_region;

    QTimer unredirectTimer;
    bool forceUnredirectCheck;
    QTimer compositeResetTimer; // for compressing composite resets
    bool m_finishing; // finish() sets this variable while shutting down
    int m_timeSinceLastVBlank, m_nextFrameDelay;
    Scene *m_scene;

    static Compositor *s_compositor;
};
}

# endif // KWIN_COMPOSITE_H
