#ifndef NOSLOTS
# define DEF( name, key3, key4, fnSlot ) \
   keys->insertAction( name, i18n(name), KShortcuts(key3), KShortcuts(key4), this, SLOT(fnSlot) )
#else
# define DEF( name, key3, key4, fnSlot ) \
   keys->insertAction( name, i18n(name), KShortcuts(key3), KShortcuts(key4) )
#endif

	keys->insertLabel( "Program:kwin", i18n("System") );

	keys->insertLabel( "Group:Navigation", i18n("Navigation") );
	DEF( "Walk Through Windows",                "Alt+Tab", "Alt+Tab", slotWalkThroughWindows() );
	DEF( "Walk Through Windows (Reverse)",      "Alt+Shift+Tab", "Alt+Shift+Tab", slotBackWalkThroughWindows() );
	DEF( "Walk Through Desktops",               "", "Meta+Tab", slotWalkThroughDesktops() );
	DEF( "Walk Through Desktops (Reverse)",     "", "Meta+Tab", slotWalkBackThroughDesktops() );
	DEF( "Walk Through Desktop List",           "Ctrl+Tab", "", slotWalkThroughDesktopList() );
	DEF( "Walk Through Desktop List (Reverse)", "Ctrl+Shift+Tab", "", slotWalkBackThroughDesktopList() );

	keys->insertLabel( "Group:Windows", i18n("Windows") );
	DEF( "Window Operations Menu",              "Alt+F3", "Alt+Menu", slotWindowOperations() );
	DEF( "Window Close",                        "Alt+F4", "Alt+Escape", slotWindowClose() );
	DEF( "Window Close (All)",                  "ALT+Shift+F4", "Alt+Shift+Escape", slotWindowCloseAll() );
	DEF( "Window Maximize",                     "", "Meta+Plus", slotWindowMaximize() );
	DEF( "Window Maximize Vertical",            "", "Meta+Bar", slotWindowMaximizeVertical() );
	DEF( "Window Maximize Horizontal",          "", "Meta+Equal", slotWindowMaximizeHorizontal() );
	DEF( "Window Iconify",                      "", "Meta+Minus", slotWindowIconify() );
	//keys->insertAction(i18n("Window Iconify (All)"Window iconify all", Meta+Ctrl+Minus"));
	DEF( "Window Shade",                        "", "Meta+Underscore", slotWindowShade() );
	DEF( "Window Move",                         "", "", slotWindowMove() );
	DEF( "Window Resize",                       "", "", slotWindowResize() );
	DEF( "Window Raise",                        "", "", slotWindowRaise() );
	DEF( "Window Lower",                        "", "", slotWindowLower() );
	DEF( "Window Toggle Raise/Lower",           "", "", slotWindowRaiseOrLower() );

	keys->insertLabel( "Group:Window Desktop", i18n("Window & Desktop") );
	DEF( "Window to Desktop 1",                 "", "Meta+Alt+F1", slotWindowToDesktop(int) );
	DEF( "Window to Desktop 2",                 "", "Meta+Alt+F2", slotWindowToDesktop(int) );
	DEF( "Window to Desktop 3",                 "", "Meta+Alt+F3", slotWindowToDesktop(int) );
	DEF( "Window to Desktop 4",                 "", "Meta+Alt+F4", slotWindowToDesktop(int) );
	DEF( "Window to Desktop 5",                 "", "Meta+Alt+F5", slotWindowToDesktop(int) );
	DEF( "Window to Desktop 6",                 "", "Meta+Alt+F6", slotWindowToDesktop(int) );
	DEF( "Window to Desktop 7",                 "", "Meta+Alt+F7", slotWindowToDesktop(int) );
	DEF( "Window to Desktop 8",                 "", "Meta+Alt+F8", slotWindowToDesktop(int) );
	DEF( "Window to Desktop 9",                 "", "Meta+Alt+F9", slotWindowToDesktop(int) );
	DEF( "Window to Desktop 10",                "", "Meta+Alt+F10", slotWindowToDesktop(int) );
	DEF( "Window to Desktop 11",                "", "", slotWindowToDesktop(int) );
	DEF( "Window to Desktop 12",                "", "", slotWindowToDesktop(int) );
	DEF( "Window to Desktop 13",                "", "", slotWindowToDesktop(int) );
	DEF( "Window to Desktop 14",                "", "", slotWindowToDesktop(int) );
	DEF( "Window to Desktop 15",                "", "", slotWindowToDesktop(int) );
	DEF( "Window to Desktop 16",                "", "", slotWindowToDesktop(int) );
	DEF( "Window to Next Desktop",              "", "", slotWindowToNextDesktop() );
	DEF( "Window to Previous Desktop",          "", "", slotWindowToPreviousDesktop() );

	keys->insertLabel( "Group:Desktop Switching", i18n("Desktop Switching") );
	DEF( "Switch to Desktop 1", "CTRL+F1", "Meta+F1", slotSwitchToDesktop(int) );
	DEF( "Switch to Desktop 2", "CTRL+F2", "Meta+F2", slotSwitchToDesktop(int) );
	DEF( "Switch to Desktop 3", "CTRL+F3", "Meta+F3", slotSwitchToDesktop(int) );
	DEF( "Switch to Desktop 4", "CTRL+F4", "Meta+F4", slotSwitchToDesktop(int) );
	DEF( "Switch to Desktop 5", "CTRL+F5", "Meta+F5", slotSwitchToDesktop(int) );
	DEF( "Switch to Desktop 6", "CTRL+F6", "Meta+F6", slotSwitchToDesktop(int) );
	DEF( "Switch to Desktop 7", "CTRL+F7", "Meta+F7", slotSwitchToDesktop(int) );
	DEF( "Switch to Desktop 8", "CTRL+F8", "Meta+F8", slotSwitchToDesktop(int) );
	DEF( "Switch to Desktop 9", "CTRL+F9", "Meta+F9", slotSwitchToDesktop(int) );
	DEF( "Switch to Desktop 10", "CTRL+F10", "Meta+F10", slotSwitchToDesktop(int) );
	DEF( "Switch to Desktop 11", "CTRL+F11", "", slotSwitchToDesktop(int) );
	DEF( "Switch to Desktop 12", "CTRL+F12", "", slotSwitchToDesktop(int) );
	DEF( "Switch to Desktop 13", "CTRL+SHIFT+F1", "", slotSwitchToDesktop(int) );
	DEF( "Switch to Desktop 14", "CTRL+SHIFT+F2", "", slotSwitchToDesktop(int) );
	DEF( "Switch to Desktop 15", "CTRL+SHIFT+F3", "", slotSwitchToDesktop(int) );
	DEF( "Switch to Desktop 16", "CTRL+SHIFT+F4", "", slotSwitchToDesktop(int) );
	DEF( "Switch to Next Desktop", "", "", slotSwitchDesktopNext() );
	DEF( "Switch to Previous Desktop", "", "", slotSwitchDesktopPrevious() );
	DEF( "Switch One Desktop to the Right", "", "", slotSwitchDesktopRight() );
	DEF( "Switch One Desktop to the Left", "", "", slotSwitchDesktopLeft() );
	DEF( "Switch One Desktop Up", "", "", slotSwitchDesktopUp() );
	DEF( "Switch One Desktop Down", "", "", slotSwitchDesktopDown() );

	keys->insertLabel( "Group:Miscellaneous", i18n("Miscellaneous") );
	DEF( "Mouse Emulation", "ALT+F12", "", slotMouseEmulation() );
	DEF( "Kill Window", "CTRL+ALT+Escape", "Meta+Ctrl+Delete", slotKillWindow() );
	DEF( "Window Screenshot", "Alt+Print", "Alt+Print", slotGrabWindow() );
	DEF( "Desktop Screenshot", "CTRL+Print", "Meta+Print", slotGrabDesktop() );

/*This belongs in taskbar rather than here, so it'll have to wait until after 2.2 is done.
  -- ellis
DEF( "Switch to Window 1", "Meta+1"));
DEF( "Switch to Window 2", "Meta+2"));
DEF( "Switch to Window 3", "Meta+3"));
DEF( "Switch to Window 4", "Meta+4"));
DEF( "Switch to Window 5", "Meta+5"));
DEF( "Switch to Window 6", "Meta+6"));
DEF( "Switch to Window 7", "Meta+7"));
DEF( "Switch to Window 8", "Meta+8"));
DEF( "Switch to Window 9", "Meta+9"));

#ifdef WITH_LABELS
DEF( "Window & Taskbar"Group:Window Desktop", 0);
#endif
DEF( "Window to Taskbar Position 1", "Meta+Alt+1"));
DEF( "Window to Taskbar Position 2", "Meta+Alt+2"));
DEF( "Window to Taskbar Position 3", "Meta+Alt+3"));
DEF( "Window to Taskbar Position 4", "Meta+Alt+4"));
DEF( "Window to Taskbar Position 5", "Meta+Alt+5"));
DEF( "Window to Taskbar Position 6", "Meta+Alt+6"));
DEF( "Window to Taskbar Position 7", "Meta+Alt+7"));
DEF( "Window to Taskbar Position 8", "Meta+Alt+8"));
DEF( "Window to Taskbar Position 9", "Meta+Alt+9"));
*/

#undef DEF
