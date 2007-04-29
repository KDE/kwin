#ifndef NOSLOTS
# define DEF2( name, descr, key, fnSlot ) \
   a = actionCollection->addAction( name );                        \
   a->setText( i18n(descr) );                                      \
   qobject_cast<KAction*>( a )->setGlobalShortcut(KShortcut(key)); \
   connect(a, SIGNAL(triggered(bool)), SLOT(fnSlot))
# define DEF( name, key, fnSlot ) \
   a = actionCollection->addAction( name );                        \
   a->setText( i18n(name) );                                       \
   qobject_cast<KAction*>( a )->setGlobalShortcut(KShortcut(key)); \
   connect(a, SIGNAL(triggered(bool)), SLOT(fnSlot))
#else
# define DEF2( name, descr, key, fnSlot ) \
   a = actionCollection->addAction( name );                        \
   a->setText( i18n(descr) );                                      \
   qobject_cast<KAction*>( a )->setGlobalShortcut(KShortcut(key));
# define DEF( name, key, fnSlot ) \
   a = actionCollection->addAction( name );                        \
   a->setText( i18n(name) );                                       \
   qobject_cast<KAction*>( a )->setGlobalShortcut(KShortcut(key));
#endif

// some shortcuts have Tarzan-speech like names, they need extra normal human descriptions with DEF2()
// the others can use DEF()

        a = actionCollection->addAction( "Program:kwin" );
        a->setText( i18n("System") );

        a = actionCollection->addAction( "Group:Navigation" );
        a->setText( i18n("Navigation") );
	DEF( I18N_NOOP("Walk Through Windows"),               Qt::ALT+Qt::Key_Tab, slotWalkThroughWindows() );
	DEF( I18N_NOOP("Walk Through Windows (Reverse)"),     Qt::ALT+Qt::SHIFT+Qt::Key_Tab, slotWalkBackThroughWindows() );
	DEF( I18N_NOOP("Walk Through Desktops"),              0, slotWalkThroughDesktops() );
	DEF( I18N_NOOP("Walk Through Desktops (Reverse)"),    0, slotWalkBackThroughDesktops() );
	DEF( I18N_NOOP("Walk Through Desktop List"),          0, slotWalkThroughDesktopList() );
	DEF( I18N_NOOP("Walk Through Desktop List (Reverse)"), 0, slotWalkBackThroughDesktopList() );

        a = actionCollection->addAction( "Group:Windows" );
        a->setText( i18n("Windows") );
	DEF( I18N_NOOP("Window Operations Menu"),             Qt::ALT+Qt::Key_F3, slotWindowOperations() );
	DEF2( "Window Close", I18N_NOOP("Close Window"),
            Qt::ALT+Qt::Key_F4, slotWindowClose() );
	DEF2( "Window Maximize", I18N_NOOP("Maximize Window"),
            0, slotWindowMaximize() );
	DEF2( "Window Maximize Vertical", I18N_NOOP("Maximize Window Vertically"),
            0, slotWindowMaximizeVertical() );
	DEF2( "Window Maximize Horizontal", I18N_NOOP("Maximize Window Horizontally"),
            0, slotWindowMaximizeHorizontal() );
	DEF2( "Window Minimize", I18N_NOOP("Minimize Window"),
            0, slotWindowMinimize() );
	DEF2( "Window Shade", I18N_NOOP("Shade Window"),
            0, slotWindowShade() );
	DEF2( "Window Move", I18N_NOOP("Move Window"),
            0, slotWindowMove() );
	DEF2( "Window Resize", I18N_NOOP("Resize Window"),
            0, slotWindowResize() );
	DEF2( "Window Raise", I18N_NOOP("Raise Window"),
            0, slotWindowRaise() );
	DEF2( "Window Lower", I18N_NOOP("Lower Window"),
            0, slotWindowLower() );
	DEF( I18N_NOOP("Toggle Window Raise/Lower"),          0, slotWindowRaiseOrLower() );
        DEF2( "Window Fullscreen", I18N_NOOP("Make Window Fullscreen"),
            0, slotWindowFullScreen() );
        DEF2( "Window No Border", I18N_NOOP("Hide Window Border"),
            0, slotWindowNoBorder() );
        DEF2( "Window Above Other Windows", I18N_NOOP("Keep Window Above Others"),
            0, slotWindowAbove() );
        DEF2( "Window Below Other Windows", I18N_NOOP("Keep Window Below Others"),
            0, slotWindowBelow() );
        DEF( I18N_NOOP("Activate Window Demanding Attention"),
            Qt::CTRL+Qt::ALT+Qt::Key_A, slotActivateAttentionWindow());
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

        a = actionCollection->addAction( "Group:Window Desktop" );
        a->setText( i18n("Window & Desktop") );
        DEF2( "Window On All Desktops", I18N_NOOP("Keep Window on All Desktops"),
            0, slotWindowOnAllDesktops() );
	DEF( I18N_NOOP("Window to Desktop 1"),                0, slotWindowToDesktop1() );
	DEF( I18N_NOOP("Window to Desktop 2"),                0, slotWindowToDesktop2() );
	DEF( I18N_NOOP("Window to Desktop 3"),                0, slotWindowToDesktop3() );
	DEF( I18N_NOOP("Window to Desktop 4"),                0, slotWindowToDesktop4() );
	DEF( I18N_NOOP("Window to Desktop 5"),                0, slotWindowToDesktop5() );
	DEF( I18N_NOOP("Window to Desktop 6"),                0, slotWindowToDesktop6() );
	DEF( I18N_NOOP("Window to Desktop 7"),                0, slotWindowToDesktop7() );
	DEF( I18N_NOOP("Window to Desktop 8"),                0, slotWindowToDesktop8() );
	DEF( I18N_NOOP("Window to Desktop 9"),                0, slotWindowToDesktop9() );
	DEF( I18N_NOOP("Window to Desktop 10"),               0, slotWindowToDesktop10() );
	DEF( I18N_NOOP("Window to Desktop 11"),               0, slotWindowToDesktop11() );
	DEF( I18N_NOOP("Window to Desktop 12"),               0, slotWindowToDesktop12() );
	DEF( I18N_NOOP("Window to Desktop 13"),               0, slotWindowToDesktop13() );
	DEF( I18N_NOOP("Window to Desktop 14"),               0, slotWindowToDesktop14() );
	DEF( I18N_NOOP("Window to Desktop 15"),               0, slotWindowToDesktop15() );
	DEF( I18N_NOOP("Window to Desktop 16"),               0, slotWindowToDesktop16() );
	DEF( I18N_NOOP("Window to Desktop 17"),               0, slotWindowToDesktop17() );
	DEF( I18N_NOOP("Window to Desktop 18"),               0, slotWindowToDesktop18() );
	DEF( I18N_NOOP("Window to Desktop 19"),               0, slotWindowToDesktop19() );
	DEF( I18N_NOOP("Window to Desktop 20"),               0, slotWindowToDesktop20() );
	DEF( I18N_NOOP("Window to Next Desktop"),             0, slotWindowToNextDesktop() );
	DEF( I18N_NOOP("Window to Previous Desktop"),         0, slotWindowToPreviousDesktop() );
	DEF( I18N_NOOP("Window One Desktop to the Right"),    0, slotWindowToDesktopRight() );
	DEF( I18N_NOOP("Window One Desktop to the Left"),     0, slotWindowToDesktopLeft() );
	DEF( I18N_NOOP("Window One Desktop Up"),              0, slotWindowToDesktopUp() );
	DEF( I18N_NOOP("Window One Desktop Down"),            0, slotWindowToDesktopDown() );

        a = actionCollection->addAction( "Group:Desktop Switching" );
        a->setText( i18n("Desktop Switching") );
	DEF( I18N_NOOP("Switch to Desktop 1"), Qt::CTRL+Qt::Key_F1, slotSwitchToDesktop1() );
	DEF( I18N_NOOP("Switch to Desktop 2"), Qt::CTRL+Qt::Key_F2, slotSwitchToDesktop2() );
	DEF( I18N_NOOP("Switch to Desktop 3"), Qt::CTRL+Qt::Key_F3, slotSwitchToDesktop3() );
	DEF( I18N_NOOP("Switch to Desktop 4"), Qt::CTRL+Qt::Key_F4, slotSwitchToDesktop4() );
	DEF( I18N_NOOP("Switch to Desktop 5"), Qt::CTRL+Qt::Key_F5, slotSwitchToDesktop5() );
	DEF( I18N_NOOP("Switch to Desktop 6"), Qt::CTRL+Qt::Key_F6, slotSwitchToDesktop6() );
	DEF( I18N_NOOP("Switch to Desktop 7"), Qt::CTRL+Qt::Key_F7, slotSwitchToDesktop7() );
	DEF( I18N_NOOP("Switch to Desktop 8"), Qt::CTRL+Qt::Key_F8, slotSwitchToDesktop8() );
	DEF( I18N_NOOP("Switch to Desktop 9"), Qt::CTRL+Qt::Key_F9, slotSwitchToDesktop9() );
	DEF( I18N_NOOP("Switch to Desktop 10"), Qt::CTRL+Qt::Key_F10, slotSwitchToDesktop10() );
	DEF( I18N_NOOP("Switch to Desktop 11"), Qt::CTRL+Qt::Key_F11, slotSwitchToDesktop11() );
	DEF( I18N_NOOP("Switch to Desktop 12"), Qt::CTRL+Qt::Key_F12, slotSwitchToDesktop12() );
	DEF( I18N_NOOP("Switch to Desktop 13"), Qt::CTRL+Qt::SHIFT+Qt::Key_F1, slotSwitchToDesktop13() );
	DEF( I18N_NOOP("Switch to Desktop 14"), Qt::CTRL+Qt::SHIFT+Qt::Key_F2, slotSwitchToDesktop14() );
	DEF( I18N_NOOP("Switch to Desktop 15"), Qt::CTRL+Qt::SHIFT+Qt::Key_F3, slotSwitchToDesktop15() );
	DEF( I18N_NOOP("Switch to Desktop 16"), Qt::CTRL+Qt::SHIFT+Qt::Key_F4, slotSwitchToDesktop16() );
	DEF( I18N_NOOP("Switch to Desktop 17"), Qt::CTRL+Qt::SHIFT+Qt::Key_F5, slotSwitchToDesktop17() );
	DEF( I18N_NOOP("Switch to Desktop 18"), Qt::CTRL+Qt::SHIFT+Qt::Key_F6, slotSwitchToDesktop18() );
	DEF( I18N_NOOP("Switch to Desktop 19"), Qt::CTRL+Qt::SHIFT+Qt::Key_F7, slotSwitchToDesktop19() );
	DEF( I18N_NOOP("Switch to Desktop 20"), Qt::CTRL+Qt::SHIFT+Qt::Key_F8, slotSwitchToDesktop20() );
	DEF( I18N_NOOP("Switch to Next Desktop"),             0, slotSwitchDesktopNext() );
	DEF( I18N_NOOP("Switch to Previous Desktop"),         0, slotSwitchDesktopPrevious() );
	DEF( I18N_NOOP("Switch One Desktop to the Right"),    0, slotSwitchDesktopRight() );
	DEF( I18N_NOOP("Switch One Desktop to the Left"),     0, slotSwitchDesktopLeft() );
	DEF( I18N_NOOP("Switch One Desktop Up"),              0, slotSwitchDesktopUp() );
	DEF( I18N_NOOP("Switch One Desktop Down"),            0, slotSwitchDesktopDown() );

        a = actionCollection->addAction( "Group:Miscellaneous" );
        a->setText( i18n("Miscellaneous") );
	DEF( I18N_NOOP("Mouse Emulation"),                    Qt::ALT+Qt::Key_F12, slotMouseEmulation() );
	DEF( I18N_NOOP("Kill Window"),                        Qt::CTRL+Qt::Key_Delete, slotKillWindow() );
	DEF( I18N_NOOP("Window Screenshot"),                  Qt::ALT+Qt::Key_Print, slotGrabWindow() );
	DEF( I18N_NOOP("Desktop Screenshot"),                 Qt::CTRL+Qt::Key_Print, slotGrabDesktop() );
        DEF( I18N_NOOP("Block Global Shortcuts"),           0, slotDisableGlobalShortcuts());

/*This belongs in taskbar rather than here, so it'll have to wait until after 2.2 is done.
  -- ellis
DEF( I18N_NOOP("Switch to Window 1", Qt::META+Qt::Key_1"));
DEF( I18N_NOOP("Switch to Window 2", Qt::META+Qt::Key_2"));
DEF( I18N_NOOP("Switch to Window 3", Qt::META+Qt::Key_3"));
DEF( I18N_NOOP("Switch to Window 4", Qt::META+Qt::Key_4"));
DEF( I18N_NOOP("Switch to Window 5", Qt::META+Qt::Key_5"));
DEF( I18N_NOOP("Switch to Window 6", Qt::META+Qt::Key_6"));
DEF( I18N_NOOP("Switch to Window 7", Qt::META+Qt::Key_7"));
DEF( I18N_NOOP("Switch to Window 8", Qt::META+Qt::Key_8"));
DEF( I18N_NOOP("Switch to Window 9", Qt::META+Qt::Key_9"));

#ifdef WITH_LABELS
DEF( I18N_NOOP("Window & Taskbar"Group:Window Desktop", 0);
#endif
DEF( I18N_NOOP("Window to Taskbar Position 1", Qt::META+Qt::Key_Alt+1"));
DEF( I18N_NOOP("Window to Taskbar Position 2", Qt::META+Qt::Key_Alt+2"));
DEF( I18N_NOOP("Window to Taskbar Position 3", Qt::META+Qt::Key_Alt+3"));
DEF( I18N_NOOP("Window to Taskbar Position 4", Qt::META+Qt::Key_Alt+4"));
DEF( I18N_NOOP("Window to Taskbar Position 5", Qt::META+Qt::Key_Alt+5"));
DEF( I18N_NOOP("Window to Taskbar Position 6", Qt::META+Qt::Key_Alt+6"));
DEF( I18N_NOOP("Window to Taskbar Position 7", Qt::META+Qt::Key_Alt+7"));
DEF( I18N_NOOP("Window to Taskbar Position 8", Qt::META+Qt::Key_Alt+8"));
DEF( I18N_NOOP("Window to Taskbar Position 9", Qt::META+Qt::Key_Alt+9"));
*/

#undef DEF
#undef DEF2
