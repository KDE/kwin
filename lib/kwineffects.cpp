/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "kwineffects.h"

#include <QStringList>
#include <QFile>

#include "kdebug.h"
#include "klibloader.h"
#include "kdesktopfile.h"
#include "kconfiggroup.h"
#include "kstandarddirs.h"

#include <assert.h>

namespace KWin
{

WindowPaintData::WindowPaintData()
    : opacity( 1.0 )
    , xScale( 1 )
    , yScale( 1 )
    , xTranslate( 0 )
    , yTranslate( 0 )
    , saturation( 1 )
    , brightness( 1 )
    {
    }

ScreenPaintData::ScreenPaintData()
    : xScale( 1 )
    , yScale( 1 )
    , xTranslate( 0 )
    , yTranslate( 0 )
    {
    }

//****************************************
// Effect
//****************************************

Effect::Effect()
    {
    }

Effect::~Effect()
    {
    }

void Effect::windowUserMovedResized( EffectWindow* , bool, bool )
    {
    }

void Effect::windowOpacityChanged( EffectWindow*, double )
    {
    }

void Effect::windowAdded( EffectWindow* )
    {
    }

void Effect::windowClosed( EffectWindow* )
    {
    }

void Effect::windowDeleted( EffectWindow* )
    {
    }

void Effect::windowActivated( EffectWindow* )
    {
    }

void Effect::windowMinimized( EffectWindow* )
    {
    }

void Effect::windowUnminimized( EffectWindow* )
    {
    }

void Effect::windowInputMouseEvent( Window, QEvent* )
    {
    }

void Effect::desktopChanged( int )
    {
    }

void Effect::windowDamaged( EffectWindow*, const QRect& )
    {
    }

void Effect::windowGeometryShapeChanged( EffectWindow*, const QRect& )
    {
    }

void Effect::tabBoxAdded( int )
    {
    }

void Effect::tabBoxClosed()
    {
    }

void Effect::tabBoxUpdated()
    {
    }
bool Effect::borderActivated( ElectricBorder )
    {
    return false;
    }

void Effect::prePaintScreen( int* mask, QRegion* region, int time )
    {
    effects->prePaintScreen( mask, region, time );
    }

void Effect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    effects->paintScreen( mask, region, data );
    }
    
void Effect::postPaintScreen()
    {
    effects->postPaintScreen();
    }

void Effect::prePaintWindow( EffectWindow* w, int* mask, QRegion* paint, QRegion* clip, int time )
    {
    effects->prePaintWindow( w, mask, paint, clip, time );
    }

void Effect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    effects->paintWindow( w, mask, region, data );
    }

void Effect::postPaintWindow( EffectWindow* w )
    {
    effects->postPaintWindow( w );
    }

void Effect::drawWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    effects->drawWindow( w, mask, region, data );
    }

QRect Effect::transformWindowDamage( EffectWindow* w, const QRect& r )
    {
    return effects->transformWindowDamage( w, r );
    }

void Effect::setPositionTransformations( WindowPaintData& data, QRect& region, EffectWindow* w,
    const QRect& r, Qt::AspectRatioMode aspect )
    {
    QSize size = w->size();
    size.scale( r.size(), aspect );
    data.xScale = size.width() / double( w->width());
    data.yScale = size.height() / double( w->height());
    int width = int( w->width() * data.xScale );
    int height = int( w->height() * data.yScale );
    int x = r.x() + ( r.width() - width ) / 2;
    int y = r.y() + ( r.height() - height ) / 2;
    region = QRect( x, y, width, height );
    data.xTranslate = x - w->x();
    data.yTranslate = y - w->y();
    }

int Effect::displayWidth()
    {
    return KWin::displayWidth();
    }

int Effect::displayHeight()
    {
    return KWin::displayHeight();
    }

QPoint Effect::cursorPos()
    {
    return KWin::cursorPos();
    }

//****************************************
// EffectsHandler
//****************************************

EffectsHandler::EffectsHandler(CompositingType type)
    : current_paint_screen( 0 )
    , current_paint_window( 0 )
    , current_draw_window( 0 )
    , current_transform( 0 )
    , compositing_type( type )
    {
    if( compositing_type == NoCompositing )
        return;
    KWin::effects = this;
    }

EffectsHandler::~EffectsHandler()
    {
    // All effects should already be unloaded by Impl dtor
    assert( loaded_effects.count() == 0 );
    }

void EffectsHandler::windowUserMovedResized( EffectWindow* c, bool first, bool last )
    {
    foreach( EffectPair ep, loaded_effects )
        ep.second->windowUserMovedResized( c, first, last );
    }

void EffectsHandler::windowOpacityChanged( EffectWindow* c, double old_opacity )
    {
    foreach( EffectPair ep, loaded_effects )
        ep.second->windowOpacityChanged( c, old_opacity );
    }

void EffectsHandler::windowAdded( EffectWindow* c )
    {
    foreach( EffectPair ep, loaded_effects )
        ep.second->windowAdded( c );
    }

void EffectsHandler::windowDeleted( EffectWindow* c )
    {
    foreach( EffectPair ep, loaded_effects )
        ep.second->windowDeleted( c );
    }

void EffectsHandler::windowClosed( EffectWindow* c )
    {
    foreach( EffectPair ep, loaded_effects )
        ep.second->windowClosed( c );
    }

void EffectsHandler::windowActivated( EffectWindow* c )
    {
    foreach( EffectPair ep, loaded_effects )
        ep.second->windowActivated( c );
    }

void EffectsHandler::windowMinimized( EffectWindow* c )
    {
    foreach( EffectPair ep, loaded_effects )
        ep.second->windowMinimized( c );
    }

void EffectsHandler::windowUnminimized( EffectWindow* c )
    {
    foreach( EffectPair ep, loaded_effects )
        ep.second->windowUnminimized( c );
    }

void EffectsHandler::desktopChanged( int old )
    {
    foreach( EffectPair ep, loaded_effects )
        ep.second->desktopChanged( old );
    }

void EffectsHandler::windowDamaged( EffectWindow* w, const QRect& r )
    {
    if( w == NULL )
        return;
    foreach( EffectPair ep, loaded_effects )
        ep.second->windowDamaged( w, r );
    }

void EffectsHandler::windowGeometryShapeChanged( EffectWindow* w, const QRect& old )
    {
    if( w == NULL ) // during late cleanup effectWindow() may be already NULL
        return;     // in some functions that may still call this
    foreach( EffectPair ep, loaded_effects )
        ep.second->windowGeometryShapeChanged( w, old );
    }

void EffectsHandler::tabBoxAdded( int mode )
    {
    foreach( EffectPair ep, loaded_effects )
        ep.second->tabBoxAdded( mode );
    }

void EffectsHandler::tabBoxClosed()
    {
    foreach( EffectPair ep, loaded_effects )
        ep.second->tabBoxClosed();
    }

void EffectsHandler::tabBoxUpdated()
    {
    foreach( EffectPair ep, loaded_effects )
        ep.second->tabBoxUpdated();
    }

bool EffectsHandler::borderActivated( ElectricBorder border )
    {
    bool ret = false;
    foreach( EffectPair ep, loaded_effects )
        if( ep.second->borderActivated( border ))
            ret = true; // bail out or tell all?
    return ret;
    }


// start another painting pass
void EffectsHandler::startPaint()
    {
    assert( current_paint_screen == 0 );
    assert( current_paint_window == 0 );
    assert( current_draw_window == 0 );
    assert( current_transform == 0 );
    }

QRect EffectsHandler::transformWindowDamage( EffectWindow* w, const QRect& r )
    {
    if( current_transform < loaded_effects.size())
        {
        QRect rr = loaded_effects[current_transform++].second->transformWindowDamage( w, r );
        --current_transform;
        return rr;
        }
    else
        return r;
    }

Window EffectsHandler::createInputWindow( Effect* e, const QRect& r, const QCursor& cursor )
    {
    return createInputWindow( e, r.x(), r.y(), r.width(), r.height(), cursor );
    }

Window EffectsHandler::createFullScreenInputWindow( Effect* e, const QCursor& cursor )
    {
    return createInputWindow( e, 0, 0, displayWidth(), displayHeight(), cursor );
    }

KLibrary* EffectsHandler::findEffectLibrary( const QString& effectname )
{
    QString libname = "kwin4_effect_" + effectname.toLower();

    QString desktopfile = KStandardDirs::locate("appdata",
            "effects/" + effectname.toLower() + ".desktop");
    if( !desktopfile.isEmpty() )
        {
        KDesktopFile desktopconf( desktopfile );
        KConfigGroup conf = desktopconf.desktopGroup();
        libname = conf.readEntry( "X-KDE-Library", libname );
        }

    KLibrary* library = KLibLoader::self()->library(QFile::encodeName(libname));
    if( !library )
        {
        kError( 1212 ) << k_funcinfo << "couldn't open library for effect '" <<
                effectname << "'" << endl;
        return 0;
        }

    return library;
}

void EffectsHandler::loadEffect( const QString& name )
    {
    assert( current_paint_screen == 0 );
    assert( current_paint_window == 0 );
    assert( current_draw_window == 0 );
    assert( current_transform == 0 );

    // Make sure a single effect won't be loaded multiple times
    for(QVector< EffectPair >::const_iterator it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); it++)
        {
        if( (*it).first == name )
            {
            kDebug( 1212 ) << "EffectsHandler::loadEffect : Effect already loaded : " << name << endl;
            return;
            }
        }


    kDebug( 1212 ) << k_funcinfo << "Trying to load " << name << endl;
    KLibrary* library = findEffectLibrary( name );
    if( !library )
        {
        return;
        }

    QString supported_symbol = "effect_supported_" + name;
    KLibrary::void_function_ptr supported_func = library->resolveFunction(supported_symbol.toAscii().data());
    QString create_symbol = "effect_create_" + name;
    KLibrary::void_function_ptr create_func = library->resolveFunction(create_symbol.toAscii().data());
    if( supported_func )
        {
        typedef bool (*t_supportedfunc)();
        t_supportedfunc supported = reinterpret_cast<t_supportedfunc>(supported_func);
        if(!supported())
            {
            kWarning( 1212 ) << "EffectsHandler::loadEffect : Effect " << name << " is not supported" << endl;
            library->unload();
            return;
            }
        }
    if(!create_func)
        {
        kError( 1212 ) << "EffectsHandler::loadEffect : effect_create function not found" << endl;
        library->unload();
        return;
        }
    typedef Effect* (*t_createfunc)();
    t_createfunc create = reinterpret_cast<t_createfunc>(create_func);

    Effect* e = create();

    loaded_effects.append( EffectPair( name, e ) );
    effect_libraries[ name ] = library;
    }

void EffectsHandler::unloadEffect( const QString& name )
    {
    assert( current_paint_screen == 0 );
    assert( current_paint_window == 0 );
    assert( current_draw_window == 0 );
    assert( current_transform == 0 );

    for( QVector< EffectPair >::iterator it = loaded_effects.begin(); it != loaded_effects.end(); it++)
        {
        if ( (*it).first == name )
            {
            kDebug( 1212 ) << "EffectsHandler::unloadEffect : Unloading Effect : " << name << endl;
            delete (*it).second;
            loaded_effects.erase(it);
            effect_libraries[ name ]->unload();
            return;
            }
        }

    kDebug( 1212 ) << "EffectsHandler::unloadEffect : Effect not loaded : " << name << endl;
    }


EffectsHandler* effects = 0;


//****************************************
// EffectWindow
//****************************************

EffectWindow::EffectWindow()
    {
    }

EffectWindow::~EffectWindow()
    {
    }

bool EffectWindow::isOnCurrentDesktop() const
    {
    return isOnDesktop( effects->currentDesktop());
    }

bool EffectWindow::isOnDesktop( int d ) const
    {
    return desktop() == d || isOnAllDesktops();
    }


//****************************************
// EffectWindowGroup
//****************************************

EffectWindowGroup::~EffectWindowGroup()
    {
    }


} // namespace
