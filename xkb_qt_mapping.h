/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_XKB_QT_MAPPING_H
#define KWIN_XKB_QT_MAPPING_H

#include <QtGlobal>

#include <map>
#include <xkbcommon/xkbcommon-keysyms.h>

namespace KWin
{

// based on mapping in kwindowsystem/src/platforms/xcb/kkeyserver.cpp
// adjusted to XKB
static const std::map<xkb_keysym_t, Qt::Key> s_mapping{
    { XKB_KEY_Escape, Qt::Key_Escape },
    { XKB_KEY_Tab, Qt::Key_Tab },
    { XKB_KEY_ISO_Left_Tab, Qt::Key_Backtab },
    { XKB_KEY_BackSpace, Qt::Key_Backspace },
    { XKB_KEY_Return, Qt::Key_Return },
    { XKB_KEY_Insert, Qt::Key_Insert },
    { XKB_KEY_Delete, Qt::Key_Delete },
    { XKB_KEY_Pause, Qt::Key_Pause },
    { XKB_KEY_Print, Qt::Key_Print },
    { XKB_KEY_Sys_Req, Qt::Key_SysReq },
    { XKB_KEY_Home, Qt::Key_Home },
    { XKB_KEY_End, Qt::Key_End },
    { XKB_KEY_Left, Qt::Key_Left },
    { XKB_KEY_Up, Qt::Key_Up },
    { XKB_KEY_Right, Qt::Key_Right },
    { XKB_KEY_Down, Qt::Key_Down },
    { XKB_KEY_Prior, Qt::Key_PageUp },
    { XKB_KEY_Next, Qt::Key_PageDown },
    { XKB_KEY_Caps_Lock, Qt::Key_CapsLock },
    { XKB_KEY_Num_Lock, Qt::Key_NumLock },
    { XKB_KEY_Scroll_Lock, Qt::Key_ScrollLock },
    { XKB_KEY_F1, Qt::Key_F1 },
    { XKB_KEY_F2, Qt::Key_F2 },
    { XKB_KEY_F3, Qt::Key_F3 },
    { XKB_KEY_F4, Qt::Key_F4 },
    { XKB_KEY_F5, Qt::Key_F5 },
    { XKB_KEY_F6, Qt::Key_F6 },
    { XKB_KEY_F7, Qt::Key_F7 },
    { XKB_KEY_F8, Qt::Key_F8 },
    { XKB_KEY_F9, Qt::Key_F9 },
    { XKB_KEY_F10, Qt::Key_F10 },
    { XKB_KEY_F11, Qt::Key_F11 },
    { XKB_KEY_F12, Qt::Key_F12 },
    { XKB_KEY_F13, Qt::Key_F13 },
    { XKB_KEY_F14, Qt::Key_F14 },
    { XKB_KEY_F15, Qt::Key_F15 },
    { XKB_KEY_F16, Qt::Key_F16 },
    { XKB_KEY_F17, Qt::Key_F17 },
    { XKB_KEY_F18, Qt::Key_F18 },
    { XKB_KEY_F19, Qt::Key_F19 },
    { XKB_KEY_F20, Qt::Key_F20 },
    { XKB_KEY_F21, Qt::Key_F21 },
    { XKB_KEY_F22, Qt::Key_F22 },
    { XKB_KEY_F23, Qt::Key_F23 },
    { XKB_KEY_F24, Qt::Key_F24 },
    { XKB_KEY_F25, Qt::Key_F25 },
    { XKB_KEY_F26, Qt::Key_F26 },
    { XKB_KEY_F27, Qt::Key_F27 },
    { XKB_KEY_F28, Qt::Key_F28 },
    { XKB_KEY_F29, Qt::Key_F29 },
    { XKB_KEY_F30, Qt::Key_F30 },
    { XKB_KEY_F31, Qt::Key_F31 },
    { XKB_KEY_F32, Qt::Key_F32 },
    { XKB_KEY_F33, Qt::Key_F33 },
    { XKB_KEY_F34, Qt::Key_F34 },
    { XKB_KEY_F35, Qt::Key_F35 },
    { XKB_KEY_Super_L, Qt::Key_Super_L },
    { XKB_KEY_Super_R, Qt::Key_Super_R },
    { XKB_KEY_Menu, Qt::Key_Menu },
    { XKB_KEY_Hyper_L, Qt::Key_Hyper_L },
    { XKB_KEY_Hyper_R, Qt::Key_Hyper_R },
    { XKB_KEY_Help, Qt::Key_Help },
    { XKB_KEY_KP_Space, Qt::Key_Space },
    { XKB_KEY_KP_Tab, Qt::Key_Tab },
    { XKB_KEY_KP_Enter, Qt::Key_Enter },
    { XKB_KEY_KP_Home, Qt::Key_Home },
    { XKB_KEY_KP_Left, Qt::Key_Left },
    { XKB_KEY_KP_Up, Qt::Key_Up },
    { XKB_KEY_KP_Right, Qt::Key_Right },
    { XKB_KEY_KP_Down, Qt::Key_Down },
    { XKB_KEY_KP_Prior, Qt::Key_PageUp },
    { XKB_KEY_KP_Next, Qt::Key_PageDown },
    { XKB_KEY_KP_End, Qt::Key_End },
    { XKB_KEY_KP_Begin, Qt::Key_Clear },
    { XKB_KEY_KP_Insert, Qt::Key_Insert },
    { XKB_KEY_KP_Delete, Qt::Key_Delete },
    { XKB_KEY_KP_Equal, Qt::Key_Equal },
    { XKB_KEY_KP_Multiply, Qt::Key_Asterisk },
    { XKB_KEY_KP_Add, Qt::Key_Plus },
    { XKB_KEY_KP_Separator, Qt::Key_Comma },
    { XKB_KEY_KP_Subtract, Qt::Key_Minus },
    { XKB_KEY_KP_Decimal, Qt::Key_Period },
    { XKB_KEY_KP_Divide, Qt::Key_Slash },
    { XKB_KEY_XF86Back, Qt::Key_Back },
    { XKB_KEY_XF86Forward, Qt::Key_Forward },
    { XKB_KEY_XF86Stop, Qt::Key_Stop },
    { XKB_KEY_XF86Refresh, Qt::Key_Refresh },
    { XKB_KEY_XF86Favorites, Qt::Key_Favorites },
    { XKB_KEY_XF86AudioMedia, Qt::Key_LaunchMedia },
    { XKB_KEY_XF86OpenURL, Qt::Key_OpenUrl },
    { XKB_KEY_XF86HomePage, Qt::Key_HomePage },
    { XKB_KEY_XF86Search, Qt::Key_Search },
    { XKB_KEY_XF86AudioLowerVolume, Qt::Key_VolumeDown },
    { XKB_KEY_XF86AudioMute, Qt::Key_VolumeMute },
    { XKB_KEY_XF86AudioRaiseVolume, Qt::Key_VolumeUp },
    { XKB_KEY_XF86AudioPlay, Qt::Key_MediaPlay },
    { XKB_KEY_XF86AudioStop, Qt::Key_MediaStop },
    { XKB_KEY_XF86AudioPrev, Qt::Key_MediaPrevious },
    { XKB_KEY_XF86AudioNext, Qt::Key_MediaNext },
    { XKB_KEY_XF86AudioRecord, Qt::Key_MediaRecord },
    { XKB_KEY_XF86Mail, Qt::Key_LaunchMail },
    { XKB_KEY_XF86MyComputer, Qt::Key_Launch0 },
    { XKB_KEY_XF86Calculator, Qt::Key_Launch1 },
    { XKB_KEY_XF86Memo, Qt::Key_Memo },
    { XKB_KEY_XF86ToDoList, Qt::Key_ToDoList },
    { XKB_KEY_XF86Calendar, Qt::Key_Calendar },
    { XKB_KEY_XF86PowerDown, Qt::Key_PowerDown },
    { XKB_KEY_XF86ContrastAdjust, Qt::Key_ContrastAdjust },
    { XKB_KEY_XF86Standby, Qt::Key_Standby },
    { XKB_KEY_XF86MonBrightnessUp, Qt::Key_MonBrightnessUp },
    { XKB_KEY_XF86MonBrightnessDown, Qt::Key_MonBrightnessDown },
    { XKB_KEY_XF86KbdLightOnOff, Qt::Key_KeyboardLightOnOff },
    { XKB_KEY_XF86KbdBrightnessUp, Qt::Key_KeyboardBrightnessUp },
    { XKB_KEY_XF86KbdBrightnessDown, Qt::Key_KeyboardBrightnessDown },
    { XKB_KEY_XF86PowerOff, Qt::Key_PowerOff },
    { XKB_KEY_XF86WakeUp, Qt::Key_WakeUp },
    { XKB_KEY_XF86Eject, Qt::Key_Eject },
    { XKB_KEY_XF86ScreenSaver, Qt::Key_ScreenSaver },
    { XKB_KEY_XF86WWW, Qt::Key_WWW },
    { XKB_KEY_XF86Sleep, Qt::Key_Sleep },
    { XKB_KEY_XF86LightBulb, Qt::Key_LightBulb },
    { XKB_KEY_XF86Shop, Qt::Key_Shop },
    { XKB_KEY_XF86History, Qt::Key_History },
    { XKB_KEY_XF86AddFavorite, Qt::Key_AddFavorite },
    { XKB_KEY_XF86HotLinks, Qt::Key_HotLinks },
    { XKB_KEY_XF86BrightnessAdjust, Qt::Key_BrightnessAdjust },
    { XKB_KEY_XF86Finance, Qt::Key_Finance },
    { XKB_KEY_XF86Community, Qt::Key_Community },
    { XKB_KEY_XF86AudioRewind, Qt::Key_AudioRewind },
    { XKB_KEY_XF86BackForward, Qt::Key_BackForward },
    { XKB_KEY_XF86ApplicationLeft, Qt::Key_ApplicationLeft },
    { XKB_KEY_XF86ApplicationRight, Qt::Key_ApplicationRight },
    { XKB_KEY_XF86Book, Qt::Key_Book },
    { XKB_KEY_XF86CD, Qt::Key_CD },
    { XKB_KEY_XF86Calculater, Qt::Key_Calculator },
    { XKB_KEY_XF86Clear, Qt::Key_Clear },
    { XKB_KEY_XF86ClearGrab, Qt::Key_ClearGrab },
    { XKB_KEY_XF86Close, Qt::Key_Close },
    { XKB_KEY_XF86Copy, Qt::Key_Copy },
    { XKB_KEY_XF86Cut, Qt::Key_Cut },
    { XKB_KEY_XF86Display, Qt::Key_Display },
    { XKB_KEY_XF86DOS, Qt::Key_DOS },
    { XKB_KEY_XF86Documents, Qt::Key_Documents },
    { XKB_KEY_XF86Excel, Qt::Key_Excel },
    { XKB_KEY_XF86Explorer, Qt::Key_Explorer },
    { XKB_KEY_XF86Game, Qt::Key_Game },
    { XKB_KEY_XF86Go, Qt::Key_Go },
    { XKB_KEY_XF86iTouch, Qt::Key_iTouch },
    { XKB_KEY_XF86LogOff, Qt::Key_LogOff },
    { XKB_KEY_XF86Market, Qt::Key_Market },
    { XKB_KEY_XF86Meeting, Qt::Key_Meeting },
    { XKB_KEY_XF86MenuKB, Qt::Key_MenuKB },
    { XKB_KEY_XF86MenuPB, Qt::Key_MenuPB },
    { XKB_KEY_XF86MySites, Qt::Key_MySites },
    { XKB_KEY_XF86News, Qt::Key_News },
    { XKB_KEY_XF86OfficeHome, Qt::Key_OfficeHome },
    { XKB_KEY_XF86Option, Qt::Key_Option },
    { XKB_KEY_XF86Paste, Qt::Key_Paste },
    { XKB_KEY_XF86Phone, Qt::Key_Phone },
    { XKB_KEY_XF86Reply, Qt::Key_Reply },
    { XKB_KEY_XF86Reload, Qt::Key_Reload },
    { XKB_KEY_XF86RotateWindows, Qt::Key_RotateWindows },
    { XKB_KEY_XF86RotationPB, Qt::Key_RotationPB },
    { XKB_KEY_XF86RotationKB, Qt::Key_RotationKB },
    { XKB_KEY_XF86Save, Qt::Key_Save },
    { XKB_KEY_XF86Send, Qt::Key_Send },
    { XKB_KEY_XF86Spell, Qt::Key_Spell },
    { XKB_KEY_XF86SplitScreen, Qt::Key_SplitScreen },
    { XKB_KEY_XF86Support, Qt::Key_Support },
    { XKB_KEY_XF86TaskPane, Qt::Key_TaskPane },
    { XKB_KEY_XF86Terminal, Qt::Key_Terminal },
    { XKB_KEY_XF86Tools, Qt::Key_Tools },
    { XKB_KEY_XF86Travel, Qt::Key_Travel },
    { XKB_KEY_XF86Video, Qt::Key_Video },
    { XKB_KEY_XF86Word, Qt::Key_Word },
    { XKB_KEY_XF86Xfer, Qt::Key_Xfer },
    { XKB_KEY_XF86ZoomIn, Qt::Key_ZoomIn },
    { XKB_KEY_XF86ZoomOut, Qt::Key_ZoomOut },
    { XKB_KEY_XF86Away, Qt::Key_Away },
    { XKB_KEY_XF86Messenger, Qt::Key_Messenger },
    { XKB_KEY_XF86WebCam, Qt::Key_WebCam },
    { XKB_KEY_XF86MailForward, Qt::Key_MailForward },
    { XKB_KEY_XF86Pictures, Qt::Key_Pictures },
    { XKB_KEY_XF86Music, Qt::Key_Music },
    { XKB_KEY_XF86Battery, Qt::Key_Battery },
    { XKB_KEY_XF86Bluetooth, Qt::Key_Bluetooth },
    { XKB_KEY_XF86WLAN, Qt::Key_WLAN },
    { XKB_KEY_XF86UWB, Qt::Key_UWB },
    { XKB_KEY_XF86AudioForward, Qt::Key_AudioForward },
    { XKB_KEY_XF86AudioRepeat, Qt::Key_AudioRepeat },
    { XKB_KEY_XF86AudioRandomPlay, Qt::Key_AudioRandomPlay },
    { XKB_KEY_XF86Subtitle, Qt::Key_Subtitle },
    { XKB_KEY_XF86AudioCycleTrack, Qt::Key_AudioCycleTrack },
    { XKB_KEY_XF86Time, Qt::Key_Time },
    { XKB_KEY_XF86Select, Qt::Key_Select },
    { XKB_KEY_XF86View, Qt::Key_View },
    { XKB_KEY_XF86TopMenu, Qt::Key_TopMenu },
    { XKB_KEY_XF86Bluetooth, Qt::Key_Bluetooth },
    { XKB_KEY_XF86Suspend, Qt::Key_Suspend },
    { XKB_KEY_XF86Hibernate, Qt::Key_Hibernate },
    { XKB_KEY_XF86TouchpadToggle, Qt::Key_TouchpadToggle },
    { XKB_KEY_XF86TouchpadOn, Qt::Key_TouchpadOn },
    { XKB_KEY_XF86TouchpadOff, Qt::Key_TouchpadOff },
    { XKB_KEY_XF86AudioMicMute, Qt::Key_MicMute },
    { XKB_KEY_XF86Launch0, Qt::Key_Launch2 },
    { XKB_KEY_XF86Launch1, Qt::Key_Launch3 },
    { XKB_KEY_XF86Launch2, Qt::Key_Launch4 },
    { XKB_KEY_XF86Launch3, Qt::Key_Launch5 },
    { XKB_KEY_XF86Launch4, Qt::Key_Launch6 },
    { XKB_KEY_XF86Launch5, Qt::Key_Launch7 },
    { XKB_KEY_XF86Launch6, Qt::Key_Launch8 },
    { XKB_KEY_XF86Launch7, Qt::Key_Launch9 },
    { XKB_KEY_XF86Launch8, Qt::Key_LaunchA },
    { XKB_KEY_XF86Launch9, Qt::Key_LaunchB },
    { XKB_KEY_XF86LaunchA, Qt::Key_LaunchC },
    { XKB_KEY_XF86LaunchB, Qt::Key_LaunchD },
    { XKB_KEY_XF86LaunchC, Qt::Key_LaunchE },
    { XKB_KEY_XF86LaunchD, Qt::Key_LaunchF }
};

static inline Qt::Key xkbToQtKey(xkb_keysym_t keySym)
{
    Qt::Key key = Qt::Key_unknown;
    if (keySym >= XKB_KEY_KP_0 && keySym <= XKB_KEY_KP_9) {
        // numeric keypad keys
        key = Qt::Key(int(Qt::Key_0) + int(keySym) - XKB_KEY_KP_0);
    } else if ((keySym >= XKB_KEY_a && keySym <= XKB_KEY_z) ||
        (keySym >= XKB_KEY_agrave && keySym < XKB_KEY_division) ||
        (keySym > XKB_KEY_division && keySym <= XKB_KEY_thorn)) {
        key = Qt::Key(QChar(keySym).toUpper().unicode());
    } else if (keySym < 0x3000) {
        key = Qt::Key(keySym);
    }
    if (key == Qt::Key_unknown) {
        const auto it = s_mapping.find(keySym);
        if (it != s_mapping.end()) {
            key = it->second;
        }
    }
    return key;
}

static inline bool isKeypadKey(xkb_keysym_t keySym)
{
    return keySym >= XKB_KEY_KP_Space && keySym <= XKB_KEY_KP_9;
}

static inline xkb_keysym_t qtKeyToXkb(Qt::Key qtKey, Qt::KeyboardModifiers modifiers)
{
    xkb_keysym_t sym = XKB_KEY_NoSymbol;
    if (modifiers.testFlag(Qt::KeypadModifier) && qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9) {
        sym = XKB_KEY_KP_0 + qtKey - Qt::Key_0;
    } else if (qtKey < 0x1000 && !modifiers.testFlag(Qt::KeypadModifier)) {
        QChar character(qtKey);
        if (!modifiers.testFlag(Qt::ShiftModifier)) {
            character = character.toLower();
        }
        sym = character.unicode();
    }

    if (sym == XKB_KEY_NoSymbol) {
        std::vector<xkb_keysym_t> possibleMatches;
        for (auto pair : s_mapping) {
            if (pair.second == qtKey) {
                possibleMatches.emplace_back(pair.first);
            }
        }
        if (!possibleMatches.empty()) {
            sym = possibleMatches.front();
            for (auto match : possibleMatches) {
                // is the current match better than existing?
                if (modifiers.testFlag(Qt::KeypadModifier)) {
                    if (isKeypadKey(match)) {
                        sym = match;
                    }
                } else {
                    if (isKeypadKey(sym)) {
                        sym = match;
                    }
                }
            }
        }
    }
    return sym;
}

}

#endif
