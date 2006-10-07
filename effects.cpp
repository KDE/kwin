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

void Effect::transformWindow( Toplevel*, Matrix&, EffectData& )
    {
    }

void Effect::transformWorkspace( Matrix&, EffectData& )
    {
    }

void Effect::windowDeleted( Toplevel* )
    {
    }

void MakeHalfTransparent::transformWindow( Toplevel* c, Matrix&, EffectData& data )
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
        c->addDamage( c->rect());
    }

ShakyMove::ShakyMove()
    {
    connect( &timer, SIGNAL( timeout()), SLOT( tick()));
    }

static const int shaky_diff[] = { 0, 1, 2, 3, 2, 1, 0, -1, -2, -3, -2, -1 };
static const int SHAKY_MAX = sizeof( shaky_diff ) / sizeof( shaky_diff[ 0 ] );

void ShakyMove::transformWindow( Toplevel* c, Matrix& matrix, EffectData& )
    {
    if( windows.contains( c ))
        {
        Matrix m;
        m.m[ 0 ][ 3 ] = shaky_diff[ windows[ c ]];
        matrix *= m;
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
        // TODO just damage whole screen, transformation is involved
        c->workspace()->addDamage( c->geometry().adjusted( -3, 7, 0, 0 ));
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
        // TODO damage whole screen, transformation is involved
        it.key()->workspace()->addDamage( it.key()->geometry().adjusted( -1, 2, 0, 0 ));
        }
    }

void GrowMove::transformWindow( Toplevel* c, Matrix& matrix, EffectData& )
    {
    if( Client* c2 = dynamic_cast< Client* >( c ))
        if( c2->isMove())
            {
            Matrix m;
            m.m[ 0 ][ 0 ] = 1.2;
            m.m[ 1 ][ 1 ] = 1.4;
            matrix *= m;
            }
    }

void GrowMove::windowUserMovedResized( Toplevel* c, bool first, bool last )
    {
    if( first || last )
        {
        // TODO damage whole screen, transformation is involved
        c->workspace()->addDamage( c->geometry());
        }
    }

ShiftWorkspaceUp::ShiftWorkspaceUp( Workspace* ws )
    : up( false )
    , wspace( ws )
    {
    connect( &timer, SIGNAL( timeout()), SLOT( tick()));
    timer.start( 2000 );
    }

void ShiftWorkspaceUp::transformWorkspace( Matrix& matrix, EffectData& )
    {
    if( !up )
        return;
    Matrix m;
    m.m[ 1 ][ 3 ] = -10;
    matrix *= m;
    }

void ShiftWorkspaceUp::tick()
    {
    up = !up;
    wspace->addDamage( 0, 0, displayWidth(), displayHeight());
    }

static MakeHalfTransparent* mht;
static ShakyMove* sm;
static GrowMove* gm;
static ShiftWorkspaceUp* swu;

//****************************************
// EffectsHandler
//****************************************

EffectsHandler::EffectsHandler( Workspace* ws )
    {
    if( !compositing())
        return;
    mht = new MakeHalfTransparent;
    sm = new ShakyMove;
//    gm = new GrowMove;
//    swu = new ShiftWorkspaceUp( ws );
    }
    
EffectsHandler::~EffectsHandler()
    {
    delete mht;
    mht = NULL;
    delete sm;
    sm = NULL;
    delete gm;
    gm = NULL;
    delete swu;
    swu = NULL;
    }

void EffectsHandler::windowUserMovedResized( Toplevel* c, bool first, bool last )
    {
    if( mht )
        mht->windowUserMovedResized( c, first, last );
    if( sm )
        sm->windowUserMovedResized( c, first, last );
    if( gm )
        gm->windowUserMovedResized( c, first, last );
    if( swu )
        swu->windowUserMovedResized( c, first, last );
    }

void EffectsHandler::transformWindow( Toplevel* c, Matrix& matrix, EffectData& data )
    {
    if( mht )
        mht->transformWindow( c, matrix, data );
    if( sm )
        sm->transformWindow( c, matrix, data );
    if( gm )
        gm->transformWindow( c, matrix, data );
    if( swu )
        swu->transformWindow( c, matrix, data );
    }

void EffectsHandler::transformWorkspace( Matrix& matrix, EffectData& data )
    {
    if( mht )
        mht->transformWorkspace( matrix, data );
    if( sm )
        sm->transformWorkspace( matrix, data );
    if( gm )
        gm->transformWorkspace( matrix, data );
    if( swu )
        swu->transformWorkspace( matrix, data );
    }

void EffectsHandler::windowDeleted( Toplevel* c )
    {
    if( mht )
        mht->windowDeleted( c );
    if( sm )
        sm->windowDeleted( c );
    if( gm )
        gm->windowDeleted( c );
    if( swu )
        swu->windowDeleted( c );
    }

EffectsHandler* effects;

} // namespace

#include "effects.moc"
