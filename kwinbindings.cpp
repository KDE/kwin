/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// This file is #included from within:
// Workspace::initShortcuts()
//  {

// Some shortcuts have Tarzan-speech like names, they need extra
// normal human descriptions with DEF2() the others can use DEF()
// new DEF3 allows to pass data to the action, replacing the %1 argument in the name

#define DEF2( name, descr, key, fnSlot )                            \
    initShortcut(QStringLiteral(name), i18n(descr), key, &Workspace::fnSlot);

#define DEF( name, key, fnSlot )                                    \
    initShortcut(QStringLiteral(name), i18n(name), key, &Workspace::fnSlot);

#define DEF3( name, key, fnSlot, value )                            \
    initShortcut(QStringLiteral(name).arg(value), i18n(name, value), key, &Workspace::fnSlot, value);

#define DEF4( name, descr, key, functor ) \
    initShortcut(QStringLiteral(name), i18n(descr), key, functor);

#define DEF5( name, key, functor, value )                            \
    initShortcut(QStringLiteral(name).arg(value), i18n(name, value), key, functor, value);

#define DEF6( name, key, target, fnSlot )                                    \
    initShortcut(QStringLiteral(name), i18n(name), key, target, &fnSlot);


DEF(I18N_NOOP("Window Operations Menu"),
    Qt::ALT + Qt::Key_F3, slotWindowOperations);
DEF2("Window Close", I18N_NOOP("Close Window"),
     Qt::ALT + Qt::Key_F4, slotWindowClose);
DEF2("Window Maximize", I18N_NOOP("Maximize Window"),
     Qt::META + Qt::Key_PageUp, slotWindowMaximize);
DEF2("Window Maximize Vertical", I18N_NOOP("Maximize Window Vertically"),
     0, slotWindowMaximizeVertical);
DEF2("Window Maximize Horizontal", I18N_NOOP("Maximize Window Horizontally"),
     0, slotWindowMaximizeHorizontal);
DEF2("Window Minimize", I18N_NOOP("Minimize Window"),
     Qt::META + Qt::Key_PageDown, slotWindowMinimize);
DEF2("Window Shade", I18N_NOOP("Shade Window"),
     0, slotWindowShade);
DEF2("Window Move", I18N_NOOP("Move Window"),
     0, slotWindowMove);
DEF2("Window Resize", I18N_NOOP("Resize Window"),
     0, slotWindowResize);
DEF2("Window Raise", I18N_NOOP("Raise Window"),
     0, slotWindowRaise);
DEF2("Window Lower", I18N_NOOP("Lower Window"),
     0, slotWindowLower);
DEF(I18N_NOOP("Toggle Window Raise/Lower"),
    0, slotWindowRaiseOrLower);
DEF2("Window Fullscreen", I18N_NOOP("Make Window Fullscreen"),
     0, slotWindowFullScreen);
DEF2("Window No Border", I18N_NOOP("Hide Window Border"),
     0, slotWindowNoBorder);
DEF2("Window Above Other Windows", I18N_NOOP("Keep Window Above Others"),
     0, slotWindowAbove);
DEF2("Window Below Other Windows", I18N_NOOP("Keep Window Below Others"),
     0, slotWindowBelow);
DEF(I18N_NOOP("Activate Window Demanding Attention"),
    Qt::CTRL + Qt::ALT + Qt::Key_A, slotActivateAttentionWindow);
DEF(I18N_NOOP("Setup Window Shortcut"),
    0, slotSetupWindowShortcut);
DEF2("Window Pack Right", I18N_NOOP("Pack Window to the Right"),
     0, slotWindowPackRight);
DEF2("Window Pack Left", I18N_NOOP("Pack Window to the Left"),
     0, slotWindowPackLeft);
DEF2("Window Pack Up", I18N_NOOP("Pack Window Up"),
     0, slotWindowPackUp);
DEF2("Window Pack Down", I18N_NOOP("Pack Window Down"),
     0, slotWindowPackDown);
DEF2("Window Grow Horizontal", I18N_NOOP("Pack Grow Window Horizontally"),
     0, slotWindowGrowHorizontal);
DEF2("Window Grow Vertical", I18N_NOOP("Pack Grow Window Vertically"),
     0, slotWindowGrowVertical);
DEF2("Window Shrink Horizontal", I18N_NOOP("Pack Shrink Window Horizontally"),
     0, slotWindowShrinkHorizontal);
DEF2("Window Shrink Vertical", I18N_NOOP("Pack Shrink Window Vertically"),
     0, slotWindowShrinkVertical);
DEF4("Window Quick Tile Left", I18N_NOOP("Quick Tile Window to the Left"),
     Qt::META + Qt::Key_Left, std::bind(&Workspace::quickTileWindow, this, QuickTileFlag::Left));
DEF4("Window Quick Tile Right", I18N_NOOP("Quick Tile Window to the Right"),
     Qt::META + Qt::Key_Right, std::bind(&Workspace::quickTileWindow, this, QuickTileFlag::Right));
DEF4("Window Quick Tile Top", I18N_NOOP("Quick Tile Window to the Top"),
     Qt::META + Qt::Key_Up, std::bind(&Workspace::quickTileWindow, this, QuickTileFlag::Top));
DEF4("Window Quick Tile Bottom", I18N_NOOP("Quick Tile Window to the Bottom"),
     Qt::META + Qt::Key_Down, std::bind(&Workspace::quickTileWindow, this, QuickTileFlag::Bottom));
DEF4("Window Quick Tile Top Left", I18N_NOOP("Quick Tile Window to the Top Left"),
     0, std::bind(&Workspace::quickTileWindow, this, QuickTileFlag::Top | QuickTileFlag::Left));
DEF4("Window Quick Tile Bottom Left", I18N_NOOP("Quick Tile Window to the Bottom Left"),
     0, std::bind(&Workspace::quickTileWindow, this, QuickTileFlag::Bottom | QuickTileFlag::Left));
DEF4("Window Quick Tile Top Right", I18N_NOOP("Quick Tile Window to the Top Right"),
     0, std::bind(&Workspace::quickTileWindow, this, QuickTileFlag::Top | QuickTileFlag::Right));
DEF4("Window Quick Tile Bottom Right", I18N_NOOP("Quick Tile Window to the Bottom Right"),
     0, std::bind(&Workspace::quickTileWindow, this, QuickTileFlag::Bottom | QuickTileFlag::Right));
DEF4("Switch Window Up", I18N_NOOP("Switch to Window Above"),
     Qt::META + Qt::ALT + Qt::Key_Up, std::bind(static_cast<void (Workspace::*)(Direction)>(&Workspace::switchWindow), this, DirectionNorth));
DEF4("Switch Window Down", I18N_NOOP("Switch to Window Below"),
     Qt::META + Qt::ALT + Qt::Key_Down, std::bind(static_cast<void (Workspace::*)(Direction)>(&Workspace::switchWindow), this, DirectionSouth));
DEF4("Switch Window Right", I18N_NOOP("Switch to Window to the Right"),
     Qt::META + Qt::ALT + Qt::Key_Right, std::bind(static_cast<void (Workspace::*)(Direction)>(&Workspace::switchWindow), this, DirectionEast));
DEF4("Switch Window Left", I18N_NOOP("Switch to Window to the Left"),
     Qt::META + Qt::ALT + Qt::Key_Left, std::bind(static_cast<void (Workspace::*)(Direction)>(&Workspace::switchWindow), this, DirectionWest));
DEF2("Increase Opacity", I18N_NOOP("Increase Opacity of Active Window by 5 %"),
    0, slotIncreaseWindowOpacity);
DEF2("Decrease Opacity", I18N_NOOP("Decrease Opacity of Active Window by 5 %"),
    0, slotLowerWindowOpacity);

DEF2("Window On All Desktops", I18N_NOOP("Keep Window on All Desktops"),
     0, slotWindowOnAllDesktops);

for (int i = 1; i < 21; ++i) {
    DEF5(I18N_NOOP("Window to Desktop %1"),        0, std::bind(&Workspace::slotWindowToDesktop, this, i), i);
}
DEF(I18N_NOOP("Window to Next Desktop"),           0, slotWindowToNextDesktop);
DEF(I18N_NOOP("Window to Previous Desktop"),       0, slotWindowToPreviousDesktop);
DEF(I18N_NOOP("Window One Desktop to the Right"),  0, slotWindowToDesktopRight);
DEF(I18N_NOOP("Window One Desktop to the Left"),   0, slotWindowToDesktopLeft);
DEF(I18N_NOOP("Window One Desktop Up"),            0, slotWindowToDesktopUp);
DEF(I18N_NOOP("Window One Desktop Down"),          0, slotWindowToDesktopDown);

for (int i = 0; i < 8; ++i) {
    DEF3(I18N_NOOP("Window to Screen %1"),         0, slotWindowToScreen, i);
}
DEF(I18N_NOOP("Window to Next Screen"),            0, slotWindowToNextScreen);
DEF(I18N_NOOP("Window to Previous Screen"),        0, slotWindowToPrevScreen);
DEF(I18N_NOOP("Show Desktop"),                     Qt::META + Qt::Key_D, slotToggleShowDesktop);

for (int i = 0; i < 8; ++i) {
    DEF3(I18N_NOOP("Switch to Screen %1"),         0, slotSwitchToScreen, i);
}

DEF(I18N_NOOP("Switch to Next Screen"),            0, slotSwitchToNextScreen);
DEF(I18N_NOOP("Switch to Previous Screen"),        0, slotSwitchToPrevScreen);

DEF(I18N_NOOP("Kill Window"),                      Qt::CTRL + Qt::ALT + Qt::Key_Escape, slotKillWindow);
DEF6(I18N_NOOP("Suspend Compositing"),             Qt::SHIFT + Qt::ALT + Qt::Key_F12, Compositor::self(), Compositor::toggleCompositing);
DEF6(I18N_NOOP("Invert Screen Colors"),            0, kwinApp()->platform(), Platform::invertScreen);

#undef DEF
#undef DEF2
#undef DEF3
#undef DEF4
#undef DEF5
#undef DEF6

//  }
