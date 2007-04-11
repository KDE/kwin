/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_EFFECTSIMPL_H
#define KWIN_EFFECTSIMPL_H

#include "kwineffects.h"

#include "scene.h"



namespace KWin
{

class EffectsHandlerImpl : public EffectsHandler
{
    public:
        EffectsHandlerImpl(CompositingType type);
        virtual ~EffectsHandlerImpl();
        virtual void prePaintScreen( int* mask, QRegion* region, int time );
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        virtual void postPaintScreen();
        virtual void prePaintWindow( EffectWindow* w, int* mask, QRegion* paint, QRegion* clip, int time );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual void postPaintWindow( EffectWindow* w );

        virtual void drawWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );

        virtual void activateWindow( EffectWindow* c );

        virtual int currentDesktop() const;
        virtual int displayWidth() const;
        virtual int displayHeight() const;
        virtual QPoint cursorPos() const;
        virtual EffectWindowList stackingOrder() const;
        virtual void addRepaintFull();
        virtual void addRepaint( const QRect& r );
        virtual void addRepaint( int x, int y, int w, int h );
        virtual QRect clientArea( clientAreaOption opt, const QPoint& p, int desktop ) const;

        virtual Window createInputWindow( Effect* e, int x, int y, int w, int h, const QCursor& cursor );
        virtual void destroyInputWindow( Window w );
        virtual bool checkInputWindowEvent( XEvent* e );
        virtual void checkInputWindowStacking();

        virtual void checkElectricBorder(const QPoint &pos, Time time);
        virtual void reserveElectricBorder( ElectricBorder border );
        virtual void unreserveElectricBorder( ElectricBorder border );
        virtual void reserveElectricBorderSwitching( bool reserve );

        virtual unsigned long xrenderBufferPicture();

        // internal (used by kwin core or compositing code)
        virtual void startPaint();
        virtual void windowUserMovedResized( EffectWindow* c, bool first, bool last );
        virtual void windowOpacityChanged( EffectWindow* c, double old_opacity );
        virtual void windowAdded( EffectWindow* c );
        virtual void windowClosed( EffectWindow* c );
        virtual void windowDeleted( EffectWindow* c );
        virtual void windowActivated( EffectWindow* c );
        virtual void windowMinimized( EffectWindow* c );
        virtual void windowUnminimized( EffectWindow* c );
        virtual void desktopChanged( int old );
        virtual void windowDamaged( EffectWindow* w, const QRect& r );
        virtual void windowGeometryShapeChanged( EffectWindow* w, const QRect& old );
        virtual void tabBoxAdded( int mode );
        virtual void tabBoxClosed();
        virtual void tabBoxUpdated();
        virtual bool borderActivated( ElectricBorder border );

        void loadEffect( const QString& name );
        void unloadEffect( const QString& name );

    protected:
        KLibrary* findEffectLibrary( const QString& effectname );
};

class EffectWindowImpl : public EffectWindow
{
    public:
        EffectWindowImpl();
        virtual ~EffectWindowImpl();

        virtual void enablePainting( int reason );
        virtual void disablePainting( int reason );
        virtual void addRepaint( const QRect& r );
        virtual void addRepaint( int x, int y, int w, int h );
        virtual void addRepaintFull();

        virtual bool isDeleted() const;

        virtual bool isOnAllDesktops() const;
        virtual int desktop() const; // prefer isOnXXX()
        virtual bool isMinimized() const;
        virtual QString caption() const;
        virtual const EffectWindowGroup* group() const;

        virtual int x() const;
        virtual int y() const;
        virtual int width() const;
        virtual int height() const;
        virtual QRect geometry() const;
        virtual QPoint pos() const;
        virtual QSize size() const;
        virtual QRect rect() const;

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

        virtual QVector<Vertex>& vertices();
        virtual void requestVertexGrid(int maxquadsize);
        virtual void markVerticesDirty();

        const Toplevel* window() const;
        Toplevel* window();

        void setWindow( Toplevel* w ); // internal
        void setSceneWindow( Scene::Window* w ); // internal
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
EffectWindowGroupImpl::EffectWindowGroupImpl( Group* g )
    : group( g )
    {
    }

EffectWindow* effectWindow( Toplevel* w );
EffectWindow* effectWindow( Scene::Window* w );


} // namespace

#endif
