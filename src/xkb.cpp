/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013, 2016, 2017 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "xkb.h"
#include "dbusproperties_interface.h"
#include "inputmethod.h"
#include "utils/c_ptr.h"
#include "utils/common.h"
#include "wayland/inputmethod_v1.h"
#include "wayland/keyboard.h"
#include "wayland/seat.h"
// frameworks
#include <KConfigGroup>
// Qt
#include <QKeyEvent>
#include <QTemporaryFile>
#include <QtGui/private/qxkbcommon_p.h>
// xkbcommon
#include <xkbcommon/xkbcommon-compose.h>
#include <xkbcommon/xkbcommon-keysyms.h>
// system
#include "main.h"
#include <bitset>
#include <linux/input-event-codes.h>
#include <sys/mman.h>
#include <unistd.h>

// TODO: drop these ifdefs when xkbcommon >= 1.8.0 is required
#ifndef XKB_LED_NAME_COMPOSE
#define XKB_LED_NAME_COMPOSE "Compose"
#endif
#ifndef XKB_LED_NAME_KANA
#define XKB_LED_NAME_KANA "Kana"
#endif

Q_LOGGING_CATEGORY(KWIN_XKB, "kwin_xkbcommon", QtWarningMsg)

/* The offset between KEY_* numbering, and keycodes in the XKB evdev
 * dataset. */
static const int EVDEV_OFFSET = 8;
static const char *s_locale1Interface = "org.freedesktop.locale1";

namespace KWin
{

struct TransKey
{
    uint keySymX;
    int keySymQt;
};

//---------------------------------------------------------------------
// Arrays
//---------------------------------------------------------------------
// clang-format off

static const TransKey g_rgSymXToQT[] = {
    // The following must be kept in sync with qxkbcommon.cpp in Qt!
    { XKB_KEY_Escape,                  Qt::Key_Escape },
    { XKB_KEY_Tab,                     Qt::Key_Tab },
    { XKB_KEY_ISO_Left_Tab,            Qt::Key_Backtab },
    { XKB_KEY_BackSpace,               Qt::Key_Backspace },
    { XKB_KEY_Return,                  Qt::Key_Return },
    { XKB_KEY_Insert,                  Qt::Key_Insert },
    { XKB_KEY_Delete,                  Qt::Key_Delete },
    { XKB_KEY_Clear,                   Qt::Key_Delete },
    { XKB_KEY_Pause,                   Qt::Key_Pause },
    { XKB_KEY_Print,                   Qt::Key_Print },
    { XKB_KEY_Sys_Req,                 Qt::Key_SysReq },
    { 0x1005FF60,                      Qt::Key_SysReq },         // hardcoded Sun SysReq
    { 0x1007ff00,                      Qt::Key_SysReq },         // hardcoded X386 SysReq

    // cursor movement

    { XKB_KEY_Home,                    Qt::Key_Home },
    { XKB_KEY_End,                     Qt::Key_End },
    { XKB_KEY_Left,                    Qt::Key_Left },
    { XKB_KEY_Up,                      Qt::Key_Up },
    { XKB_KEY_Right,                   Qt::Key_Right },
    { XKB_KEY_Down,                    Qt::Key_Down },
    { XKB_KEY_Prior,                   Qt::Key_PageUp },
    { XKB_KEY_Next,                    Qt::Key_PageDown },

    // modifiers

    { XKB_KEY_Shift_L,                 Qt::Key_Shift },
    { XKB_KEY_Shift_R,                 Qt::Key_Shift },
    { XKB_KEY_Shift_Lock,              Qt::Key_Shift },
    { XKB_KEY_Control_L,               Qt::Key_Control },
    { XKB_KEY_Control_R,               Qt::Key_Control },
    { XKB_KEY_Meta_L,                  Qt::Key_Meta },
    { XKB_KEY_Meta_R,                  Qt::Key_Meta },
    { XKB_KEY_Alt_L,                   Qt::Key_Alt },
    { XKB_KEY_Alt_R,                   Qt::Key_Alt },
    { XKB_KEY_Caps_Lock,               Qt::Key_CapsLock },
    { XKB_KEY_Num_Lock,                Qt::Key_NumLock },
    { XKB_KEY_Scroll_Lock,             Qt::Key_ScrollLock },
    { XKB_KEY_Super_L,                 Qt::Key_Super_L },
    { XKB_KEY_Super_R,                 Qt::Key_Super_R },
    { XKB_KEY_Menu,                    Qt::Key_Menu },
    { XKB_KEY_Hyper_L,                 Qt::Key_Hyper_L },
    { XKB_KEY_Hyper_R,                 Qt::Key_Hyper_R },
    { XKB_KEY_Help,                    Qt::Key_Help },
    { 0x1000FF74,                      Qt::Key_Backtab },        // hardcoded HP backtab
    { 0x1005FF10,                      Qt::Key_F11 },            // hardcoded Sun F36 (labeled F11)
    { 0x1005FF11,                      Qt::Key_F12 },            // hardcoded Sun F37 (labeled F12)

    // numeric and function keypad keys

    { XKB_KEY_KP_Space,                Qt::Key_Space },
    { XKB_KEY_KP_Tab,                  Qt::Key_Tab },
    { XKB_KEY_KP_Enter,                Qt::Key_Enter },
    { XKB_KEY_KP_Home,                 Qt::Key_Home },
    { XKB_KEY_KP_Left,                 Qt::Key_Left },
    { XKB_KEY_KP_Up,                   Qt::Key_Up },
    { XKB_KEY_KP_Right,                Qt::Key_Right },
    { XKB_KEY_KP_Down,                 Qt::Key_Down },
    { XKB_KEY_KP_Prior,                Qt::Key_PageUp },
    { XKB_KEY_KP_Next,                 Qt::Key_PageDown },
    { XKB_KEY_KP_End,                  Qt::Key_End },
    { XKB_KEY_KP_Begin,                Qt::Key_Clear },
    { XKB_KEY_KP_Insert,               Qt::Key_Insert },
    { XKB_KEY_KP_Delete,               Qt::Key_Delete },
    { XKB_KEY_KP_Equal,                Qt::Key_Equal },
    { XKB_KEY_KP_Multiply,             Qt::Key_Asterisk },
    { XKB_KEY_KP_Add,                  Qt::Key_Plus },
    { XKB_KEY_KP_Separator,            Qt::Key_Comma },
    { XKB_KEY_KP_Subtract,             Qt::Key_Minus },
    { XKB_KEY_KP_Decimal,              Qt::Key_Period },
    { XKB_KEY_KP_Divide,               Qt::Key_Slash },

    // special non-XF86 function keys

    { XKB_KEY_Undo,                    Qt::Key_Undo },
    { XKB_KEY_Redo,                    Qt::Key_Redo },
    { XKB_KEY_Find,                    Qt::Key_Find },
    { XKB_KEY_Cancel,                  Qt::Key_Cancel },

    // International input method support keys

    // International & multi-key character composition
    { XKB_KEY_ISO_Level3_Shift,        Qt::Key_AltGr },
    { XKB_KEY_Multi_key,               Qt::Key_Multi_key },
    { XKB_KEY_Codeinput,               Qt::Key_Codeinput },
    { XKB_KEY_SingleCandidate,         Qt::Key_SingleCandidate },
    { XKB_KEY_MultipleCandidate,       Qt::Key_MultipleCandidate },
    { XKB_KEY_PreviousCandidate,       Qt::Key_PreviousCandidate },

    // Misc Functions
    { XKB_KEY_Mode_switch,             Qt::Key_Mode_switch },
    { XKB_KEY_script_switch,           Qt::Key_Mode_switch },

    // Japanese keyboard support
    { XKB_KEY_Kanji,                   Qt::Key_Kanji },
    { XKB_KEY_Muhenkan,                Qt::Key_Muhenkan },
    //{ XKB_KEY_Henkan_Mode,           Qt::Key_Henkan_Mode },
    { XKB_KEY_Henkan_Mode,             Qt::Key_Henkan },
    { XKB_KEY_Henkan,                  Qt::Key_Henkan },
    { XKB_KEY_Romaji,                  Qt::Key_Romaji },
    { XKB_KEY_Hiragana,                Qt::Key_Hiragana },
    { XKB_KEY_Katakana,                Qt::Key_Katakana },
    { XKB_KEY_Hiragana_Katakana,       Qt::Key_Hiragana_Katakana },
    { XKB_KEY_Zenkaku,                 Qt::Key_Zenkaku },
    { XKB_KEY_Hankaku,                 Qt::Key_Hankaku },
    { XKB_KEY_Zenkaku_Hankaku,         Qt::Key_Zenkaku_Hankaku },
    { XKB_KEY_Touroku,                 Qt::Key_Touroku },
    { XKB_KEY_Massyo,                  Qt::Key_Massyo },
    { XKB_KEY_Kana_Lock,               Qt::Key_Kana_Lock },
    { XKB_KEY_Kana_Shift,              Qt::Key_Kana_Shift },
    { XKB_KEY_Eisu_Shift,              Qt::Key_Eisu_Shift },
    { XKB_KEY_Eisu_toggle,             Qt::Key_Eisu_toggle },
    //{ XKB_KEY_Kanji_Bangou,          Qt::Key_Kanji_Bangou },
    //{ XKB_KEY_Zen_Koho,              Qt::Key_Zen_Koho },
    //{ XKB_KEY_Mae_Koho,              Qt::Key_Mae_Koho },
    { XKB_KEY_Kanji_Bangou,            Qt::Key_Codeinput },
    { XKB_KEY_Zen_Koho,                Qt::Key_MultipleCandidate },
    { XKB_KEY_Mae_Koho,                Qt::Key_PreviousCandidate },

    // Korean keyboard support
    { XKB_KEY_Hangul,                  Qt::Key_Hangul },
    { XKB_KEY_Hangul_Start,            Qt::Key_Hangul_Start },
    { XKB_KEY_Hangul_End,              Qt::Key_Hangul_End },
    { XKB_KEY_Hangul_Hanja,            Qt::Key_Hangul_Hanja },
    { XKB_KEY_Hangul_Jamo,             Qt::Key_Hangul_Jamo },
    { XKB_KEY_Hangul_Romaja,           Qt::Key_Hangul_Romaja },
    //{ XKB_KEY_Hangul_Codeinput,      Qt::Key_Hangul_Codeinput },
    { XKB_KEY_Hangul_Codeinput,        Qt::Key_Codeinput },
    { XKB_KEY_Hangul_Jeonja,           Qt::Key_Hangul_Jeonja },
    { XKB_KEY_Hangul_Banja,            Qt::Key_Hangul_Banja },
    { XKB_KEY_Hangul_PreHanja,         Qt::Key_Hangul_PreHanja },
    { XKB_KEY_Hangul_PostHanja,        Qt::Key_Hangul_PostHanja },
    //{ XKB_KEY_Hangul_SingleCandidate,Qt::Key_Hangul_SingleCandidate },
    //{ XKB_KEY_Hangul_MultipleCandidate,Qt::Key_Hangul_MultipleCandidate },
    //{ XKB_KEY_Hangul_PreviousCandidate,Qt::Key_Hangul_PreviousCandidate },
    { XKB_KEY_Hangul_SingleCandidate,  Qt::Key_SingleCandidate },
    { XKB_KEY_Hangul_MultipleCandidate,Qt::Key_MultipleCandidate },
    { XKB_KEY_Hangul_PreviousCandidate,Qt::Key_PreviousCandidate },
    { XKB_KEY_Hangul_Special,          Qt::Key_Hangul_Special },
    //{ XKB_KEY_Hangul_switch,         Qt::Key_Hangul_switch },
    { XKB_KEY_Hangul_switch,           Qt::Key_Mode_switch },

    // dead keys
    { XKB_KEY_dead_grave,              Qt::Key_Dead_Grave },
    { XKB_KEY_dead_acute,              Qt::Key_Dead_Acute },
    { XKB_KEY_dead_circumflex,         Qt::Key_Dead_Circumflex },
    { XKB_KEY_dead_tilde,              Qt::Key_Dead_Tilde },
    { XKB_KEY_dead_macron,             Qt::Key_Dead_Macron },
    { XKB_KEY_dead_breve,              Qt::Key_Dead_Breve },
    { XKB_KEY_dead_abovedot,           Qt::Key_Dead_Abovedot },
    { XKB_KEY_dead_diaeresis,          Qt::Key_Dead_Diaeresis },
    { XKB_KEY_dead_abovering,          Qt::Key_Dead_Abovering },
    { XKB_KEY_dead_doubleacute,        Qt::Key_Dead_Doubleacute },
    { XKB_KEY_dead_caron,              Qt::Key_Dead_Caron },
    { XKB_KEY_dead_cedilla,            Qt::Key_Dead_Cedilla },
    { XKB_KEY_dead_ogonek,             Qt::Key_Dead_Ogonek },
    { XKB_KEY_dead_iota,               Qt::Key_Dead_Iota },
    { XKB_KEY_dead_voiced_sound,       Qt::Key_Dead_Voiced_Sound },
    { XKB_KEY_dead_semivoiced_sound,   Qt::Key_Dead_Semivoiced_Sound },
    { XKB_KEY_dead_belowdot,           Qt::Key_Dead_Belowdot },
    { XKB_KEY_dead_hook,               Qt::Key_Dead_Hook },
    { XKB_KEY_dead_horn,               Qt::Key_Dead_Horn },
    { XKB_KEY_dead_stroke,             Qt::Key_Dead_Stroke },
    { XKB_KEY_dead_abovecomma,         Qt::Key_Dead_Abovecomma },
    { XKB_KEY_dead_abovereversedcomma, Qt::Key_Dead_Abovereversedcomma },
    { XKB_KEY_dead_doublegrave,        Qt::Key_Dead_Doublegrave },
    { XKB_KEY_dead_belowring,          Qt::Key_Dead_Belowring },
    { XKB_KEY_dead_belowmacron,        Qt::Key_Dead_Belowmacron },
    { XKB_KEY_dead_belowcircumflex,    Qt::Key_Dead_Belowcircumflex },
    { XKB_KEY_dead_belowtilde,         Qt::Key_Dead_Belowtilde },
    { XKB_KEY_dead_belowbreve,         Qt::Key_Dead_Belowbreve },
    { XKB_KEY_dead_belowdiaeresis,     Qt::Key_Dead_Belowdiaeresis },
    { XKB_KEY_dead_invertedbreve,      Qt::Key_Dead_Invertedbreve },
    { XKB_KEY_dead_belowcomma,         Qt::Key_Dead_Belowcomma },
    { XKB_KEY_dead_currency,           Qt::Key_Dead_Currency },
    { XKB_KEY_dead_a,                  Qt::Key_Dead_a },
    { XKB_KEY_dead_A,                  Qt::Key_Dead_A },
    { XKB_KEY_dead_e,                  Qt::Key_Dead_e },
    { XKB_KEY_dead_E,                  Qt::Key_Dead_E },
    { XKB_KEY_dead_i,                  Qt::Key_Dead_i },
    { XKB_KEY_dead_I,                  Qt::Key_Dead_I },
    { XKB_KEY_dead_o,                  Qt::Key_Dead_o },
    { XKB_KEY_dead_O,                  Qt::Key_Dead_O },
    { XKB_KEY_dead_u,                  Qt::Key_Dead_u },
    { XKB_KEY_dead_U,                  Qt::Key_Dead_U },
    { XKB_KEY_dead_small_schwa,        Qt::Key_Dead_Small_Schwa },
    { XKB_KEY_dead_capital_schwa,      Qt::Key_Dead_Capital_Schwa },
    { XKB_KEY_dead_greek,              Qt::Key_Dead_Greek },
/* The following four XKB_KEY_dead keys got removed in libxkbcommon 1.6.0
   The define check is kind of version check here. */
#ifdef XKB_KEY_dead_lowline
    { XKB_KEY_dead_lowline,            Qt::Key_Dead_Lowline },
    { XKB_KEY_dead_aboveverticalline,  Qt::Key_Dead_Aboveverticalline },
    { XKB_KEY_dead_belowverticalline,  Qt::Key_Dead_Belowverticalline },
    { XKB_KEY_dead_longsolidusoverlay, Qt::Key_Dead_Longsolidusoverlay },
#endif

    // Special keys from X.org - This include multimedia keys,
    // wireless/bluetooth/uwb keys, special launcher keys, etc.
    { XKB_KEY_XF86Back,                Qt::Key_Back },
    { XKB_KEY_XF86Forward,             Qt::Key_Forward },
    { XKB_KEY_XF86Stop,                Qt::Key_Stop },
    { XKB_KEY_XF86Refresh,             Qt::Key_Refresh },
    { XKB_KEY_XF86Favorites,           Qt::Key_Favorites },
    { XKB_KEY_XF86AudioMedia,          Qt::Key_LaunchMedia },
    { XKB_KEY_XF86OpenURL,             Qt::Key_OpenUrl },
    { XKB_KEY_XF86HomePage,            Qt::Key_HomePage },
    { XKB_KEY_XF86Search,              Qt::Key_Search },
    { XKB_KEY_XF86AudioLowerVolume,    Qt::Key_VolumeDown },
    { XKB_KEY_XF86AudioMute,           Qt::Key_VolumeMute },
    { XKB_KEY_XF86AudioRaiseVolume,    Qt::Key_VolumeUp },
    { XKB_KEY_XF86AudioPlay,           Qt::Key_MediaPlay },
    { XKB_KEY_XF86AudioStop,           Qt::Key_MediaStop },
    { XKB_KEY_XF86AudioPrev,           Qt::Key_MediaPrevious },
    { XKB_KEY_XF86AudioNext,           Qt::Key_MediaNext },
    { XKB_KEY_XF86AudioRecord,         Qt::Key_MediaRecord },
    { XKB_KEY_XF86AudioPause,          Qt::Key_MediaPause },
    { XKB_KEY_XF86Mail,                Qt::Key_LaunchMail },
    { XKB_KEY_XF86MyComputer,          Qt::Key_LaunchMedia },
    { XKB_KEY_XF86Memo,                Qt::Key_Memo },
    { XKB_KEY_XF86ToDoList,            Qt::Key_ToDoList },
    { XKB_KEY_XF86Calendar,            Qt::Key_Calendar },
    { XKB_KEY_XF86PowerDown,           Qt::Key_PowerDown },
    { XKB_KEY_XF86ContrastAdjust,      Qt::Key_ContrastAdjust },
    { XKB_KEY_XF86Standby,             Qt::Key_Standby },
    { XKB_KEY_XF86MonBrightnessUp,     Qt::Key_MonBrightnessUp },
    { XKB_KEY_XF86MonBrightnessDown,   Qt::Key_MonBrightnessDown },
    { XKB_KEY_XF86KbdLightOnOff,       Qt::Key_KeyboardLightOnOff },
    { XKB_KEY_XF86KbdBrightnessUp,     Qt::Key_KeyboardBrightnessUp },
    { XKB_KEY_XF86KbdBrightnessDown,   Qt::Key_KeyboardBrightnessDown },
    { XKB_KEY_XF86PowerOff,            Qt::Key_PowerOff },
    { XKB_KEY_XF86WakeUp,              Qt::Key_WakeUp },
    { XKB_KEY_XF86Eject,               Qt::Key_Eject },
    { XKB_KEY_XF86ScreenSaver,         Qt::Key_ScreenSaver },
    { XKB_KEY_XF86WWW,                 Qt::Key_WWW },
    { XKB_KEY_XF86Sleep,               Qt::Key_Sleep },
    { XKB_KEY_XF86LightBulb,           Qt::Key_LightBulb },
    { XKB_KEY_XF86Shop,                Qt::Key_Shop },
    { XKB_KEY_XF86History,             Qt::Key_History },
    { XKB_KEY_XF86AddFavorite,         Qt::Key_AddFavorite },
    { XKB_KEY_XF86HotLinks,            Qt::Key_HotLinks },
    { XKB_KEY_XF86BrightnessAdjust,    Qt::Key_BrightnessAdjust },
    { XKB_KEY_XF86Finance,             Qt::Key_Finance },
    { XKB_KEY_XF86Community,           Qt::Key_Community },
    { XKB_KEY_XF86AudioRewind,         Qt::Key_AudioRewind },
    { XKB_KEY_XF86BackForward,         Qt::Key_BackForward },
    { XKB_KEY_XF86ApplicationLeft,     Qt::Key_ApplicationLeft },
    { XKB_KEY_XF86ApplicationRight,    Qt::Key_ApplicationRight },
    { XKB_KEY_XF86Book,                Qt::Key_Book },
    { XKB_KEY_XF86CD,                  Qt::Key_CD },
    { XKB_KEY_XF86Calculater,          Qt::Key_Calculator },
    { XKB_KEY_XF86Calculator,          Qt::Key_Calculator },
    { XKB_KEY_XF86Clear,               Qt::Key_Clear },
    { XKB_KEY_XF86ClearGrab,           Qt::Key_ClearGrab },
    { XKB_KEY_XF86Close,               Qt::Key_Close },
    { XKB_KEY_XF86Copy,                Qt::Key_Copy },
    { XKB_KEY_XF86Cut,                 Qt::Key_Cut },
    { XKB_KEY_XF86Display,             Qt::Key_Display },
    { XKB_KEY_XF86DOS,                 Qt::Key_DOS },
    { XKB_KEY_XF86Documents,           Qt::Key_Documents },
    { XKB_KEY_XF86Excel,               Qt::Key_Excel },
    { XKB_KEY_XF86Explorer,            Qt::Key_Explorer },
    { XKB_KEY_XF86Game,                Qt::Key_Game },
    { XKB_KEY_XF86Go,                  Qt::Key_Go },
    { XKB_KEY_XF86iTouch,              Qt::Key_iTouch },
    { XKB_KEY_XF86LogOff,              Qt::Key_LogOff },
    { XKB_KEY_XF86Market,              Qt::Key_Market },
    { XKB_KEY_XF86Meeting,             Qt::Key_Meeting },
    { XKB_KEY_XF86MenuKB,              Qt::Key_MenuKB },
    { XKB_KEY_XF86MenuPB,              Qt::Key_MenuPB },
    { XKB_KEY_XF86MySites,             Qt::Key_MySites },
    { XKB_KEY_XF86New,                 Qt::Key_New },
    { XKB_KEY_XF86News,                Qt::Key_News },
    { XKB_KEY_XF86OfficeHome,          Qt::Key_OfficeHome },
    { XKB_KEY_XF86Open,                Qt::Key_Open },
    { XKB_KEY_XF86Option,              Qt::Key_Option },
    { XKB_KEY_XF86Paste,               Qt::Key_Paste },
    { XKB_KEY_XF86Phone,               Qt::Key_Phone },
    { XKB_KEY_XF86Reply,               Qt::Key_Reply },
    { XKB_KEY_XF86Reload,              Qt::Key_Reload },
    { XKB_KEY_XF86RotateWindows,       Qt::Key_RotateWindows },
    { XKB_KEY_XF86RotationPB,          Qt::Key_RotationPB },
    { XKB_KEY_XF86RotationKB,          Qt::Key_RotationKB },
    { XKB_KEY_XF86Save,                Qt::Key_Save },
    { XKB_KEY_XF86Send,                Qt::Key_Send },
    { XKB_KEY_XF86Spell,               Qt::Key_Spell },
    { XKB_KEY_XF86SplitScreen,         Qt::Key_SplitScreen },
    { XKB_KEY_XF86Support,             Qt::Key_Support },
    { XKB_KEY_XF86TaskPane,            Qt::Key_TaskPane },
    { XKB_KEY_XF86Terminal,            Qt::Key_Terminal },
    { XKB_KEY_XF86Tools,               Qt::Key_Tools },
    { XKB_KEY_XF86Travel,              Qt::Key_Travel },
    { XKB_KEY_XF86Video,               Qt::Key_Video },
    { XKB_KEY_XF86Word,                Qt::Key_Word },
    { XKB_KEY_XF86Xfer,                Qt::Key_Xfer },
    { XKB_KEY_XF86ZoomIn,              Qt::Key_ZoomIn },
    { XKB_KEY_XF86ZoomOut,             Qt::Key_ZoomOut },
    { XKB_KEY_XF86Away,                Qt::Key_Away },
    { XKB_KEY_XF86Messenger,           Qt::Key_Messenger },
    { XKB_KEY_XF86WebCam,              Qt::Key_WebCam },
    { XKB_KEY_XF86MailForward,         Qt::Key_MailForward },
    { XKB_KEY_XF86Pictures,            Qt::Key_Pictures },
    { XKB_KEY_XF86Music,               Qt::Key_Music },
    { XKB_KEY_XF86Battery,             Qt::Key_Battery },
    { XKB_KEY_XF86Bluetooth,           Qt::Key_Bluetooth },
    { XKB_KEY_XF86WLAN,                Qt::Key_WLAN },
    { XKB_KEY_XF86UWB,                 Qt::Key_UWB },
    { XKB_KEY_XF86AudioForward,        Qt::Key_AudioForward },
    { XKB_KEY_XF86AudioRepeat,         Qt::Key_AudioRepeat },
    { XKB_KEY_XF86AudioRandomPlay,     Qt::Key_AudioRandomPlay },
    { XKB_KEY_XF86Subtitle,            Qt::Key_Subtitle },
    { XKB_KEY_XF86AudioCycleTrack,     Qt::Key_AudioCycleTrack },
    { XKB_KEY_XF86Time,                Qt::Key_Time },
    { XKB_KEY_XF86Select,              Qt::Key_Select },
    { XKB_KEY_XF86View,                Qt::Key_View },
    { XKB_KEY_XF86TopMenu,             Qt::Key_TopMenu },
    { XKB_KEY_XF86Red,                 Qt::Key_Red },
    { XKB_KEY_XF86Green,               Qt::Key_Green },
    { XKB_KEY_XF86Yellow,              Qt::Key_Yellow },
    { XKB_KEY_XF86Blue,                Qt::Key_Blue },
    { XKB_KEY_XF86Bluetooth,           Qt::Key_Bluetooth },
    { XKB_KEY_XF86Suspend,             Qt::Key_Suspend },
    { XKB_KEY_XF86Hibernate,           Qt::Key_Hibernate },
    { XKB_KEY_XF86TouchpadToggle,      Qt::Key_TouchpadToggle },
    { XKB_KEY_XF86TouchpadOn,          Qt::Key_TouchpadOn },
    { XKB_KEY_XF86TouchpadOff,         Qt::Key_TouchpadOff },
    { XKB_KEY_XF86AudioMicMute,        Qt::Key_MicMute },
    { XKB_KEY_XF86Launch0,             Qt::Key_Launch0 },
    { XKB_KEY_XF86Launch1,             Qt::Key_Launch1 },
    { XKB_KEY_XF86Launch2,             Qt::Key_Launch2 },
    { XKB_KEY_XF86Launch3,             Qt::Key_Launch3 },
    { XKB_KEY_XF86Launch4,             Qt::Key_Launch4 },
    { XKB_KEY_XF86Launch5,             Qt::Key_Launch5 },
    { XKB_KEY_XF86Launch6,             Qt::Key_Launch6 },
    { XKB_KEY_XF86Launch7,             Qt::Key_Launch7 },
    { XKB_KEY_XF86Launch8,             Qt::Key_Launch8 },
    { XKB_KEY_XF86Launch9,             Qt::Key_Launch9 },
    { XKB_KEY_XF86LaunchA,             Qt::Key_LaunchA },
    { XKB_KEY_XF86LaunchB,             Qt::Key_LaunchB },
    { XKB_KEY_XF86LaunchC,             Qt::Key_LaunchC },
    { XKB_KEY_XF86LaunchD,             Qt::Key_LaunchD },
    { XKB_KEY_XF86LaunchE,             Qt::Key_LaunchE },
    { XKB_KEY_XF86LaunchF,             Qt::Key_LaunchF },
};
// clang-format on

static void xkbLogHandler(xkb_context *context, xkb_log_level priority, const char *format, va_list args)
{
    char buf[1024];
    int length = std::vsnprintf(buf, 1023, format, args);
    while (length > 0 && std::isspace(buf[length - 1])) {
        --length;
    }
    if (length <= 0) {
        return;
    }
    switch (priority) {
    case XKB_LOG_LEVEL_DEBUG:
        qCDebug(KWIN_XKB, "XKB: %.*s", length, buf);
        break;
    case XKB_LOG_LEVEL_INFO:
        qCInfo(KWIN_XKB, "XKB: %.*s", length, buf);
        break;
    case XKB_LOG_LEVEL_WARNING:
        qCWarning(KWIN_XKB, "XKB: %.*s", length, buf);
        break;
    case XKB_LOG_LEVEL_ERROR:
    case XKB_LOG_LEVEL_CRITICAL:
    default:
        qCCritical(KWIN_XKB, "XKB: %.*s", length, buf);
        break;
    }
}

#if HAVE_XKBCOMMON_NO_SECURE_GETENV
constexpr xkb_context_flags KWIN_XKB_CONTEXT_FLAGS = XKB_CONTEXT_NO_SECURE_GETENV;
#else
constexpr xkb_context_flags KWIN_XKB_CONTEXT_FLAGS = XKB_CONTEXT_NO_FLAGS;
#endif

Xkb::Xkb(bool followLocale1)
    : m_context(xkb_context_new(KWIN_XKB_CONTEXT_FLAGS))
    , m_keymap(nullptr)
    , m_state(nullptr)
    , m_shiftModifier(0)
    , m_capsModifier(0)
    , m_controlModifier(0)
    , m_altModifier(0)
    , m_metaModifier(0)
    , m_numModifier(0)
    , m_numLock(0)
    , m_capsLock(0)
    , m_scrollLock(0)
    , m_composeLed(0)
    , m_kanaLed(0)
    , m_modifiers(Qt::NoModifier)
    , m_consumedModifiers(Qt::NoModifier)
    , m_keysym(XKB_KEY_NoSymbol)
    , m_leds()
    , m_followLocale1(followLocale1)
{
    qRegisterMetaType<KWin::LEDs>();
    if (!m_context) {
        qCDebug(KWIN_XKB) << "Could not create xkb context";
    } else {
        xkb_context_set_log_level(m_context, XKB_LOG_LEVEL_DEBUG);
        xkb_context_set_log_fn(m_context, &xkbLogHandler);

        // get locale as described in xkbcommon doc
        // cannot use QLocale as it drops the modifier part
        QByteArray locale = qgetenv("LC_ALL");
        if (locale.isEmpty()) {
            locale = qgetenv("LC_CTYPE");
        }
        if (locale.isEmpty()) {
            locale = qgetenv("LANG");
        }
        if (locale.isEmpty()) {
            locale = QByteArrayLiteral("C");
        }

        m_compose.table = xkb_compose_table_new_from_locale(m_context, locale.constData(), XKB_COMPOSE_COMPILE_NO_FLAGS);
        if (m_compose.table) {
            m_compose.state = xkb_compose_state_new(m_compose.table, XKB_COMPOSE_STATE_NO_FLAGS);
        }
    }

    if (m_followLocale1) {
        bool connected = QDBusConnection::systemBus().connect(s_locale1Interface, "/org/freedesktop/locale1", QStringLiteral("org.freedesktop.DBus.Properties"),
                                                              QStringLiteral("PropertiesChanged"),
                                                              this,
                                                              SLOT(reconfigure()));
        if (!connected) {
            qCWarning(KWIN_XKB) << "Could not connect to org.freedesktop.locale1";
        }
    }
}

Xkb::~Xkb()
{
    xkb_compose_state_unref(m_compose.state);
    xkb_compose_table_unref(m_compose.table);
    xkb_state_unref(m_state);
    xkb_keymap_unref(m_keymap);
    xkb_context_unref(m_context);
}

void Xkb::setConfig(const KSharedConfigPtr &config)
{
    m_configGroup = config->group(QStringLiteral("Layout"));
}

void Xkb::setNumLockConfig(const KSharedConfigPtr &config)
{
    m_numLockConfig = config;
}

void Xkb::reconfigure()
{
    if (!m_context) {
        return;
    }

    xkb_keymap *keymap = nullptr;
    if (!qEnvironmentVariableIsSet("KWIN_XKB_DEFAULT_KEYMAP")) {
        if (m_followLocale1) {
            keymap = loadKeymapFromLocale1();
        } else {
            keymap = loadKeymapFromConfig();
        }
    }
    if (!keymap) {
        qCDebug(KWIN_XKB) << "Could not create xkb keymap from configuration";
        keymap = loadDefaultKeymap();
    }
    if (keymap) {
        updateKeymap(keymap);
    } else {
        qCDebug(KWIN_XKB) << "Could not create default xkb keymap";
    }
}

static bool stringIsEmptyOrNull(const char *str)
{
    return str == nullptr || str[0] == '\0';
}

/**
 * libxkbcommon uses secure_getenv to read the XKB_DEFAULT_* variables.
 * As kwin_wayland may have the CAP_SET_NICE capability, it returns nullptr
 * so we need to do it ourselves (see xkb_context_sanitize_rule_names).
 **/
void Xkb::applyEnvironmentRules(xkb_rule_names &ruleNames)
{
    if (stringIsEmptyOrNull(ruleNames.rules)) {
        ruleNames.rules = getenv("XKB_DEFAULT_RULES");
    }

    if (stringIsEmptyOrNull(ruleNames.model)) {
        ruleNames.model = getenv("XKB_DEFAULT_MODEL");
    }

    if (stringIsEmptyOrNull(ruleNames.layout)) {
        ruleNames.layout = getenv("XKB_DEFAULT_LAYOUT");
        ruleNames.variant = getenv("XKB_DEFAULT_VARIANT");
    }

    if (ruleNames.options == nullptr) {
        ruleNames.options = getenv("XKB_DEFAULT_OPTIONS");
    }
}

xkb_keymap *Xkb::loadKeymapFromConfig()
{
    // load config
    if (!m_configGroup.isValid()) {
        return nullptr;
    }
    const QByteArray model = m_configGroup.readEntry("Model", "pc104").toLatin1();
    const QByteArray layout = m_configGroup.readEntry("LayoutList").toLatin1();
    const QByteArray variant = m_configGroup.readEntry("VariantList").toLatin1();
    const QByteArray options = m_configGroup.readEntry("Options").toLatin1();

    xkb_rule_names ruleNames = {
        .rules = nullptr,
        .model = model.constData(),
        .layout = layout.constData(),
        .variant = variant.constData(),
        .options = nullptr,
    };

    if (m_configGroup.readEntry("ResetOldOptions", false)) {
        ruleNames.options = options.constData();
    }

    applyEnvironmentRules(ruleNames);

    m_layoutList = QString::fromLatin1(ruleNames.layout).split(QLatin1Char(','));

    return xkb_keymap_new_from_names(m_context, &ruleNames, XKB_KEYMAP_COMPILE_NO_FLAGS);
}

xkb_keymap *Xkb::loadDefaultKeymap()
{
    xkb_rule_names ruleNames = {};
    applyEnvironmentRules(ruleNames);
    m_layoutList = QString::fromLatin1(ruleNames.layout).split(QLatin1Char(','));
    return xkb_keymap_new_from_names(m_context, &ruleNames, XKB_KEYMAP_COMPILE_NO_FLAGS);
}

xkb_keymap *Xkb::loadKeymapFromLocale1()
{
    OrgFreedesktopDBusPropertiesInterface locale1Properties(s_locale1Interface, "/org/freedesktop/locale1", QDBusConnection::systemBus(), this);
    const QVariantMap properties = locale1Properties.GetAll(s_locale1Interface);

    const QByteArray model = properties["X11Model"].toByteArray();
    const QByteArray layout = properties["X11Layout"].toByteArray();
    const QByteArray variant = properties["X11Variant"].toByteArray();
    const QByteArray options = properties["X11Options"].toByteArray();

    xkb_rule_names ruleNames = {
        .rules = nullptr,
        .model = model.constData(),
        .layout = layout.constData(),
        .variant = variant.constData(),
        .options = options.constData(),
    };

    applyEnvironmentRules(ruleNames);

    m_layoutList = QString::fromLatin1(ruleNames.layout).split(QLatin1Char(','));

    return xkb_keymap_new_from_names(m_context, &ruleNames, XKB_KEYMAP_COMPILE_NO_FLAGS);
}

void Xkb::updateKeymap(xkb_keymap *keymap)
{
    Q_ASSERT(keymap);
    xkb_state *state = xkb_state_new(keymap);
    if (!state) {
        qCDebug(KWIN_XKB) << "Could not create XKB state";
        xkb_keymap_unref(keymap);
        return;
    }

    // save Locks
    bool numLockIsOn, capsLockIsOn;
    static bool s_startup = true;
    if (!s_startup) {
        numLockIsOn = xkb_state_mod_index_is_active(m_state, m_numModifier, XKB_STATE_MODS_LOCKED);
        capsLockIsOn = xkb_state_mod_index_is_active(m_state, m_capsModifier, XKB_STATE_MODS_LOCKED);
    }

    // now release the old ones
    xkb_state_unref(m_state);
    xkb_keymap_unref(m_keymap);

    m_keymap = keymap;
    m_state = state;

    m_shiftModifier = xkb_keymap_mod_get_index(m_keymap, XKB_MOD_NAME_SHIFT);
    m_capsModifier = xkb_keymap_mod_get_index(m_keymap, XKB_MOD_NAME_CAPS);
    m_controlModifier = xkb_keymap_mod_get_index(m_keymap, XKB_MOD_NAME_CTRL);
    m_altModifier = xkb_keymap_mod_get_index(m_keymap, XKB_MOD_NAME_ALT);
    m_metaModifier = xkb_keymap_mod_get_index(m_keymap, XKB_MOD_NAME_LOGO);
    m_numModifier = xkb_keymap_mod_get_index(m_keymap, XKB_MOD_NAME_NUM);
    m_mod5Modifier = xkb_keymap_mod_get_index(m_keymap, "Mod5");

    m_numLock = xkb_keymap_led_get_index(m_keymap, XKB_LED_NAME_NUM);
    m_capsLock = xkb_keymap_led_get_index(m_keymap, XKB_LED_NAME_CAPS);
    m_scrollLock = xkb_keymap_led_get_index(m_keymap, XKB_LED_NAME_SCROLL);
    m_composeLed = xkb_keymap_led_get_index(m_keymap, XKB_LED_NAME_COMPOSE);
    m_kanaLed = xkb_keymap_led_get_index(m_keymap, XKB_LED_NAME_KANA);

    m_currentLayout = xkb_state_serialize_layout(m_state, XKB_STATE_LAYOUT_EFFECTIVE);

    m_modifierState.depressed = xkb_state_serialize_mods(m_state, xkb_state_component(XKB_STATE_MODS_DEPRESSED));
    m_modifierState.latched = xkb_state_serialize_mods(m_state, xkb_state_component(XKB_STATE_MODS_LATCHED));
    m_modifierState.locked = xkb_state_serialize_mods(m_state, xkb_state_component(XKB_STATE_MODS_LOCKED));

    auto setLock = [this](xkb_mod_index_t modifier, bool value) {
        if (modifier != XKB_MOD_INVALID) {
            std::bitset<sizeof(xkb_mod_mask_t) * 8> mask{m_modifierState.locked};
            if (mask.size() > modifier) {
                mask[modifier] = value;
                m_modifierState.locked = mask.to_ulong();
                xkb_state_update_mask(m_state, m_modifierState.depressed, m_modifierState.latched, m_modifierState.locked, 0, 0, m_currentLayout);
                m_modifierState.locked = xkb_state_serialize_mods(m_state, xkb_state_component(XKB_STATE_MODS_LOCKED));
            }
        }
    };

    if (s_startup || qEnvironmentVariableIsSet("KWIN_FORCE_NUM_LOCK_EVALUATION")) {
        s_startup = false;
        if (m_numLockConfig) {
            const KConfigGroup config = m_numLockConfig->group(QStringLiteral("Keyboard"));
            // STATE_ON = 0,  STATE_OFF = 1, STATE_UNCHANGED = 2, see plasma-desktop/kcms/keyboard/kcmmisc.h
            const auto setting = config.readEntry("NumLock", 2);
            if (setting != 2) {
                setLock(m_numModifier, !setting);
            }
        }
    } else {
        setLock(m_numModifier, numLockIsOn);
        setLock(m_capsModifier, capsLockIsOn);
    }

    createKeymapFile();
    forwardModifiers();
    if (auto *inputmethod = kwinApp()->inputMethod()) {
        inputmethod->forwardModifiers(InputMethod::Force);
    }
    updateModifiers();
}

void Xkb::createKeymapFile()
{
    const auto currentKeymap = keymapContents();
    if (currentKeymap.isEmpty()) {
        return;
    }
    m_seat->keyboard()->setKeymap(currentKeymap);
    auto *inputmethod = kwinApp()->inputMethod();
    if (!inputmethod) {
        return;
    }
    if (auto *keyboardGrab = inputmethod->keyboardGrab()) {
        keyboardGrab->sendKeymap(currentKeymap);
    }
}

QByteArray Xkb::keymapContents() const
{
    if (!m_seat || !m_seat->keyboard()) {
        return {};
    }
    // TODO: uninstall keymap on server?
    if (!m_keymap) {
        return {};
    }

    UniqueCPtr<char> keymapString(xkb_keymap_get_as_string(m_keymap, XKB_KEYMAP_FORMAT_TEXT_V1));
    if (!keymapString) {
        return {};
    }
    return keymapString.get();
}

void Xkb::updateModifiers(uint32_t modsDepressed, uint32_t modsLatched, uint32_t modsLocked, uint32_t group)
{
    if (!m_keymap || !m_state) {
        return;
    }
    // Avoid to create a infinite loop between input method and compositor.
    if (xkb_state_update_mask(m_state, modsDepressed, modsLatched, modsLocked, 0, 0, group) == 0) {
        return;
    }
    updateModifiers();
    forwardModifiers();
}

void Xkb::updateKey(uint32_t key, KeyboardKeyState state)
{
    if (!m_keymap || !m_state) {
        return;
    }
    const auto sym = toKeysym(key);
    xkb_state_update_key(m_state, key + EVDEV_OFFSET, static_cast<xkb_key_direction>(state));
    if (m_compose.state) {
        if (state == KeyboardKeyState::Pressed) {
            xkb_compose_state_feed(m_compose.state, sym);
        }
        switch (xkb_compose_state_get_status(m_compose.state)) {
        case XKB_COMPOSE_NOTHING:
            m_keysym = sym;
            break;
        case XKB_COMPOSE_COMPOSED:
            m_keysym = xkb_compose_state_get_one_sym(m_compose.state);
            break;
        default:
            m_keysym = XKB_KEY_NoSymbol;
            break;
        }
    } else {
        m_keysym = sym;
    }
    updateModifiers();
    updateConsumedModifiers(key);
}

void Xkb::updateModifiers()
{
    Qt::KeyboardModifiers mods = Qt::NoModifier;
    if (xkb_state_mod_index_is_active(m_state, m_shiftModifier, XKB_STATE_MODS_EFFECTIVE) == 1) {
        mods |= Qt::ShiftModifier;
    }
    if (xkb_state_mod_index_is_active(m_state, m_altModifier, XKB_STATE_MODS_EFFECTIVE) == 1) {
        mods |= Qt::AltModifier;
    }
    if (xkb_state_mod_index_is_active(m_state, m_controlModifier, XKB_STATE_MODS_EFFECTIVE) == 1) {
        mods |= Qt::ControlModifier;
    }
    if (xkb_state_mod_index_is_active(m_state, m_metaModifier, XKB_STATE_MODS_EFFECTIVE) == 1) {
        mods |= Qt::MetaModifier;
    }
    if (m_keysym >= XKB_KEY_KP_Space && m_keysym <= XKB_KEY_KP_Equal) {
        mods |= Qt::KeypadModifier;
    }
    m_modifiers = mods;

    // update LEDs
    LEDs leds;
    if (xkb_state_led_index_is_active(m_state, m_numLock) == 1) {
        leds = leds | LED::NumLock;
    }
    if (xkb_state_led_index_is_active(m_state, m_capsLock) == 1) {
        leds = leds | LED::CapsLock;
    }
    if (xkb_state_led_index_is_active(m_state, m_scrollLock) == 1) {
        leds = leds | LED::ScrollLock;
    }
    if (xkb_state_led_index_is_active(m_state, m_composeLed) == 1) {
        leds = leds | LED::Compose;
    }
    if (xkb_state_led_index_is_active(m_state, m_kanaLed) == 1) {
        leds = leds | LED::Kana;
    }
    if (m_leds != leds) {
        m_leds = leds;
        Q_EMIT ledsChanged(m_leds);
    }

    const uint32_t newLayout = xkb_state_serialize_layout(m_state, XKB_STATE_LAYOUT_EFFECTIVE);
    const uint32_t depressed = xkb_state_serialize_mods(m_state, XKB_STATE_MODS_DEPRESSED);
    const uint32_t latched = xkb_state_serialize_mods(m_state, XKB_STATE_MODS_LATCHED);
    const uint32_t locked = xkb_state_serialize_mods(m_state, XKB_STATE_MODS_LOCKED);

    if (newLayout != m_currentLayout || depressed != m_modifierState.depressed || latched != m_modifierState.latched || locked != m_modifierState.locked) {
        m_currentLayout = newLayout;
        m_modifierState.depressed = depressed;
        m_modifierState.latched = latched;
        m_modifierState.locked = locked;

        Q_EMIT modifierStateChanged();
    }
}

void Xkb::forwardModifiers()
{
    if (!m_seat || !m_seat->keyboard()) {
        return;
    }
    m_seat->notifyKeyboardModifiers(m_modifierState.depressed,
                                    m_modifierState.latched,
                                    m_modifierState.locked,
                                    m_currentLayout);
}

QString Xkb::layoutName(xkb_layout_index_t index) const
{
    if (!m_keymap) {
        return QString{};
    }
    return QString::fromLocal8Bit(xkb_keymap_layout_get_name(m_keymap, index));
}

QString Xkb::layoutName() const
{
    return layoutName(m_currentLayout);
}

QString Xkb::layoutShortName(int index) const
{
    return m_layoutList.value(index);
}

void Xkb::updateConsumedModifiers(uint32_t key)
{
    Qt::KeyboardModifiers mods = Qt::NoModifier;
    if (xkb_state_mod_index_is_consumed2(m_state, key + EVDEV_OFFSET, m_shiftModifier, XKB_CONSUMED_MODE_GTK) == 1) {
        mods |= Qt::ShiftModifier;
    }
    if (xkb_state_mod_index_is_consumed2(m_state, key + EVDEV_OFFSET, m_altModifier, XKB_CONSUMED_MODE_GTK) == 1) {
        mods |= Qt::AltModifier;
    }
    if (xkb_state_mod_index_is_consumed2(m_state, key + EVDEV_OFFSET, m_controlModifier, XKB_CONSUMED_MODE_GTK) == 1) {
        mods |= Qt::ControlModifier;
    }
    if (xkb_state_mod_index_is_consumed2(m_state, key + EVDEV_OFFSET, m_metaModifier, XKB_CONSUMED_MODE_GTK) == 1) {
        mods |= Qt::MetaModifier;
    }
    m_consumedModifiers = mods;
}

Qt::KeyboardModifiers Xkb::modifiersRelevantForGlobalShortcuts(uint32_t scanCode) const
{
    if (!m_state) {
        return Qt::NoModifier;
    }
    Qt::KeyboardModifiers mods = Qt::NoModifier;
    if (xkb_state_mod_index_is_active(m_state, m_shiftModifier, XKB_STATE_MODS_EFFECTIVE) == 1) {
        mods |= Qt::ShiftModifier;
    }
    if (xkb_state_mod_index_is_active(m_state, m_altModifier, XKB_STATE_MODS_EFFECTIVE) == 1) {
        mods |= Qt::AltModifier;
    }
    if (xkb_state_mod_index_is_active(m_state, m_controlModifier, XKB_STATE_MODS_EFFECTIVE) == 1) {
        mods |= Qt::ControlModifier;
    }
    if (xkb_state_mod_index_is_active(m_state, m_metaModifier, XKB_STATE_MODS_EFFECTIVE) == 1) {
        mods |= Qt::MetaModifier;
    }
    if (m_keysym >= XKB_KEY_KP_Space && m_keysym <= XKB_KEY_KP_Equal) {
        mods |= Qt::KeypadModifier;
    }

    Qt::KeyboardModifiers consumedMods = m_consumedModifiers;
    if ((mods & Qt::ShiftModifier) && (consumedMods == Qt::ShiftModifier)) {
        // test whether current keysym is a letter
        // in that case the shift should be removed from the consumed modifiers again
        // otherwise it would not be possible to trigger e.g. Shift+W as a shortcut
        // see BUG: 370341
        if (QChar::isLetter(toQtKey(m_keysym, scanCode, Qt::ControlModifier))) {
            consumedMods = Qt::KeyboardModifiers();
        }
    }

    return mods & ~consumedMods;
}

xkb_keysym_t Xkb::toKeysym(uint32_t key)
{
    if (!m_state) {
        return XKB_KEY_NoSymbol;
    }

    // Workaround because there's some kind of overlap between KEY_ZENKAKUHANKAKU and TLDE
    // This key is important because some hardware manufacturers use it to indicate touchpad toggling.
    xkb_keysym_t ret = xkb_state_key_get_one_sym(m_state, key + EVDEV_OFFSET);
    if (ret == 0 && key == KEY_ZENKAKUHANKAKU) {
        ret = XKB_KEY_Zenkaku_Hankaku;
    }
    return ret;
}

QString Xkb::toString(xkb_keysym_t keysym)
{
    if (!m_state || keysym == XKB_KEY_NoSymbol) {
        return QString();
    }
    QByteArray byteArray(7, 0);
    int ok = xkb_keysym_to_utf8(keysym, byteArray.data(), byteArray.size());
    if (ok == -1 || ok == 0) {
        return QString();
    }
    return QString::fromUtf8(byteArray.constData());
}

Qt::Key Xkb::toQtKey(xkb_keysym_t keySym,
                     uint32_t scanCode,
                     Qt::KeyboardModifiers modifiers) const
{
    // FIXME: passing superAsMeta doesn't have impact due to bug in the Qt function, so handle it below
    Qt::Key qtKey = Qt::Key(QXkbCommon::keysymToQtKey(keySym, modifiers, m_state, scanCode + EVDEV_OFFSET));

    // FIXME: workarounds for symbols currently wrong/not mappable via keysymToQtKey()
    if (qtKey > 0xff && keySym <= 0xff) {
        // XKB_KEY_mu, XKB_KEY_ydiaeresis go here
        qtKey = Qt::Key(keySym);
    }
    return qtKey;
}

bool Xkb::shouldKeyRepeat(quint32 key) const
{
    if (!m_keymap) {
        return false;
    }
    return xkb_keymap_key_repeats(m_keymap, key + EVDEV_OFFSET) != 0;
}

void Xkb::switchToNextLayout()
{
    if (!m_keymap || !m_state) {
        return;
    }
    const xkb_layout_index_t numLayouts = xkb_keymap_num_layouts(m_keymap);
    const xkb_layout_index_t nextLayout = (xkb_state_serialize_layout(m_state, XKB_STATE_LAYOUT_EFFECTIVE) + 1) % numLayouts;
    switchToLayout(nextLayout);
}

void Xkb::switchToPreviousLayout()
{
    if (!m_keymap || !m_state) {
        return;
    }
    const xkb_layout_index_t previousLayout = m_currentLayout == 0 ? numberOfLayouts() - 1 : m_currentLayout - 1;
    switchToLayout(previousLayout);
}

bool Xkb::switchToLayout(xkb_layout_index_t layout)
{
    if (!m_keymap || !m_state || layout >= numberOfLayouts()) {
        return false;
    }
    const xkb_mod_mask_t depressed = xkb_state_serialize_mods(m_state, xkb_state_component(XKB_STATE_MODS_DEPRESSED));
    const xkb_mod_mask_t latched = xkb_state_serialize_mods(m_state, xkb_state_component(XKB_STATE_MODS_LATCHED));
    const xkb_mod_mask_t locked = xkb_state_serialize_mods(m_state, xkb_state_component(XKB_STATE_MODS_LOCKED));
    xkb_state_update_mask(m_state, depressed, latched, locked, 0, 0, layout);
    updateModifiers();
    forwardModifiers();
    return true;
}

void Xkb::setModifierLatched(KWin::Xkb::Modifier mod, bool latched)
{
    xkb_mod_index_t modifier = XKB_MOD_INVALID;

    switch (mod) {
    case NoModifier: {
        break;
    }
    case Shift: {
        modifier = m_shiftModifier;
        break;
    }
    case Mod1: {
        modifier = m_altModifier;
        break;
    }
    case Control: {
        modifier = m_controlModifier;
        break;
    }
    case Mod4: {
        modifier = m_metaModifier;
        break;
    }
    case Mod5: {
        modifier = m_mod5Modifier;
        break;
    }
    case Num: {
        modifier = m_numModifier;
        break;
    }
    case Mod3: {
        break;
    }
    case Lock: {
        modifier = m_capsModifier;
        break;
    }
    }

    if (modifier != XKB_MOD_INVALID) {
        std::bitset<sizeof(xkb_mod_mask_t) * 8> mask{m_modifierState.latched};
        if (mask.size() > modifier) {
            mask[modifier] = latched;
            m_modifierState.latched = mask.to_ulong();
            xkb_state_update_mask(m_state, m_modifierState.depressed, m_modifierState.latched, m_modifierState.locked, 0, 0, m_currentLayout);
            m_modifierState.latched = xkb_state_serialize_mods(m_state, xkb_state_component(XKB_STATE_MODS_LATCHED));
        }
    }
}

Xkb::Modifiers Xkb::depressedModifiers() const
{
    Xkb::Modifiers result;

    if (xkb_state_mod_index_is_active(m_state, m_altModifier, XKB_STATE_MODS_DEPRESSED) == 1) {
        result |= Modifier::Mod1;
    } else if (xkb_state_mod_index_is_active(m_state, m_controlModifier, XKB_STATE_MODS_DEPRESSED) == 1) {
        result |= Modifier::Control;
    } else if (xkb_state_mod_index_is_active(m_state, m_shiftModifier, XKB_STATE_MODS_DEPRESSED) == 1) {
        result |= Modifier::Shift;
    } else if (xkb_state_mod_index_is_active(m_state, m_metaModifier, XKB_STATE_MODS_DEPRESSED) == 1) {
        result |= Modifier::Mod4;
    } else if (xkb_state_mod_index_is_active(m_state, m_mod5Modifier, XKB_STATE_MODS_DEPRESSED) == 1) {
        result |= Modifier::Mod5;
    } else if (xkb_state_mod_index_is_active(m_state, m_capsModifier, XKB_STATE_MODS_DEPRESSED) == 1) {
        result |= Modifier::Lock;
    } else if (xkb_state_mod_index_is_active(m_state, m_numModifier, XKB_STATE_MODS_DEPRESSED) == 1) {
        result |= Modifier::Num;
    }

    return result;
}

Xkb::Modifiers Xkb::latchedModifiers() const
{
    Xkb::Modifiers result;

    if (xkb_state_mod_index_is_active(m_state, m_altModifier, XKB_STATE_MODS_LATCHED) == 1) {
        result |= Modifier::Mod1;
    } else if (xkb_state_mod_index_is_active(m_state, m_controlModifier, XKB_STATE_MODS_LATCHED) == 1) {
        result |= Modifier::Control;
    } else if (xkb_state_mod_index_is_active(m_state, m_shiftModifier, XKB_STATE_MODS_LATCHED) == 1) {
        result |= Modifier::Shift;
    } else if (xkb_state_mod_index_is_active(m_state, m_metaModifier, XKB_STATE_MODS_LATCHED) == 1) {
        result |= Modifier::Mod4;
    } else if (xkb_state_mod_index_is_active(m_state, m_mod5Modifier, XKB_STATE_MODS_LATCHED) == 1) {
        result |= Modifier::Mod5;
    } else if (xkb_state_mod_index_is_active(m_state, m_capsModifier, XKB_STATE_MODS_LATCHED) == 1) {
        result |= Modifier::Lock;
    } else if (xkb_state_mod_index_is_active(m_state, m_numModifier, XKB_STATE_MODS_LATCHED) == 1) {
        result |= Modifier::Num;
    }

    return result;
}

Xkb::Modifiers Xkb::lockedModifiers() const
{
    Xkb::Modifiers result;

    if (xkb_state_mod_index_is_active(m_state, m_altModifier, XKB_STATE_MODS_LOCKED) == 1) {
        result |= Modifier::Mod1;
    } else if (xkb_state_mod_index_is_active(m_state, m_controlModifier, XKB_STATE_MODS_LOCKED) == 1) {
        result |= Modifier::Control;
    } else if (xkb_state_mod_index_is_active(m_state, m_shiftModifier, XKB_STATE_MODS_LOCKED) == 1) {
        result |= Modifier::Shift;
    } else if (xkb_state_mod_index_is_active(m_state, m_metaModifier, XKB_STATE_MODS_LOCKED) == 1) {
        result |= Modifier::Mod4;
    } else if (xkb_state_mod_index_is_active(m_state, m_mod5Modifier, XKB_STATE_MODS_LOCKED) == 1) {
        result |= Modifier::Mod5;
    } else if (xkb_state_mod_index_is_active(m_state, m_capsModifier, XKB_STATE_MODS_LOCKED) == 1) {
        result |= Modifier::Lock;
    } else if (xkb_state_mod_index_is_active(m_state, m_numModifier, XKB_STATE_MODS_LOCKED) == 1) {
        result |= Modifier::Num;
    }

    return result;
}

void Xkb::setModifierLocked(KWin::Xkb::Modifier mod, bool locked)
{
    xkb_mod_index_t modifier = XKB_MOD_INVALID;

    switch (mod) {
    case NoModifier: {
        break;
    }
    case Shift: {
        modifier = m_shiftModifier;
        break;
    }
    case Mod1: {
        modifier = m_altModifier;
        break;
    }
    case Control: {
        modifier = m_controlModifier;
        break;
    }
    case Mod4: {
        modifier = m_metaModifier;
        break;
    }
    case Mod5: {
        modifier = m_mod5Modifier;
        break;
    }
    case Num: {
        modifier = m_numModifier;
        break;
    }
    case Mod3: {
        break;
    }
    case Lock: {
        modifier = m_capsModifier;
        break;
    }
    }

    if (modifier != XKB_MOD_INVALID) {
        std::bitset<sizeof(xkb_mod_mask_t) * 8> mask{m_modifierState.locked};
        if (mask.size() > modifier) {
            mask[modifier] = locked;
            m_modifierState.locked = mask.to_ulong();
            xkb_state_update_mask(m_state, m_modifierState.depressed, m_modifierState.latched, m_modifierState.locked, 0, 0, m_currentLayout);
            m_modifierState.locked = xkb_state_serialize_mods(m_state, xkb_state_component(XKB_STATE_MODS_LOCKED));
        }
    }
}

quint32 Xkb::numberOfLayouts() const
{
    if (!m_keymap) {
        return 0;
    }
    return xkb_keymap_num_layouts(m_keymap);
}

void Xkb::setSeat(SeatInterface *seat)
{
    m_seat = QPointer<SeatInterface>(seat);
}

std::optional<std::pair<int, int>> Xkb::keycodeFromKeysym(xkb_keysym_t keysym)
{
    auto layout = xkb_state_serialize_layout(m_state, XKB_STATE_LAYOUT_EFFECTIVE);
    const xkb_keycode_t max = xkb_keymap_max_keycode(m_keymap);
    for (xkb_keycode_t keycode = xkb_keymap_min_keycode(m_keymap); keycode < max; keycode++) {
        uint levelCount = xkb_keymap_num_levels_for_key(m_keymap, keycode, layout);
        for (uint currentLevel = 0; currentLevel < levelCount; currentLevel++) {
            const xkb_keysym_t *syms;
            uint num_syms = xkb_keymap_key_get_syms_by_level(m_keymap, keycode, layout, currentLevel, &syms);
            for (uint sym = 0; sym < num_syms; sym++) {
                if (syms[sym] == keysym) {
                    return {{keycode - EVDEV_OFFSET, currentLevel}};
                }
            }
        }
    }
    return {};
}

QList<xkb_keysym_t> Xkb::keysymsFromQtKey(int keyQt)
{
    const int symQt = keyQt & ~Qt::KeyboardModifierMask;
    QList<xkb_keysym_t> syms;

    if (symQt >= Qt::Key_F1 && symQt <= Qt::Key_F35) {
        syms.append(XKB_KEY_F1 + (symQt - Qt::Key_F1));
        return syms;
    }

    const bool hasKeypadMod = keyQt & Qt::KeypadModifier;
    if (hasKeypadMod) {
        if (symQt >= Qt::Key_0 && symQt <= Qt::Key_9) {
            syms.append(XKB_KEY_KP_0 + (symQt - Qt::Key_0));
            return syms;
        }
    } else if (QXkbCommon::isLatin1(symQt)) {
        xkb_keysym_t lower, upper;
        QXkbCommon::xkbcommon_XConvertCase(symQt, &lower, &upper);
        if (keyQt & Qt::ShiftModifier) {
            syms.append(upper);
        } else {
            syms.append(lower);
        }
        return syms;
    }

    for (const TransKey &tk : g_rgSymXToQT) {
        if (tk.keySymQt == symQt) {
            // Use keysyms from the keypad if and only if KeypadModifier is set
            if (hasKeypadMod && !QXkbCommon::isKeypad(tk.keySymX)) {
                continue;
            }
            if (!hasKeypadMod && QXkbCommon::isKeypad(tk.keySymX)) {
                continue;
            }
            syms.append(tk.keySymX);
        }
    }
    if (!syms.isEmpty()) {
        return syms;
    }

    const QList<uint> ucs4 = QString(QChar(symQt)).toUcs4();
    for (uint utf32 : ucs4) {
        syms.append(utf32 | 0x01000000);
    }
    return syms;
}
}

#include "moc_xkb.cpp"
