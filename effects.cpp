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
// Matrix
//****************************************

Matrix::Matrix()
    {
    m[ 0 ][ 0 ] = 1;
    m[ 0 ][ 1 ] = 0;
    m[ 0 ][ 2 ] = 0;
    m[ 0 ][ 3 ] = 0;
    m[ 1 ][ 0 ] = 0;
    m[ 1 ][ 1 ] = 1;
    m[ 1 ][ 2 ] = 0;
    m[ 1 ][ 3 ] = 0;
    m[ 2 ][ 0 ] = 0;
    m[ 2 ][ 1 ] = 0;
    m[ 2 ][ 2 ] = 1;
    m[ 2 ][ 3 ] = 0;
    m[ 3 ][ 0 ] = 0;
    m[ 3 ][ 1 ] = 0;
    m[ 3 ][ 2 ] = 0;
    m[ 3 ][ 3 ] = 1;
    }

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

void Effect::paintWindow( Toplevel*, EffectData& )
    {
    }

void Effect::paintWorkspace( Workspace*, EffectData& )
    {
    }

//****************************************
// EffectsHandler
//****************************************

void EffectsHandler::windowUserMoved( Toplevel* )
    {
    }

void EffectsHandler::windowUserResized( Toplevel* )
    {
    }

void EffectsHandler::paintWindow( Toplevel*, EffectData& )
    {
    }

void EffectsHandler::paintWorkspace( Workspace*, EffectData& )
    {
    }

EffectsHandler* effects;

} // namespace
