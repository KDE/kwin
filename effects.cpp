/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "effects.h"

#include "toplevel.h"
#include "client.h"
#include "scene.h"

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

Matrix operator*( const Matrix& m1, const Matrix& m2 )
    {
    Matrix r;
    for( int i = 0;
         i < 4;
         ++i )
        for( int j = 0;
             j < 4;
             ++j )
            {
            double s = 0;
            for( int k = 0;
                 k < 4;
                 ++k )
                s += m1.m[ k ][ j ]  * m2.m[ i ][ k ];
            r.m[ i ][ j ] = s;
            }
    return r;
    }

bool Matrix::isIdentity() const
    {
    return m[ 0 ][ 0 ] == 1
        && m[ 0 ][ 1 ] == 0
        && m[ 0 ][ 2 ] == 0
        && m[ 0 ][ 3 ] == 0
        && m[ 1 ][ 0 ] == 0
        && m[ 1 ][ 1 ] == 1
        && m[ 1 ][ 2 ] == 0
        && m[ 1 ][ 3 ] == 0
        && m[ 2 ][ 0 ] == 0
        && m[ 2 ][ 1 ] == 0
        && m[ 2 ][ 2 ] == 1
        && m[ 2 ][ 3 ] == 0
        && m[ 3 ][ 0 ] == 0
        && m[ 3 ][ 1 ] == 0
        && m[ 3 ][ 2 ] == 0
        && m[ 3 ][ 3 ] == 1;
    }


bool Matrix::isOnlyTranslate() const
    {
    return m[ 0 ][ 0 ] == 1
        && m[ 0 ][ 1 ] == 0
        && m[ 0 ][ 2 ] == 0
//        && m[ 0 ][ 3 ] == 
        && m[ 1 ][ 0 ] == 0
        && m[ 1 ][ 1 ] == 1
        && m[ 1 ][ 2 ] == 0
//        && m[ 1 ][ 3 ] == 
        && m[ 2 ][ 0 ] == 0
        && m[ 2 ][ 1 ] == 0
        && m[ 2 ][ 2 ] == 1
//        && m[ 2 ][ 3 ] == 
        && m[ 3 ][ 0 ] == 0
        && m[ 3 ][ 1 ] == 0
        && m[ 3 ][ 2 ] == 0
        && m[ 3 ][ 3 ] == 1;
    }

QPoint Matrix::transform( const QPoint& p ) const
    {
    int vec[ 4 ] = { p.x(), p.y(), 0, 1 };
    int res[ 4 ];
    for( int i = 0;
         i < 4;
         ++i )
        {
        double s = 0;
        for( int j = 0;
             j < 4;
             ++j )
            s += m[ i ][ j ] * vec[ j ];
        res[ i ] = int( s );
        }
    return QPoint( res[ 0 ], res[ 1 ] );
    }

//****************************************
// Effect
//****************************************

Effect::~Effect()
    {
    }

void Effect::windowUserMovedResized( Toplevel* , bool, bool )
    {
    }

void Effect::transformWindow( Toplevel*, EffectData& )
    {
    }

void Effect::transformWorkspace( Workspace*, EffectData& )
    {
    }

void Effect::windowDeleted( Toplevel* )
    {
    }

void MakeHalfTransparent::transformWindow( Toplevel* c, EffectData& data )
    {
    if( c->isDialog())
        data.opacity *= 0.8;
    if( Client* c2 = dynamic_cast< Client* >( c ))
        if( c2->isMove() || c2->isResize())
            data.opacity *= 0.5;
    }

void MakeHalfTransparent::windowUserMovedResized( Toplevel* c, bool first, bool last )
    {
    if( first || last )
        c->workspace()->addDamage( c, c->geometry());
    }

ShakyMove::ShakyMove()
    {
    connect( &timer, SIGNAL( timeout()), SLOT( tick()));
    }

static const int shaky_diff[] = { 0, 1, 2, 3, 2, 1, 0, -1, -2, -3, -2, -1 };
static const int SHAKY_MAX = sizeof( shaky_diff ) / sizeof( shaky_diff[ 0 ] );

void ShakyMove::transformWindow( Toplevel* c, EffectData& data )
    {
    if( windows.contains( c ))
        {
        Matrix m;
        m.m[ 0 ][ 3 ] = shaky_diff[ windows[ c ]];
        data.matrix *= m;
        }
    }

void ShakyMove::windowUserMovedResized( Toplevel* c, bool first, bool last )
    {
    if( first )
        {
        if( windows.isEmpty())
            timer.start( 50 );
        windows[ c ] = 0;
        }
    else if( last )
        {
        windows.remove( c );
        scene->updateTransformation( c );
        c->workspace()->addDamage( c, c->geometry().adjusted( -3, 7, 0, 0 ));
        if( windows.isEmpty())
            timer.stop();
        }
    }

void ShakyMove::windowDeleted( Toplevel* c )
    {
    windows.remove( c );
    if( windows.isEmpty())
        timer.stop();
    }

void ShakyMove::tick()
    {
    for( QMap< Toplevel*, int >::Iterator it = windows.begin();
         it != windows.end();
         ++it )
        {
        if( *it == SHAKY_MAX - 1 )
            *it = 0;
        else
            ++(*it);
        scene->updateTransformation( it.key());
        it.key()->workspace()->addDamage( it.key(), it.key()->geometry().adjusted( -1, 2, 0, 0 ));
        }
    }

static MakeHalfTransparent* mht;
static ShakyMove* sm;

//****************************************
// EffectsHandler
//****************************************

EffectsHandler::EffectsHandler()
    {
    mht = new MakeHalfTransparent;
    sm = new ShakyMove;
    }

void EffectsHandler::windowUserMovedResized( Toplevel* c, bool first, bool last )
    {
    if( mht )
        mht->windowUserMovedResized( c, first, last );
    if( sm )
        sm->windowUserMovedResized( c, first, last );
    }

void EffectsHandler::transformWindow( Toplevel* c, EffectData& data )
    {
    if( mht )
        mht->transformWindow( c, data );
    if( sm )
        sm->transformWindow( c, data );
    }

void EffectsHandler::transformWorkspace( Workspace* w, EffectData& data )
    {
    if( mht )
        mht->transformWorkspace( w, data );
    if( sm )
        sm->transformWorkspace( w, data );
    }

void EffectsHandler::windowDeleted( Toplevel* c )
    {
    if( mht )
        mht->windowDeleted( c );
    if( sm )
        sm->windowDeleted( c );
    }

EffectsHandler* effects;

} // namespace

#include "effects.moc"
