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

#include <kdecorations_export.h>

#include <QtGui/QColor>
#include <QtGui/QFont>
#include <QObject>
#include <QtGui/QIcon>
#include <netwm_def.h>
#include <QtGui/QMouseEvent>

#define KWIN_DECORATION_API_VERSION 1

/**
 * Defines the KDecorationFactory implementation provided by the
 * decoration plugin and  defines a KPluginFactory sub class.
 * The KPluginFactory sub class returns a new instance of the specified KDecorationFactory.
 *
 * @param pluginfactoryname The name to be used for the KPluginFactory, passed to K_PLUGIN_FACTORY
 * @param jsonFile Name of the json file to be compiled into the decoration plugin as metadata
 * @param classname The class name of the KDecorationFactory subclass
 *
 * Example:
 * @code
 * KWIN_DECORATION(MyDecoPluginFactory, "mydeco.json", MyDeco::Factory)
 * @endcode
 *
 * Please refer to the documentation of K_PLUGIN_FACTORY_WITH_JSON in KF5::Service for how to
 * create and use the json file.
 *
 * In case the decoration plugin wants to have more control over the created factory (e.g. a singleton)
 * it is recommended to not use this convenience macro, but specify an own K_PLUGIN_FACTORY. In that
 * case it is also required to add
 * @code
 * K_EXPORT_PLUGIN_VERSION(KWIN_DECORATION_API_VERSION)
 * @endcode
 *
 * to add the correct plugin version. This is also handled by this macro in the default case.
 **/
#define KWIN_DECORATION( pluginfactoryname, json, classname ) \
    QObject *createDecorationFactory(QWidget *, QObject *, const QList<QVariant> &) { return new classname(); } \
    K_PLUGIN_FACTORY_WITH_JSON(pluginfactoryname, json, registerPlugin<classname>(QString(), &createDecorationFactory);) \
    K_EXPORT_PLUGIN_VERSION(KWIN_DECORATION_API_VERSION)

#define KWIN_DECORATION_BRIDGE_API_VERSION 1
extern "C" {
    int decoration_bridge_version();
}

class KConfig;
class KConfigGroup;

/** @defgroup kdecoration KWin decorations library */

/** @addtogroup kdecoration */
/** @{ */

class KDecorationOptionsPrivate;
class KDecorationBridge;
class KDecorationPrivate;
class KDecorationFactory;

/**
 * This class provides a namespace for all decoration related classes.
 * All shared types are defined here.
 */
class KDECORATIONS_EXPORT KDecorationDefines
{
public:
    /**
     * These values represent positions inside an area
     */
    enum Position {
        // without prefix, they'd conflict with Qt::TopLeftCorner etc. :(
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
    enum MaximizeMode {
        MaximizeRestore    = 0, ///< The window is not maximized in any direction.
        MaximizeVertical   = 1, ///< The window is maximized vertically.
        MaximizeHorizontal = 2, ///< The window is maximized horizontally.
        /// Equal to @p MaximizeVertical | @p MaximizeHorizontal
        MaximizeFull = MaximizeVertical | MaximizeHorizontal
    };

    enum QuickTileFlag {
        QuickTileNone = 0,
        QuickTileLeft = 1,
        QuickTileRight = 1<<1,
        QuickTileTop = 1<<2,
        QuickTileBottom = 1<<3,
        QuickTileHorizontal = QuickTileLeft|QuickTileRight,
        QuickTileVertical = QuickTileTop|QuickTileBottom,
        QuickTileMaximize = QuickTileLeft|QuickTileRight|QuickTileTop|QuickTileBottom
    };

    Q_DECLARE_FLAGS(QuickTileMode, QuickTileFlag)

    enum WindowOperation {
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
        ApplicationRulesOp,
        RemoveTabFromGroupOp, // Remove from group
        CloseTabGroupOp, // Close the group
        ActivateNextTabOp, // Move left in the group
        ActivatePreviousTabOp, // Move right in the group
        TabDragOp,
    };
    /**
     * Basic color types that should be recognized by all decoration styles.
     * Decorations are not required to implement all the colors, but for the ones that
     * are implemented the color setting for them should be obeyed.
     */
    enum ColorType {
        ColorTitleBar,   ///< The color for the titlebar
        ColorTitleBlend, ///< The blend color for the titlebar
        ColorFont,       ///< The titlebar text color
        ColorButtonBg,   ///< The color to use for the titlebar buttons
        ColorFrame,      ///< The color for the window frame (border)
        ColorHandle,     ///< The color for the resize handle
        NUM_COLORS       ///< @internal This value may change, do not use
    };

    /**
     * Border size. KDecorationOptions::preferredBorderSize() returns
     * one of these values.
     */
    enum BorderSize {
        BorderTiny,      ///< Minimal borders
        BorderNormal,    ///< Standard size borders, the default setting
        BorderLarge,     ///< Larger borders
        BorderVeryLarge, ///< Very large borders
        BorderHuge,      ///< Huge borders
        BorderVeryHuge,  ///< Very huge borders
        BorderOversized, ///< Oversized borders
        BorderNoSides,   ///< No borders on sides @since 4.11
        BorderNone,      ///< No borders except title @since 4.11
        BordersCount     ///< @internal
    };

    /**
     * Used to find out which features the decoration supports.
     * @see KDecorationFactory::supports()
     */
    enum Ability {
        // announce
        AbilityAnnounceButtons = 0, ///< decoration supports AbilityButton* values (always use)
        // buttons
        AbilityButtonMenu = 1000,   ///< decoration supports the window menu button
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
        AbilityButtonApplicationMenu = 1011,   ///< decoration supports the application menu button
        // compositing
        AbilityProvidesShadow = 3000, ///< The decoration draws its own shadows.
        ///  @since 4.3
        AbilityUsesAlphaChannel = 3001, ///< The decoration isn't clipped to the mask when compositing is enabled.
        ///  The mask is still used to define the input region and the blurred
        ///  region, when the blur plugin is enabled.
        ///  @since 4.3
        AbilityExtendIntoClientArea = 3002, ///< The decoration respects transparentRect()
        ///  @since 4.4
        AbilityUsesBlurBehind = 3003, ///< The decoration wants the background to be blurred, when the blur plugin is enabled.
        /// @since 4.6
        AbilityAnnounceAlphaChannel = 4004, ///< The decoration can tell whether it currently uses an alpha channel or not. Requires AbilityUsesAlphaChannel.
        /// @since 4.10
        // Tabbing
        AbilityTabbing = 4000, ///< The decoration supports tabbing
        // TODO colors for individual button types
        ABILITY_DUMMY = 10000000,
    };

    enum Requirement { REQUIREMENT_DUMMY = 1000000 };

    /**
     * Regions that can be returned by KDecorationUnstable::region().
     */
    enum Region {
        /**
         * This is an invisible input region that can be used to expand the
         * borders by an invisible amount, both inside and outside the
         * decoration. The intended use case is to provide an active border
         * area that's larger than the visible border.
         *
         * The mousePosition() implementation must return correct values
         * for the pixels inside this region.
         *
         * Note that mouse events that occur within this region are not
         * forwarded to the decoration. This may change in the future.
         *
         * @since 4.8
         */
        ExtendedBorderRegion
    };

    /**
     * Enum values to identify the decorations buttons which should be used
     * by the decoration.
     *
     */
    enum DecorationButton {
        /**
         * Invalid button value. A decoration should not create a button for
         * this type.
         */
        DecorationButtonNone,
        DecorationButtonMenu,
        DecorationButtonApplicationMenu,
        DecorationButtonOnAllDesktops,
        DecorationButtonQuickHelp,
        DecorationButtonMinimize,
        DecorationButtonMaximizeRestore,
        DecorationButtonClose,
        DecorationButtonKeepAbove,
        DecorationButtonKeepBelow,
        DecorationButtonShade,
        DecorationButtonResize,
        /**
         * The decoration should create an empty spacer instead of a button for
         * this type.
         */
        DecorationButtonExplicitSpacer
    };

    /**
     * Returns the mimeType used to drag and drop clientGroupItems
     */
    static QString tabDragMimeType();

};

class KDecorationProvides
    : public KDecorationDefines
{
public:
    virtual ~KDecorationProvides() {}
    virtual bool provides(Requirement req) = 0;
};

/**
 * This class holds various configuration settings for the decoration.
 * It is accessible from the decorations either as KDecoration::options()
 * or KDecorationFactory::options().
 */
class KDECORATIONS_EXPORT KDecorationOptions : public QObject, public KDecorationDefines
{
    Q_OBJECT
public:
    KDecorationOptions(QObject *parent = nullptr);
    virtual ~KDecorationOptions();
    /**
     * Returns the color that should be used for the given part of the decoration.
     * The changed flags for this setting is SettingColors.
     *
     * @param type   The requested color type.
     * @param active Whether the color should be for active or inactive windows.
     */
    QColor color(ColorType type, bool active = true) const;
    /**
     * Returns a palette using the given decoration color as the background.
     * The changed flags for this setting is SettingColors.
     *
     * @param type   The requested color type.
     * @param active Whether to return the color for active or inactive windows.
     */
    QPalette palette(ColorType type, bool active = true) const;
    /**
     * Returns the active or inactive decoration font.
     * The changed flags for this setting is SettingFont.
     *
     * @param active Whether to return the color for active or inactive windows.
     * @param small  If @a true, returns a font that's suitable for tool windows.
     */
    QFont font(bool active = true, bool small = false) const;
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
     * to right.
     */
    QList<DecorationButton> titleButtonsLeft() const;
    /**
     * Returns the default left button sequence
     */
    static QList<DecorationButton> defaultTitleButtonsLeft();
    /**
     * If customButtonPositions() returns true, titleButtonsRight
     * returns which buttons should be on the right side of the titlebar from left
     * to right.
     */
    QList<DecorationButton> titleButtonsRight() const;
    /**
     * Returns the default right button sequence.
     */
    static QList<DecorationButton> defaultTitleButtonsRight();
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
    BorderSize preferredBorderSize(KDecorationFactory* factory) const;

    /**
     * @internal
     */
    WindowOperation operationMaxButtonClick(Qt::MouseButtons button) const;

    static KDecorationOptions *self();

    /**
     * Reads the decoration buttons from the @p config group for the @p key and takes care of
     * deserialization of the storage format.
     *
     * @param config The ConfigGroup from which the value should be read
     * @param key The name of the configuration key for the value
     * @param defaultValue The default value to use
     * @return QList< DecorationButton >
     * @see writeDecorationButtons
     */
    static QList<DecorationButton> readDecorationButtons(const KConfigGroup &config, const char *key,
                                                         const QList<DecorationButton> &defaultValue);
    /**
     * @brief Writes the given buttons as @p value to the configuration @p key in the given @p config.
     * Takes care of serializing to the storage format.
     *
     * @param config The ConfigGroup in which the value should be stored
     * @param key The name of the config option
     * @param value The decoration buttons to store
     * @see readDecorationButtons
     */
    static void writeDecorationButtons(KConfigGroup &config, const char *key,
                                       const QList<DecorationButton> &value);

Q_SIGNALS:
    /**
     * @brief Emitted when at least one of the color settings changed.
     *
     * @see color
     */
    void colorsChanged();
    /**
     * @brief Emitted when the active font changed.
     *
     * @see font
     */
    void activeFontChanged();
    /**
     * @brief Emitted when the inactive font changed.
     *
     * @see font
     */
    void inactiveFontChanged();
    /**
     * @brief Emitted when the small active font changed
     *
     * @see font
     */
    void smallActiveFontChanged();
    /**
     * @brief Emitted when the small inactive font changed
     *
     * @see font
     */
    void smallInactiveFontChanged();
    /**
     * @brief Emitted if any of the fonts changed.
     *
     * @see activeFontChanged
     * @see inactiveFontChanged
     * @see smallInactiveFontChanged
     * @see smallInactiveFontChanged
     */
    void fontsChanged();
    /**
     * @brief Emitted when the left title buttons changed.
     *
     * @see titleButtonsLeft
     * @see defaultTitleButtonsLeft
     */
    void leftButtonsChanged();
    /**
     * @brief Emitted when the right title buttons changed.
     *
     * @see titleButtonsRight
     * @see defaultTitleButtonsRight
     */
    void rightButtonsChanged();
    /**
     * @brief Emitted when the custom buttons position setting changed.
     *
     * @see customButtonPositions
     */
    void customButtonPositionsChanged();
    /**
     * @brief Emitted when one of the title buttons relevant settings changed.
     *
     * @see leftButtonsChanged
     * @see rightButtonsChanged
     * @see customButtonPositionsChanged
     */
    void buttonsChanged();
    /**
     * @brief Emitted when show tooltips setting changed.
     *
     * @see showTooltips
     */
    void showTooltipsChanged();
    /**
     * @brief Emitted when the border size setting changed.
     *
     * @see preferredBorderSize
     */
    void borderSizeChanged();
    /**
     * @brief This signal is emitted whenever the configuration changed.
     *
     * A decoration plugin should connect to this signal and evaluate whether
     * some decoration specific settings need to be updated.
     */
    void configChanged();
    /**
     * @brief Emitted when the compositing state in KWin core changes.
     */
    void compositingChanged();

protected:
    /** @internal */
    void setOpMaxButtonLeftClick(WindowOperation op);
    /** @internal */
    void setOpMaxButtonRightClick(WindowOperation op);
    /** @internal */
    void setOpMaxButtonMiddleClick(WindowOperation op);
    /** @internal */
    void setBorderSize(BorderSize bs);
    /** @internal */
    void setCustomButtonPositions(bool b);
    /** @internal */
    void setTitleButtonsLeft(const QList<DecorationButton>& b);
    /** @internal */
    void setTitleButtonsRight(const QList<DecorationButton>& b);
    /**
     * Call to update settings when the config changes.
     * @since 4.0.1
     * @internal
     */
    void updateSettings(KConfig* config);
private:
    /**
     * @internal
     */
    KDecorationOptionsPrivate* d;
    static KDecorationOptions *s_self;
};


/**
 * This is the base class for a decoration object. It provides functions
 * that give various information about the decorated window, and also
 * provides pure virtual functions for controlling the decoration that
 * every decoration should implement.
 */
class KDECORATIONS_EXPORT KDecoration
    : public QObject, public KDecorationDefines
{
    Q_OBJECT
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
    Q_PROPERTY(QString caption READ caption NOTIFY captionChanged)
    Q_PROPERTY(int desktop READ desktop WRITE setDesktop NOTIFY desktopChanged)
    Q_PROPERTY(bool onAllDesktops READ isOnAllDesktops NOTIFY desktopChanged)
    Q_PROPERTY(bool setShade READ isSetShade NOTIFY shadeChanged)
    Q_PROPERTY(QIcon icon READ icon NOTIFY iconChanged)
    Q_PROPERTY(bool maximized READ isMaximized NOTIFY maximizeChanged)
    Q_PROPERTY(bool keepAbove READ keepAbove WRITE setKeepAbove NOTIFY keepAboveChanged)
    Q_PROPERTY(bool keepBelow READ keepBelow WRITE setKeepBelow NOTIFY keepBelowChanged)
    Q_PROPERTY(bool onAllDesktopsAvailable READ isOnAllDesktopsAvailable NOTIFY onAllDesktopsAvailableChanged)
public:
    /**
     * Constructs a KDecoration object. Both the arguments are passed from
     * KDecorationFactory. Note that the initialization code of the decoration
     * should be done in the init() method.
     */
    KDecoration(KDecorationBridge* bridge, KDecorationFactory* factory);
    /**
     * Destroys the KDecoration.
     */
    virtual ~KDecoration();

    // requests from decoration

    /**
     * Returns the KDecorationOptions object, which is used to access
     * configuration settings for the decoration.
     *
     * @deprecated use KDecorationOptions::self()
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
     * @see isMaximized
     */
    MaximizeMode maximizeMode() const;
    /**
     * @brief Convenience method for the case that the decoration is
     * only interested in whether the decoration is in fully maximized
     * or restored state ignoring vertical and horizontal maximized modes.
     *
     * @return bool @c true if maximized fully, @c false otherwise
     * @see maximizeMode
     */
    bool isMaximized() const;
    /**
     * Returns @a true if the decorated window can be minimized by the user.
     */

    /**
     * Returns the current quicktiling mode of the decorated window.
     * (window is places into one of the corners or edges)
     */
    QuickTileMode quickTileMode() const;

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
     * @returns @c true if the decorated window can be put on all desktops
     * @since 5.0
     * @see onAllDesktopsAvailableChanged()
     **/
    bool isOnAllDesktopsAvailable() const;
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
     *  | NET::DockMask | NET::ToolbarMask | NET::MenuMask | NET::DialogMask
     *  | NET::OverrideMask | NET::TopMenuMask | NET::UtilityMask | NET::SplashMask;
     *
     * NET::WindowType type = windowType( supported_types );
     *
     * if ( type == NET::Utility || type == NET::Menu || type == NET::Toolbar )
     *    // ... use smaller decorations for tool window types
     * else
     *    // ... use normal decorations
     * @endcode
     */
    NET::WindowType windowType(unsigned long supported_types) const;
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
     * if ( !f->exists( this )) // destroyed, return immediately
     *     return;
     * button[MenuButton]->setDown(false);
     * \endcode
     */
    void showWindowMenu(const QRect &pos);

    /**
     * Overloaded version of the above.
     */
    void showWindowMenu(QPoint pos);
    /**
     * show application menu at p
     */
    void showApplicationMenu(const QPoint& p);
    /**
     * Returns @a true if menu available for client
     */
    bool menuAvailable() const;
    /**
     * This function performs the given window operation. This function may destroy
     * the current decoration object, just like showWindowMenu().
     */
    void performWindowOperation(WindowOperation op);
    /**
     * If the decoration is non-rectangular, this function needs to be called
     * to set the shape of the decoration.
     *
     * @param reg  The shape of the decoration.
     * @param mode The X11 values Unsorted, YSorted, YXSorted and YXBanded that specify
     *             the sorting of the rectangles, default value is Unsorted.
     */
    void setMask(const QRegion& reg, int mode = 0);
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
    QRegion unobscuredRegion(const QRegion& r) const;
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
     * Returns the rectangle within the window frame that should be transparent.
     * Usually this rectangle is the same as the client area, and it is never
     * larger.
     *
     * If the client has requested that the window frame is extended into the
     * client area, this rectangle is smaller than the client area.
     *
     * If the window frame should cover the whole client area, a null rectangle
     * is returned.
     *
     * @since 4.4
     */
    QRect transparentRect() const;
    /**
     * This function is the default handler for mouse events. All mouse events
     * that are not handled by the decoration itself should be passed to it
     * in order to make work operations like window resizing by dragging borders etc.
     */
    void processMousePressEvent(QMouseEvent* e);

    /**
     * Whether the alpha channel is currently enabled. The value of this property is
     * only relevant in case the decoration provides the AbilityAnnounceAlphaChannel.
     *
     * The compositor can make use of this information to optimize the rendering of the
     * decoration.
     *
     * The default value of this property is @c false. That means if a decoration wants to make
     * use alpha channel it has to call setAlphaEnabled with @c true.
     *
     * @see setAlphaEnabled
     * @see alphaEnabledChanged
     * @since 4.10
     **/
    bool isAlphaEnabled() const;

    /**
     * The recommended palette for the decoration. This can be different per decorated client.
     */
    QPalette palette() const;

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
    virtual Position mousePosition(const QPoint& p) const = 0;

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
    virtual void borders(int& left, int& right, int& top, int& bottom) const = 0;
    /**
     * This method is called by kwin when the style should resize the decoration window.
     * The usual implementation is to resize the main widget of the decoration to the
     * given size.
     *
     * @param s Specifies the new size of the decoration window.
     */
    virtual void resize(const QSize& s) = 0;
    /**
     * This function should return the minimum required size for the decoration.
     * Note that the returned size shouldn't be too large, because it will be
     * used to keep the decorated window at least as large.
     */
    virtual QSize minimumSize() const = 0;

    /**
     * @brief Renders the decoration into the given paint @p device.
     *
     * A decoration can implement this method to provide an optimized way to render the window
     * decoration with compositing enabled. A decoration providing an implementation of this
     * method should best not render to the decoration widget at all. To still schedule repaints
     * it has to invoke the slot @link update() with the geometry that should be repainted in the
     * next painting pass. The compositor might compress several calls to update in one call to
     * render().
     *
     * If compositing is disabled the decoration should render in the normal way, this method is
     * not going to be invoked.
     *
     * The default implementation just delegates to render() on the decoration's widget.
     *
     * @param device The paint device to render to
     * @param sourceRegion The region which should be rendered to the device
     * @see update
     */
    virtual void render(QPaintDevice *device, const QRegion &sourceRegion);

Q_SIGNALS:
    /**
     * This signal is emitted whenever the window either becomes or stops being active.
     * Use isActive() to find out the current state.
     */
    void activeChanged();
    /**
     * This signal is emitted whenever the caption changes. Use caption() to get it.
     */
    void captionChanged();
    /**
     * This signal is emitted whenever the desktop for the window changes. Use
     * desktop() or isOnAllDesktops() to find out the current desktop
     * on which the window is.
     */
    void desktopChanged();
    /**
     * This signal is emitted whenever the window is shaded or unshaded. Use
     * isShade() to get the current state.
     */
    void shadeChanged();
    /**
     * This signal is emitted whenever the window icon changes. Use icon() to get it.
     */
    void iconChanged();
    /**
     * This signal is emitted whenever the maximalization state of the window changes.
     * Use maximizeMode() or isMaximized() to get the current state.
     */
    void maximizeChanged();
    /**
     * This signal is emitted whenever the window's keep-above state changes.
     */
    void keepAboveChanged();
    /**
     * This signal is emitted whenever the window's keep-below state changes.
     */
    void keepBelowChanged();
    /**
     * This signal is emitted whenever the window's keep-above state changes.
     * @deprecated connect to signal without argument
     */
    void keepAboveChanged(bool);
    /**
     * This signal is emitted whenever the window's keep-below state changes.
     * @deprecated connect to signal without argument
     */
    void keepBelowChanged(bool);
    /**
     * This signal is emitted whenever the decorated Client indicated that the
     * available buttons should change (e.g. a closeable window becomes non
     * closable).
     *
     * An implementing class should connect to this signal if it wants to update
     * the buttons accordingly.
     **/
    void decorationButtonsChanged();

    /**
     * This signal is emitted whenever application menu is closed
     * Application menu button may need to be modified on this signal
     */
    void menuHidden();
    /**
     * This signal is emitted whenever application want to show it menu
     */
    void showRequest();
    /**
     * This signal is emitted whenever application menu becomes available
     */
    void appMenuAvailable();
    /**
     * This signal is emitted whenever application menu becomes unavailable
     */
    void appMenuUnavailable();

    /**
     * This signal is emitted whenever the decoration changes it's alpha enabled
     * change. Only relevant in case the decoration provides AbilityAnnounceAlphaChannel.
     *
     * @param enabled The new state of alpha channel usage
     * @see setAlphaEnabled
     * @see isAlphaEnabled
     * @since 4.10
     **/
    void alphaEnabledChanged(bool enabled);
    /**
     * This signal is emitted whenever the decorated window can be put on all desktops or no
     * longer be put on all desktops.
     *
     * @see isOnAllDesktopsAvailable()
     * @since 5.0
     **/
    void onAllDesktopsAvailableChanged();

public:
    /**
     * @internal Reserved.
     */
    // TODO position will need also values for top+left+bottom etc. docking ?
    virtual bool windowDocked(Position side);
    /**
     * This function can return additional padding values that are added outside the
     * borders of the window, and can be used by the decoration if it wants to paint
     * outside the frame.
     *
     * The typical use case is for drawing a drop shadow or glowing effect around the window.
     *
     * The area outside the frame cannot receive input, and when compositing is disabled,
     * painting is clipped to the mask, or the window frame if no mask is defined.
     */
    virtual void padding(int &left, int &right, int &top, int &bottom) const;

    // special

    /**
     * This should be the first function called in init() to specify
     * the main widget of the decoration. The widget should be created
     * with parent specified by initialParentWidget() and flags
     * specified by initialWFlags().
     */
    void setMainWidget(QWidget*);
    /**
     * Convenience functions that creates and sets a main widget as necessary.
     * In such case, it's usually needed to install an event filter
     * on the main widget to receive important events on it.
     *
     * @param flags Additional widget flags for the main widget. Note that only
     *              flags that affect widget drawing are allowed. Window type flags
     *              like WX11BypassWM or WStyle_NoBorder are forbidden.
     */
    void createMainWidget(Qt::WindowFlags flags = 0);
    /**
     * This should be the first function called in init() to specify
     * the main window of the decoration. The window should be created
     * with window flags specified by initialWFlags().
     *
     * This class takes over ownership of the QWindow.
     *
     * A window decoration can either be QWidget based or QWindow based. If it
     * calls both setMainWidget() and setMainWindow() the behavior is undefined.
     */
    void setMainWindow(QWindow *window);
    /**
     * The flags that should be used when creating the main widget.
     * It is possible to add more flags when creating the main widget, but only flags
     * that affect widget drawing are allowed. Window type flags like WX11BypassWM
     * or WStyle_NoBorder are forbidden.
     */
    Qt::WindowFlags initialWFlags() const;
    /**
     * Returns the main widget for the decoration.
     */
    QWidget* widget();
    /**
     * Returns the main widget for the decoration.
     */
    const QWidget* widget() const;
    /**
     * Returns the main window for the decoration.
     */
    QWindow* window();
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
    /**
     * Returns @a true if compositing--and therefore ARGB--is enabled.
     */
    bool compositingActive() const;
    /**
     * Determine which action the user has mapped \p button to. Useful for determining whether
     * a button press was for window tab dragging or for displaying the client menu.
     */
    WindowOperation buttonToWindowOperation(Qt::MouseButtons button);
    /**
     * Convenient method to show the decoration's widget or window.
     **/
    void show();
    /**
     * Convenient method to hide the decoration's widget or window.
     **/
    void hide();
    /**
     * Convenient method to get the geometry of the decoration widget or window.
     **/
    QRect rect() const;


    // Window tabbing

    /**
     * Returns whether or not this client group contains the active client.
     */
    bool isInActiveTabGroup();
    /**
     * Return the amount of tabs in this group
     */
    int tabCount() const;

    /**
     * Return the icon for the tab at index \p idx (\p idx must be smaller than tabCount())
     */
    QIcon icon(int idx) const;

    /**
     * Return the caption for the tab at index \p idx (\p idx must be smaller than tabCount())
     */
    QString caption(int idx) const;

    /**
     * Return the unique id for the tab at index \p idx (\p idx must be smaller than tabCount())
     */
    long tabId(int idx) const;
    /**
     * Returns the id of the currently active client in this group.
     */
    long currentTabId() const;
    /**
     * Activate tab for the window with the id  \p id.
     */
    void setCurrentTab(long id);

    /**
     * Entab windw with id \p A beFORE the window with the id \p B.
     */
    virtual void tab_A_before_B(long A, long B);
    /**
     * Entab windw with id \p A beHIND the window with the id \p B.
     */
    virtual void tab_A_behind_B(long A, long B);
    /**
     * Remove the window with the id \p id from its tabgroup and place it at \p newGeom
     */
    virtual void untab(long id, const QRect& newGeom);

    /**
     * Close the client with the id \p id.
     */
    void closeTab(long id);
    /**
     * Close all windows in this group.
     */
    void closeTabGroup();
    /**
     * Display the right-click client menu belonging to the client at index \p index at the
     * global coordinates specified by \p pos.
     */
    void showWindowMenu(const QPoint& pos, long id);

public: // invokables; runtime resolution
    /**
     * reimplement this invokable to signal the core where the titlebar is (usually PositionTop)
     */
    Q_INVOKABLE KDecorationDefines::Position titlebarPosition();

public Q_SLOTS:
    // requests from decoration

    /**
     * This function can be called by the decoration to request
     * closing of the decorated window. Note that closing the window
     * also involves destroying the decoration.
     * IMPORTANT: This function may destroy the current decoration object,
     * just like showWindowMenu().
     */
    void closeWindow();
    /**
     * Changes the maximize mode of the decorated window. This function should
     * be preferred to the other maximize() overload for reacting on clicks
     * on the maximize titlebar button.
     */
    void maximize(Qt::MouseButtons button);
    /**
     * Set the maximize mode of the decorated window.
     * @param mode The maximization mode to be set.
     */
    void maximize(MaximizeMode mode);
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
    void setDesktop(int desktop);
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
     */
    void titlebarMouseWheelOperation(int delta);
    /**
     * Shades or unshades the decorated window.
     * @param set Whether the window should be shaded
     */
    void setShade(bool set);
    /**
     * Sets or reset keeping this window above others.
     * @param set Whether to keep the window above others
     */
    void setKeepAbove(bool set);
    /**
     * Sets or reset keeping this window below others.
     * @param set Whether to keep the window below others
     */
    void setKeepBelow(bool set);

    /**
     * Schedules a complete repaint of the window decoration.
     *
     * A decoration needs to schedule repaints if compositing is active and it does not render to
     * the decoration's QWidget.
     *
     * The compositor might compress several calls to update into just one call to render().
     *
     * @see render()
     */
    void update();
    /**
     * Overloaded method for convenience.
     *
     * @param rect the geometry to schedule for repaint.
     */
    void update(const QRect &rect);
    /**
     * Overloaded method for convenience.
     *
     * @param region the geometry to schedule for repaint.
     */
    void update(const QRegion &region);

protected Q_SLOTS:
    /**
     * A decoration providing AbilityAnnounceAlphaChannel can use this method to enable/disable the
     * use of alpha channel. This is useful if for a normal window the decoration renders its own
     * shadows or round corners and thus needs alpha channel. But in maximized state the decoration
     * is fully opaque. By disabling the alpha channel the Compositor can optimize the rendering.
     *
     * @param enabled If @c true alpha channel is enabled, if @c false alpha channel is disabled
     * @see isAlphaEnabled
     * @see alphaEnabledChanged
     * @since 4.10
     **/
    void setAlphaEnabled(bool enabled);
    /**
     * This slot can be reimplemented to return the regions defined
     * by KDecorationDefines::Region.
     *
     * The default implementation always returns an empty region.
     *
     * @since 4.8
     */
    QRegion region(KDecorationDefines::Region r);

private:
    KDecorationPrivate* d;

};

inline
KDecorationDefines::MaximizeMode operator^(KDecorationDefines::MaximizeMode m1, KDecorationDefines::MaximizeMode m2)
{
    return KDecorationDefines::MaximizeMode(int(m1) ^ int(m2));
}

inline
KDecorationDefines::MaximizeMode operator&(KDecorationDefines::MaximizeMode m1, KDecorationDefines::MaximizeMode m2)
{
    return KDecorationDefines::MaximizeMode(int(m1) & int(m2));
}

inline
KDecorationDefines::MaximizeMode operator|(KDecorationDefines::MaximizeMode m1, KDecorationDefines::MaximizeMode m2)
{
    return KDecorationDefines::MaximizeMode(int(m1) | int(m2));
}

/** @} */

Q_DECLARE_OPERATORS_FOR_FLAGS(KDecoration::QuickTileMode)
Q_DECLARE_METATYPE(KDecorationDefines::WindowOperation)

#endif
