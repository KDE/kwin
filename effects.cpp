/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "effects.h"

namespace KWinInternal
{

//****************************************
// Effect
//****************************************

Effect::~Effect()
    {
    }

void Effect::windowUserMoved( Toplevel* )
    {
    }

void Effect::windowUserResized( Toplevel* )
    {
    }

void Effect::paintWindow( Toplevel* )
    {
    }

void Effect::paintWorkspace( Workspace* )
    {
    }

//****************************************
// EffectsHandler
//****************************************

void EffectsHandler::windowUserMoved( Toplevel* c )
    {
    }

void EffectsHandler::windowUserResized( Toplevel* c )
    {
    }

void EffectsHandler::paintWindow( Toplevel* c )
    {
    }

void EffectsHandler::paintWorkspace( Workspace* )
    {
    }

EffectsHandler* effects;

} // namespace
