/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

// TODO MIT or some other licence, perhaps move to some lib

#ifndef KWIN_EFFECTS_H
#define KWIN_EFFECTS_H

#include "scene.h"

#include <qpair.h>

namespace KWinInternal
{

class Toplevel;
class Workspace;
class EffectWindow;

class WindowPaintData
    {
    public:
        WindowPaintData();
        /**
         * Window opacity, in range 0 = transparent to 1 = fully opaque
         */
        double opacity;
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
        float saturation;
        /**
         * Brightness of the window, in range [0; 1]
         * 1 means that the window is unchanged, 0 means that it's completely
         * black. 0.5 would make it 50% darker than usual
         **/
        float brightness;
    };

class ScreenPaintData
    {
    public:
        ScreenPaintData();
        double xScale;
        double yScale;
        int xTranslate;
        int yTranslate;
    };

class Effect
    {
    public:
        virtual ~Effect();
        virtual void prePaintScreen( int* mask, QRegion* region, int time );
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        virtual void postPaintScreen();
        virtual void prePaintWindow( EffectWindow* w, int* mask, QRegion* region, int time );
        // paintWindow() can do various transformations
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual void postPaintWindow( EffectWindow* w );
        // drawWindow() is used even for thumbnails etc. - it can alter the window itself where it
        // makes sense (e.g.darkening out unresponsive windows), but it cannot do transformations
        virtual void drawWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
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

        // Interpolates between x and y
        static float interpolate(float x, float y, float a)
            {
            return x * (1 - a) + y * a;
            }
        // helper to set WindowPaintData and QRegion to necessary transformations so that
        // a following drawWindow() would put the window at the requested geometry (useful for thumbnails)
        static void setPositionTransformations( WindowPaintData& data, QRect& region, EffectWindow* w,
            const QRect& r, Qt::AspectRatioMode aspect );

    protected:
        Workspace* workspace() const;
    };

class EffectFactory
    {
    public:
        // only here to avoid warnings
        virtual ~EffectFactory();

        virtual Effect* create() const = 0;
    };

template <class EFFECT>
class GenericEffectFactory : public EffectFactory
    {
        virtual Effect* create() const
            {
            return new EFFECT();
            }
    };


class EffectsHandler
    {
    friend class Effect;
    public:
        EffectsHandler();
        ~EffectsHandler();
        // for use by effects
        void prePaintScreen( int* mask, QRegion* region, int time );
        void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        void postPaintScreen();
        void prePaintWindow( EffectWindow* w, int* mask, QRegion* region, int time );
        void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        void postPaintWindow( EffectWindow* w );
        void drawWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        // Functions for handling input - e.g. when an Expose-like effect is shown, an input window
        // covering the whole screen is created and all mouse events will be intercepted by it.
        // The effect's windowInputMouseEvent() will get called with such events.
        Window createInputWindow( Effect* e, int x, int y, int w, int h, const QCursor& cursor );
        Window createInputWindow( Effect* e, const QRect& r, const QCursor& cursor );
        void destroyInputWindow( Window w );
        // functions that allow controlling windows/desktop
        void activateWindow( EffectWindow* c );
        // 
        int currentDesktop() const;
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
        bool checkInputWindowEvent( XEvent* e );
        void checkInputWindowStacking();
        void desktopChanged( int old );
        void windowDamaged( EffectWindow* w, const QRect& r );
        void windowGeometryShapeChanged( EffectWindow* w, const QRect& old );

        void registerEffect( const QString& name, EffectFactory* factory );
        void loadEffect( const QString& name );
        void unloadEffect( const QString& name );

    private:
        typedef QPair< QString, Effect* > EffectPair;
        QVector< EffectPair > loaded_effects;
        typedef QPair< Effect*, Window > InputWindowPair;
        QList< InputWindowPair > input_windows;
        QMap< QString, EffectFactory* > effect_factories;
        int current_paint_screen;
        int current_paint_window;
        int current_draw_window;
    };

// This class is a representation of a window used by/for Effect classes.
// The purpose is to hide internal data and also to serve as a single
// representation for the case when Client/Unmanaged becomes Deleted.
class EffectWindow
    {
    public:
        EffectWindow();
        const Toplevel* window() const;
        Toplevel* window();
        void enablePainting( int reason );
        void disablePainting( int reason );
        bool isOnDesktop( int d ) const;
        bool isOnCurrentDesktop() const;
        bool isOnAllDesktops() const;
        int desktop() const; // prefer isOnXXX()
        int x() const;
        int y() const;
        int width() const;
        int height() const;
        QRect geometry() const;
        QPoint pos() const;
        QSize size() const;
        QRect rect() const;
        bool isDesktop() const;
        bool isDock() const;
        bool isToolbar() const;
        bool isTopMenu() const;
        bool isMenu() const;
        bool isNormalWindow() const; // normal as in 'NET::Normal or NET::Unknown non-transient'
        bool isDialog() const;
        bool isSplash() const;
        bool isUtility() const;
        bool isDropdownMenu() const;
        bool isPopupMenu() const; // a context popup, not dropdown, not torn-off
        bool isTooltip() const;
        bool isNotification() const;
        bool isComboBox() const;
        bool isDNDIcon() const;

        void setWindow( Toplevel* w ); // internal
        void setSceneWindow( Scene::Window* w ); // internal
        Scene::Window* sceneWindow(); // internal
    private:
        Toplevel* toplevel;
        Scene::Window* sw; // This one is used only during paint pass.
    };

EffectWindow* effectWindow( Toplevel* w );
EffectWindow* effectWindow( Scene::Window* w );

extern EffectsHandler* effects;

inline
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

inline
ScreenPaintData::ScreenPaintData()
    : xScale( 1 )
    , yScale( 1 )
    , xTranslate( 0 )
    , yTranslate( 0 )
    {
    }

inline
const Toplevel* EffectWindow::window() const
    {
    return toplevel;
    }

inline
Toplevel* EffectWindow::window()
    {
    return toplevel;
    }

inline
void EffectWindow::setWindow( Toplevel* w )
    {
    toplevel = w;
    }

inline
void EffectWindow::setSceneWindow( Scene::Window* w )
    {
    sw = w;
    }

inline
Scene::Window* EffectWindow::sceneWindow()
    {
    return sw;
    }

inline
int EffectWindow::x() const
    {
    return toplevel->x();
    }
    
inline
int EffectWindow::y() const
    {
    return toplevel->y();
    }

inline
int EffectWindow::width() const
    {
    return toplevel->width();
    }
    
inline
int EffectWindow::height() const
    {
    return toplevel->height();
    }

inline
QRect EffectWindow::geometry() const
    {
    return toplevel->geometry();
    }

inline
QSize EffectWindow::size() const
    {
    return toplevel->size();
    }
    
inline
QPoint EffectWindow::pos() const
    {
    return toplevel->pos();
    }
    
inline
QRect EffectWindow::rect() const
    {
    return toplevel->rect();
    }

inline
bool EffectWindow::isDesktop() const
    {
    return toplevel->isDesktop();
    }

inline
bool EffectWindow::isDock() const
    {
    return toplevel->isDock();
    }

inline
bool EffectWindow::isToolbar() const
    {
    return toplevel->isToolbar();
    }

inline
bool EffectWindow::isTopMenu() const
    {
    return toplevel->isTopMenu();
    }

inline
bool EffectWindow::isMenu() const
    {
    return toplevel->isMenu();
    }

inline
bool EffectWindow::isNormalWindow() const
    {
    return toplevel->isNormalWindow();
    }

inline
bool EffectWindow::isDialog() const
    {
    return toplevel->isDialog();
    }

inline
bool EffectWindow::isSplash() const
    {
    return toplevel->isSplash();
    }

inline
bool EffectWindow::isUtility() const
    {
    return toplevel->isUtility();
    }

inline
bool EffectWindow::isDropdownMenu() const
    {
    return toplevel->isDropdownMenu();
    }

inline
bool EffectWindow::isPopupMenu() const
    {
    return toplevel->isPopupMenu();
    }

inline
bool EffectWindow::isTooltip() const
    {
    return toplevel->isTooltip();
    }

inline
bool EffectWindow::isNotification() const
    {
    return toplevel->isNotification();
    }

inline
bool EffectWindow::isComboBox() const
    {
    return toplevel->isComboBox();
    }

inline
bool EffectWindow::isDNDIcon() const
    {
    return toplevel->isDNDIcon();
    }

inline
EffectWindow* effectWindow( Toplevel* w )
    {
    EffectWindow* ret = w->effectWindow();
    ret->setSceneWindow( NULL ); // just in case
    return ret;
    }

inline
EffectWindow* effectWindow( Scene::Window* w )
    {
    EffectWindow* ret = w->window()->effectWindow();
    ret->setSceneWindow( w );
    return ret;
    }

inline
Workspace* Effect::workspace() const
    {
    return Workspace::self();
    }

} // namespace

#endif
