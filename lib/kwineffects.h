/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

// TODO MIT or some other license, perhaps move to some lib

#ifndef KWINEFFECTS_H
#define KWINEFFECTS_H

#include <kwinglobals.h>

#include <QtCore/QPair>
#include <QtCore/QRect>
#include <QtGui/QRegion>
#include <QtGui/QFont>

#include <QtCore/QVector>
#include <QtCore/QList>
#include <QtCore/QHash>

#include <KDE/KPluginFactory> 

#include <assert.h>

class KLibrary;
class KConfigGroup;
class QKeyEvent;

namespace KWin
{


class EffectWindow;
class EffectWindowGroup;
class Effect;
class WindowQuad;
class GLRenderTarget;
class GLShader;
class WindowQuadList;
class WindowPrePaintData;
class WindowPaintData;
class ScreenPrePaintData;
class ScreenPaintData;

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

        virtual void prePaintScreen( ScreenPrePaintData& data, int time );
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        virtual void postPaintScreen();
        virtual void prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time );
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
        virtual void mouseChanged( const QPoint& pos, const QPoint& old,
            Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers );
        virtual void grabbedKeyboardEvent( QKeyEvent* e );

        virtual void tabBoxAdded( int mode );
        virtual void tabBoxClosed();
        virtual void tabBoxUpdated();
        virtual bool borderActivated( ElectricBorder border );

        static int displayWidth();
        static int displayHeight();
        static QPoint cursorPos();

        // Interpolates between x and y
        static double interpolate(double x, double y, double a)
            {
            return x * (1 - a) + y * a;
            }
        // helper to set WindowPaintData and QRegion to necessary transformations so that
        // a following drawWindow() would put the window at the requested geometry (useful for thumbnails)
        static void setPositionTransformations( WindowPaintData& data, QRect& region, EffectWindow* w,
            const QRect& r, Qt::AspectRatioMode aspect );
    };


/**
 * Defines the class to be used for effect with given name.
 * The name must be same as effect's X-KDE-PluginInfo-Name values in .desktop
 *  file, but without the "kwin4_effect_" prefix.
 * E.g.  KWIN_EFFECT( flames, MyFlameEffect )
 * In this case object of MyFlameEffect class would be created when effect
 *  "flames" (which has X-KDE-PluginInfo-Name=kwin4_effect_flames in .desktop
 *  file) is loaded.
 **/
#define KWIN_EFFECT( name, classname ) \
    extern "C" { \
        KWIN_EXPORT Effect* effect_create_kwin4_effect_##name() { return new classname; } \
    }
/**
 * Defines the function used to check whether an effect is supported
 * E.g.  KWIN_EFFECT_SUPPORTED( flames, MyFlameEffect::supported() )
 **/
#define KWIN_EFFECT_SUPPORTED( name, function ) \
    extern "C" { \
        KWIN_EXPORT bool effect_supported_kwin4_effect_##name() { return function; } \
    }
/**
 * Defines the function used to retrieve an effect's config widget
 * E.g.  KWIN_EFFECT_CONFIG( flames, MyFlameEffectConfig )
 **/
#define KWIN_EFFECT_CONFIG( name, classname ) \
    K_PLUGIN_FACTORY(name##_factory, registerPlugin<classname>();) \
    K_EXPORT_PLUGIN(name##_factory("kcm_kwineffect_" #name))

/**
 * The declaration of the factory to export the effect
 **/
#define KWIN_EFFECT_CONFIG_FACTORY K_PLUGIN_FACTORY_DECLARATION(EffectFactory)


class KWIN_EXPORT EffectsHandler
    {
    friend class Effect;
    public:
        EffectsHandler(CompositingType type);
        virtual ~EffectsHandler();
        // for use by effects
        virtual void prePaintScreen( ScreenPrePaintData& data, int time ) = 0;
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data ) = 0;
        virtual void postPaintScreen() = 0;
        virtual void prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time ) = 0;
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
        // window will be temporarily painted as if being at the top of the stack
        virtual void setElevatedWindow( EffectWindow* w, bool set ) = 0;

        virtual void setTabBoxWindow(EffectWindow*) = 0;
        virtual void setTabBoxDesktop(int) = 0;
        virtual EffectWindowList currentTabBoxWindowList() const = 0;
        virtual void refTabBox() = 0;
        virtual void unrefTabBox() = 0;
        virtual void closeTabBox() = 0;
        virtual QList< int > currentTabBoxDesktopList() const = 0;
        virtual int currentTabBoxDesktop() const = 0;
        virtual EffectWindow* currentTabBoxWindow() const = 0;

        virtual void setActiveFullScreenEffect( Effect* e ) = 0;
        virtual Effect* activeFullScreenEffect() const = 0;

        virtual void pushRenderTarget(GLRenderTarget* target) = 0;
        virtual GLRenderTarget* popRenderTarget() = 0;

        // Repaints the entire workspace
        virtual void addRepaintFull() = 0;
        virtual void addRepaint( const QRect& r ) = 0;
        virtual void addRepaint( int x, int y, int w, int h ) = 0;

        CompositingType compositingType() const;
        virtual unsigned long xrenderBufferPicture() = 0;
        virtual void reconfigure() = 0;

        /**
         * Paints given text onto screen, possibly in elided form
         * @param text
         * @param center center point of the painted text
         * @param maxwidth if text is longer than this, is will be elided
         * @param color color of the text, may contain alpha
         * @param font font for the text
         **/
        bool paintText( const QString& text, const QPoint& center, int maxwidth,
                        const QColor& color, const QFont& font = QFont() );
        bool paintTextWithBackground( const QString& text, const QPoint& center, int maxwidth,
                                      const QColor& color, const QColor& bgcolor,
                                      const QFont& font = QFont() );


        /**
         * Sends message over DCOP to reload given effect.
         * @param effectname effect's name without "kwin4_effect_" prefix.
         * Can be called from effect's config module to apply config changes.
         **/
        static void sendReloadMessage( const QString& effectname );
        /**
         * @return @ref KConfigGroup which holds given effect's config options
         **/
        static KConfigGroup effectConfig( const QString& effectname );


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
        virtual bool isPaintingEnabled() = 0;
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
        /**
         * Geometry of the actual window contents inside the whole (including decorations) window.
         */
        virtual QRect contentsRect() const = 0;
        bool hasDecoration() const;

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

        // TODO internal?
        virtual WindowQuadList buildQuads() const = 0;
    };

class KWIN_EXPORT EffectWindowGroup
    {
    public:
        virtual ~EffectWindowGroup();
        virtual EffectWindowList members() const = 0;
    };

/**
 * @short Vertex class
 * A vertex is one position in a window. WindowQuad consists of four WindowVertex objects
 * and represents one part of a window.
 **/
class KWIN_EXPORT WindowVertex
    {
    public:
        double x() const;
        double y() const;
        void move( double x, double y );
        void setX( double x );
        void setY( double y );
        double originalX() const;
        double originalY() const;
        WindowVertex();
        WindowVertex( double x, double y, double tx, double ty );
    private:
        friend class WindowQuad;
        friend class WindowQuadList;
        double px, py; // position
        double ox, oy; // origional position
        double tx, ty; // texture coords
    };

enum WindowQuadType
    {
    WindowQuadError, // for the stupid default ctor
    WindowQuadContents,
    WindowQuadDecoration
    };

/**
 * @short Class representing one area of a window.
 * WindowQuads consists of four WindowVertex objects and represents one part of a window.
 */
// NOTE: This class expects the (original) vertices to be in the clockwise order starting from topleft.
class KWIN_EXPORT WindowQuad
    {
    public:
        explicit WindowQuad( WindowQuadType type );
        WindowQuad makeSubQuad( double x1, double y1, double x2, double y2 ) const;
        WindowVertex& operator[]( int index );
        const WindowVertex& operator[]( int index ) const;
        bool decoration() const;
        double left() const;
        double right() const;
        double top() const;
        double bottom() const;
        double originalLeft() const;
        double originalRight() const;
        double originalTop() const;
        double originalBottom() const;
        bool smoothNeeded() const;
        bool isTransformed() const;
    private:
        friend class WindowQuadList;
        WindowVertex verts[ 4 ];
        WindowQuadType type; // 0 - contents, 1 - decoration
    };

class KWIN_EXPORT WindowQuadList
    : public QList< WindowQuad >
    {
    public:
        WindowQuadList splitAtX( double x ) const;
        WindowQuadList splitAtY( double y ) const;
        WindowQuadList makeGrid( int maxquadsize ) const;
        WindowQuadList select( WindowQuadType type ) const;
        WindowQuadList filterOut( WindowQuadType type ) const;
        bool smoothNeeded() const;
        void makeArrays( float** vertices, float** texcoords ) const;
    };

class KWIN_EXPORT WindowPrePaintData
    {
    public:
        int mask;
        QRegion paint;
        QRegion clip;
        WindowQuadList quads;
        /**
         * Simple helper that sets data to say the window will be painted as non-opaque.
         * Takes also care of changing the regions.
         */
        void setTranslucent();
        /**
         * Helper to mark that this window will be transformed
         **/
        void setTransformed();
    };

class KWIN_EXPORT WindowPaintData
    {
    public:
        WindowPaintData( EffectWindow* w );
        /**
         * Window opacity, in range 0 = transparent to 1 = fully opaque
         * Opacity for contents is opacity*contents_opacity, the same
         * way for decoration.
         */
        double opacity;
        double contents_opacity;
        double decoration_opacity;
        double xScale;
        double yScale;
        int xTranslate;
        int yTranslate;
        /**
         * Saturation of the window, in range [0; 1]
         * 1 means that the window is unchanged, 0 means that it's completely
         *  unsaturated (greyscale). 0.5 would make the colors less intense,
         *  but not completely grey
         **/
        double saturation;
        /**
         * Brightness of the window, in range [0; 1]
         * 1 means that the window is unchanged, 0 means that it's completely
         * black. 0.5 would make it 50% darker than usual
         **/
        double brightness;
        WindowQuadList quads;
        /**
         * Shader to be used for rendering, if any.
         */
        GLShader* shader;
    };

class KWIN_EXPORT ScreenPaintData
    {
    public:
        ScreenPaintData();
        double xScale;
        double yScale;
        int xTranslate;
        int yTranslate;
    };

class KWIN_EXPORT ScreenPrePaintData
    {
    public:
        int mask;
        QRegion paint;
    };

extern KWIN_EXPORT EffectsHandler* effects;

/***************************************************************
 WindowVertex
***************************************************************/

inline
WindowVertex::WindowVertex()
    : px( 0 ), py( 0 ), tx( 0 ), ty( 0 )
    {
    }

inline
WindowVertex::WindowVertex( double _x, double _y, double _tx, double _ty )
    : px( _x ), py( _y ), ox( _x ), oy( _y ), tx( _tx ), ty( _ty )
    {
    }

inline
double WindowVertex::x() const
    {
    return px;
    }

inline
double WindowVertex::y() const
    {
    return py;
    }

inline
double WindowVertex::originalX() const
    {
    return ox;
    }

inline
double WindowVertex::originalY() const
    {
    return oy;
    }

inline
void WindowVertex::move( double x, double y )
    {
    px = x;
    py = y;
    }

inline
void WindowVertex::setX( double x )
    {
    px = x;
    }

inline
void WindowVertex::setY( double y )
    {
    py = y;
    }

/***************************************************************
 WindowQuad
***************************************************************/

inline
WindowQuad::WindowQuad( WindowQuadType t )
    : type( t )
    {
    }

inline
WindowVertex& WindowQuad::operator[]( int index )
    {
    assert( index >= 0 && index < 4 );
    return verts[ index ];
    }

inline
const WindowVertex& WindowQuad::operator[]( int index ) const
    {
    assert( index >= 0 && index < 4 );
    return verts[ index ];
    }

inline
bool WindowQuad::decoration() const
    {
    assert( type != WindowQuadError );
    return type == WindowQuadDecoration;
    }

inline
bool WindowQuad::isTransformed() const
    {
    return !( verts[ 0 ].px == verts[ 0 ].ox && verts[ 0 ].py == verts[ 0 ].oy
        && verts[ 1 ].px == verts[ 1 ].ox && verts[ 1 ].py == verts[ 1 ].oy
        && verts[ 2 ].px == verts[ 2 ].ox && verts[ 2 ].py == verts[ 2 ].oy
        && verts[ 3 ].px == verts[ 3 ].ox && verts[ 3 ].py == verts[ 3 ].oy );
    }

inline
double WindowQuad::left() const
    {
    return qMin( verts[ 0 ].px, qMin( verts[ 1 ].px, qMin( verts[ 2 ].px, verts[ 3 ].px )));
    }

inline
double WindowQuad::right() const
    {
    return qMax( verts[ 0 ].px, qMax( verts[ 1 ].px, qMax( verts[ 2 ].px, verts[ 3 ].px )));
    }

inline
double WindowQuad::top() const
    {
    return qMin( verts[ 0 ].py, qMin( verts[ 1 ].py, qMin( verts[ 2 ].py, verts[ 3 ].py )));
    }

inline
double WindowQuad::bottom() const
    {
    return qMax( verts[ 0 ].py, qMax( verts[ 1 ].py, qMax( verts[ 2 ].py, verts[ 3 ].py )));
    }

inline
double WindowQuad::originalLeft() const
    {
    return verts[ 0 ].ox;
    }

inline
double WindowQuad::originalRight() const
    {
    return verts[ 2 ].ox;
    }

inline
double WindowQuad::originalTop() const
    {
    return verts[ 0 ].oy;
    }

inline
double WindowQuad::originalBottom() const
    {
    return verts[ 2 ].oy;
    }

} // namespace

#endif // KWINEFFECTS_H
