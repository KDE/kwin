/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2004 Lubos Lunak <l.lunak@kde.org>

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

#include "rules.h"

#include <fixx11h.h>
#include <kconfig.h>
#include <QRegExp>
#include <ktemporaryfile.h>
#include <QFile>
#include <ktoolinvocation.h>

#ifndef KCMRULES
#include "client.h"
#include "workspace.h"
#endif

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
    , tilingoptionrule(UnusedForceRule)
    , ignorepositionrule(UnusedForceRule)
    , desktoprule(UnusedSetRule)
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
    , blockcompositingrule(UnusedForceRule)
    , fsplevelrule(UnusedForceRule)
    , acceptfocusrule(UnusedForceRule)
    , closeablerule(UnusedForceRule)
    , autogrouprule(UnusedForceRule)
    , autogroupfgrule(UnusedForceRule)
    , autogroupidrule(UnusedForceRule)
    , strictgeometryrule(UnusedForceRule)
    , shortcutrule(UnusedSetRule)
    , disableglobalshortcutsrule(UnusedForceRule)
{
}

Rules::Rules(const QString& str, bool temporary)
    : temporary_state(temporary ? 2 : 0)
{
    KTemporaryFile file;
    if (file.open()) {
        QByteArray s = str.toUtf8();
        file.write(s.data(), s.length());
    }
    file.flush();
    KConfig cfg(file.fileName(), KConfig::SimpleConfig);
    readFromCfg(cfg.group(QString()));
    if (description.isEmpty())
        description = "temporary";
}

#define READ_MATCH_STRING( var, func ) \
    var = cfg.readEntry( #var ) func; \
    var##match = (StringMatch) qMax( FirstStringMatch, \
                                     qMin( LastStringMatch, static_cast< StringMatch >( cfg.readEntry( #var "match",0 ))));

#define READ_SET_RULE( var, func, def ) \
    var = func ( cfg.readEntry( #var, def)); \
    var##rule = readSetRule( cfg, #var "rule" );

#define READ_SET_RULE_DEF( var , func, def ) \
    var = func ( cfg.readEntry( #var, def )); \
    var##rule = readSetRule( cfg, #var "rule" );

#define READ_FORCE_RULE( var, func, def) \
    var = func ( cfg.readEntry( #var, def)); \
    var##rule = readForceRule( cfg, #var "rule" );

#define READ_FORCE_RULE2( var, def, func, funcarg ) \
    var = func ( cfg.readEntry( #var, def),funcarg ); \
    var##rule = readForceRule( cfg, #var "rule" );



Rules::Rules(const KConfigGroup& cfg)
    : temporary_state(0)
{
    readFromCfg(cfg);
}

static int limit0to4(int i)
{
    return qMax(0, qMin(4, i));
}

void Rules::readFromCfg(const KConfigGroup& cfg)
{
    description = cfg.readEntry("Description");
    if (description.isEmpty())  // capitalized first, lowercase for backwards compatibility
        description = cfg.readEntry("description");
    READ_MATCH_STRING(wmclass, .toLower().toLatin1());
    wmclasscomplete = cfg.readEntry("wmclasscomplete" , false);
    READ_MATCH_STRING(windowrole, .toLower().toLatin1());
    READ_MATCH_STRING(title,);
    READ_MATCH_STRING(clientmachine, .toLower().toLatin1());
    types = cfg.readEntry("types", uint(NET::AllTypesMask));
    READ_FORCE_RULE2(placement, QString(), Placement::policyFromString, false);
    READ_SET_RULE_DEF(position, , invalidPoint);
    READ_SET_RULE(size, , QSize());
    if (size.isEmpty() && sizerule != (SetRule)Remember)
        sizerule = UnusedSetRule;
    READ_FORCE_RULE(minsize, , QSize());
    if (!minsize.isValid())
        minsize = QSize(1, 1);
    READ_FORCE_RULE(maxsize, , QSize());
    if (maxsize.isEmpty())
        maxsize = QSize(32767, 32767);
    READ_FORCE_RULE(opacityactive, , 0);
    if (opacityactive < 0 || opacityactive > 100)
        opacityactive = 100;
    READ_FORCE_RULE(opacityinactive, , 0);
    if (opacityinactive < 0 || opacityinactive > 100)
        opacityinactive = 100;
    READ_FORCE_RULE(tilingoption, , 0);
    READ_FORCE_RULE(ignoreposition, , false);
    READ_SET_RULE(desktop, , 0);
    type = readType(cfg, "type");
    typerule = type != NET::Unknown ? readForceRule(cfg, "typerule") : UnusedForceRule;
    READ_SET_RULE(maximizevert, , false);
    READ_SET_RULE(maximizehoriz, , false);
    READ_SET_RULE(minimize, , false);
    READ_SET_RULE(shade, , false);
    READ_SET_RULE(skiptaskbar, , false);
    READ_SET_RULE(skippager, , false);
    READ_SET_RULE(skipswitcher, , false);
    READ_SET_RULE(above, , false);
    READ_SET_RULE(below, , false);
    READ_SET_RULE(fullscreen, , false);
    READ_SET_RULE(noborder, , false);
    READ_FORCE_RULE(blockcompositing, , false);
    READ_FORCE_RULE(fsplevel, limit0to4, 0); // fsp is 0-4
    READ_FORCE_RULE(acceptfocus, , false);
    READ_FORCE_RULE(closeable, , false);
    READ_FORCE_RULE(autogroup, , false);
    READ_FORCE_RULE(autogroupfg, , true);
    READ_FORCE_RULE(autogroupid, , QString());
    READ_FORCE_RULE(strictgeometry, , false);
    READ_SET_RULE(shortcut, , QString());
    READ_FORCE_RULE(disableglobalshortcuts, , false);
}

#undef READ_MATCH_STRING
#undef READ_SET_RULE
#undef READ_FORCE_RULE
#undef READ_FORCE_RULE2

#define WRITE_MATCH_STRING( var, cast, force ) \
    if ( !var.isEmpty() || force ) \
    { \
        cfg.writeEntry( #var, cast var ); \
        cfg.writeEntry( #var "match", (int)var##match ); \
    } \
    else \
    { \
        cfg.deleteEntry( #var ); \
        cfg.deleteEntry( #var "match" ); \
    }

#define WRITE_SET_RULE( var, func ) \
    if ( var##rule != UnusedSetRule ) \
    { \
        cfg.writeEntry( #var, func ( var )); \
        cfg.writeEntry( #var "rule", (int)var##rule ); \
    } \
    else \
    { \
        cfg.deleteEntry( #var ); \
        cfg.deleteEntry( #var "rule" ); \
    }

#define WRITE_FORCE_RULE( var, func ) \
    if ( var##rule != UnusedForceRule ) \
    { \
        cfg.writeEntry( #var, func ( var )); \
        cfg.writeEntry( #var "rule", (int)var##rule ); \
    } \
    else \
    { \
        cfg.deleteEntry( #var ); \
        cfg.deleteEntry( #var "rule" ); \
    }

void Rules::write(KConfigGroup& cfg) const
{
    cfg.writeEntry("Description", description);
    // always write wmclass
    WRITE_MATCH_STRING(wmclass, (const char*), true);
    cfg.writeEntry("wmclasscomplete", wmclasscomplete);
    WRITE_MATCH_STRING(windowrole, (const char*), false);
    WRITE_MATCH_STRING(title, , false);
    WRITE_MATCH_STRING(clientmachine, (const char*), false);
    if (types != NET::AllTypesMask)
        cfg.writeEntry("types", uint(types));
    else
        cfg.deleteEntry("types");
    WRITE_FORCE_RULE(placement, Placement::policyToString);
    WRITE_SET_RULE(position,);
    WRITE_SET_RULE(size,);
    WRITE_FORCE_RULE(minsize,);
    WRITE_FORCE_RULE(maxsize,);
    WRITE_FORCE_RULE(opacityactive,);
    WRITE_FORCE_RULE(opacityinactive,);
    WRITE_FORCE_RULE(tilingoption,);
    WRITE_FORCE_RULE(ignoreposition,);
    WRITE_SET_RULE(desktop,);
    WRITE_FORCE_RULE(type, int);
    WRITE_SET_RULE(maximizevert,);
    WRITE_SET_RULE(maximizehoriz,);
    WRITE_SET_RULE(minimize,);
    WRITE_SET_RULE(shade,);
    WRITE_SET_RULE(skiptaskbar,);
    WRITE_SET_RULE(skippager,);
    WRITE_SET_RULE(skipswitcher,);
    WRITE_SET_RULE(above,);
    WRITE_SET_RULE(below,);
    WRITE_SET_RULE(fullscreen,);
    WRITE_SET_RULE(noborder,);
    WRITE_FORCE_RULE(blockcompositing,);
    WRITE_FORCE_RULE(fsplevel,);
    WRITE_FORCE_RULE(acceptfocus,);
    WRITE_FORCE_RULE(closeable,);
    WRITE_FORCE_RULE(autogroup,);
    WRITE_FORCE_RULE(autogroupfg,);
    WRITE_FORCE_RULE(autogroupid,);
    WRITE_FORCE_RULE(strictgeometry,);
    WRITE_SET_RULE(shortcut,);
    WRITE_FORCE_RULE(disableglobalshortcuts,);
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
           && tilingoptionrule == UnusedForceRule
           && ignorepositionrule == UnusedForceRule
           && desktoprule == UnusedSetRule
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
           && blockcompositingrule == UnusedForceRule
           && fsplevelrule == UnusedForceRule
           && acceptfocusrule == UnusedForceRule
           && closeablerule == UnusedForceRule
           && autogrouprule == UnusedForceRule
           && autogroupfgrule == UnusedForceRule
           && autogroupidrule == UnusedForceRule
           && strictgeometryrule == UnusedForceRule
           && shortcutrule == UnusedSetRule
           && disableglobalshortcutsrule == UnusedForceRule);
}

Rules::SetRule Rules::readSetRule(const KConfigGroup& cfg, const QString& key)
{
    int v = cfg.readEntry(key, 0);
    if (v >= DontAffect && v <= ForceTemporarily)
        return static_cast< SetRule >(v);
    return UnusedSetRule;
}

Rules::ForceRule Rules::readForceRule(const KConfigGroup& cfg, const QString& key)
{
    int v = cfg.readEntry(key, 0);
    if (v == DontAffect || v == Force || v == ForceTemporarily)
        return static_cast< ForceRule >(v);
    return UnusedForceRule;
}

NET::WindowType Rules::readType(const KConfigGroup& cfg, const QString& key)
{
    int v = cfg.readEntry(key, 0);
    if (v >= NET::Normal && v <= NET::Splash)
        return static_cast< NET::WindowType >(v);
    return NET::Unknown;
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
        if (wmclassmatch == RegExpMatch && QRegExp(wmclass).indexIn(cwmclass) == -1)
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
        if (windowrolematch == RegExpMatch && QRegExp(windowrole).indexIn(match_role) == -1)
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

bool Rules::matchClientMachine(const QByteArray& match_machine) const
{
    if (clientmachinematch != UnimportantMatch) {
        // if it's localhost, check also "localhost" before checking hostname
        if (match_machine != "localhost" && isLocalMachine(match_machine)
                && matchClientMachine("localhost"))
            return true;
        if (clientmachinematch == RegExpMatch
                && QRegExp(clientmachine).indexIn(match_machine) == -1)
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
bool Rules::match(const Client* c) const
{
    if (!matchType(c->windowType(true)))
        return false;
    if (!matchWMClass(c->resourceClass(), c->resourceName()))
        return false;
    if (!matchRole(c->windowRole()))
        return false;
    if (!matchTitle(c->caption(false)))
        return false;
    if (!matchClientMachine(c->wmClientMachine(false)))
        return false;
    return true;
}

#define NOW_REMEMBER(_T_, _V_) ((selection & _T_) && (_V_##rule == (SetRule)Remember))

bool Rules::update(Client* c, int selection)
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
    if NOW_REMEMBER(OpacityActive, opacityactive) {
        // TODO
    }
    if NOW_REMEMBER(OpacityInactive, opacityinactive) {
        // TODO
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
APPLY_FORCE_RULE(ignoreposition, IgnorePosition, bool)
APPLY_FORCE_RULE(tilingoption, TilingOption, int)

// the cfg. entry needs to stay named the say for backwards compatibility
bool Rules::applyIgnoreGeometry(bool& ignore) const
{
    return applyIgnorePosition(ignore);
}

APPLY_RULE(desktop, Desktop, int)
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
APPLY_FORCE_RULE(blockcompositing, BlockCompositing, bool)
APPLY_FORCE_RULE(fsplevel, FSP, int)
APPLY_FORCE_RULE(acceptfocus, AcceptFocus, bool)
APPLY_FORCE_RULE(closeable, Closeable, bool)
APPLY_FORCE_RULE(autogroup, Autogrouping, bool)
APPLY_FORCE_RULE(autogroupfg, AutogroupInForeground, bool)
APPLY_FORCE_RULE(autogroupid, AutogroupById, QString)
APPLY_FORCE_RULE(strictgeometry, StrictGeometry, bool)
APPLY_RULE(shortcut, Shortcut, QString)
APPLY_FORCE_RULE(disableglobalshortcuts, DisableGlobalShortcuts, bool)


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
        if ( var##rule == ( SetRule ) ApplyNow || ( withdrawn && var##rule == ( SetRule ) ForceTemporarily )) \
            var##rule = UnusedSetRule; \
    } while ( false )
#define DISCARD_USED_FORCE_RULE( var ) \
    do { \
        if ( withdrawn && var##rule == ( ForceRule ) ForceTemporarily ) \
            var##rule = UnusedForceRule; \
    } while ( false )

void Rules::discardUsed(bool withdrawn)
{
    DISCARD_USED_FORCE_RULE(placement);
    DISCARD_USED_SET_RULE(position);
    DISCARD_USED_SET_RULE(size);
    DISCARD_USED_FORCE_RULE(minsize);
    DISCARD_USED_FORCE_RULE(maxsize);
    DISCARD_USED_FORCE_RULE(opacityactive);
    DISCARD_USED_FORCE_RULE(opacityinactive);
    DISCARD_USED_FORCE_RULE(tilingoption);
    DISCARD_USED_FORCE_RULE(ignoreposition);
    DISCARD_USED_SET_RULE(desktop);
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
    DISCARD_USED_FORCE_RULE(blockcompositing);
    DISCARD_USED_FORCE_RULE(fsplevel);
    DISCARD_USED_FORCE_RULE(acceptfocus);
    DISCARD_USED_FORCE_RULE(closeable);
    DISCARD_USED_FORCE_RULE(autogroup);
    DISCARD_USED_FORCE_RULE(autogroupfg);
    DISCARD_USED_FORCE_RULE(autogroupid);
    DISCARD_USED_FORCE_RULE(strictgeometry);
    DISCARD_USED_SET_RULE(shortcut);
    DISCARD_USED_FORCE_RULE(disableglobalshortcuts);
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

void WindowRules::update(Client* c, int selection)
{
    bool updated = false;
    for (QVector< Rules* >::ConstIterator it = rules.constBegin();
            it != rules.constEnd();
            ++it)
        if ((*it)->update(c, selection))    // no short-circuiting here
            updated = true;
    if (updated)
        Workspace::self()->rulesUpdated();
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
CHECK_FORCE_RULE(TilingOption, int)
CHECK_FORCE_RULE(IgnorePosition, bool)

bool WindowRules::checkIgnoreGeometry(bool ignore) const
{
    return checkIgnorePosition(ignore);
}

CHECK_RULE(Desktop, int)
CHECK_FORCE_RULE(Type, NET::WindowType)
CHECK_RULE(MaximizeVert, KDecorationDefines::MaximizeMode)
CHECK_RULE(MaximizeHoriz, KDecorationDefines::MaximizeMode)

KDecorationDefines::MaximizeMode WindowRules::checkMaximize(MaximizeMode mode, bool init) const
{
    bool vert = checkMaximizeVert(mode, init) & MaximizeVertical;
    bool horiz = checkMaximizeHoriz(mode, init) & MaximizeHorizontal;
    return static_cast< MaximizeMode >((vert ? MaximizeVertical : 0) | (horiz ? MaximizeHorizontal : 0));
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
CHECK_FORCE_RULE(BlockCompositing, bool)
CHECK_FORCE_RULE(FSP, int)
CHECK_FORCE_RULE(AcceptFocus, bool)
CHECK_FORCE_RULE(Closeable, bool)
CHECK_FORCE_RULE(Autogrouping, bool)
CHECK_FORCE_RULE(AutogroupInForeground, bool)
CHECK_FORCE_RULE(AutogroupById, QString)
CHECK_FORCE_RULE(StrictGeometry, bool)
CHECK_RULE(Shortcut, QString)
CHECK_FORCE_RULE(DisableGlobalShortcuts, bool)

#undef CHECK_RULE
#undef CHECK_FORCE_RULE

// Client

void Client::setupWindowRules(bool ignore_temporary)
{
    client_rules = workspace()->findWindowRules(this, ignore_temporary);
    // check only after getting the rules, because there may be a rule forcing window type
}

// Applies Force, ForceTemporarily and ApplyNow rules
// Used e.g. after the rules have been modified using the kcm.
void Client::applyWindowRules()
{
    // apply force rules
    // Placement - does need explicit update, just like some others below
    // Geometry : setGeometry() doesn't check rules
    QRect orig_geom = QRect(pos(), sizeForClientSize(clientSize()));   // handle shading
    QRect geom = client_rules.checkGeometry(orig_geom);
    if (geom != orig_geom)
        setGeometry(geom);
    // MinSize, MaxSize handled by Geometry
    // IgnorePosition
    setDesktop(desktop());
    // Type
    maximize(maximizeMode());
    // Minimize : functions don't check, and there are two functions
    if (client_rules.checkMinimize(isMinimized()))
        minimize();
    else
        unminimize();
    setShade(shadeMode());
    setSkipTaskbar(skipTaskbar(), true);
    setSkipPager(skipPager());
    setSkipSwitcher(skipSwitcher());
    setKeepAbove(keepAbove());
    setKeepBelow(keepBelow());
    setFullScreen(isFullScreen(), true);
    setNoBorder(noBorder());
    // FSP
    // AcceptFocus :
    if (workspace()->mostRecentlyActivatedClient() == this
            && !client_rules.checkAcceptFocus(true))
        workspace()->activateNextClient(this);
    // Closeable
    QSize s = adjustedSize();
    if (s != size())
        resizeWithChecks(s);
    // Autogrouping : Only checked on window manage
    // AutogroupInForeground : Only checked on window manage
    // AutogroupById : Only checked on window manage
    // StrictGeometry
    setShortcut(rules()->checkShortcut(shortcut().toString()));
    // see also Client::setActive()
    if (isActive()) {
        setOpacity(rules()->checkOpacityActive(qRound(opacity() * 100.0)) / 100.0);
        workspace()->disableGlobalShortcutsForClient(rules()->checkDisableGlobalShortcuts(false));
    } else
        setOpacity(rules()->checkOpacityInactive(qRound(opacity() * 100.0)) / 100.0);
}

void Client::updateWindowRules(Rules::Types selection)
{
    if (!isManaged())  // not fully setup yet
        return;
    if (workspace()->rulesUpdatesDisabled())
        return;
    client_rules.update(this, selection);
}

void Client::finishWindowRules()
{
    updateWindowRules(Rules::All);
    client_rules = WindowRules();
}

// Workspace

WindowRules Workspace::findWindowRules(const Client* c, bool ignore_temporary)
{
    QVector< Rules* > ret;
    for (QList< Rules* >::Iterator it = rules.begin();
            it != rules.end();
       ) {
        if (ignore_temporary && (*it)->isTemporary()) {
            ++it;
            continue;
        }
        if ((*it)->match(c)) {
            Rules* rule = *it;
            kDebug(1212) << "Rule found:" << rule << ":" << c;
            if (rule->isTemporary())
                it = rules.erase(it);
            else
                ++it;
            ret.append(rule);
            continue;
        }
        ++it;
    }
    return WindowRules(ret);
}

void Workspace::editWindowRules(Client* c, bool whole_app)
{
    writeWindowRules();
    QStringList args;
    args << "--wid" << QString::number(c->window());
    if (whole_app)
        args << "--whole-app";
    KToolInvocation::kdeinitExec("kwin_rules_dialog", args);
}

void Workspace::loadWindowRules()
{
    while (!rules.isEmpty()) {
        delete rules.front();
        rules.pop_front();
    }
    KConfig cfg("kwinrulesrc", KConfig::NoGlobals);
    int count = cfg.group("General").readEntry("count", 0);
    for (int i = 1;
            i <= count;
            ++i) {
        KConfigGroup cg(&cfg, QString::number(i));
        Rules* rule = new Rules(cg);
        rules.append(rule);
    }
}

void Workspace::writeWindowRules()
{
    rulesUpdatedTimer.stop();
    KConfig cfg("kwinrulesrc", KConfig::NoGlobals);
    QStringList groups = cfg.groupList();
    for (QStringList::ConstIterator it = groups.constBegin();
            it != groups.constEnd();
            ++it)
        cfg.deleteGroup(*it);
    cfg.group("General").writeEntry("count", rules.count());
    int i = 1;
    for (QList< Rules* >::ConstIterator it = rules.constBegin();
            it != rules.constEnd();
            ++it) {
        if ((*it)->isTemporary())
            continue;
        KConfigGroup cg(&cfg, QString::number(i));
        (*it)->write(cg);
        ++i;
    }
}

void Workspace::gotTemporaryRulesMessage(const QString& message)
{
    bool was_temporary = false;
    for (QList< Rules* >::ConstIterator it = rules.constBegin();
            it != rules.constEnd();
            ++it)
        if ((*it)->isTemporary())
            was_temporary = true;
    Rules* rule = new Rules(message, true);
    rules.prepend(rule);   // highest priority first
    if (!was_temporary)
        QTimer::singleShot(60000, this, SLOT(cleanupTemporaryRules()));
}

void Workspace::cleanupTemporaryRules()
{
    bool has_temporary = false;
    for (QList< Rules* >::Iterator it = rules.begin();
            it != rules.end();
       ) {
        if ((*it)->discardTemporary(false))
            it = rules.erase(it);
        else {
            if ((*it)->isTemporary())
                has_temporary = true;
            ++it;
        }
    }
    if (has_temporary)
        QTimer::singleShot(60000, this, SLOT(cleanupTemporaryRules()));
}

void Workspace::discardUsedWindowRules(Client* c, bool withdrawn)
{
    bool updated = false;
    for (QList< Rules* >::Iterator it = rules.begin();
            it != rules.end();
       ) {
        if (c->rules()->contains(*it)) {
            updated = true;
            (*it)->discardUsed(withdrawn);
            if ((*it)->isEmpty()) {
                c->removeRule(*it);
                Rules* r = *it;
                it = rules.erase(it);
                delete r;
                continue;
            }
        }
        ++it;
    }
    if (updated)
        rulesUpdated();
}

void Workspace::rulesUpdated()
{
    rulesUpdatedTimer.setSingleShot(true);
    rulesUpdatedTimer.start(1000);
}

void Workspace::disableRulesUpdates(bool disable)
{
    rules_updates_disabled = disable;
    if (!disable) {
        foreach (Client * c, clients)
            c->updateWindowRules(Rules::All);
    }
}

#endif

} // namespace
