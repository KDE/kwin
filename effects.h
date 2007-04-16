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
        virtual EffectWindow* activeWindow() const;

        virtual int currentDesktop() const;
        virtual int numberOfDesktops() const;
        virtual QString desktopName( int desktop ) const;
        virtual int displayWidth() const;
        virtual int displayHeight() const;
        virtual QPoint cursorPos() const;
        virtual bool grabKeyboard( Effect* effect );
        virtual void ungrabKeyboard();
        virtual EffectWindowList stackingOrder() const;

        virtual void setTabBoxWindow(EffectWindow*);
        virtual void setTabBoxDesktop(int);
        virtual EffectWindowList currentTabBoxWindowList() const;
        virtual void refTabBox();
        virtual void unrefTabBox();
        virtual void closeTabBox();
        virtual QList< int > currentTabBoxDesktopList() const;
        virtual int currentTabBoxDesktop() const;
        virtual EffectWindow* currentTabBoxWindow() const;

        virtual void addRepaintFull();
        virtual void addRepaint( const QRect& r );
        virtual void addRepaint( int x, int y, int w, int h );
        virtual QRect clientArea( clientAreaOption opt, const QPoint& p, int desktop ) const;
        virtual void calcDesktopLayout(int* x, int* y, Qt::Orientation* orientation) const;
        virtual bool optionRollOverDesktops() const;

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
        void cursorMoved( const QPoint& pos, Qt::MouseButtons buttons );
        void grabbedKeyboardEvent( QKeyEvent* e );
        bool hasKeyboardGrab() const;

        void loadEffect( const QString& name );
        void unloadEffect( const QString& name );

    protected:
        KLibrary* findEffectLibrary( const QString& effectname );
        Effect* keyboard_grab_effect;
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
        virtual const EffectWindowGroup* group() const;

        virtual int x() const;
        virtual int y() const;
        virtual int width() const;
        virtual int height() const;
        virtual QRect geometry() const;
        virtual QPoint pos() const;
        virtual QSize size() const;
        virtual QRect rect() const;
        virtual bool isUserMove() const;
        virtual bool isUserResize() const;
        virtual QRect iconGeometry() const;

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

        virtual bool isModal() const;
        virtual EffectWindow* findModal();
        virtual EffectWindowList mainWindows() const;

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
