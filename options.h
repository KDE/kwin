#ifndef OPTIONS_H
#define OPTIONS_H


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

    bool focusPolicyIsReasonable() {
	return focusPolicy == ClickToFocus || focusPolicy == FocusFollowsMouse;
    }

    Options(){
	focusPolicy = ClickToFocus;
    }

};

extern Options* options;

#endif
