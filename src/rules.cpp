/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2004 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "rules.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTemporaryFile>
#include <kconfig.h>

#ifndef KCMRULES
#include "client_machine.h"
#include "main.h"
#include "virtualdesktops.h"
#include "window.h"
#endif

#include "core/output.h"
#include "rulebooksettings.h"
#include "rulesettings.h"
#include "workspace.h"

namespace KWin
{

Rules::Rules()
    : layerrule(UnusedForceRule)
    , wmclassmatch(UnimportantMatch)
    , wmclasscomplete(UnimportantMatch)
    , windowrolematch(UnimportantMatch)
    , titlematch(UnimportantMatch)
    , clientmachinematch(UnimportantMatch)
    , tagmatch(UnimportantMatch)
    , types(NET::AllTypesMask)
    , placementrule(UnusedForceRule)
    , positionrule(UnusedSetRule)
    , sizerule(UnusedSetRule)
    , minsizerule(UnusedForceRule)
    , maxsizerule(UnusedForceRule)
    , opacityactiverule(UnusedForceRule)
    , opacityinactiverule(UnusedForceRule)
    , ignoregeometryrule(UnusedSetRule)
    , desktopsrule(UnusedSetRule)
    , screenrule(UnusedSetRule)
    , activityrule(UnusedSetRule)
    , maximizevertrule(UnusedSetRule)
    , maximizehorizrule(UnusedSetRule)
    , minimizerule(UnusedSetRule)
    , skiptaskbarrule(UnusedSetRule)
    , skippagerrule(UnusedSetRule)
    , skipswitcherrule(UnusedSetRule)
    , aboverule(UnusedSetRule)
    , belowrule(UnusedSetRule)
    , fullscreenrule(UnusedSetRule)
    , noborderrule(UnusedSetRule)
    , decocolorrule(UnusedForceRule)
    , blockcompositingrule(UnusedForceRule)
    , fsplevelrule(UnusedForceRule)
    , fpplevelrule(UnusedForceRule)
    , acceptfocusrule(UnusedForceRule)
    , closeablerule(UnusedForceRule)
    , autogrouprule(UnusedForceRule)
    , autogroupfgrule(UnusedForceRule)
    , autogroupidrule(UnusedForceRule)
    , strictgeometryrule(UnusedForceRule)
    , shortcutrule(UnusedSetRule)
    , disableglobalshortcutsrule(UnusedForceRule)
    , desktopfilerule(UnusedSetRule)
    , adaptivesyncrule(UnusedForceRule)
    , tearingrule(UnusedForceRule)
{
}

#define READ_MATCH_STRING(var, func) \
    var = settings->var() func;      \
    var##match = static_cast<StringMatch>(settings->var##match())

#define READ_SET_RULE(var) \
    var = settings->var(); \
    var##rule = static_cast<SetRule>(settings->var##rule())

#define READ_FORCE_RULE(var, func) \
    var = func(settings->var());   \
    var##rule = convertForceRule(settings->var##rule())

Rules::Rules(const RuleSettings *settings)
{
    readFromSettings(settings);
}

void Rules::readFromSettings(const RuleSettings *settings)
{
    m_id = settings->currentGroup();
    m_enabled = settings->enabled();
    description = settings->description();
    if (description.isEmpty()) {
        description = settings->descriptionLegacy();
    }
    READ_MATCH_STRING(wmclass, );
    wmclasscomplete = settings->wmclasscomplete();
    READ_MATCH_STRING(windowrole, );
    READ_MATCH_STRING(title, );
    READ_MATCH_STRING(clientmachine, .toLower());
    READ_MATCH_STRING(tag, );
    types = WindowTypes(settings->types());
    READ_FORCE_RULE(placement, );
    READ_SET_RULE(position);
    READ_SET_RULE(size);
    if (size.isEmpty() && sizerule != static_cast<SetRule>(Remember)) {
        sizerule = UnusedSetRule;
    }
    READ_FORCE_RULE(minsize, );
    if (!minsize.isValid()) {
        minsize = QSize(1, 1);
    }
    READ_FORCE_RULE(maxsize, );
    if (maxsize.isEmpty()) {
        maxsize = QSize(32767, 32767);
    }
    READ_FORCE_RULE(opacityactive, );
    READ_FORCE_RULE(opacityinactive, );
    READ_SET_RULE(ignoregeometry);
    READ_SET_RULE(desktops);
    READ_SET_RULE(screen);
    READ_SET_RULE(activity);
    READ_SET_RULE(maximizevert);
    READ_SET_RULE(maximizehoriz);
    READ_SET_RULE(minimize);
    READ_SET_RULE(skiptaskbar);
    READ_SET_RULE(skippager);
    READ_SET_RULE(skipswitcher);
    READ_SET_RULE(above);
    READ_SET_RULE(below);
    READ_SET_RULE(fullscreen);
    READ_SET_RULE(noborder);

    READ_FORCE_RULE(decocolor, getDecoColor);
    if (decocolor.isEmpty()) {
        decocolorrule = UnusedForceRule;
    }

    READ_FORCE_RULE(blockcompositing, );
    READ_FORCE_RULE(fsplevel, );
    READ_FORCE_RULE(fpplevel, );
    READ_FORCE_RULE(acceptfocus, );
    READ_FORCE_RULE(closeable, );
    READ_FORCE_RULE(autogroup, );
    READ_FORCE_RULE(autogroupfg, );
    READ_FORCE_RULE(autogroupid, );
    READ_FORCE_RULE(strictgeometry, );
    READ_SET_RULE(shortcut);
    READ_FORCE_RULE(disableglobalshortcuts, );
    READ_SET_RULE(desktopfile);
    READ_FORCE_RULE(layer, );
    READ_FORCE_RULE(adaptivesync, );
    READ_FORCE_RULE(tearing, );
}

#undef READ_MATCH_STRING
#undef READ_SET_RULE
#undef READ_FORCE_RULE
#undef READ_FORCE_RULE2

QString Rules::id() const
{
    return m_id;
}

#define WRITE_MATCH_STRING(var, capital, force) \
    settings->set##capital##match(var##match);  \
    if (!var.isEmpty() || force) {              \
        settings->set##capital(var);            \
    }

#define WRITE_SET_RULE(var, capital, func)   \
    settings->set##capital##rule(var##rule); \
    if (var##rule != UnusedSetRule) {        \
        settings->set##capital(func(var));   \
    }

#define WRITE_FORCE_RULE(var, capital, func) \
    settings->set##capital##rule(var##rule); \
    if (var##rule != UnusedForceRule) {      \
        settings->set##capital(func(var));   \
    }

void Rules::write(RuleSettings *settings) const
{
    settings->setDefaults();

    settings->setEnabled(m_enabled);
    settings->setDescription(description);
    // always write wmclass
    WRITE_MATCH_STRING(wmclass, Wmclass, true);
    settings->setWmclasscomplete(wmclasscomplete);
    WRITE_MATCH_STRING(windowrole, Windowrole, false);
    WRITE_MATCH_STRING(title, Title, false);
    WRITE_MATCH_STRING(clientmachine, Clientmachine, false);
    WRITE_MATCH_STRING(tag, Tag, false);
    settings->setTypes(types);
    WRITE_FORCE_RULE(placement, Placement, );
    WRITE_SET_RULE(position, Position, );
    WRITE_SET_RULE(size, Size, );
    WRITE_FORCE_RULE(minsize, Minsize, );
    WRITE_FORCE_RULE(maxsize, Maxsize, );
    WRITE_FORCE_RULE(opacityactive, Opacityactive, );
    WRITE_FORCE_RULE(opacityinactive, Opacityinactive, );
    WRITE_SET_RULE(ignoregeometry, Ignoregeometry, );
    WRITE_SET_RULE(desktops, Desktops, );
    WRITE_SET_RULE(screen, Screen, );
    WRITE_SET_RULE(activity, Activity, );
    WRITE_SET_RULE(maximizevert, Maximizevert, );
    WRITE_SET_RULE(maximizehoriz, Maximizehoriz, );
    WRITE_SET_RULE(minimize, Minimize, );
    WRITE_SET_RULE(skiptaskbar, Skiptaskbar, );
    WRITE_SET_RULE(skippager, Skippager, );
    WRITE_SET_RULE(skipswitcher, Skipswitcher, );
    WRITE_SET_RULE(above, Above, );
    WRITE_SET_RULE(below, Below, );
    WRITE_SET_RULE(fullscreen, Fullscreen, );
    WRITE_SET_RULE(noborder, Noborder, );
    auto colorToString = [](const QString &value) -> QString {
        if (value.endsWith(QLatin1String(".colors"))) {
            return QFileInfo(value).baseName();
        } else {
            return value;
        }
    };
    WRITE_FORCE_RULE(decocolor, Decocolor, colorToString);
    WRITE_FORCE_RULE(blockcompositing, Blockcompositing, );
    WRITE_FORCE_RULE(fsplevel, Fsplevel, );
    WRITE_FORCE_RULE(fpplevel, Fpplevel, );
    WRITE_FORCE_RULE(acceptfocus, Acceptfocus, );
    WRITE_FORCE_RULE(closeable, Closeable, );
    WRITE_FORCE_RULE(autogroup, Autogroup, );
    WRITE_FORCE_RULE(autogroupfg, Autogroupfg, );
    WRITE_FORCE_RULE(autogroupid, Autogroupid, );
    WRITE_FORCE_RULE(strictgeometry, Strictgeometry, );
    WRITE_SET_RULE(shortcut, Shortcut, );
    WRITE_FORCE_RULE(disableglobalshortcuts, Disableglobalshortcuts, );
    WRITE_SET_RULE(desktopfile, Desktopfile, );
    WRITE_FORCE_RULE(layer, Layer, );
    WRITE_FORCE_RULE(adaptivesync, Adaptivesync, );
}

#undef WRITE_MATCH_STRING
#undef WRITE_SET_RULE
#undef WRITE_FORCE_RULE

// returns true if it doesn't affect anything
bool Rules::isEmpty() const
{
    return placementrule == UnusedForceRule
        && positionrule == UnusedSetRule
        && sizerule == UnusedSetRule
        && minsizerule == UnusedForceRule
        && maxsizerule == UnusedForceRule
        && opacityactiverule == UnusedForceRule
        && opacityinactiverule == UnusedForceRule
        && ignoregeometryrule == UnusedSetRule
        && desktopsrule == UnusedSetRule
        && screenrule == UnusedSetRule
        && activityrule == UnusedSetRule
        && maximizevertrule == UnusedSetRule
        && maximizehorizrule == UnusedSetRule
        && minimizerule == UnusedSetRule
        && skiptaskbarrule == UnusedSetRule
        && skippagerrule == UnusedSetRule
        && skipswitcherrule == UnusedSetRule
        && aboverule == UnusedSetRule
        && belowrule == UnusedSetRule
        && fullscreenrule == UnusedSetRule
        && noborderrule == UnusedSetRule
        && decocolorrule == UnusedForceRule
        && blockcompositingrule == UnusedForceRule
        && fsplevelrule == UnusedForceRule
        && fpplevelrule == UnusedForceRule
        && acceptfocusrule == UnusedForceRule
        && closeablerule == UnusedForceRule
        && autogrouprule == UnusedForceRule
        && autogroupfgrule == UnusedForceRule
        && autogroupidrule == UnusedForceRule
        && strictgeometryrule == UnusedForceRule
        && shortcutrule == UnusedSetRule
        && disableglobalshortcutsrule == UnusedForceRule
        && desktopfilerule == UnusedSetRule
        && layerrule == UnusedForceRule
        && adaptivesyncrule == UnusedForceRule
        && tearingrule == UnusedForceRule;
}

Rules::ForceRule Rules::convertForceRule(int v)
{
    if (v == DontAffect || v == Force || v == ForceTemporarily) {
        return static_cast<ForceRule>(v);
    }
    return UnusedForceRule;
}

QString Rules::getDecoColor(const QString &themeName)
{
    if (themeName.isEmpty()) {
        return QString();
    }
    // find the actual scheme file
    return QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                  QLatin1String("color-schemes/") + themeName + QLatin1String(".colors"));
}

bool typeMatchesMask(WindowType type, WindowTypes mask)
{
    switch (type) {
        // clang-format off
#define CHECK_TYPE_MASK( type ) \
case WindowType:: type: \
    if( int(mask) & int(type##Mask) ) \
        return true; \
    break;
        // clang-format on
        CHECK_TYPE_MASK(Normal)
        CHECK_TYPE_MASK(Desktop)
        CHECK_TYPE_MASK(Dock)
        CHECK_TYPE_MASK(Toolbar)
        CHECK_TYPE_MASK(Menu)
        CHECK_TYPE_MASK(Dialog)
        CHECK_TYPE_MASK(Override)
        CHECK_TYPE_MASK(TopMenu)
        CHECK_TYPE_MASK(Utility)
        CHECK_TYPE_MASK(Splash)
        CHECK_TYPE_MASK(DropdownMenu)
        CHECK_TYPE_MASK(PopupMenu)
        CHECK_TYPE_MASK(Tooltip)
        CHECK_TYPE_MASK(Notification)
        CHECK_TYPE_MASK(ComboBox)
        CHECK_TYPE_MASK(DNDIcon)
        CHECK_TYPE_MASK(OnScreenDisplay)
        CHECK_TYPE_MASK(CriticalNotification)
        CHECK_TYPE_MASK(AppletPopup)
#undef CHECK_TYPE_MASK
    default:
        break;
    }
    return false;
}

bool Rules::matchType(WindowType match_type) const
{
    if (types != AllTypesMask) {
        if (match_type == WindowType::Unknown) {
            match_type = WindowType::Normal; // WindowType::Unknown->WindowType::Normal is only here for matching
        }
        if (!typeMatchesMask(match_type, types)) {
            return false;
        }
    }
    return true;
}

bool Rules::matchWMClass(const QString &match_class, const QString &match_name) const
{
    if (wmclassmatch != UnimportantMatch) {
        // TODO optimize?
        QString cwmclass = wmclasscomplete
            ? match_name + ' ' + match_class
            : match_class;
        if (wmclassmatch == RegExpMatch && !QRegularExpression(wmclass).match(cwmclass).hasMatch()) {
            return false;
        }
        if (wmclassmatch == ExactMatch && cwmclass != wmclass) {
            return false;
        }
        if (wmclassmatch == SubstringMatch && !cwmclass.contains(wmclass)) {
            return false;
        }
    }
    return true;
}

bool Rules::matchRole(const QString &match_role) const
{
    if (windowrolematch != UnimportantMatch) {
        if (windowrolematch == RegExpMatch && !QRegularExpression(windowrole).match(match_role).hasMatch()) {
            return false;
        }
        if (windowrolematch == ExactMatch && match_role != windowrole) {
            return false;
        }
        if (windowrolematch == SubstringMatch && !match_role.contains(windowrole)) {
            return false;
        }
    }
    return true;
}

bool Rules::matchTitle(const QString &match_title) const
{
    if (titlematch != UnimportantMatch) {
        if (titlematch == RegExpMatch && !QRegularExpression(title).match(match_title).hasMatch()) {
            return false;
        }
        if (titlematch == ExactMatch && title != match_title) {
            return false;
        }
        if (titlematch == SubstringMatch && !match_title.contains(title)) {
            return false;
        }
    }
    return true;
}

bool Rules::matchClientMachine(const QString &match_machine, bool local) const
{
    if (clientmachinematch != UnimportantMatch) {
        // if it's localhost, check also "localhost" before checking hostname
        if (match_machine != "localhost" && local
            && matchClientMachine("localhost", true)) {
            return true;
        }
        if (clientmachinematch == RegExpMatch
            && !QRegularExpression(clientmachine).match(match_machine).hasMatch()) {
            return false;
        }
        if (clientmachinematch == ExactMatch
            && clientmachine != match_machine) {
            return false;
        }
        if (clientmachinematch == SubstringMatch
            && !match_machine.contains(clientmachine)) {
            return false;
        }
    }
    return true;
}

bool Rules::matchTag(const QString &match_tag) const
{
    if (tagmatch != UnimportantMatch) {
        if (tagmatch == RegExpMatch && !QRegularExpression(tag).match(match_tag).hasMatch()) {
            return false;
        }
        if (tagmatch == ExactMatch && tag != match_tag) {
            return false;
        }
        if (tagmatch == SubstringMatch && !match_tag.contains(tag)) {
            return false;
        }
    }
    return true;
}

#ifndef KCMRULES
bool Rules::match(const Window *c) const
{
    if (!m_enabled) {
        return false;
    }
    if (!matchType(c->windowType())) {
        return false;
    }
    if (!matchWMClass(c->resourceClass(), c->resourceName())) {
        return false;
    }
    if (!matchRole(c->windowRole())) {
        return false;
    }
    if (!matchClientMachine(c->clientMachine()->hostName(), c->clientMachine()->isLocal())) {
        return false;
    }
    if (titlematch != UnimportantMatch) { // track title changes to rematch rules
        QObject::connect(c, &Window::captionNormalChanged, c, &Window::evaluateWindowRules, Qt::UniqueConnection);
    }
    if (!matchTitle(c->captionNormal())) {
        return false;
    }
    if (!matchTag(c->tag())) {
        return false;
    }
    return true;
}

#define NOW_REMEMBER(_T_, _V_) ((selection & _T_) && (_V_##rule == (SetRule)Remember))

bool Rules::update(Window *c, int selection)
{
    // TODO check this setting is for this client ?
    bool updated = false;
    if NOW_REMEMBER (Position, position) {
        if (!c->isFullScreen()) {
            QPoint new_pos = position;
            // don't use the position in the direction which is maximized
            if ((c->maximizeMode() & MaximizeHorizontal) == 0) {
                new_pos.setX(c->pos().x());
            }
            if ((c->maximizeMode() & MaximizeVertical) == 0) {
                new_pos.setY(c->pos().y());
            }
            updated = updated || position != new_pos;
            position = new_pos;
        }
    }
    if NOW_REMEMBER (Size, size) {
        if (!c->isFullScreen()) {
            QSize new_size = size;
            // don't use the position in the direction which is maximized
            if ((c->maximizeMode() & MaximizeHorizontal) == 0) {
                new_size.setWidth(c->size().width());
            }
            if ((c->maximizeMode() & MaximizeVertical) == 0) {
                new_size.setHeight(c->size().height());
            }
            updated = updated || size != new_size;
            size = new_size;
        }
    }
    if NOW_REMEMBER (Desktops, desktops) {
        updated = updated || desktops != c->desktopIds();
        desktops = c->desktopIds();
    }
    if NOW_REMEMBER (Screen, screen) {
        const int index = workspace()->outputs().indexOf(c->output());
        updated = updated || screen != index;
        screen = index;
    }
    if NOW_REMEMBER (Activity, activity) {
        updated = updated || activity != c->activities();
        activity = c->activities();
    }
    if NOW_REMEMBER (MaximizeVert, maximizevert) {
        updated = updated || maximizevert != bool(c->maximizeMode() & MaximizeVertical);
        maximizevert = c->maximizeMode() & MaximizeVertical;
    }
    if NOW_REMEMBER (MaximizeHoriz, maximizehoriz) {
        updated = updated || maximizehoriz != bool(c->maximizeMode() & MaximizeHorizontal);
        maximizehoriz = c->maximizeMode() & MaximizeHorizontal;
    }
    if NOW_REMEMBER (Minimize, minimize) {
        updated = updated || minimize != c->isMinimized();
        minimize = c->isMinimized();
    }
    if NOW_REMEMBER (SkipTaskbar, skiptaskbar) {
        updated = updated || skiptaskbar != c->skipTaskbar();
        skiptaskbar = c->skipTaskbar();
    }
    if NOW_REMEMBER (SkipPager, skippager) {
        updated = updated || skippager != c->skipPager();
        skippager = c->skipPager();
    }
    if NOW_REMEMBER (SkipSwitcher, skipswitcher) {
        updated = updated || skipswitcher != c->skipSwitcher();
        skipswitcher = c->skipSwitcher();
    }
    if NOW_REMEMBER (Above, above) {
        updated = updated || above != c->keepAbove();
        above = c->keepAbove();
    }
    if NOW_REMEMBER (Below, below) {
        updated = updated || below != c->keepBelow();
        below = c->keepBelow();
    }
    if NOW_REMEMBER (Fullscreen, fullscreen) {
        updated = updated || fullscreen != c->isFullScreen();
        fullscreen = c->isFullScreen();
    }
    if NOW_REMEMBER (NoBorder, noborder) {
        updated = updated || noborder != c->noBorder();
        noborder = c->noBorder();
    }
    if NOW_REMEMBER (DesktopFile, desktopfile) {
        updated = updated || desktopfile != c->desktopFileName();
        desktopfile = c->desktopFileName();
    }
    return updated;
}

#undef NOW_REMEMBER

#define APPLY_RULE(var, name, type)                     \
    bool Rules::apply##name(type &arg, bool init) const \
    {                                                   \
        if (checkSetRule(var##rule, init))              \
            arg = this->var;                            \
        return checkSetStop(var##rule);                 \
    }

#define APPLY_FORCE_RULE(var, name, type)    \
    bool Rules::apply##name(type &arg) const \
    {                                        \
        if (checkForceRule(var##rule))       \
            arg = this->var;                 \
        return checkForceStop(var##rule);    \
    }

APPLY_FORCE_RULE(placement, Placement, PlacementPolicy)

bool Rules::applyGeometry(QRectF &rect, bool init) const
{
    QPointF p = rect.topLeft();
    QSizeF s = rect.size();
    bool ret = false; // no short-circuiting
    if (applyPosition(p, init)) {
        rect.moveTopLeft(p);
        ret = true;
    }
    if (applySize(s, init)) {
        rect.setSize(s);
        ret = true;
    }
    return ret;
}

bool Rules::applyPosition(QPointF &pos, bool init) const
{
    if (this->position != invalidPoint && checkSetRule(positionrule, init)) {
        pos = this->position;
    }
    return checkSetStop(positionrule);
}

bool Rules::applySize(QSizeF &s, bool init) const
{
    if (this->size.isValid() && checkSetRule(sizerule, init)) {
        s = this->size;
    }
    return checkSetStop(sizerule);
}

APPLY_FORCE_RULE(minsize, MinSize, QSizeF)
APPLY_FORCE_RULE(maxsize, MaxSize, QSizeF)
APPLY_FORCE_RULE(opacityactive, OpacityActive, int)
APPLY_FORCE_RULE(opacityinactive, OpacityInactive, int)
APPLY_RULE(ignoregeometry, IgnoreGeometry, bool)

APPLY_RULE(screen, Screen, int)
APPLY_RULE(activity, Activity, QStringList)
APPLY_FORCE_RULE(layer, Layer, enum Layer)

bool Rules::applyDesktops(QList<VirtualDesktop *> &vds, bool init) const
{
    if (checkSetRule(desktopsrule, init)) {
        vds.clear();
        for (auto id : desktops) {
            if (auto vd = VirtualDesktopManager::self()->desktopForId(id)) {
                vds << vd;
            }
        }
    }
    return checkSetStop(desktopsrule);
}

bool Rules::applyMaximizeHoriz(MaximizeMode &mode, bool init) const
{
    if (checkSetRule(maximizehorizrule, init)) {
        mode = static_cast<MaximizeMode>((maximizehoriz ? MaximizeHorizontal : 0) | (mode & MaximizeVertical));
    }
    return checkSetStop(maximizehorizrule);
}

bool Rules::applyMaximizeVert(MaximizeMode &mode, bool init) const
{
    if (checkSetRule(maximizevertrule, init)) {
        mode = static_cast<MaximizeMode>((maximizevert ? MaximizeVertical : 0) | (mode & MaximizeHorizontal));
    }
    return checkSetStop(maximizevertrule);
}

APPLY_RULE(minimize, Minimize, bool)
APPLY_RULE(skiptaskbar, SkipTaskbar, bool)
APPLY_RULE(skippager, SkipPager, bool)
APPLY_RULE(skipswitcher, SkipSwitcher, bool)
APPLY_RULE(above, KeepAbove, bool)
APPLY_RULE(below, KeepBelow, bool)
APPLY_RULE(fullscreen, FullScreen, bool)
APPLY_RULE(noborder, NoBorder, bool)
APPLY_FORCE_RULE(decocolor, DecoColor, QString)
APPLY_FORCE_RULE(blockcompositing, BlockCompositing, bool)
APPLY_FORCE_RULE(fsplevel, FSP, int)
APPLY_FORCE_RULE(fpplevel, FPP, int)
APPLY_FORCE_RULE(acceptfocus, AcceptFocus, bool)
APPLY_FORCE_RULE(closeable, Closeable, bool)
APPLY_FORCE_RULE(autogroup, Autogrouping, bool)
APPLY_FORCE_RULE(autogroupfg, AutogroupInForeground, bool)
APPLY_FORCE_RULE(autogroupid, AutogroupById, QString)
APPLY_FORCE_RULE(strictgeometry, StrictGeometry, bool)
APPLY_RULE(shortcut, Shortcut, QString)
APPLY_FORCE_RULE(disableglobalshortcuts, DisableGlobalShortcuts, bool)
APPLY_RULE(desktopfile, DesktopFile, QString)
APPLY_FORCE_RULE(adaptivesync, AdaptiveSync, bool)
APPLY_FORCE_RULE(tearing, Tearing, bool)

#undef APPLY_RULE
#undef APPLY_FORCE_RULE

#define DISCARD_USED_SET_RULE(var)                                                                     \
    do {                                                                                               \
        if (var##rule == (SetRule)ApplyNow || (withdrawn && var##rule == (SetRule)ForceTemporarily)) { \
            var##rule = UnusedSetRule;                                                                 \
            changed = true;                                                                            \
        }                                                                                              \
    } while (false)
#define DISCARD_USED_FORCE_RULE(var)                                 \
    do {                                                             \
        if (withdrawn && var##rule == (ForceRule)ForceTemporarily) { \
            var##rule = UnusedForceRule;                             \
            changed = true;                                          \
        }                                                            \
    } while (false)

bool Rules::discardUsed(bool withdrawn)
{
    bool changed = false;
    DISCARD_USED_FORCE_RULE(placement);
    DISCARD_USED_SET_RULE(position);
    DISCARD_USED_SET_RULE(size);
    DISCARD_USED_FORCE_RULE(minsize);
    DISCARD_USED_FORCE_RULE(maxsize);
    DISCARD_USED_FORCE_RULE(opacityactive);
    DISCARD_USED_FORCE_RULE(opacityinactive);
    DISCARD_USED_SET_RULE(ignoregeometry);
    DISCARD_USED_SET_RULE(desktops);
    DISCARD_USED_SET_RULE(screen);
    DISCARD_USED_SET_RULE(activity);
    DISCARD_USED_SET_RULE(maximizevert);
    DISCARD_USED_SET_RULE(maximizehoriz);
    DISCARD_USED_SET_RULE(minimize);
    DISCARD_USED_SET_RULE(skiptaskbar);
    DISCARD_USED_SET_RULE(skippager);
    DISCARD_USED_SET_RULE(skipswitcher);
    DISCARD_USED_SET_RULE(above);
    DISCARD_USED_SET_RULE(below);
    DISCARD_USED_SET_RULE(fullscreen);
    DISCARD_USED_SET_RULE(noborder);
    DISCARD_USED_FORCE_RULE(decocolor);
    DISCARD_USED_FORCE_RULE(blockcompositing);
    DISCARD_USED_FORCE_RULE(fsplevel);
    DISCARD_USED_FORCE_RULE(fpplevel);
    DISCARD_USED_FORCE_RULE(acceptfocus);
    DISCARD_USED_FORCE_RULE(closeable);
    DISCARD_USED_FORCE_RULE(autogroup);
    DISCARD_USED_FORCE_RULE(autogroupfg);
    DISCARD_USED_FORCE_RULE(autogroupid);
    DISCARD_USED_FORCE_RULE(strictgeometry);
    DISCARD_USED_SET_RULE(shortcut);
    DISCARD_USED_FORCE_RULE(disableglobalshortcuts);
    DISCARD_USED_SET_RULE(desktopfile);
    DISCARD_USED_FORCE_RULE(layer);
    DISCARD_USED_FORCE_RULE(adaptivesync);
    DISCARD_USED_FORCE_RULE(tearing);

    return changed;
}
#undef DISCARD_USED_SET_RULE
#undef DISCARD_USED_FORCE_RULE

#endif

QDebug &operator<<(QDebug &stream, const Rules *r)
{
    return stream << "[" << r->description << ":" << r->wmclass << "]";
}

#ifndef KCMRULES

void WindowRules::update(Window *c, int selection)
{
    bool updated = false;
    for (QList<Rules *>::ConstIterator it = rules.constBegin();
         it != rules.constEnd();
         ++it) {
        if ((*it)->update(c, selection)) { // no short-circuiting here
            updated = true;
        }
    }
    if (updated) {
        workspace()->rulebook()->requestDiskStorage();
    }
}

#define CHECK_RULE(rule, type)                                      \
    type WindowRules::check##rule(type arg, bool init) const        \
    {                                                               \
        if (rules.count() == 0)                                     \
            return arg;                                             \
        type ret = arg;                                             \
        for (QList<Rules *>::ConstIterator it = rules.constBegin(); \
             it != rules.constEnd();                                \
             ++it) {                                                \
            if ((*it)->apply##rule(ret, init))                      \
                break;                                              \
        }                                                           \
        return ret;                                                 \
    }

#define CHECK_FORCE_RULE(rule, type)                           \
    type WindowRules::check##rule(type arg) const              \
    {                                                          \
        if (rules.count() == 0)                                \
            return arg;                                        \
        type ret = arg;                                        \
        for (QList<Rules *>::ConstIterator it = rules.begin(); \
             it != rules.end();                                \
             ++it) {                                           \
            if ((*it)->apply##rule(ret))                       \
                break;                                         \
        }                                                      \
        return ret;                                            \
    }

CHECK_FORCE_RULE(Placement, PlacementPolicy)

QRectF WindowRules::checkGeometry(QRectF rect, bool init) const
{
    return QRectF(checkPosition(rect.topLeft(), init), checkSize(rect.size(), init));
}

QRectF WindowRules::checkGeometrySafe(QRectF rect, bool init) const
{
    return QRectF(checkPositionSafe(rect.topLeft(), init), checkSize(rect.size(), init));
}

QPointF WindowRules::checkPositionSafe(QPointF pos, bool init) const
{
    const auto ret = checkPosition(pos, init);
    if (ret == pos || ret == invalidPoint) {
        return ret;
    }
    const auto outputs = workspace()->outputs();
    const bool inAnyOutput = std::any_of(outputs.begin(), outputs.end(), [ret](const auto output) {
        return output->geometryF().contains(ret);
    });
    if (inAnyOutput) {
        return ret;
    } else {
        return invalidPoint;
    }
}

CHECK_RULE(Position, QPointF)
CHECK_RULE(Size, QSizeF)
CHECK_FORCE_RULE(MinSize, QSizeF)
CHECK_FORCE_RULE(MaxSize, QSizeF)
CHECK_FORCE_RULE(OpacityActive, int)
CHECK_FORCE_RULE(OpacityInactive, int)
CHECK_RULE(IgnoreGeometry, bool)

CHECK_RULE(Desktops, QList<VirtualDesktop *>)
CHECK_RULE(Activity, QStringList)
CHECK_RULE(MaximizeVert, MaximizeMode)
CHECK_RULE(MaximizeHoriz, MaximizeMode)

MaximizeMode WindowRules::checkMaximize(MaximizeMode mode, bool init) const
{
    bool vert = checkMaximizeVert(mode, init) & MaximizeVertical;
    bool horiz = checkMaximizeHoriz(mode, init) & MaximizeHorizontal;
    return static_cast<MaximizeMode>((vert ? MaximizeVertical : 0) | (horiz ? MaximizeHorizontal : 0));
}

LogicalOutput *WindowRules::checkOutput(LogicalOutput *output, bool init) const
{
    if (rules.isEmpty()) {
        return output;
    }
    int ret = workspace()->outputs().indexOf(output);
    for (Rules *rule : rules) {
        if (rule->applyScreen(ret, init)) {
            break;
        }
    }
    LogicalOutput *ruleOutput = workspace()->outputs().value(ret);
    return ruleOutput ? ruleOutput : output;
}

CHECK_RULE(Minimize, bool)
CHECK_RULE(SkipTaskbar, bool)
CHECK_RULE(SkipPager, bool)
CHECK_RULE(SkipSwitcher, bool)
CHECK_RULE(KeepAbove, bool)
CHECK_RULE(KeepBelow, bool)
CHECK_RULE(FullScreen, bool)
CHECK_RULE(NoBorder, bool)
CHECK_FORCE_RULE(DecoColor, QString)
CHECK_FORCE_RULE(BlockCompositing, bool)
CHECK_FORCE_RULE(FSP, int)
CHECK_FORCE_RULE(FPP, int)
CHECK_FORCE_RULE(AcceptFocus, bool)
CHECK_FORCE_RULE(Closeable, bool)
CHECK_FORCE_RULE(Autogrouping, bool)
CHECK_FORCE_RULE(AutogroupInForeground, bool)
CHECK_FORCE_RULE(AutogroupById, QString)
CHECK_FORCE_RULE(StrictGeometry, bool)
CHECK_RULE(Shortcut, QString)
CHECK_FORCE_RULE(DisableGlobalShortcuts, bool)
CHECK_RULE(DesktopFile, QString)
CHECK_FORCE_RULE(Layer, Layer)
CHECK_FORCE_RULE(AdaptiveSync, bool)
CHECK_FORCE_RULE(Tearing, bool)

#undef CHECK_RULE
#undef CHECK_FORCE_RULE

RuleBook::RuleBook()
    : m_updateTimer(new QTimer(this))
    , m_updatesDisabled(false)
{
    connect(m_updateTimer, &QTimer::timeout, this, &RuleBook::save);
    m_updateTimer->setInterval(1000);
    m_updateTimer->setSingleShot(true);
}

RuleBook::~RuleBook()
{
    save();
    deleteAll();
}

void RuleBook::deleteAll()
{
    qDeleteAll(m_rules);
    m_rules.clear();
}

WindowRules RuleBook::find(const Window *window) const
{
    QList<Rules *> ret;
    for (Rules *rule : m_rules) {
        if (rule->match(window)) {
            qCDebug(KWIN_CORE) << "Rule found:" << rule << ":" << window;
            ret.append(rule);
        }
    }
    return WindowRules(ret);
}

void RuleBook::edit(Window *c, bool whole_app)
{
    save();
    QStringList args;
    args << QStringLiteral("uuid=%1").arg(c->internalId().toString());
    if (whole_app) {
        args << QStringLiteral("whole-app");
    }
    QProcess *p = new QProcess(this);
    p->setArguments({"kcm_kwinrules", "--args", args.join(QLatin1Char(' '))});
    p->setProcessEnvironment(kwinApp()->processStartupEnvironment());
    p->setProgram(QStandardPaths::findExecutable("kcmshell6"));
    p->setProcessChannelMode(QProcess::MergedChannels);
    connect(p, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), p, &QProcess::deleteLater);
    connect(p, &QProcess::errorOccurred, this, [p](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            qCDebug(KWIN_CORE) << "Failed to start" << p->program();
        }
    });
    p->start();
}

void RuleBook::setConfig(const KSharedConfig::Ptr &config)
{
    m_book = std::make_unique<RuleBookSettings>(config);
}

void RuleBook::load()
{
    deleteAll();
    if (!m_book) {
        m_book = std::make_unique<RuleBookSettings>();
    } else {
        m_book->sharedConfig()->reparseConfiguration();
    }
    m_book->load();
    m_rules = m_book->rules();
}

void RuleBook::save()
{
    m_updateTimer->stop();
    if (!m_book) {
        qCWarning(KWIN_CORE) << "RuleBook::save invoked without prior invocation of RuleBook::load";
        return;
    }
    m_book->save();
}

void RuleBook::discardUsed(Window *c, bool withdrawn)
{
    for (QList<Rules *>::Iterator it = m_rules.begin();
         it != m_rules.end();) {
        if (c->rules()->contains(*it)) {
            const auto index = m_book->indexForId((*it)->id());
            if ((*it)->discardUsed(withdrawn)) {
                if (index) {
                    RuleSettings *setting = m_book->ruleSettingsAt(index.value());
                    (*it)->write(setting);
                }
            }
            if ((*it)->isEmpty()) {
                c->removeRule(*it);
                Rules *r = *it;
                it = m_rules.erase(it);
                delete r;
                if (index) {
                    m_book->removeRuleSettingsAt(index.value());
                }
                continue;
            }
        }
        ++it;
    }
    if (m_book->usrIsSaveNeeded()) {
        requestDiskStorage();
    }
}

void RuleBook::requestDiskStorage()
{
    m_updateTimer->start();
}

void RuleBook::setUpdatesDisabled(bool disable)
{
    m_updatesDisabled = disable;
    if (!disable) {
        const auto windows = Workspace::self()->windows();
        for (Window *window : windows) {
            if (window->supportsWindowRules()) {
                window->updateWindowRules(Rules::All);
            }
        }
    }
}

#endif

} // namespace

#include "moc_rules.cpp"
