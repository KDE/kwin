/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#ifndef KWIN_EFFECTSIMPL_H
#define KWIN_EFFECTSIMPL_H

#include "kwineffects.h"

#include "scene.h"

#include <QStack>
#include <QMap>


class KService;


namespace KWin
{

class EffectsHandlerImpl : public EffectsHandler
{
    public:
        EffectsHandlerImpl(CompositingType type);
        virtual ~EffectsHandlerImpl();
        virtual void prePaintScreen( ScreenPrePaintData& data, int time );
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        virtual void postPaintScreen();
        virtual void prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual void postPaintWindow( EffectWindow* w );

        virtual void drawWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );

        virtual void activateWindow( EffectWindow* c );
        virtual EffectWindow* activeWindow() const;
        virtual void moveWindow( EffectWindow* w, const QPoint& pos );
        virtual void windowToDesktop( EffectWindow* w, int desktop );

        virtual int currentDesktop() const;
        virtual int numberOfDesktops() const;
        virtual void setCurrentDesktop( int desktop );
        virtual QString desktopName( int desktop ) const;
        virtual int displayWidth() const;
        virtual int displayHeight() const;
        virtual QPoint cursorPos() const;
        virtual bool grabKeyboard( Effect* effect );
        virtual void ungrabKeyboard();
        virtual EffectWindowList stackingOrder() const;
        virtual void setElevatedWindow( EffectWindow* w, bool set );

        virtual void setTabBoxWindow(EffectWindow*);
        virtual void setTabBoxDesktop(int);
        virtual EffectWindowList currentTabBoxWindowList() const;
        virtual void refTabBox();
        virtual void unrefTabBox();
        virtual void closeTabBox();
        virtual QList< int > currentTabBoxDesktopList() const;
        virtual int currentTabBoxDesktop() const;
        virtual EffectWindow* currentTabBoxWindow() const;

        virtual void setActiveFullScreenEffect( Effect* e );
        virtual Effect* activeFullScreenEffect() const;

        virtual void pushRenderTarget(GLRenderTarget* target);
        virtual GLRenderTarget* popRenderTarget();

        virtual void addRepaintFull();
        virtual void addRepaint( const QRect& r );
        virtual void addRepaint( int x, int y, int w, int h );
        virtual int activeScreen() const;
        virtual QRect clientArea( clientAreaOption, int screen, int desktop ) const;
        virtual QRect clientArea( clientAreaOption, const EffectWindow* c ) const;
        virtual QRect clientArea( clientAreaOption, const QPoint& p, int desktop ) const;
        virtual void calcDesktopLayout(int* x, int* y, Qt::Orientation* orientation) const;
        virtual bool optionRollOverDesktops() const;
        virtual int desktopToLeft( int desktop, bool wrap ) const;
        virtual int desktopToRight( int desktop, bool wrap ) const;
        virtual int desktopUp( int desktop, bool wrap ) const;
        virtual int desktopDown( int desktop, bool wrap ) const;

        virtual Window createInputWindow( Effect* e, int x, int y, int w, int h, const QCursor& cursor );
        using EffectsHandler::createInputWindow;
        virtual void destroyInputWindow( Window w );
        virtual bool checkInputWindowEvent( XEvent* e );
        virtual void checkInputWindowStacking();

        virtual void checkElectricBorder(const QPoint &pos, Time time);
        virtual void reserveElectricBorder( ElectricBorder border );
        virtual void unreserveElectricBorder( ElectricBorder border );
        virtual void reserveElectricBorderSwitching( bool reserve );

        virtual unsigned long xrenderBufferPicture();
        virtual void reconfigure();
        virtual void registerPropertyType( long atom, bool reg );

        // internal (used by kwin core or compositing code)
        void startPaint();
        void windowUserMovedResized( EffectWindow* c, bool first, bool last );
        void windowOpacityChanged( EffectWindow* c, double old_opacity );
        void windowAdded( EffectWindow* c );
        void windowClosed( EffectWindow* c );
        void windowDeleted( EffectWindow* c );
        void windowActivated( EffectWindow* c );
        void windowMinimized( EffectWindow* c );
        void windowUnminimized( EffectWindow* c );
        void desktopChanged( int old );
        void windowDamaged( EffectWindow* w, const QRect& r );
        void windowGeometryShapeChanged( EffectWindow* w, const QRect& old );
        void tabBoxAdded( int mode );
        void tabBoxClosed();
        void tabBoxUpdated();
        bool borderActivated( ElectricBorder border );
        void mouseChanged( const QPoint& pos, const QPoint& old,
            Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers );
        void grabbedKeyboardEvent( QKeyEvent* e );
        bool hasKeyboardGrab() const;
        void propertyNotify( EffectWindow* c, long atom );

        bool loadEffect( const QString& name );
        void toggleEffect( const QString& name );
        void unloadEffect( const QString& name );
        void reloadEffect( const QString& name );
        bool isEffectLoaded( const QString& name );

        ToplevelList elevatedWindows() const;

    protected:
        KLibrary* findEffectLibrary( KService* service );
        void effectsChanged();

        Effect* keyboard_grab_effect;
        QStack<GLRenderTarget*> render_targets;
        Effect* fullscreen_effect;
        ToplevelList elevated_windows;
        QMultiMap< int, EffectPair > effect_order;
        QHash< long, int > registered_atoms;
};

class EffectWindowImpl : public EffectWindow
{
    public:
        EffectWindowImpl();
        virtual ~EffectWindowImpl();

        virtual void enablePainting( int reason );
        virtual void disablePainting( int reason );
        virtual bool isPaintingEnabled();
        virtual void addRepaint( const QRect& r );
        virtual void addRepaint( int x, int y, int w, int h );
        virtual void addRepaintFull();

        virtual void refWindow();
        virtual void unrefWindow();
        virtual bool isDeleted() const;

        virtual bool isOnAllDesktops() const;
        virtual int desktop() const; // prefer isOnXXX()
        virtual bool isMinimized() const;
        virtual double opacity() const;
        virtual QString caption() const;
        virtual QPixmap icon() const;
        virtual QString windowClass() const;
        virtual QString windowRole() const;
        virtual const EffectWindowGroup* group() const;

        virtual int x() const;
        virtual int y() const;
        virtual int width() const;
        virtual int height() const;
        virtual QRect geometry() const;
        virtual QRegion shape() const;
        virtual QPoint pos() const;
        virtual QSize size() const;
        virtual QRect rect() const;
        virtual bool isMovable() const;
        virtual bool isUserMove() const;
        virtual bool isUserResize() const;
        virtual QRect iconGeometry() const;
        virtual QRect contentsRect() const;
        virtual QByteArray readProperty( long atom, long type, int format ) const;

        virtual bool isDesktop() const;
        virtual bool isDock() const;
        virtual bool isToolbar() const;
        virtual bool isTopMenu() const;
        virtual bool isMenu() const;
        virtual bool isNormalWindow() const; // normal as in 'NET::Normal or NET::Unknown non-transient'
        virtual bool isSpecialWindow() const;
        virtual bool isDialog() const;
        virtual bool isSplash() const;
        virtual bool isUtility() const;
        virtual bool isDropdownMenu() const;
        virtual bool isPopupMenu() const; // a context popup, not dropdown, not torn-off
        virtual bool isTooltip() const;
        virtual bool isNotification() const;
        virtual bool isComboBox() const;
        virtual bool isDNDIcon() const;
        virtual bool isManaged() const; // managed or override-redirect

        virtual bool isModal() const;
        virtual EffectWindow* findModal();
        virtual EffectWindowList mainWindows() const;

        virtual WindowQuadList buildQuads() const;

        const Toplevel* window() const;
        Toplevel* window();

        void setWindow( Toplevel* w ); // internal
        void setSceneWindow( Scene::Window* w ); // internal
        const Scene::Window* sceneWindow() const; // internal
        Scene::Window* sceneWindow(); // internal
    private:
        Toplevel* toplevel;
        Scene::Window* sw; // This one is used only during paint pass.
};

class EffectWindowGroupImpl
    : public EffectWindowGroup
    {
    public:
        EffectWindowGroupImpl( Group* g );
        virtual EffectWindowList members() const;
    private:
        Group* group;
    };


inline
ToplevelList EffectsHandlerImpl::elevatedWindows() const
    {
    return elevated_windows;
    }


inline
EffectWindowGroupImpl::EffectWindowGroupImpl( Group* g )
    : group( g )
    {
    }

EffectWindow* effectWindow( Toplevel* w );
EffectWindow* effectWindow( Scene::Window* w );

inline
const Scene::Window* EffectWindowImpl::sceneWindow() const
    {
    return sw;
    }

inline
Scene::Window* EffectWindowImpl::sceneWindow()
    {
    return sw;
    }

inline
const Toplevel* EffectWindowImpl::window() const
    {
    return toplevel;
    }

inline
Toplevel* EffectWindowImpl::window()
    {
    return toplevel;
    }


} // namespace

#endif
