/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

// This file is #included from within:
// Workspace::initShortcuts()
//  {

// Some shortcuts have Tarzan-speech like names, they need extra
// normal human descriptions with DEF2() the others can use DEF()
// new DEF3 allows to pass data to the action, replacing the %1 argument in the name

#ifndef NOSLOTS
#define KWIN_CONNECT(_FNSLOT_) connect(a,SIGNAL(triggered(bool)),SLOT(_FNSLOT_));
#else
#define KWIN_CONNECT(_FNSLOT_) /*noop*/
#endif

#define DEF2( name, descr, key, fnSlot )                            \
    a = actionCollection->addAction( name );                        \
    a->setText( i18n(descr) );                                      \
    qobject_cast<KAction*>( a )->setGlobalShortcut(KShortcut(key)); \
    KWIN_CONNECT(fnSlot)

#define DEF4( name, descr, key, fnSlot, value )                     \
    DEF2(name, descr, key, fnSlot)                                  \
    a->setData(value);

#define DEF( name, key, fnSlot )                                    \
    DEF2(name, name, key, fnSlot)

#define DEF3( name, key, fnSlot, value )                            \
    a = actionCollection->addAction( QString(name).arg(value) );    \
    a->setText( i18n(name, value) );                                \
    qobject_cast<KAction*>( a )->setGlobalShortcut(KShortcut(key)); \
    a->setData(value);                                              \
    KWIN_CONNECT(fnSlot)


a = actionCollection->addAction("Program:kwin");
a->setText(i18n("System"));

a = actionCollection->addAction("Group:Navigation");
a->setText(i18n("Navigation"));
DEF(I18N_NOOP("Walk Through Window Tabs"),             0, slotSwitchToTabRight());
DEF(I18N_NOOP("Walk Through Window Tabs (Reverse)"),   0, slotSwitchToTabLeft());
DEF(I18N_NOOP("Remove Window From Group"),             0, slotRemoveFromGroup());

a = actionCollection->addAction("Group:Windows");
a->setText(i18n("Windows"));
DEF(I18N_NOOP("Window Operations Menu"),
    Qt::ALT + Qt::Key_F3, slotWindowOperations());
DEF2("Window Close", I18N_NOOP("Close Window"),
     Qt::ALT + Qt::Key_F4, slotWindowClose());
DEF2("Window Maximize", I18N_NOOP("Maximize Window"),
     0, slotWindowMaximize());
DEF2("Window Maximize Vertical", I18N_NOOP("Maximize Window Vertically"),
     0, slotWindowMaximizeVertical());
DEF2("Window Maximize Horizontal", I18N_NOOP("Maximize Window Horizontally"),
     0, slotWindowMaximizeHorizontal());
DEF2("Window Minimize", I18N_NOOP("Minimize Window"),
     0, slotWindowMinimize());
DEF2("Window Shade", I18N_NOOP("Shade Window"),
     0, slotWindowShade());
DEF2("Window Move", I18N_NOOP("Move Window"),
     0, slotWindowMove());
DEF2("Window Resize", I18N_NOOP("Resize Window"),
     0, slotWindowResize());
DEF2("Window Raise", I18N_NOOP("Raise Window"),
     0, slotWindowRaise());
DEF2("Window Lower", I18N_NOOP("Lower Window"),
     0, slotWindowLower());
DEF(I18N_NOOP("Toggle Window Raise/Lower"),
    0, slotWindowRaiseOrLower());
DEF2("Window Fullscreen", I18N_NOOP("Make Window Fullscreen"),
     0, slotWindowFullScreen());
DEF2("Window No Border", I18N_NOOP("Hide Window Border"),
     0, slotWindowNoBorder());
DEF2("Window Above Other Windows", I18N_NOOP("Keep Window Above Others"),
     0, slotWindowAbove());
DEF2("Window Below Other Windows", I18N_NOOP("Keep Window Below Others"),
     0, slotWindowBelow());
DEF(I18N_NOOP("Activate Window Demanding Attention"),
    Qt::CTRL + Qt::ALT + Qt::Key_A, slotActivateAttentionWindow());
DEF(I18N_NOOP("Setup Window Shortcut"),
    0, slotSetupWindowShortcut());
DEF2("Window Pack Right", I18N_NOOP("Pack Window to the Right"),
     0, slotWindowPackRight());
DEF2("Window Pack Left", I18N_NOOP("Pack Window to the Left"),
     0, slotWindowPackLeft());
DEF2("Window Pack Up", I18N_NOOP("Pack Window Up"),
     0, slotWindowPackUp());
DEF2("Window Pack Down", I18N_NOOP("Pack Window Down"),
     0, slotWindowPackDown());
DEF2("Window Grow Horizontal", I18N_NOOP("Pack Grow Window Horizontally"),
     0, slotWindowGrowHorizontal());
DEF2("Window Grow Vertical", I18N_NOOP("Pack Grow Window Vertically"),
     0, slotWindowGrowVertical());
DEF2("Window Shrink Horizontal", I18N_NOOP("Pack Shrink Window Horizontally"),
     0, slotWindowShrinkHorizontal());
DEF2("Window Shrink Vertical", I18N_NOOP("Pack Shrink Window Vertically"),
     0, slotWindowShrinkVertical());
DEF2("Window Quick Tile Left", I18N_NOOP("Quick Tile Window to the Left"),
     0, slotWindowQuickTileLeft());
DEF2("Window Quick Tile Right", I18N_NOOP("Quick Tile Window to the Right"),
     0, slotWindowQuickTileRight());
DEF2("Window Quick Tile Top Left", I18N_NOOP("Quick Tile Window to the Top Left"),
     0, slotWindowQuickTileTopLeft());
DEF2("Window Quick Tile Bottom Left", I18N_NOOP("Quick Tile Window to the Bottom Left"),
     0, slotWindowQuickTileBottomLeft());
DEF2("Window Quick Tile Top Right", I18N_NOOP("Quick Tile Window to the Top Right"),
     0, slotWindowQuickTileTopRight());
DEF2("Window Quick Tile Bottom Right", I18N_NOOP("Quick Tile Window to the Bottom Right"),
     0, slotWindowQuickTileBottomRight());
DEF2("Switch Window Up", I18N_NOOP("Switch to Window Above"),
     Qt::META + Qt::ALT + Qt::Key_Up, slotSwitchWindowUp());
DEF2("Switch Window Down", I18N_NOOP("Switch to Window Below"),
     Qt::META + Qt::ALT + Qt::Key_Down, slotSwitchWindowDown());
DEF2("Switch Window Right", I18N_NOOP("Switch to Window to the Right"),
     Qt::META + Qt::ALT + Qt::Key_Right, slotSwitchWindowRight());
DEF2("Switch Window Left", I18N_NOOP("Switch to Window to the Left"),
     Qt::META + Qt::ALT + Qt::Key_Left, slotSwitchWindowLeft());

a = actionCollection->addAction("Group:Window Desktop");
a->setText(i18n("Window & Desktop"));
DEF2("Window On All Desktops", I18N_NOOP("Keep Window on All Desktops"),
     0, slotWindowOnAllDesktops());

for (int i = 1; i < 21; ++i) {
    DEF3(I18N_NOOP("Window to Desktop %1"),        0, slotWindowToDesktop(), i);
}
DEF(I18N_NOOP("Window to Next Desktop"),           0, slotWindowToNextDesktop());
DEF(I18N_NOOP("Window to Previous Desktop"),       0, slotWindowToPreviousDesktop());
DEF(I18N_NOOP("Window One Desktop to the Right"),  0, slotWindowToDesktopRight());
DEF(I18N_NOOP("Window One Desktop to the Left"),   0, slotWindowToDesktopLeft());
DEF(I18N_NOOP("Window One Desktop Up"),            0, slotWindowToDesktopUp());
DEF(I18N_NOOP("Window One Desktop Down"),          0, slotWindowToDesktopDown());

for (int i = 0; i < 8; ++i) {
    DEF3(I18N_NOOP("Window to Screen %1"),         0, slotWindowToScreen(), i);
}
DEF(I18N_NOOP("Window to Next Screen"),            0, slotWindowToNextScreen());
DEF(I18N_NOOP("Show Desktop"),                     0, slotToggleShowDesktop());

a = actionCollection->addAction("Group:Desktop Switching");
a->setText(i18n("Desktop Switching"));
DEF3("Switch to Desktop %1", Qt::CTRL + Qt::Key_F1,   slotSwitchToDesktop(), 1);
DEF3("Switch to Desktop %1", Qt::CTRL + Qt::Key_F2,   slotSwitchToDesktop(), 2);
DEF3("Switch to Desktop %1", Qt::CTRL + Qt::Key_F3,   slotSwitchToDesktop(), 3);
DEF3("Switch to Desktop %1", Qt::CTRL + Qt::Key_F4,   slotSwitchToDesktop(), 4);
for (int i = 5; i < 21; ++i) {
    DEF3(I18N_NOOP("Switch to Desktop %1"),        0, slotSwitchToDesktop(), i);
}

DEF(I18N_NOOP("Switch to Next Desktop"),           0, slotSwitchDesktopNext());
DEF(I18N_NOOP("Switch to Previous Desktop"),       0, slotSwitchDesktopPrevious());
DEF(I18N_NOOP("Switch One Desktop to the Right"),  0, slotSwitchDesktopRight());
DEF(I18N_NOOP("Switch One Desktop to the Left"),   0, slotSwitchDesktopLeft());
DEF(I18N_NOOP("Switch One Desktop Up"),            0, slotSwitchDesktopUp());
DEF(I18N_NOOP("Switch One Desktop Down"),          0, slotSwitchDesktopDown());

for (int i = 0; i < 8; ++i) {
    DEF3(I18N_NOOP("Switch to Screen %1"),         0, slotSwitchToScreen(), i);
}

DEF(I18N_NOOP("Switch to Next Screen"),            0, slotSwitchToNextScreen());

a = actionCollection->addAction("Group:Miscellaneous");
a->setText(i18n("Miscellaneous"));
DEF(I18N_NOOP("Kill Window"),                      Qt::CTRL + Qt::ALT + Qt::Key_Escape, slotKillWindow());
DEF(I18N_NOOP("Block Global Shortcuts"),           0, slotDisableGlobalShortcuts());
DEF(I18N_NOOP("Suspend Compositing"),              Qt::SHIFT + Qt::ALT + Qt::Key_F12, slotToggleCompositing());

#undef DEF
#undef DEF2

//  }
