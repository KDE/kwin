#ifndef OPTIONS_H
#define OPTIONS_H

#include <qfont.h>
#include <qpalette.h>

// increment this when you add a color type (mosfet)
#define KWINCOLORS 8

class Options {
public:

    /*!
      Different focus policies:
      <ul>

      <li>ClickToFocus - Clicking into a window activates it. This is
      also the default.

      <li>FocusFollowsMouse - Moving the mouse pointer actively onto a
      window activates it.

      <li>FocusUnderMouse - The window that happens to be under the
      mouse pointer becomes active.

      <li>FocusStricklyUnderMouse - Only the window under the mouse
      pointer is active. If the mouse points nowhere, nothing has the
      focus. In practice, this is the same as FocusUnderMouse, since
      kdesktop can take the focus.

      Note that FocusUnderMouse and FocusStricklyUnderMouse are not
      particulary useful. They are only provided for old-fashined
      die-hard UNIX people ;-)

      </ul>
     */
    enum FocusPolicy { ClickToFocus, FocusFollowsMouse, FocusUnderMouse, FocusStricklyUnderMouse };
    FocusPolicy focusPolicy;
    
    enum MoveResizeMode { Transparent, Opaque, HalfTransparent };

    /**
     * Basic color types that should be recognized by all decoration styles.
     * Not all styles have to implement all the colors, but for the ones that
     * are implemented you should retrieve them here.
     */
    // increment KWINCOLORS if you add something (mosfet)
    enum ColorType{TitleBar=0, TitleBlend, Font, ButtonFg, ButtonBg,
    ButtonBlend, Frame, Handle};

    MoveResizeMode resizeMode;
    MoveResizeMode moveMode;

    bool focusPolicyIsReasonable() {
        return focusPolicy == ClickToFocus || focusPolicy == FocusFollowsMouse;
    }

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
    const QFont& font(bool active=true);
    // When restarting is implemented this should get called (mosfet).
    void reload();
    
    Options();
    ~Options();
protected:
    QFont activeFont, inactiveFont;
    QColor colors[KWINCOLORS*2];
    QColorGroup *cg[KWINCOLORS*2];
};

extern Options* options;

#endif
