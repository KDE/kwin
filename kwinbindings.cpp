#ifdef WITH_LABELS
 keys->insertItem(i18n("System"), "Program:kwin", 0);
#endif

#ifdef WITH_LABELS
 keys->insertItem(i18n("Navigation"), "Group:Navigation", 0);
#endif
 keys->insertItem(i18n("Walk through Windows"), "Walk through windows", KKey("ALT+Tab"), KKey("Alt+Tab"));
 keys->insertItem(i18n("Walk through Windows (Reverse)"), "Walk back through windows", KKey("SHIFT+ALT+Tab"), KKey("Alt+Shift+Tab"));
 keys->insertItem(i18n("Walk through Desktops"), "Walk through desktops", KKey(), KKey("Meta+Tab"));
 keys->insertItem(i18n("Walk through Desktops (Reverse)"), "Walk back through desktops", KKey(), KKey("Meta+Shift+Tab"));
 keys->insertItem(i18n("Walk through Desktop List"), "Walk through desktop list", KKey("CTRL+Tab"), KKey());
 keys->insertItem(i18n("Walk through Desktop List (Reverse)"), "Walk back through desktop list", KKey("SHIFT+CTRL+Tab"), KKey());

#ifdef WITH_LABELS
 keys->insertItem(i18n("Windows"), "Group:Windows", 0);
#endif
 keys->insertItem(i18n("Window Operations Menu"), "Pop-up window operations menu", KKey("ALT+F3"), KKey("Alt+Space"));
 keys->insertItem(i18n("Window Close"), "Window close", KKey("ALT+F4"), KKey("Alt+Escape"));
 keys->insertItem(i18n("Window Close (All)"), "Window close all", KKey("ALT+Shift+F4"), KKey("Alt+Shift+Escape"));
 keys->insertItem(i18n("Window Maximize"), "Window maximize", KKey(), KKey("Meta+Plus"));
 keys->insertItem(i18n("Window Maximize Vertical"), "Window maximize vertical", KKey(), KKey("Meta+Bar"));
 keys->insertItem(i18n("Window Maximize Horizontal"), "Window maximize horizontal", KKey(), KKey("Meta+Equal"));
 keys->insertItem(i18n("Window Iconify"), "Window iconify", KKey(), KKey("Meta+Minus"));
 //keys->insertItem(i18n("Window Iconify (All)"), "Window iconify all", KKey(), KKey("Meta+Ctrl+Minus"));
 keys->insertItem(i18n("Window Shade"), "Window shade", KKey(), KKey("Meta+Underscore"));
 keys->insertItem(i18n("Window Move"), "Window move", 0);
 keys->insertItem(i18n("Window Resize"), "Window resize", 0);
 keys->insertItem(i18n("Window Raise"), "Window raise", 0);
 keys->insertItem(i18n("Window Lower"), "Window lower", 0);
 keys->insertItem(i18n("Window Raise or Lower"), "Toggle raise and lower", 0);
/*
This belongs in taskbar rather than here, so it'll have to wait until after 2.2 is done.
  -- ellis
 keys->insertItem(i18n("Switch to Window 1"), "Switch to Window 1", KKey(), KKey("Meta+1"));
 keys->insertItem(i18n("Switch to Window 2"), "Switch to Window 2", KKey(), KKey("Meta+2"));
 keys->insertItem(i18n("Switch to Window 3"), "Switch to Window 3", KKey(), KKey("Meta+3"));
 keys->insertItem(i18n("Switch to Window 4"), "Switch to Window 4", KKey(), KKey("Meta+4"));
 keys->insertItem(i18n("Switch to Window 5"), "Switch to Window 5", KKey(), KKey("Meta+5"));
 keys->insertItem(i18n("Switch to Window 6"), "Switch to Window 6", KKey(), KKey("Meta+6"));
 keys->insertItem(i18n("Switch to Window 7"), "Switch to Window 7", KKey(), KKey("Meta+7"));
 keys->insertItem(i18n("Switch to Window 8"), "Switch to Window 8", KKey(), KKey("Meta+8"));
 keys->insertItem(i18n("Switch to Window 9"), "Switch to Window 9", KKey(), KKey("Meta+9"));
*/

#ifdef WITH_LABELS
 keys->insertItem(i18n("Window & Desktop"), "Group:Window Desktop", 0);
#endif
 keys->insertItem(i18n("Window to Desktop 1"), "Window to Desktop 1", KKey(), KKey("Meta+Alt+F1"));
 keys->insertItem(i18n("Window to Desktop 2"), "Window to Desktop 2", KKey(), KKey("Meta+Alt+F2"));
 keys->insertItem(i18n("Window to Desktop 3"), "Window to Desktop 3", KKey(), KKey("Meta+Alt+F3"));
 keys->insertItem(i18n("Window to Desktop 4"), "Window to Desktop 4", KKey(), KKey("Meta+Alt+F4"));
 keys->insertItem(i18n("Window to Desktop 5"), "Window to Desktop 5", KKey(), KKey("Meta+Alt+F5"));
 keys->insertItem(i18n("Window to Desktop 6"), "Window to Desktop 6", KKey(), KKey("Meta+Alt+F6"));
 keys->insertItem(i18n("Window to Desktop 7"), "Window to Desktop 7", KKey(), KKey("Meta+Alt+F7"));
 keys->insertItem(i18n("Window to Desktop 8"), "Window to Desktop 8", KKey(), KKey("Meta+Alt+F8"));
 keys->insertItem(i18n("Window to Desktop 9"), "Window to Desktop 9", KKey(), KKey("Meta+Alt+F9"));
 keys->insertItem(i18n("Window to Desktop 10"), "Window to Desktop 10", KKey(), KKey("Meta+Alt+F10"));
 keys->insertItem(i18n("Window to Desktop 11"), "Window to Desktop 11", KKey(), KKey());
 keys->insertItem(i18n("Window to Desktop 12"), "Window to Desktop 12", KKey(), KKey());
 keys->insertItem(i18n("Window to Desktop 13"), "Window to Desktop 13", KKey(), KKey());
 keys->insertItem(i18n("Window to Desktop 14"), "Window to Desktop 14", KKey(), KKey());
 keys->insertItem(i18n("Window to Desktop 15"), "Window to Desktop 15", KKey(), KKey());
 keys->insertItem(i18n("Window to Desktop 16"), "Window to Desktop 16", KKey(), KKey());
 keys->insertItem(i18n("Window to Next Desktop"), "Window to next desktop", 0);
 keys->insertItem(i18n("Window to Previous Desktop"), "Window to previous desktop", 0);

/*
This belongs in taskbar rather than here, so it'll have to wait until after 2.2 is done.
  -- ellis
#ifdef WITH_LABELS
 keys->insertItem(i18n("Window & Taskbar"), "Group:Window Desktop", 0);
#endif
 keys->insertItem(i18n("Window to Taskbar Position 1"), "Window to Taskbar Position 1", KKey(), KKey("Meta+Alt+1"));
 keys->insertItem(i18n("Window to Taskbar Position 2"), "Window to Taskbar Position 2", KKey(), KKey("Meta+Alt+2"));
 keys->insertItem(i18n("Window to Taskbar Position 3"), "Window to Taskbar Position 3", KKey(), KKey("Meta+Alt+3"));
 keys->insertItem(i18n("Window to Taskbar Position 4"), "Window to Taskbar Position 4", KKey(), KKey("Meta+Alt+4"));
 keys->insertItem(i18n("Window to Taskbar Position 5"), "Window to Taskbar Position 5", KKey(), KKey("Meta+Alt+5"));
 keys->insertItem(i18n("Window to Taskbar Position 6"), "Window to Taskbar Position 6", KKey(), KKey("Meta+Alt+6"));
 keys->insertItem(i18n("Window to Taskbar Position 7"), "Window to Taskbar Position 7", KKey(), KKey("Meta+Alt+7"));
 keys->insertItem(i18n("Window to Taskbar Position 8"), "Window to Taskbar Position 8", KKey(), KKey("Meta+Alt+8"));
 keys->insertItem(i18n("Window to Taskbar Position 9"), "Window to Taskbar Position 9", KKey(), KKey("Meta+Alt+9"));
*/

#ifdef WITH_LABELS
 keys->insertItem(i18n("Desktop Switching"), "Group:Desktop Switching", 0);
#endif
 keys->insertItem(i18n("Switch to Desktop 1"), "Switch to desktop 1", KKey("CTRL+F1"), KKey("Meta+F1"));
 keys->insertItem(i18n("Switch to Desktop 2"), "Switch to desktop 2", KKey("CTRL+F2"), KKey("Meta+F2"));
 keys->insertItem(i18n("Switch to Desktop 3"), "Switch to desktop 3", KKey("CTRL+F3"), KKey("Meta+F3"));
 keys->insertItem(i18n("Switch to Desktop 4"), "Switch to desktop 4", KKey("CTRL+F4"), KKey("Meta+F4"));
 keys->insertItem(i18n("Switch to Desktop 5"), "Switch to desktop 5", KKey("CTRL+F5"), KKey("Meta+F5"));
 keys->insertItem(i18n("Switch to Desktop 6"), "Switch to desktop 6", KKey("CTRL+F6"), KKey("Meta+F6"));
 keys->insertItem(i18n("Switch to Desktop 7"), "Switch to desktop 7", KKey("CTRL+F7"), KKey("Meta+F7"));
 keys->insertItem(i18n("Switch to Desktop 8"), "Switch to desktop 8", KKey("CTRL+F8"), KKey("Meta+F8"));
 keys->insertItem(i18n("Switch to Desktop 9"), "Switch to desktop 9", KKey("CTRL+F9"), KKey("Meta+F9"));
 keys->insertItem(i18n("Switch to Desktop 10"), "Switch to desktop 10", KKey("CTRL+F10"), KKey("Meta+F10"));
 keys->insertItem(i18n("Switch to Desktop 11"), "Switch to desktop 11", KKey("CTRL+F11"), KKey());
 keys->insertItem(i18n("Switch to Desktop 12"), "Switch to desktop 12", KKey("CTRL+F12"), KKey());
 keys->insertItem(i18n("Switch to Desktop 13"), "Switch to desktop 13", KKey("CTRL+SHIFT+F1"), KKey());
 keys->insertItem(i18n("Switch to Desktop 14"), "Switch to desktop 14", KKey("CTRL+SHIFT+F2"), KKey());
 keys->insertItem(i18n("Switch to Desktop 15"), "Switch to desktop 15", KKey("CTRL+SHIFT+F3"), KKey());
 keys->insertItem(i18n("Switch to Desktop 16"), "Switch to desktop 16", KKey("CTRL+SHIFT+F4"), KKey());
 keys->insertItem(i18n("Switch to Next Desktop"), "Switch desktop next", KKey().key());
 keys->insertItem(i18n("Switch to Previous Desktop"), "Switch desktop previous", KKey().key());
 keys->insertItem(i18n("Switch One Desktop to the Right"), "Switch desktop right", KKey().key());
 keys->insertItem(i18n("Switch One Desktop to the Left"), "Switch desktop left", KKey().key());
 keys->insertItem(i18n("Switch One Desktop Up"), "Switch desktop up", KKey().key());
 keys->insertItem(i18n("Switch One Desktop Down"), "Switch desktop down", KKey().key());

#ifdef WITH_LABELS
 keys->insertItem(i18n("Miscellaneous"), "Group:Miscellaneous", 0);
#endif
 keys->insertItem(i18n("Mouse Emulation"), "Mouse emulation", KKey("ALT+F12"), KKey());
 keys->insertItem(i18n("Kill Window"), "Kill Window", KKey("CTRL+ALT+Escape"), KKey("Meta+Ctrl+Delete"));
 keys->insertItem(i18n("Window Screenshot"), "Screenshot of active window", KKey("Alt+Print"), KKey("Alt+Print"));
 keys->insertItem(i18n("Desktop Screenshot"), "Screenshot of desktop", KKey("CTRL+Print"), KKey("Meta+Print"));

