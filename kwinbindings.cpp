#ifndef NOSLOTS
# define DEF( name, key3, key4, fnSlot ) \
   keys->insert( name, i18n(name), QString::null, key3, key4, this, SLOT(fnSlot) )
#else
# define DEF( name, key3, key4, fnSlot ) \
   keys->insert( name, i18n(name), QString::null, key3, key4 )
#endif

#define WIN KKey::QtWIN

	keys->insert( "Program:kwin", i18n("System") );

	keys->insert( "Group:Navigation", i18n("Navigation") );
	DEF( I18N_NOOP("Walk Through Windows"),                ALT+Qt::Key_Tab, ALT+Qt::Key_Tab, slotWalkThroughWindows() );
	DEF( I18N_NOOP("Walk Through Windows (Reverse)"),      ALT+SHIFT+Qt::Key_Tab, ALT+SHIFT+Qt::Key_Tab, slotWalkBackThroughWindows() );
	DEF( I18N_NOOP("Walk Through Desktops"),               0, WIN+Qt::Key_Tab, slotWalkThroughDesktops() );
	DEF( I18N_NOOP("Walk Through Desktops (Reverse)"),     0, WIN+SHIFT+Qt::Key_Tab, slotWalkBackThroughDesktops() );
	DEF( I18N_NOOP("Walk Through Desktop List"),           CTRL+Qt::Key_Tab, 0, slotWalkThroughDesktopList() );
	DEF( I18N_NOOP("Walk Through Desktop List (Reverse)"), CTRL+SHIFT+Qt::Key_Tab, 0, slotWalkBackThroughDesktopList() );

	keys->insert( "Group:Windows", i18n("Windows") );
	DEF( I18N_NOOP("Window Operations Menu"),              ALT+Qt::Key_F3, ALT+Qt::Key_Menu, slotWindowOperations() );
	DEF( I18N_NOOP("Window Close"),                        ALT+Qt::Key_F4, "Alt+Escape;Alt+F4", slotWindowClose() );
	DEF( I18N_NOOP("Window Maximize"),                     0, WIN+Qt::Key_Plus, slotWindowMaximize() );
	DEF( I18N_NOOP("Window Maximize Vertical"),            0, WIN+Qt::Key_Bar, slotWindowMaximizeVertical() );
	DEF( I18N_NOOP("Window Maximize Horizontal"),          0, WIN+Qt::Key_Equal, slotWindowMaximizeHorizontal() );
	DEF( I18N_NOOP("Window Minimize"),                     0, WIN+Qt::Key_Minus, slotWindowMinimize() );
	DEF( I18N_NOOP("Window Shade"),                        0, WIN+Qt::Key_Underscore, slotWindowShade() );
	DEF( I18N_NOOP("Window Move"),                         0, 0, slotWindowMove() );
	DEF( I18N_NOOP("Window Resize"),                       0, 0, slotWindowResize() );
	DEF( I18N_NOOP("Window Raise"),                        0, 0, slotWindowRaise() );
	DEF( I18N_NOOP("Window Lower"),                        0, 0, slotWindowLower() );
        DEF( I18N_NOOP("Window On All Desktops"),              0, 0, slotWindowOnAllDesktops() );
        DEF( I18N_NOOP("Window Fullscreen"),                   0, 0, slotWindowFullScreen() );
        DEF( I18N_NOOP("Window No Border"),                    0, 0, slotWindowNoBorder() );
        DEF( I18N_NOOP("Window Above Other Windows"),          0, 0, slotWindowAbove() );
        DEF( I18N_NOOP("Window Below Other Windows"),          0, 0, slotWindowBelow() );
	DEF( I18N_NOOP("Toggle Window Raise/Lower"),           0, 0, slotWindowRaiseOrLower() );
        DEF( I18N_NOOP("Activate Window Demanding Attention"), CTRL+ALT+Qt::Key_A, 0, slotActivateAttentionWindow());
        DEF( I18N_NOOP("Window Pack Left"),                    CTRL+ALT+Qt::Key_Left, 0, slotWindowPackLeft() );
        DEF( I18N_NOOP("Window Pack Right"),                   CTRL+ALT+Qt::Key_Right, 0, slotWindowPackRight() );
        DEF( I18N_NOOP("Window Pack Up"),                      CTRL+ALT+Qt::Key_Up, 0, slotWindowPackUp() );
        DEF( I18N_NOOP("Window Pack Down"),                    CTRL+ALT+Qt::Key_Down, 0, slotWindowPackDown() );
        DEF( I18N_NOOP("Window Grow Horizontal"),              SHIFT+ALT+Qt::Key_Left, 0, slotWindowGrowHorizontal() );
        DEF( I18N_NOOP("Window Grow Vertical"),                SHIFT+ALT+Qt::Key_Right, 0, slotWindowGrowVertical() );
        DEF( I18N_NOOP("Window Shrink Horizontal"),            CTRL+ALT+Qt::Key_Up, 0, slotWindowShrinkHorizontal() );
        DEF( I18N_NOOP("Window Shrink Vertical"),              CTRL+ALT+Qt::Key_Down, 0, slotWindowShrinkVertical() );

	keys->insert( "Group:Window Desktop", i18n("Window & Desktop") );
	DEF( I18N_NOOP("Window to Desktop 1"),                 0, WIN+ALT+Qt::Key_F1, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 2"),                 0, WIN+ALT+Qt::Key_F2, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 3"),                 0, WIN+ALT+Qt::Key_F3, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 4"),                 0, WIN+ALT+Qt::Key_F4, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 5"),                 0, WIN+ALT+Qt::Key_F5, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 6"),                 0, WIN+ALT+Qt::Key_F6, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 7"),                 0, WIN+ALT+Qt::Key_F7, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 8"),                 0, WIN+ALT+Qt::Key_F8, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 9"),                 0, WIN+ALT+Qt::Key_F9, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 10"),                0, WIN+ALT+Qt::Key_F10, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 11"),                0, 0, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 12"),                0, 0, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 13"),                0, 0, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 14"),                0, 0, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 15"),                0, 0, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 16"),                0, 0, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Next Desktop"),              0, 0, slotWindowToNextDesktop() );
	DEF( I18N_NOOP("Window to Previous Desktop"),          0, 0, slotWindowToPreviousDesktop() );

	keys->insert( "Group:Desktop Switching", i18n("Desktop Switching") );
	DEF( I18N_NOOP("Switch to Desktop 1"),  CTRL+Qt::Key_F1, WIN+Qt::Key_F1, slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Desktop 2"),  CTRL+Qt::Key_F2, WIN+Qt::Key_F2, slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Desktop 3"),  CTRL+Qt::Key_F3, WIN+Qt::Key_F3, slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Desktop 4"),  CTRL+Qt::Key_F4, WIN+Qt::Key_F4, slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Desktop 5"),  CTRL+Qt::Key_F5, WIN+Qt::Key_F5, slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Desktop 6"),  CTRL+Qt::Key_F6, WIN+Qt::Key_F6, slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Desktop 7"),  CTRL+Qt::Key_F7, WIN+Qt::Key_F7, slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Desktop 8"),  CTRL+Qt::Key_F8, WIN+Qt::Key_F8, slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Desktop 9"),  CTRL+Qt::Key_F9, WIN+Qt::Key_F9, slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Desktop 10"),  CTRL+Qt::Key_F10, WIN+Qt::Key_F10, slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Desktop 11"),  CTRL+Qt::Key_F11, 0, slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Desktop 12"),  CTRL+Qt::Key_F12, 0, slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Desktop 13"),  CTRL+SHIFT+Qt::Key_F1, 0, slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Desktop 14"),  CTRL+SHIFT+Qt::Key_F2, 0, slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Desktop 15"),  CTRL+SHIFT+Qt::Key_F3, 0, slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Desktop 16"),  CTRL+SHIFT+Qt::Key_F4, 0, slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Next Desktop"),              0, 0, slotSwitchDesktopNext() );
	DEF( I18N_NOOP("Switch to Previous Desktop"),          0, 0, slotSwitchDesktopPrevious() );
	DEF( I18N_NOOP("Switch One Desktop to the Right"),     0, 0, slotSwitchDesktopRight() );
	DEF( I18N_NOOP("Switch One Desktop to the Left"),      0, 0, slotSwitchDesktopLeft() );
	DEF( I18N_NOOP("Switch One Desktop Up"),               0, 0, slotSwitchDesktopUp() );
	DEF( I18N_NOOP("Switch One Desktop Down"),             0, 0, slotSwitchDesktopDown() );

	keys->insert( "Group:Miscellaneous", i18n("Miscellaneous") );
	DEF( I18N_NOOP("Mouse Emulation"),                     ALT+Qt::Key_F12, 0, slotMouseEmulation() );
	DEF( I18N_NOOP("Kill Window"),                         ALT+CTRL+Qt::Key_Escape, WIN+CTRL+Qt::Key_Delete, slotKillWindow() );
	DEF( I18N_NOOP("Window Screenshot"),                   ALT+Qt::Key_Print, ALT+Qt::Key_Print, slotGrabWindow() );
	DEF( I18N_NOOP("Desktop Screenshot"),                  CTRL+Qt::Key_Print, WIN+Qt::Key_Print, slotGrabDesktop() );

/*This belongs in taskbar rather than here, so it'll have to wait until after 2.2 is done.
  -- ellis
DEF( I18N_NOOP("Switch to Window 1", WIN+Qt::Key_1"));
DEF( I18N_NOOP("Switch to Window 2", WIN+Qt::Key_2"));
DEF( I18N_NOOP("Switch to Window 3", WIN+Qt::Key_3"));
DEF( I18N_NOOP("Switch to Window 4", WIN+Qt::Key_4"));
DEF( I18N_NOOP("Switch to Window 5", WIN+Qt::Key_5"));
DEF( I18N_NOOP("Switch to Window 6", WIN+Qt::Key_6"));
DEF( I18N_NOOP("Switch to Window 7", WIN+Qt::Key_7"));
DEF( I18N_NOOP("Switch to Window 8", WIN+Qt::Key_8"));
DEF( I18N_NOOP("Switch to Window 9", WIN+Qt::Key_9"));

#ifdef WITH_LABELS
DEF( I18N_NOOP("Window & Taskbar"Group:Window Desktop", 0);
#endif
DEF( I18N_NOOP("Window to Taskbar Position 1", WIN+Qt::Key_Alt+1"));
DEF( I18N_NOOP("Window to Taskbar Position 2", WIN+Qt::Key_Alt+2"));
DEF( I18N_NOOP("Window to Taskbar Position 3", WIN+Qt::Key_Alt+3"));
DEF( I18N_NOOP("Window to Taskbar Position 4", WIN+Qt::Key_Alt+4"));
DEF( I18N_NOOP("Window to Taskbar Position 5", WIN+Qt::Key_Alt+5"));
DEF( I18N_NOOP("Window to Taskbar Position 6", WIN+Qt::Key_Alt+6"));
DEF( I18N_NOOP("Window to Taskbar Position 7", WIN+Qt::Key_Alt+7"));
DEF( I18N_NOOP("Window to Taskbar Position 8", WIN+Qt::Key_Alt+8"));
DEF( I18N_NOOP("Window to Taskbar Position 9", WIN+Qt::Key_Alt+9"));
*/

#undef DEF
#undef WIN
