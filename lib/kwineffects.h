/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

// TODO MIT or some other licence, perhaps move to some lib

#ifndef KWIN_LIB_EFFECTS_H
#define KWIN_LIB_EFFECTS_H

#include <kwinglobals.h>

#include <qpair.h>
#include <QRect>
#include <QRegion>
#include <QString>
#include <QVector>
#include <QList>
#include <QHash>

#include <stdio.h>

class KLibrary;
class QKeyEvent;

namespace KWin
{


class EffectWindow;
class EffectWindowGroup;
class Effect;
class Vertex;

typedef QPair< QString, Effect* > EffectPair;
typedef QPair< Effect*, Window > InputWindowPair;
typedef QList< EffectWindow* > EffectWindowList;


class KWIN_EXPORT Effect
    {
    public:
        // Flags controlling how painting is done.
        // TODO: is that ok here?
        enum
        {
            // Window (or at least part of it) will be painted opaque.
            PAINT_WINDOW_OPAQUE         = 1 << 0,
            // Window (or at least part of it) will be painted translucent.
            PAINT_WINDOW_TRANSLUCENT    = 1 << 1,
            // Window will be painted with transformed geometry.
            PAINT_WINDOW_TRANSFORMED    = 1 << 2,
            // Paint only a region of the screen (can be optimized, cannot
            // be used together with TRANSFORMED flags).
            PAINT_SCREEN_REGION         = 1 << 3,
            // Whole screen will be painted with transformed geometry.
            PAINT_SCREEN_TRANSFORMED    = 1 << 4,
            // At least one window will be painted with transformed geometry.
            PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS = 1 << 5,
            // Clear whole background as the very first step, without optimizing it
            PAINT_SCREEN_BACKGROUND_FIRST = 1 << 6
        };

        Effect();
        virtual ~Effect();

        virtual void prePaintScreen( int* mask, QRegion* region, int time );
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        virtual void postPaintScreen();
        virtual void prePaintWindow( EffectWindow* w, int* mask, QRegion* paint, QRegion* clip, int time );
        // paintWindow() can do various transformations
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual void postPaintWindow( EffectWindow* w );

        // drawWindow() is used even for thumbnails etc. - it can alter the window itself where it
        // makes sense (e.g.darkening out unresponsive windows), but it cannot do transformations
        virtual void drawWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        // This function is used e.g. by the shadow effect which adds area around windows
        // that needs to be painted as well - e.g. when a window is hidden and the workspace needs
        // to be repainted at that area, shadow's transformWindowDamage() adds the shadow area
        // to it, so that it is repainted as well.
        virtual QRect transformWindowDamage( EffectWindow* w, const QRect& r );

        // called when moved/resized or once after it's finished
        virtual void windowUserMovedResized( EffectWindow* c, bool first, bool last );
        virtual void windowOpacityChanged( EffectWindow* c, double old_opacity );
        virtual void windowAdded( EffectWindow* c );
        virtual void windowClosed( EffectWindow* c );
        virtual void windowDeleted( EffectWindow* c );
        virtual void windowActivated( EffectWindow* c );
        virtual void windowMinimized( EffectWindow* c );
        virtual void windowUnminimized( EffectWindow* c );
        virtual void windowInputMouseEvent( Window w, QEvent* e );
        virtual void desktopChanged( int old );
        virtual void windowDamaged( EffectWindow* w, const QRect& r );
        virtual void windowGeometryShapeChanged( EffectWindow* w, const QRect& old );
        virtual void cursorMoved( const QPoint& pos, Qt::MouseButtons buttons );
        virtual void grabbedKeyboardEvent( QKeyEvent* e );

        virtual void tabBoxAdded( int mode );
        virtual void tabBoxClosed();
        virtual void tabBoxUpdated();
        virtual bool borderActivated( ElectricBorder border );

        static int displayWidth();
        static int displayHeight();
        static QPoint cursorPos();

        // Interpolates between x and y
        static float interpolate(float x, float y, float a)
            {
            return x * (1 - a) + y * a;
            }
        // helper to set WindowPaintData and QRegion to necessary transformations so that
        // a following drawWindow() would put the window at the requested geometry (useful for thumbnails)
        static void setPositionTransformations( WindowPaintData& data, QRect& region, EffectWindow* w,
            const QRect& r, Qt::AspectRatioMode aspect );
    };


/**
 * Defines the class to be used for effect with given name
 * E.g.  KWIN_EFFECT( Flames, MyFlameEffect )
 * In this case object of MyFlameEffect class would be created when effect
 *  "Flames" is loaded.
 **/
#define KWIN_EFFECT( name, classname ) \
    extern "C" { \
        KWIN_EXPORT Effect* effect_create_##name() { return new classname; } \
    }
/**
 * Defines the function used to check whether an effect is supported
 * E.g.  KWIN_EFFECT_SUPPORTED( Flames, MyFlameEffect::supported() )
 **/
#define KWIN_EFFECT_SUPPORTED( name, function ) \
    extern "C" { \
        KWIN_EXPORT bool effect_supported_##name() { return function; } \
    }
/**
 * Defines the function used to retrieve an effect's config widget
 * E.g.  KWIN_EFFECT_CONFIG( Flames, MyFlameEffect::configWidget() )
 **/
#define KWIN_EFFECT_CONFIG( name, function ) \
    extern "C" { \
        KWIN_EXPORT Effect* effect_config_##name() { return function; } \
    }


class KWIN_EXPORT EffectsHandler
    {
    friend class Effect;
    public:
        EffectsHandler(CompositingType type);
        virtual ~EffectsHandler();
        // for use by effects
        virtual void prePaintScreen( int* mask, QRegion* region, int time ) = 0;
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data ) = 0;
        virtual void postPaintScreen() = 0;
        virtual void prePaintWindow( EffectWindow* w, int* mask, QRegion* paint, QRegion* clip, int time ) = 0;
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data ) = 0;
        virtual void postPaintWindow( EffectWindow* w ) = 0;
        virtual void drawWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data ) = 0;
        virtual QRect transformWindowDamage( EffectWindow* w, const QRect& r );
        // Functions for handling input - e.g. when an Expose-like effect is shown, an input window
        // covering the whole screen is created and all mouse events will be intercepted by it.
        // The effect's windowInputMouseEvent() will get called with such events.
        virtual Window createInputWindow( Effect* e, int x, int y, int w, int h, const QCursor& cursor ) = 0;
        virtual Window createInputWindow( Effect* e, const QRect& r, const QCursor& cursor );
        virtual Window createFullScreenInputWindow( Effect* e, const QCursor& cursor );
        virtual void destroyInputWindow( Window w ) = 0;
        virtual QPoint cursorPos() const = 0;
        virtual bool grabKeyboard( Effect* effect ) = 0;
        virtual void ungrabKeyboard() = 0;

        virtual void checkElectricBorder(const QPoint &pos, Time time) = 0;
        virtual void reserveElectricBorder( ElectricBorder border ) = 0;
        virtual void unreserveElectricBorder( ElectricBorder border ) = 0;
        virtual void reserveElectricBorderSwitching( bool reserve ) = 0;

        // functions that allow controlling windows/desktop
        virtual void activateWindow( EffectWindow* c ) = 0;
        virtual EffectWindow* activeWindow() const = 0 ;
        virtual void moveWindow( EffectWindow* w, const QPoint& pos ) = 0;
        virtual void windowToDesktop( EffectWindow* w, int desktop ) = 0;
        // 
        virtual int currentDesktop() const = 0;
        virtual int numberOfDesktops() const = 0;
        virtual void setCurrentDesktop( int desktop ) = 0;
        virtual QString desktopName( int desktop ) const = 0;
        virtual QRect clientArea( clientAreaOption, const QPoint& p, int desktop ) const = 0;
        virtual void calcDesktopLayout(int* x, int* y, Qt::Orientation* orientation) const = 0;
        virtual bool optionRollOverDesktops() const = 0;

        virtual EffectWindowList stackingOrder() const = 0;

        virtual void setTabBoxWindow(EffectWindow*) = 0;
        virtual void setTabBoxDesktop(int) = 0;
        virtual EffectWindowList currentTabBoxWindowList() const = 0;
        virtual void refTabBox() = 0;
        virtual void unrefTabBox() = 0;
        virtual void closeTabBox() = 0;
        virtual QList< int > currentTabBoxDesktopList() const = 0;
        virtual int currentTabBoxDesktop() const = 0;
        virtual EffectWindow* currentTabBoxWindow() const = 0;

        // Repaints the entire workspace
        virtual void addRepaintFull() = 0;
        virtual void addRepaint( const QRect& r ) = 0;
        virtual void addRepaint( int x, int y, int w, int h ) = 0;

        CompositingType compositingType() const  { return compositing_type; }
        virtual unsigned long xrenderBufferPicture() = 0;


    protected:
        QVector< EffectPair > loaded_effects;
        QHash< QString, KLibrary* > effect_libraries;
        QList< InputWindowPair > input_windows;
        //QHash< QString, EffectFactory* > effect_factories;
        int current_paint_screen;
        int current_paint_window;
        int current_draw_window;
        int current_transform;
        CompositingType compositing_type;
    };


// This class is a representation of a window used by/for Effect classes.
// The purpose is to hide internal data and also to serve as a single
// representation for the case when Client/Unmanaged becomes Deleted.
class KWIN_EXPORT EffectWindow
    {
    public:
        // Flags explaining why painting should be disabled
        enum
        {
            // Window will not be painted
            PAINT_DISABLED              = 1 << 0,
            // Window will not be painted because it is deleted
            PAINT_DISABLED_BY_DELETE    = 1 << 1,
            // Window will not be painted because of which desktop it's on
            PAINT_DISABLED_BY_DESKTOP   = 1 << 2,
            // Window will not be painted because it is minimized
            PAINT_DISABLED_BY_MINIMIZE  = 1 << 3
        };

        EffectWindow();
        virtual ~EffectWindow();

        virtual void enablePainting( int reason ) = 0;
        virtual void disablePainting( int reason ) = 0;
        virtual void addRepaint( const QRect& r ) = 0;
        virtual void addRepaint( int x, int y, int w, int h ) = 0;
        virtual void addRepaintFull() = 0;

        virtual void refWindow() = 0;
        virtual void unrefWindow() = 0;
        virtual bool isDeleted() const = 0;

        virtual bool isMinimized() const = 0;
        virtual double opacity() const = 0;

        virtual bool isOnDesktop( int d ) const;
        virtual bool isOnCurrentDesktop() const;
        virtual bool isOnAllDesktops() const = 0;
        virtual int desktop() const = 0; // prefer isOnXXX()

        virtual int x() const = 0;
        virtual int y() const = 0;
        virtual int width() const = 0;
        virtual int height() const = 0;
        virtual QRect geometry() const = 0;
        virtual QPoint pos() const = 0;
        virtual QSize size() const = 0;
        virtual QRect rect() const = 0;
        virtual bool isMovable() const = 0;
        virtual bool isUserMove() const = 0;
        virtual bool isUserResize() const = 0;
        virtual QRect iconGeometry() const = 0;

        virtual QString caption() const = 0;
        virtual QPixmap icon() const = 0;
        virtual QString windowClass() const = 0;
        virtual const EffectWindowGroup* group() const = 0;

        virtual bool isDesktop() const = 0;
        virtual bool isDock() const = 0;
        virtual bool isToolbar() const = 0;
        virtual bool isTopMenu() const = 0;
        virtual bool isMenu() const = 0;
        virtual bool isNormalWindow() const = 0; // normal as in 'NET::Normal or NET::Unknown non-transient'
        virtual bool isSpecialWindow() const = 0;
        virtual bool isDialog() const = 0;
        virtual bool isSplash() const = 0;
        virtual bool isUtility() const = 0;
        virtual bool isDropdownMenu() const = 0;
        virtual bool isPopupMenu() const = 0; // a context popup, not dropdown, not torn-off
        virtual bool isTooltip() const = 0;
        virtual bool isNotification() const = 0;
        virtual bool isComboBox() const = 0;
        virtual bool isDNDIcon() const = 0;
        
        virtual bool isModal() const = 0;
        virtual EffectWindow* findModal() = 0;
        virtual EffectWindowList mainWindows() const = 0;

        virtual QVector<Vertex>& vertices() = 0;
        // Can be called in pre-paint pass. Makes sure that all quads that the
        //  window consists of are not bigger than maxquadsize x maxquadsize
        //  (in pixels) in the following paint pass.
        virtual void requestVertexGrid(int maxquadsize) = 0;
        // Marks vertices of the window as dirty. Call this if you change
        //  position of the vertices
        virtual void markVerticesDirty() = 0;
    };

class KWIN_EXPORT EffectWindowGroup
    {
    public:
        virtual ~EffectWindowGroup();
        virtual EffectWindowList members() const = 0;
    };

/**
 * @short Vertex class
 * Vertex has position and texture coordinate which are equal at first,
 *  however effects can e.g. modify position to move the window or part of it.
 **/
class KWIN_EXPORT Vertex
{
    public:
        Vertex()  {}
        Vertex(float x, float y)
            {
            pos[0] = texcoord[0] = x; pos[1] = texcoord[1] = y; pos[2] = 0.0f;
            }
        Vertex(float x, float y, float u, float v)
            {
            pos[0] = x; pos[1] = y; pos[2] = 0.0f; texcoord[0] = u; texcoord[1] = v;
            }
        float pos[3];
        float texcoord[2];
};

extern KWIN_EXPORT EffectsHandler* effects;



} // namespace

#endif
