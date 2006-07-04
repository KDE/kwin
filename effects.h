/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_EFFECTS_H
#define KWIN_EFFECTS_H

namespace KWinInternal
{

class Toplevel;
class Workspace;

class Effect
    {
    public:
        virtual ~Effect();
        virtual void windowUserMoved( Toplevel* c );
        virtual void windowUserResized( Toplevel* c );
        virtual void paintWindow( Toplevel* c );
        virtual void paintWorkspace( Workspace* );
    };

class EffectsHandler
    {
    public:
        void windowUserMoved( Toplevel* c );
        void windowUserResized( Toplevel* c );
        void paintWindow( Toplevel* c );
        void paintWorkspace( Workspace* );
    };

extern EffectsHandler* effects;

} // namespace

#endif
