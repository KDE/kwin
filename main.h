/*****************************************************************
kwin - the KDE window manager
								
Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
******************************************************************/
#ifndef MAIN_H
#define MAIN_H

#include <kapplication.h>
#include <kmanagerselection.h>
#include "workspace.h"

class KWinSelectionOwner
    : public KSelectionOwner
    {
    Q_OBJECT
    public:
        KWinSelectionOwner( int screen );
    protected:
        virtual bool genericReply( Atom target, Atom property, Window requestor );
        virtual void replyTargets( Atom property, Window requestor );
        virtual void getAtoms();
    private:
        Atom make_selection_atom( int screen );
        static Atom xa_version;
    };

class Application : public  KApplication
{
    Q_OBJECT
public:
    Application();
    ~Application();

protected:
    bool x11EventFilter( XEvent * );
private:
    KWinSelectionOwner owner;
};


#endif
