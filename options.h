/*****************************************************************
kwin - the KDE window manager
								
Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
******************************************************************/
#ifndef OPTIONS_H
#define OPTIONS_H

#include <qobject.h>
#include <qfont.h>
#include <qpalette.h>
#include <qstringlist.h>

// increment this when you add a color type (mosfet)
#define KWINCOLORS 6

namespace KWinInternal {

class OptionsPrivate;
// NOTE: this class has to keep binary compatibility, just like other
// KWin classes accessible from the plugins
class Options : public QObject {
    Q_OBJECT
public:

    Options();
    ~Options();


    /*!
      Different focus policies:
      <ul>

      <li>ClickToFocus - Clicking into a window activates it. This is
      also the default.

      <li>FocusFollowsMouse - Moving the mouse pointer actively onto a
      normal window activates it. For convenience, the desktop and
      windows on the dock are excluded. They require clicking.

      <li>FocusUnderMouse - The window that happens to be under the
      mouse pointer becomes active. The invariant is: no window can
      have focus that is not under the mouse. This also means that
      Alt-Tab won't work properly and popup dialogs are usually
      unsable with the keyboard. Note that the desktop and windows on
      the dock are excluded for convenience. They get focus only when
      clicking on it.

      <li>FocusStrictlyUnderMouse - this is even worse than
      FocusUnderMouse. Only the window under the mouse pointer is
      active. If the mouse points nowhere, nothing has the focus. If
      the mouse points onto the desktop, the desktop has focus. The
      same holds for windows on the dock.

      Note that FocusUnderMouse and FocusStrictlyUnderMouse are not
      particulary useful. They are only provided for old-fashined
      die-hard UNIX people ;-)

      </ul>
     */
    enum FocusPolicy { ClickToFocus, FocusFollowsMouse, FocusUnderMouse, FocusStrictlyUnderMouse };
    FocusPolicy focusPolicy;


    /**
       Whether clicking on a window raises it in FocusFollowsMouse
       mode or not.
     */
    bool clickRaise;

    /**
       whether autoraise is enabled FocusFollowsMouse mode or not.
     */
    bool autoRaise;

    /**
       autoraise interval
     */
    int autoRaiseInterval;

    /**
       Whether shade hover is enabled or not
     */
    bool shadeHover;

    /**
       shade hover interval
     */
    int shadeHoverInterval;

    /**
       Different Alt-Tab-Styles:
       <ul>

       <li> KDE - the recommended KDE style. Alt-Tab opens a nice icon
       box that makes it easy to select the window you want to tab
       to. The order automatically adjusts to the most recently used
       windows. Note that KDE style does not work with the
       FocusUnderMouse and FocusStrictlyUnderMouse focus
       policies. Choose ClickToFocus or FocusFollowsMouse instead.

       <li> CDE - the old-fashion CDE style. Alt-Tab cycles between
       the windows in static order. The current window gets raised,
       the previous window gets lowered.

       </ul>
     */
    enum AltTabStyle { KDE, CDE };
    AltTabStyle altTabStyle;

    /**
     * Xinerama options
     */
    bool xineramaEnabled;
    bool xineramaPlacementEnabled;
    bool xineramaMovementEnabled;
    bool xineramaMaximizeEnabled;

    /**
      Number of desktop rowsd
     */
    int desktopRows;


    /**
       MoveResizeMode, either Tranparent or Opaque.
     */
    enum MoveResizeMode { Transparent, Opaque };

    MoveResizeMode resizeMode;
    MoveResizeMode moveMode;

    /**
     * Can maximized windows be moved or resized
     */
    bool moveResizeMaximizedWindows;

    /**
     * Placement policies. How workspace decides the way windows get positioned
     * on the screen. The better the policy, the heavier the resource use.
     * Normally you don't have to worry. What the WM adds to the startup time
     * is nil compared to the creation of the window itself in the memory
     */
    enum PlacementPolicy { Random, Smart, Cascade };
    PlacementPolicy placement;

    bool focusPolicyIsReasonable() {
        return focusPolicy == ClickToFocus || focusPolicy == FocusFollowsMouse;
    }

    /**
     * Basic color types that should be recognized by all decoration styles.
     * Not all styles have to implement all the colors, but for the ones that
     * are implemented you should retrieve them here.
     */
    // increment KWINCOLORS if you add something (mosfet)
    enum ColorType{TitleBar=0, TitleBlend, Font, ButtonBg, Frame, Handle};

    /**
     * Return the color for the given decoration.
     */
    const QColor& color(ColorType type, bool active=true);
    /**
     * Return a colorgroup using the given decoration color as the background
     */
    const QColorGroup& colorGroup(ColorType type, bool active=true);
    /**
     * Return the active or inactive decoration font.
     */
    const QFont& font(bool active=true, bool small = false);

    /**
     * whether we animate the shading of windows to titlebar or not
     */
    bool animateShade;

    /**
     * the size of the zone that triggers snapping on desktop borders
     */
    int borderSnapZone;

    /**
     * the number of animation steps (would this be general?)
     */
    int windowSnapZone;


    /**
     * snap only when windows will overlap
     */
    bool snapOnlyWhenOverlapping;

    /**
     * whether we animate the minimization of windows or not
     */
    bool  animateMinimize;

    /**
     * Animation speed (0 .. 10 )
    */
    int animateMinimizeSpeed;


    /**
     * List of window classes to ignore PPosition size hint
     */
    QStringList ignorePositionClasses;

    // mouse bindings

    enum WindowOperation{
	MaximizeOp = 5000,
	RestoreOp,
	IconifyOp,
	MoveOp,
	ResizeOp,
	CloseOp,
	StickyOp,
	ShadeOp,
	StaysOnTopOp,
	OperationsOp,
	ToggleStoreSettingsOp,
	HMaximizeOp,
	VMaximizeOp,
	LowerOp,
	NoOp
    };

    WindowOperation operationTitlebarDblClick() { return OpTitlebarDblClick; }

    enum MouseCommand {
    MouseRaise, MouseLower, MouseOperationsMenu, MouseToggleRaiseAndLower,
    MouseActivateAndRaise, MouseActivateAndLower, MouseActivate,
    MouseActivateRaiseAndPassClick, MouseActivateAndPassClick,
    MouseMove, MouseResize,
    MouseShade,
    MouseNothing
    };

    MouseCommand commandActiveTitlebar1() { return CmdActiveTitlebar1; }
    MouseCommand commandActiveTitlebar2() { return CmdActiveTitlebar2; }
    MouseCommand commandActiveTitlebar3() { return CmdActiveTitlebar3; }
    MouseCommand commandInactiveTitlebar1() { return CmdInactiveTitlebar1; }
    MouseCommand commandInactiveTitlebar2() { return CmdInactiveTitlebar2; }
    MouseCommand commandInactiveTitlebar3() { return CmdInactiveTitlebar3; }
    MouseCommand commandWindow1() { return CmdWindow1; }
    MouseCommand commandWindow2() { return CmdWindow2; }
    MouseCommand commandWindow3() { return CmdWindow3; }
    MouseCommand commandAll1() { return CmdAll1; }
    MouseCommand commandAll2() { return CmdAll2; }
    MouseCommand commandAll3() { return CmdAll3; }
    uint keyCmdAllModKey() { return CmdAllModKey; }


    static WindowOperation windowOperation(const QString &name );
    static MouseCommand mouseCommand(const QString &name);
    
    /**
    * @returns true if the style should use custom button positions
    * @see #titleButtonsLeft
    * @see #titleButtonsRight
    */
    bool customButtonPositions();
    /**
    * If @ref customButtonPositions returns true, titleButtonsLeft
    * returns which buttons should be on the left side of the titlebar from left
    * to right. Characters in the returned string have this meaning :
    * @li 'M' menu button
    * @li 'S' sticky button
    * @li 'H' quickhelp button
    * @li 'I' iconify ( minimize ) button
    * @li 'A' maximize button
    * @li 'X' close button
    * @li '_' spacer
    *
    * The default ( which is also returned if customButtonPositions returns false )
    * is "MS".
    */
    QString titleButtonsLeft();
    /**
    * If @ref customButtonPositions returns true, titleButtonsRight
    * returns which buttons should be on the right side of the titlebar from left
    * to right. Characters in the return string have the same meaning like
    * in @ref titleButtonsLeft .
    *
    * The default ( which is also returned if customButtonPositions returns false )
    * is "HIAX".
    */
    QString titleButtonsRight();

    /**
    * @returns true if the style should use tooltips for window buttons
    */
    bool showTooltips();

    /**
    * @returns true if window button tooltips should use a fade-in effect
    * @see #animateTooltips
    * @see #showTooltips
    */
    bool fadeTooltips();

    /**
    * @returns true if window button tooltips should use an animated effect.
    * When this is true, tooltips should use a animated scroll effect. If
    * fadeToolTips is also true, tooltips should be faded-in instead.
    * @see #fadeTooltips
    * @see #showTooltips
    */
    bool animateTooltips();


public slots:
    void reload();

signals:
    void resetPlugin();
    void resetClients();

protected:
    QFont activeFont, inactiveFont, activeFontSmall, inactiveFontSmall;

private:



    WindowOperation OpTitlebarDblClick;

    // mouse bindings
    MouseCommand CmdActiveTitlebar1;
    MouseCommand CmdActiveTitlebar2;
    MouseCommand CmdActiveTitlebar3;
    MouseCommand CmdInactiveTitlebar1;
    MouseCommand CmdInactiveTitlebar2;
    MouseCommand CmdInactiveTitlebar3;
    MouseCommand CmdWindow1;
    MouseCommand CmdWindow2;
    MouseCommand CmdWindow3;
    MouseCommand CmdAll1;
    MouseCommand CmdAll2;
    MouseCommand CmdAll3;
    uint CmdAllModKey;

    OptionsPrivate* d;
};

extern Options* options;

}; // namespace

#endif
