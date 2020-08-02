/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2004 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "rules.h"

#include <kconfig.h>
#include <KXMessages>
#include <QRegExp>
#include <QTemporaryFile>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QDir>

#ifndef KCMRULES
#include "x11client.h"
#include "client_machine.h"
#include "screens.h"
#include "workspace.h"
#endif

#include "rulesettings.h"
#include "rulebooksettings.h"

namespace KWin
{

Rules::Rules()
    : temporary_state(0)
    , wmclassmatch(UnimportantMatch)
    , wmclasscomplete(UnimportantMatch)
    , windowrolematch(UnimportantMatch)
    , titlematch(UnimportantMatch)
    , clientmachinematch(UnimportantMatch)
    , types(NET::AllTypesMask)
    , placementrule(UnusedForceRule)
    , positionrule(UnusedSetRule)
    , sizerule(UnusedSetRule)
    , minsizerule(UnusedForceRule)
    , maxsizerule(UnusedForceRule)
    , opacityactiverule(UnusedForceRule)
    , opacityinactiverule(UnusedForceRule)
    , ignoregeometryrule(UnusedSetRule)
    , desktoprule(UnusedSetRule)
    , screenrule(UnusedSetRule)
    , activityrule(UnusedSetRule)
    , typerule(UnusedForceRule)
    , maximizevertrule(UnusedSetRule)
    , maximizehorizrule(UnusedSetRule)
    , minimizerule(UnusedSetRule)
    , shaderule(UnusedSetRule)
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
{
}

Rules::Rules(const QString& str, bool temporary)
    : temporary_state(temporary ? 2 : 0)
{
    QTemporaryFile file;
    if (file.open()) {
        QByteArray s = str.toUtf8();
        file.write(s.data(), s.length());
    }
    file.flush();
    auto cfg = KSharedConfig::openConfig(file.fileName(), KConfig::SimpleConfig);
    RuleSettings settings(cfg, QString());
    readFromSettings(&settings);
    if (description.isEmpty())
        description = QStringLiteral("temporary");
}

#define READ_MATCH_STRING(var, func) \
    var = settings->var() func; \
    var##match = static_cast<StringMatch>(settings->var##match())

#define READ_SET_RULE(var) \
    var = settings->var(); \
    var##rule = static_cast<SetRule>(settings->var##rule())

#define READ_FORCE_RULE(var, func) \
    var = func(settings->var()); \
    var##rule = convertForceRule(settings->var##rule())


Rules::Rules(const RuleSettings *settings)
    : temporary_state(0)
{
    readFromSettings(settings);
}

void Rules::readFromSettings(const RuleSettings *settings)
{
    description = settings->description();
    if (description.isEmpty()) {
        description = settings->descriptionLegacy();
    }
    READ_MATCH_STRING(wmclass, .toLower().toLatin1());
    wmclasscomplete = settings->wmclasscomplete();
    READ_MATCH_STRING(windowrole, .toLower().toLatin1());
    READ_MATCH_STRING(title,);
    READ_MATCH_STRING(clientmachine, .toLower().toLatin1());
    types = NET::WindowTypeMask(settings->types());
    READ_FORCE_RULE(placement,);
    READ_SET_RULE(position);
    READ_SET_RULE(size);
    if (size.isEmpty() && sizerule != static_cast<SetRule>(Remember))
        sizerule = UnusedSetRule;
    READ_FORCE_RULE(minsize,);
    if (!minsize.isValid())
        minsize = QSize(1, 1);
    READ_FORCE_RULE(maxsize,);
    if (maxsize.isEmpty())
        maxsize = QSize(32767, 32767);
    READ_FORCE_RULE(opacityactive,);
    READ_FORCE_RULE(opacityinactive,);
    READ_SET_RULE(ignoregeometry);
    READ_SET_RULE(desktop);
    READ_SET_RULE(screen);
    READ_SET_RULE(activity);
    READ_FORCE_RULE(type, static_cast<NET::WindowType>);
    if (type == NET::Unknown)
        typerule = UnusedForceRule;
    READ_SET_RULE(maximizevert);
    READ_SET_RULE(maximizehoriz);
    READ_SET_RULE(minimize);
    READ_SET_RULE(shade);
    READ_SET_RULE(skiptaskbar);
    READ_SET_RULE(skippager);
    READ_SET_RULE(skipswitcher);
    READ_SET_RULE(above);
    READ_SET_RULE(below);
    READ_SET_RULE(fullscreen);
    READ_SET_RULE(noborder);

    READ_FORCE_RULE(decocolor, getDecoColor);
    if (decocolor.isEmpty())
        decocolorrule = UnusedForceRule;

    READ_FORCE_RULE(blockcompositing,);
    READ_FORCE_RULE(fsplevel,);
    READ_FORCE_RULE(fpplevel,);
    READ_FORCE_RULE(acceptfocus,);
    READ_FORCE_RULE(closeable,);
    READ_FORCE_RULE(autogroup,);
    READ_FORCE_RULE(autogroupfg,);
    READ_FORCE_RULE(autogroupid,);
    READ_FORCE_RULE(strictgeometry,);
    READ_SET_RULE(shortcut);
    READ_FORCE_RULE(disableglobalshortcuts,);
    READ_SET_RULE(desktopfile);
}

#undef READ_MATCH_STRING
#undef READ_SET_RULE
#undef READ_FORCE_RULE
#undef READ_FORCE_RULE2

#define WRITE_MATCH_STRING(var, capital, force) \
    settings->set##capital##match(var##match); \
    if (!var.isEmpty() || force) \
    { \
        settings->set##capital(var); \
    }

#define WRITE_SET_RULE(var, capital, func) \
    settings->set##capital##rule(var##rule); \
    if (var##rule != UnusedSetRule) \
    { \
        settings->set##capital(func(var)); \
    }

#define WRITE_FORCE_RULE(var, capital, func) \
    settings->set##capital##rule(var##rule); \
    if ( var##rule != UnusedForceRule ) \
    { \
        settings->set##capital(func(var)); \
    }

void Rules::write(RuleSettings *settings) const
{
    settings->setDescription(description);
    // always write wmclass
    WRITE_MATCH_STRING(wmclass, Wmclass, true);
    settings->setWmclasscomplete(wmclasscomplete);
    WRITE_MATCH_STRING(windowrole, Windowrole, false);
    WRITE_MATCH_STRING(title, Title, false);
    WRITE_MATCH_STRING(clientmachine, Clientmachine, false);
    settings->setTypes(types);
    WRITE_FORCE_RULE(placement, Placement,);
    WRITE_SET_RULE(position, Position,);
    WRITE_SET_RULE(size, Size,);
    WRITE_FORCE_RULE(minsize, Minsize,);
    WRITE_FORCE_RULE(maxsize, Maxsize,);
    WRITE_FORCE_RULE(opacityactive, Opacityactive,);
    WRITE_FORCE_RULE(opacityinactive, Opacityinactive,);
    WRITE_SET_RULE(ignoregeometry, Ignoregeometry,);
    WRITE_SET_RULE(desktop, Desktop,);
    WRITE_SET_RULE(screen, Screen,);
    WRITE_SET_RULE(activity, Activity,);
    WRITE_FORCE_RULE(type, Type,);
    WRITE_SET_RULE(maximizevert, Maximizevert,);
    WRITE_SET_RULE(maximizehoriz, Maximizehoriz,);
    WRITE_SET_RULE(minimize, Minimize,);
    WRITE_SET_RULE(shade, Shade,);
    WRITE_SET_RULE(skiptaskbar, Skiptaskbar,);
    WRITE_SET_RULE(skippager, Skippager,);
    WRITE_SET_RULE(skipswitcher, Skipswitcher,);
    WRITE_SET_RULE(above, Above,);
    WRITE_SET_RULE(below, Below,);
    WRITE_SET_RULE(fullscreen, Fullscreen,);
    WRITE_SET_RULE(noborder, Noborder,);
    auto colorToString = [](const QString &value) -> QString {
        if (value.endsWith(QLatin1String(".colors"))) {
            return QFileInfo(value).baseName();
        } else {
            return value;
        }
    };
    WRITE_FORCE_RULE(decocolor, Decocolor, colorToString);
    WRITE_FORCE_RULE(blockcompositing, Blockcompositing,);
    WRITE_FORCE_RULE(fsplevel, Fsplevel,);
    WRITE_FORCE_RULE(fpplevel, Fpplevel,);
    WRITE_FORCE_RULE(acceptfocus, Acceptfocus,);
    WRITE_FORCE_RULE(closeable, Closeable,);
    WRITE_FORCE_RULE(autogroup, Autogroup,);
    WRITE_FORCE_RULE(autogroupfg, Autogroupfg,);
    WRITE_FORCE_RULE(autogroupid, Autogroupid,);
    WRITE_FORCE_RULE(strictgeometry, Strictgeometry,);
    WRITE_SET_RULE(shortcut, Shortcut,);
    WRITE_FORCE_RULE(disableglobalshortcuts, Disableglobalshortcuts,);
    WRITE_SET_RULE(desktopfile, Desktopfile,);
}

#undef WRITE_MATCH_STRING
#undef WRITE_SET_RULE
#undef WRITE_FORCE_RULE

// returns true if it doesn't affect anything
bool Rules::isEmpty() const
{
    return(placementrule == UnusedForceRule
           && positionrule == UnusedSetRule
           && sizerule == UnusedSetRule
           && minsizerule == UnusedForceRule
           && maxsizerule == UnusedForceRule
           && opacityactiverule == UnusedForceRule
           && opacityinactiverule == UnusedForceRule
           && ignoregeometryrule == UnusedSetRule
           && desktoprule == UnusedSetRule
           && screenrule == UnusedSetRule
           && activityrule == UnusedSetRule
           && typerule == UnusedForceRule
           && maximizevertrule == UnusedSetRule
           && maximizehorizrule == UnusedSetRule
           && minimizerule == UnusedSetRule
           && shaderule == UnusedSetRule
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
           && desktopfilerule == UnusedSetRule);
}

Rules::ForceRule Rules::convertForceRule(int v)
{
    if (v == DontAffect || v == Force || v == ForceTemporarily)
        return static_cast<ForceRule>(v);
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

bool Rules::matchType(NET::WindowType match_type) const
{
    if (types != NET::AllTypesMask) {
        if (match_type == NET::Unknown)
            match_type = NET::Normal; // NET::Unknown->NET::Normal is only here for matching
        if (!NET::typeMatchesMask(match_type, types))
            return false;
    }
    return true;
}

bool Rules::matchWMClass(const QByteArray& match_class, const QByteArray& match_name) const
{
    if (wmclassmatch != UnimportantMatch) {
        // TODO optimize?
        QByteArray cwmclass = wmclasscomplete
                              ? match_name + ' ' + match_class : match_class;
        if (wmclassmatch == RegExpMatch && QRegExp(QString::fromUtf8(wmclass)).indexIn(QString::fromUtf8(cwmclass)) == -1)
            return false;
        if (wmclassmatch == ExactMatch && wmclass != cwmclass)
            return false;
        if (wmclassmatch == SubstringMatch && !cwmclass.contains(wmclass))
            return false;
    }
    return true;
}

bool Rules::matchRole(const QByteArray& match_role) const
{
    if (windowrolematch != UnimportantMatch) {
        if (windowrolematch == RegExpMatch && QRegExp(QString::fromUtf8(windowrole)).indexIn(QString::fromUtf8(match_role)) == -1)
            return false;
        if (windowrolematch == ExactMatch && windowrole != match_role)
            return false;
        if (windowrolematch == SubstringMatch && !match_role.contains(windowrole))
            return false;
    }
    return true;
}

bool Rules::matchTitle(const QString& match_title) const
{
    if (titlematch != UnimportantMatch) {
        if (titlematch == RegExpMatch && QRegExp(title).indexIn(match_title) == -1)
            return false;
        if (titlematch == ExactMatch && title != match_title)
            return false;
        if (titlematch == SubstringMatch && !match_title.contains(title))
            return false;
    }
    return true;
}

bool Rules::matchClientMachine(const QByteArray& match_machine, bool local) const
{
    if (clientmachinematch != UnimportantMatch) {
        // if it's localhost, check also "localhost" before checking hostname
        if (match_machine != "localhost" && local
                && matchClientMachine("localhost", true))
            return true;
        if (clientmachinematch == RegExpMatch
                && QRegExp(QString::fromUtf8(clientmachine)).indexIn(QString::fromUtf8(match_machine)) == -1)
            return false;
        if (clientmachinematch == ExactMatch
                && clientmachine != match_machine)
            return false;
        if (clientmachinematch == SubstringMatch
                && !match_machine.contains(clientmachine))
            return false;
    }
    return true;
}

#ifndef KCMRULES
bool Rules::match(const AbstractClient* c) const
{
    if (!matchType(c->windowType(true)))
        return false;
    if (!matchWMClass(c->resourceClass(), c->resourceName()))
        return false;
    if (!matchRole(c->windowRole().toLower()))
        return false;
    if (!matchClientMachine(c->clientMachine()->hostName(), c->clientMachine()->isLocal()))
        return false;
    if (titlematch != UnimportantMatch) // track title changes to rematch rules
        QObject::connect(c, &AbstractClient::captionChanged, c, &AbstractClient::evaluateWindowRules,
                         // QueuedConnection, because title may change before
                         // the client is ready (could segfault!)
                         static_cast<Qt::ConnectionType>(Qt::QueuedConnection|Qt::UniqueConnection));
    if (!matchTitle(c->captionNormal()))
        return false;
    return true;
}

#define NOW_REMEMBER(_T_, _V_) ((selection & _T_) && (_V_##rule == (SetRule)Remember))

bool Rules::update(AbstractClient* c, int selection)
{
    // TODO check this setting is for this client ?
    bool updated = false;
    if NOW_REMEMBER(Position, position) {
        if (!c->isFullScreen()) {
            QPoint new_pos = position;
            // don't use the position in the direction which is maximized
            if ((c->maximizeMode() & MaximizeHorizontal) == 0)
                new_pos.setX(c->pos().x());
            if ((c->maximizeMode() & MaximizeVertical) == 0)
                new_pos.setY(c->pos().y());
            updated = updated || position != new_pos;
            position = new_pos;
        }
    }
    if NOW_REMEMBER(Size, size) {
        if (!c->isFullScreen()) {
            QSize new_size = size;
            // don't use the position in the direction which is maximized
            if ((c->maximizeMode() & MaximizeHorizontal) == 0)
                new_size.setWidth(c->size().width());
            if ((c->maximizeMode() & MaximizeVertical) == 0)
                new_size.setHeight(c->size().height());
            updated = updated || size != new_size;
            size = new_size;
        }
    }
    if NOW_REMEMBER(Desktop, desktop) {
        updated = updated || desktop != c->desktop();
        desktop = c->desktop();
    }
    if NOW_REMEMBER(Screen, screen) {
        updated = updated || screen != c->screen();
        screen = c->screen();
    }
    if NOW_REMEMBER(Activity, activity) {
        // TODO: ivan - multiple activities support
        const QString & joinedActivities = c->activities().join(QStringLiteral(","));
        updated = updated || activity != joinedActivities;
        activity = joinedActivities;
    }
    if NOW_REMEMBER(MaximizeVert, maximizevert) {
        updated = updated || maximizevert != bool(c->maximizeMode() & MaximizeVertical);
        maximizevert = c->maximizeMode() & MaximizeVertical;
    }
    if NOW_REMEMBER(MaximizeHoriz, maximizehoriz) {
        updated = updated || maximizehoriz != bool(c->maximizeMode() & MaximizeHorizontal);
        maximizehoriz = c->maximizeMode() & MaximizeHorizontal;
    }
    if NOW_REMEMBER(Minimize, minimize) {
        updated = updated || minimize != c->isMinimized();
        minimize = c->isMinimized();
    }
    if NOW_REMEMBER(Shade, shade) {
        updated = updated || (shade != (c->shadeMode() != ShadeNone));
        shade = c->shadeMode() != ShadeNone;
    }
    if NOW_REMEMBER(SkipTaskbar, skiptaskbar) {
        updated = updated || skiptaskbar != c->skipTaskbar();
        skiptaskbar = c->skipTaskbar();
    }
    if NOW_REMEMBER(SkipPager, skippager) {
        updated = updated || skippager != c->skipPager();
        skippager = c->skipPager();
    }
    if NOW_REMEMBER(SkipSwitcher, skipswitcher) {
        updated = updated || skipswitcher != c->skipSwitcher();
        skipswitcher = c->skipSwitcher();
    }
    if NOW_REMEMBER(Above, above) {
        updated = updated || above != c->keepAbove();
        above = c->keepAbove();
    }
    if NOW_REMEMBER(Below, below) {
        updated = updated || below != c->keepBelow();
        below = c->keepBelow();
    }
    if NOW_REMEMBER(Fullscreen, fullscreen) {
        updated = updated || fullscreen != c->isFullScreen();
        fullscreen = c->isFullScreen();
    }
    if NOW_REMEMBER(NoBorder, noborder) {
        updated = updated || noborder != c->noBorder();
        noborder = c->noBorder();
    }
    if NOW_REMEMBER(DesktopFile, desktopfile) {
        updated = updated || desktopfile != c->desktopFileName();
        desktopfile = c->desktopFileName();
    }
    return updated;
}

#undef NOW_REMEMBER

#define APPLY_RULE( var, name, type ) \
    bool Rules::apply##name( type& arg, bool init ) const \
    { \
        if ( checkSetRule( var##rule, init )) \
            arg = this->var; \
        return checkSetStop( var##rule ); \
    }

#define APPLY_FORCE_RULE( var, name, type ) \
    bool Rules::apply##name( type& arg ) const \
    { \
        if ( checkForceRule( var##rule )) \
            arg = this->var; \
        return checkForceStop( var##rule ); \
    }

APPLY_FORCE_RULE(placement, Placement, Placement::Policy)

bool Rules::applyGeometry(QRect& rect, bool init) const
{
    QPoint p = rect.topLeft();
    QSize s = rect.size();
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

bool Rules::applyPosition(QPoint& pos, bool init) const
{
    if (this->position != invalidPoint && checkSetRule(positionrule, init))
        pos = this->position;
    return checkSetStop(positionrule);
}

bool Rules::applySize(QSize& s, bool init) const
{
    if (this->size.isValid() && checkSetRule(sizerule, init))
        s = this->size;
    return checkSetStop(sizerule);
}

APPLY_FORCE_RULE(minsize, MinSize, QSize)
APPLY_FORCE_RULE(maxsize, MaxSize, QSize)
APPLY_FORCE_RULE(opacityactive, OpacityActive, int)
APPLY_FORCE_RULE(opacityinactive, OpacityInactive, int)
APPLY_RULE(ignoregeometry, IgnoreGeometry, bool)

APPLY_RULE(desktop, Desktop, int)
APPLY_RULE(screen, Screen, int)
APPLY_RULE(activity, Activity, QString)
APPLY_FORCE_RULE(type, Type, NET::WindowType)

bool Rules::applyMaximizeHoriz(MaximizeMode& mode, bool init) const
{
    if (checkSetRule(maximizehorizrule, init))
        mode = static_cast< MaximizeMode >((maximizehoriz ? MaximizeHorizontal : 0) | (mode & MaximizeVertical));
    return checkSetStop(maximizehorizrule);
}

bool Rules::applyMaximizeVert(MaximizeMode& mode, bool init) const
{
    if (checkSetRule(maximizevertrule, init))
        mode = static_cast< MaximizeMode >((maximizevert ? MaximizeVertical : 0) | (mode & MaximizeHorizontal));
    return checkSetStop(maximizevertrule);
}

APPLY_RULE(minimize, Minimize, bool)

bool Rules::applyShade(ShadeMode& sh, bool init) const
{
    if (checkSetRule(shaderule, init)) {
        if (!this->shade)
            sh = ShadeNone;
        if (this->shade && sh == ShadeNone)
            sh = ShadeNormal;
    }
    return checkSetStop(shaderule);
}

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


#undef APPLY_RULE
#undef APPLY_FORCE_RULE

bool Rules::isTemporary() const
{
    return temporary_state > 0;
}

bool Rules::discardTemporary(bool force)
{
    if (temporary_state == 0)   // not temporary
        return false;
    if (force || --temporary_state == 0) { // too old
        delete this;
        return true;
    }
    return false;
}

#define DISCARD_USED_SET_RULE( var ) \
    do { \
        if ( var##rule == ( SetRule ) ApplyNow || ( withdrawn && var##rule == ( SetRule ) ForceTemporarily )) { \
            var##rule = UnusedSetRule; \
            changed = true; \
        } \
    } while ( false )
#define DISCARD_USED_FORCE_RULE( var ) \
    do { \
        if ( withdrawn && var##rule == ( ForceRule ) ForceTemporarily ) { \
            var##rule = UnusedForceRule; \
            changed = true; \
        } \
    } while ( false )

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
    DISCARD_USED_SET_RULE(desktop);
    DISCARD_USED_SET_RULE(screen);
    DISCARD_USED_SET_RULE(activity);
    DISCARD_USED_FORCE_RULE(type);
    DISCARD_USED_SET_RULE(maximizevert);
    DISCARD_USED_SET_RULE(maximizehoriz);
    DISCARD_USED_SET_RULE(minimize);
    DISCARD_USED_SET_RULE(shade);
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

    return changed;
}
#undef DISCARD_USED_SET_RULE
#undef DISCARD_USED_FORCE_RULE

#endif

QDebug& operator<<(QDebug& stream, const Rules* r)
{
    return stream << "[" << r->description << ":" << r->wmclass << "]" ;
}

#ifndef KCMRULES
void WindowRules::discardTemporary()
{
    QVector< Rules* >::Iterator it2 = rules.begin();
    for (QVector< Rules* >::Iterator it = rules.begin();
            it != rules.end();
       ) {
        if ((*it)->discardTemporary(true))
            ++it;
        else {
            *it2++ = *it++;
        }
    }
    rules.erase(it2, rules.end());
}

void WindowRules::update(AbstractClient* c, int selection)
{
    bool updated = false;
    for (QVector< Rules* >::ConstIterator it = rules.constBegin();
            it != rules.constEnd();
            ++it)
        if ((*it)->update(c, selection))    // no short-circuiting here
            updated = true;
    if (updated)
        RuleBook::self()->requestDiskStorage();
}

#define CHECK_RULE( rule, type ) \
    type WindowRules::check##rule( type arg, bool init ) const \
    { \
        if ( rules.count() == 0 ) \
            return arg; \
        type ret = arg; \
        for ( QVector< Rules* >::ConstIterator it = rules.constBegin(); \
                it != rules.constEnd(); \
                ++it ) \
        { \
            if ( (*it)->apply##rule( ret, init )) \
                break; \
        } \
        return ret; \
    }

#define CHECK_FORCE_RULE( rule, type ) \
    type WindowRules::check##rule( type arg ) const \
    { \
        if ( rules.count() == 0 ) \
            return arg; \
        type ret = arg; \
        for ( QVector< Rules* >::ConstIterator it = rules.begin(); \
                it != rules.end(); \
                ++it ) \
        { \
            if ( (*it)->apply##rule( ret )) \
                break; \
        } \
        return ret; \
    }

CHECK_FORCE_RULE(Placement, Placement::Policy)

QRect WindowRules::checkGeometry(QRect rect, bool init) const
{
    return QRect(checkPosition(rect.topLeft(), init), checkSize(rect.size(), init));
}

CHECK_RULE(Position, QPoint)
CHECK_RULE(Size, QSize)
CHECK_FORCE_RULE(MinSize, QSize)
CHECK_FORCE_RULE(MaxSize, QSize)
CHECK_FORCE_RULE(OpacityActive, int)
CHECK_FORCE_RULE(OpacityInactive, int)
CHECK_RULE(IgnoreGeometry, bool)

CHECK_RULE(Desktop, int)
CHECK_RULE(Activity, QString)
CHECK_FORCE_RULE(Type, NET::WindowType)
CHECK_RULE(MaximizeVert, MaximizeMode)
CHECK_RULE(MaximizeHoriz, MaximizeMode)

MaximizeMode WindowRules::checkMaximize(MaximizeMode mode, bool init) const
{
    bool vert = checkMaximizeVert(mode, init) & MaximizeVertical;
    bool horiz = checkMaximizeHoriz(mode, init) & MaximizeHorizontal;
    return static_cast< MaximizeMode >((vert ? MaximizeVertical : 0) | (horiz ? MaximizeHorizontal : 0));
}

int WindowRules::checkScreen(int screen, bool init) const
{
    if ( rules.count() == 0 )
        return screen;
    int ret = screen;
    for ( QVector< Rules* >::ConstIterator it = rules.constBegin(); it != rules.constEnd(); ++it ) {
        if ( (*it)->applyScreen( ret, init ))
            break;
    }
    if (ret >= Screens::self()->count())
        ret = screen;
    return ret;
}

CHECK_RULE(Minimize, bool)
CHECK_RULE(Shade, ShadeMode)
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

#undef CHECK_RULE
#undef CHECK_FORCE_RULE

// Client

void AbstractClient::setupWindowRules(bool ignore_temporary)
{
    disconnect(this, &AbstractClient::captionChanged, this, &AbstractClient::evaluateWindowRules);
    m_rules = RuleBook::self()->find(this, ignore_temporary);
    // check only after getting the rules, because there may be a rule forcing window type
}

// Applies Force, ForceTemporarily and ApplyNow rules
// Used e.g. after the rules have been modified using the kcm.
void AbstractClient::applyWindowRules()
{
    // apply force rules
    // Placement - does need explicit update, just like some others below
    // Geometry : setGeometry() doesn't check rules
    auto client_rules = rules();
    QRect orig_geom = QRect(pos(), adjustedSize());   // handle shading
    QRect geom = client_rules->checkGeometry(orig_geom);
    if (geom != orig_geom)
        setFrameGeometry(geom);
    // MinSize, MaxSize handled by Geometry
    // IgnoreGeometry
    setDesktop(desktop());
    workspace()->sendClientToScreen(this, screen());
    setOnActivities(activities());
    // Type
    maximize(maximizeMode());
    // Minimize : functions don't check, and there are two functions
    if (client_rules->checkMinimize(isMinimized()))
        minimize();
    else
        unminimize();
    setShade(shadeMode());
    setOriginalSkipTaskbar(skipTaskbar());
    setSkipPager(skipPager());
    setSkipSwitcher(skipSwitcher());
    setKeepAbove(keepAbove());
    setKeepBelow(keepBelow());
    setFullScreen(isFullScreen(), true);
    setNoBorder(noBorder());
    updateColorScheme();
    // FSP
    // AcceptFocus :
    if (workspace()->mostRecentlyActivatedClient() == this
            && !client_rules->checkAcceptFocus(true))
        workspace()->activateNextClient(this);
    // Closeable
    QSize s = adjustedSize();
    if (s != size() && s.isValid())
        resizeWithChecks(s);
    // Autogrouping : Only checked on window manage
    // AutogroupInForeground : Only checked on window manage
    // AutogroupById : Only checked on window manage
    // StrictGeometry
    setShortcut(rules()->checkShortcut(shortcut().toString()));
    // see also X11Client::setActive()
    if (isActive()) {
        setOpacity(rules()->checkOpacityActive(qRound(opacity() * 100.0)) / 100.0);
        workspace()->disableGlobalShortcutsForClient(rules()->checkDisableGlobalShortcuts(false));
    } else
        setOpacity(rules()->checkOpacityInactive(qRound(opacity() * 100.0)) / 100.0);
    setDesktopFileName(rules()->checkDesktopFile(desktopFileName()).toUtf8());
}

void X11Client::updateWindowRules(Rules::Types selection)
{
    if (!isManaged())  // not fully setup yet
        return;
    AbstractClient::updateWindowRules(selection);
}

void AbstractClient::updateWindowRules(Rules::Types selection)
{
    if (RuleBook::self()->areUpdatesDisabled())
        return;
    m_rules.update(this, selection);
}

void AbstractClient::finishWindowRules()
{
    updateWindowRules(Rules::All);
    m_rules = WindowRules();
}

// Workspace
KWIN_SINGLETON_FACTORY(RuleBook)

RuleBook::RuleBook(QObject *parent)
    : QObject(parent)
    , m_updateTimer(new QTimer(this))
    , m_updatesDisabled(false)
    , m_temporaryRulesMessages()
{
    initializeX11();
    connect(kwinApp(), &Application::x11ConnectionChanged, this, &RuleBook::initializeX11);
    connect(kwinApp(), &Application::x11ConnectionAboutToBeDestroyed, this, &RuleBook::cleanupX11);
    connect(m_updateTimer, SIGNAL(timeout()), SLOT(save()));
    m_updateTimer->setInterval(1000);
    m_updateTimer->setSingleShot(true);
}

RuleBook::~RuleBook()
{
    save();
    deleteAll();
}

void RuleBook::initializeX11()
{
    auto c = kwinApp()->x11Connection();
    if (!c) {
        return;
    }
    m_temporaryRulesMessages.reset(new KXMessages(c, kwinApp()->x11RootWindow(), "_KDE_NET_WM_TEMPORARY_RULES", nullptr));
    connect(m_temporaryRulesMessages.data(), SIGNAL(gotMessage(QString)), SLOT(temporaryRulesMessage(QString)));
}

void RuleBook::cleanupX11()
{
    m_temporaryRulesMessages.reset();
}

void RuleBook::deleteAll()
{
    qDeleteAll(m_rules);
    m_rules.clear();
}

WindowRules RuleBook::find(const AbstractClient* c, bool ignore_temporary)
{
    QVector< Rules* > ret;
    for (QList< Rules* >::Iterator it = m_rules.begin();
            it != m_rules.end();
       ) {
        if (ignore_temporary && (*it)->isTemporary()) {
            ++it;
            continue;
        }
        if ((*it)->match(c)) {
            Rules* rule = *it;
            qCDebug(KWIN_CORE) << "Rule found:" << rule << ":" << c;
            if (rule->isTemporary())
                it = m_rules.erase(it);
            else
                ++it;
            ret.append(rule);
            continue;
        }
        ++it;
    }
    return WindowRules(ret);
}

void RuleBook::edit(AbstractClient* c, bool whole_app)
{
    save();
    QStringList args;
    args << QStringLiteral("--uuid") << c->internalId().toString();
    if (whole_app)
        args << QStringLiteral("--whole-app");
    QProcess *p = new Process(this);
    p->setArguments(args);
    p->setProcessEnvironment(kwinApp()->processStartupEnvironment());
    const QFileInfo buildDirBinary{QDir{QCoreApplication::applicationDirPath()}, QStringLiteral("kwin_rules_dialog")};
    p->setProgram(buildDirBinary.exists() ? buildDirBinary.absoluteFilePath() : QStringLiteral(KWIN_RULES_DIALOG_BIN));
    p->setProcessChannelMode(QProcess::MergedChannels);
    connect(p, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), p, &QProcess::deleteLater);
    connect(p, &QProcess::errorOccurred, this, [p](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            qCDebug(KWIN_CORE) << "Failed to start" << p->program();
        }
    });
    p->start();
}

void RuleBook::load()
{
    deleteAll();
    if (!m_config) {
        m_config = KSharedConfig::openConfig(QStringLiteral(KWIN_NAME "rulesrc"), KConfig::NoGlobals);
    } else {
        m_config->reparseConfiguration();
    }
    m_rules = RuleBookSettings(m_config).rules().toList();
}

void RuleBook::save()
{
    m_updateTimer->stop();
    if (!m_config) {
        qCWarning(KWIN_CORE) << "RuleBook::save invoked without prior invocation of RuleBook::load";
        return;
    }
    QVector<Rules *> filteredRules;
    for (const auto &rule : qAsConst(m_rules)) {
        if (!rule->isTemporary()) {
            filteredRules.append(rule);
        }
    }
    RuleBookSettings settings(m_config);
    settings.setRules(filteredRules);
    settings.save();
}

void RuleBook::temporaryRulesMessage(const QString& message)
{
    bool was_temporary = false;
    for (QList< Rules* >::ConstIterator it = m_rules.constBegin();
            it != m_rules.constEnd();
            ++it)
        if ((*it)->isTemporary())
            was_temporary = true;
    Rules* rule = new Rules(message, true);
    m_rules.prepend(rule);   // highest priority first
    if (!was_temporary)
        QTimer::singleShot(60000, this, SLOT(cleanupTemporaryRules()));
}

void RuleBook::cleanupTemporaryRules()
{
    bool has_temporary = false;
    for (QList< Rules* >::Iterator it = m_rules.begin();
            it != m_rules.end();
       ) {
        if ((*it)->discardTemporary(false)) { // deletes (*it)
            it = m_rules.erase(it);
        } else {
            if ((*it)->isTemporary())
                has_temporary = true;
            ++it;
        }
    }
    if (has_temporary)
        QTimer::singleShot(60000, this, SLOT(cleanupTemporaryRules()));
}

void RuleBook::discardUsed(AbstractClient* c, bool withdrawn)
{
    bool updated = false;
    for (QList< Rules* >::Iterator it = m_rules.begin();
            it != m_rules.end();
       ) {
        if (c->rules()->contains(*it)) {
            if ((*it)->discardUsed(withdrawn)) {
                updated = true;
            }
            if ((*it)->isEmpty()) {
                c->removeRule(*it);
                Rules* r = *it;
                it = m_rules.erase(it);
                delete r;
                continue;
            }
        }
        ++it;
    }
    if (updated)
        requestDiskStorage();
}

void RuleBook::requestDiskStorage()
{
    m_updateTimer->start();
}

void RuleBook::setUpdatesDisabled(bool disable)
{
    m_updatesDisabled = disable;
    if (!disable) {
        foreach (X11Client *c, Workspace::self()->clientList())
            c->updateWindowRules(Rules::All);
    }
}

#endif

} // namespace
