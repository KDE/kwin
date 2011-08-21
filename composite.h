/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2011 Arthur Arlt <a.arlt@stud.uni-heidelberg.de>

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
#include <QtCore/QTimer>
#include <QRegion>
#include <kmanagerselection.h>
#include "workspace.h"

namespace KWin {

class Workspace;

class Compositor : public QObject {
    Q_OBJECT
public:
    Compositor(Workspace *workspace);
    ~Compositor();
    void checkCompositeTimer();
    // when adding repaints caused by a window, you probably want to use
    // either Toplevel::addRepaint() or Toplevel::addWorkspaceRepaint()
    void addRepaint(const QRect& r);
    void addRepaint(const QRegion& r);
    void addRepaint(int x, int y, int w, int h);
    void checkUnredirect(bool force = false);
    void toggleCompositing();
    void updateCompositeBlocking(Client* c = NULL);
    // Mouse polling
    void startMousePolling();
    void stopMousePolling();
    bool compositingActive();
    int xrrRefreshRate() {
        return m_xrrRefreshRate;
    }
    void setCompositeResetTimer(int msecs);
    // returns the _estimated_ delay to the next screen update
    // good for having a rough idea to calculate transformations, bad to rely on.
    // might happen few ms earlier, might be an entire frame to short. This is NOT deterministic.
    int nextFrameDelay() {
        return m_nextFrameDelay;
    }

public Q_SLOTS:
    void addRepaintFull();
    void slotToggleCompositing();
    void suspendCompositing();
    void suspendCompositing(bool suspend);
    void resetCompositing();

Q_SIGNALS:
    void compositingToggled(bool active);
    void signalRestartKWin(const QString &reason);

protected:
    void timerEvent(QTimerEvent *te);

private Q_SLOTS:
    void setupCompositing();
    /**
     * Called from setupCompositing() when the CompositingPrefs are ready.
     **/
    void slotCompositingOptionsInitialized();
    void finishCompositing();
    void fallbackToXRenderCompositing();
    void lostCMSelection();
    void performCompositing();
    void performMousePoll();
    void delayedCheckUnredirect();
    void slotConfigChanged();
    void slotReinitialize();

private:
    void setCompositeTimer();
    bool windowRepaintsPending() const;

    bool compositingSuspended, compositingBlocked;
    QBasicTimer compositeTimer;
    KSelectionOwner* cm_selection;
    uint vBlankInterval, fpsInterval;
    int m_xrrRefreshRate;
    QElapsedTimer nextPaintReference;
    QTimer mousePollingTimer;
    QRegion repaints_region;
    Workspace* m_workspace;

    QTimer unredirectTimer;
    bool forceUnredirectCheck;
    QTimer compositeResetTimer; // for compressing composite resets
    bool m_finishingCompositing; // finishCompositing() sets this variable while shutting down
    int m_timeSinceLastVBlank, m_nextFrameDelay;
};
}

# endif // KWIN_COMPOSITE_H
