/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_SM_H
#define KWIN_SM_H

#include <QDataStream>
#include <kapplication.h>
#include <ksessionmanager.h>
#include <X11/SM/SMlib.h>
#include <netwm_def.h>

class QSocketNotifier;

namespace KWinInternal
{

struct SessionInfo
    {
    QByteArray sessionId;
    QByteArray windowRole;
    QByteArray wmCommand;
    QByteArray wmClientMachine;
    QByteArray resourceName;
    QByteArray resourceClass;

    QRect geometry;
    QRect restore;
    QRect fsrestore;
    int maximized;
    int fullscreen;
    int desktop;
    bool minimized;
    bool onAllDesktops;
    bool shaded;
    bool keepAbove;
    bool keepBelow;
    bool skipTaskbar;
    bool skipPager;
    bool userNoBorder;
    NET::WindowType windowType;
    QString shortcut;
    bool active; // means 'was active in the saved session'
    int stackingOrder;
    };


enum SMSavePhase
    {
    SMSavePhase0,     // saving global state in "phase 0"
    SMSavePhase2,     // saving window state in phase 2
    SMSavePhase2Full  // complete saving in phase2, there was no phase 0
    };
    
class SessionSaveDoneHelper
    : public QObject
    {
    Q_OBJECT
    public:
        SessionSaveDoneHelper();
        virtual ~SessionSaveDoneHelper();
        SmcConn connection() const { return conn; }
        void saveDone();
        void close();
    private slots:
        void processData();
    private:
        QSocketNotifier* notifier;
        SmcConn conn;
    };


class SessionManager
    : public KSessionManager
    {
    public:
        virtual bool saveState( QSessionManager& sm );
        virtual bool commitData( QSessionManager& sm );
    };

} // namespace

#endif
