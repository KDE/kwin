#ifndef NOSLOTS
# define DEF( name, key3, key4, fnSlot ) \
   keys->insert( name, i18n(name), QString::null, key3, key4, this, SLOT(fnSlot) )
#else
# define DEF( name, key3, key4, fnSlot ) \
   keys->insert( name, i18n(name), QString::null, key3, key4 )
#endif

#define SHIFT Qt::SHIFT
#define CTRL Qt::CTRL
#define ALT Qt::ALT
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
	DEF( I18N_NOOP("Window Close"),                        "Alt+F4", "Alt+Escape;Alt+F4", slotWindowClose() );
	DEF( I18N_NOOP("Window Maximize"),                     "", "Meta+Plus", slotWindowMaximize() );
	DEF( I18N_NOOP("Window Maximize Vertical"),            "", "Meta+Bar", slotWindowMaximizeVertical() );
	DEF( I18N_NOOP("Window Maximize Horizontal"),          "", "Meta+Equal", slotWindowMaximizeHorizontal() );
	DEF( I18N_NOOP("Window Iconify"),                      "", "Meta+Minus", slotWindowIconify() );
	DEF( I18N_NOOP("Window Shade"),                        "", "Meta+Underscore", slotWindowShade() );
	DEF( I18N_NOOP("Window Move"),                         0, 0, slotWindowMove() );
	DEF( I18N_NOOP("Window Resize"),                       0, 0, slotWindowResize() );
	DEF( I18N_NOOP("Window Raise"),                        0, 0, slotWindowRaise() );
	DEF( I18N_NOOP("Window Lower"),                        0, 0, slotWindowLower() );
	DEF( I18N_NOOP("Window Toggle Raise/Lower"),           0, 0, slotWindowRaiseOrLower() );

	keys->insert( "Group:Window Desktop", i18n("Window & Desktop") );
	DEF( I18N_NOOP("Window to Desktop 1"),                 "", "Meta+Alt+F1", slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 2"),                 "", "Meta+Alt+F2", slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 3"),                 "", "Meta+Alt+F3", slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 4"),                 "", "Meta+Alt+F4", slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 5"),                 "", "Meta+Alt+F5", slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 6"),                 "", "Meta+Alt+F6", slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 7"),                 "", "Meta+Alt+F7", slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 8"),                 "", "Meta+Alt+F8", slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 9"),                 "", "Meta+Alt+F9", slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 10"),                "", "Meta+Alt+F10", slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 11"),                0, 0, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 12"),                0, 0, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 13"),                0, 0, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 14"),                0, 0, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 15"),                0, 0, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 16"),                0, 0, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Next Desktop"),              0, 0, slotWindowToNextDesktop() );
	DEF( I18N_NOOP("Window to Previous Desktop"),          0, 0, slotWindowToPreviousDesktop() );

	keys->insert( "Group:Desktop Switching", i18n("Desktop Switching") );
	DEF( I18N_NOOP("Switch to Desktop 1"),  "CTRL+F1", "Meta+F1", slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Desktop 2"),  "CTRL+F2", "Meta+F2", slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Desktop 3"),  "CTRL+F3", "Meta+F3", slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Desktop 4"),  "CTRL+F4", "Meta+F4", slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Desktop 5"),  "CTRL+F5", "Meta+F5", slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Desktop 6"),  "CTRL+F6", "Meta+F6", slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Desktop 7"),  "CTRL+F7", "Meta+F7", slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Desktop 8"),  "CTRL+F8", "Meta+F8", slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Desktop 9"),  "CTRL+F9", "Meta+F9", slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Desktop 10"),  "CTRL+F10", "Meta+F10", slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Desktop 11"),  "CTRL+F11", "", slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Desktop 12"),  "CTRL+F12", "", slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Desktop 13"),  "CTRL+SHIFT+F1", "", slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Desktop 14"),  "CTRL+SHIFT+F2", "", slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Desktop 15"),  "CTRL+SHIFT+F3", "", slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Desktop 16"),  "CTRL+SHIFT+F4", "", slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Next Desktop"),              0, 0, slotSwitchDesktopNext() );
	DEF( I18N_NOOP("Switch to Previous Desktop"),          0, 0, slotSwitchDesktopPrevious() );
	DEF( I18N_NOOP("Switch One Desktop to the Right"),     0, 0, slotSwitchDesktopRight() );
	DEF( I18N_NOOP("Switch One Desktop to the Left"),      0, 0, slotSwitchDesktopLeft() );
	DEF( I18N_NOOP("Switch One Desktop Up"),               0, 0, slotSwitchDesktopUp() );
	DEF( I18N_NOOP("Switch One Desktop Down"),             0, 0, slotSwitchDesktopDown() );

	keys->insert( "Group:Miscellaneous", i18n("Miscellaneous") );
	DEF( I18N_NOOP("Mouse Emulation"),                     ALT+Qt::Key_F12, 0, slotMouseEmulation() );
	DEF( I18N_NOOP("Kill Window"),                         "CTRL+ALT+Escape", "Meta+Ctrl+Delete", slotKillWindow() );
	DEF( I18N_NOOP("Window Screenshot"),                   ALT+Qt::Key_Print, ALT+Qt::Key_Print, slotGrabWindow() );
	DEF( I18N_NOOP("Desktop Screenshot"),                  "CTRL+Print", "Meta+Print", slotGrabDesktop() );

/*This belongs in taskbar rather than here, so it'll have to wait until after 2.2 is done.
  -- ellis
DEF( I18N_NOOP("Switch to Window 1", "Meta+1"));
DEF( I18N_NOOP("Switch to Window 2", "Meta+2"));
DEF( I18N_NOOP("Switch to Window 3", "Meta+3"));
DEF( I18N_NOOP("Switch to Window 4", "Meta+4"));
DEF( I18N_NOOP("Switch to Window 5", "Meta+5"));
DEF( I18N_NOOP("Switch to Window 6", "Meta+6"));
DEF( I18N_NOOP("Switch to Window 7", "Meta+7"));
DEF( I18N_NOOP("Switch to Window 8", "Meta+8"));
DEF( I18N_NOOP("Switch to Window 9", "Meta+9"));

#ifdef WITH_LABELS
DEF( I18N_NOOP("Window & Taskbar"Group:Window Desktop", 0);
#endif
DEF( I18N_NOOP("Window to Taskbar Position 1", "Meta+Alt+1"));
DEF( I18N_NOOP("Window to Taskbar Position 2", "Meta+Alt+2"));
DEF( I18N_NOOP("Window to Taskbar Position 3", "Meta+Alt+3"));
DEF( I18N_NOOP("Window to Taskbar Position 4", "Meta+Alt+4"));
DEF( I18N_NOOP("Window to Taskbar Position 5", "Meta+Alt+5"));
DEF( I18N_NOOP("Window to Taskbar Position 6", "Meta+Alt+6"));
DEF( I18N_NOOP("Window to Taskbar Position 7", "Meta+Alt+7"));
DEF( I18N_NOOP("Window to Taskbar Position 8", "Meta+Alt+8"));
DEF( I18N_NOOP("Window to Taskbar Position 9", "Meta+Alt+9"));
*/

#undef DEF
#undef SHIFT
#undef CTRL
#undef ALT
#undef WIN
