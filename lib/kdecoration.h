/*****************************************************************
This file is part of the KDE project.

Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
******************************************************************/

#ifndef KDECORATION_H
#define KDECORATION_H

#include <qcolor.h>
#include <qfont.h>
#include <qobject.h>
#include <qiconset.h>
#include <netwm_def.h>
#include <kdeversion.h>
#include <QMouseEvent>

class KDecorationOptionsPrivate;
class KDecorationBridge;
class KDecorationPrivate;
class KDecorationFactory;

#define KWIN_EXPORT KDE_EXPORT

/**
 * This class provides a namespace for all decoration related classes.
 * All shared types are defined here.
 * @since 3.2
 */
class KWIN_EXPORT KDecorationDefines
{
public:
    /**
     * These values represent positions inside an area
     */
    enum Position
        { // without prefix, they'd conflict with Qt::TopLeftCorner etc. :(
        PositionCenter         = 0x00,
        PositionLeft           = 0x01,
        PositionRight          = 0x02,
        PositionTop            = 0x04,
        PositionBottom         = 0x08,
        PositionTopLeft        = PositionLeft | PositionTop,
        PositionTopRight       = PositionRight | PositionTop,
        PositionBottomLeft     = PositionLeft | PositionBottom,
        PositionBottomRight    = PositionRight | PositionBottom
        };
    /**
     * Maximize mode. These values specify how a window is maximized.
     */
    // these values are written to session files, don't change the order
    enum MaximizeMode
        {
	MaximizeRestore    = 0, ///< The window is not maximized in any direction.
	MaximizeVertical   = 1, ///< The window is maximized vertically.
	MaximizeHorizontal = 2, ///< The window is maximized horizontally.
	/// Equal to @p MaximizeVertical | @p MaximizeHorizontal
	MaximizeFull = MaximizeVertical | MaximizeHorizontal 
        };

    enum WindowOperation
        {
        MaximizeOp = 5000,
        RestoreOp,
        MinimizeOp,
        MoveOp,
        UnrestrictedMoveOp,
        ResizeOp,
        UnrestrictedResizeOp,
        CloseOp,
        OnAllDesktopsOp,
        ShadeOp,
        KeepAboveOp,
        KeepBelowOp,
        OperationsOp,
        WindowRulesOp,
        ToggleStoreSettingsOp = WindowRulesOp, ///< @obsolete
        HMaximizeOp,
        VMaximizeOp,
        LowerOp,
        FullScreenOp,
        NoBorderOp,
        NoOp,
        SetupWindowShortcutOp,
        ApplicationRulesOp     ///< @since 3.5
        };
    /**
     * Basic color types that should be recognized by all decoration styles.
     * Decorations are not required to implement all the colors, but for the ones that
     * are implemented the color setting for them should be obeyed.
     */
    enum ColorType
	{
	ColorTitleBar,   ///< The color for the titlebar
	ColorTitleBlend, ///< The blend color for the titlebar
	ColorFont,       ///< The titlebar text color
	ColorButtonBg,   ///< The color to use for the titlebar buttons
	ColorFrame,      ///< The color for the window frame (border)
	ColorHandle,     ///< The color for the resize handle
	NUM_COLORS
	};
    
    /**
     * These flags specify which settings changed when rereading settings.
     * Each setting in class KDecorationOptions specifies its matching flag.
     */
    enum
        {
        SettingDecoration = 1 << 0, ///< The decoration was changed
        SettingColors     = 1 << 1, ///< The color palette was changed
        SettingFont       = 1 << 2, ///< The titlebar font was changed
        SettingButtons    = 1 << 3, ///< The button layout was changed
        SettingTooltips   = 1 << 4, ///< The tooltip setting was changed
        SettingBorder     = 1 << 5  ///< The border size setting was changed
        };
        
    /**
     * Border size. KDecorationOptions::preferredBorderSize() returns
     * one of these values.
     */
    enum BorderSize
        {
        BorderTiny,      ///< Minimal borders
        BorderNormal,    ///< Standard size borders, the default setting
        BorderLarge,     ///< Larger borders
        BorderVeryLarge, ///< Very large borders
        BorderHuge,      ///< Huge borders
        BorderVeryHuge,  ///< Very huge borders
        BorderOversized, ///< Oversized borders
        BordersCount     ///< @internal
        };

    /**
     * Used to find out which features the decoration supports.
     * @see KDecorationFactory::supports()
     */
    enum Ability
        {
        AbilityAnnounceButtons = 0, ///< decoration supports AbilityButton* values (always use)
        AbilityButtonMenu = 1000,   ///< decoration supports the menu button
        AbilityButtonOnAllDesktops = 1001, ///< decoration supports the on all desktops button
        AbilityButtonSpacer = 1002, ///< decoration supports inserting spacers between buttons
        AbilityButtonHelp = 1003,   ///< decoration supports what's this help button
        AbilityButtonMinimize = 1004,  ///< decoration supports a minimize button
        AbilityButtonMaximize = 1005, ///< decoration supports a maximize button
        AbilityButtonClose = 1006, ///< decoration supports a close button
        AbilityButtonAboveOthers = 1007, ///< decoration supports an above button
        AbilityButtonBelowOthers = 1008, ///< decoration supports a below button
        AbilityButtonShade = 1009, ///< decoration supports a shade button
        AbilityButtonResize = 1010, ///< decoration supports a resize button
        ABILITY_DUMMY = 10000000
        };

    enum Requirement { REQUIREMENT_DUMMY = 1000000 };
};    

class KDecorationProvides
    : public KDecorationDefines
    {
    public:
        virtual  ~KDecorationProvides(){};
		virtual bool provides( Requirement req ) = 0;
    };

/**
 * This class holds various configuration settings for the decoration.
 * It is accessible from the decorations either as KDecoration::options()
 * or KDecorationFactory::options().
 * @since 3.2
 */
class KWIN_EXPORT KDecorationOptions : public KDecorationDefines
    {
public:
    KDecorationOptions();
    virtual ~KDecorationOptions();
    /**
     * Returns the color that should be used for the given part of the decoration.
     * The changed flags for this setting is SettingColors.
     *
     * @param type   The requested color type.
     * @param active Whether the color should be for active or inactive windows.
     */
    const QColor& color(ColorType type, bool active=true) const;
    /**
     * Returns a colorgroup using the given decoration color as the background.
     * The changed flags for this setting is SettingColors.
     *
     * @param type   The requested color type.
     * @param active Whether to return the color for active or inactive windows.
     */
    const QColorGroup& colorGroup(ColorType type, bool active=true) const;
    /**
     * Returns the active or inactive decoration font.
     * The changed flags for this setting is SettingFont.
     *
     * @param active Whether to return the color for active or inactive windows.
     * @param small  If @a true, returns a font that's suitable for tool windows.
     */
    const QFont& font(bool active=true, bool small = false) const;
    /**
    * Returns @a true if the style should use custom button positions
    * The changed flags for this setting is SettingButtons.
    *
    * @see titleButtonsLeft
    * @see titleButtonsRight
    */
    bool customButtonPositions() const;
    /**
    * If customButtonPositions() returns true, titleButtonsLeft
    * returns which buttons should be on the left side of the titlebar from left
    * to right. Characters in the returned string have this meaning :
    * @li 'M' menu button
    * @li 'S' on_all_desktops button
    * @li 'H' quickhelp button
    * @li 'I' minimize ( iconify ) button
    * @li 'A' maximize button
    * @li 'X' close button
    * @li 'F' keep_above_others button
    * @li 'B' keep_below_others button
    * @li 'L' shade button
    * @li 'R' resize button
    * @li '_' spacer
    *
    * The default ( which is also returned if customButtonPositions returns false )
    * is "MS".
    * Unknown buttons in the returned string must be ignored.
    * The changed flags for this setting is SettingButtons.
    */
    QString titleButtonsLeft() const;
    /**
    * If customButtonPositions() returns true, titleButtonsRight
    * returns which buttons should be on the right side of the titlebar from left
    * to right. Characters in the return string have the same meaning like
    * in titleButtonsLeft().
    *
    * The default ( which is also returned if customButtonPositions returns false )
    * is "HIAX".
    * Unknown buttons in the returned string must be ignored.
    * The changed flags for this setting is SettingButtons.
    */
    QString titleButtonsRight() const;

    /**
    * @returns true if the style should use tooltips for window buttons
    * The changed flags for this setting is SettingTooltips.
    */
    bool showTooltips() const;
    
    /**
     * The preferred border size selected by the user, e.g. for accessibility
     * reasons, or when using high resolution displays. It's up to the decoration
     * to decide which borders or if any borders at all will obey this setting.
     * It is guaranteed that the returned value will be one of those
     * returned by KDecorationFactory::borderSizes(), so if that one hasn't been
     * reimplemented, BorderNormal is always returned.
     * The changed flags for this setting is SettingBorder.
     * @param factory the decoration factory used
     */
    BorderSize preferredBorderSize( KDecorationFactory* factory ) const;

    /*
     * When this functions returns false, moving and resizing of maximized windows
     * is not allowed, and therefore the decoration is allowed to turn off (some of)
     * its borders.
     * The changed flags for this setting is SettingButtons.
     */
    bool moveResizeMaximizedWindows() const;

    /**
     * @internal
     */
    WindowOperation operationMaxButtonClick( Qt::ButtonState button ) const;

    /**
     * @internal
     */
    virtual unsigned long updateSettings() = 0; // returns SettingXYZ mask

protected:
    /**
     * @internal
     */
    KDecorationOptionsPrivate* d;
    };


/**
 * This is the base class for a decoration object. It provides functions
 * that give various information about the decorated window, and also
 * provides pure virtual functions for controlling the decoration that
 * every decoration should implement.
 * @since 3.2
 */
class KWIN_EXPORT KDecoration
    : public QObject, public KDecorationDefines
    {
    Q_OBJECT
    public:
	/**
	 * Constructs a KDecoration object. Both the arguments are passed from
	 * KDecorationFactory. Note that the initialization code of the decoration
	 * should be done in the init() method.
	 */
	KDecoration( KDecorationBridge* bridge, KDecorationFactory* factory );
	/**
	 * Destroys the KDecoration.
	 */
	virtual ~KDecoration();
	
	// requests from decoration
	
	/**
	 * Returns the KDecorationOptions object, which is used to access
	 * configuration settings for the decoration.
	 */
        static const KDecorationOptions* options();
	/**
	 * Returns @a true if the decorated window is currently active.
	 */
	bool isActive() const;
	/**
	 * Returns @a true if the decoration window can be closed by the user.
	 */
	bool isCloseable() const;
	/**
	 * Returns @a true if the decorated window can be maximized.
	 */
	bool isMaximizable() const;
	/**
	 * Returns the current maximization mode of the decorated window.
	 * Note that only fully maximized windows should be treated
	 * as "maximized" (e.g. if the maximize button has only two states).
	 */
	MaximizeMode maximizeMode() const;
	/**
	 * Returns @a true if the decorated window can be minimized by the user.
	 */
	bool isMinimizable() const;
	/**
	 * Return @a true if the decorated window can show context help
	 * (i.e. the decoration should provide the context help button).
	 */
        bool providesContextHelp() const;
	/**
	 * Returns the number of the virtual desktop the decorated window
	 * is currently on (including NET::OnAllDesktops for being on all
	 * desktops).
	 */
        int desktop() const;
	/**
	 * Convenience function that returns @a true if the window is on all
	 * virtual desktops.
	 */
        bool isOnAllDesktops() const; // convenience
	/**
	 * Returns @a true if the decoration window is modal (usually a modal dialog).
	 */
        bool isModal() const;
	/**
	 * Returns @a true if the decorated window can be shaded.
	 */
        bool isShadeable() const;
	/**
	 * Returns @a true if the decorated window is currently shaded.
         * If the window is e.g. hover unshaded, it's not considered to be shaded.
         * This function should not be used for the shade titlebar button, use
         * @ref isSetShade() instead.
         *
         * @see isSetShade
	 */
        bool isShade() const;
	/**
	 * Returns @a true if the decorated window was set to be shaded. This function
         * returns also true if the window is e.g. hover unshaded, so it doesn't
         * always correspond to the actual window state.
         *
         * @see isShade
	 */
        bool isSetShade() const;
	/**
	 * Returns @a true if the decorated window should be kept above other windows.
	 */
        bool keepAbove() const;
	/**
	 * Returns @a true if the decorated window should be kept below other windows.
	 */
        bool keepBelow() const;
	/**
	 * Returns @a true if the decorated window can be moved by the user.
	 */
        bool isMovable() const;
	/**
	 * Returns @a true if the decorated window can be resized by the user.
	 */
        bool isResizable() const;
	/**
	 * This function returns the window type of the decorated window.
	 * The argument to this function is a mask of all window types
	 * the decoration knows about (as the list of valid window types
	 * is extended over time, and fallback types are specified in order
	 * to support older code). For a description of all window types,
	 * see the definition of the NET::WindowType type. Note that
	 * some window types never have decorated windows.
	 * 
	 * An example of usage:
	 * @code
	 * const unsigned long supported_types = NET::NormalMask | NET::DesktopMask
	 * 	| NET::DockMask | NET::ToolbarMask | NET::MenuMask | NET::DialogMask
	 * 	| NET::OverrideMask | NET::TopMenuMask | NET::UtilityMask | NET::SplashMask;
	 *
	 * NET::WindowType type = windowType( supported_types );
	 *
	 * if( type == NET::Utility || type == NET::Menu || type == NET::Toolbar )
	 *    // ... use smaller decorations for tool window types
	 * else
	 *    // ... use normal decorations
	 * @endcode
	 */
        NET::WindowType windowType( unsigned long supported_types ) const;
	/**
	 * Returns an icon set with the decorated window's icon.
	 */
	QIcon icon() const;
	/**
	 * Returns the decorated window's caption that should be shown in the titlebar.
	 */
	QString caption() const;
	/**
	 * This function invokes the window operations menu. 
	 * \param pos specifies the place on the screen where the menu should
	 * show up. The menu pops up at the bottom-left corner of the specified
	 * rectangle, unless there is no space, in which case the menu is
	 * displayed above the rectangle.
	 * 
	 * \note Decorations that enable a double-click operation for the menu
	 * button must ensure to call \a showWindowMenu() with the \a pos
	 * rectangle set to the menu button geometry.
	 * IMPORTANT: As a result of this function, the decoration object that
	 * called it may be destroyed after the function returns. This means
	 * that the decoration object must either return immediately after
	 * calling showWindowMenu(), or it must use
	 * KDecorationFactory::exists() to check it's still valid. For example,
	 * the code handling clicks on the menu button should look similarly
	 * like this:
         *
         * \code
         * KDecorationFactory* f = factory(); // needs to be saved before
         * showWindowMenu( button[MenuButton]->mapToGlobal( menuPoint ));
         * if( !f->exists( this )) // destroyed, return immediately
         *     return;
	 * button[MenuButton]->setDown(false);
         * \endcode
	 */
	void showWindowMenu( const QRect &pos );

	/**
	 * Overloaded version of the above. 
	 */
	void showWindowMenu( QPoint pos );
        /**
         * This function performs the given window operation. This function may destroy
         * the current decoration object, just like showWindowMenu().
         */
	void performWindowOperation( WindowOperation op );
	/**
	 * If the decoration is non-rectangular, this function needs to be called
	 * to set the shape of the decoration.
	 *
	 * @param reg  The shape of the decoration.
	 * @param mode The X11 values Unsorted, YSorted, YXSorted and YXBanded that specify
	 *             the sorting of the rectangles, default value is Unsorted.
	 */
        void setMask( const QRegion& reg, int mode = 0 );
	/**
	 * This convenience function resets the shape mask.
	 */
        void clearMask(); // convenience
	/**
	 * If this function returns @a true, the decorated window is used as a preview
	 * e.g. in the configuration module. In such case, the decoration can e.g.
	 * show some information in the window area.
	 */
        bool isPreview() const;
	/**
	 * Returns the geometry of the decoration.
	 */
        QRect geometry() const;
	/**
	 * Returns the icon geometry for the window, i.e. the geometry of the taskbar
	 * entry. This is used mainly for window minimize animations. Note that
	 * the geometry may be null.
	 */
        QRect iconGeometry() const;
        /**
         * Returns the intersection of the given region with the region left
         * unobscured by the windows stacked above the current one. You can use
         * this function to, for example, try to keep the titlebar visible if
         * there is a hole available. The region returned is in the coordinate
         * space of the decoration.
         * @param r The region you want to check for holes
         */
        QRegion unobscuredRegion( const QRegion& r ) const;
	/**
	 * Returns the main workspace widget. The main purpose of this function is to
	 * allow painting the minimize animation or the transparent move bound on it.
	 */
        QWidget* workspaceWidget() const;
        /**
         * Returns the handle of the window that is being decorated. It is possible
         * the returned value will be 0.
         * IMPORTANT: This function is meant for special purposes, and it
         * usually should not be used. The main purpose is finding out additional
         * information about the window's state. Also note that different kinds
         * of windows are decorated: Toplevel windows managed by the window manager,
         * test window in the window manager decoration module, and possibly also
         * other cases.
         * Careless abuse of this function will usually sooner or later lead
         * to problems.
         * @since 3.4
         */
        WId windowId() const;
	/**
	 * Convenience function that returns the width of the decoration.
	 */
        int width() const; // convenience
	/**
	 * Convenience function that returns the height of the decoration.
	 */
        int height() const;  // convenience
	/**
	 * This function is the default handler for mouse events. All mouse events
	 * that are not handled by the decoration itself should be passed to it
	 * in order to make work operations like window resizing by dragging borders etc.
	 */
	void processMousePressEvent( QMouseEvent* e );

	// requests to decoration

	/**
	 * This function is called immediately after the decoration object is created.
	 * Due to some technical reasons, initialization should be done here
	 * instead of in the constructor.
	 */
        virtual void init() = 0; // called once right after created
	
        /**
         * This function should return mouse cursor position in the decoration.
         * Positions at the edge will result in window resizing with mouse button
         * pressed, center position will result in moving.
         */
	virtual Position mousePosition( const QPoint& p ) const = 0;

	/**
	 * This function should return the distance from each window side to the inner
	 * window. The sizes may depend on the state of the decorated window, such as
	 * whether it's shaded. Decorations often turn off their bottom border when the
	 * window is shaded, and turn off their left/right/bottom borders when
	 * the window is maximized and moving and resizing of maximized windows is disabled.
	 * This function mustn't do any repaints or resizes. Also, if the sizes returned
	 * by this function don't match the real values, this may result in drawing errors
	 * or other problems.
	 *
	 * @see KDecorationOptions::moveResizeMaximizedWindows()
	 */
        // mustn't do any repaints, resizes or anything like that
	virtual void borders( int& left, int& right, int& top, int& bottom ) const = 0;
	/**
	 * This method is called by kwin when the style should resize the decoration window.
	 * The usual implementation is to resize the main widget of the decoration to the
	 * given size.
	 *
	 * @param s Specifies the new size of the decoration window.
	 */
	virtual void resize( const QSize& s ) = 0;
	/**
	 * This function should return the minimum required size for the decoration.
	 * Note that the returned size shouldn't be too large, because it will be
	 * used to keep the decorated window at least as large.
	 */
	virtual QSize minimumSize() const = 0;
	/**
	 * This function is called whenever the window either becomes or stops being active.
	 * Use isActive() to find out the current state.
	 */
        virtual void activeChange() = 0;
	/**
	 * This function is called whenever the caption changes. Use caption() to get it.
	 */
        virtual void captionChange() = 0;
	/**
	 * This function is called whenever the window icon changes. Use icon() to get it.
	 */
        virtual void iconChange() = 0;
	/**
	 * This function is called whenever the maximalization state of the window changes.
	 * Use maximizeMode() to get the current state.
	 */
        virtual void maximizeChange() = 0;
	/**
	 * This function is called whenever the desktop for the window changes. Use
	 * desktop() or isOnAllDesktops() to find out the current desktop
	 * on which the window is.
	 */
        virtual void desktopChange() = 0;
	/**
	 * This function is called whenever the window is shaded or unshaded. Use
	 * isShade() to get the current state.
	 */
        virtual void shadeChange() = 0;
#warning Redo all the XYZChange() virtuals as signals.
    signals:
        /**
         * This signal is emitted whenever the window's keep-above state changes.
         * @since 3.3
         */
        void keepAboveChanged( bool );
        /**
         * This signal is emitted whenever the window's keep-below state changes.
         * @since 3.3
         */
        void keepBelowChanged( bool );
    public:
	/**
	 * This function may be reimplemented to provide custom bound drawing
	 * for transparent moving or resizing of the window.
	 * @a False should be returned if the default implementation should be used.
         * Note that if you e.g. paint the outline using a 5 pixels wide line,
         * you should compensate for the 2 pixels that would make the window
         * look larger.
	 *
	 * @param geom  The geometry at this the bound should be drawn
	 * @param clear @a true if the bound should be cleared
	 *
	 * @see workspaceWidget() and geometry().
	 */
        virtual bool drawbound( const QRect& geom, bool clear );
	/**
	 * This function may be reimplemented to provide custom minimize/restore animations
	 * The reimplementation is allowed to perform X server grabs if necessary
         * (only using the functions provided by this API, no direct Xlib calls), but no
	 * futher event processing is allowed (i.e. no kapp->processEvents()).
	 * @a False should be returned if the default implementation should be used.
	 * Note that you should not use this function to force disabling of the animation.
	 *
	 * @see workspaceWidget(), geometry() and helperShowHide().
	 */
        virtual bool animateMinimize( bool minimize );
        /**
         * @internal Reserved.
         */
        // TODO position will need also values for top+left+bottom etc. docking ?
        virtual bool windowDocked( Position side );
	/**
	 * This function is called to reset the decoration on settings changes.
	 * It is usually invoked by calling KDecorationFactory::resetDecorations().
	 * 
	 * @param changed Specifies which settings were changed, given by the SettingXXX masks
	 */
        virtual void reset( unsigned long changed );

	// special

	/**
	 * This should be the first function called in init() to specify
	 * the main widget of the decoration. The widget should be created
	 * with parent specified by initialParentWidget() and flags
	 * specified by initialWFlags().
	 */	
        void setMainWidget( QWidget* );
	/**
	 * Convenience functions that creates and sets a main widget as necessary.
	 * In such case, it's usually needed to install an event filter
	 * on the main widget to receive important events on it.
	 *
	 * @param flags Additional widget flags for the main widget. Note that only
	 *              flags that affect widget drawing are allowed. Window type flags
	 *              like WX11BypassWM or WStyle_NoBorder are forbidden.
	 */
        void createMainWidget( Qt::WFlags flags = 0 );
	/**
	 * The parent widget that should be used for the main widget.
	 */
        QWidget* initialParentWidget() const;
	/**
	 * The flags that should be used when creating the main widget.
	 * It is possible to add more flags when creating the main widget, but only flags
	 * that affect widget drawing are allowed. Window type flags like WX11BypassWM
	 * or WStyle_NoBorder are forbidden.
	 */
		Qt::WFlags initialWFlags() const;
	/**
	 * This function is only allowed to be called once from animateMinimize().
	 * It can be used if the window should be shown or hidden at a specific
	 * time during the animation. It is forbidden to use this function
	 * for other purposes.
	 */
        void helperShowHide( bool show );
	/**
	 * Returns the main widget for the decoration.
	 */
	QWidget* widget();
	/**
	 * Returns the main widget for the decoration.
	 */
	const QWidget* widget() const;
	/**
	 * Returns the factory that created this decoration.
	 */
        KDecorationFactory* factory() const;
        /**
         * Performs X server grab. It is safe to call it several times in a row.
         */
        void grabXServer();
        /**
         * Ungrabs X server (if the number of ungrab attempts matches the number of grab attempts).
         */
        void ungrabXServer();
    public slots:
	// requests from decoration

	/**
	 * This function can be called by the decoration to request 
	 * closing of the decorated window. Note that closing the window
	 * also involves destroying the decoration.
         * IMPORTANT: This function may destroy the current decoration object,
         * just like showWindowMenu().
	 */
	void closeWindow();
        /*
         * Changes the maximize mode of the decorated window. This function should
         * be preferred to the other maximize() overload for reacting on clicks
         * on the maximize titlebar button.
         * NOTE: This function is new in KDE3.3. In order to support also KDE3.2,
         * it is recommended to use code like this:
         * \code
         * ButtonState button = ... ;
         * #if KDE_IS_VERSION( 3, 3, 0 )
         * maximize( button );
         * #else
         * if( button == MidButton )
	 *     maximize( maximizeMode() ^ MaximizeVertical );
         * else if( button == RightButton )
         *     maximize( maximizeMode() ^ MaximizeHorizontal );
         * else
         *     maximize( maximizeMode() == MaximizeFull ? MaximizeRestore : MaximizeFull );
         * #endif
         * \endcode
         * @since 3.3
         */
#warning Update the docs.
        void maximize( Qt::ButtonState button );
	/**
	 * Set the maximize mode of the decorated window.
	 * @param mode The maximization mode to be set.
	 */
	void maximize( MaximizeMode mode );
	/**
	 * Minimize the decorated window.
	 */
	void minimize();
	/**
	 * Start showing context help in the window (i.e. the mouse will enter
	 * the what's this mode).
	 */
        void showContextHelp();
        /**
         * Moves the window to the given desktop. Use NET::OnAllDesktops for making
         * the window appear on all desktops.
         */
        void setDesktop( int desktop );
	/**
	 * This function toggles the on-all-desktops state of the decorated window.
	 */
        void toggleOnAllDesktops(); // convenience
	/**
	 * This function performs the operation configured as titlebar double click
	 * operation.
	 */
        void titlebarDblClickOperation();
	/**
	 * This function performs the operation configured as titlebar wheel mouse
	 * operation.
         * @param delta the mouse wheel delta
         * @since 3.5
	 */
        void titlebarMouseWheelOperation( int delta );
	/**
	 * Shades or unshades the decorated window.
	 * @param set Whether the window should be shaded
	 */
        void setShade( bool set );
	/**
	 * Sets or reset keeping this window above others.
	 * @param set Whether to keep the window above others
	 */
        void setKeepAbove( bool set );
	/**
	 * Sets or reset keeping this window below others.
	 * @param set Whether to keep the window below others
	 */
        void setKeepBelow( bool set );
        /**
         * @internal
         */
        void emitKeepAboveChanged( bool above ) { emit keepAboveChanged( above ); }
        /**
         * @internal
         */
        void emitKeepBelowChanged( bool below ) { emit keepBelowChanged( below ); }
    private:
	KDecorationBridge* bridge_;
	QWidget* w_;
        KDecorationFactory* factory_;
        friend class KDecorationOptions; // for options_
        static KDecorationOptions* options_;
        KDecorationPrivate* d;
    };

inline
KDecorationDefines::MaximizeMode operator^( KDecorationDefines::MaximizeMode m1, KDecorationDefines::MaximizeMode m2 )
    {
    return KDecorationDefines::MaximizeMode( int(m1) ^ int(m2) );
    }

inline
KDecorationDefines::MaximizeMode operator&( KDecorationDefines::MaximizeMode m1, KDecorationDefines::MaximizeMode m2 )
    {
    return KDecorationDefines::MaximizeMode( int(m1) & int(m2) );
    }

inline
KDecorationDefines::MaximizeMode operator|( KDecorationDefines::MaximizeMode m1, KDecorationDefines::MaximizeMode m2 )
    {
    return KDecorationDefines::MaximizeMode( int(m1) | int(m2) );
    }

inline QWidget* KDecoration::widget()
    {
    return w_;
    }

inline const QWidget* KDecoration::widget() const
    {
    return w_;
    }

inline KDecorationFactory* KDecoration::factory() const
    {
    return factory_;
    }

inline bool KDecoration::isOnAllDesktops() const
    {
    return desktop() == NET::OnAllDesktops;
    }

inline int KDecoration::width() const
    {
    return geometry().width();
    }
    
inline int KDecoration::height() const
    {
    return geometry().height();
    }
    
#endif
