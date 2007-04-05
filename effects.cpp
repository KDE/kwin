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
#include "options.h"
#include "deleted.h"

#include "effects/boxswitch.h"
#include "effects/demo_taskbarthumbnail.h"
#include "effects/desktopchangeslide.h"
#include "effects/dialogparent.h"
#include "effects/drunken.h"
#include "effects/explosioneffect.h"
#include "effects/fade.h"
#include "effects/howto.h"
#include "effects/maketransparent.h"
#include "effects/minimizeanimation.h"
#include "effects/presentwindows.h"
#include "effects/scalein.h"
#include "effects/shadow.h"
#include "effects/shakymove.h"
#include "effects/shiftworkspaceup.h"
#include "effects/showfps.h"
#ifdef HAVE_CAPTURY
#include "effects/videorecord.h"
#endif
#ifdef HAVE_OPENGL
#include "effects/flame.h"
#include "effects/fallapart.h"
#include "effects/wavywindows.h"
#endif
#include "effects/zoom.h"

#include "effects/test_input.h"
#include "effects/test_thumbnail.h"

namespace KWin
{

//****************************************
// Effect
//****************************************

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

//****************************************
// EffectsHandler
//****************************************

EffectsHandler::EffectsHandler()
    : current_paint_screen( 0 )
    , current_paint_window( 0 )
    , current_draw_window( 0 )
    , current_transform( 0 )
    {
    if( !compositing())
        return;
    KWin::effects = this;

#ifdef HAVE_CAPTURY
    registerEffect("VideoRecord", new GenericEffectFactory<VideoRecordEffect>);
#endif
    registerEffect("ShowFps", new GenericEffectFactory<ShowFpsEffect>);
    registerEffect("Zoom", new GenericEffectFactory<ZoomEffect>);
    registerEffect("PresentWindows", new GenericEffectFactory<PresentWindowsEffect>);
#ifdef HAVE_OPENGL
    registerEffect("WavyWindows", new GenericEffectFactory<WavyWindowsEffect>);
    registerEffect("Explosion", new GenericEffectFactory<ExplosionEffect>);
#endif
    registerEffect("MinimizeAnimation", new GenericEffectFactory<MinimizeAnimationEffect>);
    registerEffect("Howto", new GenericEffectFactory<HowtoEffect>);
    registerEffect("MakeTransparent", new GenericEffectFactory<MakeTransparentEffect>);
    registerEffect("ShakyMove", new GenericEffectFactory<ShakyMoveEffect>);
    registerEffect("ShiftWorkspaceUp", new GenericEffectFactory<ShiftWorkspaceUpEffect>);
    registerEffect("Fade", new GenericEffectFactory<FadeEffect>);
    registerEffect("ScaleIn", new GenericEffectFactory<ScaleInEffect>);
    registerEffect("FallApart", new GenericEffectFactory<FallApartEffect>);
    registerEffect("Flame", new GenericEffectFactory<FlameEffect>);
    registerEffect("DialogParent", new GenericEffectFactory<DialogParentEffect>);
    registerEffect("DesktopChangeSlide", new GenericEffectFactory<DesktopChangeSlideEffect>);
    registerEffect("BoxSwitch", new GenericEffectFactory<BoxSwitchEffect>);
    registerEffect("Drunken", new GenericEffectFactory<DrunkenEffect>);
    registerEffect("TaskbarThumbnail", new GenericEffectFactory<TaskbarThumbnailEffect>);
    registerEffect("Shadow", new GenericEffectFactory<ShadowEffect>);

    registerEffect("TestInput", new GenericEffectFactory<TestInputEffect>);
    registerEffect("TestThumbnail", new GenericEffectFactory<TestThumbnailEffect>);

     QStringList::const_iterator effectsIterator;
     for( effectsIterator = options->defaultEffects.constBegin();
          effectsIterator != options->defaultEffects.constEnd();
           ++effectsIterator)
        {
        loadEffect(*effectsIterator);
        }
    }

EffectsHandler::~EffectsHandler()
    {
    foreach( EffectPair ep, loaded_effects )
        delete ep.second;
    foreach( EffectFactory* ef, effect_factories )
        delete ef;
    foreach( InputWindowPair pos, input_windows )
        XDestroyWindow( display(), pos.second );
    }

void EffectsHandler::windowUserMovedResized( EffectWindow* c, bool first, bool last )
    {
    foreach( EffectPair ep, loaded_effects )
        ep.second->windowUserMovedResized( c, first, last );
    }

void EffectsHandler::windowOpacityChanged( EffectWindow* c, double old_opacity )
    {
    if( c->window()->opacity() == old_opacity )
        return;
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

// start another painting pass
void EffectsHandler::startPaint()
    {
    assert( current_paint_screen == 0 );
    assert( current_paint_window == 0 );
    assert( current_draw_window == 0 );
    assert( current_transform == 0 );
    }

// the idea is that effects call this function again which calls the next one
void EffectsHandler::prePaintScreen( int* mask, QRegion* region, int time )
    {
    if( current_paint_screen < loaded_effects.size())
        {
        loaded_effects[current_paint_screen++].second->prePaintScreen( mask, region, time );
        --current_paint_screen;
        }
    // no special final code
    }

void EffectsHandler::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    if( current_paint_screen < loaded_effects.size())
        {
        loaded_effects[current_paint_screen++].second->paintScreen( mask, region, data );
        --current_paint_screen;
        }
    else
        scene->finalPaintScreen( mask, region, data );
    }

void EffectsHandler::postPaintScreen()
    {
    if( current_paint_screen < loaded_effects.size())
        {
        loaded_effects[current_paint_screen++].second->postPaintScreen();
        --current_paint_screen;
        }
    // no special final code
    }

void EffectsHandler::prePaintWindow( EffectWindow* w, int* mask, QRegion* paint, QRegion* clip, int time )
    {
    if( current_paint_window < loaded_effects.size())
        {
        loaded_effects[current_paint_window++].second->prePaintWindow( w, mask, paint, clip, time );
        --current_paint_window;
        }
    // no special final code
    }

void EffectsHandler::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( current_paint_window < loaded_effects.size())
        {
        loaded_effects[current_paint_window++].second->paintWindow( w, mask, region, data );
        --current_paint_window;
        }
    else
        scene->finalPaintWindow( w, mask, region, data );
    }

void EffectsHandler::postPaintWindow( EffectWindow* w )
    {
    if( current_paint_window < loaded_effects.size())
        {
        loaded_effects[current_paint_window++].second->postPaintWindow( w );
        --current_paint_window;
        }
    // no special final code
    }

void EffectsHandler::drawWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( current_draw_window < loaded_effects.size())
        {
        loaded_effects[current_draw_window++].second->drawWindow( w, mask, region, data );
        --current_draw_window;
        }
    else
        scene->finalDrawWindow( w, mask, region, data );
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

bool EffectsHandler::borderActivated( ElectricBorder border )
    {
    bool ret = false;
    foreach( EffectPair ep, loaded_effects )
        if( ep.second->borderActivated( border ))
            ret = true; // bail out or tell all?
    return ret;
    }

Window EffectsHandler::createInputWindow( Effect* e, int x, int y, int w, int h, const QCursor& cursor )
    {
    XSetWindowAttributes attrs;
    attrs.override_redirect = True;
    Window win = XCreateWindow( display(), rootWindow(), x, y, w, h, 0, 0, InputOnly, CopyFromParent,
        CWOverrideRedirect, &attrs );
// TODO keeping on top?
// TODO enter/leave notify?
    XSelectInput( display(), win, ButtonPressMask | ButtonReleaseMask | PointerMotionMask );
    XDefineCursor( display(), win, cursor.handle());
    XMapWindow( display(), win );
    input_windows.append( qMakePair( e, win ));
    return win;
    }

Window EffectsHandler::createInputWindow( Effect* e, const QRect& r, const QCursor& cursor )
    {
    return createInputWindow( e, r.x(), r.y(), r.width(), r.height(), cursor );
    }

void EffectsHandler::destroyInputWindow( Window w )
    {
    foreach( InputWindowPair pos, input_windows )
        {
        if( pos.second == w )
            {
            input_windows.removeAll( pos );
            XDestroyWindow( display(), w );
            return;
            }
        }
    assert( false );
    }

bool EffectsHandler::checkInputWindowEvent( XEvent* e )
    {
    if( e->type != ButtonPress && e->type != ButtonRelease && e->type != MotionNotify )
        return false;
    foreach( InputWindowPair pos, input_windows )
        {
        if( pos.second == e->xany.window )
            {
            switch( e->type )
                {
                case ButtonPress:
                case ButtonRelease:
                    {
                    XButtonEvent* e2 = &e->xbutton;
                    QMouseEvent ev( e->type == ButtonPress ? QEvent::MouseButtonPress : QEvent::MouseButtonRelease,
                        QPoint( e2->x, e2->y ), QPoint( e2->x_root, e2->y_root ), x11ToQtMouseButton( e2->button ),
                        x11ToQtMouseButtons( e2->state ), x11ToQtKeyboardModifiers( e2->state ));
                    pos.first->windowInputMouseEvent( pos.second, &ev );
                    break; // --->
                    }
                case MotionNotify:
                    {
                    XMotionEvent* e2 = &e->xmotion;
                    QMouseEvent ev( QEvent::MouseMove, QPoint( e2->x, e2->y ), QPoint( e2->x_root, e2->y_root ),
                        Qt::NoButton, x11ToQtMouseButtons( e2->state ), x11ToQtKeyboardModifiers( e2->state ));
                    pos.first->windowInputMouseEvent( pos.second, &ev );
                    break; // --->
                    }
                }
            return true; // eat event
            }
        }
    return false;
    }

void EffectsHandler::checkInputWindowStacking()
    {
    if( input_windows.count() == 0 )
        return;
    Window* wins = new Window[ input_windows.count() ];
    int pos = 0;
    foreach( InputWindowPair it, input_windows )
        wins[ pos++ ] = it.second;
    XRaiseWindow( display(), wins[ 0 ] );
    XRestackWindows( display(), wins, pos );
    delete[] wins;
    }

void EffectsHandler::activateWindow( EffectWindow* c )
    {
    if( Client* cl = dynamic_cast< Client* >( c->window()))
        Workspace::self()->activateClient( cl, true );
    }

void EffectsHandler::registerEffect( const QString& name, EffectFactory* factory )
    {
    QHash<QString, EffectFactory*>::const_iterator factories_iterator = effect_factories.find(name);
    if( factories_iterator != effect_factories.end() )
        {
        kDebug( 1212 ) << "EffectsHandler::registerEffect : Effect name already registered : " << name << endl;
        }
    else
        {
        kDebug( 1212 ) << "EffectsHandler::registerEffect : Register effect : " << name << endl;
        effect_factories[name] = factory;
        }
    }

void EffectsHandler::loadEffect( const QString& name )
    {
    assert( current_paint_screen == 0 );
    assert( current_paint_window == 0 );
    assert( current_draw_window == 0 );
    assert( current_transform == 0 );

    for(QVector< EffectPair >::const_iterator it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); it++)
        {
        if( (*it).first == name )
            {
            kDebug( 1212 ) << "EffectsHandler::loadEffect : Effect already loaded : " << name << endl;
            return;
            }
        }

    QHash<QString, EffectFactory*>::const_iterator factories_iterator = effect_factories.find(name);
    if( factories_iterator != effect_factories.end() )
        {
        kDebug( 1212 ) << "EffectsHandler::loadEffect : Loading effect : " << name << endl;
        loaded_effects.append( EffectPair( name, factories_iterator.value()->create() ) );
        }
    else
        {
        kDebug( 1212 ) << "EffectsHandler::loadEffect : Unknown effect : " << name << endl;
        }
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
            return;
            }
        }

    kDebug( 1212 ) << "EffectsHandler::unloadEffect : Effect not loaded : " << name << endl;
    }

EffectFactory::~EffectFactory()
    {
    }

int EffectsHandler::currentDesktop() const
    {
    return Workspace::self()->currentDesktop();
    }

EffectsHandler* effects;


//****************************************
// EffectWindow
//****************************************

EffectWindow::EffectWindow()
    : toplevel( NULL )
    , sw( NULL )
    {
    }

void EffectWindow::enablePainting( int reason )
    {
    sceneWindow()->enablePainting( reason );
    }

void EffectWindow::disablePainting( int reason )
    {
    sceneWindow()->disablePainting( reason );
    }

int EffectWindow::desktop() const
    {
    return toplevel->desktop();
    }

bool EffectWindow::isOnCurrentDesktop() const
    {
    return isOnDesktop( effects->currentDesktop());
    }

bool EffectWindow::isOnAllDesktops() const
    {
    return desktop() == NET::OnAllDesktops;
    }

bool EffectWindow::isOnDesktop( int d ) const
    {
    return desktop() == d || isOnAllDesktops();
    }

bool EffectWindow::isDeleted() const
{
    return (dynamic_cast<Deleted*>( toplevel ) != 0);
}

} // namespace
