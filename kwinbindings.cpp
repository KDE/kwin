#ifndef NOSLOTS
# define DEF2( name, descr, key, fnSlot ) \
   keys->insert( name, i18n(descr), QString::null, key, this, SLOT(fnSlot) )
# define DEF( name, key, fnSlot ) \
   keys->insert( name, i18n(name), QString::null, key, this, SLOT(fnSlot) )
#else
# define DEF2( name, descr, key, fnSlot ) \
   keys->insert( name, i18n(descr), QString::null, key )
# define DEF( name, key, fnSlot ) \
   keys->insert( name, i18n(name), QString::null, key )
#endif

#define WIN Qt::META
#define ALT Qt::ALT
#define SHIFT Qt::SHIFT
#define CTRL Qt::CTRL

// some shortcuts have Tarzan-speech like names, they need extra normal human descriptions with DEF2()
// the others can use DEF()

	keys->insert( "Program:kwin", i18n("System") );

	keys->insert( "Group:Navigation", i18n("Navigation") );
	DEF( I18N_NOOP("Walk Through Windows"),                ALT+Qt::Key_Tab, slotWalkThroughWindows() );
	DEF( I18N_NOOP("Walk Through Windows (Reverse)"),      ALT+SHIFT+Qt::Key_Tab, slotWalkBackThroughWindows() );
	DEF( I18N_NOOP("Walk Through Desktops"),               WIN+Qt::Key_Tab, slotWalkThroughDesktops() );
	DEF( I18N_NOOP("Walk Through Desktops (Reverse)"),     WIN+SHIFT+Qt::Key_Tab, slotWalkBackThroughDesktops() );
	DEF( I18N_NOOP("Walk Through Desktop List"),           0, slotWalkThroughDesktopList() );
	DEF( I18N_NOOP("Walk Through Desktop List (Reverse)"), 0, slotWalkBackThroughDesktopList() );

	keys->insert( "Group:Windows", i18n("Windows") );
	DEF( I18N_NOOP("Window Operations Menu"),              ALT+Qt::Key_F3, ALT+Qt::Key_Menu, slotWindowOperations() );
	DEF2( "Window Close", I18N_NOOP("Close Window"),
            ALT+Qt::Key_F4, "Alt+Escape;Alt+F4", slotWindowClose() );
	DEF2( "Window Maximize", I18N_NOOP("Maximize Window"),
            WIN+Qt::Key_Plus, slotWindowMaximize() );
	DEF2( "Window Maximize Vertical", I18N_NOOP("Maximize Window Vertically"),
            WIN+Qt::Key_Bar, slotWindowMaximizeVertical() );
	DEF2( "Window Maximize Horizontal", I18N_NOOP("Maximize Window Horizontally"),
            WIN+Qt::Key_Equal, slotWindowMaximizeHorizontal() );
	DEF2( "Window Minimize", I18N_NOOP("Minimize Window"),
            WIN+Qt::Key_Minus, slotWindowMinimize() );
	DEF2( "Window Shade", I18N_NOOP("Shade Window"),
            WIN+Qt::Key_Underscore, slotWindowShade() );
	DEF2( "Window Move", I18N_NOOP("Move Window"),
            0, slotWindowMove() );
	DEF2( "Window Resize", I18N_NOOP("Resize Window"),
            0, slotWindowResize() );
	DEF2( "Window Raise", I18N_NOOP("Raise Window"),
            0, slotWindowRaise() );
	DEF2( "Window Lower", I18N_NOOP("Lower Window"),
            0, slotWindowLower() );
	DEF( I18N_NOOP("Toggle Window Raise/Lower"),           0, slotWindowRaiseOrLower() );
        DEF2( "Window Fullscreen", I18N_NOOP("Make Window Fullscreen"),
            0, slotWindowFullScreen() );
        DEF2( "Window No Border", I18N_NOOP("Hide Window Border"),
            0, slotWindowNoBorder() );
        DEF2( "Window Above Other Windows", I18N_NOOP("Keep Window Above Others"),
            0, slotWindowAbove() );
        DEF2( "Window Below Other Windows", I18N_NOOP("Keep Window Below Others"),
            0, slotWindowBelow() );
        DEF( I18N_NOOP("Activate Window Demanding Attention"), 
            CTRL+ALT+Qt::Key_A, slotActivateAttentionWindow());
        DEF( I18N_NOOP("Setup Window Shortcut"), 
            0, slotSetupWindowShortcut());
        DEF2( "Window Pack Right", I18N_NOOP("Pack Window to the Right"),
            0, slotWindowPackRight() );
        DEF2( "Window Pack Left", I18N_NOOP("Pack Window to the Left"),
            0, slotWindowPackLeft() );
        DEF2( "Window Pack Up", I18N_NOOP("Pack Window Up"),
            0, slotWindowPackUp() );
        DEF2( "Window Pack Down", I18N_NOOP("Pack Window Down"),
            0, slotWindowPackDown() );
        DEF2( "Window Grow Horizontal", I18N_NOOP("Pack Grow Window Horizontally"),
            0, slotWindowGrowHorizontal() );
        DEF2( "Window Grow Vertical", I18N_NOOP("Pack Grow Window Vertically"),
            0, slotWindowGrowVertical() );
        DEF2( "Window Shrink Horizontal", I18N_NOOP("Pack Shrink Window Horizontally"),
            0, slotWindowShrinkHorizontal() );
        DEF2( "Window Shrink Vertical", I18N_NOOP("Pack Shrink Window Vertically"),
            0, slotWindowShrinkVertical() );

	keys->insert( "Group:Window Desktop", i18n("Window & Desktop") );
        DEF2( "Window On All Desktops", I18N_NOOP("Keep Window on All Desktops"),
            0, slotWindowOnAllDesktops() );
	DEF( I18N_NOOP("Window to Desktop 1"),                 WIN+ALT+Qt::Key_F1, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 2"),                 WIN+ALT+Qt::Key_F2, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 3"),                 WIN+ALT+Qt::Key_F3, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 4"),                 WIN+ALT+Qt::Key_F4, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 5"),                 WIN+ALT+Qt::Key_F5, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 6"),                 WIN+ALT+Qt::Key_F6, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 7"),                 WIN+ALT+Qt::Key_F7, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 8"),                 WIN+ALT+Qt::Key_F8, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 9"),                 WIN+ALT+Qt::Key_F9, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 10"),                WIN+ALT+Qt::Key_F10, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 11"),                0, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 12"),                0, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 13"),                0, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 14"),                0, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 15"),                0, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 16"),                0, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 17"),                0, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 18"),                0, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 19"),                0, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Desktop 20"),                0, slotWindowToDesktop(int) );
	DEF( I18N_NOOP("Window to Next Desktop"),              0, slotWindowToNextDesktop() );
	DEF( I18N_NOOP("Window to Previous Desktop"),          0, slotWindowToPreviousDesktop() );
	DEF( I18N_NOOP("Window One Desktop to the Right"),     0, slotWindowToDesktopRight() );
	DEF( I18N_NOOP("Window One Desktop to the Left"),      0, slotWindowToDesktopLeft() );
	DEF( I18N_NOOP("Window One Desktop Up"),               0, slotWindowToDesktopUp() );
	DEF( I18N_NOOP("Window One Desktop Down"),             0, slotWindowToDesktopDown() );

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
	DEF( I18N_NOOP("Switch to Desktop 11"),  CTRL+Qt::Key_F11, slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Desktop 12"),  CTRL+Qt::Key_F12, slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Desktop 13"),  CTRL+SHIFT+Qt::Key_F1, slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Desktop 14"),  CTRL+SHIFT+Qt::Key_F2, slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Desktop 15"),  CTRL+SHIFT+Qt::Key_F3, slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Desktop 16"),  CTRL+SHIFT+Qt::Key_F4, slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Desktop 17"),  CTRL+SHIFT+Qt::Key_F5, slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Desktop 18"),  CTRL+SHIFT+Qt::Key_F6, slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Desktop 19"),  CTRL+SHIFT+Qt::Key_F7, slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Desktop 20"),  CTRL+SHIFT+Qt::Key_F8, slotSwitchToDesktop(int) );
	DEF( I18N_NOOP("Switch to Next Desktop"),              0, slotSwitchDesktopNext() );
	DEF( I18N_NOOP("Switch to Previous Desktop"),          0, slotSwitchDesktopPrevious() );
	DEF( I18N_NOOP("Switch One Desktop to the Right"),     0, slotSwitchDesktopRight() );
	DEF( I18N_NOOP("Switch One Desktop to the Left"),      0, slotSwitchDesktopLeft() );
	DEF( I18N_NOOP("Switch One Desktop Up"),               0, slotSwitchDesktopUp() );
	DEF( I18N_NOOP("Switch One Desktop Down"),             0, slotSwitchDesktopDown() );

	keys->insert( "Group:Miscellaneous", i18n("Miscellaneous") );
	DEF( I18N_NOOP("Mouse Emulation"),                     ALT+Qt::Key_F12, slotMouseEmulation() );
	DEF( I18N_NOOP("Kill Window"),                         ALT+CTRL+Qt::Key_Escape, WIN+CTRL+Qt::Key_Delete, slotKillWindow() );
	DEF( I18N_NOOP("Window Screenshot"),                   ALT+Qt::Key_Print, slotGrabWindow() );
	DEF( I18N_NOOP("Desktop Screenshot"),                  CTRL+Qt::Key_Print, WIN+Qt::Key_Print, slotGrabDesktop() );
#ifdef IN_KWIN
        {
        KGlobalAccel* keys = disable_shortcuts_keys;
#endif
        DEF( I18N_NOOP("Block Global Shortcuts"),            0, slotDisableGlobalShortcuts());
#ifdef IN_KWIN
        }
#endif

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
#undef DEF2
#undef WIN
#undef ALT
#undef SHIFT
#undef CTRL
