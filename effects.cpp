/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "effects.h"

#include "deleted.h"
#include "client.h"
#include "group.h"
#include "scene_xrender.h"
#include "scene_opengl.h"
#include "workspace.h"

#include <QFile>

#include "kdebug.h"
#include "klibloader.h"
#include "kdesktopfile.h"
#include "kconfiggroup.h"
#include "kstandarddirs.h"

#include <assert.h>


namespace KWin
{


EffectsHandlerImpl::EffectsHandlerImpl(CompositingType type)
    : EffectsHandler(type)
    , keyboard_grab_effect( NULL )
    {
    foreach( const QString& effect, options->defaultEffects )
        loadEffect( effect );
    }

EffectsHandlerImpl::~EffectsHandlerImpl()
    {
    if( keyboard_grab_effect != NULL )
        ungrabKeyboard();
    foreach( EffectPair ep, loaded_effects )
        unloadEffect( ep.first );
    foreach( InputWindowPair pos, input_windows )
        XDestroyWindow( display(), pos.second );
    }

// the idea is that effects call this function again which calls the next one
void EffectsHandlerImpl::prePaintScreen( int* mask, QRegion* region, int time )
    {
    if( current_paint_screen < loaded_effects.size())
        {
        loaded_effects[current_paint_screen++].second->prePaintScreen( mask, region, time );
        --current_paint_screen;
        }
    // no special final code
    }

void EffectsHandlerImpl::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    if( current_paint_screen < loaded_effects.size())
        {
        loaded_effects[current_paint_screen++].second->paintScreen( mask, region, data );
        --current_paint_screen;
        }
    else
        scene->finalPaintScreen( mask, region, data );
    }

void EffectsHandlerImpl::postPaintScreen()
    {
    if( current_paint_screen < loaded_effects.size())
        {
        loaded_effects[current_paint_screen++].second->postPaintScreen();
        --current_paint_screen;
        }
    // no special final code
    }

void EffectsHandlerImpl::prePaintWindow( EffectWindow* w, int* mask, QRegion* paint, QRegion* clip, int time )
    {
    if( current_paint_window < loaded_effects.size())
        {
        loaded_effects[current_paint_window++].second->prePaintWindow( w, mask, paint, clip, time );
        --current_paint_window;
        }
    // no special final code
    }

void EffectsHandlerImpl::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( current_paint_window < loaded_effects.size())
        {
        loaded_effects[current_paint_window++].second->paintWindow( w, mask, region, data );
        --current_paint_window;
        }
    else
        scene->finalPaintWindow( static_cast<EffectWindowImpl*>( w ), mask, region, data );
    }

void EffectsHandlerImpl::postPaintWindow( EffectWindow* w )
    {
    if( current_paint_window < loaded_effects.size())
        {
        loaded_effects[current_paint_window++].second->postPaintWindow( w );
        --current_paint_window;
        }
    // no special final code
    }

void EffectsHandlerImpl::drawWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( current_draw_window < loaded_effects.size())
        {
        loaded_effects[current_draw_window++].second->drawWindow( w, mask, region, data );
        --current_draw_window;
        }
    else
        scene->finalDrawWindow( static_cast<EffectWindowImpl*>( w ), mask, region, data );
    }

// start another painting pass
void EffectsHandlerImpl::startPaint()
    {
    assert( current_paint_screen == 0 );
    assert( current_paint_window == 0 );
    assert( current_draw_window == 0 );
    assert( current_transform == 0 );
    }

void EffectsHandlerImpl::windowUserMovedResized( EffectWindow* c, bool first, bool last )
    {
    foreach( EffectPair ep, loaded_effects )
        ep.second->windowUserMovedResized( c, first, last );
    }

void EffectsHandlerImpl::windowOpacityChanged( EffectWindow* c, double old_opacity )
    {
    if( static_cast<EffectWindowImpl*>(c)->window()->opacity() == old_opacity )
        return;
    foreach( EffectPair ep, loaded_effects )
        ep.second->windowOpacityChanged( c, old_opacity );
    }

void EffectsHandlerImpl::windowAdded( EffectWindow* c )
    {
    foreach( EffectPair ep, loaded_effects )
        ep.second->windowAdded( c );
    }

void EffectsHandlerImpl::windowDeleted( EffectWindow* c )
    {
    foreach( EffectPair ep, loaded_effects )
        ep.second->windowDeleted( c );
    }

void EffectsHandlerImpl::windowClosed( EffectWindow* c )
    {
    foreach( EffectPair ep, loaded_effects )
        ep.second->windowClosed( c );
    }

void EffectsHandlerImpl::windowActivated( EffectWindow* c )
    {
    foreach( EffectPair ep, loaded_effects )
        ep.second->windowActivated( c );
    }

void EffectsHandlerImpl::windowMinimized( EffectWindow* c )
    {
    foreach( EffectPair ep, loaded_effects )
        ep.second->windowMinimized( c );
    }

void EffectsHandlerImpl::windowUnminimized( EffectWindow* c )
    {
    foreach( EffectPair ep, loaded_effects )
        ep.second->windowUnminimized( c );
    }

void EffectsHandlerImpl::desktopChanged( int old )
    {
    foreach( EffectPair ep, loaded_effects )
        ep.second->desktopChanged( old );
    }

void EffectsHandlerImpl::windowDamaged( EffectWindow* w, const QRect& r )
    {
    if( w == NULL )
        return;
    foreach( EffectPair ep, loaded_effects )
        ep.second->windowDamaged( w, r );
    }

void EffectsHandlerImpl::windowGeometryShapeChanged( EffectWindow* w, const QRect& old )
    {
    if( w == NULL ) // during late cleanup effectWindow() may be already NULL
        return;     // in some functions that may still call this
    foreach( EffectPair ep, loaded_effects )
        ep.second->windowGeometryShapeChanged( w, old );
    }

void EffectsHandlerImpl::tabBoxAdded( int mode )
    {
    foreach( EffectPair ep, loaded_effects )
        ep.second->tabBoxAdded( mode );
    }

void EffectsHandlerImpl::tabBoxClosed()
    {
    foreach( EffectPair ep, loaded_effects )
        ep.second->tabBoxClosed();
    }

void EffectsHandlerImpl::tabBoxUpdated()
    {
    foreach( EffectPair ep, loaded_effects )
        ep.second->tabBoxUpdated();
    }

bool EffectsHandlerImpl::borderActivated( ElectricBorder border )
    {
    bool ret = false;
    foreach( EffectPair ep, loaded_effects )
        if( ep.second->borderActivated( border ))
            ret = true; // bail out or tell all?
    return ret;
    }

void EffectsHandlerImpl::cursorMoved( const QPoint& pos, Qt::MouseButtons buttons )
    {
    foreach( EffectPair ep, loaded_effects )
        ep.second->cursorMoved( pos, buttons );
    }

bool EffectsHandlerImpl::grabKeyboard( Effect* effect )
    {
    if( keyboard_grab_effect != NULL )
        return false;
    bool ret = grabXKeyboard();
    if( !ret )
        return false;
    keyboard_grab_effect = effect;
    return true;
    }

void EffectsHandlerImpl::ungrabKeyboard()
    {
    assert( keyboard_grab_effect != NULL );
    ungrabXKeyboard();
    keyboard_grab_effect = NULL;
    }

void EffectsHandlerImpl::grabbedKeyboardEvent( QKeyEvent* e )
    {
    if( keyboard_grab_effect != NULL )
        keyboard_grab_effect->grabbedKeyboardEvent( e );
    }

bool EffectsHandlerImpl::hasKeyboardGrab() const
    {
    return keyboard_grab_effect != NULL;
    }

void EffectsHandlerImpl::activateWindow( EffectWindow* c )
    {
    if( Client* cl = dynamic_cast< Client* >( static_cast<EffectWindowImpl*>(c)->window()))
        Workspace::self()->activateClient( cl, true );
    }

EffectWindow* EffectsHandlerImpl::activeWindow() const
    {
    return Workspace::self()->activeClient() ? Workspace::self()->activeClient()->effectWindow() : NULL;
    }

void EffectsHandlerImpl::moveWindow( EffectWindow* w, const QPoint& pos )
    {
    Client* cl = dynamic_cast< Client* >( static_cast<EffectWindowImpl*>(w)->window());
    if( cl && cl->isMovable())
        cl->move( pos );
    }

void EffectsHandlerImpl::windowToDesktop( EffectWindow* w, int desktop )
    {
    Client* cl = dynamic_cast< Client* >( static_cast<EffectWindowImpl*>(w)->window());
    if( cl && cl->isMovable())
        Workspace::self()->sendClientToDesktop( cl, desktop, true );
    }

int EffectsHandlerImpl::currentDesktop() const
    {
    return Workspace::self()->currentDesktop();
    }

int EffectsHandlerImpl::numberOfDesktops() const
    {
    return Workspace::self()->numberOfDesktops();
    }

void EffectsHandlerImpl::setCurrentDesktop( int desktop )
    {
    Workspace::self()->setCurrentDesktop( desktop );
    }

QString EffectsHandlerImpl::desktopName( int desktop ) const
    {
    return Workspace::self()->desktopName( desktop );
    }

void EffectsHandlerImpl::calcDesktopLayout(int* x, int* y, Qt::Orientation* orientation) const
    {
    Workspace::self()->calcDesktopLayout( x, y, orientation );
    }

bool EffectsHandlerImpl::optionRollOverDesktops() const
    {
    return options->rollOverDesktops;
    }

int EffectsHandlerImpl::displayWidth() const
    {
    return KWin::displayWidth();
    }

int EffectsHandlerImpl::displayHeight() const
    {
    return KWin::displayWidth();
    }

EffectWindowList EffectsHandlerImpl::stackingOrder() const
    {
    ClientList list = Workspace::self()->stackingOrder();
    EffectWindowList ret;
    foreach( Client* c, list )
        ret.append( effectWindow( c ));
    return ret;
    }

void EffectsHandlerImpl::setTabBoxWindow(EffectWindow* w)
    {
    if( Client* c = dynamic_cast< Client* >( static_cast< EffectWindowImpl* >( w )->window()))
        Workspace::self()->setTabBoxClient( c );
    }

void EffectsHandlerImpl::setTabBoxDesktop(int desktop)
    {
    Workspace::self()->setTabBoxDesktop( desktop );
    }

EffectWindowList EffectsHandlerImpl::currentTabBoxWindowList() const
    {
    EffectWindowList ret;
    ClientList clients = Workspace::self()->currentTabBoxClientList();
    foreach( Client* c, clients )
        ret.append( c->effectWindow());
    return ret;
    }

void EffectsHandlerImpl::refTabBox()
    {
    Workspace::self()->refTabBox();
    }

void EffectsHandlerImpl::unrefTabBox()
    {
    Workspace::self()->unrefTabBox();
    }

void EffectsHandlerImpl::closeTabBox()
    {
    Workspace::self()->closeTabBox();
    }

QList< int > EffectsHandlerImpl::currentTabBoxDesktopList() const
    {
    return Workspace::self()->currentTabBoxDesktopList();
    }

int EffectsHandlerImpl::currentTabBoxDesktop() const
    {
    return Workspace::self()->currentTabBoxDesktop();
    }

EffectWindow* EffectsHandlerImpl::currentTabBoxWindow() const
    {
    if( Client* c = Workspace::self()->currentTabBoxClient())
        return c->effectWindow();
    return NULL;
    }

void EffectsHandlerImpl::addRepaintFull()
    {
    Workspace::self()->addRepaintFull();
    }

void EffectsHandlerImpl::addRepaint( const QRect& r )
    {
    Workspace::self()->addRepaint( r );
    }

void EffectsHandlerImpl::addRepaint( int x, int y, int w, int h )
    {
    Workspace::self()->addRepaint( x, y, w, h );
    }

QRect EffectsHandlerImpl::clientArea( clientAreaOption opt, const QPoint& p, int desktop ) const
    {
    return Workspace::self()->clientArea( opt, p, desktop );
    }

Window EffectsHandlerImpl::createInputWindow( Effect* e, int x, int y, int w, int h, const QCursor& cursor )
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

void EffectsHandlerImpl::destroyInputWindow( Window w )
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

bool EffectsHandlerImpl::checkInputWindowEvent( XEvent* e )
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
                    {
                    XButtonEvent* e2 = &e->xbutton;
                    Qt::MouseButton button = x11ToQtMouseButton( e2->button );
                    Qt::MouseButtons buttons = x11ToQtMouseButtons( e2->state ) | button;
                    QMouseEvent ev( QEvent::MouseButtonPress,
                        QPoint( e2->x, e2->y ), QPoint( e2->x_root, e2->y_root ),
                        button, buttons, x11ToQtKeyboardModifiers( e2->state ));
                    pos.first->windowInputMouseEvent( pos.second, &ev );
                    break; // --->
                    }
                case ButtonRelease:
                    {
                    XButtonEvent* e2 = &e->xbutton;
                    Qt::MouseButton button = x11ToQtMouseButton( e2->button );
                    Qt::MouseButtons buttons = x11ToQtMouseButtons( e2->state ) & ~button;
                    QMouseEvent ev( QEvent::MouseButtonRelease,
                        QPoint( e2->x, e2->y ), QPoint( e2->x_root, e2->y_root ),
                        button, buttons, x11ToQtKeyboardModifiers( e2->state ));
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

void EffectsHandlerImpl::checkInputWindowStacking()
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

QPoint EffectsHandlerImpl::cursorPos() const
    {
    return Workspace::self()->cursorPos();
    }

void EffectsHandlerImpl::checkElectricBorder(const QPoint &pos, Time time)
    {
    Workspace::self()->checkElectricBorder( pos, time );
    }

void EffectsHandlerImpl::reserveElectricBorder( ElectricBorder border )
    {
    Workspace::self()->reserveElectricBorder( border );
    }

void EffectsHandlerImpl::unreserveElectricBorder( ElectricBorder border )
    {
    Workspace::self()->unreserveElectricBorder( border );
    }

void EffectsHandlerImpl::reserveElectricBorderSwitching( bool reserve )
    {
    Workspace::self()->reserveElectricBorderSwitching( reserve );
    }

unsigned long EffectsHandlerImpl::xrenderBufferPicture()
    {
#ifdef HAVE_XRENDER
    if( SceneXrender* s = dynamic_cast< SceneXrender* >( scene ))
        return s->bufferPicture();
#endif
    return None;
    }

KLibrary* EffectsHandlerImpl::findEffectLibrary( const QString& effectname )
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

void EffectsHandlerImpl::loadEffect( const QString& name )
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

void EffectsHandlerImpl::unloadEffect( const QString& name )
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


//****************************************
// EffectWindowImpl
//****************************************

EffectWindowImpl::EffectWindowImpl() : EffectWindow()
    , toplevel( NULL )
    , sw( NULL )
    {
    }

EffectWindowImpl::~EffectWindowImpl()
    {
    }

void EffectWindowImpl::enablePainting( int reason )
    {
    sceneWindow()->enablePainting( reason );
    }

void EffectWindowImpl::disablePainting( int reason )
    {
    sceneWindow()->disablePainting( reason );
    }

void EffectWindowImpl::addRepaint( const QRect& r )
    {
    toplevel->addRepaint( r );
    }

void EffectWindowImpl::addRepaint( int x, int y, int w, int h )
    {
    toplevel->addRepaint( x, y, w, h );
    }

void EffectWindowImpl::addRepaintFull()
    {
    toplevel->addRepaintFull();
    }

int EffectWindowImpl::desktop() const
    {
    return toplevel->desktop();
    }

bool EffectWindowImpl::isOnAllDesktops() const
    {
    return desktop() == NET::OnAllDesktops;
    }

QString EffectWindowImpl::caption() const
    {
    if( Client* c = dynamic_cast<Client*>( toplevel ))
        return c->caption();
    else
        return "";
    }

QString EffectWindowImpl::windowClass() const
    {
    return toplevel->resourceName() + ' ' + toplevel->resourceClass();
    }

QPixmap EffectWindowImpl::icon() const
    {
    if( Client* c = dynamic_cast<Client*>( toplevel ))
        return c->icon();
    return QPixmap(); // TODO
    }

const EffectWindowGroup* EffectWindowImpl::group() const
    {
    if( Client* c = dynamic_cast< Client* >( toplevel ))
        return c->group()->effectGroup();
    return NULL; // TODO
    }

bool EffectWindowImpl::isMinimized() const
    {
    if( Client* c = dynamic_cast<Client*>( toplevel ))
        return c->isMinimized();
    else
        return false;
    }

double EffectWindowImpl::opacity() const
    {
    return toplevel->opacity();
    }

bool EffectWindowImpl::isDeleted() const
    {
    return (dynamic_cast<Deleted*>( toplevel ) != 0);
    }

void EffectWindowImpl::refWindow()
    {
    if( Deleted* d = dynamic_cast< Deleted* >( toplevel ))
        return d->refWindow();
    abort(); // TODO
    }

void EffectWindowImpl::unrefWindow()
    {
    if( Deleted* d = dynamic_cast< Deleted* >( toplevel ))
        return d->unrefWindow();
    abort(); // TODO
    }

const Toplevel* EffectWindowImpl::window() const
    {
    return toplevel;
    }

Toplevel* EffectWindowImpl::window()
    {
    return toplevel;
    }

void EffectWindowImpl::setWindow( Toplevel* w )
    {
    toplevel = w;
    }

void EffectWindowImpl::setSceneWindow( Scene::Window* w )
    {
    sw = w;
    }

Scene::Window* EffectWindowImpl::sceneWindow()
    {
    return sw;
    }

int EffectWindowImpl::x() const
    {
    return toplevel->x();
    }

int EffectWindowImpl::y() const
    {
    return toplevel->y();
    }

int EffectWindowImpl::width() const
    {
    return toplevel->width();
    }

int EffectWindowImpl::height() const
    {
    return toplevel->height();
    }

QRect EffectWindowImpl::geometry() const
    {
    return toplevel->geometry();
    }

QSize EffectWindowImpl::size() const
    {
    return toplevel->size();
    }

QPoint EffectWindowImpl::pos() const
    {
    return toplevel->pos();
    }

QRect EffectWindowImpl::rect() const
    {
    return toplevel->rect();
    }

bool EffectWindowImpl::isMovable() const
    {
    if( Client* c = dynamic_cast< Client* >( toplevel ))
        return c->isMovable();
    return false;
    }

bool EffectWindowImpl::isUserMove() const
    {
    if( Client* c = dynamic_cast< Client* >( toplevel ))
        return c->isMove();
    return false;
    }

bool EffectWindowImpl::isUserResize() const
    {
    if( Client* c = dynamic_cast< Client* >( toplevel ))
        return c->isResize();
    return false;
    }

QRect EffectWindowImpl::iconGeometry() const
    {
    if( Client* c = dynamic_cast< Client* >( toplevel ))
        return c->iconGeometry();
    return QRect();
    }

bool EffectWindowImpl::isDesktop() const
    {
    return toplevel->isDesktop();
    }

bool EffectWindowImpl::isDock() const
    {
    return toplevel->isDock();
    }

bool EffectWindowImpl::isToolbar() const
    {
    return toplevel->isToolbar();
    }

bool EffectWindowImpl::isTopMenu() const
    {
    return toplevel->isTopMenu();
    }

bool EffectWindowImpl::isMenu() const
    {
    return toplevel->isMenu();
    }

bool EffectWindowImpl::isNormalWindow() const
    {
    return toplevel->isNormalWindow();
    }

bool EffectWindowImpl::isSpecialWindow() const
    {
    if( Client* c = dynamic_cast<Client*>( toplevel ))
        return c->isSpecialWindow();
    else
        return false;
    }

bool EffectWindowImpl::isDialog() const
    {
    return toplevel->isDialog();
    }

bool EffectWindowImpl::isSplash() const
    {
    return toplevel->isSplash();
    }

bool EffectWindowImpl::isUtility() const
    {
    return toplevel->isUtility();
    }

bool EffectWindowImpl::isDropdownMenu() const
    {
    return toplevel->isDropdownMenu();
    }

bool EffectWindowImpl::isPopupMenu() const
    {
    return toplevel->isPopupMenu();
    }

bool EffectWindowImpl::isTooltip() const
    {
    return toplevel->isTooltip();
    }

bool EffectWindowImpl::isNotification() const
    {
    return toplevel->isNotification();
    }

bool EffectWindowImpl::isComboBox() const
    {
    return toplevel->isComboBox();
    }

bool EffectWindowImpl::isDNDIcon() const
    {
    return toplevel->isDNDIcon();
    }

bool EffectWindowImpl::isModal() const
    {
    if( Client* c = dynamic_cast< Client* >( toplevel ))
        return c->isModal();
    return false;
    }

EffectWindow* EffectWindowImpl::findModal()
    {
    if( Client* c = dynamic_cast< Client* >( toplevel ))
        {
        if( Client* c2 = c->findModal())
            return c2->effectWindow();
        }
    return NULL;
    }

EffectWindowList EffectWindowImpl::mainWindows() const
    {
    if( Client* c = dynamic_cast< Client* >( toplevel ))
        {
        EffectWindowList ret;
        ClientList mainclients = c->mainClients();
        foreach( Client* tmp, mainclients )
            ret.append( tmp->effectWindow());
        return ret;
        }
    return EffectWindowList();
    }

QVector<Vertex>& EffectWindowImpl::vertices()
    {
    if( SceneOpenGL::Window* w = dynamic_cast< SceneOpenGL::Window* >( sceneWindow()))
        return w->vertices();
    abort(); // TODO
    }

void EffectWindowImpl::requestVertexGrid(int maxquadsize)
    {
    if( SceneOpenGL::Window* w = dynamic_cast< SceneOpenGL::Window* >( sceneWindow()))
        return w->requestVertexGrid( maxquadsize );
    abort(); // TODO
    }

void EffectWindowImpl::markVerticesDirty()
    {
    if( SceneOpenGL::Window* w = dynamic_cast< SceneOpenGL::Window* >( sceneWindow()))
        return w->markVerticesDirty();
    abort(); // TODO
    }

EffectWindow* effectWindow( Toplevel* w )
    {
    EffectWindowImpl* ret = w->effectWindow();
    ret->setSceneWindow( NULL ); // just in case
    return ret;
    }

EffectWindow* effectWindow( Scene::Window* w )
    {
    EffectWindowImpl* ret = w->window()->effectWindow();
    ret->setSceneWindow( w );
    return ret;
    }

//****************************************
// EffectWindowGroupImpl
//****************************************

 
EffectWindowList EffectWindowGroupImpl::members() const
    {
    EffectWindowList ret;
    foreach( Toplevel* c, group->members())
        ret.append( c->effectWindow());
    return ret;
    }

} // namespace
