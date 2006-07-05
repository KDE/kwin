/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

// TODO MIT or some other licence, perhaps move to some lib

#ifndef KWIN_EFFECTS_H
#define KWIN_EFFECTS_H

namespace KWinInternal
{

class Toplevel;
class Workspace;

class Matrix
    {
    public:
        Matrix();
        double m[ 4 ][ 4 ];
    };

class EffectData
    {
    public:
        Matrix matrix;
        double opacity;
    };

class Effect
    {
    public:
        virtual ~Effect();
        virtual void windowUserMoved( Toplevel* c );
        virtual void windowUserResized( Toplevel* c );
        virtual void paintWindow( Toplevel* c, EffectData& data );
        virtual void paintWorkspace( Workspace*, EffectData& data );
    };

class EffectsHandler
    {
    public:
        void windowUserMoved( Toplevel* c );
        void windowUserResized( Toplevel* c );
        void paintWindow( Toplevel* c, EffectData& data );
        void paintWorkspace( Workspace*, EffectData& data );
    };

extern EffectsHandler* effects;

} // namespace

#endif
